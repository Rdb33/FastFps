#include "fastfps.h"
#include <math.h>

int graphics_detail(int quality){return quality==0?0:(quality==2?2:1);}

Color sector_theme(int wave) {
    static const Color themes[]={{.28f,.76f,1},{.72f,.40f,1},{1,.59f,.24f},{.25f,1,.70f}};
    return themes[((wave>0?wave-1:0)/5)%4];
}

static float floor_height(const World *world,Vec3 p) {
    float height=0;
    for(int i=0;i<world->solid_count;i++) {
        const Box *b=&world->solids[i];
        if(p.x>=b->min.x&&p.x<=b->max.x&&p.z>=b->min.z&&p.z<=b->max.z&&b->max.y<=p.y+.1f)
            height=fmaxf(height,b->max.y);
    }
    return height;
}
static void shadow(Mesh *m,const World *world,Vec3 p,float radius) {
    float height=floor_height(world,p),fade=clampf(1-(p.y-height)*.2f,.35f,1);
    p.y=height+.027f;
    mesh_ring(m,p,radius,.20f,(Color){.12f,.135f,.12f},12);
    mesh_ring(m,vadd(p,(Vec3){0,.001f,0}),fmaxf(.1f,radius-.16f),radius,(Color){.085f/fade,.10f/fade,.09f/fade},12);
}

