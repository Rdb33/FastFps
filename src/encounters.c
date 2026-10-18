#include "fastfps.h"
#include <math.h>
#include <stdio.h>

static Player *player(Game *g,int id) {
    return id==g->current_actor?&g->player:&g->actors[id].player;
}
static bool alive(const Game *g,int id) {
    return g->actors[id].active&&!(id==g->current_actor?g->dead:g->actors[id].dead);
}

/* Pression des vagues indépendante du module choisi : les bonus restent utiles. */
float game_threat(const Game *g) {
    return clampf((float)(g->wave-1)/30,0,1);
}

const char *challenge_name(int id) {
    static const char *names[]={"", "AUCUN JOUEUR A TERRE", "INTERROMPRE 3 ATTAQUES", "CHAMPION EN MOINS DE 25 S"};
    return id>=CHALLENGE_NONE&&id<=CHALLENGE_CHAMPION?names[id]:"";
}
Color supply_color(int kind) {
    return kind==BONUS_SHIELD?(Color){.3f,.75f,1}:
        (kind==BONUS_POWER?(Color){1,.45f,.85f}:(Color){.45f,1,.45f});
}

void encounters_map(Game *g) {
    /* Le bonus occupe un autre point du quai que le poste de soins. */
    for(int i=0;i<MAX_DECKS;i++) {
        Vec3 p=g->world->decks[i].pickup;
        if(g->map_id==1)p.x+=3;
        else p.z-=2;
        g->supplies[i]=(Supply){p,0,(g->wave+i)%3};
    }
}
void encounters_start(Game *g) {
    g->challenge_state=0;g->challenge_progress=0;g->challenge_notice=0;
    g->challenge=g->wave%5==0?CHALLENGE_CHAMPION:
        (g->wave>1?(g->wave%2==0?CHALLENGE_SURVIVE:CHALLENGE_INTERRUPT):CHALLENGE_NONE);
    g->challenge_deadline=g->challenge==CHALLENGE_CHAMPION?25:0;
}
static void challenge_success(Game *g) {
    if(!g->challenge||g->challenge_state||g->prediction)return;
    g->challenge_state=1;g->challenge_notice=5;g->score+=400+25*g->wave;
    for(int id=0;id<MAX_PLAYERS;id++)if(alive(g,id)) {
        Player *p=player(g,id);p->speed_time=fmaxf(p->speed_time,8);
        p->shield=fmaxf(p->shield,15);p->shield_time=fmaxf(p->shield_time,12);
    }
    audio_play(SOUND_UPGRADE);
}
void encounters_finish(Game *g) {
    if(g->challenge==CHALLENGE_SURVIVE&&g->challenge_state==0) {
        g->challenge_progress=1;challenge_success(g);
    }else if(g->challenge&&g->challenge_state==0) {
        g->challenge_state=2;g->challenge_notice=4;
    }
}
void encounters_interrupt(Game *g) {
    if(g->prediction||g->challenge!=CHALLENGE_INTERRUPT||g->challenge_state)return;
    if(++g->challenge_progress>=3)challenge_success(g);
}
void encounters_kill(Game *g,bool champion) {
    if(g->prediction)return;
    if(g->reactor_cooldown==0&&g->reactor_charge<REACTOR_CHARGE)g->reactor_charge++;
    if(champion&&g->challenge==CHALLENGE_CHAMPION&&g->challenge_deadline>0) {
        g->challenge_progress=1;challenge_success(g);
    }
}
void encounters_down(Game *g) {
    if(g->challenge==CHALLENGE_SURVIVE&&g->challenge_state==0) {
        g->challenge_state=2;g->challenge_notice=4;
    }
}
void encounters_update(Game *g,float dt) {
    g->reactor_cooldown=fmaxf(0,g->reactor_cooldown-dt);
    g->challenge_notice=fmaxf(0,g->challenge_notice-dt);
    if(g->challenge==CHALLENGE_CHAMPION&&g->challenge_state==0) {
        g->challenge_deadline=fmaxf(0,g->challenge_deadline-dt);
        if(g->challenge_deadline==0){g->challenge_state=2;g->challenge_notice=4;}
    }
    for(int i=0;i<MAX_DECKS;i++)g->supplies[i].cooldown=fmaxf(0,g->supplies[i].cooldown-dt);
}

