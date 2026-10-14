#include "network.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

void audio_play(Sound s){(void)s;}
void audio_play_at(Sound s,Vec3 p){(void)s;(void)p;}
void audio_listener(Vec3 p,float yaw){(void)p;(void)yaw;}
void audio_suppressed(bool s){(void)s;}

static Enemy inert(Vec3 p){return (Enemy){.body={p,{0,0,0},true},.health=1000,.max_health=1000,.spawn_time=1000};}
static void quiet(Game *g,World *w,bool coop) {
    game_reset(g,w,64);
    if(coop)game_start_coop(g,(bool[MAX_PLAYERS]){true,true,false,false},0);
    g->wave=3;g->wave_timer=-1;g->enemy_count=1;g->enemies[0]=inert((Vec3){21,0,-17});g->barrel_count=0;
    if(coop)g->actors[1].player.body=(Body){vadd(g->player.body.position,(Vec3){1.5f,0,0}),{0,0,0},true};
}
static void down(Game *g,int id) {
    if(id==g->current_actor){g->dead=true;g->player.health=0;game_actor_save(g);}
    else {g->actors[id].dead=true;g->actors[id].player.health=0;}
}
static void revive_tests(World *w) {
    Game g;quiet(&g,w,true);down(&g,1);g.actors[1].respawn_timer=.01f;
    Input inputs[MAX_PLAYERS]={0};
    for(int i=0;i<720;i++)game_update_coop(&g,inputs,FIXED_STEP);
    assert(g.actors[1].dead&&!g.round_over&&g.actors[1].revive_progress==0);
    inputs[0].revive=true;
    for(int i=0;i<60;i++)game_update_coop(&g,inputs,FIXED_STEP);
    assert(g.actors[1].dead&&g.actors[1].revive_progress>.9f);
    inputs[0].revive=false;game_update_coop(&g,inputs,FIXED_STEP);assert(g.actors[1].revive_progress==0);
    inputs[0].revive=true;
    for(int i=0;i<60;i++)game_update_coop(&g,inputs,FIXED_STEP);
    g.projectile_count=1;g.projectiles[0]=(Projectile){.position=vadd(g.player.body.position,(Vec3){0,1,0}),.velocity={0,0,1},.life=1,.owner=-1};
    game_update_coop(&g,inputs,FIXED_STEP);assert(g.player.health<100&&g.actors[1].revive_progress==0);
    for(int i=0;i<165;i++)game_update_coop(&g,inputs,FIXED_STEP);
    assert(!g.actors[1].dead&&g.actors[1].player.health==50&&g.actors[1].invulnerable>0);
    down(&g,1);g.actors[1].player.body.position.x=15;
    for(int i=0;i<180;i++)game_update_coop(&g,inputs,FIXED_STEP);
    assert(g.actors[1].dead&&g.actors[1].revive_progress==0);
    world_set_map(w,2);g.map_id=2;g.wave=11;world_platforms_update(w,g.time);
    g.player.body=(Body){{13,0,8.61f},{0,0,0},true};g.actors[1].player.body=(Body){{13,0,11.39f},{0,0,0},true};
    assert(vlength(vsub(g.player.body.position,g.actors[1].player.body.position))<REVIVE_RANGE);
    assert(game_revive_target(&g,0)==-1);
    down(&g,0);g.actors[1].revive_progress=REVIVE_TIME-.001f;
    game_update_coop(&g,inputs,FIXED_STEP);assert(g.round_over&&g.dead&&g.actors[1].dead);
}
static void rocket_tests(World *w) {
    Game g;quiet(&g,w,true);down(&g,1);game_begin_rocket_event(&g);
    assert(g.rocket_event&&g.player.weapon==WEAPON_ROCKET&&g.actors[1].player.weapon==WEAPON_ROCKET&&g.actors[1].dead);
    game_cycle_weapon(&g,1);assert(g.player.weapon_target==0);g.player.weapon_switch=0;g.player.weapon_target=4;
    game_end_rocket_event(&g);assert(!g.rocket_event&&g.player.weapon==0&&g.actors[1].player.weapon==0);
    game_request_weapon(&g,4);assert(g.player.weapon_switch==0);
    quiet(&g,w,false);game_begin_rocket_event(&g);
    g.enemy_count=3;g.enemies[0]=inert((Vec3){0,0,11});g.enemies[1]=inert((Vec3){2,0,11});g.enemies[2]=inert((Vec3){21,0,-17});
    game_shoot(&g);assert(g.projectile_count==1&&g.projectiles[0].kind==1&&g.projectiles[0].owner==0&&g.player.ammo[4]==2);
    for(int i=0;i<18;i++)game_update(&g,(Input){0},FIXED_STEP);
    assert(g.enemies[0].health<1000&&g.enemies[1].health<1000&&g.enemies[2].health==1000&&g.blast_count>0);
    quiet(&g,w,false);g.enemy_count=2;g.enemies[0]=inert((Vec3){21,0,12});g.enemies[1]=inert((Vec3){16.5f,0,14});
    g.projectile_count=1;g.projectiles[0]=(Projectile){.position={16.5f,.1f,12},.velocity={0,-30,0},.life=3,.kind=1,.owner=0,.damage=180};
    game_update(&g,(Input){0},FIXED_STEP);
    assert(g.enemies[0].health==1000&&g.enemies[1].health<1000);
    quiet(&g,w,true);game_begin_rocket_event(&g);
    g.projectile_count=1;g.projectiles[0]=(Projectile){.position=vadd(g.player.body.position,(Vec3){0,.1f,0}),.velocity={0,-30,0},.life=3,.kind=1,.owner=0,.damage=180};
    game_update_coop(&g,(Input[MAX_PLAYERS]){0},FIXED_STEP);
    assert(g.player.health<100&&g.player.body.velocity.y>10&&g.player.boost_timer>.5f&&g.actors[1].player.health==100);
    game_update_coop(&g,(Input[MAX_PLAYERS]){0},FIXED_STEP);assert(g.player.body.position.y>0);
    g.enemy_count=0;g.wave_timer=-1;game_update_coop(&g,(Input[MAX_PLAYERS]){0},FIXED_STEP);
    assert(!g.rocket_event&&g.player.weapon!=WEAPON_ROCKET);
    quiet(&g,w,false);g.event_rng=1;int events=0;
    for(int i=0;i<1000;i++) {
        g.wave=3+i*6;game_roll_event(&g);
        if(g.rocket_event){events++;game_end_rocket_event(&g);}
    }
    assert(events>40&&events<120);
    g.wave=3;g.event_last_wave=2;game_roll_event(&g);assert(!g.rocket_event);
    printf("Evenements rares : %d / 1000 tirages eligibles\n",events);
}
static void champion_tests(World *w) {
    Game g;quiet(&g,w,false);g.enemies[0]=inert((Vec3){0,0,13});
    Enemy *e=&g.enemies[0];e->spawn_time=0;e->type=ENEMY_BRUTE;e->elite=true;e->id=12;
    for(int i=0;i<40;i++)game_update(&g,(Input){0},FIXED_STEP);
    assert(e->slam_state==1&&g.player.health==100);
    bool airborne=false,landed=false;float peak=0;int damage_count=0;
    for(int i=0;i<140;i++) {
        float before=g.player.health;game_update(&g,(Input){0},FIXED_STEP);
        peak=fmaxf(peak,e->body.position.y);
        if(e->slam_state==2)airborne=true;
        if(e->slam_state==3)landed=true;
        if(g.player.health<before)damage_count++;
    }
    assert(airborne&&landed&&peak>1.5f&&damage_count==1&&g.player.health<100);
    quiet(&g,w,false);e=&g.enemies[0];*e=inert((Vec3){0,.01f,16});e->spawn_time=0;e->elite=true;e->type=ENEMY_BRUTE;
    e->slam_state=2;e->slam_timer=1;e->body.grounded=false;e->body.velocity.y=-1;
    g.player.body.position.y=1.2f;g.player.body.grounded=false;g.player.body.velocity.y=0;
    game_update(&g,(Input){0},FIXED_STEP);assert(e->slam_state==3&&g.player.health==100);
    quiet(&g,w,false);world_set_map(w,2);g.map_id=2;g.wave=11;
    g.player.body.position=(Vec3){13,0,12};e=&g.enemies[0];*e=inert((Vec3){13,.01f,8});e->spawn_time=0;e->elite=true;e->type=ENEMY_BRUTE;
    e->slam_state=2;e->slam_timer=1;e->body.grounded=false;e->body.velocity.y=-1;
    game_update(&g,(Input){0},FIXED_STEP);assert(g.player.health==100&&e->slam_state==3);
}
static void ai_tests(World *w) {
    Game g;quiet(&g,w,true);g.enemies[0]=inert((Vec3){0,0,14});Enemy *e=&g.enemies[0];e->type=ENEMY_CASTER;
    Vec3 move=ai_move(&g,e,&g.player,true,FIXED_STEP);assert(move.z<0);
    g.enemy_count=2;g.enemies[0]=inert((Vec3){0,0,10});g.enemies[1]=g.enemies[0];g.enemies[1].id=1;
    move=ai_move(&g,&g.enemies[0],&g.player,true,FIXED_STEP);Vec3 other=ai_move(&g,&g.enemies[1],&g.player,true,FIXED_STEP);
    assert(move.x>0&&other.x<0);
    down(&g,0);assert(ai_target(&g,&g.enemies[0])==1);
    navigation_actor(w,0,(Vec3){-23,0,-19});navigation_actor(w,1,(Vec3){23,0,-19});
    assert(navigation_for_actor(w,0,0,-19).x<0&&navigation_for_actor(w,1,0,-19).x>0);
    g.enemies[0].stuck_time=1;g.enemies[0].body.position=(Vec3){8.35f,0,-8};g.enemies[0].target_actor=1;
    Vec3 position=g.enemies[0].body.position;g.player.body.position=(Vec3){15,0,-8};
    move=ai_move(&g,&g.enemies[0],&g.player,true,FIXED_STEP);
    assert(!world_blocked(w,vadd(position,vscale(move,.8f)),ENEMY_RADIUS,ENEMY_HEIGHT,-1));
}
static void platform_tests(World *w) {
    Game host,guest;World other;assert(world_init(&other));quiet(&host,w,false);quiet(&guest,&other,false);
    Vec3 foot=w->platforms[0].base;foot.y=w->solids[w->platforms[0].solid].max.y;host.player.body=(Body){foot,{0,0,0},true};guest.player.body=host.player.body;
    for(int tick=0;tick<240;tick++) {
        game_update(&host,(Input){0},FIXED_STEP);game_predict(&guest,(Input){0},FIXED_STEP);
        Box b=w->solids[w->platforms[0].solid];
        assert(fabsf(host.player.body.position.y-b.max.y)<.02f);
        assert(vlength(vsub(host.player.body.position,guest.player.body.position))<.01f);
        assert(!world_blocked(w,host.player.body.position,PLAYER_RADIUS,PLAYER_HEIGHT,-1));
    }
    assert(host.player.body.position.y>3.4f);
    game_update(&host,(Input){.jump=true},FIXED_STEP);assert(!host.player.body.grounded&&host.player.body.velocity.y>0);
    quiet(&host,w,false);Vec3 start=w->platforms[2].base;start.y=w->solids[w->platforms[2].solid].max.y;
    host.player.body=(Body){start,{0,0,0},true};
    for(int tick=0;tick<(int)(w->platforms[2].period*.5f/FIXED_STEP);tick++)game_update(&host,(Input){0},FIXED_STEP);
    Box shuttle=w->solids[w->platforms[2].solid];
    assert(host.player.body.position.x>start.x+w->platforms[2].travel.x*.9f&&fabsf(host.player.body.position.x-(shuttle.min.x+shuttle.max.x)/2)<.02f);
    quiet(&host,w,false);host.time=4;world_platforms_update(w,host.time);
    Vec3 lower=w->platforms[0].base;host.player.body=(Body){lower,{0,0,0},true};
    for(int tick=0;tick<240;tick++) {
        game_update(&host,(Input){0},FIXED_STEP);
        assert(!world_blocked(w,host.player.body.position,PLAYER_RADIUS,PLAYER_HEIGHT,-1));
    }
    assert(fabsf(host.player.body.position.x-lower.x)>w->platforms[0].size.x/2+PLAYER_RADIUS
        ||fabsf(host.player.body.position.z-lower.z)>w->platforms[0].size.z/2+PLAYER_RADIUS);
    mesh_free(&other.mesh);
}
static void snapshot_tests(World *w) {
    Game g;quiet(&g,w,true);game_begin_rocket_event(&g);down(&g,1);g.actors[1].revive_progress=1.2f;
    g.unlock_weapon=1;g.unlock_time=5;g.player.boost_timer=.4f;game_actor_save(&g);
    g.enemy_count=MAX_ENEMIES;g.corpse_count=MAX_CORPSES;
    for(int i=0;i<MAX_ENEMIES;i++){g.enemies[i]=inert((Vec3){0,2,13});g.enemies[i].elite=true;g.enemies[i].slam_state=2;g.enemies[i].slam_timer=1;g.enemies[i].slam_target=(Vec3){0,0,16};}
    for(int i=0;i<MAX_CORPSES;i++){g.corpses[i]=inert((Vec3){20,0,20});g.corpses[i].death_time=1;}
    g.projectile_count=MAX_PROJECTILES;
    for(int i=0;i<MAX_PROJECTILES;i++)g.projectiles[i]=(Projectile){.position={0,1,15},.velocity={0,0,-30},.kind=1,.owner=0,.life=2,.damage=180};
    g.blast_count=MAX_BLASTS;for(int i=0;i<MAX_BLASTS;i++)g.blasts[i]=(Blast){{0,.1f,15},.7f,6.5f};
    g.particle_count=MAX_PARTICLES;for(int i=0;i<MAX_PARTICLES;i++)g.particles[i]=(Particle){.life=1,.max_life=1,.size=.1f};
    uint8_t data[NET_PACKET_MAX];size_t len=protocol_snapshot_write(data,sizeof(data),&g);assert(len&&len<NET_PACKET_MAX);
    World other;assert(world_init(&other));Game copy;game_reset(&copy,&other,MAX_PARTICLES);copy.local_actor=1;
    assert(!protocol_snapshot_read(&copy,data,len-1));assert(protocol_snapshot_read(&copy,data,len));
    assert(copy.rocket_event&&copy.player.weapon==4&&copy.actors[1].revive_progress==1.2f&&copy.enemies[0].slam_state==2);
    assert(copy.projectiles[0].kind==1&&copy.projectiles[0].damage==180&&copy.blast_count==MAX_BLASTS&&copy.unlock_time==5);
    uint32_t revision=other.revision;int map=other.map_id;
    data[6]=255;assert(!protocol_snapshot_read(&copy,data,len));assert(other.revision==revision&&other.map_id==map);
    printf("Etat 0.9.2 maximal : %zu / %d octets\n",len,NET_PACKET_MAX);mesh_free(&other.mesh);
}
int main(void) {
    assert(SDL_Init(SDL_INIT_TIMER)==0);srand(1);World w;assert(world_init(&w));
    revive_tests(&w);rocket_tests(&w);champion_tests(&w);ai_tests(&w);platform_tests(&w);snapshot_tests(&w);
    mesh_free(&w.mesh);SDL_Quit();puts("OK : reanimations, evenements, degats de zone, rocket jump, champions, IA, plateformes et protocole");return 0;
}
