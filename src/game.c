#include "fastfps.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static float random01(void) { return (float)rand() / ((float)RAND_MAX + 1.0f); }
static float enemy_scale(const Enemy *e) { return enemy_size(e); }
static bool actor_alive(const Game *g,int id);
static Player *actor_player(Game *g,int id);
static void update_revives(Game *g,float dt);

static void say(Game *g, const char *text, float duration) {
    snprintf(g->message, sizeof(g->message), "%s", text);
    g->message_time = duration;
}

void game_reset(Game *g, World *world, int limit) {
    memset(g, 0, sizeof(*g));
    g->world = world;world_set_map(world,0);g->map_id=0;g->lives=3;
    world_platforms_update(world,0);g->unlock_weapon=-1;g->event_last_wave=-5;g->event_rng=(uint32_t)rand();
    g->particle_limit = limit;
    g->motion = true;
    g->difficulty=1;
    g->shot_age = 10;
    g->player.body = (Body){{0, 0, 16}, {0, 0, 0}, true};
    g->player.health = 100;
    for(int i=0;i<WEAPON_COUNT;i++)g->player.ammo[i]=(uint8_t)WEAPONS[i].magazine;
    g->wave_timer = 1.5f;
    g->pickups[0].position = (Vec3){-23, 0, -18};
    g->pickups[1].position = (Vec3){23, 0, -18};
    g->pickups[2].position = (Vec3){-23, 0, 18};
    g->pickups[3].position = (Vec3){23, 0, 18};
    g->pickups[4].position = world->decks[0].pickup;
    g->pickups[5].position = world->decks[1].pickup;
    g->pickup_count = 6;
    encounters_map(g);
    const Vec3 barrels[MAX_BARRELS]={{-6,0,-15},{-2,0,-15},{-20,0,2},{-20,0,6},{18,0,5},{18,0,9}};
    g->barrel_count=MAX_BARRELS;
    for(int i=0;i<MAX_BARRELS;i++)g->barrels[i]=(Barrel){.position=barrels[i],.health=45,.owner=-1};
    say(g, "BOUGE - CLIC DROIT : ESQUIVE - F : FRAPPE", 3);
    g->actors[0].active=true;
    for(int id=0;id<MAX_PLAYERS;id++)g->actors[id].revive_target=-1;
    snprintf(g->actors[0].name,sizeof(g->actors[0].name),"VOUS");
    game_actor_save(g);
    navigation_update(world, 0, 16);navigation_actor(world,0,g->player.body.position);
}

/* Petits groupes espacés, sans dépasser le budget de 18 ennemis actifs. */
static void spawn_reinforcements(Game *g,int budget) {
    int players=0;for(int id=0;id<MAX_PLAYERS;id++)if(g->actors[id].active)players++;
    int front=3+g->wave*2+(g->difficulty-1)*2;
    if(front<2)front=2;
    if(front>MAX_ENEMIES)front=MAX_ENEMIES;
    while(budget-->0&&g->reinforcements>0&&g->enemy_count<MAX_ENEMIES) {
        Enemy next={0};int index=g->wave_spawned,cycle=index%MAX_ENEMIES;
        next.type=index%3==0?ENEMY_CASTER:ENEMY_RUNNER;
        if(g->wave>=3&&(index==front-1||cycle==17))next.type=ENEMY_BRUTE;
        if(g->wave>=6&&(index==front-3||cycle==15))next.type=ENEMY_BRUTE;
        if(g->wave>=11&&(index==front-2||index==front-4||index==front-5||cycle==16||cycle==14||cycle==13))next.type=ENEMY_BOMBER;
        next.elite=g->wave%5==0&&index==front-1;
        if(next.elite)next.type=ENEMY_BRUTE;
        float scale=enemy_size(&next),best=-1;Vec3 chosen={0,0,0};
        int offset=rand()%8;
        for(int k=0;k<8;k++)for(int slot=0;slot<9;slot++) {
            Vec3 q=g->world->spawns[(k+offset)%8];
            q.x+=(float)(slot%3-1)*2;q.z+=(float)(slot/3-1)*2;
            if(world_blocked(g->world,q,ENEMY_RADIUS*scale,ENEMY_HEIGHT*scale,-1))continue;
            float nearest=100;bool safe=true;
            for(int id=0;id<MAX_PLAYERS;id++)if(actor_alive(g,id)) {
                Vec3 d=vsub(q,actor_player(g,id)->body.position);float distance=hypotf(d.x,d.z);
                nearest=fminf(nearest,distance);if(distance<9)safe=false;
            }
            for(int j=0;j<g->enemy_count;j++) {
                Vec3 d=vsub(q,g->enemies[j].body.position);
                if(hypotf(d.x,d.z)<ENEMY_RADIUS*(scale+enemy_size(&g->enemies[j]))+.25f)safe=false;
            }
            if(safe&&nearest>best){best=nearest;chosen=q;}
        }
        if(best<0)break; /* Réessayer après déplacement des joueurs. */
        next.body=(Body){chosen,{0,0,0},true};next.id=g->next_id++;
        next.max_health=next.type==ENEMY_BRUTE?260:(next.type==ENEMY_BOMBER?220:(next.type==ENEMY_CASTER?120:80));
        next.max_health*=(1+.10f*(float)(players-1))*(g->difficulty==0?.8f:(g->difficulty==2?1.25f:1));
        next.max_health*=(1+fminf(3.25f,(float)(g->wave-1)*.055f))*(next.elite?2.5f:1);
        next.health=next.max_health;next.shot_timer=1.5f+random01()*1.4f;next.spawn_time=.65f;
        Vec3 d=vsub(g->player.body.position,chosen);next.yaw=atan2f(d.x,-d.z);
        g->enemies[g->enemy_count++]=next;g->wave_spawned++;g->reinforcements--;
    }
}

