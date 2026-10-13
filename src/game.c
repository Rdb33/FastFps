#include "fastfps.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static float random01(void) { return (float)rand() / ((float)RAND_MAX + 1.0f); }
static float enemy_scale(const Enemy *e) { return enemy_size(e); }
static bool actor_alive(const Game *g,int id);
static Player *actor_player(Game *g,int id);

static void say(Game *g, const char *text, float duration) {
    snprintf(g->message, sizeof(g->message), "%s", text);
    g->message_time = duration;
}

void game_reset(Game *g, World *world, int limit) {
    memset(g, 0, sizeof(*g));
    g->world = world;
    g->particle_limit = limit;
    g->motion = true;
    g->difficulty=1;
    g->shot_age = 10;
    g->player.body = (Body){{0, 0, 16}, {0, 0, 0}, true};
    g->player.health = 100;
    g->wave_timer = 1.5f;
    g->pickups[0].position = (Vec3){-23, 0, -18};
    g->pickups[1].position = (Vec3){23, 0, -18};
    g->pickups[2].position = (Vec3){-23, 0, 18};
    g->pickups[3].position = (Vec3){23, 0, 18};
    g->pickup_count = 4;
    const Vec3 barrels[MAX_BARRELS]={{-6,0,-15},{-2,0,-15},{-20,0,2},{-20,0,6},{18,0,5},{18,0,9}};
    g->barrel_count=MAX_BARRELS;
    for(int i=0;i<MAX_BARRELS;i++)g->barrels[i]=(Barrel){.position=barrels[i],.health=45,.owner=-1};
    say(g, "BOUGE - CLIC DROIT : ESQUIVE - F : FRAPPE", 3);
    g->actors[0].active=true;
    snprintf(g->actors[0].name,sizeof(g->actors[0].name),"VOUS");
    game_actor_save(g);
    navigation_update(world, 0, 16);
}

static void start_wave(Game *g) {
    g->wave++;
    int players=0;for(int i=0;i<MAX_PLAYERS;i++)if(actor_alive(g,i))players++;
    int count = 2 + g->wave * 2 + (players-1)*2 + (g->difficulty-1)*2;
    if(count<2)count=2;
    if (count > MAX_ENEMIES) count = MAX_ENEMIES;
    int choices[8], n = 0;
    for (int i = 0; i < 8; i++) {
        bool safe=true;
        for(int id=0;id<MAX_PLAYERS;id++)if(actor_alive(g,id)) {
            Vec3 delta=vsub(g->world->spawns[i],actor_player(g,id)->body.position);
            if(hypotf(delta.x,delta.z)<9)safe=false;
        }
        if(safe)choices[n++]=i;
    }
    if (n == 0) for (int i = 0; i < 8; i++) choices[n++] = i;
    for (int i = n - 1; i > 0; i--) {
        int j = rand() % (i + 1), temp = choices[i];
        choices[i] = choices[j]; choices[j] = temp;
    }
    for (int i = 0; i < count; i++) {
        Enemy *e = &g->enemies[g->enemy_count++];
        memset(e, 0, sizeof(*e));
        e->body.position = g->world->spawns[choices[i % n]];
        float offset = (float)(i / n) * 1.5f;
        e->body.position.x += e->body.position.x > 0 ? -offset : offset;
        e->body.grounded = true;
        e->id = g->next_id++;
        e->type = i % 3 == 0 ? ENEMY_CASTER : ENEMY_RUNNER;
        if (g->wave >= 3 && i == count - 1) e->type = ENEMY_BRUTE;
        if (g->wave >= 6 && i == count - 3) e->type = ENEMY_BRUTE;
        e->elite=g->wave%5==0&&i==count-1;
        if(e->elite)e->type=ENEMY_BRUTE;
        e->max_health = e->type == ENEMY_BRUTE ? 260 : (e->type == ENEMY_CASTER ? 120 : 80);
        e->max_health*=(1+.15f*(float)(players-1))*(g->difficulty==0?.8f:(g->difficulty==2?1.25f:1));
        e->max_health*=(1+fminf(2,(float)(g->wave-1)*.035f))*(e->elite?2.5f:1);e->health = e->max_health;
        e->shot_timer = 1.5f + random01() * 1.4f;
        e->spawn_time = .5f;
        Vec3 delta = vsub(g->player.body.position, e->body.position);
        e->yaw = atan2f(delta.x, -delta.z);
    }
    char message[80];
    if(g->wave%5==0)snprintf(message,sizeof(message),"VAGUE %02d - CHAMPION DU SECTEUR %02d",g->wave,(g->wave-1)/5+1);
    else if(g->wave==1)snprintf(message,sizeof(message),"VAGUE 01 - VISE LA TETE / BARILS ROUGES EXPLOSIFS");
    else if(g->wave==2)snprintf(message,sizeof(message),"VAGUE 02 - F : FRAPPE FATALE POUR RECUPERER DE LA SANTE");
    else snprintf(message, sizeof(message), g->wave == 3 ? "VAGUE %02d - ATTENTION AUX BRUTES" : "VAGUE %02d - %d CREATURES", g->wave, count);
    say(g, message, 2.5f);
    g->wave_timer = -1;
    for(int i=0;i<g->barrel_count;i++)if(g->barrels[i].health<=0) {
        bool safe=true;
        for(int id=0;id<MAX_PLAYERS;id++)if(actor_alive(g,id)
            &&vlength(vsub(actor_player(g,id)->body.position,g->barrels[i].position))<2)safe=false;
        if(safe){g->barrels[i].health=45;g->barrels[i].explosion_time=0;g->barrels[i].owner=-1;}
    }
    audio_play(g->wave%5==0?SOUND_CHAMPION:SOUND_WAVE);
}

