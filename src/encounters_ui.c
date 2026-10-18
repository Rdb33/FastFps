#include "fastfps.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static void center(Mesh *m,float y,Color c,const char *text) {
    mesh_text(m,(640-(float)strlen(text)*6)/2,y,1,c,text);
}
void encounters_draw(Mesh *m,const Game *g) {
    float time=g->time,charge=(float)g->reactor_charge/REACTOR_CHARGE;
    Color reactor=g->reactor_cooldown>30?(Color){.45f,1,.65f}:
        (g->reactor_charge==REACTOR_CHARGE?(Color){1,.8f,.3f}:(Color){.2f,.55f,.85f});
    m->glow=1;
    for(int i=0;i<REACTOR_CHARGE;i++) {
        float angle=(float)i*2*PI_F/REACTOR_CHARGE;
        Color c=i<g->reactor_charge?reactor:(Color){.09f,.17f,.22f};
        mesh_box(m,(Vec3){sinf(angle)*1.7f,1.86f,cosf(angle)*1.7f},(Vec3){.15f,.06f,.15f},c);
    }
    if(charge>=1||g->reactor_cooldown>30) {
        mesh_ring(m,(Vec3){0,1.87f,0},2.25f+.12f*sinf(time*3),.055f,reactor,20);
        mesh_diamond(m,(Vec3){0,5.1f+.1f*sinf(time*3),0},.14f,reactor);
    }
    for(int i=0;i<MAX_DECKS;i++) {
        const Supply *s=&g->supplies[i];Color c=supply_color(s->kind);
        if(s->cooldown>0) {
            mesh_ring(m,vadd(s->position,(Vec3){0,.04f,0}),.4f,.035f,(Color){c.r*.3f,c.g*.3f,c.b*.3f},12);
            continue;
        }
        Vec3 p=vadd(s->position,(Vec3){0,.65f+.12f*sinf(time*2+(float)i),0});
        mesh_ring(m,vadd(s->position,(Vec3){0,.045f,0}),.7f,.055f,c,16);
        int first=m->count;mesh_diamond(m,p,.28f,c);
        mesh_rotate(m,first,p,0,time*.8f,0);
        if(s->kind==BONUS_SHIELD)mesh_ring(m,p,.4f,.055f,c,12);
        else if(s->kind==BONUS_POWER)mesh_diamond(m,vadd(p,(Vec3){0,.45f,0}),.10f,c);
        else for(int j=0;j<2;j++)mesh_diamond(m,vadd(p,(Vec3){0,-.36f-(float)j*.18f,0}),.08f,c);
    }
    for(int id=0;id<MAX_PLAYERS;id++)if(id!=g->local_actor&&g->actors[id].active&&!g->actors[id].dead) {
        const Player *p=&g->actors[id].player;
        if(p->shield>0)mesh_ring(m,vadd(p->body.position,(Vec3){0,.9f,0}),.65f,.04f,supply_color(BONUS_SHIELD),12);
        if(p->reactor_time>0)mesh_ring(m,vadd(p->body.position,(Vec3){0,.04f,0}),.6f,.035f,(Color){.45f,1,.65f},12);
    }
    m->glow=0;
}

