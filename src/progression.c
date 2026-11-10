#include "fastfps.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

float player_max_health(const Player *p){return 100+15*(float)p->upgrades[UPGRADE_VITALITY];}
float player_damage_multiplier(const Player *p){return 1+.12f*(float)p->upgrades[UPGRADE_POWER];}
float player_fire_interval(const Player *p){return WEAPONS[p->weapon].interval/(1+.10f*(float)p->upgrades[UPGRADE_HASTE]);}
float player_dash_cooldown(const Player *p){return DASH_COOLDOWN*(1-.10f*(float)p->upgrades[UPGRADE_MOBILITY]);}
int player_level(const Player *p){int level=1;for(int i=0;i<UPGRADE_COUNT;i++)level+=p->upgrades[i];return level;}
float enemy_size(const Enemy *e){return (e->type==ENEMY_BRUTE?1.3f:(e->type==ENEMY_BOMBER?1.15f:1))*(e->elite?1.22f:1);}

const char *upgrade_name(int id) {
    static const char *names[]={"SURCHARGE","CADENCE","REFLEXES","VITALITE","BLINDAGE","VAMPIRISME","PRECISION","ASSAUT","ALTERNANCE","PERFORATION"};
    return id>=0&&id<UPGRADE_COUNT?names[id]:"MODULE";
}
const char *upgrade_description(int id) {
    static const char *text[]={"DEGATS DES ARMES +12%","CADENCE DE TIR +10%","ESQUIVE -10% / VITESSE +3%",
        "SANTE MAX +15 / SOIN +15","DEGATS RECUS -6%","ELIMINATION : +1 SANTE",
        "TETE FATALE : +1 MUNITION", "APRES ESQUIVE : DEGATS +8%",
        "CHANGER : PREMIER TIR +10%", "TRAVERSE 1 / 2E : 35% +10%"};
    return id>=0&&id<UPGRADE_COUNT?text[id]:"";
}
Color upgrade_color(int id) {
    static const Color colors[]={{1,.46f,.18f},{1,.78f,.24f},{.27f,.83f,1},{.4f,1,.53f},{.54f,.63f,1},{.83f,.42f,1},{.4f,1,1},{1,.35f,.22f},{1,.8f,.4f},{.7f,.55f,1}};
    return id>=0&&id<UPGRADE_COUNT?colors[id]:(Color){.7f,.7f,.6f};
}

void game_offer_upgrades(Game *g) {
    for(int actor=0;actor<MAX_PLAYERS;actor++)if(g->actors[actor].active) {
        Player *p=actor==g->current_actor?&g->player:&g->actors[actor].player;
        int pool[UPGRADE_COUNT],count=0;
        for(int id=0;id<UPGRADE_COUNT;id++)if(p->upgrades[id]<UPGRADE_MAX_LEVEL)pool[count++]=id;
        if(!count){p->upgrade_pending=false;continue;}
        /* Ordre déterministe et distinct pour chaque joueur ; aucun état global RNG. */
        uint32_t seed=(uint32_t)g->wave*747796405u+(uint32_t)(actor+1)*2891336453u;
        for(int i=count-1;i>0;i--) {
            seed=seed*1664525u+1013904223u;
            int j=(int)(seed%(uint32_t)(i+1)),swap=pool[i];pool[i]=pool[j];pool[j]=swap;
        }
        for(int i=0;i<UPGRADE_CHOICES;i++)p->offers[i]=(uint8_t)pool[i%count];
        p->upgrade_pending=true;p->upgrade_lock=UPGRADE_LOCK_TIME;
    }
}

bool game_choose_upgrade(Game *g,int choice) {
    Player *p=&g->player;
    if(g->prediction||p->upgrade_lock>0||!p->upgrade_pending||choice<1||choice>UPGRADE_CHOICES)return false;
    int id=p->offers[choice-1];
    if(id>=UPGRADE_COUNT||p->upgrades[id]>=UPGRADE_MAX_LEVEL)return false;
    p->upgrades[id]++;p->last_upgrade=(uint8_t)id;p->upgrade_pending=false;p->upgrade_flash=1.4f;
    if(id==UPGRADE_VITALITY&&!g->dead)p->health=fminf(player_max_health(p),p->health+15);
    if(g->current_actor==g->local_actor)audio_play(SOUND_UPGRADE);
    return true;
}