void game_burst(Game *g, Vec3 p, Color c, int count) {
    for (int i = 0; i < count && g->particle_count < g->particle_limit; i++) {
        float life = .16f + random01() * .25f;
        g->particles[g->particle_count++] = (Particle){p,
            {(random01() - .5f) * 5, random01() * 4, (random01() - .5f) * 5},
            c, life, life, .06f + random01() * .06f};
    }
}

static Player *actor_player(Game *g,int id) {
    return id==g->current_actor?&g->player:&g->actors[id].player;
}
static bool actor_alive(const Game *g,int id) {
    if(!g->actors[id].active)return false;
    return id==g->current_actor?!g->dead:!g->actors[id].dead;
}
static int nearest_actor(Game *g,Vec3 origin) {
    int best=-1;float distance=1e9f;
    for(int i=0;i<MAX_PLAYERS;i++)if(actor_alive(g,i)) {
        float d=vlength(vsub(actor_player(g,i)->body.position,origin));
        if(d<distance){distance=d;best=i;}
    }
    return best;
}
static void hurt_actor(Game *g,int id,float damage,Vec3 source) {
    if(id<0||id>=MAX_PLAYERS||!actor_alive(g,id)||g->actors[id].invulnerable>0)return;
    damage*=g->difficulty==0?.65f:(g->difficulty==2?1.35f:1);
    Player *p=actor_player(g,id);damage*=1-.06f*(float)p->upgrades[UPGRADE_ARMOR];
    p->health=fmaxf(0,p->health-damage);
    Vec3 direction=vsub(source,p->body.position);float yaw=atan2f(direction.x,-direction.z);
    g->actors[id].damage_yaw=yaw;if(id==g->current_actor)g->damage_yaw=yaw;
    if(id==g->current_actor){g->damage_time=.32f;g->dead=p->health==0;}
    else {g->actors[id].damage_time=.32f;g->actors[id].dead=p->health==0;}
    if(p->health==0)g->actors[id].respawn_timer=5;
    if(id==g->local_actor)audio_play(SOUND_HURT);
}

static void add_corpse(Game *g, const Enemy *e) {
    int slot = g->corpse_count;
    if (slot == MAX_CORPSES) {
        slot = 0;
        for (int i = 1; i < MAX_CORPSES; i++)
            if (g->corpses[i].death_time > g->corpses[slot].death_time) slot = i;
    } else g->corpse_count++;
    g->corpses[slot] = *e;
    g->corpses[slot].death_time = .001f;
    g->corpses[slot].flash = 0;
    g->corpses[slot].attack_timer = 0;
    g->corpses[slot].body.velocity.x=e->knockback.x*.7f;
    g->corpses[slot].body.velocity.z=e->knockback.z*.7f;
}

static void damage_enemy(Game *g, Enemy *e, float damage, Vec3 dir) {
    if (e->health <= 0) return;
    e->health -= damage;
    e->flash = .12f;
    e->stagger = e->type == ENEMY_BRUTE ? .065f : .16f;
    e->knockback = vscale(dir, e->type == ENEMY_BRUTE ? 1.0f : 3.5f);
    e->knockback.y = 0;
    g->hit_time = .14f;
    if (e->health > 0) return;
    g->combo = g->combo_timer > 0 ? g->combo + 1 : 1;
    g->combo_timer = 3.5f;
    int multiplier = 1 + (g->combo - 1) / 3;
    if (multiplier > 4) multiplier = 4;
    int points=(100+g->wave*10)*multiplier*(e->elite?5:1);
    g->score += points;g->reward_points=points;g->reward_time=.85f;
    g->kills++;
    g->kill_time=.3f;
    g->actors[g->current_actor].run_kills++;
    if(!g->dead)g->player.health=fminf(player_max_health(&g->player),g->player.health+2*(float)g->player.upgrades[UPGRADE_VAMPIRE]);
    add_corpse(g, e);
    game_burst(g, vadd(e->body.position, (Vec3){0, .8f, 0}), (Color){.9f, .26f, .08f}, 8);
    audio_play_at(SOUND_KILL,e->body.position);
}