static void start_wave(Game *g) {
    g->wave++;
    int map=((g->wave-1)/5)%MAP_COUNT;
    if(map!=g->map_id) {
        world_set_map(g->world,map);world_platforms_update(g->world,g->time);g->map_id=map;
        g->corpse_count=g->particle_count=g->projectile_count=g->bomb_count=0;g->nav_timer=0;
        int original=g->current_actor;
        for(int i=0;i<MAX_PLAYERS;i++)if(g->actors[i].active) {
            game_actor_select(g,i);
            g->player.body=(Body){{(float)i*1.7f,0,19},{0,0,0},true};
            g->player.yaw=0;g->player.pitch=0;g->player.dash_timer=0;
            game_actor_save(g);g->actors[i].invulnerable=2;
            g->actors[i].revive_progress=0;g->actors[i].revive_held=false;g->actors[i].revive_target=-1;
        }
        game_actor_select(g,original);
        const Vec3 locations[MAP_COUNT][MAX_BARRELS]={
            {{-6,0,-15},{-2,0,-15},{-20,0,2},{-20,0,6},{18,0,5},{18,0,9}},
            {{-6,0,-18},{-2,0,-18},{-20,0,6},{-20,0,10},{20,0,6},{20,0,10}},
            {{-6,0,-18},{-2,0,-18},{-23,0,3},{-23,0,7},{23,0,3},{23,0,7}}};
        for(int i=0;i<g->barrel_count;i++)g->barrels[i]=(Barrel){.position=locations[map][i],.health=45,.owner=-1};
        for(int k=0;k<MAX_DECKS;k++)g->pickups[4+k].position=g->world->decks[k].pickup;
        encounters_map(g);
        for(int i=0;i<g->pickup_count;i++)g->pickups[i].cooldown=0;
        navigation_update(g->world,0,19);
    }
    int players=0;for(int i=0;i<MAX_PLAYERS;i++)if(g->actors[i].active)players++;
    int count=3+g->wave*2+(g->difficulty-1)*2;
    if(count<2)count=2;
    if(count>MAX_ENEMIES)count=MAX_ENEMIES;
    int late=(g->wave-7)/2;if(late<0)late=0;if(late>12)late=12;
    g->wave_total=count+late+(players-1)*(3+g->wave/3);
    if(g->wave_total>MAX_WAVE_ENEMIES)g->wave_total=MAX_WAVE_ENEMIES;
    g->wave_spawned=0;g->reinforcements=g->wave_total;g->reinforce_timer=2;
    spawn_reinforcements(g,count);
    encounters_start(g);
    char message[80];
    if(g->wave%5==0)snprintf(message,sizeof(message),"VAGUE %02d - CHAMPION DU SECTEUR %02d",g->wave,(g->wave-1)/5+1);
    else if(g->wave==1)snprintf(message,sizeof(message),"VAGUE 01 - VISE LA TETE / BARILS ROUGES EXPLOSIFS");
    else if(g->wave==2)snprintf(message,sizeof(message),"VAGUE 02 - F : FRAPPE FATALE POUR RECUPERER DE LA SANTE");
    else snprintf(message, sizeof(message), g->wave == 3 ? "VAGUE %02d - ATTENTION AUX BRUTES" : "VAGUE %02d - %d CREATURES", g->wave, g->wave_total);
    for(int i=1;i<STANDARD_WEAPONS;i++)if(g->wave==WEAPONS[i].unlock_wave) {
        snprintf(message,sizeof(message),"ARME DEBLOQUEE : %s / TOUCHE %d",WEAPONS[i].name,i+1);
        g->unlock_weapon=i;g->unlock_time=6;
    }
    game_roll_event(g);
    if(g->wave>1&&g->wave%5==1)snprintf(message,sizeof(message),"NOUVELLE MAP : %s / VAGUE %02d",world_map_name(g->map_id),g->wave);
    if(g->wave==11)snprintf(message,sizeof(message),"VAGUE 11 - BOMBARDIERS : QUITTE LES CERCLES ORANGE !");
    say(g, message, g->wave==11?6:3.5f);
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
    damage*=g->difficulty==0?.65f:(g->difficulty==2?1.45f:1.1f);
    damage*=1+game_threat(g)*(g->difficulty==0?.25f:.4f);
    Player *p=actor_player(g,id);damage*=1-.06f*(float)p->upgrades[UPGRADE_ARMOR];
    float absorbed=fminf(p->shield,damage);p->shield-=absorbed;damage-=absorbed;
    p->health=fmaxf(0,p->health-damage);
    Vec3 direction=vsub(source,p->body.position);float yaw=atan2f(direction.x,-direction.z);
    g->actors[id].damage_yaw=yaw;if(id==g->current_actor)g->damage_yaw=yaw;
    Actor *a=&g->actors[id];int slot=0;float oldest=2;
    for(int i=0;i<DAMAGE_DIRECTIONS;i++) {
        if(a->damage_timers[i]>0&&fabsf(atan2f(sinf(yaw-a->damage_angles[i]),cosf(yaw-a->damage_angles[i])))<.4f){slot=i;break;}
        if(a->damage_timers[i]<oldest){oldest=a->damage_timers[i];slot=i;}
    }
    a->damage_angles[slot]=yaw;a->damage_timers[slot]=1.15f;
    if(id==g->current_actor){g->damage_time=.32f;g->dead=p->health==0;}
    else {g->actors[id].damage_time=.32f;g->actors[id].dead=p->health==0;}
    if(p->health==0) {
        encounters_down(g);
        p->shield=p->shield_time=p->power_time=p->speed_time=p->reactor_time=0;
        p->dash_power_time=p->switch_power_time=0;
        g->actors[id].respawn_timer=g->cooperative?0:3;
        g->actors[id].revive_progress=0;g->actors[id].revive_target=-1;
        p->reload_timer=0;
        if(!g->cooperative){g->lives--;if(g->lives<=0)g->round_over=true;}
    }
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
    /* Toucher pendant la préparation annule une attaque légère, une seule fois.
       Les bombes déjà larguées et les sauts de champion restent dangereux. */
    if(e->attack_timer>0&&!e->attack_fired&&e->type!=ENEMY_BOMBER&&!e->elite) {
        e->attack_timer=0;e->attack_fired=true;
        e->shot_timer=fmaxf(e->shot_timer,.9f);e->touch_timer=fmaxf(e->touch_timer,.7f);
        encounters_interrupt(g);
    }
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
    g->kills++;encounters_kill(g,e->elite);
    g->kill_time=.3f;
    g->actors[g->current_actor].run_kills++;
    if(!g->dead)g->player.health=fminf(player_max_health(&g->player),g->player.health+(float)g->player.upgrades[UPGRADE_VAMPIRE]);
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
    float wall=world_ray(g->world,origin,dir,SHOT_RANGE),previous=0;
    Enemy *visited=NULL;bool any=false;
    int rank=g->player.upgrades[UPGRADE_PIERCE];
    for(int pass=0;pass<(rank?2:1);pass++) {
        float distance=wall;Enemy *victim=NULL;int barrel=-1;
        for(int i=0;i<g->enemy_count;i++) {
            Enemy *e=&g->enemies[i];if(e->health<=0||e==visited)continue;
            Vec3 p=e->body.position;float scale=enemy_scale(e),r=.48f*scale;
            float hit=ray_box(origin,dir,(Box){{p.x-r,p.y,p.z-r},{p.x+r,p.y+ENEMY_HEIGHT*scale,p.z+r}},distance);
            if(hit<distance&&hit>=previous){distance=hit;victim=e;}
        }
        for(int i=0;i<g->barrel_count;i++)if(g->barrels[i].health>0) {
            Vec3 p=g->barrels[i].position;
            float hit=ray_box(origin,dir,(Box){{p.x-.43f,p.y,p.z-.43f},{p.x+.43f,p.y+1.1f,p.z+.43f}},distance);
            if(hit<distance&&hit>=previous){distance=hit;barrel=i;victim=NULL;}
        }
        Vec3 point=vadd(origin,vscale(dir,distance));
        game_burst(g,point,victim?(Color){.95f,.15f,.05f}:(Color){1,.7f,.24f},victim?3:1);
        if(trail)for(int i=1;i<=4&&g->particle_count<g->particle_limit;i++) {
            Vec3 p=vadd(origin,vscale(dir,previous+(distance-previous)*(float)i/5));
            g->particles[g->particle_count++]=(Particle){p,{0,0,0},{1,.8f,.4f},.055f,.055f,.022f};
        }
        if(victim) {
            bool head=point.y>victim->body.position.y+1.22f*enemy_scale(victim);
            damage_enemy(g,victim,damage*(head?2:1),dir);any=true;
            if(head) {
                g->headshot_time=.28f;g->actors[g->current_actor].run_headshots++;
                if(victim->health<=0) {
                    g->score+=50;g->reward_points+=50;
                    Player *p=&g->player;int ammo=p->ammo[p->weapon]+p->upgrades[UPGRADE_PRECISION];
                    p->ammo[p->weapon]=(uint8_t)(ammo>WEAPONS[p->weapon].magazine?WEAPONS[p->weapon].magazine:ammo);
                }
            }
        }
        if(barrel>=0) {
            Barrel *b=&g->barrels[barrel];
            if(b->health<=damage)explode_barrel(g,barrel,g->current_actor);else b->health-=damage;
            g->hit_time=.14f;any=true;
        }
        if(!victim)break;
        visited=victim;previous=distance;damage*=.35f+.10f*(float)rank;
    }
    return any;
}