void upgrade_sync(App *app,const Game *g,bool demo) {
    if(demo)return;
    bool pending=g->player.upgrade_pending&&!g->dead;
    if(app->state==STATE_PLAYING&&pending) {
        /* Laisser apparaître la confirmation visuelle du dernier coup avant
           les cartes ; le délai de choix autoritaire continue normalement. */
        if(app->impact_intensity>0&&g->wave_clear_time>2.50f)return;
        app->state=STATE_UPGRADE;app->selected=0;app->upgrade_choice=0;
        app->upgrade_ready_at=SDL_GetTicks()+1000;
        app->upgrade_released=!(SDL_GetMouseState(NULL,NULL)&SDL_BUTTON(SDL_BUTTON_LEFT));
        app->upgrade_confirm=false;
        SDL_SetRelativeMouseMode(SDL_FALSE);
    }else if(app->state==STATE_UPGRADE&&!pending) {
        app->upgrade_choice=0;app->capture_error=SDL_SetRelativeMouseMode(SDL_TRUE)!=0;
        app->state=app->capture_error?STATE_PAUSED:STATE_PLAYING;app->selected=0;
        int x,y;SDL_GetRelativeMouseState(&x,&y);
    }
}

static int card_hit(Renderer *r,int x,int y) {
    int width,height;SDL_GetWindowSize(r->window,&width,&height);
    if(width<=0||height<=0)return -1;
    float dw=(float)width,dh=dw*9/16;if(dh>height){dh=(float)height;dw=dh*16/9;}
    float px=((float)x-((float)width-dw)/2)*640/dw,py=((float)y-((float)height-dh)/2)*360/dh;
    if(py<122||py>248)return -1;
    for(int i=0;i<UPGRADE_CHOICES;i++){float left=42+(float)i*188;if(px>=left&&px<=left+180)return i;}
    return -1;
}
static bool confirm_hit(Renderer *r,int x,int y) {
    int width,height;SDL_GetWindowSize(r->window,&width,&height);
    if(width<=0||height<=0)return false;
    float dw=(float)width,dh=dw*9/16;if(dh>height){dh=(float)height;dw=dh*16/9;}
    float px=((float)x-((float)width-dw)/2)*640/dw,py=((float)y-((float)height-dh)/2)*360/dh;
    return px>=220&&px<=420&&py>=257&&py<=277;
}
bool upgrade_event(App *app,const Game *g,Renderer *r,const SDL_Event *event) {
    if(app->state!=STATE_UPGRADE)return false;
    if(event->type==SDL_MOUSEBUTTONUP&&event->button.button==SDL_BUTTON_LEFT){app->upgrade_released=true;return true;}
    bool ready=SDL_TICKS_PASSED(SDL_GetTicks(),app->upgrade_ready_at)&&g->player.upgrade_lock<=0;
    if(event->type==SDL_MOUSEMOTION) {
        int hit=card_hit(r,event->motion.x,event->motion.y);
        if(hit>=0&&hit!=app->selected){app->selected=hit;app->upgrade_confirm=false;}
        return true;
    }
    if(event->type==SDL_MOUSEBUTTONDOWN&&event->button.button==SDL_BUTTON_LEFT) {
        if(!ready||!app->upgrade_released||app->upgrade_choice)return true;
        int hit=card_hit(r,event->button.x,event->button.y);
        if(hit>=0&&g->player.upgrade_pending) {
            app->selected=hit;app->upgrade_confirm=true;
        }else if(app->upgrade_confirm&&confirm_hit(r,event->button.x,event->button.y))app->upgrade_choice=app->selected+1;
        return true;
    }
    if(event->type==SDL_KEYDOWN&&!event->key.repeat) {
        SDL_Scancode key=event->key.keysym.scancode;
        if(key==SDL_SCANCODE_ESCAPE)return false;
        if(!ready||app->upgrade_choice)return true;
        if(key==SDL_SCANCODE_LEFT||key==SDL_SCANCODE_A){app->selected=(app->selected+2)%3;app->upgrade_confirm=false;}
        else if(key==SDL_SCANCODE_RIGHT||key==SDL_SCANCODE_D||key==SDL_SCANCODE_TAB){app->selected=(app->selected+1)%3;app->upgrade_confirm=false;}
        else if(key==SDL_SCANCODE_RETURN||key==SDL_SCANCODE_KP_ENTER)app->upgrade_choice=app->selected+1;
        else if(key>=SDL_SCANCODE_1&&key<=SDL_SCANCODE_3){app->selected=key-SDL_SCANCODE_1;app->upgrade_confirm=true;}
        else if(key>=SDL_SCANCODE_KP_1&&key<=SDL_SCANCODE_KP_3){app->selected=key-SDL_SCANCODE_KP_1;app->upgrade_confirm=true;}
        return true;
    }
    return false;
}