static void explode_barrel(Game *g,int index,int owner) {
    Barrel *barrel=&g->barrels[index];
    if(barrel->health<=0)return;
    barrel->health=0;barrel->explosion_time=.65f;barrel->owner=owner;
    Vec3 origin=vadd(barrel->position,(Vec3){0,.65f,0});
    game_burst(g,origin,(Color){1,.52f,.08f},12);audio_play_at(SOUND_EXPLOSION,origin);
    for(int i=0;i<g->enemy_count;i++) {
        Enemy *e=&g->enemies[i];if(e->health<=0)continue;
        Vec3 delta=vsub(vadd(e->body.position,(Vec3){0,.8f,0}),origin);
        float distance=vlength(delta);Vec3 direction=vnormalize(delta);
        if(distance<6&&world_ray(g->world,origin,direction,distance)>=distance-.05f)
            damage_enemy(g,e,200*(1-distance/6),direction);
    }
    /* Les explosions sont un danger de l'arène pour toute l'équipe. */
    for(int id=0;id<MAX_PLAYERS;id++)if(actor_alive(g,id)) {
        Vec3 delta=vsub(player_eye(actor_player(g,id),false),origin);
        float distance=vlength(delta);
        if(distance<6&&world_ray(g->world,origin,vnormalize(delta),distance)>=distance-.05f)
            hurt_actor(g,id,40*(1-distance/6),origin);
    }
    for(int i=0;i<g->barrel_count;i++)if(g->barrels[i].health>0) {
        Vec3 target=vadd(g->barrels[i].position,(Vec3){0,.65f,0}),delta=vsub(target,origin);
        float distance=vlength(delta);
        if(distance<5&&world_ray(g->world,origin,vnormalize(delta),distance)>=distance-.05f)
            explode_barrel(g,i,owner);
    }
}

static bool fire_ray(Game *g, Vec3 origin, Vec3 dir, float damage, bool trail) {
    float distance = world_ray(g->world, origin, dir, SHOT_RANGE);
    Enemy *victim = NULL;
    int barrel=-1;
    for (int i = 0; i < g->enemy_count; i++) {
        Enemy *e = &g->enemies[i];
        if (e->health <= 0) continue;
        Vec3 p = e->body.position;
        float scale = enemy_scale(e), r = .48f * scale;
        Box bounds = {{p.x-r, p.y, p.z-r}, {p.x+r, p.y+ENEMY_HEIGHT*scale, p.z+r}};
        float hit = ray_box(origin, dir, bounds, distance);
        if (hit < distance) { distance = hit; victim = e; }
    }
    for(int i=0;i<g->barrel_count;i++)if(g->barrels[i].health>0) {
        Vec3 p=g->barrels[i].position;
        float hit=ray_box(origin,dir,(Box){{p.x-.43f,p.y,p.z-.43f},{p.x+.43f,p.y+1.1f,p.z+.43f}},distance);
        if(hit<distance){distance=hit;barrel=i;victim=NULL;}
    }
    Vec3 point = vadd(origin, vscale(dir, distance));
    game_burst(g, point, victim ? (Color){.95f,.15f,.05f} : (Color){1,.7f,.24f}, victim ? 3 : 1);
    if (trail) for (int i=1; i<=4 && g->particle_count<g->particle_limit; i++) {
        Vec3 p = vadd(origin, vscale(dir, distance*(float)i/5));
        g->particles[g->particle_count++] = (Particle){p,{0,0,0},{1,.8f,.4f},.055f,.055f,.022f};
    }
    if (victim) {
        bool headshot=point.y>victim->body.position.y+1.22f*enemy_scale(victim);
        damage_enemy(g,victim,damage*(headshot?2:1),dir);
        if(headshot){g->headshot_time=.28f;g->actors[g->current_actor].run_headshots++;if(victim->health<=0){g->score+=50;g->reward_points+=50;}}
    }
    if(barrel>=0) {
        Barrel *b=&g->barrels[barrel];
        if(b->health<=damage)explode_barrel(g,barrel,g->current_actor);
        else b->health-=damage;
        g->hit_time=.14f;
    }
    return victim != NULL||barrel>=0;
}