void game_shoot(Game *g) {
    Player *p = &g->player;
    if (g->dead||g->round_over||p->upgrade_pending||p->weapon_switch>0||p->melee_timer>0||p->reload_timer>0) return;
    if(!p->ammo[p->weapon]){game_reload(g);return;}
    if(p->weapon==WEAPON_ROCKET&&g->projectile_count>=MAX_PROJECTILES&&!g->prediction)return;
    p->ammo[p->weapon]--;
    Vec3 origin = player_eye(p, g->motion);
    bool hit = false;
    float power=player_damage_multiplier(p)*(1+(p->dash_power_time>0?.08f*(float)p->upgrades[UPGRADE_DASH_POWER]:0)
        +(p->switch_power_time>0?.10f*(float)p->upgrades[UPGRADE_SWITCH_POWER]:0)+(p->power_time>0?.65f:0));
    p->switch_power_time=0;p->power_time=0;
    if(g->prediction) {
        g->shot_cooldown=player_fire_interval(p);
        g->recoil=1;g->shot_age=0;
        audio_play(p->weapon==1?SOUND_SHOTGUN:SOUND_SHOT);
        if(!p->ammo[p->weapon])game_reload(g);
        return;
    }
    g->actors[g->current_actor].shots++;
    if (p->weapon == 0) {
        hit = fire_ray(g, origin, look_direction(p->yaw, p->pitch), 34*power, true);
        g->shot_cooldown = player_fire_interval(p);
        audio_play(SOUND_SHOT);
    } else if(p->weapon==1) {
        hit = fire_ray(g, origin, look_direction(p->yaw, p->pitch), 18*power, true);
        for (int i = 0; i < 8; i++) {
            float angle = (float)i * PI_F / 4;
            hit |= fire_ray(g, origin, look_direction(p->yaw+cosf(angle)*.052f, p->pitch+sinf(angle)*.04f), 18*power, false);
        }
        g->shot_cooldown = player_fire_interval(p);
        audio_play(SOUND_SHOTGUN);
    }
    if(p->weapon>=2&&p->weapon<STANDARD_WEAPONS) {
        float spread=p->weapon==2?.01f:0;
        hit=fire_ray(g,origin,look_direction(p->yaw+(random01()-.5f)*spread,p->pitch+(random01()-.5f)*spread),
            WEAPONS[p->weapon].damage*power,true);
        g->shot_cooldown=player_fire_interval(p);audio_play(SOUND_SHOT);
    }
    if(p->weapon==WEAPON_ROCKET) {
        Vec3 direction=look_direction(p->yaw,p->pitch);
        g->projectiles[g->projectile_count++]=(Projectile){.position=origin,.velocity=vscale(direction,30),
            .life=3,.color={1,.5f,.12f},.kind=1,.owner=g->current_actor,.damage=WEAPONS[WEAPON_ROCKET].damage*power};
        g->shot_cooldown=player_fire_interval(p);audio_play(SOUND_SHOTGUN);
    }
    if (hit) audio_play(g->headshot_time==.28f?SOUND_HEADSHOT:SOUND_HIT);
    g->recoil = 1;
    g->shot_age = 0;
    if(!p->ammo[p->weapon])game_reload(g);
    if(g->particle_count<g->particle_limit) {
        Vec3 right={cosf(p->yaw),0,sinf(p->yaw)};
        Vec3 shell=vadd(origin,vadd(vscale(look_direction(p->yaw,p->pitch),.5f),vscale(right,.35f)));
        g->particles[g->particle_count++]=(Particle){shell,vadd(vscale(right,3),(Vec3){0,2,0}),{.85f,.66f,.24f},.55f,.55f,.045f};
    }
}

