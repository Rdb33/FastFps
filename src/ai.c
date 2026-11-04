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
        const AiContact *contact=&g->world->ai.contacts[id];
        bool informed=contact->known&&g->time-contact->seen_at<8;
        if(g->world->ai.initialized&&!visible) {
            delta=informed?vsub(contact->position,e->body.position):(Vec3){0,0,0};
            distance=informed?vlength(delta):45+(float)((id+e->id)%MAX_PLAYERS);
        }
        float priority=distance+(visible?0:7)-(id==e->target_actor?3.5f:0);
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
            if(world_ray(g->world,foot,(Vec3){0,-1,0},drop+.2f)>drop+.08f) {
                foot=vadd(foot,vscale(move,r*.85f));
                if(world_ray(g->world,foot,(Vec3){0,-1,0},drop+.2f)>drop+.08f)return false;
            }
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
static Vec3 steer(Game *g,Enemy *e,const Player *p,bool visible,float dt,Vec3 requested,bool planned) {
    Vec3 target=p->body.position;
    Vec3 delta=vsub(target,e->body.position);delta.y=0;float distance=vlength(delta);
    Vec3 forward=vnormalize(delta),side={-forward.z,0,forward.x};
    float sign=e->id%2?1:-1;
    if(planned)target=requested;
    else if(!visible)target=navigation_for_actor(g->world,e->target_actor,e->body.position.x,e->body.position.z);
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
    Vec3 avoidance={0,0,0};
    for(int i=0;i<g->projectile_count;i++) {
        const Projectile *rocket=&g->projectiles[i];if(rocket->kind!=1||rocket->owner<0)continue;
        Vec3 center=vadd(e->body.position,(Vec3){0,.8f*enemy_size(e),0}),to=vsub(center,rocket->position);
        float speed2=rocket->velocity.x*rocket->velocity.x+rocket->velocity.z*rocket->velocity.z;
        if(speed2<1||vlength(to)>10)continue;
        float arrival=(to.x*rocket->velocity.x+to.z*rocket->velocity.z)/speed2;
        if(arrival<=0||arrival>.65f)continue;
        float range=vlength(to);
        if(world_ray(g->world,center,vnormalize(vscale(to,-1)),range)<range-.1f)continue;
        Vec3 miss=vsub(center,vadd(rocket->position,vscale(rocket->velocity,arrival)));miss.y=0;
        if(vlength(miss)>3)continue;
        if(vlength(miss)<.2f)miss=(Vec3){-rocket->velocity.z*sign,0,rocket->velocity.x*sign};
        avoidance=vadd(avoidance,vscale(vnormalize(miss),2));
    }
    Vec3 preferred=vnormalize(vadd(vadd(move,separation),avoidance));
    bool ranged=e->type==ENEMY_CASTER||e->type==ENEMY_WEAVER||e->type==ENEMY_BOMBER;
    bool cover=ranged&&visible&&e->shot_timer>1.2f&&distance<30;
    if(visible&&!cover&&e->stuck_time<.3f&&safe_step(g,e,p,preferred,visible))return preferred;
    Vec3 route=planned?requested:navigation_for_actor(g->world,e->target_actor,e->body.position.x,e->body.position.z);
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

/* Choix tactique collectif, puis chemin global, puis évitement local. */
static Vec3 tactical_goal(Game *g,Enemy *e,AiBrain *brain,const Player *p,bool visible) {
    AiState *a=&g->world->ai;AiContact *contact=&a->contacts[e->target_actor];
    Vec3 target=p->body.position;
    if(!visible) {
        brain->role=AI_SEARCH;
        if(contact->known&&g->time-contact->seen_at<8) {
            float age=g->time-contact->seen_at;
            target=vadd(contact->position,vscale(contact->velocity,fminf(age,.6f)*.5f));
            if(hypotf(target.x-e->body.position.x,target.z-e->body.position.z)<2) {
                float angle=(float)(e->id%8)*PI_F/4+floorf(g->time/3)*PI_F*.5f;
                target=vadd(contact->position,(Vec3){cosf(angle)*3,0,sinf(angle)*3});
            }
        }else {
            static const Vec3 crossings[]={{0,0,16},{0,0,-16},{-12,0,0},{12,0,0}};
            int sector=(e->id+(int)(g->time/5))%12;
            target=sector<8?g->world->spawns[sector]:crossings[sector-8];
        }
        return target;
    }
    bool ranged=e->type==ENEMY_CASTER||e->type==ENEMY_BOMBER||e->type==ENEMY_WEAVER;
    float distance=vlength(vsub(target,e->body.position));int pressure=0;bool frontline=false;
    for(int i=0;i<g->enemy_count;i++) {
        const Enemy *other=&g->enemies[i];if(other==e||other->health<=0||other->target_actor!=e->target_actor)continue;
        float near=vlength(vsub(other->body.position,target));
        if(other->type==ENEMY_RUNNER||other->type==ENEMY_BRUTE) {
            if(near<10)frontline=true;
            if(near<distance-.5f||(fabsf(near-distance)<=.5f&&other->id<e->id))pressure++;
        }
    }
    if(ranged)brain->role=(distance<5||e->health<e->max_health*.3f||(e->shot_timer>1.2f&&frontline))?AI_COVER:AI_SUPPORT;
    else brain->role=e->elite||pressure<2?AI_PRESSURE:AI_FLANK;
    Vec3 toward=vnormalize((Vec3){e->body.position.x-target.x,0,e->body.position.z-target.z});
    if(vlength(toward)<.1f)toward=(Vec3){1,0,0};
    Vec3 side={-toward.z,0,toward.x};float sign=e->id%2?1:-1;
    if(brain->role==AI_FLANK&&contact->samples>=6&&contact->confidence>.5f
        &&fabsf(contact->strafe_bias)>.25f&&e->id%3!=0) {
        float habitual_exit=contact->velocity.x*side.x+contact->velocity.z*side.z;
        if(fabsf(habitual_exit)>.5f)sign=habitual_exit>0?1:-1;
    }
    float range=e->type==ENEMY_BOMBER?20:(e->type==ENEMY_WEAVER?13:11);
    if(brain->role==AI_PRESSURE) {
        float lead=fminf(.55f,distance/20);target=vadd(target,vscale(contact->velocity,lead*.65f));
        return vadd(target,vscale(side,sign*fminf(1.3f,distance*.1f)));
    }
    Vec3 ideal;
    if(brain->role==AI_FLANK)ideal=vadd(target,vadd(vscale(toward,3),vscale(side,sign*4)));
    else ideal=vadd(target,vadd(vscale(toward,brain->role==AI_COVER?range+3:range),vscale(side,sign*2)));
    int candidates[12];float scores[12];for(int i=0;i<12;i++){candidates[i]=-1;scores[i]=1e9f;}
    for(int n=0;n<a->node_count;n++) {
        const AiNode *node=&a->nodes[n];if(!(node->mask&(1<<ai_profile(e)))||node->platform>=0)continue;
        float travel=vlength(vsub(node->position,e->body.position));if(travel>15)continue;
        float score=vlength(vsub(node->position,ideal))+travel*.3f;
        float height=node->position.y-e->body.position.y;
        if(ranged)score+=height>1.2f?height*2:fabsf(height)*.4f;
        for(int i=0;i<g->enemy_count;i++)if(g->enemies[i].id!=e->id&&g->enemies[i].health>0) {
            float gap=vlength(vsub(node->position,g->enemies[i].body.position));if(gap<2.5f)score+=(2.5f-gap)*2;
        }
        for(int j=0;j<MAX_ENEMIES;j++) {
            const AiBrain *other=&a->brains[j];if(!other->used||other->id==e->id||other->path_count<=0)continue;
            float gap=vlength(vsub(node->position,other->goal));if(gap<3)score+=(3-gap)*1.1f;
        }
        for(int k=0;k<12;k++)if(score<scores[k]) {
            for(int j=11;j>k;j--){scores[j]=scores[j-1];candidates[j]=candidates[j-1];}
            scores[k]=score;candidates[k]=n;break;
        }
    }
    Vec3 best=ideal;float best_score=1e9f;
    for(int k=0;k<12;k++)if(candidates[k]>=0) {
        Vec3 point=a->nodes[candidates[k]].position,eye=vadd(point,(Vec3){0,1.3f*enemy_size(e),0});
        Vec3 sight=vsub(player_eye(p,false),eye);float length=vlength(sight);
        bool clear=world_ray(g->world,eye,vnormalize(sight),length)>=length-.05f;
        float score=scores[k]+(brain->role==AI_COVER?(clear?6:0):(clear?0:7));
        if(score<best_score){best_score=score;best=point;}
    }
    return best;
}
Vec3 ai_move(Game *g,Enemy *e,const Player *p,bool visible,float dt) {
    AiState *a=&g->world->ai;
    if(!a->initialized)return steer(g,e,p,visible,dt,p->body.position,false);
    AiBrain *b=ai_brain(g,e);if(!b)return (Vec3){0,0,0};
    if(b->target!=e->target_actor){b->target=e->target_actor;b->decide_at=0;b->plan_at=-1;b->path_count=0;}
    if(g->time>=b->decide_at&&!b->riding) {
        Vec3 goal=tactical_goal(g,e,b,p,visible);
        if(vlength(vsub(goal,b->goal))>2)b->plan_at=-1;
        b->goal=goal;b->decide_at=g->time+.4f+(float)(e->id%4)*.05f;
    }
    if(!b->riding&&(g->time>=b->plan_at||b->revision!=a->revision))ai_plan(g,e,b,b->goal);
    Vec3 point=ai_waypoint(g,e,b),direction=vsub(point,e->body.position);
    if(vlength(direction)<.25f)return (Vec3){0,0,0};
    if(point.y>e->body.position.y+PLAYER_STEP&&hypotf(direction.x,direction.z)<2.3f
        &&enemy_try_climb(g,e,direction))return (Vec3){0,0,0};
    Player perceived=*p;
    if(!visible){perceived.body.position=point;perceived.body.velocity=(Vec3){0,0,0};}
    return steer(g,e,&perceived,visible&&point.y>=e->body.position.y-.7f,dt,point,true);
}
