#include "network.h"
#include <math.h>
#include <string.h>

/* Encodage explicite, sans copier des structures dont le padding varie sur ARM. */
_Static_assert(sizeof(float)==4,"Le protocole demande des flottants 32 bits");
typedef struct {uint8_t *data;size_t size,pos;bool ok;} Writer;
typedef struct {const uint8_t *data;size_t size,pos;bool ok;} Reader;
static void w8(Writer *w,uint8_t v){if(w->pos>=w->size){w->ok=false;return;}w->data[w->pos++]=v;}
static void w32(Writer *w,uint32_t v){for(int i=3;i>=0;i--)w8(w,(uint8_t)(v>>(i*8)));}
static void wf(Writer *w,float v){uint32_t bits;memcpy(&bits,&v,4);w32(w,bits);}
static void wv(Writer *w,Vec3 v){wf(w,v.x);wf(w,v.y);wf(w,v.z);}
static void wc(Writer *w,Color c){wf(w,c.r);wf(w,c.g);wf(w,c.b);}
static uint8_t r8(Reader *r){if(r->pos>=r->size){r->ok=false;return 0;}return r->data[r->pos++];}
static uint32_t r32(Reader *r){uint32_t v=0;for(int i=0;i<4;i++)v=(v<<8)|r8(r);return v;}
static float rf(Reader *r){uint32_t bits=r32(r);float v;memcpy(&v,&bits,4);if(!isfinite(v)||fabsf(v)>1e6f){r->ok=false;return 0;}return v;}
static Vec3 rv(Reader *r){Vec3 v;v.x=rf(r);v.y=rf(r);v.z=rf(r);return v;}
static Color rc(Reader *r){Color c;c.r=rf(r);c.g=rf(r);c.b=rf(r);return c;}
static void ws(Writer *w,const char *s,size_t limit){size_t n=strnlen(s,limit);w8(w,(uint8_t)n);for(size_t i=0;i<n;i++)w8(w,(uint8_t)s[i]);}
static void rs(Reader *r,char *s,size_t capacity){unsigned n=r8(r);if(n>=capacity){r->ok=false;s[0]=0;return;}for(unsigned i=0;i<n;i++){uint8_t c=r8(r);if(c<32||c>126)r->ok=false;s[i]=(char)c;}s[n]=0;}
static void wb(Writer *w,const Body *b){wv(w,b->position);wv(w,b->velocity);w8(w,b->grounded);}
static void rb(Reader *r,Body *b){b->position=rv(r);b->velocity=rv(r);uint8_t ground=r8(r);b->grounded=ground!=0;if(ground>1)r->ok=false;}

