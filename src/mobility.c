#include "fastfps.h"
#include <math.h>

Box platform_box(const Platform *p,float time) {
    /* Stop at each dock for 16% of the cycle, with smooth acceleration. */
    float phase=fmodf(time/p->period,1);if(phase<0)phase+=1;
    float amount;
    if(phase<.16f)amount=0;
    else if(phase<.5f)amount=(phase-.16f)/.34f;
    else if(phase<.66f)amount=1;
    else amount=1-(phase-.66f)/.34f;
    amount=amount*amount*(3-2*amount);
    Vec3 center=vadd(p->base,vscale(p->travel,amount));
    return (Box){{center.x-p->size.x/2,center.y,center.z-p->size.z/2},
        {center.x+p->size.x/2,center.y+p->size.y,center.z+p->size.z/2}};
}
Vec3 platform_velocity(const Platform *p,float time) {
    Box a=platform_box(p,time-FIXED_STEP*.5f),b=platform_box(p,time+FIXED_STEP*.5f);
    return vscale(vsub(b.min,a.min),1/FIXED_STEP);
}
void world_platforms_update(World *w,float time) {
    for(int i=0;i<w->platform_count;i++)w->solids[w->platforms[i].solid]=platform_box(&w->platforms[i],time);
    w->platform_time=time;
}
static void carry(World *w,Body *b,Box before,Box after,int solid,float radius,float height) {
    bool rider=b->velocity.y<=0&&b->position.y>=before.max.y-.065f&&b->position.y<=before.max.y+.065f
        &&b->position.x>=before.min.x&&b->position.x<=before.max.x&&b->position.z>=before.min.z&&b->position.z<=before.max.z;
    if(rider) {
        Vec3 target=vadd(b->position,vsub(after.min,before.min));target.y=after.max.y;
        if(!world_blocked(w,target,radius,height,solid)){b->position=target;b->grounded=true;return;}
    }
    Vec3 p=b->position;
    if(p.y>=after.max.y-.001f||p.y+height<=after.min.y+.001f||p.x+radius<=after.min.x||p.x-radius>=after.max.x
        ||p.z+radius<=after.min.z||p.z-radius>=after.max.z)return;
    /* Une plaque qui descend ne peut pas enfermer un joueur sous sa surface. */
    Vec3 exits[4]={{after.min.x-radius-.03f,p.y,p.z},{after.max.x+radius+.03f,p.y,p.z},
        {p.x,p.y,after.min.z-radius-.03f},{p.x,p.y,after.max.z+radius+.03f}};
    float best=1e9f;int chosen=-1;
    for(int k=0;k<4;k++) {
        float distance=vlength(vsub(exits[k],p));
        if(distance<best&&!world_blocked(w,exits[k],radius,height,-1)){best=distance;chosen=k;}
    }
    if(chosen>=0){b->position=exits[chosen];b->velocity.x=b->velocity.z=0;}

}
void game_prepare_mobility(Game *g,float dt) {
    World *w=g->world;Box old[MAX_PLATFORMS];
    for(int k=0;k<w->platform_count;k++)old[k]=w->solids[w->platforms[k].solid];
    world_platforms_update(w,g->time+dt);
    for(int k=0;k<w->platform_count;k++) {
        int solid=w->platforms[k].solid;Box next=w->solids[solid];
        for(int id=0;id<MAX_PLAYERS;id++)if(g->actors[id].active&&(!g->prediction||id==g->current_actor)) {
            Player *p=id==g->current_actor?&g->player:&g->actors[id].player;
            carry(w,&p->body,old[k],next,solid,PLAYER_RADIUS,PLAYER_HEIGHT);
        }
        if(g->prediction)continue;
        for(int i=0;i<g->enemy_count;i++)if(g->enemies[i].health>0) {
            Enemy *e=&g->enemies[i];float scale=enemy_size(e);
            carry(w,&e->body,old[k],next,solid,ENEMY_RADIUS*scale,ENEMY_HEIGHT*scale);
        }
        for(int i=0;i<g->corpse_count;i++)carry(w,&g->corpses[i].body,old[k],next,solid,.4f,.2f);
    }
}
