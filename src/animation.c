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
    if(!dead&&e->slam_state==1)shoulder-=.15f*(1-e->slam_timer/.75f);
    if(!dead&&e->slam_state==3)shoulder-=e->slam_timer/.85f*.17f;
    Color skin={.64f,.17f,.09f},armor={.27f,.17f,.12f},eye={1,.78f,.22f};
    float width=.72f;
    if(e->type==ENEMY_CASTER) {
        skin=(Color){.40f,.25f,.66f};armor=(Color){.18f,.13f,.31f};eye=(Color){.3f,1,.81f};width=.59f;
    } else if(e->type==ENEMY_BRUTE) {
        skin=(Color){.39f,.46f,.24f};armor=(Color){.61f,.52f,.24f};eye=(Color){1,.35f,.1f};width=.94f;
    }
    if(e->flash>0)skin=(Color){1,.88f,.67f};
    if(e->elite){armor=(Color){.88f,.59f,.18f};eye=(Color){1,.87f,.36f};skin=(Color){.37f,.26f,.16f};}
    float stagger=dead?0:e->stagger*.24f;
    mesh_beveled_box(mesh,(Vec3){0,.50f+shoulder,stagger},(Vec3){width,.65f,.55f},.065f,skin);
    mesh_beveled_box(mesh,(Vec3){0,.95f+shoulder,-.02f+stagger},(Vec3){width+.13f,.28f,.61f},.08f,armor);
    mesh_beveled_box(mesh,(Vec3){0,1.22f+shoulder,stagger},(Vec3){.57f,.38f,.51f},.075f,skin);
    mesh->glow=dead?0:.8f;
    mesh_box(mesh,(Vec3){-.14f,1.40f+shoulder,-.268f+stagger},(Vec3){.105f,.075f,.04f},eye);
    mesh_box(mesh,(Vec3){.14f,1.40f+shoulder,-.268f+stagger},(Vec3){.105f,.075f,.04f},eye);
    mesh->glow=0;
    mesh_box(mesh,(Vec3){0,1.26f+shoulder,-.276f+stagger},(Vec3){.27f,.07f,.035f},(Color){.10f,.07f,.05f});
    mesh_box(mesh,(Vec3){0,1.45f+shoulder,-.285f+stagger},(Vec3){.43f,.035f,.045f},armor);
    for(int tooth=-1;tooth<=1;tooth++)mesh_box(mesh,(Vec3){(float)tooth*.065f,1.255f+shoulder,-.3f+stagger},(Vec3){.032f,.034f,.018f},(Color){.73f,.70f,.51f});
    if(e->type==ENEMY_CASTER) {
        mesh_diamond(mesh,(Vec3){0,1.69f+shoulder,.015f},.19f,armor);
        mesh_box(mesh,(Vec3){0,.54f+shoulder,.10f},(Vec3){.53f,.48f,.42f},armor);
    } else if(e->type==ENEMY_BRUTE) {
        mesh_beveled_box(mesh,(Vec3){-.55f,1.02f+shoulder,0},(Vec3){.35f,.27f,.68f},.06f,armor);
        mesh_beveled_box(mesh,(Vec3){.55f,1.02f+shoulder,0},(Vec3){.35f,.27f,.68f},.06f,armor);
        mesh_diamond(mesh,(Vec3){-.34f,1.68f+shoulder,0},.20f,armor);
        mesh_diamond(mesh,(Vec3){.34f,1.68f+shoulder,0},.20f,armor);
    } else {
        mesh_diamond(mesh,(Vec3){-.30f,1.66f+shoulder,0},.16f,(Color){.78f,.68f,.44f});
        mesh_diamond(mesh,(Vec3){.30f,1.66f+shoulder,0},.16f,(Color){.78f,.68f,.44f});
    }
    for(int side=-1;side<=1;side+=2) {
        float leg_angle=stride*(float)side;
        Vec3 hip={side*.22f,.57f+shoulder,0};
        int leg=mesh->count;
        mesh_beveled_box(mesh,(Vec3){hip.x,.32f+shoulder,0},(Vec3){.23f,.25f,.25f},.025f,skin);
        int shin=mesh->count;
        mesh_beveled_box(mesh,(Vec3){hip.x,.09f+shoulder,0},(Vec3){.21f,.24f,.23f},.025f,skin);
        mesh_beveled_box(mesh,(Vec3){hip.x,.025f+shoulder,-.07f},(Vec3){.27f,.15f,.37f},.03f,armor);
        float knee=dead?.08f:fmaxf(0,-leg_angle)*.45f;
        if(!dead&&e->slam_state==2)knee=.55f;
        mesh_rotate(mesh,shin,(Vec3){hip.x,.33f+shoulder,0},knee,0,0);
        mesh_rotate(mesh,leg,hip,leg_angle,0,0);
        float arm_angle=-leg_angle*.75f;
        if(!dead&&e->slam_state)arm_angle=e->slam_state==2?1.8f:(e->slam_state==1?-.5f:1.2f);
        if(e->attack_timer>0)arm_angle=e->type==ENEMY_CASTER?.55f+strike*.65f:-.42f+strike*1.75f;
        Vec3 pivot={side*(width/2+.12f),1.1f+shoulder,0};
        mesh_joint_box(mesh,(Vec3){pivot.x,.62f+shoulder,0},(Vec3){.20f,.48f,.25f},pivot,arm_angle,skin);
        mesh_joint_box(mesh,(Vec3){pivot.x,.49f+shoulder,-.035f},(Vec3){.25f,.17f,.30f},pivot,arm_angle,armor);
    }
    if(!dead&&e->type==ENEMY_CASTER&&e->attack_timer>0&&!e->attack_fired) {
        mesh->glow=1;
        float charge=clampf(progress/.62f,0,1);
        mesh_diamond(mesh,(Vec3){0,1.2f,-.67f},.07f+.16f*charge,(Color){.3f,1,.83f});
        mesh_diamond(mesh,(Vec3){0,1.2f,-.67f},.055f,(Color){.95f,1,.70f});
        for(int i=0;i<3;i++) {
            float angle=time*9+(float)i*2*PI_F/3;
            mesh_diamond(mesh,(Vec3){sinf(angle)*.24f,1.2f+cosf(angle)*.24f,-.67f},.035f,(Color){.42f,1,.82f});
        }
        mesh->glow=0;
    }
    if(e->elite) {
        mesh->glow=dead?0:.8f;
        mesh_diamond(mesh,(Vec3){0,.89f+shoulder,-.34f},.16f,(Color){1,.68f,.18f});
        mesh->glow=0;
        for(int side=-1;side<=1;side+=2) {
            mesh_diamond(mesh,(Vec3){(float)side*.19f,1.76f+shoulder,0},.16f,armor);
            mesh_box(mesh,(Vec3){(float)side*.53f,.64f+shoulder,-.14f},(Vec3){.22f,.44f,.08f},armor);
        }
    }
    float scale=enemy_size(e);
    Vec3 position=e->body.position;
    float fall=0;
    if(dead) {
        fall=smooth(e->death_time/.55f)*PI_F*.49f;
        scale*=1-smooth((e->death_time-3.15f)/.85f);
        position.y+=sinf(fall)*.30f*scale;
    } else scale*=.68f+.32f*smooth(1-e->spawn_time/.5f);
    mesh_pose(mesh,first,position,e->yaw,fall,scale);
    if(!dead&&e->body.grounded)for(int i=first;i<mesh->count;i++)
        mesh->data[i].position.y=fmaxf(mesh->data[i].position.y,e->body.position.y+.007f);
    if(dead)for(int i=first;i<mesh->count;i++)
        mesh->data[i].position.y=fmaxf(mesh->data[i].position.y,e->body.position.y+.012f);
    if(!dead&&e->spawn_time>0)mesh_ring(mesh,vadd(position,(Vec3){0,.035f,0}),
        .6f+(1-e->spawn_time/.5f)*.9f,.1f,e->elite?(Color){1,.68f,.24f}:(Color){.42f,.63f,.88f},16);
    if(!dead&&e->elite&&(e->slam_state==1||e->slam_state==2)) {
        mesh->glow=1;Color warning={1,.38f+.12f*sinf(time*12),.10f};
        Vec3 center=vadd(e->slam_target,(Vec3){0,.045f,0});
        mesh_ring(mesh,center,6,.075f,warning,32);
        mesh_ring(mesh,vadd(center,(Vec3){0,.005f,0}),.7f,.10f,warning,20);
        mesh_diamond(mesh,vadd(center,(Vec3){0,.3f,0}),.1f,warning);mesh->glow=0;
    }
    if(!dead&&e->slam_flash>0) {
        mesh->glow=1;float amount=e->slam_flash/.8f;
        mesh_ring(mesh,vadd(e->body.position,(Vec3){0,.05f,0}),.4f+(1-amount)*6,.18f,(Color){amount,amount*.6f,amount*.15f},32);
        mesh->glow=0;
    }
}