void animate_arena(Mesh *m,const Game *g,const App *app) {
    float time=g->time;
    if(app->state==STATE_MENU||app->state==STATE_NETWORK||app->state==STATE_LOBBY)time+=app->ui_time;
    Color theme=sector_theme(g->wave);
    Vec3 core={0,4.15f,0};
    float pulse=.88f+.12f*sinf(time*2.2f);
    Color glow={theme.r*pulse,theme.g*pulse,theme.b*pulse};
    m->glow=1;
    mesh_diamond(m,core,.54f+.025f*sinf(time*3),glow);
    mesh_diamond(m,core,.27f,(Color){.84f,.96f,1});
    for(int i=0;i<3;i++) {
        int first=m->count;
        mesh_ring(m,(Vec3){0,0,0},.78f+(float)i*.16f,.06f,glow,20);
        mesh_pose(m,first,core,time*.4f+(float)i*PI_F/3,time*.24f+(float)i*.8f,1);
    }
    mesh_ring(m,(Vec3){0,1.85f,0},1.25f,.07f,glow,24);
    for(int i=0;i<4;i++) {
        float angle=time*.9f+(float)i*PI_F/2;
        mesh_diamond(m,(Vec3){sinf(angle)*1.45f,3.4f+sinf(time*2+(float)i)*.22f,cosf(angle)*1.45f},.09f,glow);
    }
    for(int k=0;k<g->world->platform_count;k++) {
        const Platform *platform=&g->world->platforms[k];Box box=g->world->solids[platform->solid];
        Vec3 base={(box.min.x+box.max.x)/2,box.min.y,(box.min.z+box.max.z)/2};
        m->glow=0;mesh_beveled_box(m,base,platform->size,.08f,(Color){.18f,.26f,.3f});
        m->glow=.85f;
        for(int side=-1;side<=1;side+=2) {
            mesh_box(m,vadd(base,(Vec3){(float)side*1.43f,.28f,0}),(Vec3){.08f,.025f,2.8f},theme);
            mesh_box(m,vadd(base,(Vec3){0,.28f,(float)side*1.43f}),(Vec3){2.8f,.025f,.08f},theme);
        }
        mesh_ring(m,vadd(base,(Vec3){0,.30f,0}),.50f,.045f,theme,16);
        if(platform->travel.y>0) {
            mesh_ring(m,vadd(platform->base,(Vec3){0,.035f,0}),1.9f,.06f,theme,20);
            for(int step=0;step<4;step++)mesh_diamond(m,vadd(base,(Vec3){0,.5f+(float)step*.23f,0}),.04f,theme);
        }else mesh_box(m,(Vec3){0,.025f,10},(Vec3){13,.02f,.06f},theme);
        m->glow=0;
    }
    for(int k=0;k<g->blast_count;k++) {
        const Blast *b=&g->blasts[k];float life=b->time/.8f,age=1-life;
        m->glow=1;Color fire={life,life*.5f,life*.13f};
        mesh_ring(m,vadd(b->position,(Vec3){0,.04f,0}),.3f+age*b->radius,.15f,fire,24);
        if(age<.4f)mesh_diamond(m,b->position,.35f+age*2,fire);
        m->glow=0;
    }
    int detail=graphics_detail(app->quality);
    int sparks=detail==0?8:(detail==1?14:20);
    for(int i=0;i<sparks;i++) {
        float phase=time*.7f+(float)i*2.4f;
        Vec3 p={(i%2?1:-1)*(14.5f+sinf(phase)*.25f),.4f+fmodf(time*.45f+(float)i*.73f,3.2f),
            -18+(float)(i%8)*5.1f+cosf(phase)*.22f};
        mesh_diamond(m,p,.022f+(sinf(phase)*.5f+.5f)*.02f,glow);
    }
    m->glow=0;
    for(int i=0;i<g->enemy_count;i++)shadow(m,g->world,g->enemies[i].body.position,.58f*enemy_size(&g->enemies[i]));
    for(int i=0;i<g->corpse_count;i++)shadow(m,g->world,g->corpses[i].body.position,.66f);
    for(int i=0;i<MAX_PLAYERS;i++)if(g->actors[i].active) {
        const Player *p=i==g->local_actor?&g->player:&g->actors[i].player;
        if(i!=g->local_actor)shadow(m,g->world,p->body.position,.48f);
        if(g->cooperative&&g->actors[i].dead) {
            m->glow=1;Vec3 marker=vadd(p->body.position,(Vec3){0,1.25f+.1f*sinf(time*3),0});
            mesh_diamond(m,marker,.13f,(Color){1,.53f,.18f});
            mesh_box(m,vadd(marker,(Vec3){0,.20f,0}),(Vec3){.06f,.27f,.06f},(Color){.75f,1,.95f});
            mesh_box(m,vadd(marker,(Vec3){0,.29f,0}),(Vec3){.27f,.06f,.06f},(Color){.75f,1,.95f});
            m->glow=0;
        }
        if(p->upgrade_flash>0) {
            m->glow=.8f;
            float age=1.4f-p->upgrade_flash;
            Vec3 base=p->body.position;base.y=floor_height(g->world,base)+.038f;
            mesh_ring(m,base,.6f+age*1.6f,.07f,(Color){.38f,.82f,1},24);
            for(int j=0;j<5;j++) {
                float angle=time*3+(float)j*2*PI_F/5;
                mesh_diamond(m,vadd(p->body.position,(Vec3){sinf(angle)*.7f,.2f+age*.8f,cosf(angle)*.7f}),.055f,glow);
            }
            m->glow=0;
        }
        if(p->dash_timer>0) {
            Vec3 behind=vsub(p->body.position,vscale(vnormalize(p->body.velocity),.55f));
            mesh_diamond(m,vadd(behind,(Vec3){0,.6f,0}),.10f,(Color){.23f,.61f,.8f});
        }
    }
    for(int i=0;i<g->pickup_count;i++)if(g->pickups[i].cooldown==0)
        mesh_ring(m,vadd(g->pickups[i].position,(Vec3){0,.048f,0}),.75f+.04f*sinf(time*2),.06f,(Color){.25f,.75f,.4f},16);
    if(g->wave_clear_time>0) {
        float age=2.8f-g->wave_clear_time;
        mesh_ring(m,(Vec3){0,1.86f,0},.4f+age*1.9f,.085f,(Color){.45f,.9f,.72f},32);
    }
    m->glow=0;
}
