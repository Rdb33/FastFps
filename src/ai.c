#include "fastfps.h"
#include <math.h>

static const Player *player(const Game *g,int id){return id==g->current_actor?&g->player:&g->actors[id].player;}
static bool living(const Game *g,int id){return g->actors[id].active&&!(id==g->current_actor?g->dead:g->actors[id].dead);}
int ai_target(Game *g,const Enemy *e) {
    int best=-1;float score=1e9f;
    Vec3 eye=vadd(e->body.position,(Vec3){0,1.3f*enemy_size(e),0});
    for(int id=0;id<MAX_PLAYERS;id++)if(living(g,id)) {
        Vec3 delta=vsub(player_eye(player(g,id),false),eye);float distance=vlength(delta);
        bool visible=world_ray(g->world,eye,vnormalize(delta),distance)>=distance-.05f;
        float priority=distance+(visible?0:7)-(id==e->target_actor?2.5f:0);
        if(g->actors[id].revive_target>=0)priority-=2;
        if(priority<score){score=priority;best=id;}
    }
    return best;
}
Vec3 ai_move(Game *g,Enemy *e,const Player *p,bool visible,float dt) {
    Vec3 target=p->body.position;
    Vec3 delta=vsub(target,e->body.position);delta.y=0;float distance=vlength(delta);
    Vec3 forward=vnormalize(delta),side={-forward.z,0,forward.x};
    float sign=e->id%2?1:-1;
    if(!visible)target=navigation_for_actor(g->world,e->target_actor,e->body.position.x,e->body.position.z);
    else if(e->type==ENEMY_CASTER) {
        if(distance<7)target=vadd(e->body.position,vadd(vscale(forward,-3),vscale(side,sign*2)));
        else if(distance<14)target=vadd(e->body.position,vscale(side,sign*3));
    }else if(e->type==ENEMY_RUNNER&&distance>3&&distance<18) {
        float lead=clampf(distance/20,0,.45f);
        target=vadd(target,(Vec3){p->body.velocity.x*lead,0,p->body.velocity.z*lead});
        target=vadd(target,vscale(side,sign*fminf(3,distance*.22f)));
    }
    Vec3 move=vsub(target,e->body.position);move.y=0;move=vnormalize(move);
    /* Sonde au sol : les passages étroits et les élévateurs restent des obstacles réels. */
    float radius=ENEMY_RADIUS*enemy_size(e),height=ENEMY_HEIGHT*enemy_size(e);
    Vec3 probe=vadd(e->body.position,vscale(move,1.0f));
    if(world_blocked(g->world,probe,radius,height,-1)||e->stuck_time>.3f) {
        Vec3 route=navigation_for_actor(g->world,e->target_actor,e->body.position.x,e->body.position.z);
        Vec3 choices[4]={vnormalize(vsub(route,e->body.position)),vscale(side,sign),vscale(side,-sign),vscale(move,-1)};
        for(int k=0;k<4;k++) {
            choices[k].y=0;probe=vadd(e->body.position,vscale(choices[k],.9f));
            if(vlength(choices[k])>.1f&&!world_blocked(g->world,probe,radius,height,-1)){move=choices[k];break;}
        }
        if(e->stuck_time>1.5f){e->stuck_time=0;e->shot_timer=fmaxf(e->shot_timer,.3f);}
    }
    Vec3 separation={0,0,0};
    for(int i=0;i<g->enemy_count;i++)if(&g->enemies[i]!=e&&g->enemies[i].health>0) {
        Vec3 apart=vsub(e->body.position,g->enemies[i].body.position);apart.y=0;float d=vlength(apart);
        if(d>.01f&&d<2.3f)separation=vadd(separation,vscale(apart,(2.3f-d)/d*.35f));
    }
    (void)dt;return vnormalize(vadd(move,separation));
}
