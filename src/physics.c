#include "fastfps.h"
#include <math.h>

void move_horizontal(const World *world,Body *b,float dx,float dz,float radius,float height,float step) {
    int steps=(int)ceilf(fmaxf(fabsf(dx),fabsf(dz))/(radius*.6f));if(steps<1)steps=1;
    bool hit_x=false,hit_z=false;
    for(int s=0;s<steps;s++)for(int axis=0;axis<2;axis++) {
        float delta=(axis==0?dx:dz)/(float)steps;if(delta==0)continue;
        float *value=axis==0?&b->position.x:&b->position.z;
        float other=axis==0?b->position.z:b->position.x,target=*value+delta;
        for(int i=0;i<world->solid_count;i++) {
            Box box=world->solids[i];
            if(b->position.y>=box.max.y-.001f||b->position.y+height<=box.min.y+.001f)continue;
            float omin=axis==0?box.min.z:box.min.x,omax=axis==0?box.max.z:box.max.x;
            float amin=axis==0?box.min.x:box.min.z,amax=axis==0?box.max.x:box.max.z;
            if(other+radius<=omin||other-radius>=omax||target+radius<=amin||target-radius>=amax)continue;
            Vec3 stepped=b->position;stepped.y=box.max.y;
            if(axis==0)stepped.x=target;else stepped.z=target;
            if(b->grounded&&box.max.y-b->position.y<=step+.001f
                &&!world_blocked(world,stepped,radius,height,i)) {b->position.y=box.max.y;continue;}
            target=delta>0?amin-radius:amax+radius;
            if(axis==0)hit_x=true;else hit_z=true;
        }
        *value=target;
    }
    if(hit_x)b->velocity.x=0;
    if(hit_z)b->velocity.z=0;
}
void move_vertical(const World *world,Body *b,float dt,float radius,float height) {
    float previous=b->position.y;b->velocity.y-=PLAYER_GRAVITY*dt;
    float target=previous+b->velocity.y*dt;b->grounded=false;
    if(target<=0){target=0;b->velocity.y=0;b->grounded=true;}
    for(int i=0;i<world->solid_count;i++) {
        Box box=world->solids[i];Vec3 p=b->position;
        if(p.x+radius<=box.min.x||p.x-radius>=box.max.x||p.z+radius<=box.min.z||p.z-radius>=box.max.z)continue;
        if(b->velocity.y<=0&&previous>=box.max.y-.001f&&target<=box.max.y) {
            target=box.max.y;b->velocity.y=0;b->grounded=true;
        } else if(b->velocity.y>0&&previous+height<=box.min.y+.001f&&target+height>=box.min.y) {
            target=box.min.y-height;b->velocity.y=0;
        }
    }
    b->position.y=target;
}
static float approach(float value,float target,float step) {
    return value<target?fminf(value+step,target):fmaxf(value-step,target);
}
void update_player(World *world,Player *p,Input input,float dt) {
    Body *b=&p->body;
    p->dash_cooldown=fmaxf(0,p->dash_cooldown-dt);
    p->dash_timer=fmaxf(0,p->dash_timer-dt);
    p->landing+=(0-p->landing)*fminf(1,dt*13);
    p->sway_x*=fmaxf(0,1-dt*11);p->sway_y*=fmaxf(0,1-dt*11);
    float sy=sinf(p->yaw),cy=cosf(p->yaw);
    float wx=input.right*cy+input.forward*sy,wz=input.right*sy-input.forward*cy;
    float length=hypotf(wx,wz);if(length>0){wx/=length;wz/=length;}
    if(input.dash&&p->dash_cooldown==0) {
        if(length==0){wx=sy;wz=-cy;}
        b->velocity.x=wx*DASH_SPEED;b->velocity.z=wz*DASH_SPEED;
        p->dash_timer=DASH_DURATION;p->dash_cooldown=DASH_COOLDOWN;
        audio_play(SOUND_DASH);
    }
    float speed=input.sprint?PLAYER_SPRINT:PLAYER_SPEED;
    float acceleration=b->grounded?PLAYER_ACCELERATION:22;
    if(p->dash_timer==0&&(length>0||b->grounded)) {
        b->velocity.x=approach(b->velocity.x,wx*speed,acceleration*dt);
        b->velocity.z=approach(b->velocity.z,wz*speed,acceleration*dt);
    }
    float current=hypotf(b->velocity.x,b->velocity.z);
    if(p->dash_timer==0&&current>PLAYER_SPRINT) {
        b->velocity.x*=PLAYER_SPRINT/current;b->velocity.z*=PLAYER_SPRINT/current;
    }
    if(b->grounded&&input.jump){b->velocity.y=PLAYER_JUMP;b->grounded=false;audio_play(SOUND_JUMP);}
    move_horizontal(world,b,b->velocity.x*dt,b->velocity.z*dt,PLAYER_RADIUS,PLAYER_HEIGHT,PLAYER_STEP);
    bool was_grounded=b->grounded;float fall_speed=b->velocity.y;
    move_vertical(world,b,dt,PLAYER_RADIUS,PLAYER_HEIGHT);
    if(!was_grounded&&b->grounded&&fall_speed<-2) {
        p->landing=-.11f*clampf(-fall_speed/12,.25f,1);audio_play(SOUND_LAND);
    }
    p->bob+=hypotf(b->velocity.x,b->velocity.z)*dt*1.5f;
    float blend=fminf(1,dt*10);
    p->movement_blend+=(clampf(hypotf(b->velocity.x,b->velocity.z)/PLAYER_SPRINT,0,1)-p->movement_blend)*blend;
    float strafe=(b->velocity.x*cy+b->velocity.z*sy)/PLAYER_SPRINT;
    p->strafe_blend+=(strafe-p->strafe_blend)*blend;
    p->step_timer-=dt;
    if(b->grounded&&hypotf(b->velocity.x,b->velocity.z)>2&&p->step_timer<=0) {
        audio_play(SOUND_STEP);
        p->step_timer=clampf(3.6f/hypotf(b->velocity.x,b->velocity.z),.18f,.5f);
    }
}
Vec3 player_eye(const Player *p,bool motion) {
    float bob=motion&&p->body.grounded?sinf(p->bob*2)*p->movement_blend*.018f:0;
    return vadd(p->body.position,(Vec3){0,PLAYER_EYE+bob+(motion?p->landing:0),0});
}