void game_request_weapon(Game *g, int weapon) {
    Player *p = &g->player;
    if (!game_weapon_available(g,weapon) || weapon == p->weapon || p->weapon_switch > 0 || p->reload_timer > 0 || g->dead || g->round_over || p->upgrade_pending) return;
    p->weapon_target = weapon;
    p->weapon_switch = WEAPON_SWITCH_TIME;
    audio_play(SOUND_SWITCH);
}

void game_melee(Game *g) {
    Player *p = &g->player;
    if (p->melee_timer > 0 || p->weapon_switch > 0 || p->reload_timer>0) return;
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
    e->attack_timer = e->attack_duration;
    e->attack_direction = direction;
    e->attack_fired = false;
    e->target_actor=target;
    audio_play_at(e->type == ENEMY_CASTER ? SOUND_CAST : SOUND_GROWL,e->body.position);
}

/* La position visée reste fixe : l'esquive est possible durant toute la chute.
   L'hôte crée et résout les bombes ; les invités affichent leurs délais reçus. */
static void begin_bomb(Game *g,Enemy *e,int target) {
    g->actors[target].heavy_cooldown=BOMB_FALL_TIME+1.25f;
    Player *p=actor_player(g,target);
    Vec3 floor=vadd(p->body.position,(Vec3){0,.08f,0});
    floor.y=fmaxf(0,floor.y-world_ray(g->world,floor,(Vec3){0,-1,0},16));
    Vec3 above=vadd(floor,(Vec3){0,.15f,0});
    float height=fmaxf(.3f,world_ray(g->world,above,(Vec3){0,1,0},6)-.2f);
    g->bombs[g->bomb_count++]=(Bomb){.target=floor,.height=height,
        .time=BOMB_FALL_TIME,.source=e->id,.target_actor=target};
    e->attack_duration=e->attack_timer=BOMB_FALL_TIME;e->attack_fired=false;
    e->attack_direction=vnormalize(vsub(player_eye(p,false),e->body.position));
    e->target_actor=target;e->shot_timer=BOMB_FALL_TIME+BOMB_RELOAD_TIME;
    audio_play_at(SOUND_CAST,floor);
}

static void update_bombs(Game *g,float dt) {
    for(int i=0;i<g->bomb_count;) {
        Bomb *b=&g->bombs[i];b->time=fmaxf(0,b->time-dt);
        if(b->time>0){i++;continue;}
        Vec3 point=vadd(b->target,(Vec3){0,.25f,0});
        if(g->blast_count<MAX_BLASTS)g->blasts[g->blast_count++]=(Blast){b->target,.8f,BOMB_RADIUS};
        else g->blasts[0]=(Blast){b->target,.8f,BOMB_RADIUS};
        game_burst(g,point,(Color){1,.55f,.13f},18);audio_play_at(SOUND_EXPLOSION,point);
        for(int id=0;id<MAX_PLAYERS;id++)if(actor_alive(g,id)) {
            Vec3 center=vadd(actor_player(g,id)->body.position,(Vec3){0,.65f,0});
            Vec3 delta=vsub(center,point);float distance=vlength(delta);
            if(distance<BOMB_RADIUS&&world_ray(g->world,point,vnormalize(delta),distance)>=distance-.05f)
                hurt_actor(g,id,42*(1-.6f*distance/BOMB_RADIUS),point);
        }
        g->bombs[i]=g->bombs[--g->bomb_count];
    }
}

static void attack_update(Game *g, Enemy *e, float dt) {
    e->attack_timer = fmaxf(0, e->attack_timer-dt);
    if(e->type==ENEMY_BOMBER) {
        e->attack_fired=e->attack_timer==0;return;
    }
    float progress = 1-e->attack_timer/e->attack_duration;
    if (progress < .62f || e->attack_fired) return;
    e->attack_fired = true;
    if (e->type == ENEMY_CASTER) {
        if (g->projectile_count < MAX_PROJECTILES) {
            Vec3 origin = vadd(e->body.position,(Vec3){0,1.3f,0});
            g->projectiles[g->projectile_count++] = (Projectile){.position=origin,.velocity=vscale(e->attack_direction,14+2*game_threat(g)),.life=5,.color={.3f,1,.78f},.owner=-1};
        }
        e->shot_timer = fmaxf(.9f,2.5f-(float)g->wave*.06f)+random01()*.5f;
    } else {
        Vec3 origin = vadd(e->body.position,(Vec3){0,1.0f*enemy_scale(e),0});
        if(!actor_alive(g,e->target_actor))return;
        Vec3 delta = vsub(player_eye(actor_player(g,e->target_actor),g->motion),origin), dir = vnormalize(delta);
        float distance = vlength(delta), range = e->type == ENEMY_BRUTE ? 2.7f : 1.9f;
        float dot = dir.x*e->attack_direction.x+dir.y*e->attack_direction.y+dir.z*e->attack_direction.z;
        if (distance < range && dot > .45f && world_ray(g->world,origin,dir,distance) >= distance-.01f)
            hurt_actor(g,e->target_actor,e->type == ENEMY_BRUTE ? 28 : 15,origin);
        e->touch_timer = e->type == ENEMY_BRUTE ? 1.0f : .6f;
    }
}

