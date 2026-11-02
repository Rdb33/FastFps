#include "fastfps.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

int menu_count(const App *a){return a->menu_page?11:4;}
static float ease(float t){t=clampf(t,0,1);return t*t*(3-2*t);}
static Color mix(Color a,Color b,float t){return (Color){a.r+(b.r-a.r)*t,a.g+(b.g-a.g)*t,a.b+(b.b-a.b)*t};}
bool menu_visible(const App *a) {
    return a->state==STATE_MENU||a->state==STATE_PAUSED||a->state==STATE_DEAD
        ||a->state==STATE_NETWORK||a->state==STATE_LOBBY;
}
Vec3 menu_offset(const App *a,float depth) {
    if(!a->motion||!menu_visible(a))return (Vec3){0,0,0};
    return (Vec3){-a->menu_parallax_x*depth*12,-a->menu_parallax_y*depth*7,0};
}
/* La profondeur est un marqueur temporaire du maillage UI. Elle est consommée
   avant l'envoi au GPU ; aucune perspective ne déforme les textes. */
void menu_layer(Mesh *m,int first,float depth) {
    for(int i=first;i<m->count;i++)m->data[i].position.z=depth;
}
void menu_parallax_apply(Mesh *m,const App *a) {
    if(!menu_visible(a))return;
    for(int i=0;i<m->count;i++) {
        Vertex *v=&m->data[i];Vec3 offset=menu_offset(a,v->position.z>0?v->position.z:MENU_CONTROL_DEPTH);
        v->position.x+=offset.x;v->position.y+=offset.y;v->position.z=0;
    }
}
bool menu_coordinates(const App *a,const Renderer *r,int x,int y,float *px,float *py) {
    int w,h;SDL_GetWindowSize(r->window,&w,&h);if(w<=0||h<=0)return false;
    float dw=(float)w,dh=dw*9/16;if(dh>h){dh=(float)h;dw=dh*16/9;}
    Vec3 offset=menu_offset(a,MENU_CONTROL_DEPTH);
    *px=((float)x-((float)w-dw)/2)*640/dw-offset.x;
    *py=((float)y-((float)h-dh)/2)*360/dh-offset.y;return true;
}
int menu_row_id(const App *a,int row) {
    static const int main_ids[4]={0,1,11,10};
    if(row<0||row>=menu_count(a))return -1;
    return a->menu_page?(row<8?row+2:(row==8?13:(row==9?14:12))):main_ids[row];
}
void menu_move(App *a,int direction) {
    int count=menu_count(a),row=0;
    for(int i=0;i<count;i++)if(menu_row_id(a,i)==a->selected)row=i;
    a->selected=menu_row_id(a,(row+direction+count)%count);
}
void menu_pointer_update(App *a,const Renderer *r) {
    a->menu_pointer_x=a->menu_pointer_y=0;
    if(SDL_GetMouseFocus()!=r->window)return;
    int x,y,w,h;SDL_GetMouseState(&x,&y);SDL_GetWindowSize(r->window,&w,&h);
    if(w<=0||h<=0)return;
    float dw=(float)w,dh=dw*9/16;if(dh>h){dh=(float)h;dw=dh*16/9;}
    a->menu_pointer_x=clampf(((float)x-(float)w*.5f)/(dw*.5f),-1,1);
    a->menu_pointer_y=clampf(((float)y-(float)h*.5f)/(dh*.5f),-1,1);
}
void menu_animate(App *a,float dt) {
    bool visible=a->state==STATE_MENU||a->state==STATE_PAUSED||a->state==STATE_DEAD;
    bool pointer=menu_visible(a);
    float blend=1-expf(-8*dt);
    if(a->motion) {
        a->menu_parallax_x+=((pointer?a->menu_pointer_x:0)-a->menu_parallax_x)*blend;
        a->menu_parallax_y+=((pointer?a->menu_pointer_y:0)-a->menu_parallax_y)*blend;
    }else a->menu_parallax_x=a->menu_parallax_y=0;
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
    float px,py;if(!menu_coordinates(a,r,x,y,&px,&py))return -1;
    if(px<64||px>(a->menu_page?576:342)||py<128)return -1;
    float step=a->menu_page?SETTINGS_ROW_HEIGHT:42;int row=(int)((py-128)/step);
    if(!a->menu_page&&py-128-(float)row*step>38)return -1;
    return menu_row_id(a,row);
}
static void center(Mesh *m,float y,int scale,Color c,const char *text) {
    mesh_text(m,(640-(float)strlen(text)*6*(float)scale)/2,y,scale,c,text);
}
static const Color ink={.91f,.96f,1},blue={.32f,.8f,1},muted={.51f,.64f,.72f};
void ui_music_visualizer(Mesh *m,const App *a,float x,float baseline,float width,float height) {
    float step=width/MUSIC_VISUAL_BANDS;
    for(int i=0;i<MUSIC_VISUAL_BANDS;i++) {
        float level=clampf(a->music_bands[i],0,1);
        float h=floorf(level*height*(a->motion?1:.45f)),bx=x+(float)i*step;
        mesh_rect(m,bx,baseline-1,step-2,1,(Color){.08f,.19f,.24f});
        for(float y=0;y<h;y+=3) {
            Color c=y>height*.75f?(Color){1,.72f,.25f}:(Color){.25f,.85f,1};
            mesh_rect(m,bx,baseline-y-fminf(2,h-y),step-2,fminf(2,h-y),c);
        }
        float peak=floorf(a->music_peaks[i]*height*(a->motion?1:.45f));
        if(peak>=1)mesh_rect(m,bx,baseline-peak,step-2,1,(Color){.9f,.97f,1});
    }
}
void menu_build(Mesh *m,const Game *g,const App *a) {
    char text[160];int first=m->count;float time=a->motion?a->ui_time:0;
    mesh_rect(m,48,16,544,328,(Color){.025f,.045f,.064f});
    mesh_rect(m,48,16,544,2,blue);mesh_rect(m,48,342,544,2,(Color){.1f,.23f,.30f});
    menu_layer(m,first,.25f);int grid=m->count;
    for(int i=0;i<6;i++)mesh_rect(m,368+(float)i*36,26,1,63,(Color){.055f,.1f,.14f});
    if(a->motion)mesh_rect(m,368,27+fmodf(time*12,59),206,1,(Color){.085f,.17f,.22f});
    menu_layer(m,grid,.45f);
    mesh_text(m,64,27,1,muted,"FASTFPS " GAME_VERSION " / SURVIE COOPERATIVE");
    int title=m->count;mesh_text(m,64,44,7,ink,"FASTFPS");menu_layer(m,title,1.1f);
    mesh_text(m,427,45,1,blue,a->menu_page?"REGLAGES":"SECTEUR ZERO");
    snprintf(text,sizeof(text),"RECORD %d",a->best_score);mesh_text(m,427,61,1,muted,text);
    mesh_text(m,427,80,1,muted,"MUSIQUE");
    ui_music_visualizer(m,a,475,87,96,12);
    int orb=m->count;
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
    menu_layer(m,orb,1.35f);
    if(a->state==STATE_DEAD)center(m,95,2,(Color){1,.53f,.24f},"PARTIE TERMINEE");
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
        int card=m->count;
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
        menu_layer(m,card,.6f);
        center(m,312,1,muted,"FLECHES OU SOURIS / ENTREE POUR JOUER");
    }else {
        mesh_rect(m,64,128+a->menu_highlight*SETTINGS_ROW_HEIGHT,512,15,(Color){.1f,.2f,.26f});
        mesh_rect(m,64,128+a->menu_highlight*SETTINGS_ROW_HEIGHT,2,15,blue);
        for(int row=0;row<menu_count(a);row++) {
            int id=menu_row_id(a,row);float y=133+(float)row*SETTINGS_ROW_HEIGHT;bool selected=id==a->selected;
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
                case 14:snprintf(text,sizeof(text),"INTENSITE DES IMPACTS");break;
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