bool reactor_in_range(const Player *p) {
    return hypotf(p->body.position.x,p->body.position.z)<3.4f&&fabsf(p->body.position.y-1.8f)<.65f;
}
void encounters_reactor(Game *g) {
    if(g->prediction||g->dead||g->round_over||g->player.upgrade_pending||g->wave_timer>=0
        ||g->reactor_charge<REACTOR_CHARGE||g->reactor_cooldown>0||!reactor_in_range(&g->player))return;
    g->reactor_charge=0;g->reactor_cooldown=45;
    for(int id=0;id<MAX_PLAYERS;id++)if(alive(g,id)) {
        Player *p=player(g,id);p->reactor_time=REACTOR_DURATION;
        p->health=fminf(player_max_health(p),p->health+15);
    }
    snprintf(g->message,sizeof(g->message),"REACTEUR ACTIVE / +15 PV / BONUS D'EQUIPE PENDANT 15 S");
    g->message_time=4;audio_play_at(SOUND_UPGRADE,(Vec3){0,2,0});
}
void encounters_actor(Game *g,float dt) {
    Player *p=&g->player;Actor *a=&g->actors[g->current_actor];
    a->heavy_cooldown=fmaxf(0,a->heavy_cooldown-dt);
    for(int i=0;i<DAMAGE_DIRECTIONS;i++)a->damage_timers[i]=fmaxf(0,a->damage_timers[i]-dt);
    float combat_dt=g->wave_timer<0?dt:0;
    p->shield_time=fmaxf(0,p->shield_time-combat_dt);if(p->shield_time==0)p->shield=0;
    p->speed_time=fmaxf(0,p->speed_time-combat_dt);p->power_time=fmaxf(0,p->power_time-combat_dt);
    p->reactor_time=fmaxf(0,p->reactor_time-combat_dt);
    p->dash_power_time=fmaxf(0,p->dash_power_time-dt);p->switch_power_time=fmaxf(0,p->switch_power_time-dt);
    if(g->prediction||g->dead||p->upgrade_pending||g->wave_timer>=0)return;
    for(int i=0;i<MAX_DECKS;i++) {
        Supply *s=&g->supplies[i];Vec3 delta=vsub(s->position,p->body.position);
        if(s->cooldown>0||fabsf(delta.y)>.8f||hypotf(delta.x,delta.z)>1.1f)continue;
        if(s->kind==BONUS_SHIELD){p->shield=30;p->shield_time=12;}
        else if(s->kind==BONUS_POWER)p->power_time=12;
        else p->speed_time=8;
        static const char *names[]={"BOUCLIER : 30 POINTS / 12 S", "SURTENSION : PROCHAIN TIR +65% / 12 S", "CELERITE : VITESSE +20% / 8 S"};
        snprintf(g->message,sizeof(g->message),"%s",names[s->kind]);g->message_time=2;
        s->cooldown=30;s->kind=(s->kind+1)%3;
        if(g->current_actor==g->local_actor)audio_play(SOUND_PICKUP);
    }
}

bool game_heavy_available(const Game *g,int target) {
    if(target<0||target>=MAX_PLAYERS||g->actors[target].heavy_cooldown>0)return false;
    const Player *p=target==g->current_actor?&g->player:&g->actors[target].player;
    for(int i=0;i<g->bomb_count;i++) {
        const Bomb *b=&g->bombs[i];Vec3 d=vsub(p->body.position,b->target);
        if(b->target_actor==target||(hypotf(d.x,d.z)<BOMB_RADIUS+6&&fabsf(d.y)<7))return false;
    }
    for(int i=0;i<g->enemy_count;i++) {
        const Enemy *e=&g->enemies[i];if(e->health<=0||!e->slam_state)continue;
        Vec3 d=vsub(p->body.position,e->slam_target);
        if(e->target_actor==target||(hypotf(d.x,d.z)<12&&fabsf(d.y)<7))return false;
    }
    return true;
}
