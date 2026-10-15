#include "updates.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

#define UPDATE_SOURCE "https://raw.githubusercontent.com/Rdb33/FastFps/main/src/config.h"
#define UPDATE_BODY_LIMIT 8192

static bool version_read(const char *text,unsigned parts[3]) {
    for(int i=0;i<3;i++) {
        unsigned value=0;
        if(*text<'0'||*text>'9')return false;
        do {
            value=value*10+(unsigned)(*text++-'0');
            if(value>999999)return false;
        }while(*text>='0'&&*text<='9');
        parts[i]=value;
        if(i<2&&*text++!='.')return false;
    }
    return *text=='\0';
}

static bool newer(const char *latest) {
    unsigned remote[3],local[3];
    if(!version_read(latest,remote)||!version_read(GAME_VERSION,local))return false;
    for(int i=0;i<3;i++)if(remote[i]!=local[i])return remote[i]>local[i];
    return false;
}

/* curl vérifie HTTPS ; aucun shell ni contenu téléchargé n'est exécuté.
   curl est facultatif : absent, hors ligne ou en erreur, le jeu continue. */
static bool download(UpdateChecker *u,char body[UPDATE_BODY_LIMIT]) {
    int pipefd[2];if(pipe(pipefd)!=0)return false;
    if(fcntl(pipefd[0],F_SETFD,FD_CLOEXEC)<0||fcntl(pipefd[1],F_SETFD,FD_CLOEXEC)<0
        ||fcntl(pipefd[0],F_SETFL,O_NONBLOCK)<0) {
        close(pipefd[0]);close(pipefd[1]);return false;
    }
    posix_spawn_file_actions_t actions;
    if(posix_spawn_file_actions_init(&actions)!=0){close(pipefd[0]);close(pipefd[1]);return false;}
    int error=posix_spawn_file_actions_addopen(&actions,STDIN_FILENO,"/dev/null",O_RDONLY,0);
    if(!error)error=posix_spawn_file_actions_addopen(&actions,STDERR_FILENO,"/dev/null",O_WRONLY,0);
    if(!error)error=posix_spawn_file_actions_adddup2(&actions,pipefd[1],STDOUT_FILENO);
    if(!error)error=posix_spawn_file_actions_addclose(&actions,pipefd[0]);
    if(!error)error=posix_spawn_file_actions_addclose(&actions,pipefd[1]);
    char *args[]={"curl","--disable","--silent","--fail","--location","--max-redirs","2",
        "--proto","=https","--proto-redir","=https","--connect-timeout","2",
        "--max-time","4","--max-filesize","8191",UPDATE_SOURCE,NULL};
    pid_t child=-1;
    if(!error)error=posix_spawnp(&child,"curl",&actions,NULL,args,environ);
    posix_spawn_file_actions_destroy(&actions);close(pipefd[1]);
    if(error){close(pipefd[0]);return false;}
    size_t used=0;int status=0;bool exited=false,eof=false,failed=false;
    Uint32 start=SDL_GetTicks();
    while(!SDL_AtomicGet(&u->cancel)&&SDL_GetTicks()-start<5000) {
        if(!eof) {
            ssize_t count=read(pipefd[0],body+used,UPDATE_BODY_LIMIT-1-used);
            if(count>0) {
                used+=(size_t)count;
                if(used==UPDATE_BODY_LIMIT-1){failed=true;break;}
            }else if(count==0)eof=true;
            else if(errno!=EAGAIN&&errno!=EWOULDBLOCK&&errno!=EINTR){failed=true;break;}
        }
        if(!exited) {
            pid_t done=waitpid(child,&status,WNOHANG);
            if(done==child)exited=true;
            else if(done<0&&errno!=EINTR){failed=true;break;}
        }
        if(exited&&eof)break;
        if(eof)SDL_Delay(20);
        else {struct pollfd fd={.fd=pipefd[0],.events=POLLIN};poll(&fd,1,50);}
    }
    close(pipefd[0]);
    if(!exited) {
        kill(child,SIGKILL);
        while(waitpid(child,&status,0)<0&&errno==EINTR){}
        return false;
    }
    body[used]='\0';
    return !failed&&eof&&!SDL_AtomicGet(&u->cancel)&&WIFEXITED(status)&&WEXITSTATUS(status)==0;
}