void game_shoot(Game *g) {
    Player *p = &g->player;
    if (p->weapon_switch > 0 || p->melee_timer > 0) return;
    Vec3 origin = player_eye(p, g->motion);
    bool hit = false;
    if(g->prediction) {
        g->shot_cooldown=player_fire_interval(p);
        g->recoil=1;g->shot_age=0;
        audio_play(p->weapon==0?SOUND_SHOT:SOUND_SHOTGUN);
        return;
    }
    g->actors[g->current_actor].shots++;
    if (p->weapon == 0) {
        hit = fire_ray(g, origin, look_direction(p->yaw, p->pitch), 34*player_damage_multiplier(p), true);
        g->shot_cooldown = player_fire_interval(p);
        audio_play(SOUND_SHOT);
    } else {
        hit = fire_ray(g, origin, look_direction(p->yaw, p->pitch), 18*player_damage_multiplier(p), true);
        for (int i = 0; i < 8; i++) {
            float angle = (float)i * PI_F / 4;
            hit |= fire_ray(g, origin, look_direction(p->yaw+cosf(angle)*.052f, p->pitch+sinf(angle)*.04f), 18*player_damage_multiplier(p), false);
        }
        g->shot_cooldown = player_fire_interval(p);
        audio_play(SOUND_SHOTGUN);
    }
    if (hit) audio_play(g->headshot_time==.28f?SOUND_HEADSHOT:SOUND_HIT);
    g->recoil = 1;
    g->shot_age = 0;
    if(g->particle_count<g->particle_limit) {
        Vec3 right={cosf(p->yaw),0,sinf(p->yaw)};
        Vec3 shell=vadd(origin,vadd(vscale(look_direction(p->yaw,p->pitch),.5f),vscale(right,.35f)));
        g->particles[g->particle_count++]=(Particle){shell,vadd(vscale(right,3),(Vec3){0,2,0}),{.85f,.66f,.24f},.55f,.55f,.045f};
    }
}

void game_request_weapon(Game *g, int weapon) {
    Player *p = &g->player;
    if (weapon < 0 || weapon > 1 || weapon == p->weapon || p->weapon_switch > 0) return;
    p->weapon_target = weapon;
    p->weapon_switch = WEAPON_SWITCH_TIME;
    audio_play(SOUND_SWITCH);
}

void game_melee(Game *g) {
    Player *p = &g->player;
    if (p->melee_timer > 0 || p->weapon_switch > 0) return;
    p->melee_timer = MELEE_COOLDOWN;
    if(g->prediction){audio_play(SOUND_MELEE);return;}
    Vec3 origin = player_eye(p, g->motion), forward = look_direction(p->yaw, p->pitch);
    Enemy *victim = NULL;
    float nearest = MELEE_RANGE;
    for (int i = 0; i < g->enemy_count; i++) {
        Enemy *e = &g->enemies[i];
        if (e->health <= 0) continue;
        Vec3 target = vadd(e->body.position, (Vec3){0, .95f*enemy_scale(e), 0});
        Vec3 delta = vsub(target, origin), dir = vnormalize(delta);
        float distance = vlength(delta), dot = dir.x*forward.x + dir.y*forward.y + dir.z*forward.z;
        if (distance < nearest && dot > .62f && world_ray(g->world, origin, dir, distance) >= distance-.01f) {
            victim = e; nearest = distance;
        }
    }
    audio_play(SOUND_MELEE);
    if (victim) {
        damage_enemy(g, victim, 95*player_damage_multiplier(p), forward);
        game_burst(g, vadd(victim->body.position,(Vec3){0,.9f,0}), (Color){1,.5f,.12f}, 5);
        if (victim->health <= 0) {
            p->health = fminf(player_max_health(p), p->health+10);
            say(g, "FRAPPE FATALE - +10 SANTE", 1.2f);
        } else audio_play(SOUND_HIT);
    }
}

static void begin_attack(Enemy *e, Vec3 direction, int target) {
    e->attack_duration = e->type == ENEMY_BRUTE ? .95f : (e->type == ENEMY_CASTER ? .75f : .62f);
    if(e->elite)e->attack_duration=1.25f;
    e->attack_timer = e->attack_duration;
    e->attack_direction = direction;
    e->attack_fired = false;
    e->target_actor=target;
    audio_play_at(e->type == ENEMY_CASTER ? SOUND_CAST : SOUND_GROWL,e->body.position);
}

