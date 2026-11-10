#include "fastfps.h"
#include "network.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static const Color ink={.92f,.9f,.79f},muted={.51f,.55f,.45f},orange={1,.48f,.18f},green={.58f,.82f,.40f};
static void centered(Mesh *m,float y,int scale,Color color,const char *text) {
    mesh_text(m,(640-(float)strlen(text)*6*(float)scale)/2,y,scale,color,text);
}
static float notice_offset(const App *app,float remaining,float duration) {
    if(!app->motion)return 0;
    float t=clampf(fminf((duration-remaining)/.32f,remaining/.32f),0,1);
    return 244*(1-t*t*(3-2*t));
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
    if(app->state==STATE_PLAYING&&g->intro_time>0) {
        static const char *titles[MAP_COUNT]={"LE GARDIEN DU COEUR","LE MAITRE DES FOURNAISES","LE VEILLEUR DES RUINES"};
        Color gold={1,.74f,.3f};
        mesh_rect(m,0,0,640,43,(Color){.008f,.012f,.018f});mesh_rect(m,0,285,640,75,(Color){.008f,.012f,.018f});
        centered(m,17,1,gold,"UNE PRESENCE HOSTILE APPROCHE");
        centered(m,301,2,ink,titles[g->map_id]);
        snprintf(text,sizeof(text),"CHAMPION / %s / VAGUE %d",world_map_name(g->map_id),g->wave);centered(m,325,1,gold,text);
        centered(m,344,1,muted,g->cooperative?"ESPACE : PASSER POUR TOUTE L'EQUIPE":"ESPACE : PASSER");return;
    }
    if(app->state==STATE_NETWORK||app->state==STATE_LOBBY){lobby_build(m,g,app);return;}
    if(app->state==STATE_PLAYING||app->state==STATE_UPGRADE) {
        mesh_rect(m,0,0,640,30,(Color){.05f,.055f,.045f});
        snprintf(text,sizeof(text),"FASTFPS / %s",world_map_name(g->map_id));mesh_text(m,12,11,1,orange,text);
        snprintf(text,sizeof(text),"VAGUE %02d / %d ENNEMIS",g->wave,g->enemy_count+g->reinforcements);centered(m,11,1,ink,text);
        mesh_rect(m,0,30,640,2,(Color){.12f,.16f,.19f});
        mesh_rect(m,0,30,640*(float)(g->completed_waves%5)/5,2,(Color){.35f,.72f,.86f});
        snprintf(text,sizeof(text),"SCORE %05d",g->score);mesh_text(m,640-(float)strlen(text)*6-12,11,1,ink,text);
        mesh_rect(m,0,307,640,53,(Color){.065f,.07f,.055f});
        mesh_rect(m,0,307,640,1,(Color){.25f,.27f,.19f});
        mesh_text(m,14,315,1,muted,"SANTE");
        snprintf(text,sizeof(text),"%03d",(int)ceilf(g->player.health));
        int health_first=m->count;
        mesh_text(m,14,329,3,g->player.health<30?orange:ink,text);
        if(app->motion&&g->damage_time>0) {
            float t=clampf(g->damage_time/.32f,0,1),scale=1+.07f*t*t*app->impact_intensity;
            for(int i=health_first;i<m->count;i++) {
                m->data[i].position.x=41+(m->data[i].position.x-41)*scale;
                m->data[i].position.y=339+(m->data[i].position.y-339)*scale;
            }
        }
        mesh_rect(m,79,334,70,5,(Color){.22f,.23f,.16f});
        mesh_rect(m,79,334,70*g->player.health/player_max_health(&g->player),5,g->player.health<30?orange:green);
        snprintf(text,sizeof(text),"/%d",(int)player_max_health(&g->player));mesh_text(m,80,345,1,muted,text);
        snprintf(text,sizeof(text),"[%d] %s",g->player.weapon+1,WEAPONS[g->player.weapon].name);centered(m,319,1,muted,text);
        if(g->player.reload_timer>0) {
            snprintf(text,sizeof(text),"RECHARGE %.1f S",g->player.reload_timer);
            centered(m,352,1,orange,text);
            mesh_rect(m,260,345,120,3,(Color){.17f,.22f,.24f});
            float progress=clampf(1-g->player.reload_timer/fmaxf(.001f,g->player.reload_duration),0,1);
            mesh_rect(m,260,345,120*progress,3,(Color){.35f,.8f,1});
            mesh_rect(m,259+120*progress,344,2,5,(Color){.75f,.94f,1});
            const float steps[]={.24f,.66f,.86f};
            for(int step=0;step<3;step++)mesh_rect(m,260+120*steps[step],349,1,1,progress>=steps[step]?ink:muted);
        }else {
            snprintf(text,sizeof(text),"%d / %d MUNITIONS / R : RECHARGER",g->player.ammo[g->player.weapon],WEAPONS[g->player.weapon].magazine);
            float tap=app->motion?2*expf(-clampf(g->shot_age,0,2)*26)*app->impact_intensity:0;
            centered(m,352-tap,1,g->player.ammo[g->player.weapon]<=WEAPONS[g->player.weapon].magazine/4?orange:muted,text);
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
                if(g->actors[i].dead)snprintf(text,sizeof(text),"%s : A TERRE / E",g->actors[i].name);
                else snprintf(text,sizeof(text),"%s : %.0f PV",g->actors[i].name,g->actors[i].player.health);
                mesh_text(m,12,45+(float)row*13,1,g->actors[i].dead?orange:teammate_color(i),text);row++;
            }
            const Network *n=app->network;
            if(n->mode==NET_CLIENT&&n->latency_ms>=0) {
                snprintf(text,sizeof(text),"%.0f MS",n->latency_ms);
                mesh_text(m,640-(float)strlen(text)*6-12,45,1,n->latency_ms>120?orange:muted,text);
            }
            if(g->dead) {
                centered(m,145,2,orange,"TU ES A TERRE");
                const Actor *down=&g->actors[g->local_actor];int helper=-1;
                for(int id=0;id<MAX_PLAYERS;id++)if(g->actors[id].active&&!g->actors[id].dead
                    &&g->actors[id].revive_target==g->local_actor)helper=id;
                if(helper>=0&&down->revive_progress>0)
                    snprintf(text,sizeof(text),"%s TE REANIME / %.1f S",g->actors[helper].name,
                        fmaxf(0,REVIVE_TIME-down->revive_progress));
                else snprintf(text,sizeof(text),"UN ALLIE DOIT MAINTENIR E PRES DE TOI");
                centered(m,166,1,ink,text);
                mesh_rect(m,230,180,180,5,(Color){.13f,.20f,.23f});
                mesh_rect(m,230,180,180*clampf(down->revive_progress/REVIVE_TIME,0,1),5,(Color){.4f,1,.7f});
            }
        }
        int revive=game_revive_target(g,g->local_actor);
        if(revive>=0&&!g->dead) {
            snprintf(text,sizeof(text),"MAINTIENS E : REANIMER %s",g->actors[revive].name);centered(m,253,1,(Color){.4f,1,.7f},text);
            mesh_rect(m,240,268,160,4,(Color){.13f,.20f,.23f});
            float progress=clampf(g->actors[revive].revive_progress/REVIVE_TIME,0,1);
            mesh_rect(m,240,268,160*progress,4,(Color){.4f,1,.7f});
            if(progress>0) {
                snprintf(text,sizeof(text),"%.1f S",fmaxf(0,REVIVE_TIME-g->actors[revive].revive_progress));
                mesh_text(m,408,265,1,(Color){.4f,1,.7f},text);
            }
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
            mesh_rect(m,280,54,80,2,(Color){.18f,.12f,.07f});
            mesh_rect(m,280,54,80*clampf(g->combo_timer/3.5f,0,1),2,orange);
        }
        float bomb_time=BOMB_FALL_TIME+1;
        for(int i=0;i<g->bomb_count;i++) {
            const Bomb *b=&g->bombs[i];Vec3 delta=vsub(g->player.body.position,b->target);
            Vec3 source=vadd(b->target,(Vec3){0,.25f,0}),center=vadd(g->player.body.position,(Vec3){0,.65f,0});
            Vec3 ray=vsub(center,source);float length=vlength(ray);
            if(hypotf(delta.x,delta.z)<BOMB_RADIUS+1&&fabsf(delta.y)<BOMB_RADIUS
                &&world_ray(g->world,source,vnormalize(ray),length)>=length-.05f)bomb_time=fminf(bomb_time,b->time);
        }
        if(bomb_time<=BOMB_FALL_TIME&&!g->dead) {
            mesh_rect(m,219,213,202,20,(Color){.19f,.055f,.025f});
            snprintf(text,sizeof(text),"BOMBE : %.1f S / ESQUIVE !",bomb_time);
            centered(m,220,1,(Color){1,.72f,.3f},text);
            mesh_rect(m,219,232,202*(1-bomb_time/BOMB_FALL_TIME),2,orange);
        }
        const Enemy *target=aim_target(g);
        if(target) {
            Color color=target->type==ENEMY_CASTER?(Color){.67f,.43f,1}:
                (target->type==ENEMY_BRUTE?(Color){.78f,.83f,.32f}:(target->type==ENEMY_BOMBER?(Color){1,.75f,.24f}:orange));
            if(target->elite)color=(Color){1,.76f,.25f};
            if(target->type==ENEMY_WEAVER)color=(Color){.75f,.95f,1};
            centered(m,86,1,color,target->elite?"CHAMPION":(target->type==ENEMY_WEAVER?"LANCE-TOILE":(target->type==ENEMY_CASTER?"INVOCATEUR":(target->type==ENEMY_BRUTE?"BRUTE":(target->type==ENEMY_BOMBER?"BOMBARDIER":"TRAQUEUR")))));
            mesh_rect(m,270,98,100,3,(Color){.16f,.15f,.12f});
            mesh_rect(m,270,98,100*clampf(target->health/fmaxf(1,target->max_health),0,1),3,color);
        }
        float gap=5+g->recoil*(g->player.weapon?6:3);
        mesh_rect(m,319,180-gap-7,2,6,ink);mesh_rect(m,319,180+gap+1,2,6,ink);
        mesh_rect(m,320-gap-7,179,6,2,ink);mesh_rect(m,320+gap+1,179,6,2,ink);
        mesh_rect(m,319,179,2,2,ink);
        if(g->hit_time>0||g->kill_time>0) {
            Color color=g->kill_time>0?(Color){1,.85f,.35f}:(g->headshot_time>0?(Color){.35f,.9f,1}:orange);
            float snap=app->motion?clampf(g->hit_time/.14f,0,1)*4*app->impact_intensity:0;
            for(int x=-1;x<=1;x+=2)for(int y=-1;y<=1;y+=2) {
                float cx=320+x*(9+snap),cy=180+y*(8+snap);
                mesh_rect(m,cx-1,cy-1,3,3,color);
                if(g->kill_time>0)mesh_rect(m,cx-1+x*3,cy-1+y*3,2,2,color);
            }
            if(g->headshot_time>0)centered(m,207,1,color,"TETE / X2");
            else if(g->kill_time>0)centered(m,207,1,color,"ELIMINATION");
        }
        if(g->damage_time>0) {
            Color red={.65f*g->damage_time/.32f,.07f,.02f};
            mesh_rect(m,0,30,4,277,red);mesh_rect(m,636,30,4,277,red);
            mesh_rect(m,0,30,640,4,red);mesh_rect(m,0,303,640,4,red);
        }
        if(g->player.health<30&&!g->dead) {
            float pulse=.6f+.4f*sinf(g->time*6);
            centered(m,276,1,(Color){pulse,.18f,.08f},"SANTE BASSE - CROIX VERTES / F : FRAPPE FATALE");
        }
        if(app->show_build&&app->state==STATE_PLAYING) {
            mesh_rect(m,10,108,206,173,(Color){.04f,.065f,.08f});
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
            Color c=WEAPONS[g->unlock_weapon].color;float dx=notice_offset(app,g->unlock_time,6);
            mesh_rect(m,398+dx,106,232,73,(Color){.045f,.085f,.12f});mesh_rect(m,398+dx,106,232,3,c);
            mesh_text(m,410+dx,119,1,c,"NOUVELLE ARME DEBLOQUEE");
            mesh_text(m,410+dx,136,2,ink,WEAPONS[g->unlock_weapon].name);
            snprintf(text,sizeof(text),"TOUCHE %d OU MOLETTE POUR L'EQUIPER",g->unlock_weapon+1);mesh_text(m,410+dx,161,1,c,text);
        }
        if(g->event_notice_time>0) {
            float dx=notice_offset(app,g->event_notice_time,6);
            mesh_rect(m,398+dx,188,232,80,(Color){.12f,.065f,.04f});mesh_rect(m,398+dx,188,232,3,orange);
            mesh_text(m,410+dx,201,1,orange,"EVENEMENT RARE / TOUTE L'EQUIPE");
            mesh_text(m,410+dx,218,2,ink,"ROQUETTES !");
            mesh_text(m,410+dx,241,1,orange,"LANCEUR PRETE POUR CE ROUND");
            mesh_text(m,410+dx,255,1,muted,"TOUCHE 5 / DEGATS DE ZONE");
        }
        encounters_ui(m,g,app);
        if(g->player.web_timer>0&&!g->dead) {
            Color web={.75f,.95f,1};mesh_rect(m,12,158,192,36,(Color){.035f,.07f,.10f});
            mesh_rect(m,12,158,3,36,web);
            snprintf(text,sizeof(text),"ENTOILE : %.2f S",g->player.web_timer);mesh_text(m,24,166,1,web,text);
            mesh_text(m,24,178,1,ink,"VISEE ET TIRS DISPONIBLES");
            mesh_rect(m,24,190,168*g->player.web_timer/WEB_ROOT_TIME,2,web);
        }
        if(app->state==STATE_PLAYING&&!app->show_build&&!g->dead)
            ui_music_visualizer(m,app,548,302,78,9);
        if(app->state==STATE_PLAYING&&app->music_notice_time>0&&app->music_volume<=0&&!g->dead&&!app->show_build) {
            float slide=app->motion?clampf(fminf((8-app->music_notice_time)/.25f,app->music_notice_time/.25f),0,1):1;
            slide=slide*slide*(3-2*slide);
            float x=-228+240*slide;Color blue={.35f,.8f,1};
            mesh_rect(m,x,108,228,43,(Color){.04f,.075f,.10f});
            mesh_rect(m,x,108,3,43,blue);
            mesh_text(m,x+12,119,1,blue,"TU JOUES SANS MUSIQUE");
            mesh_text(m,x+12,135,1,ink,"ECHAP > REGLAGES > MUSIQUE");
        }
        if(app->state==STATE_UPGRADE)upgrade_ui(m,g,app);
        return;
    }
    menu_build(m,g,app);
}