static void player_write(Writer *w,const Player *p) {
    wb(w,&p->body);
    #define F(field) wf(w,p->field)
    F(yaw);F(pitch);F(health);F(bob);F(pad_cooldown);F(movement_blend);F(strafe_blend);
    F(landing);F(sway_x);F(sway_y);F(step_timer);F(dash_timer);F(dash_cooldown);
    F(weapon_switch);F(melee_timer);
    #undef F
    w8(w,(uint8_t)p->weapon);w8(w,(uint8_t)p->weapon_target);
    for(int i=0;i<UPGRADE_COUNT;i++)w8(w,p->upgrades[i]);
    for(int i=0;i<UPGRADE_CHOICES;i++)w8(w,p->offers[i]);
    w8(w,p->upgrade_pending);wf(w,p->upgrade_flash);
}
static void player_read(Reader *r,Player *p) {
    rb(r,&p->body);
    #define F(field) p->field=rf(r)
    F(yaw);F(pitch);F(health);F(bob);F(pad_cooldown);F(movement_blend);F(strafe_blend);
    F(landing);F(sway_x);F(sway_y);F(step_timer);F(dash_timer);F(dash_cooldown);
    F(weapon_switch);F(melee_timer);
    #undef F
    p->weapon=r8(r);p->weapon_target=r8(r);
    for(int i=0;i<UPGRADE_COUNT;i++){p->upgrades[i]=r8(r);if(p->upgrades[i]>UPGRADE_MAX_LEVEL)r->ok=false;}
    for(int i=0;i<UPGRADE_CHOICES;i++){p->offers[i]=r8(r);if(p->offers[i]>=UPGRADE_COUNT)r->ok=false;}
    uint8_t pending=r8(r);p->upgrade_pending=pending!=0;p->upgrade_flash=rf(r);
    if(pending>1||p->upgrade_flash<0||p->upgrade_flash>1.5f)r->ok=false;
    if(p->weapon>1||p->weapon_target>1||p->health<0||p->health>player_max_health(p)||fabsf(p->pitch)>1.46f)r->ok=false;
}
static void actor_write(Writer *w,const Actor *a) {
    w8(w,(uint8_t)((a->active?1:0)|(a->dead?2:0)|(a->dash_pressed?4:0)|(a->melee_pressed?8:0)));
    ws(w,a->name,PLAYER_NAME_LENGTH);player_write(w,&a->player);
    #define F(field) wf(w,a->field)
    F(shot_cooldown);F(recoil);F(hit_time);F(damage_time);F(shot_age);F(combo_timer);F(respawn_timer);F(invulnerable);
    F(headshot_time);F(kill_time);F(damage_yaw);
    #undef F
    w32(w,(uint32_t)a->combo);w32(w,a->input_ack);w32(w,a->shots);
    w32(w,(uint32_t)a->run_kills);w32(w,(uint32_t)a->run_headshots);w32(w,(uint32_t)a->reward_points);wf(w,a->reward_time);
}
static void actor_read(Reader *r,Actor *a) {
    uint8_t flags=r8(r);if(flags>15)r->ok=false;
    a->active=(flags&1)!=0;a->dead=(flags&2)!=0;a->dash_pressed=(flags&4)!=0;a->melee_pressed=(flags&8)!=0;
    rs(r,a->name,sizeof(a->name));player_read(r,&a->player);
    #define F(field) a->field=rf(r)
    F(shot_cooldown);F(recoil);F(hit_time);F(damage_time);F(shot_age);F(combo_timer);F(respawn_timer);F(invulnerable);
    F(headshot_time);F(kill_time);F(damage_yaw);
    #undef F
    uint32_t combo=r32(r);if(combo>1000000)r->ok=false;a->combo=(int)combo;
    a->input_ack=r32(r);a->shots=r32(r);
    uint32_t kills=r32(r),heads=r32(r),reward=r32(r);
    if(kills>1000000||heads>10000000||reward>2147483647u)r->ok=false;
    a->run_kills=(int)kills;a->run_headshots=(int)heads;a->reward_points=(int)reward;a->reward_time=rf(r);
}
static void enemy_write(Writer *w,const Enemy *e) {
    w32(w,(uint32_t)e->id);w8(w,(uint8_t)e->type);wb(w,&e->body);
    #define F(field) wf(w,e->field)
    F(health);F(flash);F(shot_timer);F(touch_timer);F(max_health);F(yaw);F(walk_phase);
    F(movement_blend);F(attack_timer);F(attack_duration);F(stagger);F(spawn_time);F(death_time);
    #undef F
    wv(w,e->attack_direction);wv(w,e->knockback);w8(w,e->attack_fired);w8(w,(uint8_t)e->target_actor);
    w8(w,e->elite);
}
static void enemy_read(Reader *r,Enemy *e) {
    uint32_t id=r32(r);if(id>2147483647u)r->ok=false;e->id=(int)id;e->type=r8(r);rb(r,&e->body);
    #define F(field) e->field=rf(r)
    F(health);F(flash);F(shot_timer);F(touch_timer);F(max_health);F(yaw);F(walk_phase);
    F(movement_blend);F(attack_timer);F(attack_duration);F(stagger);F(spawn_time);F(death_time);
    #undef F
    e->attack_direction=rv(r);e->knockback=rv(r);uint8_t fired=r8(r);e->attack_fired=fired!=0;
    e->target_actor=r8(r);
    uint8_t elite=r8(r);e->elite=elite!=0;if(elite>1)r->ok=false;
    if(e->type>ENEMY_BRUTE||fired>1||e->target_actor>=MAX_PLAYERS||e->attack_timer<0
        ||(e->attack_timer>0&&e->attack_duration<=0)||e->max_health<0||e->max_health>10000)r->ok=false;
}

