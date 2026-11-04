#include "fastfps.h"
#include <math.h>
#include <string.h>

static const Player *actor(const Game *g,int id){return id==g->current_actor?&g->player:&g->actors[id].player;}
static bool alive(const Game *g,int id){return g->actors[id].active&&!(id==g->current_actor?g->dead:g->actors[id].dead);}
void ai_reset(World *w){memset(&w->ai,0,sizeof(w->ai));}
AiBrain *ai_brain(Game *g,const Enemy *e) {
    AiState *a=&g->world->ai;
    for(int i=0;i<MAX_ENEMIES;i++)if(a->brains[i].used&&a->brains[i].id==e->id)return &a->brains[i];
    for(int i=0;i<MAX_ENEMIES;i++)if(!a->brains[i].used) {
        AiBrain *b=&a->brains[i];*b=(AiBrain){.used=true,.id=e->id,.target=-1,.role=AI_SEARCH,.plan_at=-1};return b;
    }
    return NULL;
}
void ai_update(Game *g,float dt) {
    if(g->prediction)return;
    AiState *a=&g->world->ai;
    if(!a->initialized){a->initialized=true;ai_nav_build(g->world);}
    else if(a->revision!=g->world->revision) {
        ai_nav_build(g->world);
        for(int id=0;id<MAX_PLAYERS;id++){a->contacts[id].known=false;a->contacts[id].confidence=0;}
    }
    for(int k=0;k<MAX_ENEMIES;k++)if(a->brains[k].used) {
        bool found=false;for(int i=0;i<g->enemy_count;i++)if(g->enemies[i].id==a->brains[k].id&&g->enemies[i].health>0)found=true;
        if(!found)a->brains[k].used=false;
    }
    if(g->time<a->next_observation)return;
    float interval=fmaxf(.05f,g->time-a->next_observation+.12f);a->next_observation=g->time+.12f;
    for(int id=0;id<MAX_PLAYERS;id++) {
        AiContact *c=&a->contacts[id];if(!alive(g,id)){c->known=false;c->confidence=0;continue;}
        const Player *p=actor(g,id);Vec3 eye=player_eye(p,false);bool seen=false;
        for(int i=0;i<g->enemy_count&&!seen;i++)if(g->enemies[i].health>0&&g->enemies[i].spawn_time<=0) {
            Enemy *e=&g->enemies[i];Vec3 origin=vadd(e->body.position,(Vec3){0,1.3f*enemy_size(e),0}),d=vsub(eye,origin);float range=vlength(d);
            seen=range<48&&world_ray(g->world,origin,vnormalize(d),range)>=range-.05f;
        }
        if(!seen){c->confidence=fmaxf(0,c->confidence-interval*.14f);continue;}
        bool continuous=c->known&&g->time-c->seen_at<.4f;
        Vec3 velocity=p->body.velocity;
        if(continuous) {
            Vec3 old=c->history[(c->cursor+AI_HISTORY-1)%AI_HISTORY];
            float speed=hypotf(velocity.x,velocity.z),prior=hypotf(old.x,old.z);
            bool reversal=speed>1.5f&&prior>1.5f&&(old.x*velocity.x+old.z*velocity.z)/(speed*prior)<-.2f;
            c->reversals=c->reversals*.88f+(reversal? .12f:0);
            c->acceleration=vscale(vsub(velocity,c->velocity),1/fmaxf(.05f,g->time-c->sample_at));
            float magnitude=vlength(c->acceleration);if(magnitude>18)c->acceleration=vscale(c->acceleration,18/magnitude);
            c->velocity=vadd(vscale(c->velocity,.4f),vscale(velocity,.6f));
        }else {c->velocity=velocity;c->acceleration=(Vec3){0,0,0};c->reversals=0;c->samples=0;c->cursor=0;}
        float lateral=velocity.x*cosf(p->yaw)+velocity.z*sinf(p->yaw);
        c->strafe_bias=c->strafe_bias*.92f+clampf(lateral/8,-1,1)*.08f;
        c->jump_bias=c->jump_bias*.94f+(!p->body.grounded?.06f:0);
        c->history[c->cursor]=velocity;c->cursor=(c->cursor+1)%AI_HISTORY;if(c->samples<AI_HISTORY)c->samples++;
        c->position=p->body.position;c->sample_at=c->seen_at=g->time;c->known=true;c->confidence=fminf(1,c->confidence+.2f);
    }
    (void)dt;
}
Vec3 ai_predict(const Game *g,const Enemy *e,const Player *p,float projectile_speed) {
    Vec3 origin=vadd(e->body.position,(Vec3){0,1.3f*enemy_size(e),0});
    const AiContact *c=e->target_actor>=0&&e->target_actor<MAX_PLAYERS?&g->world->ai.contacts[e->target_actor]:NULL;
    float horizon=clampf(vlength(vsub(player_eye(p,false),origin))/projectile_speed,0,.85f);
    if(!c||!c->known||c->samples<3||g->time-c->seen_at>.4f)
        return vadd(player_eye(p,false),vscale(p->body.velocity,horizon*.45f));
    Vec3 average={0,0,0};for(int i=0;i<c->samples;i++)average=vadd(average,c->history[i]);average=vscale(average,1/(float)c->samples);
    /* Les changements de direction fréquents réduisent la poursuite d'une
       vitesse instantanée. Une trajectoire régulière autorise davantage de lead. */
    float zigzag=clampf(c->reversals*3,0,.8f);
    Vec3 velocity=vadd(vscale(c->velocity,1-zigzag),vscale(average,zigzag));
    float lead=horizon*(.55f+.3f*c->confidence);
    Vec3 offset=vadd(vscale(velocity,lead),vscale(c->acceleration,.15f*lead*lead));
    if(c->jump_bias>.2f)offset.y=clampf(velocity.y*lead-PLAYER_GRAVITY*lead*lead*.35f,-.65f,1.2f);
    float length=vlength(offset);if(length>5)offset=vscale(offset,5/length);
    return vadd(player_eye(p,false),offset);
}
