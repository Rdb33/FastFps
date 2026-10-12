#include "network.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static const Color ink={.92f,.9f,.79f},muted={.51f,.55f,.45f},orange={1,.48f,.18f},blue={.35f,.85f,1};
static void centered(Mesh *m,float y,int scale,Color color,const char *text){mesh_text(m,(640-(float)strlen(text)*6*(float)scale)/2,y,scale,color,text);}
void lobby_open(App *app){app->state=STATE_NETWORK;app->selected=0;app->editing=-1;SDL_SetRelativeMouseMode(SDL_FALSE);}
static void stop_edit(App *app){app->editing=-1;app->select_all=false;SDL_StopTextInput();}
static void leave(App *app,Game *g) {
    net_close(app->network);game_reset(g,g->world,g->particle_limit);g->motion=app->motion;
    lobby_open(app);snprintf(app->network->message,sizeof(app->network->message),"SALON QUITTE");
}
static void action(App *app,Game *g) {
    Network *n=app->network;
    if(app->state==STATE_LOBBY) {
        if(app->selected==1){leave(app,g);return;}
        if(n->mode==NET_HOST)net_start(n,g,(double)SDL_GetPerformanceCounter()/(double)SDL_GetPerformanceFrequency());
        else net_ready(n);
        return;
    }
    if(app->selected>=2&&app->selected<=4){app->editing=app->selected;app->select_all=true;SDL_StartTextInput();return;}
    if(app->selected==5){stop_edit(app);app->state=STATE_MENU;app->selected=0;return;}
    bool success=app->selected==0?net_host(n):net_join(n);
    if(success){app->state=STATE_LOBBY;app->selected=0;}
}
static int hit(Renderer *r,int x,int y,State state) {
    int width,height;SDL_GetWindowSize(r->window,&width,&height);
    float dw=(float)width,dh=dw*9/16;if(dh>height){dh=(float)height;dw=dh*16/9;}
    float px=((float)x-((float)width-dw)/2)*640/dw,py=((float)y-((float)height-dh)/2)*360/dh;
    if(px<76||px>564)return -1;
    if(state==STATE_NETWORK)return py>=136&&py<256?(int)((py-136)/20):-1;
    if(py>=241&&py<261)return 0;
    if(py>=265&&py<285)return 1;
    return -1;
}
bool lobby_event(App *app,Game *g,Renderer *r,const SDL_Event *event) {
    if(app->state!=STATE_NETWORK&&app->state!=STATE_LOBBY)return false;
    Network *n=app->network;
    if(event->type==SDL_QUIT){app->running=false;return true;}
    if(event->type==SDL_TEXTINPUT&&app->editing>=2) {
        char *text=app->editing==2?n->name:(app->editing==3?n->address:n->port_text);
        size_t capacity=app->editing==2?sizeof(n->name):(app->editing==3?sizeof(n->address):sizeof(n->port_text));
        size_t used=strlen(text);
        if(app->select_all){used=0;text[0]=0;app->select_all=false;}
        for(const unsigned char *c=(const unsigned char *)event->text.text;*c;c++) {
            bool allowed=*c>=32&&*c<=126;
            if(app->editing>=3)allowed=(*c>='0'&&*c<='9')||(app->editing==3&&*c=='.');
            if(allowed&&used+1<capacity){text[used++]=(char)*c;text[used]=0;}
        }
        return true;
    }
    if(event->type==SDL_KEYDOWN&&(!event->key.repeat||app->editing>=2)) {
        SDL_Scancode key=event->key.keysym.scancode;
        if(app->editing>=2) {
            if(event->key.keysym.sym==SDLK_a&&(event->key.keysym.mod&KMOD_CTRL)){app->select_all=true;return true;}
            if(key==SDL_SCANCODE_RETURN||key==SDL_SCANCODE_ESCAPE){stop_edit(app);return true;}
            if(key==SDL_SCANCODE_BACKSPACE) {
                char *text=app->editing==2?n->name:(app->editing==3?n->address:n->port_text);
                size_t len=strlen(text);if(app->select_all){text[0]=0;app->select_all=false;}else if(len)text[len-1]=0;
            }
            return true;
        }
        int count=app->state==STATE_NETWORK?6:2;
        if(key==SDL_SCANCODE_UP)app->selected=(app->selected+count-1)%count;
        else if(key==SDL_SCANCODE_DOWN||key==SDL_SCANCODE_TAB)app->selected=(app->selected+1)%count;
        else if(key==SDL_SCANCODE_RETURN||key==SDL_SCANCODE_KP_ENTER||key==SDL_SCANCODE_SPACE)action(app,g);
        else if(key==SDL_SCANCODE_ESCAPE) {
            if(app->state==STATE_LOBBY)leave(app,g);else {app->state=STATE_MENU;app->selected=0;}
        }else return false;
        return true;
    }
    if(event->type==SDL_MOUSEMOTION) {
        if(app->editing<0){int row=hit(r,event->motion.x,event->motion.y,app->state);if(row>=0)app->selected=row;}
        return true;
    }
    if(event->type==SDL_MOUSEBUTTONDOWN&&event->button.button==SDL_BUTTON_LEFT) {
        int row=hit(r,event->button.x,event->button.y,app->state);
        stop_edit(app);if(row>=0){app->selected=row;action(app,g);}return true;
    }
    return false;
}
void lobby_sync(App *app,Game *g) {
    Network *n=app->network;
    if(n->mode==NET_OFF) {
        if(app->state==STATE_LOBBY||g->cooperative) {
            game_reset(g,g->world,g->particle_limit);g->motion=app->motion;lobby_open(app);
        }
        return;
    }
    if(n->phase==ROOM_PLAYING&&app->state==STATE_LOBBY) {
        app->capture_error=SDL_SetRelativeMouseMode(SDL_TRUE)!=0;
        app->state=app->capture_error?STATE_PAUSED:STATE_PLAYING;
        g->motion=app->motion;
    }else if(n->phase==ROOM_FINISHED&&app->state!=STATE_LOBBY) {
        app->state=STATE_LOBBY;app->selected=0;SDL_SetRelativeMouseMode(SDL_FALSE);
        if(g->score>app->best_score)app->best_score=g->score;
    }
}
static void notice(Mesh *m,const char *text,float y) {
    size_t len=strlen(text);char line[75];
    for(size_t offset=0;offset<len&&offset<148;offset+=74) {
        size_t n=len-offset;if(n>74)n=74;memcpy(line,text+offset,n);line[n]=0;
        mesh_text(m,80,y,1,orange,line);y+=11;
    }
}
void lobby_build(Mesh *m,const Game *g,const App *app) {
    Network *n=app->network;char text[128];
    mesh_rect(m,48,12,544,336,(Color){.055f,.06f,.047f});mesh_rect(m,48,12,544,3,blue);
    mesh_text(m,76,27,1,muted,"FASTFPS " GAME_VERSION " / COOPERATION A 4 JOUEURS");
    centered(m,50,6,ink,app->state==STATE_NETWORK?"SALONS":"COOP");
    if(app->state==STATE_NETWORK) {
        centered(m,110,1,blue,"UN JOUEUR HEBERGE - SES AMIS LE REJOIGNENT");
        for(int i=0;i<6;i++) {
            float y=141+(float)i*20;
            if(i==app->selected)mesh_rect(m,76,y-5,488,18,(Color){.15f,.22f,.22f});
            switch(i) {
                case 0:snprintf(text,sizeof(text),"CREER UN SALON");break;
                case 1:snprintf(text,sizeof(text),"REJOINDRE UN SALON");break;
                case 2:snprintf(text,sizeof(text),"PSEUDO : %s%s",n->name,app->editing==2?"_":"");break;
                case 3:snprintf(text,sizeof(text),"ADRESSE HOTE : %s%s",n->address,app->editing==3?"_":"");break;
                case 4:snprintf(text,sizeof(text),"PORT TCP : %s%s",n->port_text,app->editing==4?"_":"");break;
                default:snprintf(text,sizeof(text),"RETOUR");break;
            }
            mesh_text(m,88,y,1,i==app->selected?blue:muted,i==app->selected?">":" ");
            mesh_text(m,106,y,1,i==app->selected?ink:muted,text);
        }
        notice(m,n->message,269);
        mesh_text(m,76,309,1,ink,"ENTREE / CLIC : CHOISIR - TEXTE : PSEUDO, IP ET PORT");
        mesh_text(m,76,328,1,muted,"INTERNET : IP ACCESSIBLE / PORT OUVERT OU VPN PRIVE");
        return;
    }
    snprintf(text,sizeof(text),"SALON DE %s / PORT %d",n->members[0].name,n->port);centered(m,108,1,blue,text);
    if(n->connecting)centered(m,158,2,ink,"CONNEXION...");
    else for(int i=0;i<MAX_PLAYERS;i++) {
        float y=131+(float)i*25;
        mesh_rect(m,84,y-4,472,21,(Color){.10f,.13f,.105f});
        snprintf(text,sizeof(text),"%d  %-16s  %s%s",i+1,n->members[i].present?n->members[i].name:"PLACE LIBRE",
            n->members[i].present?(n->members[i].ready?"PRET":"EN ATTENTE"):"",i==0?" / HOTE":"");
        mesh_text(m,98,y,1,i==n->local_id?blue:ink,text);
    }
    if(n->phase==ROOM_FINISHED){snprintf(text,sizeof(text),"FIN - SCORE %d / VAGUE %d",g->score,g->wave);centered(m,222,1,orange,text);}
    for(int i=0;i<2;i++) {
        float y=246+(float)i*24;
        if(i==app->selected)mesh_rect(m,76,y-5,488,18,(Color){.15f,.22f,.22f});
        const char *label=i==1?"QUITTER LE SALON":(n->mode==NET_HOST?"LANCER LA PARTIE":
            (n->members[n->local_id].ready?"NE PLUS ETRE PRET":"JE SUIS PRET"));
        mesh_text(m,100,y,1,i==app->selected?ink:muted,label);
    }
    notice(m,n->message,294);
    centered(m,330,1,muted,n->mode==NET_HOST?"PARTAGE L'IP DE CET ORDINATEUR ET LE PORT AVEC TES AMIS":"L'HOTE LANCE QUAND TOUT LE MONDE EST PRET");
}