static void attack_update(Game *g, Enemy *e, float dt) {
    e->attack_timer = fmaxf(0, e->attack_timer-dt);
    float progress = 1-e->attack_timer/e->attack_duration;
    if (progress < .62f || e->attack_fired) return;
    e->attack_fired = true;
    if (e->type == ENEMY_CASTER) {
        if (g->projectile_count < MAX_PROJECTILES) {
            Vec3 origin = vadd(e->body.position,(Vec3){0,1.3f,0});
            g->projectiles[g->projectile_count++] = (Projectile){origin,vscale(e->attack_direction,12),5,{.3f,1,.78f}};
        }
        e->shot_timer = fmaxf(.9f,2.5f-(float)g->wave*.06f)+random01()*.5f;
    } else {
        Vec3 origin = vadd(e->body.position,(Vec3){0,1.0f*enemy_scale(e),0});
        if(e->elite) {
            for(int id=0;id<MAX_PLAYERS;id++)if(actor_alive(g,id)) {
                Vec3 delta=vsub(player_eye(actor_player(g,id),g->motion),origin);float distance=vlength(delta);
                if(distance<4.5f&&world_ray(g->world,origin,vnormalize(delta),distance)>=distance-.01f)
                    hurt_actor(g,id,32,origin);
            }
            e->touch_timer=1.6f;game_burst(g,origin,(Color){1,.68f,.2f},10);audio_play_at(SOUND_EXPLOSION,origin);return;
        }
        if(!actor_alive(g,e->target_actor))return;
        Vec3 delta = vsub(player_eye(actor_player(g,e->target_actor),g->motion),origin), dir = vnormalize(delta);
        float distance = vlength(delta), range = e->type == ENEMY_BRUTE ? 2.7f : 1.9f;
        float dot = dir.x*e->attack_direction.x+dir.y*e->attack_direction.y+dir.z*e->attack_direction.z;
        if (distance < range && dot > .45f && world_ray(g->world,origin,dir,distance) >= distance-.01f)
            hurt_actor(g,e->target_actor,e->type == ENEMY_BRUTE ? 28 : 15,origin);
        e->touch_timer = e->type == ENEMY_BRUTE ? 1.0f : .6f;
    }
}

static void update_enemies(Game *g, float dt) {
    for (int i = 0; i < g->enemy_count; i++) {
        Enemy *e = &g->enemies[i];
        if (e->health <= 0) continue;
        e->flash = fmaxf(0,e->flash-dt);
        e->stagger = fmaxf(0,e->stagger-dt);
        e->spawn_time = fmaxf(0,e->spawn_time-dt);
        e->shot_timer -= dt;
        e->touch_timer = fmaxf(0,e->touch_timer-dt);
        int target_id=nearest_actor(g,e->body.position);
        if(target_id<0)continue;
        Vec3 p = player_eye(actor_player(g,target_id),g->motion), origin = vadd(e->body.position,(Vec3){0,1.3f*enemy_scale(e),0});
        Vec3 delta = vsub(p,origin), dir = vnormalize(delta);
        float distance = vlength(delta), horizontal = hypotf(delta.x,delta.z);
        bool visible = world_ray(g->world,origin,dir,distance) >= distance-.05f;
        Vec3 target = visible ? p : navigation_next(g->world,e->body.position.x,e->body.position.z);
        Vec3 move = vsub(target,e->body.position); move.y = 0;
        if (e->type == ENEMY_CASTER && visible && horizontal<8 && horizontal>2) {
            float sign = e->id%2 ? 1 : -1; move = (Vec3){-delta.z*sign,0,delta.x*sign};
        }
        if (e->attack_timer == 0 && e->spawn_time == 0 && e->stagger == 0 && visible) {
            if (e->type == ENEMY_CASTER && distance<36 && e->shot_timer<=0) begin_attack(e,dir,target_id);
            else if (e->type != ENEMY_CASTER && horizontal<(e->elite?4.2f:(e->type == ENEMY_BRUTE ? 2.5f : 1.6f))
                && fabsf(delta.y)<1.7f && e->touch_timer==0) begin_attack(e,dir,target_id);
        }
        float desired_yaw = atan2f(delta.x,-delta.z);
        if (e->attack_timer==0) {
            float turn = atan2f(sinf(desired_yaw-e->yaw),cosf(desired_yaw-e->yaw));
            e->yaw += turn*fminf(1,dt*8);
        }
        float speed = e->type == ENEMY_BRUTE ? 2.4f : (e->type == ENEMY_CASTER ? 2.8f : 4.4f);
        speed += fminf(1.4f,(float)(g->wave-1)*.1f);
        speed*=g->difficulty==0?.9f:(g->difficulty==2?1.12f:1);
        if (e->attack_timer>0 || e->stagger>0 || e->spawn_time>0) speed = 0;
        Vec3 desired = vadd(vscale(vnormalize(move),speed),e->knockback);
        e->knockback = vscale(e->knockback,fmaxf(0,1-dt*14));
        e->body.velocity.x = desired.x; e->body.velocity.z = desired.z;
        float scale = enemy_scale(e);
        move_horizontal(g->world,&e->body,desired.x*dt,desired.z*dt,ENEMY_RADIUS*scale,ENEMY_HEIGHT*scale,PLAYER_STEP);
        move_vertical(g->world,&e->body,dt,ENEMY_RADIUS*scale,ENEMY_HEIGHT*scale);
        float moving = hypotf(e->body.velocity.x,e->body.velocity.z);
        e->walk_phase += moving*dt*(e->type == ENEMY_BRUTE ? 1.8f : 2.8f);
        e->movement_blend += (clampf(moving/4,0,1)-e->movement_blend)*fminf(1,dt*12);
        if (e->attack_timer>0 && e->stagger==0) attack_update(g,e,dt);
    }
    for (int i=0;i<g->enemy_count;i++) for (int j=i+1;j<g->enemy_count;j++) {
        Enemy *a=&g->enemies[i], *b=&g->enemies[j];
        if (a->health<=0 || b->health<=0) continue;
        Vec3 delta=vsub(a->body.position,b->body.position); delta.y=0;
        float distance=vlength(delta), separation=ENEMY_RADIUS*(enemy_scale(a)+enemy_scale(b));
        if (distance>=separation || fabsf(a->body.position.y-b->body.position.y)>1) continue;
        if (distance<.001f) { delta=(Vec3){1,0,0}; distance=1; }
        Vec3 push=vscale(delta,fminf(.07f,(separation-distance)*.5f)/distance);
        move_horizontal(g->world,&a->body,push.x,push.z,ENEMY_RADIUS*enemy_scale(a),ENEMY_HEIGHT*enemy_scale(a),0);
        move_horizontal(g->world,&b->body,-push.x,-push.z,ENEMY_RADIUS*enemy_scale(b),ENEMY_HEIGHT*enemy_scale(b),0);
    }
    for (int i=0;i<g->enemy_count;)
        if (g->enemies[i].health<=0) g->enemies[i]=g->enemies[--g->enemy_count]; else i++;
}