static void directional_damage(Mesh *m,const Game *g) {
    if(g->dead)return;
    const Actor *a=&g->actors[g->local_actor];
    for(int i=0;i<DAMAGE_DIRECTIONS;i++)if(a->damage_timers[i]>0) {
        float angle=a->damage_angles[i]-g->player.yaw;
        float fade=clampf(a->damage_timers[i]/.4f,0,1);
        Color c={fade,.18f*fade,.075f*fade};
        /* Arc lisible autour du viseur, indépendant du flash des dégâts. */
        for(int j=0;j<5;j++) {
            float b=angle-.28f+(float)j*.112f,e=b+.1f;
            Vec3 v0={320+sinf(b)*66,180-cosf(b)*66,0},v1={320+sinf(e)*66,180-cosf(e)*66,0};
            Vec3 v2={320+sinf(e)*71,180-cosf(e)*71,0},v3={320+sinf(b)*71,180-cosf(b)*71,0};
            mesh_quad(m,v0,v1,v2,v3,c,(Vec3){0,0,1});
        }
        Vec3 tip={320+sinf(angle)*83,180-cosf(angle)*83,0};
        Vec3 b={320+sinf(angle)*73-cosf(angle)*5,180-cosf(angle)*73-sinf(angle)*5,0};
        Vec3 c2={320+sinf(angle)*73+cosf(angle)*5,180-cosf(angle)*73+sinf(angle)*5,0};
        mesh_triangle(m,tip,b,c2,c,(Vec3){0,0,1});
    }
}
void encounters_ui(Mesh *m,const Game *g,const App *app) {
    directional_damage(m,g);
    if(app->show_build||g->dead)return;
    char text[100];Color blue={.4f,.8f,1},gold={1,.8f,.3f};
    if(g->wave>0) {
        if(g->reactor_cooldown>0)snprintf(text,sizeof(text),"REACTEUR : RECHARGE %.0f S",ceilf(g->reactor_cooldown));
        else snprintf(text,sizeof(text),"REACTEUR %d/%d%s",g->reactor_charge,REACTOR_CHARGE,g->reactor_charge==REACTOR_CHARGE?" / PRET":"");
        mesh_text(m,12,91,1,g->reactor_charge==REACTOR_CHARGE?gold:blue,text);
        if(g->reinforcements>0) {
            snprintf(text,sizeof(text),"RENFORTS EN RESERVE : %d",g->reinforcements);mesh_text(m,12,104,1,blue,text);
        }
    }
    if(g->challenge) {
        Color c=g->challenge_state==1?(Color){.45f,1,.65f}:(g->challenge_state==2?(Color){.5f,.5f,.5f}:gold);
        if(g->challenge_state==0) {
            if(g->challenge==CHALLENGE_CHAMPION)snprintf(text,sizeof(text),"DEFI : %s / %.0f S",challenge_name(g->challenge),ceilf(g->challenge_deadline));
            else if(g->challenge==CHALLENGE_INTERRUPT)snprintf(text,sizeof(text),"DEFI : %s / %d/3",challenge_name(g->challenge),g->challenge_progress);
            else snprintf(text,sizeof(text),"DEFI : %s",challenge_name(g->challenge));
            center(m,76,c,text);
        }else if(g->challenge_notice>0) {
            if(g->challenge_state==1)snprintf(text,sizeof(text),"DEFI REUSSI : +%d POINTS / BOUCLIER ET VITESSE",400+25*g->wave);
            else snprintf(text,sizeof(text),"DEFI MANQUE / LA PARTIE CONTINUE");
            center(m,76,c,text);
        }
    }
    if(g->reactor_charge==REACTOR_CHARGE&&g->reactor_cooldown==0&&g->wave_timer<0
        &&reactor_in_range(&g->player)&&game_revive_target(g,g->local_actor)<0) {
        center(m,253,gold,"E : ACTIVER LE REACTEUR POUR TOUTE L'EQUIPE");
        center(m,265,blue,"+15 PV / VITESSE +15% / RECHARGE +25% / 15 S");
    }
    float y=119;
    const Player *p=&g->player;
    if(p->shield>0){snprintf(text,sizeof(text),"BOUCLIER %.0f / %.0f S",ceilf(p->shield),ceilf(p->shield_time));mesh_text(m,12,y,1,blue,text);y+=12;}
    if(p->speed_time>0){snprintf(text,sizeof(text),"CELERITE %.0f S",ceilf(p->speed_time));mesh_text(m,12,y,1,supply_color(BONUS_SPEED),text);y+=12;}
    if(p->power_time>0){snprintf(text,sizeof(text),"SURTENSION : PROCHAIN TIR / %.0f S",ceilf(p->power_time));mesh_text(m,12,y,1,supply_color(BONUS_POWER),text);y+=12;}
    if(p->reactor_time>0){snprintf(text,sizeof(text),"BONUS REACTEUR %.0f S",ceilf(p->reactor_time));mesh_text(m,12,y,1,(Color){.45f,1,.65f},text);}
    if(g->wave_timer<0&&g->reinforcements==0&&g->enemy_count<=2) {
        int row=0;
        for(int i=0;i<g->enemy_count;i++)if(g->enemies[i].health>0) {
            Vec3 d=vsub(g->enemies[i].body.position,p->body.position);
            float angle=atan2f(sinf(atan2f(d.x,-d.z)-p->yaw),cosf(atan2f(d.x,-d.z)-p->yaw));
            const char *direction=fabsf(angle)<PI_F/4?"DEVANT":(fabsf(angle)>3*PI_F/4?"DERRIERE":(angle>0?"A DROITE":"A GAUCHE"));
            snprintf(text,sizeof(text),"DERNIER %s / %.0f M%s",direction,hypotf(d.x,d.z),d.y>2?" / EN HAUT":(d.y<-2?" / EN BAS":""));
            mesh_text(m,12,239+(float)row++*12,1,gold,text);
        }
    }
}
