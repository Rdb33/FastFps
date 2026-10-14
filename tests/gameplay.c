#include "network.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

void audio_play(Sound s){(void)s;}
void audio_play_at(Sound s,Vec3 p){(void)s;(void)p;}
void audio_listener(Vec3 p,float yaw){(void)p;(void)yaw;}
void audio_suppressed(bool s){(void)s;}

static void quiet(Game *g,World *w) {
    game_reset(g,w,28);g->wave=1;g->wave_timer=-1;
    g->enemy_count=1;g->enemies[0]=(Enemy){.body={{21,0,-17},{0,0,0},true},.health=100,.max_health=100,.spawn_time=100};
}
static void weapons(World *w) {
    Game g;quiet(&g,w);
    game_request_weapon(&g,1);assert(g.player.weapon_switch==0);
    g.player.ammo[0]=1;game_shoot(&g);
    assert(g.player.ammo[0]==0&&g.player.reload_timer>0&&g.actors[0].shots==1);
    game_shoot(&g);assert(g.actors[0].shots==1);
    for(int i=0;i<80;i++)game_update(&g,(Input){0},FIXED_STEP);
    assert(g.player.reload_timer==0&&g.player.ammo[0]==WEAPONS[0].magazine);
    game_reload(&g);assert(g.player.reload_timer==0);
    game_shoot(&g);game_reload(&g);assert(g.player.reload_timer>0);
    game_request_weapon(&g,1);assert(g.player.weapon_switch==0);
    g.player.reload_timer=0;
    for(int i=1;i<STANDARD_WEAPONS;i++) {
        g.wave=WEAPONS[i].unlock_wave-1;game_request_weapon(&g,i);assert(g.player.weapon_switch==0);
        g.wave++;game_request_weapon(&g,i);assert(g.player.weapon_target==i&&g.player.weapon_switch>0);
        game_update(&g,(Input){0},WEAPON_SWITCH_TIME+.01f);
        assert(g.player.weapon==i);
        g.player.ammo[i]=1;g.shot_cooldown=0;game_shoot(&g);
        assert(g.player.ammo[i]==0&&g.player.reload_timer==WEAPONS[i].reload);
        for(int tick=0;tick<140;tick++)game_update(&g,(Input){0},FIXED_STEP);
        assert(g.player.reload_timer==0&&g.player.ammo[i]==WEAPONS[i].magazine);
    }
}
static void solo_lives(World *w) {
    Game g;quiet(&g,w);g.player.upgrades[UPGRADE_VITALITY]=1;
    for(int remaining=2;remaining>=0;remaining--) {
        g.player.health=1;g.actors[0].invulnerable=0;g.projectile_count=1;
        g.projectiles[0]=(Projectile){.position=vadd(g.player.body.position,(Vec3){0,1,0}),.velocity={0,0,1},.life=1,.color={1,1,1}};
        game_update(&g,(Input){0},FIXED_STEP);
        assert(g.dead&&g.lives==remaining&&g.round_over==(remaining==0));
        if(remaining) {
            for(int i=0;i<182;i++)game_update(&g,(Input){0},FIXED_STEP);
            assert(!g.dead&&g.player.health==115&&g.player.upgrades[UPGRADE_VITALITY]==1&&g.wave==1);
        }
    }
    game_update(&g,(Input){0},10);assert(g.dead&&g.round_over&&g.lives==0);
}
static void maps(World *w) {
    for(int id=0;id<MAP_COUNT;id++) {
        world_set_map(w,id);assert(w->map_id==id&&w->mesh.count>0&&w->mesh.count<w->mesh.capacity);
        navigation_update(w,0,19);
        for(int i=0;i<8;i++) {
            Vec3 p=w->spawns[i];assert(!world_blocked(w,p,ENEMY_RADIUS*1.6f,ENEMY_HEIGHT*1.6f,-1));
            int x=(int)floorf((p.x+28)/2),z=(int)floorf((p.z+24)/2);
            assert(w->passable[z*NAV_W+x]&&w->distance[z*NAV_W+x]>=0);
        }
    }
    Game g;game_reset(&g,w,28);
    for(int wave=1;wave<=32;wave++) {
        g.enemy_count=0;g.projectile_count=0;g.wave_timer=.001f;
        g.player.upgrade_pending=false;
        game_update(&g,(Input){0},FIXED_STEP);
        assert(g.wave==wave&&g.map_id==((wave-1)/5)%MAP_COUNT&&w->map_id==g.map_id);
        assert(!world_blocked(w,g.player.body.position,PLAYER_RADIUS,PLAYER_HEIGHT,-1));
        for(int i=0;i<g.barrel_count;i++)assert(!world_blocked(w,g.barrels[i].position,.43f,1.1f,-1));
        assert(g.enemy_count>0&&g.enemy_count<=MAX_ENEMIES);
        for(int i=0;i<g.enemy_count;i++)assert(!world_blocked(w,g.enemies[i].body.position,ENEMY_RADIUS*enemy_size(&g.enemies[i]),ENEMY_HEIGHT*enemy_size(&g.enemies[i]),-1));
    }
    game_reset(&g,w,28);assert(g.map_id==0&&w->map_id==0&&g.lives==3);
}
static void modules_and_wipe(World *w) {
    Game g;quiet(&g,w);g.enemy_count=0;
    game_update(&g,(Input){0},FIXED_STEP);
    assert(g.player.upgrade_pending&&!game_choose_upgrade(&g,1));
    int chosen=g.player.offers[0];
    for(int i=0;i<62;i++)game_update(&g,(Input){0},FIXED_STEP);
    assert(game_choose_upgrade(&g,1)&&g.player.upgrades[chosen]==1&&!game_choose_upgrade(&g,1));
    game_reset(&g,w,28);bool active[MAX_PLAYERS]={true,true,false,false};game_start_coop(&g,active,0);
    g.dead=true;g.player.health=0;game_actor_save(&g);
    g.actors[1].dead=true;g.actors[1].player.health=0;
    g.actors[0].respawn_timer=g.actors[1].respawn_timer=0;
    game_update_coop(&g,(Input[MAX_PLAYERS]){0},FIXED_STEP);
    assert(g.round_over&&g.dead&&g.actors[1].dead);
}
static void snapshots(World *w) {
    Game g;quiet(&g,w);bool active[MAX_PLAYERS]={true,true,true,true};game_start_coop(&g,active,0);
    g.wave=41;g.map_id=2;world_set_map(w,2);
    for(int i=0;i<MAX_PLAYERS;i++){g.actors[i].player.weapon=g.actors[i].player.weapon_target=3;g.actors[i].player.ammo[3]=2;g.actors[i].player.reload_timer=1;g.actors[i].player.reload_duration=2.1f;}
    g.current_actor=-1;game_actor_select(&g,0);
    g.enemy_count=MAX_ENEMIES;g.corpse_count=MAX_CORPSES;
    for(int i=0;i<MAX_ENEMIES;i++)g.enemies[i]=(Enemy){.id=i,.health=100,.max_health=100};
    for(int i=0;i<MAX_CORPSES;i++)g.corpses[i]=(Enemy){.id=100+i,.death_time=1,.max_health=100};
    g.projectile_count=MAX_PROJECTILES;for(int i=0;i<MAX_PROJECTILES;i++)g.projectiles[i]=(Projectile){.life=1};
    g.particle_count=MAX_PARTICLES;for(int i=0;i<MAX_PARTICLES;i++)g.particles[i]=(Particle){.life=1,.max_life=1,.size=.1f};
    uint8_t data[NET_PACKET_MAX];size_t n=protocol_snapshot_write(data,sizeof(data),&g);assert(n>0&&n<NET_PACKET_MAX);
    World other;assert(world_init(&other));Game copy;game_reset(&copy,&other,MAX_PARTICLES);copy.local_actor=1;
    uint32_t before=other.revision;assert(!protocol_snapshot_read(&copy,data,n-1));assert(other.map_id==0&&other.revision==before);
    assert(protocol_snapshot_read(&copy,data,n));assert(other.map_id==2&&copy.player.weapon==3&&copy.player.ammo[3]==2&&copy.player.reload_timer==1);
    uint8_t saved=data[6];data[6]=255;assert(!protocol_snapshot_read(&copy,data,n));data[6]=saved;
    mesh_free(&other.mesh);printf("Etat maximal : %zu / %d octets\n",n,NET_PACKET_MAX);
}
static SDL_Event mouse(Uint32 type,int x,int y) {
    SDL_Event e={0};e.type=type;e.button.button=SDL_BUTTON_LEFT;e.button.x=x;e.button.y=y;return e;
}
static void controls(World *w) {
    Renderer r={0};r.window=SDL_CreateWindow("test",0,0,640,360,SDL_WINDOW_HIDDEN);assert(r.window);
    App a={.state=STATE_MENU,.menu_page=1,.sensitivity=1,.brightness=1,.numeric_edit=-1,.slider_drag=-1};
    SDL_Event e=mouse(SDL_MOUSEBUTTONDOWN,395,151);assert(settings_event(&a,&r,&e)&&a.slider_drag==3);
    e=(SDL_Event){.type=SDL_MOUSEMOTION};e.motion.x=-10;e.motion.y=151;assert(settings_event(&a,&r,&e)&&a.sensitivity==.3f);
    e=mouse(SDL_MOUSEBUTTONUP,0,151);assert(settings_event(&a,&r,&e)&&a.slider_drag==-1);
    e=mouse(SDL_MOUSEBUTTONDOWN,505,259);assert(settings_event(&a,&r,&e)&&a.numeric_edit==9);
    e=(SDL_Event){.type=SDL_TEXTINPUT};strcpy(e.text.text,"900");assert(settings_event(&a,&r,&e));
    e=(SDL_Event){.type=SDL_KEYDOWN};e.key.keysym.scancode=SDL_SCANCODE_RETURN;
    assert(settings_event(&a,&r,&e)&&a.brightness==2&&a.numeric_edit==-1);
    e=mouse(SDL_MOUSEBUTTONDOWN,505,151);assert(settings_event(&a,&r,&e));
    e=(SDL_Event){.type=SDL_TEXTINPUT};strcpy(e.text.text,"1,37");assert(settings_event(&a,&r,&e));
    e=(SDL_Event){.type=SDL_KEYDOWN};e.key.keysym.scancode=SDL_SCANCODE_RETURN;
    assert(settings_event(&a,&r,&e)&&fabsf(a.sensitivity-1.37f)<.001f);
    Game g;quiet(&g,w);game_offer_upgrades(&g);
    a.state=STATE_UPGRADE;a.upgrade_released=true;a.upgrade_ready_at=SDL_GetTicks()-1;
    e=mouse(SDL_MOUSEBUTTONDOWN,60,150);assert(upgrade_event(&a,&g,&r,&e)&&a.upgrade_choice==0&&!a.upgrade_confirm);
    g.player.upgrade_lock=0;a.upgrade_released=false;
    assert(upgrade_event(&a,&g,&r,&e)&&a.upgrade_choice==0&&!a.upgrade_confirm);
    e=mouse(SDL_MOUSEBUTTONUP,60,150);assert(upgrade_event(&a,&g,&r,&e));
    e=mouse(SDL_MOUSEBUTTONDOWN,60,150);assert(upgrade_event(&a,&g,&r,&e)&&a.upgrade_confirm&&a.upgrade_choice==0);
    assert(upgrade_event(&a,&g,&r,&e)&&a.upgrade_choice==0);
    e=mouse(SDL_MOUSEBUTTONDOWN,320,265);assert(upgrade_event(&a,&g,&r,&e)&&a.upgrade_choice==1);
    SDL_DestroyWindow(r.window);
}
int main(void) {
    SDL_setenv("SDL_VIDEODRIVER","dummy",1);assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)==0);
    World w;assert(world_init(&w));
    weapons(&w);solo_lives(&w);maps(&w);modules_and_wipe(&w);snapshots(&w);controls(&w);
    mesh_free(&w.mesh);SDL_Quit();puts("OK : vies, armes, rechargement, maps, modules, game over, etats reseau, sliders et saisie");return 0;
}
