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
    float progress_before=g.actors[1].revive_progress;
    game_update_coop(&g,inputs,FIXED_STEP);assert(g.player.health<100&&g.actors[1].revive_progress>progress_before);
    for(int i=0;i<165;i++)game_update_coop(&g,inputs,FIXED_STEP);
    assert(!g.actors[1].dead&&g.actors[1].player.health==50&&g.actors[1].invulnerable>0);
    down(&g,1);g.actors[1].player.body.position.x=15;
    for(int i=0;i<180;i++)game_update_coop(&g,inputs,FIXED_STEP);
    assert(g.actors[1].dead&&g.actors[1].revive_progress==0);
    world_set_map(w,2);g.map_id=2;g.wave=11;world_platforms_update(w,g.time);
    g.player.body=(Body){{2.79f,0,-17},{0,0,0},true};g.actors[1].player.body=(Body){{4.81f,0,-17},{0,0,0},true};
    assert(!world_blocked(w,g.player.body.position,PLAYER_RADIUS,PLAYER_HEIGHT,-1));
    assert(!world_blocked(w,g.actors[1].player.body.position,PLAYER_RADIUS,PLAYER_HEIGHT,-1));
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
    g.player.body.position=(Vec3){5,0,-17};e=&g.enemies[0];*e=inert((Vec3){2.2f,.01f,-17});e->spawn_time=0;e->elite=true;e->type=ENEMY_BRUTE;
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
    g.enemies[0].stuck_time=1;g.enemies[0].body.position=(Vec3){7.8f,0,-8};g.enemies[0].target_actor=1;
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
    printf("Etat maximal : %zu / %d octets\n",len,NET_PACKET_MAX);mesh_free(&other.mesh);
}
static void bomber_timing_test(World *w) {
    Game g;quiet(&g,w,false);g.wave=11;g.pickup_count=0;g.enemies[0]=inert((Vec3){0,0,10});
    Enemy *e=&g.enemies[0];e->type=ENEMY_BOMBER;e->spawn_time=0;e->shot_timer=0;
    game_update(&g,(Input){0},FIXED_STEP);assert(g.bomb_count==1&&g.bombs[0].time==2);
    e->shot_timer=1000;
    for(int i=0;i<114;i++)game_update(&g,(Input){0},FIXED_STEP);
    assert(g.bomb_count==1&&g.player.health==100);
    for(int i=0;i<9;i++)game_update(&g,(Input){0},FIXED_STEP);
    assert(g.bomb_count==0&&g.player.health<100);
    puts("OK : bombe visible a 1,9 s, explosion apres 2 s");
}
static void web_tests(World *w) {
    Game g;quiet(&g,w,false);w->solid_count=w->platform_count=0;g.pickup_count=0;
    g.projectile_count=1;g.projectiles[0]=(Projectile){.position={0,1,16},.velocity={0,0,-1},.life=1,.kind=2,.owner=-1};
    game_update(&g,(Input){0},FIXED_STEP);assert(g.player.web_timer==WEB_ROOT_TIME&&g.player.health==100);
    Vec3 start=g.player.body.position;
    for(int i=0;i<40;i++)game_update(&g,(Input){.forward=1,.right=1,.jump=true,.dash=true,.firing=true},FIXED_STEP);
    assert(hypotf(g.player.body.position.x-start.x,g.player.body.position.z-start.z)<.001f);
    assert(g.player.web_timer>0&&g.actors[0].shots>0);float remaining=g.player.web_timer;
    g.projectile_count=1;g.projectiles[0]=(Projectile){.position={0,1,16},.velocity={0,0,-1},.life=1,.kind=2,.owner=-1};
    game_update(&g,(Input){0},FIXED_STEP);assert(g.player.web_timer<remaining);
    for(int i=0;i<7;i++)game_update(&g,(Input){0},FIXED_STEP);
    assert(g.player.web_timer==0&&g.player.web_immunity>0);
    game_update(&g,(Input){.forward=1},FIXED_STEP);assert(g.player.body.position.z<start.z);
    g.player.web_timer=WEB_ROOT_TIME;start=g.player.body.position;
    for(int i=0;i<40;i++)game_predict(&g,(Input){.right=1,.dash=true,.jump=true},FIXED_STEP);
    assert(hypotf(g.player.body.position.x-start.x,g.player.body.position.z-start.z)<.001f);
    puts("OK : toile, tirs pendant immobilisation, immunite et prediction");
}
static void climbing_tests(World *w) {
    Game g;quiet(&g,w,false);w->solid_count=1;w->platform_count=0;
    w->solids[0]=(Box){{-2,0,0},{2,.9f,2}};
    Enemy e=inert((Vec3){0,0,2.6f});e.spawn_time=0;e.type=ENEMY_RUNNER;
    assert(enemy_try_climb(&g,&e,(Vec3){0,0,-1}));
    for(int i=0;i<29;i++)assert(enemy_update_climb(&g,&e,FIXED_STEP));
    assert(e.climb_timer>0&&!e.body.grounded);
    for(int i=0;i<3;i++)enemy_update_climb(&g,&e,FIXED_STEP);
    assert(e.climb_timer==0&&e.body.grounded&&fabsf(e.body.position.y-.9f)<.001f&&e.body.position.z<1.5f);
    e=inert((Vec3){0,0,2.6f});e.spawn_time=0;e.type=ENEMY_BRUTE;assert(!enemy_try_climb(&g,&e,(Vec3){0,0,-1}));
    e.type=ENEMY_RUNNER;w->solid_count=2;w->solids[1]=(Box){{-2,2.2f,0},{2,2.4f,2}};
    assert(!enemy_try_climb(&g,&e,(Vec3){0,0,-1}));
    w->solid_count=1;assert(enemy_try_climb(&g,&e,(Vec3){0,0,-1}));
    for(int i=0;i<15;i++)enemy_update_climb(&g,&e,FIXED_STEP);
    w->solids[0].min.y+=.25f;w->solids[0].max.y+=.25f;
    for(int i=0;i<17;i++)enemy_update_climb(&g,&e,FIXED_STEP);
    assert(e.climb_timer==0&&fabsf(e.body.position.y-1.15f)<.001f);
    puts("OK : escalade en 0,5 s, gabarit lourd, plafond et dalle mobile");
}
static void finisher_push_tests(World *w) {
    Game g;quiet(&g,w,false);w->solid_count=w->platform_count=0;
    g.enemy_count=4;g.enemies[0]=inert((Vec3){0,0,14});g.enemies[0].health=20;
    g.enemies[1]=inert((Vec3){1.4f,0,13.4f});g.enemies[2]=inert((Vec3){-1.4f,0,13.4f});
    g.enemies[2].elite=true;g.enemies[2].type=ENEMY_BRUTE;g.enemies[3]=inert((Vec3){8,0,12});
    game_melee(&g);assert(g.enemies[0].health<=0&&g.enemies[1].health==1000);
    assert(g.enemies[1].knockback.x>1&&g.enemies[1].knockback.z<0);
    assert(fabsf(g.enemies[2].knockback.x)<g.enemies[1].knockback.x&&g.enemies[3].knockback.x==0);
    quiet(&g,w,false);w->solid_count=1;w->platform_count=0;w->solids[0]=(Box){{.7f,0,13},{.9f,3,15}};
    g.enemy_count=2;g.enemies[0]=inert((Vec3){0,0,14});g.enemies[0].health=20;g.enemies[1]=inert((Vec3){1.4f,0,13.4f});
    game_melee(&g);assert(g.enemies[0].health<=0&&vlength(g.enemies[1].knockback)==0);
    puts("OK : recul de frappe fatale, resistance du champion, obstruction");
}
static void intro_and_weaver_tests(World *w) {
    Game g;game_reset(&g,w,64);g.wave=4;g.wave_timer=.001f;game_update(&g,(Input){0},FIXED_STEP);
    assert(g.wave==5&&g.intro_time==CHAMPION_INTRO_TIME&&g.intro_serial==1);
    Vec3 start=g.player.body.position;float clock=g.time,health=g.player.health;
    for(int i=0;i<40;i++)game_update(&g,(Input){.forward=1,.firing=true,.jump=true},FIXED_STEP);
    assert(g.intro_time>0&&g.time==clock&&g.player.health==health&&vlength(vsub(g.player.body.position,start))==0);
    game_update(&g,(Input){0},FIXED_STEP);game_update(&g,(Input){.jump=true},FIXED_STEP);assert(g.intro_time==0);
    for(int wave=10;wave<=15;wave+=5) {
        g.wave=wave-1;g.wave_timer=.001f;g.enemy_count=g.reinforcements=0;g.wave_spawned=g.wave_total;
        g.player.upgrade_pending=false;game_update(&g,(Input){0},FIXED_STEP);assert(g.wave==wave&&g.intro_time==0&&g.intro_serial==1);
    }
    int introductions=0,last=5;
    for(int wave=20;wave<=200;wave+=5) {
        game_end_rocket_event(&g);g.intro_time=0;g.wave=wave-1;g.wave_timer=.001f;
        g.enemy_count=g.reinforcements=0;g.wave_spawned=g.wave_total;g.player.upgrade_pending=false;
        game_update(&g,(Input){0},FIXED_STEP);
        if(g.intro_time>0){assert(wave-last>=15);last=wave;introductions++;}
    }
    assert(introductions>0&&introductions<=13);
    game_reset(&g,w,64);g.wave=19;g.wave_timer=.001f;game_update(&g,(Input){0},FIXED_STEP);
    for(int i=0;i<g.enemy_count;i++)assert(g.enemies[i].type!=ENEMY_WEAVER);
    game_reset(&g,w,64);g.wave=20;g.wave_timer=.001f;game_update(&g,(Input){0},FIXED_STEP);
    int count=0;for(int i=0;i<g.enemy_count;i++)if(g.enemies[i].type==ENEMY_WEAVER)count++;
    assert(g.wave==21&&count>0&&PRESETS[4].width==1920&&PRESETS[4].height==1080);
    Body airborne={{27,20,23},{40,5,40},false};
    move_horizontal(w,&airborne,10,10,PLAYER_RADIUS,PLAYER_HEIGHT,0);
    assert(airborne.position.x<=ARENA_HALF_X-PLAYER_RADIUS&&airborne.position.z<=ARENA_HALF_Z-PLAYER_RADIUS);
    puts("OK : introduction initiale, gel du combat, passage volontaire, rarete et vague 21");
}
static void coop_intro_test(World *w) {
    Game g;game_reset(&g,w,64);game_start_coop(&g,(bool[MAX_PLAYERS]){true,true,false,false},0);
    g.wave=4;g.wave_timer=.001f;Input inputs[MAX_PLAYERS]={0};game_update_coop(&g,inputs,FIXED_STEP);
    assert(g.intro_time==CHAMPION_INTRO_TIME);float time=g.time;
    Vec3 a=g.player.body.position,b=g.actors[1].player.body.position;
    inputs[0]=(Input){.forward=1,.firing=true};inputs[1]=(Input){.right=1,.firing=true};
    for(int i=0;i<40;i++)game_update_coop(&g,inputs,FIXED_STEP);
    assert(g.intro_time>0&&g.time==time&&vlength(vsub(a,g.player.body.position))==0&&vlength(vsub(b,g.actors[1].player.body.position))==0);
    inputs[1].jump=true;game_update_coop(&g,inputs,FIXED_STEP);assert(g.intro_time==0);
    puts("OK : introduction cooperative gelee pour tous, passage par un invite");
}
static void new_protocol_tests(World *w) {
    Game g;game_reset(&g,w,64);game_start_coop(&g,(bool[MAX_PLAYERS]){true,true,false,false},0);
    g.wave=21;g.map_id=1;world_set_map(w,1);g.player.body.position=w->decks[0].pickup;g.player.body.position.x=-3.14159f;
    g.actors[1].player.web_timer=.5f;g.actors[1].player.web_immunity=3;
    g.enemy_count=1;g.enemies[0]=inert((Vec3){4,0,12});g.enemies[0].type=ENEMY_WEAVER;
    g.enemies[0].climb_timer=.25f;g.enemies[0].climb_solid=0;g.enemies[0].climb_start=(Vec3){4,0,12};g.enemies[0].climb_target=(Vec3){1,.9f,1};
    g.particle_count=MAX_PARTICLES;for(int i=0;i<MAX_PARTICLES;i++)g.particles[i]=(Particle){.life=1,.max_life=1,.size=.1f};
    game_actor_save(&g);uint8_t data[NET_PACKET_MAX];size_t len=protocol_snapshot_write(data,sizeof(data),&g);assert(len>0&&len<4096);
    World other;assert(world_init(&other));Game copy;game_reset(&copy,&other,64);copy.local_actor=1;
    assert(protocol_snapshot_read(&copy,data,len)&&copy.player.web_timer==.5f);
    assert(copy.enemies[0].type==ENEMY_WEAVER&&copy.enemies[0].climb_timer==.25f);
    assert(copy.particle_count==NET_VISUAL_PARTICLES&&!copy.actors[2].active);
    assert(fabsf(copy.actors[0].player.body.position.x+3.14159f)<.004f);
    assert(copy.actors[0].player.body.position.y==g.player.body.position.y);
    game_actor_select(&copy,0);float floor_y=copy.player.body.position.y;game_predict(&copy,(Input){0},FIXED_STEP);
    assert(copy.player.body.grounded&&fabsf(copy.player.body.position.y-floor_y)<.001f);
    assert(!protocol_snapshot_read(&copy,data,len-1));
    g.map_id=0;g.wave=5;world_set_map(w,0);g.enemies[0].type=ENEMY_BRUTE;g.enemies[0].elite=true;g.enemies[0].id=77;
    g.enemies[0].climb_timer=0;g.intro_time=2;g.intro_enemy=77;g.intro_serial=1;g.intro_last_wave=5;
    len=protocol_snapshot_write(data,sizeof(data),&g);assert(protocol_snapshot_read(&copy,data,len)&&copy.intro_time==2&&copy.intro_enemy==77);
    g.intro_enemy=98;len=protocol_snapshot_write(data,sizeof(data),&g);assert(!protocol_snapshot_read(&copy,data,len));
    mesh_free(&other.mesh);printf("OK : protocole 12, toile, escalade, intro, precision et particules bornees (%zu octets)\n",len);
}

int main(void) {
    assert(SDL_Init(SDL_INIT_TIMER)==0);srand(1);World w;assert(world_init(&w));
    revive_tests(&w);rocket_tests(&w);champion_tests(&w);ai_tests(&w);platform_tests(&w);snapshot_tests(&w);
    bomber_timing_test(&w);web_tests(&w);climbing_tests(&w);finisher_push_tests(&w);intro_and_weaver_tests(&w);coop_intro_test(&w);new_protocol_tests(&w);
    mesh_free(&w.mesh);SDL_Quit();puts("OK : reanimations, evenements, degats de zone, rocket jump, champions, IA, plateformes et protocole");return 0;
}