static int check_thread(void *data) {
    UpdateChecker *u=data;char body[UPDATE_BODY_LIMIT];
    if(download(u,body)) {
        const char *line=body;
        while(*line) {
            static const char prefix[]="#define GAME_VERSION \"";
            if(strncmp(line,prefix,sizeof(prefix)-1)==0) {
                const char *value=line+sizeof(prefix)-1,*end=strchr(value,'"');unsigned parts[3];
                if(end&&(size_t)(end-value)<sizeof(u->latest)) {
                    char version[24];size_t length=(size_t)(end-value);
                    memcpy(version,value,length);version[length]='\0';
                    if(version_read(version,parts))snprintf(u->latest,sizeof(u->latest),"%s",version);
                }
                break;
            }
            const char *end=strchr(line,'\n');if(!end)break;line=end+1;
        }
    }
    SDL_AtomicSet(&u->ready,1);return 0;
}

void updates_start(UpdateChecker *u) {
    memset(u,0,sizeof(*u));u->selected=1;
    u->thread=SDL_CreateThread(check_thread,"verification-version",u);
}

static bool visible(const App *app) {
    const UpdateChecker *u=app->updates;
    return u&&u->available&&!u->dismissed&&app->state!=STATE_PLAYING&&app->state!=STATE_UPGRADE
        &&!app->menu_page&&app->editing<0&&app->numeric_edit<0&&app->slider_drag<0;
}

void updates_poll(App *app) {
    UpdateChecker *u=app->updates;if(!u)return;
    if(u->thread&&SDL_AtomicGet(&u->ready)) {
        SDL_WaitThread(u->thread,NULL);u->thread=NULL;u->available=newer(u->latest);
    }
    bool show=visible(app);
    if(show&&!u->open){u->ready_at=SDL_GetTicks()+600;u->released=false;u->selected=1;}
    u->open=show;
    if(show&&!u->released) {
        const Uint8 *keys=SDL_GetKeyboardState(NULL);
        u->released=!(SDL_GetMouseState(NULL,NULL)&SDL_BUTTON(SDL_BUTTON_LEFT))
            &&!keys[SDL_SCANCODE_RETURN]&&!keys[SDL_SCANCODE_KP_ENTER]&&!keys[SDL_SCANCODE_SPACE];
    }
}

static bool enabled(const UpdateChecker *u) {
    return u->released&&SDL_TICKS_PASSED(SDL_GetTicks(),u->ready_at);
}

static int button_at(Renderer *r,int x,int y) {
    int width,height;SDL_GetWindowSize(r->window,&width,&height);
    if(width<=0||height<=0)return -1;
    float dw=(float)width,dh=dw*9/16;
    if(dh>height){dh=(float)height;dw=dh*16/9;}
    float px=((float)x-((float)width-dw)/2)*640/dw,py=((float)y-((float)height-dh)/2)*360/dh;
    if(py<231||py>263)return -1;
    if(px>=104&&px<=312)return 0;
    return px>=328&&px<=536?1:-1;
}

static bool copy_commands(void) {
    char *base=SDL_GetBasePath();if(!base)return false;
    size_t length=strlen(base);
    if(length>=7&&strcmp(base+length-7,"/build/")==0)base[length-6]='\0';
    char commands[4096];size_t used=0;
    memcpy(commands,"cd '",4);used=4;
    for(const char *p=base;*p;p++) {
        size_t need=*p=='\''?4:1;
        if(used+need+128>=sizeof(commands)){SDL_free(base);return false;}
        if(*p=='\''){memcpy(commands+used,"'\\''",4);used+=4;}
        else commands[used++]=*p;
    }
    SDL_free(base);
    /* Pas de retour à la ligne final : coller ne valide pas la commande. */
    snprintf(commands+used,sizeof(commands)-used,"' && git pull --ff-only origin main && ./lancer.sh");
    return SDL_SetClipboardText(commands)==0;
}

static void activate(UpdateChecker *u) {
    if(u->selected==1){u->dismissed=true;u->open=false;return;}
    if(!u->details) {
        u->details=true;u->selected=0;u->ready_at=SDL_GetTicks()+300;u->released=false;
    }else {u->copied=copy_commands();u->copy_error=!u->copied;}
}