/* Rescue information remains available around obstacles and behind the camera.
 * All positions and revive states come from the existing actor snapshots. */
static void rescue_arrow(Mesh *m,float x,float y,float dx,float dy,Color color) {
    float length=hypotf(dx,dy);if(length<.001f){dx=0;dy=1;}else{dx/=length;dy/=length;}
    Vec3 tip={x+dx*5,y+dy*5,0},left={x-dx*3-dy*4,y-dy*3+dx*4,0},right={x-dx*3+dy*4,y-dy*3-dx*4,0};
    mesh_triangle(m,tip,left,right,color,(Vec3){0,0,1});
}
void ui_teammates(Mesh *m,const Game *g,const App *app,Mat4 matrix) {
    if(!g->cooperative||app->state!=STATE_PLAYING||app->show_build||g->intro_time>0||g->round_over)return;
    Vec3 eye=player_eye(&g->player,app->motion);
    float previous_x[MAX_PLAYERS],previous_y[MAX_PLAYERS],previous_w[MAX_PLAYERS],previous_h[MAX_PLAYERS];int used=0;
    /* Place rescue markers first so a living teammate label cannot hide them. */
    for(int pass=0;pass<2;pass++)for(int id=0;id<MAX_PLAYERS;id++)if(id!=g->local_actor&&g->actors[id].active) {
        const Actor *a=&g->actors[id];if(a->dead!=(pass==0))continue;
        Vec3 head=vadd(a->player.body.position,(Vec3){0,a->dead?.38f:1.48f,0});
        Vec3 delta=vsub(head,eye);float distance=vlength(delta);
        bool visible=distance<.01f||world_ray(g->world,eye,vscale(delta,1/distance),distance)>=distance-.05f;
        if(!a->dead&&(distance<.8f||distance>48||!visible))continue;
        Vec3 label=vadd(a->player.body.position,(Vec3){0,a->dead?1.02f:2.12f,0});
        float w=matrix.m[3]*label.x+matrix.m[7]*label.y+matrix.m[11]*label.z+matrix.m[15];
        float clip_x=matrix.m[0]*label.x+matrix.m[4]*label.y+matrix.m[8]*label.z+matrix.m[12];
        float clip_y=matrix.m[1]*label.x+matrix.m[5]*label.y+matrix.m[9]*label.z+matrix.m[13];
        float x=320,y=170;
        if(w>.2f){x=(clip_x/w*.5f+.5f)*640;y=(.5f-clip_y/w*.5f)*360-18;}
        bool edge=w<=.2f||x<16||x>624||y<84||y>242;
        if(!a->dead&&(w<=.2f||fabsf(clip_x/w)>.96f||fabsf(clip_y/w)>.85f))continue;
        char name[PLAYER_NAME_LENGTH+8],status[48],hint[48];hint[0]=0;
        snprintf(name,sizeof(name),"J%d %s",id+1,a->name[0]?a->name:"ALLIE");
        float progress=clampf(a->dead?a->revive_progress/REVIVE_TIME:a->player.health/player_max_health(&a->player),0,1);
        if(a->dead) {
            float meters=vlength(vsub(a->player.body.position,g->player.body.position));
            snprintf(status,sizeof(status),"A TERRE / %.0f M",meters);
            if(progress>0)snprintf(hint,sizeof(hint),"REANIMATION / %.1f S",fmaxf(0,REVIVE_TIME-a->revive_progress));
            else if(w<=.2f)snprintf(hint,sizeof(hint),"DERRIERE TOI");
            else if(!visible)snprintf(hint,sizeof(hint),"OBSTACLE A CONTOURNER");
            else if(!g->dead&&game_revive_target(g,g->local_actor)==id)snprintf(hint,sizeof(hint),"MAINTIENS E");
        }
        float width=(float)strlen(name)*6+12,height=a->dead?(hint[0]?38:27):17;
        if(a->dead)width=fmaxf(width,6*(float)fmaxf((float)strlen(status),(float)strlen(hint))+12);
        float dx=0,dy=1;
        if(a->dead&&edge) {
            dx=clip_x;dy=-clip_y;
            /* A target behind us stays on the side we must turn towards.
               Straight behind uses the lower edge instead of a mirrored label. */
            if(w<=.2f)dy=fmaxf(fabsf(dy),fabsf(w)*.6f);
            if(hypotf(dx,dy)<.01f){dx=0;dy=1;}
            float factor=fminf((304-width/2)/fmaxf(.001f,fabsf(dx)),
                (dy<0?64:75)/fmaxf(.001f,fabsf(dy)));
            x=320+dx*factor;y=170+dy*factor-height/2;
        }
        x=clampf(x,width/2+16,624-width/2);
        /* Keep the reticle and short combat prompts unobstructed. */
        bool central=x-width/2<410&&x+width/2>230;
        if(central&&y<212&&y+height>150)y=148-height;
        float original_y=y;bool placed=false;
        for(int attempt=0;attempt<MAX_PLAYERS*2+49;attempt++) {
            y=attempt<MAX_PLAYERS*2?original_y+(float)((attempt+1)/2)*(height+5)*(attempt%2?-1:1)
                :44+(float)(attempt-MAX_PLAYERS*2)*5;
            if(y<44||y+height>286||(central&&y<212&&y+height>150))continue;
            bool overlap=false;
            for(int k=0;k<used;k++)if(fabsf(x-previous_x[k])<(width+previous_w[k])/2+3
                &&y<previous_y[k]+previous_h[k]+3&&y+height>previous_y[k]-3)overlap=true;
            if(!overlap){placed=true;break;}
        }
        if(!placed)continue;
        Color color=a->dead?orange:teammate_color(id);
        mesh_rect(m,x-width/2-1,y-4,width+2,height+2,(Color){.015f,.022f,.03f});
        mesh_rect(m,x-width/2,y-3,width,height,(Color){.04f,.065f,.09f});
        if(a->dead&&edge) {
            float length=hypotf(dx,dy),reach=fminf((width/2+8)/fmaxf(.001f,fabsf(dx/length)),
                (height/2+8)/fmaxf(.001f,fabsf(dy/length)));
            rescue_arrow(m,x+dx/length*reach,y+height/2+dy/length*reach,dx,dy,color);
        }else {
            mesh_rect(m,x-3,y+height-2,6,2,color);
            mesh_rect(m,x-2,y+height,4,2,color);
            mesh_rect(m,x-1,y+height+2,2,2,color);
        }
        mesh_rect(m,x-width/2,y-3,2,height,color);
        mesh_text(m,x-width/2+6,y,1,color,name);
        float bar_y=y+height-8;
        if(a->dead) {
            mesh_text(m,x-width/2+6,y+10,1,orange,status);
            if(hint[0])mesh_text(m,x-width/2+6,y+20,1,(Color){.72f,.84f,.85f},hint);
        }
        mesh_rect(m,x-width/2+6,bar_y,width-12,2,(Color){.16f,.21f,.24f});
        mesh_rect(m,x-width/2+6,bar_y,(width-12)*progress,2,a->dead?(Color){.4f,1,.7f}:color);
        previous_x[used]=x;previous_y[used]=y-3;previous_w[used]=width;previous_h[used]=height;used++;
    }
}
