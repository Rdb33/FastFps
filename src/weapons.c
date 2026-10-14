#include "fastfps.h"

const Weapon WEAPONS[WEAPON_COUNT]={
    {"PULSE",18,1,.15f,1.25f,34,{.35f,.85f,1}},
    {"FUSIL A POMPE",6,10,.72f,1.85f,18,{1,.64f,.25f}},
    {"MITRAILLEUSE",32,20,.085f,1.65f,24,{.53f,1,.45f}},
    {"LANCE RAIL",4,30,1.0f,2.1f,170,{.8f,.45f,1}},
    {"LANCE ROQUETTES",3,0,.8f,2.1f,180,{1,.4f,.12f}}
};
int game_unlocked_weapons(const Game *g) {
    int count=1;
    for(int i=1;i<STANDARD_WEAPONS;i++)if(g->wave>=WEAPONS[i].unlock_wave)count=i+1;
    return count;
}
bool game_weapon_available(const Game *g,int weapon) {
    return weapon>=0&&weapon<WEAPON_COUNT&&(weapon==WEAPON_ROCKET?g->rocket_event:weapon<game_unlocked_weapons(g));
}
void game_cycle_weapon(Game *g,int direction) {
    int next=g->player.weapon;
    for(int i=0;i<WEAPON_COUNT;i++) {
        next=(next+(direction<0?WEAPON_COUNT-1:1))%WEAPON_COUNT;
        if(game_weapon_available(g,next)){game_request_weapon(g,next);return;}
    }
}
void game_reload(Game *g) {
    Player *p=&g->player;const Weapon *w=&WEAPONS[p->weapon];
    if(g->dead||g->round_over||p->upgrade_pending||p->reload_timer>0||p->weapon_switch>0
        ||p->melee_timer>0||p->ammo[p->weapon]>=w->magazine)return;
    p->reload_duration=w->reload;p->reload_timer=w->reload;
    audio_play(SOUND_SWITCH);
}
void game_begin_rocket_event(Game *g) {
    if(g->prediction||g->rocket_event)return;
    g->rocket_event=true;g->event_last_wave=g->wave;g->event_serial++;g->event_notice_time=6;
    int original=g->current_actor;
    for(int id=0;id<MAX_PLAYERS;id++)if(g->actors[id].active) {
        game_actor_select(g,id);Player *p=&g->player;
        p->previous_weapon=(uint8_t)p->weapon;p->weapon=p->weapon_target=WEAPON_ROCKET;
        p->ammo[WEAPON_ROCKET]=(uint8_t)WEAPONS[WEAPON_ROCKET].magazine;
        p->reload_timer=p->weapon_switch=p->melee_timer=0;g->shot_cooldown=0;
        game_actor_save(g);
    }
    game_actor_select(g,original);audio_play(SOUND_CHAMPION);
}
void game_end_rocket_event(Game *g) {
    if(!g->rocket_event)return;
    g->rocket_event=false;
    int original=g->current_actor;
    for(int id=0;id<MAX_PLAYERS;id++)if(g->actors[id].active) {
        game_actor_select(g,id);Player *p=&g->player;
        if(p->weapon==WEAPON_ROCKET||p->weapon_target==WEAPON_ROCKET) {
            p->weapon=p->weapon_target=p->previous_weapon<game_unlocked_weapons(g)?p->previous_weapon:0;
            p->reload_timer=p->weapon_switch=0;g->shot_cooldown=0;
        }
        game_actor_save(g);
    }
    game_actor_select(g,original);
}
void game_roll_event(Game *g) {
    if(g->prediction||g->rocket_event||g->wave<3||g->wave-g->event_last_wave<5)return;
    /* Un seul tirage par round, sur l'hôte. RNG séparé des particules et tirs. */
    g->event_rng=g->event_rng*1664525u+1013904223u;
    if((g->event_rng>>16)%100<8)game_begin_rocket_event(g);
}
