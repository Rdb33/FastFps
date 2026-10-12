#include "network.h"
#include <stdio.h>
#include <string.h>

static const Color ink={.92f,.9f,.79f},muted={.51f,.55f,.45f},orange={1,.48f,.18f},blue={.35f,.85f,1};
static void centered(Mesh *m,float y,int scale,Color color,const char *text){mesh_text(m,(640-(float)strlen(text)*6*(float)scale)/2,y,scale,color,text);}
void lobby_open(App *app) {
    app->state=STATE_NETWORK;app->selected=0;app->editing=-1;app->select_all=false;app->network_help=false;
    app->network->discovery_requested=true;
    net_local_addresses(app->network);SDL_StopTextInput();SDL_SetRelativeMouseMode(SDL_FALSE);
}
static void stop_edit(App *app){app->editing=-1;app->select_all=false;SDL_StopTextInput();}
static void edit(App *app,int field){app->editing=field;app->selected=field;app->select_all=true;SDL_StartTextInput();}
static char *field_text(Network *n,int field,size_t *capacity) {
    if(field==2){*capacity=sizeof(n->name);return n->name;}
    if(field==3){*capacity=sizeof(n->address);return n->address;}
    *capacity=sizeof(n->port_text);return n->port_text;
}
static void insert(App *app,const char *source) {
    size_t capacity;char *text=field_text(app->network,app->editing,&capacity);
    size_t used=strlen(text);
    if(app->select_all){used=0;text[0]=0;app->select_all=false;}
    for(const unsigned char *c=(const unsigned char *)source;*c;c++) {
        bool allowed=*c>=32&&*c<=126;
        if(app->editing==4)allowed=*c>='0'&&*c<='9';
        if(allowed&&used+1<capacity){text[used++]=(char)*c;text[used]=0;}
    }
}
static void paste(App *app) {
    char *clipboard=SDL_GetClipboardText();
    if(!clipboard||!*clipboard){SDL_free(clipboard);snprintf(app->network->message,sizeof(app->network->message),"LE PRESSE-PAPIERS EST VIDE");return;}
    bool invitation=app->editing<0;
    if(invitation){app->editing=3;app->select_all=true;}
    insert(app,clipboard);SDL_free(clipboard);
    if(invitation){stop_edit(app);app->selected=1;snprintf(app->network->message,sizeof(app->network->message),"INVITATION COLLEE - ENTREE POUR REJOINDRE");}
}
static void copy(Network *n) {
    char text[128];
    if(!net_invitation(n,text,sizeof(text)))snprintf(n->message,sizeof(n->message),"ADRESSE NON DETECTEE - VERIFIE TA CONNEXION RESEAU");
    else if(SDL_SetClipboardText(text)!=0)snprintf(n->message,sizeof(n->message),"COPIE IMPOSSIBLE : %s",SDL_GetError());
    else snprintf(n->message,sizeof(n->message),"INVITATION COPIEE - ENVOIE-LA A TON AMI");
}
static void leave(App *app,Game *g) {
    net_close(app->network);game_reset(g,g->world,g->particle_limit);g->motion=app->motion;g->difficulty=app->difficulty;
    lobby_open(app);snprintf(app->network->message,sizeof(app->network->message),"SALON QUITTE");
}
static void action(App *app,Game *g) {
    Network *n=app->network;
    if(app->state==STATE_LOBBY) {
        if(app->selected==2){leave(app,g);return;}
        if(app->selected==1){copy(n);return;}
        if(n->mode==NET_HOST)net_start(n,g,(double)SDL_GetPerformanceCounter()/(double)SDL_GetPerformanceFrequency());
        else net_ready(n);
        return;
    }
    if(app->selected>=2&&app->selected<=4){edit(app,app->selected);return;}
    if(app->selected==5){stop_edit(app);n->discovery_requested=false;app->state=STATE_MENU;app->selected=0;return;}
    if(app->selected>=6) {
        NearbyRoom *room=&n->rooms[app->selected-6];
        if(!room->name[0])return;
        if(room->phase==ROOM_PLAYING){snprintf(n->message,sizeof(n->message),"PARTIE EN COURS - ATTENDS LA FIN DE LA MANCHE");return;}
        if(room->players>=MAX_PLAYERS){snprintf(n->message,sizeof(n->message),"SALON COMPLET");return;}
        snprintf(n->address,sizeof(n->address),"%s",room->address);
        snprintf(n->port_text,sizeof(n->port_text),"%d",room->port);
    }
    if(app->selected==1&&!n->address[0]) {
        paste(app);
        if(!n->address[0]){edit(app,3);return;}
    }
    bool success=app->selected==0?net_host(n):net_join(n);
    if(success){stop_edit(app);n->discovery_requested=false;app->state=STATE_LOBBY;app->selected=0;}
}
static int hit(Renderer *r,int x,int y,State state) {
    int width,height;SDL_GetWindowSize(r->window,&width,&height);
    if(width<=0||height<=0)return -1;
    float dw=(float)width,dh=dw*9/16;if(dh>height){dh=(float)height;dw=dh*16/9;}
    float px=((float)x-((float)width-dw)/2)*640/dw,py=((float)y-((float)height-dh)/2)*360/dh;
    if(px<76||px>564)return -1;
    if(state==STATE_NETWORK) {
        if(py>=94&&py<214)return (int)((py-94)/20);
        if(py>=237&&py<291)return 6+(int)((py-237)/18);
    }else if(py>=237&&py<297)return (int)((py-237)/20);
    return -1;
}
bool lobby_event(App *app,Game *g,Renderer *r,const SDL_Event *event) {
    if(app->state!=STATE_NETWORK&&app->state!=STATE_LOBBY)return false;
    Network *n=app->network;
    if(event->type==SDL_QUIT){app->running=false;return true;}
    if(app->network_help) {
        if(event->type==SDL_KEYDOWN&&!event->key.repeat) {
            SDL_Scancode key=event->key.keysym.scancode;
            if(key==SDL_SCANCODE_RETURN||key==SDL_SCANCODE_KP_ENTER)
                SDL_OpenURL("https://tailscale.com/download");
            else if(key==SDL_SCANCODE_ESCAPE||key==SDL_SCANCODE_H)app->network_help=false;
        }
        if(event->type==SDL_MOUSEBUTTONDOWN&&event->button.button==SDL_BUTTON_LEFT)app->network_help=false;
        return true;
    }
    if(event->type==SDL_TEXTINPUT&&app->editing>=2){insert(app,event->text.text);return true;}
    if(event->type==SDL_KEYDOWN) {
        SDL_Scancode key=event->key.keysym.scancode;
        bool control=(event->key.keysym.mod&KMOD_CTRL)!=0;
        if(control&&event->key.keysym.sym==SDLK_v&&app->state==STATE_NETWORK){paste(app);return true;}
        if(control&&event->key.keysym.sym==SDLK_c&&app->state==STATE_LOBBY){copy(n);return true;}
        if(app->editing>=2) {
            if(control&&event->key.keysym.sym==SDLK_a){app->select_all=true;return true;}
            if(key==SDL_SCANCODE_RETURN||key==SDL_SCANCODE_KP_ENTER||key==SDL_SCANCODE_ESCAPE){stop_edit(app);return true;}
            if(key==SDL_SCANCODE_TAB){int next=app->editing==4?2:app->editing+1;stop_edit(app);edit(app,next);return true;}
            if(key==SDL_SCANCODE_BACKSPACE) {
                size_t capacity;char *text=field_text(n,app->editing,&capacity);(void)capacity;
                size_t len=strlen(text);if(app->select_all){text[0]=0;app->select_all=false;}else if(len)text[len-1]=0;
            }
            return true;
        }
        if(event->key.repeat)return true;
        if(key==SDL_SCANCODE_H){app->network_help=true;return true;}
        int count=app->state==STATE_NETWORK?6+NET_ROOMS:3;
        if(key==SDL_SCANCODE_UP)app->selected=(app->selected+count-1)%count;
        else if(key==SDL_SCANCODE_DOWN||key==SDL_SCANCODE_TAB)app->selected=(app->selected+1)%count;
        else if(key==SDL_SCANCODE_RETURN||key==SDL_SCANCODE_KP_ENTER||key==SDL_SCANCODE_SPACE)action(app,g);
        else if(key==SDL_SCANCODE_ESCAPE) {
            if(app->state==STATE_LOBBY)leave(app,g);
            else {n->discovery_requested=false;app->state=STATE_MENU;app->selected=0;}
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
            game_reset(g,g->world,g->particle_limit);g->motion=app->motion;g->difficulty=app->difficulty;lobby_open(app);
        }
        return;
    }
    if(n->phase==ROOM_PLAYING&&app->state==STATE_LOBBY) {
        app->capture_error=SDL_SetRelativeMouseMode(SDL_TRUE)!=0;
        app->state=app->capture_error?STATE_PAUSED:STATE_PLAYING;g->motion=app->motion;
    }else if(n->phase==ROOM_FINISHED&&app->state!=STATE_LOBBY) {
        app->state=STATE_LOBBY;app->selected=0;SDL_SetRelativeMouseMode(SDL_FALSE);
        if(g->score>app->best_score)app->best_score=g->score;
    }
}
static void notice(Mesh *m,const char *text,float y) {
    size_t len=strlen(text);char line[75];
    for(size_t offset=0;offset<len&&offset<148;offset+=74) {
        size_t count=len-offset;if(count>74)count=74;memcpy(line,text+offset,count);line[count]=0;
        mesh_text(m,80,y,1,orange,line);y+=10;
    }
}
static void row(Mesh *m,const App *app,int index,float y,const char *text,bool enabled) {
    if(index==app->selected)mesh_rect(m,76,y-5,488,18,(Color){.15f,.22f,.22f});
    mesh_text(m,88,y,1,index==app->selected?blue:muted,index==app->selected?">":" ");
    mesh_text(m,106,y,1,enabled?(index==app->selected?ink:blue):muted,text);
}
void lobby_build(Mesh *m,const Game *g,const App *app) {
    Network *n=app->network;char text[128];
    mesh_rect(m,48,12,544,336,(Color){.055f,.06f,.047f});mesh_rect(m,48,12,544,3,blue);
    mesh_text(m,76,27,1,muted,"FASTFPS " GAME_VERSION " / COOPERATION A 4 JOUEURS");
    centered(m,46,4,ink,app->state==STATE_NETWORK?"JOUER ENSEMBLE":"SALON COOP");
    if(app->network_help) {
        centered(m,88,2,blue,"CONNEXION EN QUELQUES ETAPES");
        mesh_text(m,80,120,1,ink,"MEME RESEAU / MEME WI-FI");
        mesh_text(m,80,137,1,muted,"L'HOTE CREE UN SALON. TON AMI CLIQUE SUR LE SALON DETECTE.");
        mesh_text(m,80,169,1,ink,"A DISTANCE / INTERNET : TAILSCALE");
        mesh_text(m,80,186,1,muted,"1. INSTALLE TAILSCALE SUR LES DEUX MACHINES DE JEU.");
        mesh_text(m,80,202,1,muted,"2. INVITE TON AMI DANS TON RESEAU PRIVE TAILSCALE.");
        mesh_text(m,80,218,1,muted,"3. ACTIVE LE VPN, PUIS CREE LE SALON DANS FASTFPS.");
        mesh_text(m,80,234,1,muted,"4. COPIE L'INVITATION. TON AMI LA COLLE AVEC CTRL+V.");
        centered(m,270,1,blue,"LINUX / PI : ./reseau.sh vpn POUR INSTALLER ET CONNECTER");
        centered(m,290,1,ink,"ENTREE : OUVRIR LE TELECHARGEMENT TAILSCALE");
        centered(m,326,1,muted,"ECHAP / H / CLIC : RETOUR AU SALON");
        return;
    }
    if(app->state==STATE_NETWORK) {
        centered(m,81,1,blue,"MEME RESEAU : CLIC SUR UN SALON / INTERNET : VPN PRIVE");
        for(int i=0;i<6;i++) {
            switch(i) {
                case 0:snprintf(text,sizeof(text),"CREER UN SALON");break;
                case 1:snprintf(text,sizeof(text),"REJOINDRE L'INVITATION");break;
                case 2:snprintf(text,sizeof(text),"PSEUDO : %s%s",n->name,app->editing==2?"_":"");break;
                case 3:snprintf(text,sizeof(text),"INVITATION : %.57s%s",n->address[0]?n->address:"COLLE ICI AVEC CTRL+V",app->editing==3?"_":"");break;
                case 4:snprintf(text,sizeof(text),"PORT : %s (AUTOMATIQUE AVEC UNE INVITATION)",n->port_text);break;
                default:snprintf(text,sizeof(text),"RETOUR");break;
            }
            row(m,app,i,99+(float)i*20,text,true);
        }
        mesh_text(m,82,223,1,muted,"SALONS SUR TON RESEAU - RECHERCHE AUTOMATIQUE");
        for(int i=0;i<NET_ROOMS;i++) {
            const NearbyRoom *room=&n->rooms[i];
            if(room->name[0])snprintf(text,sizeof(text),"%-16s %d/4  %s",room->name,room->players,
                room->phase==ROOM_PLAYING?"EN PARTIE":(room->players==MAX_PLAYERS?"COMPLET":"REJOINDRE"));
            else snprintf(text,sizeof(text),"%s",i==0?(n->discovery_failed?"DETECTION INDISPONIBLE - UTILISE UNE INVITATION":"RECHERCHE... INVITATION POSSIBLE A TOUT MOMENT"):"-");
            row(m,app,6+i,242+(float)i*18,text,room->name[0]&&room->phase!=ROOM_PLAYING&&room->players<MAX_PLAYERS);
        }
        notice(m,n->message,300);
        centered(m,332,1,muted,"CTRL+V : COLLER / ENTREE : VALIDER / H : AIDE RESEAU");
        return;
    }
    snprintf(text,sizeof(text),"CHEZ %s / %s",n->members[0].name,n->mode==NET_HOST?"TU HEBERGES":"TU ES INVITE");centered(m,82,1,blue,text);
    if(n->connecting)centered(m,143,2,ink,"CONNEXION...");
    else for(int i=0;i<MAX_PLAYERS;i++) {
        float y=106+(float)i*23;mesh_rect(m,84,y-4,472,20,(Color){.10f,.13f,.105f});
        snprintf(text,sizeof(text),"%d  %-16s  %s%s",i+1,n->members[i].present?n->members[i].name:"PLACE LIBRE",
            n->members[i].present?(n->members[i].ready?"PRET":"EN ATTENTE"):"",i==0?" / HOTE":"");
        mesh_rect(m,87,y,4,9,i==n->local_id?blue:muted);
        mesh_text(m,98,y,1,i==n->local_id?blue:ink,text);
    }
    if(n->phase==ROOM_FINISHED){snprintf(text,sizeof(text),"FIN - SCORE %d / VAGUE %d",g->score,g->wave);centered(m,197,1,orange,text);}
    char invitation[128];
    if(net_invitation(n,invitation,sizeof(invitation))) {
        snprintf(text,sizeof(text),"INVITATION : %.65s",invitation);centered(m,213,1,ink,text);
        if(n->mode==NET_HOST) {
            bool vpn=n->local_addresses[0].vpn;
            snprintf(text,sizeof(text),"%s",vpn?"ADRESSE VPN - TES AMIS DOIVENT ETRE CONNECTES AU VPN":"ADRESSE LOCALE - POUR INTERNET, CONNECTE UN VPN PRIVE");
        }else if(n->latency_ms>=0)snprintf(text,sizeof(text),"LATENCE : %.0f MS / PRET AUTOMATIQUEMENT",n->latency_ms);
        else snprintf(text,sizeof(text),"MESURE DE LA LATENCE...");
        centered(m,225,1,muted,text);
    }else centered(m,216,1,muted,"ADRESSE NON DETECTEE - VERIFIE TA CONNEXION RESEAU");
    for(int i=0;i<3;i++) {
        const char *label=i==2?"QUITTER LE SALON":(i==1?"COPIER L'INVITATION":(n->mode==NET_HOST?"LANCER LA PARTIE":
            (n->members[n->local_id].ready?"PRET - CLIQUE POUR ATTENDRE":"JE SUIS PRET")));
        row(m,app,i,242+(float)i*20,label,!n->connecting);
    }
    notice(m,n->message,307);
    centered(m,334,1,muted,n->mode==NET_HOST?"CTRL+C : COPIER L'INVITATION / L'HOTE LANCE LA PARTIE":"PRET PAR DEFAUT / L'HOTE LANCE / ECHAP : QUITTER");
}