void animate_weapon(Mesh *mesh, const Game *g, const App *app) {
    mesh->count=0;
    if(g->dead)return;
    const Player *p=&g->player;
    float motion=app->motion?1:0;
    float blend=p->movement_blend*(p->body.grounded?1:.2f);
    float hurry=clampf((hypotf(p->body.velocity.x,p->body.velocity.z)-PLAYER_SPEED*(1+.03f*p->upgrades[UPGRADE_MOBILITY]))/4,0,1);
    float sway=(sinf(p->bob)*.020f+sinf(p->bob*.5f)*.006f)*blend*motion;
    float vertical=(cosf(p->bob*2)*blend*.009f+sinf(g->time*2.1f)*.003f)*motion;
    float switching=0;
    if(p->weapon_switch>0) {
        float t=1-p->weapon_switch/WEAPON_SWITCH_TIME;
        switching=t<.5f?smooth(t*2):1-smooth((t-.5f)*2);
    }
    bool helping=g->cooperative&&g->actors[g->local_actor].revive_held&&g->actors[g->local_actor].revive_target>=0;
    ReloadPose gesture=helping?(ReloadPose){0}:weapon_reload_pose(p);
    float reload=gesture.progress,reload_pose=gesture.lower;
    float lower=switching*.36f+reload_pose*.10f+hurry*.035f*motion;
    if(helping)lower+=.30f;
    float melee_age=MELEE_COOLDOWN-p->melee_timer;
    float punch=p->melee_timer>0&&melee_age<.30f?sinf(melee_age/.30f*PI_F):0;
    Vec3 base={sway-p->sway_x*motion-p->strafe_blend*.014f*motion,-.39f-vertical-lower-g->recoil*.035f+(p->landing*.6f+p->sway_y)*motion,
        -.62f+g->recoil*.10f};
    if(punch>0)base.y-=punch*.22f;
    base.y-=p->dash_timer/DASH_DURATION*.06f*motion;
    Color metal={.35f,.39f,.35f},dark={.13f,.16f,.13f},glove={.30f,.36f,.21f},skin={.63f,.43f,.28f};
    Color accent=p->upgrades[UPGRADE_POWER]>=3?(Color){1,.71f,.2f}:WEAPONS[p->weapon].color;
    if(p->upgrades[UPGRADE_POWER])metal=(Color){.47f,.47f,.32f};
    if(p->upgrade_flash>0)accent=(Color){.65f,1,1};
    if(p->weapon==0) {
        mesh_beveled_box(mesh,base,(Vec3){.23f,.20f,.46f},.035f,dark);
        mesh_beveled_box(mesh,vadd(base,(Vec3){0,.12f,-.12f}),(Vec3){.14f,.11f,.48f},.025f,metal);
        mesh_box(mesh,vadd(base,(Vec3){0,.21f,-.14f}),(Vec3){.035f,.035f,.42f},(Color){1,.53f,.2f});
        mesh_box(mesh,vadd(base,(Vec3){0,-.12f,.13f}),(Vec3){.16f,.20f,.18f},dark);
        mesh_beveled_box(mesh,vadd(base,(Vec3){.10f,-.10f,.12f}),(Vec3){.13f,.13f,.22f},.025f,glove);
        mesh_box(mesh,vadd(base,(Vec3){.14f,-.19f,.30f}),(Vec3){.12f,.15f,.30f},skin);
        for(int side=-1;side<=1;side+=2) {
            mesh_box(mesh,vadd(base,(Vec3){side*.13f,.07f,-.01f}),(Vec3){.035f,.08f,.23f},metal);
            mesh->glow=.85f;mesh_box(mesh,vadd(base,(Vec3){side*.15f,.095f,-.01f}),(Vec3){.012f,.025f,.19f},accent);mesh->glow=0;
        }
        mesh_box(mesh,vadd(base,(Vec3){0,.235f,.07f}),(Vec3){.085f,.045f,.075f},dark);
        int sides=graphics_detail(app->quality)>0?10:6;
        mesh_cylinder(mesh,vadd(base,(Vec3){0,.17f,-.20f}),vadd(base,(Vec3){0,.17f,-.48f}),.075f,metal,sides);
        mesh_cylinder(mesh,vadd(base,(Vec3){0,.17f,-.482f}),vadd(base,(Vec3){0,.17f,-.486f}),.045f,(Color){.045f,.065f,.07f},sides);
        mesh->glow=.65f;
        mesh_cylinder(mesh,vadd(base,(Vec3){0,.17f,-.46f}),vadd(base,(Vec3){0,.17f,-.472f}),.079f,accent,sides);
        mesh->glow=0;
        if(p->upgrades[UPGRADE_POWER]>=2) {
            mesh_box(mesh,vadd(base,(Vec3){-.13f,.08f,-.26f}),(Vec3){.08f,.10f,.18f},accent);
            mesh_box(mesh,vadd(base,(Vec3){.13f,.08f,-.26f}),(Vec3){.08f,.10f,.18f},accent);
        }
    } else if(p->weapon==1) {
        float pump=g->shot_age>.12f&&g->shot_age<.48f?sinf((g->shot_age-.12f)/.36f*PI_F)*.14f:0;
        mesh_beveled_box(mesh,base,(Vec3){.30f,.18f,.45f},.04f,(Color){.40f,.25f,.12f});
        int sides=graphics_detail(app->quality)>0?10:6;
        for(int side=-1;side<=1;side+=2) {
            mesh_cylinder(mesh,vadd(base,(Vec3){(float)side*.077f,.165f,.08f}),vadd(base,(Vec3){(float)side*.077f,.165f,-.42f}),.068f,metal,sides);
            mesh_cylinder(mesh,vadd(base,(Vec3){(float)side*.077f,.165f,-.425f}),vadd(base,(Vec3){(float)side*.077f,.165f,-.429f}),.044f,(Color){.045f,.06f,.055f},sides);
        }
        mesh_box(mesh,vadd(base,(Vec3){0,-.11f,.18f}),(Vec3){.23f,.14f,.22f},dark);
        mesh_box(mesh,vadd(base,(Vec3){0,-.045f,-.14f+pump}),(Vec3){.32f,.09f,.17f},dark);
        if(!gesture.active&&!helping) {
            mesh_box(mesh,vadd(base,(Vec3){-.10f,-.10f,-.12f+pump}),(Vec3){.18f,.12f,.22f},glove);
            mesh_box(mesh,vadd(base,(Vec3){-.18f,-.20f,.09f+pump}),(Vec3){.13f,.14f,.32f},skin);
        }
        mesh_box(mesh,vadd(base,(Vec3){.12f,-.08f,.21f}),(Vec3){.15f,.13f,.22f},glove);
        mesh_box(mesh,vadd(base,(Vec3){0,.235f,-.30f}),(Vec3){.028f,.04f,.055f},accent);
        for(int i=0;i<3;i++)mesh_box(mesh,vadd(base,(Vec3){.16f,.09f,-.09f-(float)i*.07f}),(Vec3){.017f,.045f,.035f},metal);
        if(p->upgrades[UPGRADE_POWER]>=2) {
            mesh_box(mesh,vadd(base,(Vec3){-.077f,.16f,-.22f}),(Vec3){.04f,.02f,.4f},accent);
            mesh_box(mesh,vadd(base,(Vec3){.077f,.16f,-.22f}),(Vec3){.04f,.02f,.4f},accent);
        }
    }
    if(p->weapon>=2&&p->weapon<STANDARD_WEAPONS) {
        bool rail=p->weapon==3;int sides=graphics_detail(app->quality)>0?10:6;
        mesh_beveled_box(mesh,base,(Vec3){rail?.31f:.27f,.20f,.54f},.045f,dark);
        mesh_beveled_box(mesh,vadd(base,(Vec3){0,.14f,-.05f}),(Vec3){.18f,.13f,.63f},.03f,metal);
        mesh_cylinder(mesh,vadd(base,(Vec3){0,.18f,-.25f}),vadd(base,(Vec3){0,.18f,-.64f}),rail?.09f:.065f,metal,sides);
        mesh_cylinder(mesh,vadd(base,(Vec3){0,.18f,-.645f}),vadd(base,(Vec3){0,.18f,-.65f}),.043f,(Color){.04f,.045f,.055f},sides);
        mesh_box(mesh,vadd(base,(Vec3){0,-.08f,.12f}),(Vec3){.14f,.17f,.15f},dark);
        mesh_beveled_box(mesh,vadd(base,(Vec3){.1f,-.1f,.16f}),(Vec3){.14f,.13f,.2f},.025f,glove);
        mesh_box(mesh,vadd(base,(Vec3){.16f,-.20f,.30f}),(Vec3){.12f,.15f,.28f},skin);
        if(!gesture.active&&!helping)mesh_box(mesh,vadd(base,(Vec3){-.12f,-.08f,-.22f}),(Vec3){.14f,.13f,.2f},glove);
        if(rail) {
            mesh_cylinder(mesh,vadd(base,(Vec3){0,.32f,.02f}),vadd(base,(Vec3){0,.32f,-.12f}),.07f,dark,sides);
            mesh->glow=.8f;
            for(int i=0;i<4;i++)for(int side=-1;side<=1;side+=2)
                mesh_box(mesh,vadd(base,(Vec3){(float)side*.1f,.18f,-.23f-(float)i*.09f}),(Vec3){.025f,.09f,.048f},accent);
            mesh->glow=0;
        }else {
            for(int i=0;i<4;i++)mesh_box(mesh,vadd(base,(Vec3){0,.25f,-.3f-(float)i*.05f}),(Vec3){.16f,.025f,.022f},dark);
            mesh_box(mesh,vadd(base,(Vec3){0,.32f,.02f}),(Vec3){.09f,.05f,.055f},accent);
        }
    }
    if(p->weapon==WEAPON_ROCKET) {
        int sides=graphics_detail(app->quality)>0?10:6;
        mesh_cylinder(mesh,vadd(base,(Vec3){0,.12f,.20f}),vadd(base,(Vec3){0,.12f,-.59f}),.14f,metal,sides);
        mesh_cylinder(mesh,vadd(base,(Vec3){0,.12f,-.6f}),vadd(base,(Vec3){0,.12f,-.605f}),.10f,(Color){.04f,.05f,.055f},sides);
        mesh_beveled_box(mesh,vadd(base,(Vec3){0,-.10f,.10f}),(Vec3){.17f,.20f,.16f},.025f,dark);
        mesh_beveled_box(mesh,vadd(base,(Vec3){.10f,-.09f,.12f}),(Vec3){.15f,.13f,.20f},.025f,glove);
        mesh_box(mesh,vadd(base,(Vec3){.17f,-.20f,.27f}),(Vec3){.13f,.16f,.28f},skin);
        mesh_box(mesh,vadd(base,(Vec3){0,.29f,-.06f}),(Vec3){.06f,.065f,.22f},dark);
        mesh->glow=.7f;
        for(int i=0;i<3;i++)mesh_cylinder(mesh,vadd(base,(Vec3){0,.12f,-.32f-(float)i*.06f}),vadd(base,(Vec3){0,.12f,-.34f-(float)i*.06f}),.145f,accent,sides);
        mesh->glow=0;
    }
    if(helping) {
        Vec3 palm={-.17f,-.19f,-.62f};
        mesh_beveled_box(mesh,palm,(Vec3){.16f,.10f,.19f},.02f,glove);
        mesh_box(mesh,vadd(palm,(Vec3){-.04f,-.12f,.20f}),(Vec3){.13f,.18f,.29f},skin);
        mesh->glow=.8f;mesh_diamond(mesh,vadd(palm,(Vec3){0,.10f,0}),.055f,(Color){.4f,1,.7f});mesh->glow=0;
    }
    /* Weapon-specific reload: remove, feed, seat, then operate the action. */
    Vec3 magazine=vadd(base,(Vec3){-gesture.extract*.18f,-.16f-gesture.extract*.28f+gesture.seat*.03f,.02f+gesture.extract*.13f});
    if(p->weapon!=1&&p->weapon!=WEAPON_ROCKET) {
        if(p->weapon==3) {
            mesh_cylinder(mesh,magazine,vadd(magazine,(Vec3){0,.20f,0}),.065f,dark,6);
            mesh->glow=.65f;mesh_cylinder(mesh,vadd(magazine,(Vec3){0,.04f,0}),vadd(magazine,(Vec3){0,.16f,0}),.067f,accent,6);mesh->glow=0;
        }else {
            mesh_beveled_box(mesh,magazine,(Vec3){.12f,.20f,.14f},.02f,dark);
            mesh->glow=.5f;mesh_box(mesh,vadd(magazine,(Vec3){.065f,.04f,0}),(Vec3){.008f,.1f,.08f},accent);mesh->glow=0;
        }
    }
    mesh_box(mesh,vadd(base,(Vec3){.10f,.16f,.03f+gesture.rack*.07f}),(Vec3){.065f,.045f,.12f},metal);
    if(gesture.active) {
        Vec3 hand=vadd(magazine,(Vec3){-.09f,.02f,.08f});
        hand.y+=gesture.rack*.23f;hand.z-=gesture.rack*.14f;
        if(p->weapon==1) {
            float feed=clampf((reload-.18f)/.58f,0,1),cycle=fmodf(feed*3,1);
            float push=smooth(cycle/.60f);
            hand=vadd(base,(Vec3){-.15f,-.13f+.08f*push,-.05f-.055f*push});
            if(reload>.18f&&reload<.76f)for(int shell=0;shell<2;shell++) {
                Vec3 tip=vadd(hand,(Vec3){.01f+(float)shell*.07f,.10f,-.06f});
                mesh_cylinder(mesh,tip,vadd(tip,(Vec3){0,0,-.10f}),.027f,(Color){.76f,.52f,.16f},6);
            }
        }else if(p->weapon==WEAPON_ROCKET) {
            float feed=smooth((reload-.36f)/.35f);
            hand=vadd(base,(Vec3){-.16f+feed*.12f,.02f,-.77f+feed*.34f});
            if(reload>.20f&&reload<.74f) {
                Vec3 nose=vadd(base,(Vec3){-.12f*(1-feed),.12f,-.96f+feed*.4f});
                mesh_cylinder(mesh,nose,vadd(nose,(Vec3){0,0,.28f}),.06f,(Color){.49f,.54f,.48f},6);
                mesh_diamond(mesh,nose,.074f,accent);
            }
        }
        mesh_beveled_box(mesh,hand,(Vec3){.16f,.13f,.18f},.025f,glove);
        mesh_box(mesh,vadd(hand,(Vec3){-.06f,-.10f,.16f}),(Vec3){.13f,.17f,.28f},skin);
    }
    if(g->shot_age<.055f&&p->weapon_switch==0) {
        mesh->glow=1;
        mesh_diamond(mesh,vadd(base,(Vec3){0,.17f,p->weapon>=2?-.66f:-.45f}),p->weapon==1?.14f:.085f,(Color){1,.85f,.40f});
        for(int i=0;i<4;i++) {
            float angle=(float)i*PI_F/2+g->shot_age*18;
            mesh_diamond(mesh,vadd(base,(Vec3){sinf(angle)*.11f,.17f+cosf(angle)*.11f,p->weapon>=2?-.67f:-.46f}),.022f,(Color){1,.66f,.25f});
        }
        mesh->glow=0;
    }
    float air=motion&&!p->body.grounded?clampf(-p->body.velocity.y*.002f,-.03f,.03f):0;
    mesh_rotate(mesh,0,vadd(base,(Vec3){0,.08f,.15f}),
        g->recoil*(p->weapon==1?.12f:.08f)+reload_pose*.14f+gesture.rack*.04f+hurry*.025f*motion+air,
        reload_pose*.08f-switching*.10f,reload_pose*.23f-gesture.seat*.035f-p->strafe_blend*.025f*motion);
    if(punch>0) {
        Vec3 fist={-.18f+punch*.13f,-.39f+punch*.16f,-.36f-punch*.42f};
        mesh_box(mesh,fist,(Vec3){.20f,.19f,.23f},glove);
        mesh_box(mesh,vadd(fist,(Vec3){-.03f,-.08f,.23f}),(Vec3){.15f,.19f,.35f},skin);
    }
}
void animate_barrel(Mesh *m,const Barrel *b,float time) {
    if(b->health<=0) {
        if(b->explosion_time<=0)return;
        m->glow=1;
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
        m->glow=0;
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
