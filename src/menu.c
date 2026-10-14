#include "fastfps.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

int menu_count(const App *a){return a->menu_page?10:4;}
int menu_row_id(const App *a,int row) {
    static const int main_ids[4]={0,1,11,10};
    if(row<0||row>=menu_count(a))return -1;
    return a->menu_page?(row<8?row+2:(row==8?13:12)):main_ids[row];
}
void menu_move(App *a,int direction) {
    int count=menu_count(a),row=0;
    for(int i=0;i<count;i++)if(menu_row_id(a,i)==a->selected)row=i;
    a->selected=menu_row_id(a,(row+direction+count)%count);
}
int menu_hit(const App *a,Renderer *r,int x,int y) {
    int width,height;SDL_GetWindowSize(r->window,&width,&height);if(width<=0||height<=0)return -1;
    float dw=(float)width,dh=dw*9/16;if(dh>height){dh=(float)height;dw=dh*16/9;}
    float px=((float)x-((float)width-dw)/2)*640/dw,py=((float)y-((float)height-dh)/2)*360/dh;
    if(px<64||px>(a->menu_page?576:342)||py<128)return -1;
    float step=a->menu_page?18:42;int row=(int)((py-128)/step);
    if(!a->menu_page&&py-128-(float)row*step>38)return -1;
    return menu_row_id(a,row);
}
static void center(Mesh *m,float y,int scale,Color c,const char *text) {
    mesh_text(m,(640-(float)strlen(text)*6*(float)scale)/2,y,scale,c,text);
}
static const Color ink={.91f,.96f,1},blue={.32f,.8f,1},muted={.51f,.64f,.72f};
void menu_build(Mesh *m,const Game *g,const App *a) {
    char text[160];
    mesh_rect(m,48,16,544,328,(Color){.025f,.045f,.064f});
    mesh_rect(m,48,16,544,2,blue);mesh_rect(m,48,342,544,2,(Color){.1f,.23f,.30f});
    for(int i=0;i<6;i++)mesh_rect(m,368+(float)i*36,26,1,63,(Color){.055f,.1f,.14f});
    mesh_text(m,64,27,1,muted,"FASTFPS " GAME_VERSION " / SURVIE COOPERATIVE");
    mesh_text(m,64,44,7,ink,"FASTFPS");
    mesh_text(m,427,45,1,blue,a->menu_page?"REGLAGES":"SECTEUR ZERO");
    snprintf(text,sizeof(text),"RECORD %d",a->best_score);mesh_text(m,427,61,1,muted,text);
    for(int i=0;i<3;i++) {
        float x=396+(float)i*5,y=62;float pulse=.6f+.3f*sinf(a->ui_time*2+(float)i);
        Color c={blue.r*pulse,blue.g*pulse,blue.b*pulse};
        mesh_triangle(m,(Vec3){x,y-16,0},(Vec3){x+11,y,0},(Vec3){x,y+16,0},c,(Vec3){0,0,1});
    }
    if(a->state==STATE_DEAD)center(m,95,2,(Color){1,.53f,.24f},"GAME OVER");
    else center(m,96,1,blue,a->menu_page?"AJUSTE TON JEU / REGLAGES SAUVEGARDES":(a->state==STATE_PAUSED?"PARTIE EN PAUSE":"BOUGE. COOPERE. SURVIS."));
    if(a->state==STATE_DEAD) {
        snprintf(text,sizeof(text),"%s / SCORE %d / %d ROUNDS",g->cooperative?"EQUIPE A TERRE":"TROIS VIES EPUISEES",g->score,g->completed_waves);
        center(m,115,1,muted,text);
    }else if(a->state==STATE_PAUSED)center(m,115,1,muted,g->cooperative?"LE MONDE CONTINUE POUR TES COEQUIPIERS":"REPRENDS OU AJUSTE LES REGLAGES");
    if(!a->menu_page) {
        for(int row=0;row<4;row++) {
            int id=menu_row_id(a,row);float y=128+(float)row*42;bool selected=a->selected==id;
            mesh_rect(m,64,y,278,37,selected?(Color){.12f,.24f,.31f}:(Color){.065f,.11f,.15f});
            mesh_rect(m,64,y,3,37,selected?blue:(Color){.12f,.20f,.25f});
            const char *title,*description;
            if(id==0){title=g->cooperative&&a->state==STATE_DEAD?"RETOUR AU SALON":(a->state==STATE_PAUSED?"REPRENDRE":(a->state==STATE_DEAD?"RECOMMENCER":"JOUER EN SOLO"));description=a->state==STATE_PAUSED?"RETOUR IMMEDIAT AU COMBAT":(g->cooperative&&a->state==STATE_DEAD?"PREPARER LA PROCHAINE PARTIE":"TROIS VIES / MODULES / ARSENAL");}
            else if(id==1){title=g->cooperative?"QUITTER LE SALON":(a->state==STATE_PAUSED?"NOUVELLE PARTIE":"MULTIJOUEUR");description=g->cooperative?"DECONNECTER CET ORDINATEUR":(a->state==STATE_PAUSED?"RECOMMENCER LE MODE SOLO":"JUSQU'A 4 JOUEURS / LAN OU VPN");}
            else if(id==11){title="REGLAGES";description="IMAGE / SOURIS / SON / DIFFICULTE";}
            else {title="QUITTER";description="FERMER FASTFPS";}
            mesh_text(m,78,y+7,2,selected?ink:muted,title);
            mesh_text(m,78,y+25,1,selected?blue:muted,description);
        }
        mesh_rect(m,362,128,214,163,(Color){.04f,.08f,.11f});
        mesh_text(m,376,141,1,blue,"PREPARE TA PROCHAINE VAGUE");
        snprintf(text,sizeof(text),"MAP : %s",world_map_name(g->map_id));mesh_text(m,376,162,1,ink,text);
        snprintf(text,sizeof(text),"ROUND %d / NIVEAU %d",g->wave,player_level(&g->player));mesh_text(m,376,178,1,muted,text);
        mesh_rect(m,376,195,184,1,(Color){.1f,.24f,.3f});
        mesh_text(m,376,209,1,ink,"CLIC DROIT : ESQUIVE");
        mesh_text(m,376,225,1,ink,"R : RECHARGER / 1-5 : ARMES");
        mesh_text(m,376,241,1,blue,"E MAINTENU : REANIMER");
        mesh_text(m,376,259,1,muted,"PLATEFORMES : SAUTE DESSUS");
        mesh_text(m,376,275,1,muted,"TAB : MODULES ET ARSENAL");
        center(m,312,1,muted,"FLECHES OU SOURIS / ENTREE POUR JOUER");
    }else {
        for(int row=0;row<menu_count(a);row++) {
            int id=menu_row_id(a,row);float y=133+(float)row*18;bool selected=id==a->selected;
            if(selected)mesh_rect(m,64,y-5,512,17,(Color){.1f,.2f,.26f});
            switch(id) {
                case 2:snprintf(text,sizeof(text),"RENDU : %s / %d X %d / %d FPS",PRESETS[a->quality].name,PRESETS[a->quality].width,PRESETS[a->quality].height,PRESETS[a->quality].fps);break;
                case 3:snprintf(text,sizeof(text),"SENSIBILITE (0.30-3.00)");break;
                case 4:snprintf(text,sizeof(text),"EFFETS SONORES : %s / F3",a->sound?"ACTIFS":"COUPES");break;
                case 5:snprintf(text,sizeof(text),"PLEIN ECRAN : %s",a->fullscreen?"ACTIF":"NON");break;
                case 6:snprintf(text,sizeof(text),"CAMERA : %s",a->motion?"ANIMEE":"MOUVEMENTS REDUITS");break;
                case 7:{const char *names[]={"DETENTE","NORMAL","BRUTAL"};snprintf(text,sizeof(text),"DIFFICULTE%s : %s",g->cooperative?" DE L'HOTE":"",names[g->cooperative?g->difficulty:a->difficulty]);break;}
                case 8:snprintf(text,sizeof(text),"VISUEL : %s / F5",a->modern?"MODERNE":"RETRO");break;
                case 9:snprintf(text,sizeof(text),"LUMINOSITE (50-200%%)");break;
                case 13:snprintf(text,sizeof(text),"MUSIQUE (0-100%%)");break;
                default:snprintf(text,sizeof(text),"RETOUR");break;
            }
            mesh_text(m,74,y,1,selected?blue:muted,selected?">":" ");
            mesh_text(m,87,y,1,selected?ink:muted,text);settings_controls(m,a,id,y);
        }
        center(m,310,1,muted,"SLIDER : GLISSER / VALEUR : SAISIR / ENTREE : VALIDER");
    }
    if(a->capture_error)center(m,326,1,(Color){1,.55f,.25f},"CAPTURE SOURIS REFUSEE / REESSAIE DEPUIS UNE SESSION GRAPHIQUE");
    else center(m,329,1,muted,"ECHAP : RETOUR / F2 : QUALITE / F11 : PLEIN ECRAN");
}