static void update_projectiles(Game *g, float dt) {
    for (int i=0;i<g->projectile_count;) {
        Projectile *b=&g->projectiles[i]; b->life-=dt;
        float length=vlength(b->velocity)*dt;
        Vec3 dir=vnormalize(b->velocity);
        float wall=world_ray(g->world,b->position,dir,length),nearest=wall;
        int victim=-1;
        for(int id=0;id<MAX_PLAYERS;id++)if(actor_alive(g,id)) {
            Vec3 p=actor_player(g,id)->body.position;
            Box bounds={{p.x-PLAYER_RADIUS,p.y,p.z-PLAYER_RADIUS},{p.x+PLAYER_RADIUS,p.y+PLAYER_HEIGHT,p.z+PLAYER_RADIUS}};
            float hit=ray_box(b->position,dir,bounds,length);
            if(hit<=nearest){nearest=hit;victim=id;}
        }
        if(victim>=0){hurt_actor(g,victim,12,b->position);b->life=0;}
        else if(wall<length){game_burst(g,vadd(b->position,vscale(dir,wall)),(Color){1,.4f,.1f},2);b->life=0;}
        else b->position=vadd(b->position,vscale(b->velocity,dt));
        if(b->life<=0)g->projectiles[i]=g->projectiles[--g->projectile_count];else i++;
    }
}

static void update_actor(Game *g,Input input,float dt) {
    if(input.upgrade)game_choose_upgrade(g,input.upgrade);
    if(g->dead)return;
    if(g->player.upgrade_pending){input.firing=false;input.melee=false;}
    g->hit_time=fmaxf(0,g->hit_time-dt); g->damage_time=fmaxf(0,g->damage_time-dt);
    g->headshot_time=fmaxf(0,g->headshot_time-dt);g->kill_time=fmaxf(0,g->kill_time-dt);
    g->reward_time=fmaxf(0,g->reward_time-dt);
    g->player.upgrade_flash=fmaxf(0,g->player.upgrade_flash-dt);
    g->recoil=fmaxf(0,g->recoil-dt*9);
    g->shot_cooldown=fmaxf(0,g->shot_cooldown-dt);
    g->combo_timer=fmaxf(0,g->combo_timer-dt);
    if (g->combo_timer==0) g->combo=0;
    float previous_age=g->shot_age; g->shot_age+=dt;
    if (g->player.weapon==1 && previous_age<.19f && g->shot_age>=.19f) audio_play(SOUND_PUMP);
    Player *p=&g->player;
    p->melee_timer=fmaxf(0,p->melee_timer-dt);
    float previous_switch=p->weapon_switch;
    p->weapon_switch=fmaxf(0,p->weapon_switch-dt);
    if (previous_switch>WEAPON_SWITCH_TIME/2 && p->weapon_switch<=WEAPON_SWITCH_TIME/2) {
        p->weapon=p->weapon_target;
        g->shot_age=10;g->recoil=0;
    }
    bool dash_held=input.dash, melee_held=input.melee;
    input.dash=dash_held&&!g->dash_pressed;
    g->dash_pressed=dash_held;
    update_player(g->world,p,input,dt);

    if (melee_held&&!g->melee_pressed) game_melee(g);
    g->melee_pressed=melee_held;
    if (input.firing && g->shot_cooldown==0) game_shoot(g);
    if(!g->prediction) {
    for (int i=0;i<g->pickup_count;i++) {
        Pickup *pickup=&g->pickups[i];
        Vec3 delta=vsub(pickup->position,p->body.position);
        if (pickup->cooldown==0 && p->health<player_max_health(p) && p->body.position.y<1.2f && hypotf(delta.x,delta.z)<1.3f) {
            p->health=fminf(player_max_health(p),p->health+30); pickup->cooldown=12;
            audio_play(SOUND_PICKUP); say(g,"+30 SANTE",1);
        }
    }
    }
}

