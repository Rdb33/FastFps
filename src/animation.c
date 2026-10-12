#include "fastfps.h"
#include <math.h>

static float smooth(float t) {
    t=clampf(t,0,1);
    return t*t*(3-2*t);
}

/* Le rig est construit en espace local, puis orienté en une seule passe.
   Aucune texture, allocation ou appel de dessin supplémentaire. */
void animate_creature(Mesh *mesh, const Enemy *e, float time) {
    int first=mesh->count;
    bool dead=e->death_time>0;
    float blend=dead?0:e->movement_blend;
    float stride=sinf(e->walk_phase)*.52f*blend;
    float bounce=fabsf(cosf(e->walk_phase))*.035f*blend;
    float breath=dead?0:sinf(time*2.6f+(float)e->id)*.013f;
    float progress=e->attack_timer>0?1-e->attack_timer/e->attack_duration:0;
    float strike=e->attack_timer>0?sinf(smooth(clampf((progress-.25f)/.5f,0,1))*PI_F):0;
    float shoulder=bounce+breath;
    Color skin={.64f,.17f,.09f},armor={.27f,.17f,.12f},eye={1,.78f,.22f};
    float width=.72f;
    if(e->type==ENEMY_CASTER) {
        skin=(Color){.40f,.25f,.66f};armor=(Color){.18f,.13f,.31f};eye=(Color){.3f,1,.81f};width=.59f;
    } else if(e->type==ENEMY_BRUTE) {
        skin=(Color){.39f,.46f,.24f};armor=(Color){.61f,.52f,.24f};eye=(Color){1,.35f,.1f};width=.94f;
    }
    if(e->flash>0)skin=(Color){1,.88f,.67f};
    float stagger=dead?0:e->stagger*.24f;
    mesh_box(mesh,(Vec3){0,.50f+shoulder,stagger},(Vec3){width,.65f,.55f},skin);
    mesh_box(mesh,(Vec3){0,.95f+shoulder,-.02f+stagger},(Vec3){width+.13f,.28f,.61f},armor);
    mesh_box(mesh,(Vec3){0,1.22f+shoulder,stagger},(Vec3){.57f,.38f,.51f},skin);
    mesh_box(mesh,(Vec3){-.14f,1.40f+shoulder,-.268f+stagger},(Vec3){.105f,.075f,.04f},eye);
    mesh_box(mesh,(Vec3){.14f,1.40f+shoulder,-.268f+stagger},(Vec3){.105f,.075f,.04f},eye);
    mesh_box(mesh,(Vec3){0,1.26f+shoulder,-.276f+stagger},(Vec3){.27f,.07f,.035f},(Color){.10f,.07f,.05f});
    if(e->type==ENEMY_CASTER) {
        mesh_diamond(mesh,(Vec3){0,1.69f+shoulder,.015f},.19f,armor);
        mesh_box(mesh,(Vec3){0,.54f+shoulder,.10f},(Vec3){.53f,.48f,.42f},armor);
    } else if(e->type==ENEMY_BRUTE) {
        mesh_box(mesh,(Vec3){-.55f,1.02f+shoulder,0},(Vec3){.35f,.27f,.68f},armor);
        mesh_box(mesh,(Vec3){.55f,1.02f+shoulder,0},(Vec3){.35f,.27f,.68f},armor);
        mesh_diamond(mesh,(Vec3){-.34f,1.68f+shoulder,0},.20f,armor);
        mesh_diamond(mesh,(Vec3){.34f,1.68f+shoulder,0},.20f,armor);
    } else {
        mesh_diamond(mesh,(Vec3){-.30f,1.66f+shoulder,0},.16f,(Color){.78f,.68f,.44f});
        mesh_diamond(mesh,(Vec3){.30f,1.66f+shoulder,0},.16f,(Color){.78f,.68f,.44f});
    }
    for(int side=-1;side<=1;side+=2) {
        float leg_angle=stride*(float)side;
        Vec3 hip={side*.22f,.57f+shoulder,0};
        mesh_joint_box(mesh,(Vec3){hip.x,.13f+shoulder,0},(Vec3){.23f,.44f,.25f},hip,leg_angle,skin);
        mesh_joint_box(mesh,(Vec3){hip.x,.025f+shoulder,-.07f},(Vec3){.27f,.15f,.37f},hip,leg_angle,armor);
        float arm_angle=-leg_angle*.75f;
        if(e->attack_timer>0)arm_angle=e->type==ENEMY_CASTER?.55f+strike*.65f:-.42f+strike*1.75f;
        Vec3 pivot={side*(width/2+.12f),1.1f+shoulder,0};
        mesh_joint_box(mesh,(Vec3){pivot.x,.62f+shoulder,0},(Vec3){.20f,.48f,.25f},pivot,arm_angle,skin);
        mesh_joint_box(mesh,(Vec3){pivot.x,.49f+shoulder,-.035f},(Vec3){.25f,.17f,.30f},pivot,arm_angle,armor);
    }
    if(!dead&&e->type==ENEMY_CASTER&&e->attack_timer>0&&!e->attack_fired) {
        float charge=clampf(progress/.62f,0,1);
        mesh_diamond(mesh,(Vec3){0,1.2f,-.67f},.07f+.16f*charge,(Color){.3f,1,.83f});
        mesh_diamond(mesh,(Vec3){0,1.2f,-.67f},.055f,(Color){.95f,1,.70f});
    }
    float scale=e->type==ENEMY_BRUTE?1.3f:1;
    Vec3 position=e->body.position;
    float fall=0;
    if(dead) {
        fall=smooth(e->death_time/.55f)*PI_F*.49f;
        scale*=1-smooth((e->death_time-3.15f)/.85f);
        position.y+=sinf(fall)*.30f*scale;
    } else scale*=.68f+.32f*(1-e->spawn_time/.5f);
    mesh_pose(mesh,first,position,e->yaw,fall,scale);
    if(dead)for(int i=first;i<mesh->count;i++)
        mesh->data[i].position.y=fmaxf(mesh->data[i].position.y,e->body.position.y+.012f);
}