size_t protocol_command_write(uint8_t *data,size_t capacity,const Command *c) {
    Writer w={data,capacity,0,true};w32(&w,c->sequence);w32(&w,c->round);wf(&w,c->yaw);wf(&w,c->pitch);
    wf(&w,c->input.forward);wf(&w,c->input.right);
    uint8_t flags=(uint8_t)((c->input.jump?1:0)|(c->input.sprint?2:0)|(c->input.firing?4:0)|(c->input.dash?8:0)|(c->input.melee?16:0));
    w8(&w,flags);w8(&w,(uint8_t)c->weapon);w8(&w,(uint8_t)c->input.upgrade);return w.ok?w.pos:0;
}
bool protocol_command_read(Command *c,const uint8_t *data,size_t size) {
    Reader r={data,size,0,true};Command out={0};out.sequence=r32(&r);out.round=r32(&r);out.yaw=rf(&r);out.pitch=rf(&r);
    out.input.forward=rf(&r);out.input.right=rf(&r);uint8_t flags=r8(&r);out.weapon=r8(&r);
    out.input.jump=(flags&1)!=0;out.input.sprint=(flags&2)!=0;out.input.firing=(flags&4)!=0;
    out.input.dash=(flags&8)!=0;out.input.melee=(flags&16)!=0;
    out.input.upgrade=r8(&r);
    if(flags>31||out.weapon>1||out.input.upgrade>UPGRADE_CHOICES||fabsf(out.yaw)>2*PI_F+.01f||fabsf(out.pitch)>1.46f
        ||fabsf(out.input.forward)>1||fabsf(out.input.right)>1)r.ok=false;
    if(!r.ok||r.pos!=size)return false;
    *c=out;return true;
}
size_t protocol_snapshot_write(uint8_t *data,size_t capacity,const Game *g) {
    if(g->enemy_count<0||g->enemy_count>MAX_ENEMIES||g->corpse_count<0||g->corpse_count>MAX_CORPSES
        ||g->projectile_count<0||g->projectile_count>MAX_PROJECTILES||g->pickup_count<0||g->pickup_count>MAX_PICKUPS
        ||g->barrel_count<0||g->barrel_count>MAX_BARRELS||g->particle_count<0||g->particle_count>MAX_PARTICLES)return 0;
    Writer w={data,capacity,0,true};w8(&w,NET_PROTOCOL);w8(&w,(uint8_t)g->difficulty);w32(&w,g->tick);
    w8(&w,g->round_over);w32(&w,(uint32_t)g->wave);w32(&w,(uint32_t)g->score);w32(&w,(uint32_t)g->kills);
    wf(&w,g->time);wf(&w,g->wave_timer);wf(&w,g->message_time);ws(&w,g->message,95);
    w32(&w,(uint32_t)g->completed_waves);wf(&w,g->wave_clear_time);
    for(int i=0;i<MAX_PLAYERS;i++)actor_write(&w,&g->actors[i]);
    w8(&w,(uint8_t)g->enemy_count);for(int i=0;i<g->enemy_count;i++)enemy_write(&w,&g->enemies[i]);
    w8(&w,(uint8_t)g->corpse_count);for(int i=0;i<g->corpse_count;i++)enemy_write(&w,&g->corpses[i]);
    w8(&w,(uint8_t)g->projectile_count);
    for(int i=0;i<g->projectile_count;i++){const Projectile *p=&g->projectiles[i];wv(&w,p->position);wv(&w,p->velocity);wf(&w,p->life);wc(&w,p->color);}
    w8(&w,(uint8_t)g->pickup_count);
    for(int i=0;i<g->pickup_count;i++){wv(&w,g->pickups[i].position);wf(&w,g->pickups[i].cooldown);}
    w8(&w,(uint8_t)g->barrel_count);
    for(int i=0;i<g->barrel_count;i++) {
        const Barrel *b=&g->barrels[i];wv(&w,b->position);wf(&w,b->health);wf(&w,b->explosion_time);w8(&w,(uint8_t)b->owner);
    }
    w8(&w,(uint8_t)g->particle_count);
    for(int i=0;i<g->particle_count;i++) {
        const Particle *p=&g->particles[i];wv(&w,p->position);wv(&w,p->velocity);wc(&w,p->color);
        wf(&w,p->life);wf(&w,p->max_life);wf(&w,p->size);
    }
    return w.ok?w.pos:0;
}
bool protocol_snapshot_read(Game *g,const uint8_t *data,size_t size) {
    Reader r={data,size,0,true};Game out=*g;
    if(r8(&r)!=NET_PROTOCOL)return false;
    out.difficulty=r8(&r);if(out.difficulty>2)return false;
    out.tick=r32(&r);uint8_t ended=r8(&r);if(ended>1)r.ok=false;out.round_over=ended!=0;
    uint32_t wave=r32(&r),score=r32(&r),kills=r32(&r);
    if(wave>1000000||score>2147483647u||kills>2147483647u)r.ok=false;
    out.wave=(int)wave;out.score=(int)score;out.kills=(int)kills;
    out.time=rf(&r);out.wave_timer=rf(&r);out.message_time=rf(&r);rs(&r,out.message,sizeof(out.message));
    uint32_t completed=r32(&r);if(completed>1000000||completed>wave)r.ok=false;
    out.completed_waves=(int)completed;out.wave_clear_time=rf(&r);
    for(int i=0;i<MAX_PLAYERS;i++)actor_read(&r,&out.actors[i]);
    out.enemy_count=r8(&r);if(out.enemy_count>MAX_ENEMIES)return false;
    for(int i=0;i<out.enemy_count;i++)enemy_read(&r,&out.enemies[i]);
    out.corpse_count=r8(&r);if(out.corpse_count>MAX_CORPSES)return false;
    for(int i=0;i<out.corpse_count;i++)enemy_read(&r,&out.corpses[i]);
    out.projectile_count=r8(&r);if(out.projectile_count>MAX_PROJECTILES)return false;
    for(int i=0;i<out.projectile_count;i++){Projectile *p=&out.projectiles[i];p->position=rv(&r);p->velocity=rv(&r);p->life=rf(&r);p->color=rc(&r);}
    out.pickup_count=r8(&r);if(out.pickup_count>MAX_PICKUPS)return false;
    for(int i=0;i<out.pickup_count;i++){out.pickups[i].position=rv(&r);out.pickups[i].cooldown=rf(&r);}
    out.barrel_count=r8(&r);if(out.barrel_count>MAX_BARRELS)return false;
    for(int i=0;i<out.barrel_count;i++) {
        Barrel *b=&out.barrels[i];b->position=rv(&r);b->health=rf(&r);b->explosion_time=rf(&r);
        unsigned owner=r8(&r);b->owner=owner==255?-1:(int)owner;
        if((owner>=MAX_PLAYERS&&owner!=255)||b->health<0||b->health>45||b->explosion_time<0||b->explosion_time>.66f)r.ok=false;
    }
    unsigned particles=r8(&r);if(particles>MAX_PARTICLES)return false;
    out.particle_count=0;
    for(unsigned i=0;i<particles;i++) {
        Particle p;p.position=rv(&r);p.velocity=rv(&r);p.color=rc(&r);
        p.life=rf(&r);p.max_life=rf(&r);p.size=rf(&r);
        if(p.life<0||p.max_life<=0||p.life>p.max_life+.001f||p.size<=0||p.size>2)r.ok=false;
        if(out.particle_count<out.particle_limit&&out.particle_count<MAX_PARTICLES)out.particles[out.particle_count++]=p;
    }
    if(!r.ok||r.pos!=size||out.local_actor<0||out.local_actor>=MAX_PLAYERS)return false;
    out.cooperative=true;out.current_actor=-1;game_actor_select(&out,out.local_actor);
    *g=out;return true;
}
