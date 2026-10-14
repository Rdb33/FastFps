#include "fastfps.h"
#include <math.h>

Color teammate_color(int id) {
    static const Color colors[MAX_PLAYERS]={{.22f,.72f,.92f},{.30f,.82f,.55f},{.42f,.55f,.95f},{.94f,.73f,.25f}};
    return colors[id>=0&&id<MAX_PLAYERS?id:0];
}
/* Rotate a body section around its joint; positions and normals stay in the
 * existing dynamic mesh. No textures, extra buffers or drawing passes. */
static void joint_pose(Mesh *m,int first,Vec3 pivot,float pitch,float roll) {
    mesh_rotate(m,first,pivot,pitch,0,roll);
}
static void badge(Mesh *m,int id,Vec3 position,float yaw,Color color) {
    int first=m->count;char text[2]={(char)('1'+id),0};
    float glow=m->glow;m->glow=.45f;mesh_text(m,-3,0,1,color,text);
    for(int i=first;i<m->count;i++){m->data[i].position.y=-m->data[i].position.y;m->data[i].normal=(Vec3){0,0,1};}
    for(int i=first;i+2<m->count;i+=3){Vertex swap=m->data[i+1];m->data[i+1]=m->data[i+2];m->data[i+2]=swap;}
    mesh_pose(m,first,position,yaw,0,.018f);m->glow=glow;
}
static float held_weapon(Mesh *m,int weapon,int sides,Color accent,ReloadPose reload) {
    Color dark={.10f,.15f,.18f},metal={.46f,.52f,.54f};
    mesh_beveled_box(m,(Vec3){0,-.04f,0},(Vec3){.22f,.17f,.35f},.035f,dark);
    mesh_box(m,(Vec3){0,-.16f,.09f},(Vec3){.11f,.16f,.13f},dark);
    float muzzle=.45f;
    if(weapon==WEAPON_ROCKET) {
        mesh_cylinder(m,(Vec3){0,.055f,.23f},(Vec3){0,.055f,-.71f},.12f,metal,sides);
        mesh_cylinder(m,(Vec3){0,.055f,-.71f},(Vec3){0,.055f,-.735f},.095f,dark,sides);
        mesh_cylinder(m,(Vec3){0,.055f,.24f},(Vec3){0,.055f,.27f},.10f,dark,sides);
        for(int j=0;j<2;j++)mesh_cylinder(m,(Vec3){0,.055f,-.15f-(float)j*.32f},
            (Vec3){0,.055f,-.20f-(float)j*.32f},.133f,accent,sides);
        muzzle=.75f;
    }else if(weapon==1) {
        mesh_box(m,(Vec3){0,-.04f,.19f},(Vec3){.18f,.15f,.27f},(Color){.42f,.27f,.15f});
        for(int side=-1;side<=1;side+=2)
            mesh_cylinder(m,(Vec3){(float)side*.059f,.03f,-.1f},(Vec3){(float)side*.059f,.03f,-.61f},.047f,metal,sides);
        mesh_box(m,(Vec3){0,-.07f,-.31f},(Vec3){.21f,.11f,.20f},dark);muzzle=.63f;
    }else if(weapon==2||weapon==3) {
        bool rail=weapon==3;
        mesh_box(m,(Vec3){-reload.extract*.12f,-.19f-reload.extract*.22f,.10f*reload.extract},(Vec3){.14f,.24f,.18f},metal);
        mesh_cylinder(m,(Vec3){0,.015f,-.14f},(Vec3){0,.015f,rail?-.80f:-.66f},rail?.065f:.044f,metal,sides);
        if(rail) {
            m->glow=.65f;
            for(int coil=0;coil<3;coil++)mesh_cylinder(m,(Vec3){0,.015f,-.33f-(float)coil*.14f},
                (Vec3){0,.015f,-.37f-(float)coil*.14f},.081f,accent,sides);
            m->glow=0;
        }else mesh_box(m,(Vec3){0,.085f,-.17f},(Vec3){.20f,.035f,.43f},accent);
        muzzle=rail?.82f:.68f;
    }else {
        mesh_beveled_box(m,(Vec3){0,.01f,-.25f},(Vec3){.26f,.14f,.42f},.035f,metal);
        m->glow=.5f;mesh_box(m,(Vec3){0,.087f,-.24f},(Vec3){.065f,.022f,.30f},accent);m->glow=0;
    }
    return muzzle;
}
void animate_teammate(Mesh *m,const Actor *actor,int id,float time,int detail,float down_blend) {
    const Player *p=&actor->player;int first=m->count;
    Color armor=teammate_color(id),dark={.10f,.16f,.21f},metal={.34f,.41f,.45f},trim={.76f,.84f,.88f};
    bool dead=actor->dead,helping=!dead&&actor->revive_held&&actor->revive_target>=0;
    float move=dead?0:clampf(p->movement_blend,0,1);
    float breath=dead?0:sinf(time*2.5f+(float)id)*.011f;
    float settle=(helping?-.23f:0)+(dead?0:p->landing*.35f)+breath;
    float stride=!dead&&p->body.grounded?sinf(p->bob)*move*.48f:0;
    float recoil=!dead?clampf(actor->recoil,0,1):0;
    if(actor->damage_time>0&&!dead)armor=(Color){.80f,.84f,.86f};
    int sides=detail?8:6;
    mesh_beveled_box(m,(Vec3){0,.63f+settle,0},(Vec3){.49f,.20f,.34f},.055f,dark);
    mesh_box(m,(Vec3){0,.70f+settle,-.19f},(Vec3){.19f,.055f,.045f},metal);
    for(int side=-1;side<=1;side+=2) {
        int leg=m->count;float x=(float)side*.17f;Vec3 hip={x,.72f+settle,0};
        mesh_beveled_box(m,(Vec3){x,.34f+settle*.45f,0},(Vec3){.22f,.34f+settle*.55f,.26f},.035f,dark);
        int shin=m->count;
        mesh_beveled_box(m,(Vec3){x,.10f,0},(Vec3){.21f,.27f,.24f},.035f,armor);
        mesh_beveled_box(m,(Vec3){x,.015f,-.085f},(Vec3){.25f,.145f,.38f},.035f,dark);
        mesh_box(m,(Vec3){x,.12f,-.25f},(Vec3){.20f,.03f,.045f},metal);
        float knee=dead?.12f:(!p->body.grounded?.48f:fmaxf(0,-stride*(float)side)*.5f);
        joint_pose(m,shin,(Vec3){x,.36f,0},knee,0);
        mesh_beveled_box(m,(Vec3){x,.33f,-.14f},(Vec3){.23f,.13f,.10f},.025f,metal);
        joint_pose(m,leg,hip,stride*(float)side-(helping?.25f:0),dead?0:p->strafe_blend*.08f);
    }
    int upper=m->count;Vec3 chest={0,1.12f+settle,0};
    mesh_beveled_box(m,(Vec3){0,.81f+settle,0},(Vec3){.61f,.51f,.39f},.075f,armor);
    mesh_beveled_box(m,(Vec3){0,.85f+settle,-.225f},(Vec3){.45f,.34f,.095f},.045f,dark);
    mesh_box(m,(Vec3){0,1.22f+settle,-.24f},(Vec3){.48f,.045f,.05f},trim);
    badge(m,id,(Vec3){0,1.12f+settle,-.278f},PI_F,trim);
    mesh_beveled_box(m,(Vec3){0,.88f+settle,.26f},(Vec3){.42f,.36f,.19f},.05f,dark);
    m->glow=dead?0:.45f;
    mesh_box(m,(Vec3){0,1.05f+settle,.365f},(Vec3){.27f,.065f,.035f},armor);m->glow=0;
    badge(m,id,(Vec3){0,.98f+settle,.372f},0,trim);
    for(int side=-1;side<=1;side+=2) {
        mesh_beveled_box(m,(Vec3){(float)side*.365f,1.16f+settle,0},(Vec3){.23f,.21f,.42f},.04f,armor);
        mesh_box(m,(Vec3){(float)side*.37f,1.34f+settle,0},(Vec3){.24f,.035f,.30f},trim);
    }
    mesh_box(m,(Vec3){0,1.30f+settle,0},(Vec3){.24f,.105f,.25f},dark);
    int head=m->count;
    mesh_beveled_box(m,(Vec3){0,1.38f+settle,0},(Vec3){.46f,.32f,.42f},.075f,metal);
    mesh_box(m,(Vec3){0,1.69f+settle,.015f},(Vec3){.085f,.025f,.30f},armor);
    m->glow=dead?0:.7f;
    mesh_beveled_box(m,(Vec3){0,1.49f+settle,-.217f},(Vec3){.37f,.105f,.045f},.02f,dead?dark:(Color){.45f,.91f,1});
    m->glow=0;
    mesh_box(m,(Vec3){0,1.40f+settle,-.225f},(Vec3){.21f,.065f,.04f},dark);
    if(detail)for(int slot=-1;slot<=1;slot++)
        mesh_box(m,(Vec3){(float)slot*.105f,1.42f+settle,-.249f},(Vec3){.05f,.024f,.012f},metal);
    joint_pose(m,head,(Vec3){0,1.49f+settle,0},dead?0:(helping?-.40f:p->pitch*.9f),0);
    if(detail)for(int slot=-1;slot<=1;slot++)
        mesh_box(m,(Vec3){(float)slot*.095f,.95f+settle,.372f},(Vec3){.035f,.025f,.02f},trim);
    int arms=m->count;
    ReloadPose gesture=helping?(ReloadPose){0}:weapon_reload_pose(p);
    float reload=gesture.lower;
    float switching=clampf(p->weapon_switch/WEAPON_SWITCH_TIME,0,1);
    float punch=!dead&&p->melee_timer>0?sinf((1-p->melee_timer/MELEE_COOLDOWN)*PI_F):0;
    for(int side=-1;side<=1;side+=2) {
        Vec3 shoulder={(float)side*.34f,1.17f+settle,0};
        Vec3 elbow={(float)side*.34f,.95f+settle,-.12f};
        Vec3 hand={side>0?.22f:-.07f,1.07f+settle-(side<0?gesture.extract*.21f-gesture.rack*.12f:0),side>0?-.25f:-.56f};
        if(dead){elbow.z=.02f;hand=(Vec3){(float)side*.33f,.79f+settle,-.05f};}
        else if(helping)hand=(Vec3){(float)side*.20f,.71f+settle,-.37f};
        else if(side>0)hand.z-=punch*.32f;
        mesh_cylinder(m,shoulder,elbow,.09f,armor,sides);
        mesh_cylinder(m,elbow,hand,.073f,dark,sides);
        mesh_beveled_box(m,vadd(hand,(Vec3){0,-.045f,0}),(Vec3){.14f,.11f,.16f},.025f,dark);
    }
    if(!dead) {
        int gun=m->count;
        Color accent=WEAPONS[p->weapon].color;float muzzle=held_weapon(m,p->weapon,sides,accent,gesture);
        if(actor->shot_age<.06f&&p->reload_timer<=0&&p->weapon_switch<=0&&!helping) {
            float size=.10f*(1-actor->shot_age/.06f);m->glow=1;
            mesh_diamond(m,(Vec3){0,.03f,-muzzle},size,(Color){1,.79f,.35f});
            mesh_cylinder(m,(Vec3){0,.03f,-muzzle},(Vec3){0,.03f,-muzzle-size*2},size*.25f,(Color){1,.9f,.55f},6);m->glow=0;
        }
        mesh_pose(m,gun,(Vec3){.22f,1.09f+settle,-.31f+recoil*.06f},0,0,1);
    }
    joint_pose(m,arms,(Vec3){0,1.17f+settle,0},dead?0:(helping?-.3f:p->pitch*.9f-reload*.40f+gesture.rack*.08f-switching*.55f-punch*.3f),0);
    joint_pose(m,upper,chest,dead?0:(helping?-.16f:p->pitch*.1f-(p->dash_timer>0?.12f:0)),dead?0:-p->strafe_blend*.065f);
    Vec3 position=p->body.position;
    float fall=clampf(down_blend,0,1);fall=fall*fall*(3-2*fall);
    position=vadd(position,(Vec3){sinf(p->yaw)*.8f*fall,.28f*fall,-cosf(p->yaw)*.8f*fall});
    mesh_pose(m,first,position,p->yaw,PI_F*.5f*fall,1);
    if(fall>.01f||p->body.grounded)for(int i=first;i<m->count;i++)
        m->data[i].position.y=fmaxf(m->data[i].position.y,p->body.position.y+.012f);
    m->glow=0;
}
