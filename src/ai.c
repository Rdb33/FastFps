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
        if(e->type==ENEMY_RUNNER||e->type==ENEMY_BRUTE)priority+=fabsf(delta.y)*1.8f;
        int pressure=0;
        for(int k=0;k<g->enemy_count;k++)if(&g->enemies[k]!=e&&g->enemies[k].health>0
            &&g->enemies[k].target_actor==id)pressure++;
        priority+=fminf(4,(float)pressure*.65f);
        if(g->actors[id].revive_target>=0)priority-=2;
        if(priority<score){score=priority;best=id;}
    }
    return best;
}
/* Vérifie le volume réel et le trajet, y compris après séparation du groupe.
   Huit candidats de secours maximum ; aucun nouveau graphe ni allocation. */
static bool safe_step(const Game *g,const Enemy *e,const Player *p,Vec3 move,bool visible) {
    if(vlength(move)<.1f)return false;
    float r=ENEMY_RADIUS*enemy_size(e),h=ENEMY_HEIGHT*enemy_size(e);
    Body probe=e->body;Vec3 wanted=vadd(probe.position,vscale(move,.9f));
    move_horizontal(g->world,&probe,move.x*.9f,move.z*.9f,r,h,PLAYER_STEP);
    if(hypotf(probe.position.x-wanted.x,probe.position.z-wanted.z)>.08f)return false;
    if(world_blocked(g->world,probe.position,r,h,-1))return false;
    if(e->body.grounded&&e->body.position.y>.8f) {
        bool ranged=e->type==ENEMY_CASTER||e->type==ENEMY_WEAVER||e->type==ENEMY_BOMBER;
        float drop=ranged&&visible?.75f:(p->body.position.y<e->body.position.y-1?2.5f:1.2f);
        Vec3 side={-move.z*r*.7f,0,move.x*r*.7f};
        for(int k=-1;k<=1;k++) {
            Vec3 foot=vadd(probe.position,vscale(side,(float)k));foot.y+=.08f;
            if(world_ray(g->world,foot,(Vec3){0,-1,0},drop+.2f)>drop+.08f)return false;
        }
    }
    return true;
}
bool ai_can_fire(const Game *g,const Enemy *e,Vec3 aim) {
    Vec3 origin=vadd(e->body.position,(Vec3){0,1.3f*enemy_size(e),0});
    Vec3 delta=vsub(aim,origin);float distance=vlength(delta);
    if(distance<.05f)return false;
    Vec3 dir=vscale(delta,1/distance);
    if(world_ray(g->world,origin,dir,distance)<distance-.05f)return false;
    for(int i=0;i<g->enemy_count;i++) {
        const Enemy *other=&g->enemies[i];if(other==e||other->health<=0)continue;
        float r=ENEMY_RADIUS*enemy_size(other)+.12f;Vec3 q=other->body.position;
        if(ray_box(origin,dir,(Box){{q.x-r,q.y,q.z-r},{q.x+r,q.y+ENEMY_HEIGHT*enemy_size(other),q.z+r}},distance)<distance-.1f)return false;
    }
    return true;
}
Vec3 ai_move(Game *g,Enemy *e,const Player *p,bool visible,float dt) {
    Vec3 target=p->body.position;
    Vec3 delta=vsub(target,e->body.position);delta.y=0;float distance=vlength(delta);
    Vec3 forward=vnormalize(delta),side={-forward.z,0,forward.x};
    float sign=e->id%2?1:-1;
    if(!visible)target=navigation_for_actor(g->world,e->target_actor,e->body.position.x,e->body.position.z);
    else if(e->type==ENEMY_BOMBER) {
        if(distance<15)target=vadd(e->body.position,vadd(vscale(forward,-4),vscale(side,sign*2)));
        else if(distance<30)target=vadd(e->body.position,vscale(side,sign*2));
    }
    else if(e->type==ENEMY_CASTER||e->type==ENEMY_WEAVER) {
        if(distance<7)target=vadd(e->body.position,vadd(vscale(forward,-3),vscale(side,sign*2)));
        else if(distance<14)target=vadd(e->body.position,vscale(side,sign*3));
    }else if(e->type==ENEMY_RUNNER&&distance>3&&distance<18) {
        float lead=clampf(distance/20,0,.45f);
        target=vadd(target,(Vec3){p->body.velocity.x*lead,0,p->body.velocity.z*lead});
        target=vadd(target,vscale(side,sign*fminf(3,distance*.22f)));
    }
    Vec3 move=vsub(target,e->body.position);move.y=0;move=vnormalize(move);
    Vec3 separation={0,0,0};
    for(int i=0;i<g->enemy_count;i++)if(&g->enemies[i]!=e&&g->enemies[i].health>0) {
        Vec3 apart=vsub(e->body.position,g->enemies[i].body.position);apart.y=0;float d=vlength(apart);
        if(fabsf(e->body.position.y-g->enemies[i].body.position.y)<1.5f&&d>.01f&&d<2.3f)separation=vadd(separation,vscale(apart,(2.3f-d)/d*.35f));
    }
    Vec3 preferred=vnormalize(vadd(move,separation));
    bool ranged=e->type==ENEMY_CASTER||e->type==ENEMY_WEAVER||e->type==ENEMY_BOMBER;
    bool cover=ranged&&visible&&e->shot_timer>1.2f&&distance<30;
    if(visible&&!cover&&e->stuck_time<.3f&&safe_step(g,e,p,preferred,visible))return preferred;
    Vec3 route=navigation_for_actor(g->world,e->target_actor,e->body.position.x,e->body.position.z);
    route.y=e->body.position.y;route=vnormalize(vsub(route,e->body.position));
    Vec3 best={0,0,0};float best_score=-1e9f;
    Vec3 momentum=vnormalize((Vec3){e->body.velocity.x,0,e->body.velocity.z});
    for(int k=0;k<8;k++) {
        float angle=(float)((k+1)/2)*PI_F/4*(k%2?sign:-sign);
        Vec3 choice=k==0?route:(Vec3){preferred.x*cosf(angle)-preferred.z*sinf(angle),0,
            preferred.x*sinf(angle)+preferred.z*cosf(angle)};
        if(!safe_step(g,e,p,choice,visible))continue;
        float score=(choice.x*preferred.x+choice.z*preferred.z)*3
            +(choice.x*momentum.x+choice.z*momentum.z)*.65f;
        if(!visible||e->stuck_time>.3f)score+=(choice.x*route.x+choice.z*route.z)*2;
        if(!visible||cover) {
            Vec3 eye=vadd(e->body.position,vadd(vscale(choice,.9f),(Vec3){0,1.3f*enemy_size(e),0}));
            Vec3 sight=vsub(player_eye(p,false),eye);float range=vlength(sight);
            bool clear=world_ray(g->world,eye,vnormalize(sight),range)>=range-.05f;
            score+=cover?(clear?0:2.5f):(clear?2:0);
        }
        if(score>best_score){best_score=score;best=choice;}
    }
    (void)dt;return best;
}