static void begin_slam(Game *g,Enemy *e,const Player *p,int target) {
    g->actors[target].heavy_cooldown=5;
    Vec3 destination=vadd(p->body.position,(Vec3){p->body.velocity.x*.25f,0,p->body.velocity.z*.25f});
    Vec3 delta=vsub(destination,e->body.position);delta.y=0;float distance=vlength(delta);
    if(distance>9)destination=vadd(e->body.position,vscale(delta,9/distance));
    Vec3 chosen=e->body.position;
    for(int attempt=0;attempt<8;attempt++) {
        Vec3 q=vadd(e->body.position,vscale(vsub(destination,e->body.position),1-(float)attempt/8));
        q.y=e->body.position.y+2.25f;float floor=world_ray(g->world,q,(Vec3){0,-1,0},20);q.y-=floor;
        if(floor>.01f&&!world_blocked(g->world,q,ENEMY_RADIUS*enemy_size(e),ENEMY_HEIGHT*enemy_size(e),-1)){chosen=q;break;}
    }
    e->slam_target=chosen;e->slam_state=1;e->slam_timer=.75f;e->target_actor=target;e->attack_timer=0;
    e->body.velocity=(Vec3){0,0,0};audio_play_at(SOUND_CHAMPION,e->body.position);
}
static void slam_update(Game *g,Enemy *e,float dt) {
    e->slam_timer=fmaxf(0,e->slam_timer-dt);
    float scale=enemy_size(e);
    if(e->slam_state==1) {
        if(e->slam_timer>0)return;
        float rise=e->slam_target.y-e->body.position.y;
        float flight=(11+sqrtf(fmaxf(4,121-2*PLAYER_GRAVITY*rise)))/PLAYER_GRAVITY;
        Vec3 delta=vsub(e->slam_target,e->body.position);
        e->body.velocity=(Vec3){delta.x/flight,11,delta.z/flight};e->body.grounded=false;
        e->slam_state=2;e->slam_timer=3;
    }
    if(e->slam_state==2) {
        move_horizontal(g->world,&e->body,e->body.velocity.x*dt,e->body.velocity.z*dt,ENEMY_RADIUS*scale,ENEMY_HEIGHT*scale,0);
        move_vertical(g->world,&e->body,dt,ENEMY_RADIUS*scale,ENEMY_HEIGHT*scale);
        if(!e->body.grounded&&e->slam_timer>0)return;
        e->slam_state=3;e->slam_timer=.85f;e->slam_flash=.8f;e->touch_timer=4.5f;
        e->slam_target=e->body.position;e->body.velocity=(Vec3){0,0,0};
        Vec3 origin=vadd(e->body.position,(Vec3){0,.35f,0});
        for(int id=0;id<MAX_PLAYERS;id++)if(actor_alive(g,id)) {
            Player *p=actor_player(g,id);Vec3 delta=vsub(vadd(p->body.position,(Vec3){0,.55f,0}),origin);
            float distance=vlength(delta);
            if(distance<6&&fabsf(p->body.position.y-e->body.position.y)<1
                &&world_ray(g->world,origin,vnormalize(delta),distance)>=distance-.05f) {
                hurt_actor(g,id,38*(1-distance/10),origin);
                if(actor_alive(g,id)&&g->actors[id].invulnerable<=0) {
                    Vec3 push=vscale(vnormalize((Vec3){delta.x,0,delta.z}),5);
                    p->body.velocity=vadd(p->body.velocity,push);p->body.velocity.y=4;p->boost_timer=.35f;p->body.grounded=false;
                }
            }
        }
        game_burst(g,origin,(Color){1,.62f,.18f},16);audio_play_at(SOUND_EXPLOSION,origin);
    }else if(e->slam_state==3&&e->slam_timer==0)e->slam_state=0;
}

