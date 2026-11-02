#include "fastfps.h"
#include <math.h>

/* Même langage visuel sur l'arme tenue et celle des alliés. */
void animate_weapon_effects(Mesh *m,const Player *p,Vec3 origin,float muzzle,float time,int detail,float intensity) {
    if(intensity<=0||m->count>m->capacity-1100)return;
    float pulse=.8f+.2f*sinf(time*5),strength=clampf(intensity,0,1);
    Color orange={1,.44f,.13f},gold={1,.80f,.30f},purple={.76f,.43f,1},pink={1,.38f,.79f};
    float glow=m->glow;m->glow=.5f+.5f*strength;
    if(p->dash_power_time>0&&p->upgrades[UPGRADE_DASH_POWER]) {
        for(int side=-1;side<=1;side+=2)
            mesh_box(m,vadd(origin,(Vec3){(float)side*.14f,.095f,-muzzle*.42f}),
                (Vec3){.018f,.022f,muzzle*.45f},(Color){orange.r*pulse,orange.g*pulse,orange.b*pulse});
        if(detail)for(int i=0;i<3;i++) {
            float phase=fmodf(time*2+(float)i/3,1);
            mesh_diamond(m,vadd(origin,(Vec3){.15f,.10f,-muzzle*(.15f+phase*.65f)}),.018f,orange);
        }
    }
    if(p->switch_power_time>0&&p->upgrades[UPGRADE_SWITCH_POWER]) {
        Vec3 chamber=vadd(origin,(Vec3){0,.21f,-.08f});
        mesh_diamond(m,chamber,.035f+.008f*pulse,gold);
        mesh_box(m,vadd(chamber,(Vec3){0,-.04f,0}),(Vec3){.10f,.012f,.07f},gold);
    }
    if(p->upgrades[UPGRADE_PIERCE]&&p->weapon<STANDARD_WEAPONS)for(int i=0;i<2;i++) {
        Vec3 center=vadd(origin,(Vec3){0,.09f,-muzzle*(.65f+(float)i*.17f)});
        int first=m->count;
        mesh_ring(m,center,.10f,.013f,purple,detail?12:8);
        mesh_rotate(m,first,center,PI_F*.5f,0,0);
    }
    if(p->power_time>0) {
        Vec3 core=vadd(origin,(Vec3){0,.25f,-.17f});
        mesh_diamond(m,core,.052f+.01f*pulse,pink);
        for(int side=-1;side<=1;side+=2) {
            Vec3 start=vadd(core,(Vec3){(float)side*.15f,-.08f,.12f});
            Vec3 bend=vadd(core,(Vec3){(float)side*.07f,.025f*sinf(time*13+(float)side),.04f});
            mesh_cylinder(m,start,bend,.006f*strength,pink,4);
            mesh_cylinder(m,bend,core,.006f*strength,pink,4);
        }
    }
    if(p->reactor_time>0)for(int side=-1;side<=1;side+=2)
        mesh_diamond(m,vadd(origin,(Vec3){(float)side*.12f,.03f,.07f}),.017f,(Color){.3f,1,.65f});
    m->glow=glow;
}