void animate_weapon(Mesh *mesh, const Game *g, const App *app) {
    mesh->count=0;
    const Player *p=&g->player;
    float motion=app->motion?1:0;
    float blend=p->movement_blend;
    float sway=sinf(p->bob)*blend*.022f*motion;
    float vertical=fabsf(cosf(p->bob))*blend*.012f*motion;
    float lower=p->weapon_switch>0?sinf(p->weapon_switch/WEAPON_SWITCH_TIME*PI_F)*.40f:0;
    float melee_age=MELEE_COOLDOWN-p->melee_timer;
    float punch=p->melee_timer>0&&melee_age<.30f?sinf(melee_age/.30f*PI_F):0;
    Vec3 base={sway-p->sway_x*motion,-.39f-vertical-lower-g->recoil*.035f+p->landing*.4f*motion,
        -.62f+g->recoil*.10f};
    if(punch>0)base.y-=punch*.22f;
    Color metal={.35f,.39f,.35f},dark={.13f,.16f,.13f},glove={.30f,.36f,.21f},skin={.63f,.43f,.28f};
    if(p->weapon==0) {
        mesh_box(mesh,base,(Vec3){.23f,.20f,.46f},dark);
        mesh_box(mesh,vadd(base,(Vec3){0,.12f,-.12f}),(Vec3){.14f,.11f,.48f},metal);
        mesh_box(mesh,vadd(base,(Vec3){0,.21f,-.14f}),(Vec3){.035f,.035f,.42f},(Color){1,.53f,.2f});
        mesh_box(mesh,vadd(base,(Vec3){0,-.12f,.13f}),(Vec3){.16f,.20f,.18f},dark);
        mesh_box(mesh,vadd(base,(Vec3){.10f,-.10f,.12f}),(Vec3){.13f,.13f,.22f},glove);
        mesh_box(mesh,vadd(base,(Vec3){.14f,-.19f,.30f}),(Vec3){.12f,.15f,.30f},skin);
    } else {
        float pump=g->shot_age>.12f&&g->shot_age<.48f?sinf((g->shot_age-.12f)/.36f*PI_F)*.14f:0;
        mesh_box(mesh,base,(Vec3){.30f,.18f,.45f},(Color){.40f,.25f,.12f});
        mesh_box(mesh,vadd(base,(Vec3){-.077f,.10f,-.15f}),(Vec3){.13f,.13f,.55f},metal);
        mesh_box(mesh,vadd(base,(Vec3){.077f,.10f,-.15f}),(Vec3){.13f,.13f,.55f},metal);
        mesh_box(mesh,vadd(base,(Vec3){0,-.11f,.18f}),(Vec3){.23f,.14f,.22f},dark);
        mesh_box(mesh,vadd(base,(Vec3){0,-.045f,-.14f+pump}),(Vec3){.32f,.09f,.17f},dark);
        mesh_box(mesh,vadd(base,(Vec3){-.10f,-.10f,-.12f+pump}),(Vec3){.18f,.12f,.22f},glove);
        mesh_box(mesh,vadd(base,(Vec3){-.18f,-.20f,.09f+pump}),(Vec3){.13f,.14f,.32f},skin);
        mesh_box(mesh,vadd(base,(Vec3){.12f,-.08f,.21f}),(Vec3){.15f,.13f,.22f},glove);
    }
    if(g->shot_age<.055f&&p->weapon_switch==0)
        mesh_diamond(mesh,vadd(base,(Vec3){0,.17f,-.45f}),p->weapon?.14f:.085f,(Color){1,.85f,.40f});
    mesh_pose(mesh,0,(Vec3){-p->strafe_blend*.014f*motion,p->sway_y*motion,0},
        0,g->recoil*(p->weapon?.12f:.08f),1);
    if(punch>0) {
        Vec3 fist={-.18f+punch*.13f,-.39f+punch*.16f,-.36f-punch*.42f};
        mesh_box(mesh,fist,(Vec3){.20f,.19f,.23f},glove);
        mesh_box(mesh,vadd(fist,(Vec3){-.03f,-.08f,.23f}),(Vec3){.15f,.19f,.35f},skin);
    }
}
void animate_teammate(Mesh *m,const Actor *actor,int id) {
    const Player *p=&actor->player;int first=m->count;
    static const Color colors[MAX_PLAYERS]={{.20f,.56f,.77f},{.25f,.72f,.70f},{.34f,.49f,.82f},{.74f,.65f,.28f}};
    Color armor=colors[id],dark={.12f,.20f,.24f},visor={.6f,.95f,1};
    float stride=sinf(p->bob)*p->movement_blend*.5f;
    mesh_box(m,(Vec3){0,.53f,0},(Vec3){.66f,.62f,.43f},armor);
    mesh_box(m,(Vec3){0,1.15f,0},(Vec3){.52f,.35f,.45f},dark);
    mesh_box(m,(Vec3){0,1.29f,-.24f},(Vec3){.43f,.09f,.035f},visor);
    mesh_box(m,(Vec3){0,.72f,.26f},(Vec3){.40f,.39f,.18f},dark);
    for(int side=-1;side<=1;side+=2) {
        Vec3 hip={side*.2f,.58f,0};
        mesh_joint_box(m,(Vec3){hip.x,.04f,0},(Vec3){.23f,.54f,.27f},hip,stride*(float)side,dark);
        Vec3 shoulder={side*.40f,1.09f,0};
        mesh_joint_box(m,(Vec3){shoulder.x,.57f,-.05f},(Vec3){.2f,.52f,.24f},shoulder,.72f,armor);
    }
    mesh_box(m,(Vec3){.26f,.87f,-.50f},(Vec3){.16f,.12f,p->weapon?.55f:.36f},dark);
    if(actor->shot_age<.055f&&!actor->dead)
        mesh_diamond(m,(Vec3){.26f,.94f,-.83f},.09f,(Color){1,.82f,.4f});
    mesh_pose(m,first,p->body.position,p->yaw,actor->dead?PI_F*.45f:0,actor->dead?.45f:1);
}

