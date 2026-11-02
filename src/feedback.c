#include "feedback.h"
#include <math.h>
#include <string.h>

/* État de présentation local : aucune écriture dans Game, aucune utilisation de
   rand(), aucun paquet supplémentaire. Les tableaux restent bornés sur le Pi. */
enum { SHARD, SPARK, DUST, TRAIL };
enum { IMPACT_HEAD=1, IMPACT_INTERRUPT, IMPACT_CHAMPION, IMPACT_PIERCE };
static float random_fx(Feedback *f) {
    f->rng=f->rng*1664525u+1013904223u;
    return (float)(f->rng>>8)/16777216.0f;
}
static Vec3 cross(Vec3 a,Vec3 b) {
    return (Vec3){a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};
}
Color feedback_enemy_color(const Enemy *e) {
    if(e->elite)return (Color){1,.68f,.2f};
    if(e->type==ENEMY_CASTER)return (Color){.55f,.36f,.84f};
    if(e->type==ENEMY_BRUTE)return (Color){.70f,.61f,.28f};
    if(e->type==ENEMY_BOMBER)return (Color){1,.64f,.18f};
    return (Color){.68f,.30f,.17f};
}
static const Enemy *find_enemy(const Enemy *enemies,int count,int id) {
    for(int i=0;i<count;i++)if(enemies[i].id==id)return &enemies[i];
    return NULL;
}
static bool focused(const Game *g,Vec3 point,float cone) {
    Vec3 eye=player_eye(&g->player,false),d=vsub(point,eye);float distance=vlength(d);
    if(distance<.1f||distance>48)return false;
    Vec3 direction=vscale(d,1/distance),forward=look_direction(g->player.yaw,g->player.pitch);
    return direction.x*forward.x+direction.y*forward.y+direction.z*forward.z>cone
        &&world_ray(g->world,eye,direction,distance)>=distance-.15f;
}
static int priority(int kind) {
    return kind==IMPACT_CHAMPION?4:(kind==IMPACT_PIERCE?3:(kind==IMPACT_HEAD?2:1));
}
static void impact(Feedback *f,const Game *g,const App *app,int kind,int id,int second) {
    if(app->impact_intensity<=0||app->state!=STATE_PLAYING||g->dead)return;
    if(f->impact_time>0) {
        if(priority(kind)<=priority(f->impact_kind))return;
    }else if(f->impact_cooldown>0&&priority(kind)<=priority(f->impact_kind))return;
    f->impact_kind=kind;f->impact_ids[0]=id;f->impact_ids[1]=second;
    f->impact_color=kind==IMPACT_CHAMPION?(Color){.98f,.98f,.94f}:
        (kind==IMPACT_INTERRUPT?(Color){1,.57f,.24f}:(kind==IMPACT_PIERCE?(Color){.78f,.54f,1}:(Color){.38f,.94f,1}));
    f->impact_duration=kind==IMPACT_CHAMPION?.060f:.045f;
    f->impact_time=f->impact_duration;f->impact_cooldown=kind==IMPACT_CHAMPION?.8f:.4f;
    f->impact_drawn=f->hold_drawn=false;f->held_weapon=g->player.weapon;
    f->hold_time=app->motion?.040f*app->impact_intensity:0;
}
static int budget(const App *app) {return app->quality==0?40:FEEDBACK_PARTICLES;}
static void particle(Feedback *f,const App *app,Vec3 p,Vec3 v,Color c,float life,float size,int kind) {
    if(f->particle_count>=budget(app)||app->impact_intensity<=0)return;
    f->particles[f->particle_count++]=(FeedbackParticle){.position=p,.velocity=v,.color=c,
        .life=life,.duration=life,.size=size,.angle=random_fx(f)*PI_F*2,.spin=(random_fx(f)-.5f)*12,.kind=kind};
}
static void debris(Feedback *f,const App *app,Vec3 p,Vec3 normal,Color c,int count,int kind) {
    count=(int)ceilf((float)count*app->impact_intensity);
    for(int i=0;i<count;i++) {
        Vec3 v={(random_fx(f)-.5f)*4,random_fx(f)*3+.4f,(random_fx(f)-.5f)*4};
        v=vadd(v,vscale(normal,kind==SPARK?4:1.5f));
        float life=kind==SPARK?.16f+random_fx(f)*.14f:(kind==DUST?.25f+random_fx(f)*.20f:.5f+random_fx(f)*.35f);
        float size=kind==SPARK?.018f:(kind==DUST?.045f+random_fx(f)*.035f:.055f+random_fx(f)*.07f);
        particle(f,app,p,v,c,life,size,kind);
    }
}
static void ring(Feedback *f,Vec3 p,Color c,float radius,float duration) {
    if(f->ring_count>=FEEDBACK_RINGS)f->rings[0]=f->rings[--f->ring_count];
    f->rings[f->ring_count++]=(FeedbackRing){p,c,duration,duration,radius};
}
static Vec3 normal_at(Box b,Vec3 p) {
    float distances[]={fabsf(p.x-b.min.x),fabsf(p.x-b.max.x),fabsf(p.y-b.min.y),
        fabsf(p.y-b.max.y),fabsf(p.z-b.min.z),fabsf(p.z-b.max.z)};
    const Vec3 normals[]={{-1,0,0},{1,0,0},{0,-1,0},{0,1,0},{0,0,-1},{0,0,1}};
    int best=0;for(int i=1;i<6;i++)if(distances[i]<distances[best])best=i;
    return normals[best];
}
static void mark(Feedback *f,const Game *g,const App *app,Vec3 p,Vec3 normal,int solid) {
    int limit=app->quality==0?8:FEEDBACK_MARKS,slot=f->mark_count;
    if(slot>=limit) {
        slot=0;for(int i=1;i<f->mark_count;i++)if(f->marks[i].life<f->marks[slot].life)slot=i;
    }else f->mark_count++;
    FeedbackMark m={.position=vadd(p,vscale(normal,.012f)),.normal=normal,.life=3,.size=.09f,.platform=-1};
    if(solid<0){m.size=.16f;m.life=1.5f;}
    for(int i=0;i<g->world->platform_count;i++)if(g->world->platforms[i].solid==solid) {
        m.platform=i;m.offset=vsub(m.position,platform_position(&g->world->platforms[i],g->world->platform_time));break;
    }
    f->marks[slot]=m;
}
static void shot_surface(Feedback *f,const Game *g,const App *app,int id) {
    const Player *p=id==g->local_actor?&g->player:&g->actors[id].player;
    if(p->weapon==WEAPON_ROCKET)return;
    Vec3 origin=player_eye(p,false),dir=look_direction(p->yaw,p->pitch);
    float distance=world_ray(g->world,origin,dir,SHOT_RANGE);
    /* Les marques de surface restent cosmétiques. Respecter les créatures et
       barils évite de dessiner un impact sur le mur situé derrière une cible. */
    for(int i=0;i<g->enemy_count;i++) {
        const Enemy *e=&g->enemies[i];Vec3 q=e->body.position;float scale=enemy_size(e),r=.5f*scale;
        if(e->health>0&&ray_box(origin,dir,(Box){{q.x-r,q.y,q.z-r},{q.x+r,q.y+ENEMY_HEIGHT*scale,q.z+r}},distance)<distance)return;
    }
    for(int i=0;i<g->corpse_count;i++)if(g->corpses[i].death_time<.18f) {
        const Enemy *e=&g->corpses[i];Vec3 q=e->body.position;float scale=enemy_size(e),r=.55f*scale;
        if(ray_box(origin,dir,(Box){{q.x-r,q.y,q.z-r},{q.x+r,q.y+ENEMY_HEIGHT*scale,q.z+r}},distance)<distance)return;
    }
    for(int i=0;i<g->barrel_count;i++)if(g->barrels[i].health>0) {
        Vec3 q=g->barrels[i].position;
        if(ray_box(origin,dir,(Box){{q.x-.43f,q.y,q.z-.43f},{q.x+.43f,q.y+1.1f,q.z+.43f}},distance)<distance)return;
    }
    if(distance>=SHOT_RANGE)return;
    Vec3 hit=vadd(origin,vscale(dir,distance));
    if(vlength(vsub(hit,g->player.body.position))>38)return;
    Vec3 normal={0,1,0};int solid=-1;float nearest=SHOT_RANGE;
    for(int i=0;i<g->world->solid_count;i++) {
        float d=ray_box(origin,dir,g->world->solids[i],SHOT_RANGE);
        if(d<nearest&&fabsf(d-distance)<.035f){nearest=d;solid=i;normal=normal_at(g->world->solids[i],hit);}
    }
    Vec3 visible=vadd(hit,vscale(normal,.03f));
    debris(f,app,visible,normal,solid<0?(Color){.48f,.43f,.33f}:(Color){1,.72f,.28f},solid<0?5:7,solid<0?DUST:SPARK);
    if(solid>=0)debris(f,app,visible,normal,(Color){.38f,.45f,.48f},3,SHARD);
    mark(f,g,app,hit,normal,solid);
}
static void cache(Feedback *f,const Game *g) {
    f->enemy_count=g->enemy_count;memcpy(f->enemies,g->enemies,(size_t)g->enemy_count*sizeof(Enemy));
    f->corpse_count=g->corpse_count;memcpy(f->corpses,g->corpses,(size_t)g->corpse_count*sizeof(Enemy));
    for(int id=0;id<MAX_PLAYERS;id++) {
        const Player *p=id==g->local_actor?&g->player:&g->actors[id].player;
        FeedbackActor *a=&f->actors[id];
        a->position=p->body.position;a->fall_speed=p->body.velocity.y;a->weapon=p->weapon;
        a->grounded=p->body.grounded;a->shots=g->actors[id].shots;
        a->kills=g->actors[id].run_kills;a->heads=g->actors[id].run_headshots;a->active=g->actors[id].active;
    }
    f->reactor_cooldown=g->reactor_cooldown;f->wave=g->wave;f->tick=g->tick;
    f->revision=g->world->revision;f->local_actor=g->local_actor;
}
static void advance(Feedback *f,const Game *g,const App *app,float dt) {
    f->impact_cooldown=fmaxf(0,f->impact_cooldown-dt);
    if(f->impact_drawn)f->impact_time=fmaxf(0,f->impact_time-dt);
    bool held=f->hold_time>0;
    f->hold_time=fmaxf(0,f->hold_time-dt);
    if(!app->motion)f->hold_time=0;
    float visual_dt=held&&app->motion?0:dt;
    for(int i=0;i<f->particle_count;) {
        FeedbackParticle *p=&f->particles[i];p->life-=visual_dt;
        if(p->kind!=TRAIL) {
            p->position=vadd(p->position,vscale(p->velocity,visual_dt));p->angle+=p->spin*visual_dt;
            p->velocity.y-=visual_dt*(p->kind==DUST?3:12);
            if(p->position.y<.025f){p->position.y=.025f;p->velocity.y=fabsf(p->velocity.y)*.2f;p->velocity.x*=.5f;p->velocity.z*=.5f;}
        }
        if(p->life<=0)f->particles[i]=f->particles[--f->particle_count];else i++;
    }
    for(int i=0;i<f->mark_count;) {
        FeedbackMark *m=&f->marks[i];m->life-=dt;
        if(m->platform>=0&&m->platform<g->world->platform_count)
            m->position=vadd(platform_position(&g->world->platforms[m->platform],g->world->platform_time),m->offset);
        if(m->life<=0)f->marks[i]=f->marks[--f->mark_count];else i++;
    }
    for(int i=0;i<f->ring_count;) {
        f->rings[i].life-=visual_dt;
        if(f->rings[i].life<=0)f->rings[i]=f->rings[--f->ring_count];else i++;
    }
    f->reactor_pulse=fmaxf(0,f->reactor_pulse-dt);
}
void feedback_update(Feedback *f,const Game *g,const App *app) {
    Uint64 now=SDL_GetPerformanceCounter();
    float dt=f->clock?clampf((float)((double)(now-f->clock)/(double)SDL_GetPerformanceFrequency()),0,.1f):0;
    bool reset=!f->initialized||f->revision!=g->world->revision||g->tick<f->tick||g->wave<f->wave||g->local_actor!=f->local_actor;
    if(reset) {
        memset(f,0,sizeof(*f));f->rng=0x6f65a11u;f->initialized=true;cache(f,g);f->clock=now;return;
    }
    f->clock=now;
    if(app->impact_intensity<=0) {
        f->particle_count=f->mark_count=f->ring_count=0;
        f->impact_time=f->hold_time=f->impact_cooldown=f->reactor_pulse=0;
        cache(f,g);return;
    }
    if(f->particle_count>budget(app))f->particle_count=budget(app);
    if(app->quality==0&&f->mark_count>8)f->mark_count=8;
    if(app->state==STATE_PAUSED&&!g->cooperative)dt=0;
    advance(f,g,app,dt);
    bool active=app->state==STATE_PLAYING&&!g->round_over&&app->impact_intensity>0;
    if(!active){f->impact_time=f->hold_time=0;cache(f,g);return;}
    const FeedbackActor *local=&f->actors[g->local_actor];
    bool killed=g->actors[g->local_actor].run_kills>local->kills;
    bool headshot=g->actors[g->local_actor].run_headshots>local->heads&&g->headshot_time>0;
    const Enemy *deaths[2]={NULL,NULL};int deaths_seen=0;
    for(int i=0;i<g->enemy_count;i++) {
        const Enemy *e=&g->enemies[i],*old=find_enemy(f->enemies,f->enemy_count,e->id);
        if(!old||e->health>=old->health||e->health<=0)continue;
        Vec3 point=vadd(e->body.position,(Vec3){0,1.05f*enemy_size(e),0});
        if(vlength(vsub(point,g->player.body.position))>38)continue;
        float before=old->health/fmaxf(1,old->max_health),after=e->health/fmaxf(1,e->max_health);
        Color c=feedback_enemy_color(e);
        debris(f,app,point,vnormalize(vsub(point,player_eye(&g->player,false))),c,3,SHARD);
        if((before>.7f&&after<=.7f)||(before>.35f&&after<=.35f)) {
            debris(f,app,point,(Vec3){0,.8f,0},c,9,SHARD);
            particle(f,app,point,(Vec3){(random_fx(f)-.5f)*4,3,(random_fx(f)-.5f)*4},c,.8f,.23f*enemy_size(e),SHARD);
        }
        if(old->attack_timer>.12f&&!old->attack_fired&&e->attack_timer==0&&e->attack_fired
            &&e->type!=ENEMY_BOMBER&&!e->elite&&focused(g,point,.82f)) {
            debris(f,app,point,(Vec3){0,.8f,0},(Color){1,.57f,.24f},8,SPARK);
            impact(f,g,app,IMPACT_INTERRUPT,e->id,-1);
        }
    }
    for(int i=0;i<g->corpse_count;i++) {
        const Enemy *e=&g->corpses[i];
        if(find_enemy(f->corpses,f->corpse_count,e->id)||e->death_time>.3f)continue;
        Vec3 point=vadd(e->body.position,(Vec3){0,.9f*enemy_size(e),0});
        if(vlength(vsub(point,g->player.body.position))>48)continue;
        debris(f,app,point,(Vec3){0,1,0},feedback_enemy_color(e),e->elite?18:8,SHARD);
        if(e->elite) {
            Vec3 floor=vadd(e->body.position,(Vec3){0,.04f,0});ring(f,floor,(Color){1,.72f,.26f},6,1);
            if(focused(g,point,.3f))impact(f,g,app,IMPACT_CHAMPION,e->id,-1);
        }
        if(killed&&focused(g,point,.94f)&&deaths_seen<2)deaths[deaths_seen++]=e;
        if(killed&&headshot) {
            Vec3 q=e->body.position;float scale=enemy_size(e),r=.6f*scale;
            float hit=ray_box(player_eye(&g->player,false),look_direction(g->player.yaw,g->player.pitch),
                (Box){{q.x-r,q.y+1.18f*scale,q.z-r},{q.x+r,q.y+ENEMY_HEIGHT*scale,q.z+r}},SHOT_RANGE);
            if(hit<SHOT_RANGE)impact(f,g,app,IMPACT_HEAD,e->id,-1);
        }
    }
    if(deaths_seen==2&&g->player.upgrades[UPGRADE_PIERCE]&&local->weapon<STANDARD_WEAPONS&&g->shot_age<.18f) {
        Vec3 a=vadd(deaths[0]->body.position,(Vec3){0,1,0}),b=vadd(deaths[1]->body.position,(Vec3){0,1,0});
        particle(f,app,a,vsub(b,a),(Color){.78f,.54f,1},.22f,.045f,TRAIL);
        impact(f,g,app,IMPACT_PIERCE,deaths[0]->id,deaths[1]->id);
    }
    for(int id=0;id<MAX_PLAYERS;id++)if(g->actors[id].active&&!g->actors[id].dead) {
        const Player *p=id==g->local_actor?&g->player:&g->actors[id].player;
        FeedbackActor *old=&f->actors[id];old->dust_cooldown=fmaxf(0,old->dust_cooldown-dt);
        if(old->active&&g->actors[id].shots>old->shots)shot_surface(f,g,app,id);
        if(old->active&&!old->grounded&&p->body.grounded&&old->fall_speed<-3)
            debris(f,app,vadd(p->body.position,(Vec3){0,.06f,0}),(Vec3){0,.35f,0},(Color){.44f,.43f,.38f},7,DUST);
        float distance=vlength(vsub(p->body.position,old->position));
        if(app->motion&&p->dash_timer>0&&old->active&&distance>.04f&&distance<3&&old->dust_cooldown==0) {
            Vec3 a=vadd(old->position,(Vec3){0,.6f,0}),b=vadd(p->body.position,(Vec3){0,.6f,0});
            particle(f,app,a,vsub(b,a),teammate_color(id),.18f,.085f,TRAIL);
            debris(f,app,vadd(old->position,(Vec3){0,.06f,0}),(Vec3){0,.2f,0},(Color){.38f,.47f,.5f},2,DUST);
            old->dust_cooldown=.035f;
        }
    }
    if(g->reactor_cooldown>f->reactor_cooldown+.5f&&g->reactor_cooldown>43) {
        f->reactor_pulse=1.2f;ring(f,(Vec3){0,.045f,0},(Color){.35f,1,.65f},32,1.2f);
        ring(f,(Vec3){0,1.87f,0},(Color){.6f,1,.85f},3.1f,.55f);
        debris(f,app,(Vec3){0,4.1f,0},(Vec3){0,1,0},(Color){.4f,1,.7f},16,SPARK);
    }
    cache(f,g);
}
float feedback_impact(const Feedback *f,const App *app) {
    if(!f||f->impact_time<=0||app->state!=STATE_PLAYING)return 0;
    float t=clampf(f->impact_time/fmaxf(.001f,f->impact_duration),0,1);
    return app->impact_intensity*(f->impact_kind==IMPACT_CHAMPION?.85f:.65f)*t;
}
bool feedback_hold_weapon(const Feedback *f,const Game *g,const App *app) {
    return f&&app->motion&&app->impact_intensity>0&&f->hold_time>0&&app->state==STATE_PLAYING
        &&!g->dead&&g->player.weapon==f->held_weapon&&g->player.reload_timer==0
        &&g->player.weapon_switch==0&&g->player.melee_timer==0&&!g->player.upgrade_pending;
}
void feedback_tint(const Feedback *f,Mesh *m,int first,int enemy,float intensity) {
    if(!f||f->impact_time<=0||intensity<=0||(enemy!=f->impact_ids[0]&&enemy!=f->impact_ids[1]))return;
    for(int i=first;i<m->count;i++) {
        Vertex *v=&m->data[i];
        float brightness=v->normal.y<-.2f?.35f:1;
        Color target={brightness,brightness,brightness};
        v->color.r+=(target.r-v->color.r)*intensity;
        v->color.g+=(target.g-v->color.g)*intensity;
        v->color.b+=(target.b-v->color.b)*intensity;v->glow=fmaxf(v->glow,.8f*intensity);
    }
}
void feedback_draw(Feedback *f,Mesh *m,const Game *g,const App *app) {
    if(app->impact_intensity<=0)return;
    int reserve=3000;
    for(int i=0;i<f->particle_count&&m->count<m->capacity-reserve;i++) {
        const FeedbackParticle *p=&f->particles[i];float fade=clampf(p->life/p->duration,0,1);
        Color c={p->color.r*fade,p->color.g*fade,p->color.b*fade};
        m->glow=p->kind==SPARK||p->kind==TRAIL?.8f:0;
        if(p->kind==TRAIL)mesh_cylinder(m,p->position,vadd(p->position,p->velocity),p->size*fade,c,4);
        else if(p->kind==SPARK)mesh_cylinder(m,vsub(p->position,vscale(p->velocity,.025f)),p->position,p->size*fade,c,4);
        else if(p->kind==DUST)mesh_diamond(m,p->position,p->size*(1.3f-fade*.3f)*fminf(1,fade*3),c);
        else {
            int first=m->count;float s=p->size*fminf(1,fade*4);
            mesh_box(m,p->position,(Vec3){s,s*.38f,s*.8f},c);
            mesh_rotate(m,first,p->position,p->angle,p->angle*.7f,0);
        }
    }
    m->glow=0;
    for(int i=0;i<f->mark_count&&m->count<m->capacity-reserve;i++) {
        const FeedbackMark *p=&f->marks[i];float s=p->size*clampf(p->life/.5f,0,1);
        Vec3 u=fabsf(p->normal.y)>.8f?(Vec3){1,0,0}:vnormalize((Vec3){p->normal.z,0,-p->normal.x});
        Vec3 v=cross(p->normal,u);u=vscale(u,s);v=vscale(v,s);
        mesh_quad(m,vsub(vsub(p->position,u),v),vsub(vadd(p->position,u),v),
            vadd(vadd(p->position,u),v),vadd(vsub(p->position,u),v),(Color){.065f,.075f,.07f},p->normal);
    }
    m->glow=.8f;
    for(int i=0;i<f->ring_count&&m->count<m->capacity-reserve;i++) {
        const FeedbackRing *p=&f->rings[i];float fade=clampf(p->life/p->duration,0,1);
        Color c={p->color.r*fade,p->color.g*fade,p->color.b*fade};
        mesh_ring(m,p->position,.3f+(1-fade)*p->radius,.06f+.10f*fade,c,app->quality==0?16:24);
    }
    m->glow=0;(void)g;
}
