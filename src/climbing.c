#include "fastfps.h"
#include <math.h>

bool enemy_try_climb(Game *g,Enemy *e,Vec3 direction) {
    if(e->elite||e->type==ENEMY_BRUTE||e->type==ENEMY_BOMBER||!e->body.grounded
        ||e->climb_timer>0||e->attack_timer>0||e->spawn_time>0||e->stagger>0)return false;
    direction.y=0;direction=vnormalize(direction);if(vlength(direction)<.5f)return false;
    float r=ENEMY_RADIUS*enemy_size(e),h=ENEMY_HEIGHT*enemy_size(e);
    Vec3 p=e->body.position,probe=vadd(p,vscale(direction,r+.38f));
    for(int i=0;i<g->world->solid_count;i++) {
        Box b=g->world->solids[i];float rise=b.max.y-p.y;
        if(rise<.16f||rise>1.15f||b.max.x-b.min.x<2*r+.12f||b.max.z-b.min.z<2*r+.12f)continue;
        if(probe.x+r<=b.min.x||probe.x-r>=b.max.x||probe.z+r<=b.min.z||probe.z-r>=b.max.z)continue;
        Vec3 top={clampf(probe.x,b.min.x+r+.04f,b.max.x-r-.04f),b.max.y,
            clampf(probe.z,b.min.z+r+.04f,b.max.z-r-.04f)};
        if(hypotf(top.x-p.x,top.z-p.z)>1.65f)continue;
        Vec3 lifted={p.x,top.y+.02f,p.z};
        if(world_blocked(g->world,lifted,r,h,i)||world_blocked(g->world,top,r,h,i))continue;
        /* Tester tout le passage au-dessus du rebord, pas seulement l'arrivée. */
        bool clear=true;
        for(int k=1;k<=4;k++) {
            Vec3 q=vadd(lifted,vscale(vsub(top,lifted),(float)k/4));
            if(world_blocked(g->world,q,r,h,i)){clear=false;break;}
        }
        if(!clear)continue;
        e->climb_start=p;e->climb_target=vsub(top,b.min);e->climb_solid=i;
        e->climb_timer=ENEMY_CLIMB_TIME;e->body.velocity=(Vec3){0,0,0};
        e->yaw=atan2f(direction.x,-direction.z);e->movement_blend=0;return true;
    }
    return false;
}
bool enemy_update_climb(Game *g,Enemy *e,float dt) {
    if(e->climb_timer<=0)return false;
    int solid=e->climb_solid;
    if(solid<0||solid>=g->world->solid_count){e->climb_timer=0;return false;}
    Vec3 top=vadd(g->world->solids[solid].min,e->climb_target);
    float next=fmaxf(0,e->climb_timer-dt),t=1-next/ENEMY_CLIMB_TIME;
    float lift=clampf((t-.2f)/.4f,0,1),across=clampf((t-.6f)/.4f,0,1);
    lift=lift*lift*(3-2*lift);across=across*across*(3-2*across);
    Vec3 q={e->climb_start.x+(top.x-e->climb_start.x)*across,
        e->climb_start.y+(top.y-e->climb_start.y)*lift,
        e->climb_start.z+(top.z-e->climb_start.z)*across};
    if(world_blocked(g->world,q,ENEMY_RADIUS*enemy_size(e),ENEMY_HEIGHT*enemy_size(e),solid)) {
        e->climb_timer=0;return false;
    }
    e->body.position=q;e->body.velocity=(Vec3){0,0,0};e->body.grounded=next==0;
    e->climb_timer=next;e->stuck_time=0;return true;
}