static void update_enemies(Game *g, float dt) {
    for (int i = 0; i < g->enemy_count; i++) {
        Enemy *e = &g->enemies[i];
        if (e->health <= 0) continue;
        e->flash = fmaxf(0,e->flash-dt);
        e->stagger = fmaxf(0,e->stagger-dt);
        e->spawn_time = fmaxf(0,e->spawn_time-dt);
        e->shot_timer -= dt;
        e->touch_timer = fmaxf(0,e->touch_timer-dt);e->slam_flash=fmaxf(0,e->slam_flash-dt);
        if(e->slam_state){slam_update(g,e,dt);continue;}
        int target_id=ai_target(g,e);
        if(target_id<0)continue;
        Vec3 p = player_eye(actor_player(g,target_id),g->motion), origin = vadd(e->body.position,(Vec3){0,1.3f*enemy_scale(e),0});
        Vec3 delta = vsub(p,origin), dir = vnormalize(delta);
        float distance = vlength(delta), horizontal = hypotf(delta.x,delta.z);
        bool visible = world_ray(g->world,origin,dir,distance) >= distance-.05f;
        if(e->attack_timer==0)e->target_actor=target_id;
        if(e->elite&&e->body.grounded&&e->spawn_time==0&&e->touch_timer==0&&visible&&horizontal<14&&game_heavy_available(g,target_id)) {
            begin_slam(g,e,actor_player(g,target_id),target_id);continue;
        }
        Vec3 move=ai_move(g,e,actor_player(g,target_id),visible,dt);
        if (e->attack_timer == 0 && e->spawn_time == 0 && e->stagger == 0 && visible) {
            if (e->type == ENEMY_CASTER && distance<36 && e->shot_timer<=0) {
                Player *victim=actor_player(g,target_id);float lead=clampf(distance/(14+2*game_threat(g)),0,1.0f)*.6f;
                Vec3 aim=vadd(p,vscale(victim->body.velocity,lead));
                begin_attack(e,vnormalize(vsub(aim,origin)),target_id);
            }
            else if(e->type==ENEMY_BOMBER&&distance<BOMBER_RANGE&&e->shot_timer<=0&&g->bomb_count<MAX_BOMBS&&game_heavy_available(g,target_id))
                begin_bomb(g,e,target_id);
            else if (!e->elite&&(e->type==ENEMY_RUNNER||e->type==ENEMY_BRUTE) && horizontal<(e->type == ENEMY_BRUTE ? 2.5f : 1.6f)
                && fabsf(delta.y)<1.7f && e->touch_timer==0) begin_attack(e,dir,target_id);
        }
        float desired_yaw = atan2f(delta.x,-delta.z);
        if (e->attack_timer==0) {
            float turn = atan2f(sinf(desired_yaw-e->yaw),cosf(desired_yaw-e->yaw));
            e->yaw += turn*fminf(1,dt*8);
        }
        float speed = e->type == ENEMY_BRUTE ? 2.6f : (e->type == ENEMY_BOMBER ? 2.8f : (e->type == ENEMY_CASTER ? 3.0f : 4.8f));
        speed += fminf(2.2f,(float)(g->wave-1)*.12f);
        speed*=g->difficulty==0?.9f:(g->difficulty==2?1.12f:1);
        if (e->attack_timer>0 || e->stagger>0 || e->spawn_time>0) speed = 0;
        Vec3 desired = vadd(vscale(vnormalize(move),speed),e->knockback);
        e->knockback = vscale(e->knockback,fmaxf(0,1-dt*14));
        e->body.velocity.x = desired.x; e->body.velocity.z = desired.z;
        float scale = enemy_scale(e);
        Vec3 before=e->body.position;
        move_horizontal(g->world,&e->body,desired.x*dt,desired.z*dt,ENEMY_RADIUS*scale,ENEMY_HEIGHT*scale,PLAYER_STEP);
        move_vertical(g->world,&e->body,dt,ENEMY_RADIUS*scale,ENEMY_HEIGHT*scale);
        float moved=hypotf(e->body.position.x-before.x,e->body.position.z-before.z);
        if(speed>1&&moved<speed*dt*.15f)e->stuck_time+=dt;else e->stuck_time=fmaxf(0,e->stuck_time-dt*2);
        float moving = e->body.grounded&&dt>0?moved/dt:0;
        e->walk_phase += moving*dt*(e->type == ENEMY_BRUTE ? 1.8f : (e->type==ENEMY_BOMBER?2.1f:2.8f));
        e->movement_blend += (clampf(moving/4,0,1)-e->movement_blend)*fminf(1,dt*12);
        if (e->attack_timer>0 && (e->stagger==0||e->type==ENEMY_BOMBER)) attack_update(g,e,dt);
    }
    for (int i=0;i<g->enemy_count;i++) for (int j=i+1;j<g->enemy_count;j++) {
        Enemy *a=&g->enemies[i], *b=&g->enemies[j];
        if (a->health<=0 || b->health<=0 || a->slam_state==2 || b->slam_state==2) continue;
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

static void rocket_explode(Game *g,const Projectile *rocket,Vec3 point) {
    int original=g->current_actor,owner=rocket->owner;
    if(owner<0||owner>=MAX_PLAYERS)return;
    game_actor_select(g,owner);
    if(g->blast_count<MAX_BLASTS)g->blasts[g->blast_count++]=(Blast){point,.8f,6.5f};
    else g->blasts[0]=(Blast){point,.8f,6.5f};
    game_burst(g,point,(Color){1,.44f,.08f},18);audio_play_at(SOUND_EXPLOSION,point);
    for(int i=0;i<g->enemy_count;i++)if(g->enemies[i].health>0) {
        Enemy *e=&g->enemies[i];Vec3 delta=vsub(vadd(e->body.position,(Vec3){0,.8f*enemy_size(e),0}),point);
        float distance=vlength(delta);Vec3 direction=vnormalize(delta);
        if(distance<6.5f&&world_ray(g->world,point,direction,distance)>=distance-.08f)
            damage_enemy(g,e,rocket->damage*(1-distance/6.5f),direction);
    }
    for(int i=0;i<g->barrel_count;i++)if(g->barrels[i].health>0) {
        Vec3 delta=vsub(vadd(g->barrels[i].position,(Vec3){0,.55f,0}),point);float distance=vlength(delta);
        if(distance<5&&world_ray(g->world,point,vnormalize(delta),distance)>=distance-.08f)explode_barrel(g,i,owner);
    }
    /* Pas de dégâts de roquette aux alliés ; le tireur peut effectuer un rocket jump. */
    if(actor_alive(g,owner)) {
        Player *p=actor_player(g,owner);Vec3 delta=vsub(player_eye(p,false),point);float distance=vlength(delta);
        if(distance<5&&world_ray(g->world,point,vnormalize(delta),distance)>=distance-.08f) {
            float strength=1-distance/5;hurt_actor(g,owner,22*strength,point);
            if(actor_alive(g,owner)) {
                Vec3 push=vscale(vnormalize(delta),11*strength);
                p->body.velocity=vadd(p->body.velocity,push);p->body.velocity.y=fmaxf(p->body.velocity.y,16*strength);
                p->body.grounded=false;p->boost_timer=.6f;
            }
        }
    }
    game_actor_save(g);game_actor_select(g,original);
}
static void update_projectiles(Game *g,float dt) {
    for(int i=0;i<g->projectile_count;) {
        Projectile *b=&g->projectiles[i];b->life-=dt;
        float length=vlength(b->velocity)*dt;Vec3 direction=vnormalize(b->velocity);
        float nearest=world_ray(g->world,b->position,direction,length);
        int victim=-1;
        if(b->kind==1) {
            for(int k=0;k<g->enemy_count;k++)if(g->enemies[k].health>0) {
                const Enemy *e=&g->enemies[k];Vec3 p=e->body.position;float r=.5f*enemy_size(e);
                float hit=ray_box(b->position,direction,(Box){{p.x-r,p.y,p.z-r},{p.x+r,p.y+ENEMY_HEIGHT*enemy_size(e),p.z+r}},length);
                if(hit<nearest){nearest=hit;victim=k;}
            }
            for(int k=0;k<g->barrel_count;k++)if(g->barrels[k].health>0) {
                Vec3 p=g->barrels[k].position;
                float hit=ray_box(b->position,direction,(Box){{p.x-.43f,p.y,p.z-.43f},{p.x+.43f,p.y+1.1f,p.z+.43f}},length);
                if(hit<nearest){nearest=hit;victim=k;}
            }
            if(nearest<length||victim>=0||b->life<=0) {
                Vec3 point=vadd(b->position,vscale(direction,fmaxf(0,nearest-.06f)));
                Projectile copy=*b;rocket_explode(g,&copy,point);b->life=0;
            }else b->position=vadd(b->position,vscale(b->velocity,dt));
        }else {
            for(int id=0;id<MAX_PLAYERS;id++)if(actor_alive(g,id)) {
                Vec3 p=actor_player(g,id)->body.position;
                float hit=ray_box(b->position,direction,(Box){{p.x-PLAYER_RADIUS,p.y,p.z-PLAYER_RADIUS},{p.x+PLAYER_RADIUS,p.y+PLAYER_HEIGHT,p.z+PLAYER_RADIUS}},length);
                if(hit<=nearest){nearest=hit;victim=id;}
            }
            if(victim>=0){hurt_actor(g,victim,12,b->position);b->life=0;}
            else if(nearest<length){game_burst(g,vadd(b->position,vscale(direction,nearest)),(Color){1,.4f,.1f},2);b->life=0;}
            else b->position=vadd(b->position,vscale(b->velocity,dt));
        }
        if(b->life<=0)g->projectiles[i]=g->projectiles[--g->projectile_count];else i++;
    }
}

static void update_actor(Game *g,Input input,float dt) {
    encounters_actor(g,dt);
    g->player.upgrade_lock=fmaxf(0,g->player.upgrade_lock-dt);
    if(input.upgrade)game_choose_upgrade(g,input.upgrade);
    Actor *actor=&g->actors[g->current_actor];
    actor->revive_held=input.revive&&!g->dead;
    if(input.revive&&!actor->interact_held&&game_revive_target(g,g->current_actor)<0)encounters_reactor(g);
    actor->interact_held=input.revive;
    if(g->dead){if(g->cooperative)move_vertical(g->world,&g->player.body,dt,PLAYER_RADIUS,.5f);return;}
    input.revive=input.revive&&game_revive_target(g,g->current_actor)>=0;
    if(input.revive){input.firing=false;input.melee=false;input.sprint=false;input.dash=false;input.forward*=.25f;input.right*=.25f;}
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
    float reload_before=p->reload_timer;
    p->reload_timer=fmaxf(0,p->reload_timer-dt);
    if(reload_before>0&&p->reload_timer==0){p->ammo[p->weapon]=(uint8_t)WEAPONS[p->weapon].magazine;audio_play(SOUND_PUMP);}
    if(reload_before>0&&p->reload_timer>0&&p->reload_duration>0) {
        float before=1-reload_before/p->reload_duration,now=1-p->reload_timer/p->reload_duration;
        const float magazine_clicks[]={.24f,.66f,.86f},shell_clicks[]={.28f,.47f,.66f,.86f};
        const float *clicks=p->weapon==1?shell_clicks:magazine_clicks;int count=p->weapon==1?4:3;
        for(int phase=0;phase<count;phase++)if(before<clicks[phase]&&now>=clicks[phase])
            audio_play(phase==count-1?SOUND_PUMP:SOUND_SWITCH);
    }
    if(input.reload||(!p->ammo[p->weapon]&&p->reload_timer==0))game_reload(g);
    p->melee_timer=fmaxf(0,p->melee_timer-dt);
    float previous_switch=p->weapon_switch;
    p->weapon_switch=fmaxf(0,p->weapon_switch-dt);
    if (previous_switch>WEAPON_SWITCH_TIME/2 && p->weapon_switch<=WEAPON_SWITCH_TIME/2) {
        p->weapon=p->weapon_target;p->switch_power_time=2.5f;
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
        if (pickup->cooldown==0 && p->health<player_max_health(p) && fabsf(delta.y)<.9f && hypotf(delta.x,delta.z)<1.3f) {
            p->health=fminf(player_max_health(p),p->health+25);
            pickup->cooldown=pickup->position.y>2?UPPER_PICKUP_RESPAWN_TIME:PICKUP_RESPAWN_TIME;
            audio_play(SOUND_PICKUP); say(g,"+25 SANTE",1);
        }
    }
    }
}

int game_revive_target(const Game *g,int helper) {
    if(!g->cooperative||helper<0||helper>=MAX_PLAYERS||!actor_alive(g,helper))return -1;
    const Player *from=helper==g->current_actor?&g->player:&g->actors[helper].player;
    int target=-1;float best=REVIVE_RANGE;
    for(int id=0;id<MAX_PLAYERS;id++)if(id!=helper&&g->actors[id].active&&!actor_alive(g,id)) {
        const Player *to=id==g->current_actor?&g->player:&g->actors[id].player;
        Vec3 origin=player_eye(from,false),point=vadd(to->body.position,(Vec3){0,.55f,0});
        Vec3 delta=vsub(point,origin);float length=vlength(delta);
        float distance=vlength(vsub(to->body.position,from->body.position));
        if(distance<best&&world_ray(g->world,origin,vnormalize(delta),length)>=length-.05f){target=id;best=distance;}
    }
    return target;
}
static void update_revives(Game *g,float dt) {
    for(int id=0;id<MAX_PLAYERS;id++)g->actors[id].revive_target=-1;
    for(int down=0;down<MAX_PLAYERS;down++)if(g->actors[down].active&&!actor_alive(g,down)) {
        int helper=-1;
        for(int id=0;id<MAX_PLAYERS;id++)if(actor_alive(g,id)&&g->actors[id].revive_held) {
            float damage=id==g->current_actor?g->damage_time:g->actors[id].damage_time;
            if(damage<.29f&&game_revive_target(g,id)==down){helper=id;break;}
        }
        Actor *a=&g->actors[down];
        if(helper<0){a->revive_progress=0;continue;}
        g->actors[helper].revive_target=down;a->revive_progress=fminf(REVIVE_TIME,a->revive_progress+dt);
        if(a->revive_progress<REVIVE_TIME)continue;
        Player *p=actor_player(g,down);
        if(world_blocked(g->world,p->body.position,PLAYER_RADIUS,PLAYER_HEIGHT,-1))continue;
        p->health=player_max_health(p)*.5f;p->body.velocity=(Vec3){0,0,0};
        p->reload_timer=p->weapon_switch=p->melee_timer=0;p->weapon_target=p->weapon;
        a->dead=false;a->revive_progress=0;a->revive_held=false;a->revive_target=-1;a->respawn_timer=0;a->invulnerable=2;
        if(down==g->current_actor){g->dead=false;g->shot_cooldown=0;g->shot_age=10;}
        else {a->shot_cooldown=0;a->shot_age=10;}
        if(down==g->local_actor||helper==g->local_actor)audio_play(SOUND_PICKUP);
        say(g,"COEQUIPIER REANIME / +2 S DE PROTECTION",2);
    }
}

static void update_world(Game *g,float dt) {
    g->time+=dt;g->tick++;encounters_update(g,dt);
    g->message_time=fmaxf(0,g->message_time-dt);
    g->wave_clear_time=fmaxf(0,g->wave_clear_time-dt);
    g->unlock_time=fmaxf(0,g->unlock_time-dt);g->event_notice_time=fmaxf(0,g->event_notice_time-dt);
    for(int i=0;i<g->blast_count;) {
        g->blasts[i].time-=dt;
        if(g->blasts[i].time<=0)g->blasts[i]=g->blasts[--g->blast_count];else i++;
    }
    g->nav_timer-=dt;
    if(g->nav_timer<=0) {
        int target=nearest_actor(g,(Vec3){0,0,0});
        if(target>=0) {
            Vec3 p=actor_player(g,target)->body.position;
            navigation_update(g->world,p.x,p.z);
        }
        for(int id=0;id<MAX_PLAYERS;id++)if(actor_alive(g,id))navigation_actor(g->world,id,actor_player(g,id)->body.position);
        g->nav_timer=.35f;
    }
    for(int i=0;i<g->pickup_count;i++)g->pickups[i].cooldown=fmaxf(0,g->pickups[i].cooldown-dt);
    for(int i=0;i<g->barrel_count;i++)g->barrels[i].explosion_time=fmaxf(0,g->barrels[i].explosion_time-dt);
    update_bombs(g,dt);update_enemies(g,dt);update_projectiles(g,dt);
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
    if(alive==0){if(g->cooperative||g->lives<=0)g->round_over=true;return;}
    if(g->cooperative)update_revives(g,dt);
    if(g->reinforcements>0&&g->wave_timer<0) {
        g->reinforce_timer=fmaxf(0,g->reinforce_timer-dt);
        if(g->reinforce_timer==0){spawn_reinforcements(g,3);g->reinforce_timer=2;}
    }
    if(g->enemy_count==0&&g->reinforcements==0) {
        if(g->wave_timer<0) {
            encounters_finish(g);
            g->wave_timer=INTERMISSION_TIME;g->score+=250;g->completed_waves=g->wave;g->wave_clear_time=2.8f;
            g->projectile_count=g->bomb_count=0;game_end_rocket_event(g);game_offer_upgrades(g);
            for(int i=0;i<MAX_PLAYERS;i++)if(actor_alive(g,i)) {
                Player *p=actor_player(g,i);p->health=fminf(player_max_health(p),p->health+10);
            }
            say(g,g->wave%5==0?"CHAMPION VAINCU - NOUVEAU SECTEUR DEBLOQUE":"VAGUE TERMINEE - MODULE DEBLOQUE / +10 SANTE",2.8f);
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
    memset(a->damage_timers,0,sizeof(a->damage_timers));
    a->shot_cooldown=0;a->dash_pressed=false;a->melee_pressed=false;a->shot_age=10;a->recoil=0;
    a->player.reload_timer=a->player.weapon_switch=a->player.melee_timer=a->player.dash_timer=0;
    a->player.weapon_target=a->player.weapon;
    for(int i=0;i<WEAPON_COUNT;i++)a->player.ammo[i]=(uint8_t)WEAPONS[i].magazine;
}
void game_update_coop(Game *g,const Input inputs[MAX_PLAYERS],float dt) {
    if(g->round_over)return;
    game_actor_save(g);
    bool alive=false;for(int i=0;i<MAX_PLAYERS;i++)if(actor_alive(g,i))alive=true;
    if(!alive){g->round_over=true;return;}
    game_prepare_mobility(g,dt);
    for(int i=0;i<MAX_PLAYERS;i++)if(g->actors[i].active) {
        game_actor_select(g,i);
        Actor *a=&g->actors[i];a->invulnerable=fmaxf(0,a->invulnerable-dt);
        if(a->dead)a->respawn_timer=0;
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
    game_prepare_mobility(g,dt);update_actor(g,input,dt);g->time+=dt;
    g->prediction=false;game_actor_save(g);
}
void game_update(Game *g,Input input,float dt) {
    if(g->round_over)return;
    if(g->dead) {
        Actor *a=&g->actors[0];a->respawn_timer=fmaxf(0,a->respawn_timer-dt);
        if(a->respawn_timer==0&&g->lives>0){
            game_actor_save(g);respawn(g,0);g->current_actor=-1;game_actor_select(g,0);
            say(g,"NOUVELLE VIE / 2 SECONDES DE PROTECTION",2);
        }
        return;
    }
    game_prepare_mobility(g,dt);
    g->actors[0].invulnerable=fmaxf(0,g->actors[0].invulnerable-dt);
    audio_listener(player_eye(&g->player,g->motion),g->player.yaw);
    update_actor(g,input,dt);game_actor_save(g);
    update_world(g,dt);game_actor_save(g);
}
