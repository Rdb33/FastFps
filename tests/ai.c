#include "network.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void audio_play(Sound s){(void)s;}void audio_play_at(Sound s,Vec3 p){(void)s;(void)p;}
void audio_listener(Vec3 p,float yaw){(void)p;(void)yaw;}void audio_suppressed(bool s){(void)s;}
static void setup(Game *g,World *w) {
    memset(g,0,sizeof(*g));memset(w,0,sizeof(*w));w->revision=1;g->world=w;g->time=1;g->wave=1;
    g->enemy_count=1;g->enemies[0]=(Enemy){.id=1,.health=100,.max_health=100,.target_actor=0,.body={{-6,0,0},{0,0,0},true}};
    g->actors[0].active=true;g->player.health=100;g->player.body=(Body){{6,0,0},{0,0,0},true};
    g->particle_limit=20;g->lives=3;g->wave_timer=-1;
}
static void navigation(Game *g,World *w) {
    setup(g,w);w->solid_count=1;w->solids[0]=(Box){{-1,0,-4},{1,4,4}};
    ai_update(g,FIXED_STEP);AiBrain *b=ai_brain(g,&g->enemies[0]);
    assert(ai_plan(g,&g->enemies[0],b,g->player.body.position));assert(b->path_count>2);
    for(int k=1;k<b->path_count;k++) {
        AiNode *from=&w->ai.nodes[b->path[k-1]],*to=&w->ai.nodes[b->path[k]];
        assert(ai_nav_segment(w,from->position,to->position,0));
    }
    Vec3 next=ai_waypoint(g,&g->enemies[0],b);assert(ai_nav_segment(w,g->enemies[0].body.position,next,0));
    w->solids[0]=(Box){{-1,0,-24},{1,5,24}};w->revision++;ai_update(g,FIXED_STEP);
    assert(!ai_plan(g,&g->enemies[0],b,g->player.body.position));assert(b->path_count>0);
    setup(g,w);w->solid_count=1;w->solids[0]=(Box){{-.5f,1.85f,-24},{.5f,5,24}};
    assert(ai_nav_segment(w,(Vec3){-2,0,0},(Vec3){2,0,0},0));
    assert(!ai_nav_segment(w,(Vec3){-2,0,0},(Vec3){2,0,0},1));
    w->solids[0]=(Box){{0,0,-3},{3,.9f,3}};
    assert(ai_nav_segment(w,(Vec3){-2,0,0},(Vec3){1,.9f,0},0));
    assert(!ai_nav_segment(w,(Vec3){-2,0,0},(Vec3){1,.9f,0},1));
    setup(g,w);g->enemies[0].body.position=(Vec3){0,0,0};g->player.body.position=(Vec3){0,0,10};
    g->projectile_count=1;g->projectiles[0]=(Projectile){.kind=1,.owner=0,.position={0,1,-6},.velocity={0,0,20}};
    Vec3 dodge=ai_move(g,&g->enemies[0],&g->player,true,FIXED_STEP);assert(fabsf(dodge.x)>.6f);
    puts("OK : A*, detour, lissage, chemin partiel, plafond et escalade selon gabarit");
}
static void memory(Game *g,World *w) {
    setup(g,w);ai_update(g,.12f);assert(w->ai.contacts[0].known);
    for(int k=0;k<20;k++) {
        g->time+=.13f;g->player.body.velocity.z=k%2?6:-6;ai_update(g,.13f);
    }
    AiContact *c=&w->ai.contacts[0];assert(c->reversals>.2f&&c->samples==AI_HISTORY);
    Vec3 aim=ai_predict(g,&g->enemies[0],&g->player,14);assert(isfinite(aim.x)&&vlength(vsub(aim,player_eye(&g->player,false)))<=5.01f);
    for(int k=0;k<8;k++){g->time+=.13f;g->player.body.velocity=(Vec3){6,0,0};g->player.body.grounded=false;ai_update(g,.13f);}
    assert(c->strafe_bias>.25f&&c->jump_bias>.2f);
    Vec3 observed=c->position;w->solid_count=1;w->solids[0]=(Box){{-1,0,-24},{1,5,24}};
    g->player.body.position.z=15;g->time+=.13f;ai_update(g,.13f);assert(vlength(vsub(c->position,observed))==0);
    g->time+=9;ai_update(g,9);assert(c->confidence==0);
    ai_reset(w);assert(!w->ai.initialized&&!w->ai.contacts[0].known);
    puts("OK : observations, zigzags, prediction bornee, occlusion, oubli et nouvelle partie");
}
static void teamwork(Game *g,World *w) {
    setup(g,w);g->enemy_count=4;g->player.body.position=(Vec3){0,0,10};
    for(int i=0;i<4;i++)g->enemies[i]=(Enemy){.id=i+1,.type=ENEMY_RUNNER,.health=100,.max_health=100,.target_actor=0,
        .body={{(float)i*2-3,0,0},{0,0,0},true}};
    ai_update(g,.12f);
    for(int i=0;i<4;i++)ai_move(g,&g->enemies[i],&g->player,true,.12f);
    assert(ai_brain(g,&g->enemies[0])->role==AI_PRESSURE);
    assert(ai_brain(g,&g->enemies[3])->role==AI_FLANK);
    Enemy *caster=&g->enemies[3];caster->type=ENEMY_CASTER;caster->shot_timer=3;g->enemies[0].body.position=(Vec3){0,0,5};
    g->time+=1;ai_move(g,caster,&g->player,true,.12f);assert(ai_brain(g,caster)->role==AI_COVER);
    for(int i=0;i<3;i++)g->enemies[i].health=0;
    g->time+=1;ai_move(g,caster,&g->player,true,.12f);assert(ai_brain(g,caster)->role==AI_SUPPORT);
    int id=caster->id;ai_brain(g,caster)->path_count=12;caster->id=99;ai_update(g,.12f);
    for(int i=0;i<MAX_ENEMIES;i++)assert(!w->ai.brains[i].used||w->ai.brains[i].id!=id);
    puts("OK : pression, flanc, couvert pendant recharge, adaptation aux pertes et identites stables");
}
static void lifts(Game *g,World *w) {
    setup(g,w);w->solid_count=2;w->solids[0]=(Box){{-2,3,0},{2,3.3f,6}};
    w->platform_count=1;w->platforms[0]=(Platform){.base={0,0,-2},.size={3,.28f,3},.travel={0,3.02f,0},.period=8,.solid=1};
    w->solids[1]=(Box){{-1.5f,0,-3.5f},{1.5f,.28f,-.5f}};
    g->enemies[0].body.position=(Vec3){0,0,-6};ai_update(g,.12f);
    AiBrain *b=ai_brain(g,&g->enemies[0]);assert(ai_plan(g,&g->enemies[0],b,(Vec3){0,3.3f,3}));
    int station=-1;for(int i=0;i<b->path_count;i++)if(w->ai.nodes[b->path[i]].platform==0&&w->ai.nodes[b->path[i]].position.y>3)station=i;
    assert(station>=0);b->path_cursor=station;g->enemies[0].body.position=(Vec3){0,.28f,-2};
    Vec3 goal=ai_waypoint(g,&g->enemies[0],b);assert(b->riding&&vlength(vsub(goal,g->enemies[0].body.position))==0);
    w->solids[1].min.y=3.02f;w->solids[1].max.y=3.3f;g->enemies[0].body.position.y=3.3f;
    goal=ai_waypoint(g,&g->enemies[0],b);assert(!b->riding&&goal.z>-.5f);
    ai_reset(w);g->time=0;world_platforms_update(w,0);g->player.body.position=(Vec3){0,3.3f,3};g->player.health=1000;
    g->enemies[0].body=(Body){{0,0,-6},{0,0,0},true};g->enemies[0].health=g->enemies[0].max_health=1000;
    bool reached=false;
    for(int i=0;i<1800;i++) {
        game_update(g,(Input){0},FIXED_STEP);
        if(g->enemies[0].body.position.y>3.2f&&g->enemies[0].body.position.z>0){reached=true;break;}
    }
    if(!reached)fprintf(stderr,"Ascenseur : position finale %.2f %.2f %.2f\n",g->enemies[0].body.position.x,g->enemies[0].body.position.y,g->enemies[0].body.position.z);
    assert(reached);
    puts("OK : planification par ascenseur, embarquement physique, trajet et sortie au palier");
}
static void simulation(Game *g,World *w) {
    setup(g,w);g->player.health=1000;g->enemies[0].health=g->enemies[0].max_health=1000;
    ai_update(g,.12f);w->solid_count=1;w->solids[0]=(Box){{-1,0,-4},{1,4,4}};ai_nav_build(w);
    for(int i=0;i<480;i++)game_update(g,(Input){0},FIXED_STEP);
    assert(hypotf(g->enemies[0].body.position.x-g->player.body.position.x,g->enemies[0].body.position.z-g->player.body.position.z)<3);
    assert(!world_blocked(w,g->enemies[0].body.position,ENEMY_RADIUS,ENEMY_HEIGHT,-1));
    puts("OK : poursuite reelle autour d'un mur sur huit secondes de simulation");
}
static void central_stairs(Game *g,World *w) {
    for(int map=0;map<MAP_COUNT;map++)for(int kind=0;kind<3;kind++)for(int side=0;side<4;side++) {
        assert(world_init(w));world_set_map(w,map);game_reset(g,w,20);
        world_set_map(w,map);g->map_id=map;g->wave=1;g->wave_timer=-1;
        g->player.body=(Body){{0,1.8f,1},{0,0,0},true};g->player.health=10000;
        static const Vec3 starts[]={{8,0,0},{-8,0,0},{0,0,-9},{0,0,10}};
        g->enemy_count=1;g->enemies[0]=(Enemy){.id=1,.type=kind==1?ENEMY_BRUTE:ENEMY_RUNNER,.elite=kind==2,
            .health=10000,.max_health=10000,.target_actor=0,.touch_timer=1000,.body={starts[side],{0,0,0},true}};
        bool reached=false;
        for(int i=0;i<1800;i++) {
            game_update(g,(Input){0},FIXED_STEP);
            Vec3 p=g->enemies[0].body.position;
            if(p.y>1.7f&&hypotf(p.x,p.z-1)<3){reached=true;break;}
        }
        if(!reached)fprintf(stderr,"Escaliers map %d type %d : %.2f %.2f %.2f\n",map,kind,
            g->enemies[0].body.position.x,g->enemies[0].body.position.y,g->enemies[0].body.position.z);
        assert(reached);mesh_free(&w->mesh);
    }
    puts("OK : poursuite du joueur au centre par les escaliers, trois maps, quatre approches et trois gabarits");
}
static void authority(Game *g,World *w) {
    setup(g,w);ai_update(g,.12f);g->prediction=true;uint32_t searches=w->ai.searches;
    float sample=w->ai.next_observation;ai_update(g,.12f);assert(sample==w->ai.next_observation&&searches==w->ai.searches);g->prediction=false;
    uint8_t before[NET_PACKET_MAX],after[NET_PACKET_MAX];size_t n=protocol_snapshot_write(before,sizeof(before),g);
    assert(n>0);w->ai.contacts[0].reversals=.7f;w->ai.brains[0].path_count=40;
    size_t m=protocol_snapshot_write(after,sizeof(after),g);assert(n==m&&memcmp(before,after,n)==0);
    puts("OK : prediction sans IA, memoire et chemins absents du protocole");
}
int main(void) {
    assert(SDL_Init(SDL_INIT_TIMER)==0);World *w=calloc(1,sizeof(*w));Game *g=calloc(1,sizeof(*g));assert(w&&g);
    navigation(g,w);memory(g,w);teamwork(g,w);lifts(g,w);simulation(g,w);central_stairs(g,w);authority(g,w);
    for(int map=0;map<MAP_COUNT;map++) {
        assert(world_init(w));world_set_map(w,map);game_reset(g,w,20);world_set_map(w,map);g->map_id=map;
        g->enemy_count=1;g->enemies[0]=(Enemy){.id=1,.health=100,.max_health=100,.body={w->spawns[0],{0,0,0},true}};
        Uint64 start=SDL_GetPerformanceCounter();ai_update(g,.1f);
        printf("Map %d : %d noeuds / %zu octets IA ; construction PC %.2f ms\n",map,w->ai.node_count,sizeof(AiState),1000.0*(double)(SDL_GetPerformanceCounter()-start)/SDL_GetPerformanceFrequency());
        assert(w->ai.node_count>100&&w->ai.node_count<AI_NAV_NODES);mesh_free(&w->mesh);
    }
    free(w);free(g);SDL_Quit();return 0;
}