bool updates_event(App *app,Renderer *r,const SDL_Event *event) {
    UpdateChecker *u=app->updates;
    if(!visible(app)||!u->open)return false;
    if(event->type==SDL_QUIT||event->type==SDL_WINDOWEVENT)return false;
    if(event->type==SDL_KEYDOWN&&!event->key.repeat) {
        SDL_Scancode key=event->key.keysym.scancode;
        if(key==SDL_SCANCODE_ESCAPE){u->dismissed=true;u->open=false;}
        else if(enabled(u)) {
            if(key==SDL_SCANCODE_LEFT||key==SDL_SCANCODE_RIGHT||key==SDL_SCANCODE_TAB
                ||key==SDL_SCANCODE_UP||key==SDL_SCANCODE_DOWN)u->selected=1-u->selected;
            else if(key==SDL_SCANCODE_RETURN||key==SDL_SCANCODE_KP_ENTER||key==SDL_SCANCODE_SPACE)activate(u);
        }
    }else if(enabled(u)&&event->type==SDL_MOUSEMOTION) {
        int hit=button_at(r,event->motion.x,event->motion.y);if(hit>=0)u->selected=hit;
    }else if(enabled(u)&&event->type==SDL_MOUSEBUTTONDOWN&&event->button.button==SDL_BUTTON_LEFT) {
        int hit=button_at(r,event->button.x,event->button.y);
        if(hit>=0){u->selected=hit;activate(u);}
    }
    return event->type==SDL_KEYDOWN||event->type==SDL_KEYUP||event->type==SDL_TEXTINPUT
        ||event->type==SDL_MOUSEBUTTONDOWN||event->type==SDL_MOUSEBUTTONUP
        ||event->type==SDL_MOUSEMOTION||event->type==SDL_MOUSEWHEEL;
}

void updates_build(Mesh *m,const App *app) {
    if(!visible(app)||!app->updates->open)return;
    const UpdateChecker *u=app->updates;char text[96];
    Color blue={.32f,.8f,1},ink={.91f,.96f,1},muted={.51f,.64f,.72f};
    mesh_rect(m,0,0,640,360,(Color){.012f,.022f,.032f});
    mesh_rect(m,80,72,480,216,(Color){.035f,.07f,.10f});mesh_rect(m,80,72,480,3,blue);
    mesh_text(m,104,94,2,blue,u->details?"METTRE FASTFPS A JOUR":"MISE A JOUR DISPONIBLE");
    snprintf(text,sizeof(text),"VERSION INSTALLEE : %s / DISPONIBLE : %s",GAME_VERSION,u->latest);
    mesh_text(m,104,124,1,ink,text);
    if(u->details) {
        mesh_text(m,104,149,1,ink,"1. COPIE ET COLLE DANS UN TERMINAL SANS VALIDER.");
        mesh_text(m,104,166,1,ink,"2. FERME LE JEU, PUIS APPUIE SUR ENTREE.");
        mesh_text(m,104,193,1,u->copy_error?(Color){1,.6f,.3f}:muted,
            u->copied?"COMMANDES COPIEES : COLLE AVANT DE FERMER LE JEU.":
            (u->copy_error?"COPIE IMPOSSIBLE : CONSULTE LE README DU JEU.":"LE JEU SERA MIS A JOUR, RECOMPILE ET RELANCE."));
    }else {
        mesh_text(m,104,151,1,ink,"UNE NOUVELLE VERSION EST DISPONIBLE SUR GITHUB.");
        mesh_text(m,104,171,1,muted,"POUR JOUER ENSEMBLE, METTEZ L'EQUIPE A JOUR.");
        mesh_text(m,104,193,1,muted,"TU PEUX CONTINUER A JOUER AVEC CETTE VERSION.");
    }
    for(int i=0;i<2;i++) {
        float x=i?328:104;bool active=enabled(u)&&u->selected==i;
        mesh_rect(m,x,231,208,32,active?(Color){.10f,.28f,.37f}:(Color){.06f,.12f,.16f});
        mesh_rect(m,x,261,208,2,active?blue:muted);
        const char *label=i?(u->details?"FERMER":"PLUS TARD"):(u->details?"COPIER LES COMMANDES":"VOIR LA MISE A JOUR");
        mesh_text(m,x+(208-(float)strlen(label)*6)/2,243,1,active?ink:muted,label);
    }
    mesh_text(m,269,274,1,muted,"ECHAP : PLUS TARD");
}

void updates_shutdown(UpdateChecker *u) {
    SDL_AtomicSet(&u->cancel,1);
    if(u->thread){SDL_WaitThread(u->thread,NULL);u->thread=NULL;}
}