static void update_world(Game *g,float dt) {
    g->time+=dt;g->tick++;
    g->message_time=fmaxf(0,g->message_time-dt);
    g->wave_clear_time=fmaxf(0,g->wave_clear_time-dt);
    g->nav_timer-=dt;
    if(g->nav_timer<=0) {
        int target=nearest_actor(g,(Vec3){0,0,0});
        if(target>=0) {
            Vec3 p=actor_player(g,target)->body.position;
            navigation_update(g->world,p.x,p.z);
        }
        g->nav_timer=.35f;
    }
    for(int i=0;i<g->pickup_count;i++)g->pickups[i].cooldown=fmaxf(0,g->pickups[i].cooldown-dt);
    for(int i=0;i<g->barrel_count;i++)g->barrels[i].explosion_time=fmaxf(0,g->barrels[i].explosion_time-dt);
    update_enemies(g,dt);update_projectiles(g,dt);
    for (int i=0;i<g->particle_count;) {
        Particle *particle=&g->particles[i]; particle->life-=dt;
        particle->position=vadd(particle->position,vscale(particle->velocity,dt));
        if (particle->velocity.x!=0 || particle->velocity.z!=0) particle->velocity.y-=9*dt;
        if (particle->life<=0) g->particles[i]=g->particles[--g->particle_count]; else i++;
    }
    for (int i=0;i<g->corpse_count;) {
        Enemy *corpse=&g->corpses[i]; corpse->death_time+=dt;
        move_horizontal(g->world,&corpse->body,corpse->body.velocity.x*dt,corpse->body.velocity.z*dt,.4f,.2f,0);
        corpse->body.velocity.x*=fmaxf(0,1-dt*7);
        corpse->body.velocity.z*=fmaxf(0,1-dt*7);
        move_vertical(g->world,&corpse->body,dt,.4f,.2f);
        if (corpse->death_time>4) g->corpses[i]=g->corpses[--g->corpse_count]; else i++;
    }
    int alive=0;
    for(int i=0;i<MAX_PLAYERS;i++)if(actor_alive(g,i))alive++;
    if(alive==0){if(g->cooperative)g->round_over=true;return;}
    if(g->enemy_count==0) {
        if(g->wave_timer<0) {
            g->wave_timer=INTERMISSION_TIME;g->score+=250;g->completed_waves=g->wave;g->wave_clear_time=2.8f;
            g->projectile_count=0;game_offer_upgrades(g);
            for(int i=0;i<MAX_PLAYERS;i++)if(actor_alive(g,i)) {
                Player *p=actor_player(g,i);p->health=fminf(player_max_health(p),p->health+15);
            }
            say(g,g->wave%5==0?"CHAMPION VAINCU - NOUVEAU SECTEUR DEBLOQUE":"VAGUE TERMINEE - MODULE DEBLOQUE / +15 SANTE",2.8f);
            audio_play(SOUND_UPGRADE);
        }
        bool pending=false;
        for(int i=0;i<MAX_PLAYERS;i++)if(g->actors[i].active&&actor_player(g,i)->upgrade_pending)pending=true;
        if(g->wave>0&&!pending&&g->wave_timer>3)g->wave_timer=3;
        g->wave_timer-=dt;
        if(g->wave_timer<=0) {
            int original=g->current_actor;
            for(int i=0;i<MAX_PLAYERS;i++)if(g->actors[i].active) {
                game_actor_select(g,i);if(g->player.upgrade_pending)game_choose_upgrade(g,1);game_actor_save(g);
            }
            game_actor_select(g,original);start_wave(g);
        }
    }
}