void animate_barrel(Mesh *m,const Barrel *b,float time) {
    if(b->health<=0) {
        if(b->explosion_time<=0)return;
        float age=.65f-b->explosion_time,fade=b->explosion_time/.65f;
        Color fire={fade,fade*.58f,fade*.12f};
        Vec3 center=vadd(b->position,(Vec3){0,.08f,0});
        float radius=age*9+.4f;
        for(int i=0;i<16;i++) {
            float a=(float)i*2*PI_F/16,c=(float)(i+1)*2*PI_F/16;
            Vec3 p={center.x+sinf(a)*radius,center.y,center.z+cosf(a)*radius};
            Vec3 q={center.x+sinf(c)*radius,center.y,center.z+cosf(c)*radius};
            Vec3 r={center.x+sinf(c)*(radius-.14f),center.y,center.z+cosf(c)*(radius-.14f)};
            Vec3 s={center.x+sinf(a)*(radius-.14f),center.y,center.z+cosf(a)*(radius-.14f)};
            mesh_quad(m,p,q,r,s,fire,(Vec3){0,1,0});
        }
        if(age<.24f)mesh_diamond(m,vadd(b->position,(Vec3){0,.65f,0}),(.25f+age*5)*fade,fire);
        return;
    }
    Color red={.65f,.13f,.07f},band={.16f,.17f,.13f};
    float pulse=b->health<45?.015f*(1+sinf(time*18)):0;
    float radius=.42f+pulse;
    for(int i=0;i<8;i++) {
        float a=(float)i*2*PI_F/8,c=(float)(i+1)*2*PI_F/8;
        Vec3 p={b->position.x+sinf(a)*radius,b->position.y,b->position.z+cosf(a)*radius};
        Vec3 q={b->position.x+sinf(c)*radius,b->position.y,b->position.z+cosf(c)*radius};
        Vec3 normal={sinf((a+c)/2),0,cosf((a+c)/2)};
        for(int section=0;section<3;section++) {
            float bottom=section==0?0:(section==1?.14f:.96f),height=section==1?.82f:.14f;
            Vec3 lo=vadd(p,(Vec3){0,bottom,0}),ro=vadd(q,(Vec3){0,bottom,0});
            mesh_quad(m,lo,ro,vadd(ro,(Vec3){0,height,0}),vadd(lo,(Vec3){0,height,0}),section==1?red:band,normal);
        }
        mesh_triangle(m,vadd(b->position,(Vec3){0,1.1f,0}),vadd(p,(Vec3){0,1.1f,0}),vadd(q,(Vec3){0,1.1f,0}),band,(Vec3){0,1,0});
    }
    mesh_diamond(m,vadd(b->position,(Vec3){0,.62f,-.43f}),.16f,(Color){1,.78f,.18f});
    mesh_diamond(m,vadd(b->position,(Vec3){0,.62f,.43f}),.16f,(Color){1,.78f,.18f});
}
