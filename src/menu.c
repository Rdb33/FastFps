#include "fastfps.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

int menu_count(const App *a){return a->menu_page?10:4;}
static float ease(float t){t=clampf(t,0,1);return t*t*(3-2*t);}
static Color mix(Color a,Color b,float t){return (Color){a.r+(b.r-a.r)*t,a.g+(b.g-a.g)*t,a.b+(b.b-a.b)*t};}
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
void menu_animate(App *a,float dt) {
    bool visible=a->state==STATE_MENU||a->state==STATE_PAUSED||a->state==STATE_DEAD;
    int row=0;for(int i=0;i<menu_count(a);i++)if(menu_row_id(a,i)==a->selected)row=i;
    if(a->menu_visual_state!=a->state||a->menu_visual_page!=a->menu_page) {
        a->menu_age=0;a->menu_highlight=(float)row;
    }
    a->menu_visual_state=a->state;a->menu_visual_page=a->menu_page;
    if(!visible){a->menu_age=0;return;}
    a->menu_age=fminf(1,a->menu_age+dt);
    if(a->motion)a->menu_highlight+=((float)row-a->menu_highlight)*fminf(1,dt*18);
    else a->menu_highlight=(float)row;
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
    char text[160];int first=m->count;float time=a->motion?a->ui_time:0;
    mesh_rect(m,48,16,544,328,(Color){.025f,.045f,.064f});
    mesh_rect(m,48,16,544,2,blue);mesh_rect(m,48,342,544,2,(Color){.1f,.23f,.30f});
    for(int i=0;i<6;i++)mesh_rect(m,368+(float)i*36,26,1,63,(Color){.055f,.1f,.14f});
    if(a->motion)mesh_rect(m,368,27+fmodf(time*12,59),206,1,(Color){.085f,.17f,.22f});
    mesh_text(m,64,27,1,muted,"FASTFPS " GAME_VERSION " / SURVIE COOPERATIVE");
    mesh_text(m,64,44,7,ink,"FASTFPS");
    mesh_text(m,427,45,1,blue,a->menu_page?"REGLAGES":"SECTEUR ZERO");
    snprintf(text,sizeof(text),"RECORD %d",a->best_score);mesh_text(m,427,61,1,muted,text);
    for(int i=0;i<12;i++) {
        float angle=(float)i*2*PI_F/12+time*.30f,next=angle+.32f;
        Color c=mix((Color){.10f,.23f,.30f},blue,.30f+.20f*sinf(time*2+(float)i));
        mesh_quad(m,(Vec3){396+cosf(angle)*22,62+sinf(angle)*22,0},
            (Vec3){396+cosf(next)*22,62+sinf(next)*22,0},
            (Vec3){396+cosf(next)*20,62+sinf(next)*20,0},
            (Vec3){396+cosf(angle)*20,62+sinf(angle)*20,0},c,(Vec3){0,0,1});
    }
    float pulse=.7f+.2f*sinf(time*2);
    mesh_triangle(m,(Vec3){396,46,0},(Vec3){406,62,0},(Vec3){396,78,0},mix(muted,blue,pulse),(Vec3){0,0,1});
    mesh_triangle(m,(Vec3){396,46,0},(Vec3){396,78,0},(Vec3){386,62,0},mix(ink,blue,pulse),(Vec3){0,0,1});
    if(a->state==STATE_DEAD)center(m,95,2,(Color){1,.53f,.24f},"GAME OVER");
    else center(m,96,1,blue,a->menu_page?"AJUSTE TON JEU / REGLAGES SAUVEGARDES":(a->state==STATE_PAUSED?"PARTIE EN PAUSE":"BOUGE. COOPERE. SURVIS."));
    if(a->state==STATE_DEAD) {
        snprintf(text,sizeof(text),"%s / SCORE %d / %d ROUNDS",g->cooperative?"EQUIPE A TERRE":"TROIS VIES EPUISEES",g->score,g->completed_waves);
        center(m,115,1,muted,text);
    }else if(a->state==STATE_PAUSED)center(m,115,1,muted,g->cooperative?"LE MONDE CONTINUE POUR TES COEQUIPIERS":"REPRENDS OU AJUSTE LES REGLAGES");
    if(!a->menu_page) {
        for(int row=0;row<4;row++) {
            int id=menu_row_id(a,row);float y=128+(float)row*42;
            float weight=clampf(1-fabsf((float)row-a->menu_highlight),0,1);
            float entry=a->motion?ease((a->menu_age-(float)row*.035f)/.18f):1;
            float text_x=78+(1-entry)*8;
            mesh_rect(m,64,y,278,37,mix((Color){.065f,.11f,.15f},(Color){.12f,.24f,.31f},weight));
            mesh_rect(m,64,y,3,37,mix((Color){.12f,.20f,.25f},blue,weight));
            if(weight>.01f)mesh_rect(m,68,y+34,270*weight,1,(Color){.20f,.42f,.53f});
            const char *title,*description;
            if(id==0){title=g->cooperative&&a->state==STATE_DEAD?"RETOUR AU SALON":(a->state==STATE_PAUSED?"REPRENDRE":(a->state==STATE_DEAD?"RECOMMENCER":"JOUER EN SOLO"));description=a->state==STATE_PAUSED?"RETOUR IMMEDIAT AU COMBAT":(g->cooperative&&a->state==STATE_DEAD?"PREPARER LA PROCHAINE PARTIE":"TROIS VIES / MODULES / ARSENAL");}
            else if(id==1){title=g->cooperative?"QUITTER LE SALON":(a->state==STATE_PAUSED?"NOUVELLE PARTIE":"MULTIJOUEUR");description=g->cooperative?"DECONNECTER CET ORDINATEUR":(a->state==STATE_PAUSED?"RECOMMENCER LE MODE SOLO":"JUSQU'A 4 JOUEURS / LAN OU VPN");}
            else if(id==11){title="REGLAGES";description="IMAGE / SOURIS / SON / DIFFICULTE";}
            else {title="QUITTER";description="FERMER FASTFPS";}
            mesh_text(m,text_x,y+7,2,mix(muted,ink,weight*entry),title);
            mesh_text(m,text_x,y+25,1,mix(muted,blue,weight*entry),description);
        }
        mesh_rect(m,362,128,214,163,(Color){.04f,.08f,.11f});
        mesh_text(m,376,141,1,blue,"PREPARE TA PROCHAINE VAGUE");
        snprintf(text,sizeof(text),"MAP : %s",world_map_name(g->map_id));mesh_text(m,376,162,1,ink,text);
        snprintf(text,sizeof(text),"ROUND %d / NIVEAU %d",g->wave,player_level(&g->player));mesh_text(m,376,178,1,muted,text);
        mesh_rect(m,376,195,184,1,(Color){.1f,.24f,.3f});
        mesh_text(m,376,209,1,ink,"CLIC DROIT : ESQUIVE");
        mesh_text(m,376,225,1,ink,"R : RECHARGER / 1-5 : ARMES");
        mesh_text(m,376,241,1,blue,"E MAINTENU : REANIMER");
        mesh_text(m,376,259,1,muted,"LIFTS : HAUTEURS ET SOINS");
        mesh_text(m,376,275,1,blue,world_route_name(g->map_id));
        center(m,312,1,muted,"FLECHES OU SOURIS / ENTREE POUR JOUER");
    }else {
        mesh_rect(m,64,128+a->menu_highlight*18,512,17,(Color){.1f,.2f,.26f});
        mesh_rect(m,64,128+a->menu_highlight*18,2,17,blue);
        for(int row=0;row<menu_count(a);row++) {
            int id=menu_row_id(a,row);float y=133+(float)row*18;bool selected=id==a->selected;
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
    if(a->motion&&a->menu_age<.24f) {
        float fade=.25f+.75f*ease(a->menu_age/.24f);
        for(int i=first;i<m->count;i++) {
            m->data[i].color.r*=fade;m->data[i].color.g*=fade;m->data[i].color.b*=fade;
        }
    }
}