void game_actor_save(Game *g) {
    if(g->current_actor<0||g->current_actor>=MAX_PLAYERS)return;
    Actor *a=&g->actors[g->current_actor];
    a->player=g->player;a->shot_cooldown=g->shot_cooldown;a->recoil=g->recoil;
    a->hit_time=g->hit_time;a->damage_time=g->damage_time;a->shot_age=g->shot_age;
    a->combo_timer=g->combo_timer;a->combo=g->combo;a->dead=g->dead;
    a->dash_pressed=g->dash_pressed;a->melee_pressed=g->melee_pressed;
    a->headshot_time=g->headshot_time;a->kill_time=g->kill_time;a->damage_yaw=g->damage_yaw;
    a->reward_points=g->reward_points;a->reward_time=g->reward_time;
}
void game_actor_select(Game *g,int id) {
    if(id<0||id>=MAX_PLAYERS)return;
    game_actor_save(g);g->current_actor=id;
    const Actor *a=&g->actors[id];
    g->player=a->player;g->shot_cooldown=a->shot_cooldown;g->recoil=a->recoil;
    g->hit_time=a->hit_time;g->damage_time=a->damage_time;g->shot_age=a->shot_age;
    g->combo_timer=a->combo_timer;g->combo=a->combo;g->dead=a->dead;
    g->dash_pressed=a->dash_pressed;g->melee_pressed=a->melee_pressed;
    g->headshot_time=a->headshot_time;g->kill_time=a->kill_time;g->damage_yaw=a->damage_yaw;
    g->reward_points=a->reward_points;g->reward_time=a->reward_time;
}
void game_start_coop(Game *g,const bool active[MAX_PLAYERS],int local) {
    World *world=g->world;int limit=g->particle_limit,difficulty=g->difficulty;bool motion=g->motion;
    game_reset(g,world,limit);g->cooperative=true;g->motion=motion;g->local_actor=local;g->difficulty=difficulty;
    for(int i=0;i<MAX_PLAYERS;i++) {
        Actor *a=&g->actors[i];a->active=active[i];a->player=g->player;
        a->player.body.position=(Vec3){(float)i*1.7f,0,16+(float)(i%2)*2};
        a->shot_age=10;snprintf(a->name,sizeof(a->name),"JOUEUR %d",i+1);
    }
    g->current_actor=-1;game_actor_select(g,local);
}
static void respawn(Game *g,int id) {
    Actor *a=&g->actors[id];
    float best=-1;Vec3 position={0,0,16};
    for(int i=0;i<8;i++) {
        float nearest=100;
        for(int j=0;j<g->enemy_count;j++)nearest=fminf(nearest,vlength(vsub(g->world->spawns[i],g->enemies[j].body.position)));
        if(nearest>best){best=nearest;position=g->world->spawns[i];}
    }
    a->player.body=(Body){position,{0,0,0},true};a->player.health=player_max_health(&a->player);
    a->dead=false;a->invulnerable=2;a->respawn_timer=0;
    a->shot_cooldown=0;a->dash_pressed=false;a->melee_pressed=false;
}
void game_update_coop(Game *g,const Input inputs[MAX_PLAYERS],float dt) {
    if(g->round_over)return;
    game_actor_save(g);
    for(int i=0;i<MAX_PLAYERS;i++)if(g->actors[i].active) {
        game_actor_select(g,i);
        Actor *a=&g->actors[i];a->invulnerable=fmaxf(0,a->invulnerable-dt);
        if(a->dead){a->respawn_timer=fmaxf(0,a->respawn_timer-dt);if(a->respawn_timer==0){respawn(g,i);g->current_actor=-1;game_actor_select(g,i);}}
        audio_suppressed(i!=g->local_actor);
        update_actor(g,inputs[i],dt);
        audio_suppressed(false);
        if(i!=g->local_actor&&g->shot_age==0)audio_play_at(g->player.weapon?SOUND_SHOTGUN:SOUND_SHOT,g->player.body.position);
        game_actor_save(g);
    }
    game_actor_select(g,g->local_actor);
    audio_listener(player_eye(&g->player,g->motion),g->player.yaw);
    update_world(g,dt);game_actor_save(g);
}
void game_predict(Game *g,Input input,float dt) {
    g->prediction=true;
    update_actor(g,input,dt);
    g->prediction=false;game_actor_save(g);
}
void game_update(Game *g,Input input,float dt) {
    if(g->dead)return;
    audio_listener(player_eye(&g->player,g->motion),g->player.yaw);
    update_actor(g,input,dt);game_actor_save(g);
    update_world(g,dt);game_actor_save(g);
}
