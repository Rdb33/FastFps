#include "fastfps.h"
#include "network.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static const Color ink={.92f,.9f,.79f},muted={.51f,.55f,.45f},orange={1,.48f,.18f},green={.58f,.82f,.40f};
static void centered(Mesh *m,float y,int scale,Color color,const char *text) {
    mesh_text(m,(640-(float)strlen(text)*6*(float)scale)/2,y,scale,color,text);
}
static const Enemy *aim_target(const Game *g) {
    Vec3 origin=player_eye(&g->player,g->motion),dir=look_direction(g->player.yaw,g->player.pitch);
    float distance=world_ray(g->world,origin,dir,SHOT_RANGE);
    const Enemy *target=NULL;
    for(int i=0;i<g->enemy_count;i++) {
        const Enemy *e=&g->enemies[i];Vec3 p=e->body.position;
        float scale=enemy_size(e),r=.48f*scale;
        float hit=ray_box(origin,dir,(Box){{p.x-r,p.y,p.z-r},{p.x+r,p.y+ENEMY_HEIGHT*scale,p.z+r}},distance);
        if(hit<distance){distance=hit;target=e;}
    }
    return target;
}
void ui_build(Mesh *m,const Game *g,const App *app) {
    m->count=0;char text[160];
    if(app->state==STATE_NETWORK||app->state==STATE_LOBBY){lobby_build(m,g,app);return;}
    if(app->state==STATE_PLAYING||app->state==STATE_UPGRADE) {
        mesh_rect(m,0,0,640,30,(Color){.05f,.055f,.045f});
        snprintf(text,sizeof(text),"FASTFPS / %s",world_map_name(g->map_id));mesh_text(m,12,11,1,orange,text);
        snprintf(text,sizeof(text),"SECTEUR %02d / VAGUE %02d / %d ENNEMIS",(g->wave?g->wave-1:0)/5+1,g->wave,g->enemy_count);centered(m,11,1,ink,text);
        mesh_rect(m,0,30,640,2,(Color){.12f,.16f,.19f});
        mesh_rect(m,0,30,640*(float)(g->completed_waves%5)/5,2,(Color){.35f,.72f,.86f});
        snprintf(text,sizeof(text),"SCORE %05d",g->score);mesh_text(m,640-(float)strlen(text)*6-12,11,1,ink,text);
        mesh_rect(m,0,307,640,53,(Color){.065f,.07f,.055f});
        mesh_rect(m,0,307,640,1,(Color){.25f,.27f,.19f});
        mesh_text(m,14,315,1,muted,"SANTE");
        snprintf(text,sizeof(text),"%03d",(int)ceilf(g->player.health));
        mesh_text(m,14,329,3,g->player.health<30?orange:ink,text);
        mesh_rect(m,79,334,70,5,(Color){.22f,.23f,.16f});
        mesh_rect(m,79,334,70*g->player.health/player_max_health(&g->player),5,g->player.health<30?orange:green);
        snprintf(text,sizeof(text),"/%d",(int)player_max_health(&g->player));mesh_text(m,80,345,1,muted,text);
        snprintf(text,sizeof(text),"[%d] %s",g->player.weapon+1,WEAPONS[g->player.weapon].name);centered(m,319,1,muted,text);
        if(g->player.reload_timer>0) {
            snprintf(text,sizeof(text),"RECHARGE %.1f S",g->player.reload_timer);
            centered(m,352,1,orange,text);
            mesh_rect(m,260,345,120,3,(Color){.17f,.22f,.24f});
            mesh_rect(m,260,345,120*(1-g->player.reload_timer/g->player.reload_duration),3,(Color){.35f,.8f,1});
        }else {
            snprintf(text,sizeof(text),"%d / %d MUNITIONS / R : RECHARGER",g->player.ammo[g->player.weapon],WEAPONS[g->player.weapon].magazine);
            centered(m,352,1,g->player.ammo[g->player.weapon]<3?orange:muted,text);
        }
        if(!g->cooperative){snprintf(text,sizeof(text),"%d VIES",g->lives);mesh_text(m,160,318,1,green,text);}
        snprintf(text,sizeof(text),"NIVEAU %02d / %d MODULES",player_level(&g->player),player_level(&g->player)-1);centered(m,336,1,ink,text);
        mesh_text(m,498,315,1,muted,"DROIT : ESQUIVE");
        mesh_rect(m,498,332,123,4,(Color){.22f,.23f,.16f});
        mesh_rect(m,498,332,123*clampf(1-g->player.dash_cooldown/player_dash_cooldown(&g->player),0,1),4,green);
        snprintf(text,sizeof(text),"%02d M/S",(int)hypotf(g->player.body.velocity.x,g->player.body.velocity.z));
        mesh_text(m,498,344,1,ink,text);
        mesh_text(m,559,344,1,muted,"F : FRAPPE");
        if(app->fps>=0)snprintf(text,sizeof(text),"%d FPS / %s / TAB : MODULES",app->fps,PRESETS[app->quality].name);
        else snprintf(text,sizeof(text),"%s / TAB : MODULES",PRESETS[app->quality].name);
        mesh_text(m,12,295,1,muted,text);
        if(g->cooperative) {
            int row=0;
            for(int i=0;i<MAX_PLAYERS;i++)if(g->actors[i].active&&i!=g->local_actor) {
                if(g->actors[i].dead)snprintf(text,sizeof(text),"%s : A TERRE / REANIMER",g->actors[i].name);
                else snprintf(text,sizeof(text),"%s : %.0f PV",g->actors[i].name,g->actors[i].player.health);
                mesh_text(m,12,45+(float)row*13,1,(Color){.35f,.85f,1},text);row++;
            }
            const Network *n=app->network;
            if(n->mode==NET_CLIENT&&n->latency_ms>=0) {
                snprintf(text,sizeof(text),"%.0f MS",n->latency_ms);
                mesh_text(m,640-(float)strlen(text)*6-12,45,1,n->latency_ms>120?orange:muted,text);
            }
            if(g->dead) {
                centered(m,145,2,orange,"TU ES A TERRE");
                centered(m,166,1,ink,"UN ALLIE DOIT MAINTENIR E PRES DE TOI");
                mesh_rect(m,230,180,180,5,(Color){.13f,.20f,.23f});
                mesh_rect(m,230,180,180*g->actors[g->local_actor].revive_progress/REVIVE_TIME,5,(Color){.4f,1,.7f});
            }
        }
        int revive=game_revive_target(g,g->local_actor);
        if(revive>=0&&!g->dead) {
            snprintf(text,sizeof(text),"MAINTIENS E : REANIMER %s",g->actors[revive].name);centered(m,253,1,(Color){.4f,1,.7f},text);
            mesh_rect(m,240,268,160,4,(Color){.13f,.20f,.23f});
            mesh_rect(m,240,268,160*g->actors[revive].revive_progress/REVIVE_TIME,4,(Color){.4f,1,.7f});
        }
        if(g->dead&&!g->cooperative&&!g->round_over) {
            snprintf(text,sizeof(text),"%d VIES / RETOUR DANS %.0f S",g->lives,ceilf(g->actors[0].respawn_timer));
            centered(m,145,2,orange,text);centered(m,168,1,ink,"TES MODULES ET TA PROGRESSION SONT CONSERVES");
        }
        if(g->message_time>0)centered(m,64,1,orange,g->message);
        if(g->reward_time>0) {
            snprintf(text,sizeof(text),"+%d",g->reward_points);
            centered(m,227-(.85f-g->reward_time)*14,2,(Color){1,.82f,.38f},text);
        }
        for(int i=0;i<UPGRADE_COUNT;i++)if(g->player.upgrades[i]) {
            float x=234+(float)i*29;Color color=upgrade_color(i);
            mesh_rect(m,x,285,24,9,(Color){color.r*.2f,color.g*.2f,color.b*.2f});
            snprintf(text,sizeof(text),"%d",g->player.upgrades[i]);mesh_text(m,x+9,286,1,color,text);
        }
        if(g->player.upgrade_flash>0&&!g->player.upgrade_pending) {
            snprintf(text,sizeof(text),"MODULE INSTALLE : %s / RANG %d",upgrade_name(g->player.last_upgrade),g->player.upgrades[g->player.last_upgrade]);
            centered(m,249,1,(Color){.4f,.9f,1},text);
        }
        if(g->wave_timer>0&&g->enemy_count==0) {
            snprintf(text,sizeof(text),"PROCHAINE VAGUE DANS %.0f",ceilf(g->wave_timer));
            centered(m,122,1,ink,text);
        }
        if(g->combo>1&&g->combo_timer>0) {
            int multiplier=1+(g->combo-1)/3;if(multiplier>4)multiplier=4;
            snprintf(text,sizeof(text),"SERIE %02d / X%d",g->combo,multiplier);
            centered(m,44,1,orange,text);
        }
        const Enemy *target=aim_target(g);
        if(target) {
            Color color=target->type==ENEMY_CASTER?(Color){.67f,.43f,1}:
                (target->type==ENEMY_BRUTE?(Color){.78f,.83f,.32f}:orange);
            if(target->elite)color=(Color){1,.76f,.25f};
            centered(m,86,1,color,target->elite?"CHAMPION":(target->type==ENEMY_CASTER?"INVOCATEUR":(target->type==ENEMY_BRUTE?"BRUTE":"TRAQUEUR")));
            mesh_rect(m,270,98,100,3,(Color){.16f,.15f,.12f});
            mesh_rect(m,270,98,100*clampf(target->health/fmaxf(1,target->max_health),0,1),3,color);
        }
        float gap=5+g->recoil*(g->player.weapon?6:3);
        mesh_rect(m,319,180-gap-7,2,6,ink);mesh_rect(m,319,180+gap+1,2,6,ink);
        mesh_rect(m,320-gap-7,179,6,2,ink);mesh_rect(m,320+gap+1,179,6,2,ink);
        mesh_rect(m,319,179,2,2,ink);
        if(g->hit_time>0||g->kill_time>0) {
            Color color=g->kill_time>0?(Color){1,.85f,.35f}:(g->headshot_time>0?(Color){.35f,.9f,1}:orange);
            mesh_rect(m,309,171,3,3,color);mesh_rect(m,328,171,3,3,color);
            mesh_rect(m,309,186,3,3,color);mesh_rect(m,328,186,3,3,color);
            if(g->headshot_time>0)centered(m,207,1,color,"TETE / X2");
            else if(g->kill_time>0)centered(m,207,1,color,"ELIMINATION");
        }
        if(g->damage_time>0) {
            Color red={.65f*g->damage_time/.32f,.07f,.02f};
            mesh_rect(m,0,30,4,277,red);mesh_rect(m,636,30,4,277,red);
            mesh_rect(m,0,30,640,4,red);mesh_rect(m,0,303,640,4,red);
            float angle=g->damage_yaw-g->player.yaw;
            float x=320+sinf(angle)*42,y=180-cosf(angle)*42;
            Vec3 tip={x+sinf(angle)*7,y-cosf(angle)*7,0};
            Vec3 a={x-cosf(angle)*5,y-sinf(angle)*5,0},b={x+cosf(angle)*5,y+sinf(angle)*5,0};
            mesh_triangle(m,tip,a,b,(Color){1,.28f,.12f},(Vec3){0,0,1});
        }
        if(g->player.health<30&&!g->dead) {
            float pulse=.6f+.4f*sinf(g->time*6);
            centered(m,276,1,(Color){pulse,.18f,.08f},"SANTE BASSE - CROIX VERTES / F : FRAPPE FATALE");
        }
        if(app->show_build&&app->state==STATE_PLAYING) {
            mesh_rect(m,10,108,206,129,(Color){.04f,.065f,.08f});
            mesh_text(m,20,119,1,ink,"MODULES DE CETTE PARTIE");
            for(int i=0;i<UPGRADE_COUNT;i++) {
                snprintf(text,sizeof(text),"%-11s %d/%d",upgrade_name(i),g->player.upgrades[i],UPGRADE_MAX_LEVEL);
                mesh_text(m,20,138+(float)i*14,1,g->player.upgrades[i]?upgrade_color(i):muted,text);
            }
        }
        if(app->show_build&&app->state==STATE_PLAYING) {
            mesh_rect(m,424,108,206,133,(Color){.04f,.065f,.08f});
            mesh_text(m,434,119,1,ink,"ARSENAL DE CETTE PARTIE");
            for(int i=0;i<WEAPON_COUNT;i++) {
                bool available=game_weapon_available(g,i);
                snprintf(text,sizeof(text),"%d %-14s %s",i+1,WEAPONS[i].name,available?"PRET":"");
                mesh_text(m,434,138+(float)i*19,1,available?WEAPONS[i].color:muted,text);
                if(!available){snprintf(text,sizeof(text),i==WEAPON_ROCKET?"EVENT":"R%d",WEAPONS[i].unlock_wave);mesh_text(m,581,138+(float)i*19,1,muted,text);}
            }
        }
        if(g->rocket_event)mesh_text(m,486,82,1,orange,"EVENEMENT ROQUETTES");
        if(g->unlock_time>0&&g->unlock_weapon>=1&&g->unlock_weapon<STANDARD_WEAPONS) {
            Color c=WEAPONS[g->unlock_weapon].color;
            mesh_rect(m,398,106,232,73,(Color){.045f,.085f,.12f});mesh_rect(m,398,106,232,3,c);
            mesh_text(m,410,119,1,c,"NOUVELLE ARME DEBLOQUEE");
            mesh_text(m,410,136,2,ink,WEAPONS[g->unlock_weapon].name);
            snprintf(text,sizeof(text),"TOUCHE %d OU MOLETTE POUR L'EQUIPER",g->unlock_weapon+1);mesh_text(m,410,161,1,c,text);
        }
        if(g->event_notice_time>0) {
            mesh_rect(m,398,188,232,80,(Color){.12f,.065f,.04f});mesh_rect(m,398,188,232,3,orange);
            mesh_text(m,410,201,1,orange,"EVENEMENT RARE / TOUTE L'EQUIPE");
            mesh_text(m,410,218,2,ink,"ROQUETTES !");
            mesh_text(m,410,241,1,orange,"LANCEUR PRETE POUR CE ROUND");
            mesh_text(m,410,255,1,muted,"TOUCHE 5 / DEGATS DE ZONE");
        }
        if(app->state==STATE_UPGRADE)upgrade_ui(m,g,app);
        return;
    }
    menu_build(m,g,app);
}
