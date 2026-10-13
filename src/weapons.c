#include "fastfps.h"

const Weapon WEAPONS[WEAPON_COUNT]={
    {"PULSE",18,1,.15f,1.25f,34,{.35f,.85f,1}},
    {"FUSIL A POMPE",6,10,.72f,1.85f,18,{1,.64f,.25f}},
    {"MITRAILLEUSE",32,20,.085f,1.65f,24,{.53f,1,.45f}},
    {"LANCE RAIL",4,30,1.0f,2.1f,170,{.8f,.45f,1}}
};
int game_unlocked_weapons(const Game *g) {
    int count=1;
    for(int i=1;i<WEAPON_COUNT;i++)if(g->wave>=WEAPONS[i].unlock_wave)count=i+1;
    return count;
}
void game_cycle_weapon(Game *g,int direction) {
    int count=game_unlocked_weapons(g);
    game_request_weapon(g,(g->player.weapon+(direction<0?count-1:1))%count);
}
void game_reload(Game *g) {
    Player *p=&g->player;const Weapon *w=&WEAPONS[p->weapon];
    if(g->dead||g->round_over||p->upgrade_pending||p->reload_timer>0||p->weapon_switch>0
        ||p->melee_timer>0||p->ammo[p->weapon]>=w->magazine)return;
    p->reload_duration=w->reload;p->reload_timer=w->reload;
    audio_play(SOUND_SWITCH);
}