static void centered(Mesh *m,float y,int scale,Color c,const char *text) {
    mesh_text(m,(640-(float)strlen(text)*6*(float)scale)/2,y,scale,c,text);
}
/* Valeurs permanentes du module : les bonus temporaires ne brouillent pas
   la comparaison. Le tir et l'esquive réutilisent leurs formules de gameplay. */
static void upgrade_preview(const Player *p,int id,char *first,size_t first_size,char *second,size_t second_size) {
    Player next=*p;int rank=p->upgrades[id];next.upgrades[id]++;
    switch(id) {
    case UPGRADE_POWER:
        snprintf(first,first_size,"DEGATS +%d%% -> +%d%%",rank*12,(rank+1)*12);
        snprintf(second,second_size,"TOUTES LES ARMES");break;
    case UPGRADE_HASTE:
        snprintf(first,first_size,"CADENCE +%d%% -> +%d%%",rank*10,(rank+1)*10);
        snprintf(second,second_size,"TIR %.3f -> %.3f S",player_fire_interval(p),player_fire_interval(&next));break;
    case UPGRADE_MOBILITY:
        snprintf(first,first_size,"ESQUIVE %.2f -> %.2f S",player_dash_cooldown(p),player_dash_cooldown(&next));
        snprintf(second,second_size,"VITESSE +%d%% -> +%d%%",rank*3,(rank+1)*3);break;
    case UPGRADE_VITALITY:
        snprintf(first,first_size,"SANTE MAX %.0f -> %.0f",player_max_health(p),player_max_health(&next));
        snprintf(second,second_size,"SOIGNE AUSSI DE 15 PV");break;
    case UPGRADE_ARMOR:
        snprintf(first,first_size,"REDUCTION %d%% -> %d%%",rank*6,(rank+1)*6);
        snprintf(second,second_size,"SUR LES DEGATS RECUS");break;
    case UPGRADE_VAMPIRE:
        snprintf(first,first_size,"SOIN %d -> %d PV",rank,rank+1);
        snprintf(second,second_size,"PAR ELIMINATION");break;
    case UPGRADE_PRECISION:
        snprintf(first,first_size,"MUNITIONS +%d -> +%d",rank,rank+1);
        snprintf(second,second_size,"PAR TETE FATALE");break;
    case UPGRADE_DASH_POWER:
        snprintf(first,first_size,"DEGATS +%d%% -> +%d%%",rank*8,(rank+1)*8);
        snprintf(second,second_size,"APRES ESQUIVE : 1.25 S");break;
    case UPGRADE_SWITCH_POWER:
        snprintf(first,first_size,"DEGATS +%d%% -> +%d%%",rank*10,(rank+1)*10);
        snprintf(second,second_size,"1ER TIR APRES CHANGEMENT");break;
    case UPGRADE_PIERCE:
        snprintf(first,first_size,"2E CIBLE %d%% -> %d%%",rank?35+rank*10:0,35+(rank+1)*10);
        snprintf(second,second_size,"TRAVERSE UN ENNEMI");break;
    default:first[0]=second[0]='\0';break;
    }
}
static const char *upgrade_advice(const Player *p,int id) {
    switch(id) {
    case UPGRADE_POWER:return p->upgrades[UPGRADE_PIERCE]?"SYNERGIE : LA SURCHARGE RENFORCE AUSSI LE TIR QUI TRAVERSE.":"SURCHARGE : AUGMENTE AUSSI LES DEGATS DE TA FRAPPE.";
    case UPGRADE_HASTE:return "CADENCE : L'INTERVALLE AFFICHE EST CELUI DE TON ARME ACTUELLE.";
    case UPGRADE_MOBILITY:return p->upgrades[UPGRADE_DASH_POWER]?"SYNERGIE : DES ESQUIVES PLUS FREQUENTES POUR DECLENCHER ASSAUT.":"REFLEXES : PLUS DE MOBILITE POUR EVITER LES BOMBES ET LES TOILES.";
    case UPGRADE_VITALITY:return p->upgrades[UPGRADE_VAMPIRE]?"SYNERGIE : UNE RESERVE DE SANTE PLUS GRANDE POUR LE VAMPIRISME.":"VITALITE : LA SANTE MAXIMALE ET LE SOIN AUGMENTENT DES LA VALIDATION.";
    case UPGRADE_ARMOR:return "BLINDAGE : LA REDUCTION S'APPLIQUE AVANT L'ABSORPTION DU BOUCLIER.";
    case UPGRADE_VAMPIRE:return "VAMPIRISME : CHAQUE ELIMINATION TE SOIGNE, JUSQU'A TA SANTE MAXIMALE.";
    case UPGRADE_PRECISION:return p->weapon==WEAPON_ROCKET?"PRECISION : UTILISE UNE ARME A BALLES POUR REALISER DES TETES FATALES.":"PRECISION : LES MUNITIONS REVIENNENT DIRECTEMENT DANS LE CHARGEUR.";
    case UPGRADE_DASH_POWER:return p->upgrades[UPGRADE_MOBILITY]?"SYNERGIE : REFLEXES REDUIT L'ATTENTE ENTRE DEUX BONUS D'ASSAUT.":"ASSAUT : ESQUIVE, PUIS TIRE PENDANT LES 1.25 SECONDES DU BONUS.";
    case UPGRADE_SWITCH_POWER:return "ALTERNANCE : CHANGE D'ARME, PUIS TIRE DANS LES 2.5 SECONDES.";
    case UPGRADE_PIERCE:return p->weapon==WEAPON_ROCKET?"PERFORATION : UTILISE UNE ARME A BALLES POUR TRAVERSER UN ENNEMI.":"PERFORATION : ALIGNE DEUX ENNEMIS ; LES MURS ARRETENT TOUJOURS LE TIR.";
    default:return "";
    }
}
void upgrade_ui(Mesh *m,const Game *g,const App *app) {
    const Player *p=&g->player;char text[96];
    mesh_rect(m,24,36,592,313,(Color){.035f,.055f,.07f});
    mesh_rect(m,24,36,592,3,(Color){.4f,.83f,1});
    snprintf(text,sizeof(text),"VAGUE %02d TERMINEE / +250 POINTS",g->completed_waves);
    centered(m,49,1,(Color){1,.78f,.3f},text);
    centered(m,67,3,(Color){.92f,.96f,1},"CHOISIS TON MODULE");
    snprintf(text,sizeof(text),"NIVEAU %d -> %d / UNE AMELIORATION POUR CETTE PARTIE",player_level(p),player_level(p)+1);
    centered(m,100,1,(Color){.6f,.72f,.79f},text);
    if(g->challenge_state==1) {
        snprintf(text,sizeof(text),"DEFI REUSSI / +%d POINTS / BONUS CONSERVES POUR LE COMBAT",400+25*g->wave);
        centered(m,112,1,(Color){.45f,1,.65f},text);
    }
    for(int i=0;i<UPGRADE_CHOICES;i++) {
        int id=p->offers[i];Color c=upgrade_color(id);float x=42+(float)i*188;
        bool selected=app->selected==i;
        float y=125;
        mesh_rect(m,x,y,180,121,selected?(Color){.13f,.20f,.25f}:(Color){.075f,.11f,.14f});
        mesh_rect(m,x,y,180,3,c);
        if(selected){mesh_rect(m,x,y,2,121,c);mesh_rect(m,x+178,y,2,121,c);}
        snprintf(text,sizeof(text),"[%d]",i+1);mesh_text(m,x+10,y+12,1,c,text);
        /* Emblème géométrique animé au-dessus du nom du module. */
        float glow=app->motion?.75f+.25f*sinf(app->ui_time*5+(float)i):1;
        Color bright={c.r*glow,c.g*glow,c.b*glow};
        mesh_triangle(m,(Vec3){x+90,y+10,0},(Vec3){x+104,y+24,0},(Vec3){x+90,y+38,0},bright,(Vec3){0,0,1});
        mesh_triangle(m,(Vec3){x+90,y+10,0},(Vec3){x+90,y+38,0},(Vec3){x+76,y+24,0},bright,(Vec3){0,0,1});
        mesh_text(m,x+(180-(float)strlen(upgrade_name(id))*12)/2,y+44,2,c,upgrade_name(id));
        snprintf(text,sizeof(text),"RANG %d -> %d / %d",p->upgrades[id],p->upgrades[id]+1,UPGRADE_MAX_LEVEL);
        mesh_text(m,x+(180-(float)strlen(text)*6)/2,y+64,1,(Color){.9f,.93f,.86f},text);
        for(int rank=0;rank<UPGRADE_MAX_LEVEL;rank++) {
            float left=x+55+(float)rank*15;
            mesh_rect(m,left,y+77,11,3,rank<p->upgrades[id]?c:(Color){.19f,.25f,.29f});
            if(rank==p->upgrades[id])mesh_rect(m,left,y+77,11,3,bright);
        }
        char first[64],second[64];upgrade_preview(p,id,first,sizeof(first),second,sizeof(second));
        mesh_text(m,x+(180-(float)strlen(first)*6)/2,y+88,1,(Color){.88f,.94f,1},first);
        mesh_text(m,x+(180-(float)strlen(second)*6)/2,y+104,1,(Color){.69f,.77f,.8f},second);
    }
    if(!SDL_TICKS_PASSED(SDL_GetTicks(),app->upgrade_ready_at)||g->player.upgrade_lock>0) {
        Uint32 now=SDL_GetTicks();
        float remaining=SDL_TICKS_PASSED(now,app->upgrade_ready_at)?0:(float)(app->upgrade_ready_at-now)/1000;
        remaining=fmaxf(remaining,g->player.upgrade_lock);
        mesh_rect(m,42,249,556,2,(Color){.16f,.21f,.24f});
        mesh_rect(m,42,249,556*clampf(1-remaining/UPGRADE_LOCK_TIME,0,1),2,(Color){1,.78f,.3f});
        centered(m,264,1,(Color){1,.78f,.3f},"UN INSTANT... RELACHE LE TIR POUR CHOISIR");
    }
    else if(app->upgrade_choice)centered(m,264,1,(Color){.55f,.9f,1},"CHOIX ENVOYE...");
    else if(app->upgrade_confirm) {
        mesh_rect(m,220,257,200,20,(Color){.15f,.30f,.37f});
        snprintf(text,sizeof(text),"VALIDER : %s",upgrade_name(p->offers[app->selected]));
        centered(m,264,1,(Color){.8f,.96f,1},text);
        snprintf(text,sizeof(text),"ENTREE OU BOUTON POUR VALIDER / AUTO %.0f S",ceilf(fmaxf(0,g->wave_timer)));
        centered(m,280,1,(Color){.68f,.77f,.79f},text);
    }
    else {
        snprintf(text,sizeof(text),"CLIC / 1-3 : SELECTION / ENTREE : VALIDER / AUTO %.0f S",ceilf(fmaxf(0,g->wave_timer)));
        centered(m,264,1,(Color){.68f,.77f,.79f},text);
    }
    if(app->selected>=0&&app->selected<UPGRADE_CHOICES)
        centered(m,299,1,(Color){.68f,.83f,.9f},upgrade_advice(p,p->offers[app->selected]));
    int next_weapon=game_unlocked_weapons(g);
    if(next_weapon<STANDARD_WEAPONS) {
        int unlock=WEAPONS[next_weapon].unlock_wave;
        snprintf(text,sizeof(text),"PROCHAINE ARME : %s / VAGUE %d / ENCORE %d VAGUE%s",
            WEAPONS[next_weapon].name,unlock,unlock-g->wave,unlock-g->wave>1?"S":"");
        centered(m,322,1,(Color){.9f,.82f,.52f},text);
        int previous=WEAPONS[next_weapon-1].unlock_wave;
        float progress=clampf((float)(g->completed_waves-previous+1)/(float)(unlock-previous),0,1);
        mesh_rect(m,115,337,410,3,(Color){.17f,.23f,.26f});
        mesh_rect(m,115,337,410*progress,3,WEAPONS[next_weapon].color);
    }else centered(m,322,1,(Color){.65f,.9f,.75f},"ARSENAL PRINCIPAL COMPLET / PERFECTIONNE TES MODULES");

}
