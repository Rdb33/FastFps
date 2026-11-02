#include "fastfps.h"
#include <math.h>
#include <string.h>

static void solid(World *w,Vec3 p,Vec3 size,Color color) {
    if(w->solid_count>=MAX_SOLIDS)return;
    w->solids[w->solid_count++]=(Box){
        {p.x-size.x/2,p.y,p.z-size.z/2},{p.x+size.x/2,p.y+size.y,p.z+size.z/2}};
    mesh_beveled_box(&w->mesh,p,size,.06f,color);
}
static Vec3 deck_point(const Deck *d,Vec3 p) {
    float c=cosf(d->yaw),s=sinf(d->yaw);
    return vadd(d->base,(Vec3){p.x*c-p.z*s,p.y,p.x*s+p.z*c});
}
static Vec3 deck_size(const Deck *d,Vec3 size) {
    float c=fabsf(cosf(d->yaw)),s=fabsf(sinf(d->yaw));
    return (Vec3){size.x*c+size.z*s,size.y,size.x*s+size.z*c};
}
static void deck_box(World *w,const Deck *d,Vec3 p,Vec3 size,Color color,bool collision) {
    if(collision)solid(w,deck_point(d,p),deck_size(d,size),color);
    else mesh_box(&w->mesh,deck_point(d,p),deck_size(d,size),color);
}
bool world_blocked(const World *w,Vec3 p,float r,float h,int ignore) {
    for(int i=0;i<w->solid_count;i++) {
        const Box *b=&w->solids[i];
        if(i!=ignore&&p.y<b->max.y-0.001f&&p.y+h>b->min.y+0.001f
            &&p.x+r>b->min.x&&p.x-r<b->max.x&&p.z+r>b->min.z&&p.z-r<b->max.z)return true;
    }
    return false;
}
float world_ray(const World *w,Vec3 origin,Vec3 dir,float range) {
    for(int i=0;i<w->solid_count;i++)range=fminf(range,ray_box(origin,dir,w->solids[i],range));
    if(dir.y<-.00001f){float ground=-origin.y/dir.y;if(ground>=0)range=fminf(range,ground);}
    return range;
}
static int nav_index(float x,float z) {
    int cx=(int)floorf((x+28)/2),cz=(int)floorf((z+24)/2);
    cx=cx<0?0:(cx>=NAV_W?NAV_W-1:cx);cz=cz<0?0:(cz>=NAV_H?NAV_H-1:cz);
    return cz*NAV_W+cx;
}
static Vec3 nav_center(int i) {return (Vec3){(float)(i%NAV_W)*2-27,0,(float)(i/NAV_W)*2-23};}
static int neighbours(int i,int out[4]) {
    int n=0;if(i%NAV_W>0)out[n++]=i-1;if(i%NAV_W<NAV_W-1)out[n++]=i+1;
    if(i>=NAV_W)out[n++]=i-NAV_W;
    if(i<NAV_W*(NAV_H-1))out[n++]=i+NAV_W;
    return n;
}
static void navigation_fill(World *w,float x,float z,int16_t *distance) {
    int target=nav_index(x,z);
    if(!w->passable[target]) {
        float best=1e9f;
        for(int i=0;i<NAV_W*NAV_H;i++)if(w->passable[i]) {
            Vec3 p=nav_center(i);float d=(p.x-x)*(p.x-x)+(p.z-z)*(p.z-z);
            if(d<best){best=d;target=i;}
        }
    }
    for(int i=0;i<NAV_W*NAV_H;i++)distance[i]=-1;
    int16_t queue[NAV_W*NAV_H];int head=0,tail=0;
    queue[tail++]=(int16_t)target;distance[target]=0;
    while(head<tail) {
        int current=queue[head++],next[4],n=neighbours(current,next);
        for(int k=0;k<n;k++)if(w->passable[next[k]]&&distance[next[k]]<0) {
            distance[next[k]]=(int16_t)(distance[current]+1);queue[tail++]=(int16_t)next[k];
        }
    }
}
static Vec3 navigation_follow(const int16_t *distance,float x,float z) {
    int current=nav_index(x,z),best=current,d=distance[current],next[4],n=neighbours(current,next);
    for(int k=0;k<n;k++)if(distance[next[k]]>=0&&(d<0||distance[next[k]]<d)) {
        best=next[k];d=distance[next[k]];
    }
    return nav_center(best);
}
void navigation_update(World *w,float x,float z) {
    navigation_fill(w,x,z,w->distance);
}
Vec3 navigation_next(const World *w,float x,float z) {
    return navigation_follow(w->distance,x,z);
}
void navigation_actor(World *w,int actor,Vec3 target) {
    if(actor>=0&&actor<MAX_PLAYERS)navigation_fill(w,target.x,target.z,w->routes[actor]);
}
Vec3 navigation_for_actor(const World *w,int actor,float x,float z) {
    return navigation_follow(actor>=0&&actor<MAX_PLAYERS?w->routes[actor]:w->distance,x,z);
}
static void wall_label(Mesh *m,const char *text,Vec3 position,float yaw,Color color,float scale) {
    float previous=m->glow;m->glow=.65f;
    int first=m->count;mesh_text(m,-(float)strlen(text)*3,0,1,color,text);
    for(int i=first;i<m->count;i++){m->data[i].position.y=-m->data[i].position.y;m->data[i].normal=(Vec3){0,0,1};}
    for(int i=first;i+2<m->count;i+=3){Vertex swap=m->data[i+1];m->data[i+1]=m->data[i+2];m->data[i+2]=swap;}
    mesh_pose(m,first,position,yaw,0,scale);
    m->glow=previous;
}
static void wall_panel(Mesh *m,Vec3 p,Vec3 size,Color color) {
    mesh_beveled_box(m,p,size,.035f,color);
    if(size.x>size.z) {
        float side=p.z<0?1:-1;
        mesh_box(m,vadd(p,(Vec3){0,.13f,side*size.z*.6f}),(Vec3){size.x*.92f,.045f,.025f},(Color){.08f,.10f,.11f});
        mesh_box(m,vadd(p,(Vec3){0,size.y-.15f,side*size.z*.6f}),(Vec3){size.x*.92f,.045f,.025f},(Color){.08f,.10f,.11f});
    }else {
        float side=p.x<0?1:-1;
        mesh_box(m,vadd(p,(Vec3){side*size.x*.6f,.13f,0}),(Vec3){.025f,.045f,size.z*.92f},(Color){.08f,.10f,.11f});
        mesh_box(m,vadd(p,(Vec3){side*size.x*.6f,size.y-.15f,0}),(Vec3){.025f,.045f,size.z*.92f},(Color){.08f,.10f,.11f});
    }
}
const char *world_map_name(int id) {
    static const char *names[MAP_COUNT]={"REACTEUR", "FONDERIE", "BASTION"};
    return names[id>=0&&id<MAP_COUNT?id:0];
}
bool world_init(World *w) {
    memset(w,0,sizeof(*w));if(!mesh_init(&w->mesh,60000))return false;
    w->map_id=-1;world_set_map(w,0);return true;
}
void world_set_map(World *w,int id) {
    if(id<0||id>=MAP_COUNT||id==w->map_id)return;
    w->mesh.count=0;w->mesh.glow=0;w->solid_count=0;w->platform_count=0;w->map_id=id;w->revision++;
    world_routes(w);Mesh *m=&w->mesh;
    const Color accents[MAP_COUNT]={{.25f,.75f,1},{1,.45f,.12f},{.28f,.9f,.7f}};
    const Color walls[MAP_COUNT]={{.16f,.22f,.28f},{.29f,.20f,.14f},{.38f,.23f,.14f}};
    Color accent=accents[id],wall=walls[id];float wall_height=id==2?8:7;
    for(int x=-28;x<28;x+=4)for(int z=-24;z<24;z+=4) {
        unsigned hash=(unsigned)(x+28)*31u+(unsigned)(z+24)*17u;
        float shade=(float)(hash%7)*.008f;
        Color c=id==0?(Color){.13f+shade,.18f+shade,.22f+shade}:
            (id==1?(Color){.20f+shade,.17f+shade,.13f+shade}:(Color){.42f+shade,.29f+shade,.17f+shade});
        mesh_quad(m,(Vec3){x,0,z+4},(Vec3){x+4,0,z+4},(Vec3){x+4,0,z},(Vec3){x,0,z},c,(Vec3){0,1,0});
    }
    solid(w,(Vec3){0,0,-24},(Vec3){56,wall_height,1},wall);
    solid(w,(Vec3){0,0,24},(Vec3){56,wall_height,1},wall);
    solid(w,(Vec3){-28,0,0},(Vec3){1,wall_height,48},wall);
    solid(w,(Vec3){28,0,0},(Vec3){1,wall_height,48},wall);
    if(id==0) {
        /* Hall froid fermé : rotation autour du coeur, colonnes pour rompre
           les lignes de tir, balcons latéraux pour changer d'étage. */
        solid(w,(Vec3){0,8,0},(Vec3){56,.4f,48},(Color){.08f,.12f,.17f});
        for(int side=-1;side<=1;side+=2) {
            for(int x=-24;x<=24;x+=6) {
                wall_panel(m,(Vec3){x,1.2f,side*23.4f},(Vec3){5.5f,3.7f,.1f},wall);
                m->glow=.8f;mesh_box(m,(Vec3){x,5.5f,side*23.3f},(Vec3){3.8f,.16f,.1f},accent);m->glow=0;
            }
            for(int z=-16;z<=16;z+=8) {
                solid(w,(Vec3){side*26.8f,0,z},(Vec3){.55f,7.8f,.65f},(Color){.1f,.15f,.22f});
                m->glow=.6f;mesh_box(m,(Vec3){side*26.45f,1.5f,z},(Vec3){.06f,3,.13f},accent);m->glow=0;
            }
        }
        for(int x=-10;x<=10;x+=20)for(int z=-8;z<=8;z+=16) {
            solid(w,(Vec3){x,0,z},(Vec3){3.2f,5.5f,3.2f},(Color){.22f,.31f,.38f});
            mesh_box(m,(Vec3){x,5.5f,z},(Vec3){3.4f,.12f,3.4f},accent);
            m->glow=.7f;mesh_box(m,(Vec3){x,1.2f,z+1.63f},(Vec3){.15f,3.6f,.05f},accent);m->glow=0;
        }
        for(int x=-19;x<=19;x+=38)for(int z=-12;z<=12;z+=24)
            solid(w,(Vec3){x,0,z},(Vec3){3,.95f,2.5f},(Color){.23f,.33f,.39f});
        for(int z=-18;z<=18;z+=12) {
            mesh_box(m,(Vec3){0,7.4f,z},(Vec3){53,.35f,.65f},(Color){.12f,.19f,.25f});
            m->glow=.7f;mesh_box(m,(Vec3){0,7.35f,z},(Vec3){26,.08f,.4f},(Color){.58f,.85f,1});m->glow=0;
        }
    }else if(id==1) {
        /* Usine chaude : machines hautes, trois couloirs et traversées
           seulement au centre / extrémités, convoyeur longitudinal. */
        for(int side=-1;side<=1;side+=2) {
            solid(w,(Vec3){side*18,8,0},(Vec3){19,.4f,48},(Color){.12f,.10f,.09f});
            for(int z=-10;z<=10;z+=20) {
                solid(w,(Vec3){side*11,0,z},(Vec3){4,3.7f,9},(Color){.26f,.22f,.19f});
                for(int k=-2;k<=2;k+=4) {
                    Vec3 pipe={side*11,3.7f,(float)z+k};
                    mesh_cylinder(m,pipe,vadd(pipe,(Vec3){0,3,0}),.7f,(Color){.17f,.14f,.12f},8);
                    m->glow=1;mesh_cylinder(m,vadd(pipe,(Vec3){0,.1f,0}),vadd(pipe,(Vec3){0,.35f,0}),.73f,accent,8);m->glow=0;
                }
                m->glow=.8f;mesh_box(m,(Vec3){side*8.95f,1,z},(Vec3){.08f,1.4f,5.8f},(Color){1,.22f,.045f});m->glow=0;
            }
            solid(w,(Vec3){side*20,0,0},(Vec3){4,1,5},(Color){.32f,.22f,.12f});
            solid(w,(Vec3){0,0,side*13},(Vec3){6.5f,.9f,2},(Color){.30f,.22f,.14f});
            for(int z=-20;z<=20;z+=5) {
                solid(w,(Vec3){side*26.7f,0,z},(Vec3){.9f,8,.7f},(Color){.15f,.12f,.10f});
                m->glow=.7f;mesh_box(m,(Vec3){side*27.36f,1.5f,z},(Vec3){.08f,2.8f,3.7f},(Color){.85f,.28f,.06f});m->glow=0;
            }
            for(int k=-2;k<=2;k++) {
                float z=(float)k*8;
                mesh_box(m,(Vec3){side*18,7.2f,z},(Vec3){18,.55f,.75f},(Color){.25f,.18f,.10f});
                m->glow=.8f;mesh_box(m,(Vec3){side*18,7.18f,z},(Vec3){12,.08f,.2f},accent);m->glow=0;
            }
        }
        for(int x=-24;x<=24;x+=8)for(int side=-1;side<=1;side+=2) {
            mesh_box(m,(Vec3){x,0,side*23.4f},(Vec3){6.9f,3.5f,.15f},(Color){.16f,.13f,.11f});
            m->glow=.6f;mesh_box(m,(Vec3){x,4.4f,side*23.3f},(Vec3){4,.14f,.1f},accent);m->glow=0;
        }
    }else {
        /* Ruines à ciel ouvert : terrasses accessibles par bonds et escalade,
           accès alternatifs aux balcons, repérage par obélisques et soleil. */
        for(int side=-1;side<=1;side+=2) {
            for(int z=-8;z<=8;z+=16) {
                solid(w,(Vec3){side*11,0,z},(Vec3){9,.9f,7},(Color){.43f,.28f,.16f});
                solid(w,(Vec3){side*13,0,z},(Vec3){5,1.8f,4},(Color){.50f,.34f,.20f});
                solid(w,(Vec3){side*14,1.8f,z},(Vec3){1.6f,3.6f,1.7f},(Color){.27f,.24f,.19f});
                m->glow=.8f;mesh_box(m,(Vec3){side*14,2.5f,z+.87f},(Vec3){.12f,1.7f,.04f},accent);m->glow=0;
            }
            for(int z=-20;z<=20;z+=8) {
                float height=8+(float)((z+20)/8%3)*1.4f;
                mesh_box(m,(Vec3){side*28.7f,0,z},(Vec3){2.3f,height,5.4f},(Color){.38f,.25f,.15f});
                mesh_diamond(m,(Vec3){side*30.8f,height*.8f,z},2.7f,(Color){.42f,.27f,.16f});
            }
            for(int x=-24;x<=24;x+=8) {
                mesh_box(m,(Vec3){x,0,side*24.6f},(Vec3){5.5f,8+(x%3),2},(Color){.42f,.28f,.17f});
                mesh_diamond(m,(Vec3){x,9,side*27},3,(Color){.45f,.30f,.18f});
            }
        }
        solid(w,(Vec3){-16.3f,0,-8},(Vec3){2.2f,2.4f,2.4f},(Color){.39f,.29f,.19f});
        solid(w,(Vec3){14.8f,0,8},(Vec3){1.8f,2.4f,2.4f},(Color){.39f,.29f,.19f});
        solid(w,(Vec3){16.5f,0,8},(Vec3){2.2f,3.2f,2.4f},(Color){.44f,.32f,.20f});
        for(int side=-1;side<=1;side+=2) {
            solid(w,(Vec3){side*3.8f,0,-17},(Vec3){1.2f,5.8f,1.4f},(Color){.29f,.27f,.21f});
            mesh_box(m,(Vec3){side*2.6f,5.7f,-17},(Vec3){3.5f,.65f,1.5f},(Color){.40f,.36f,.27f});
        }
        m->glow=.7f;mesh_diamond(m,(Vec3){13,22,-44},3.3f,(Color){1,.68f,.30f});m->glow=0;
    }
    /* Socle du pouvoir d'équipe : même accès, habillage propre à la zone. */
    solid(w,(Vec3){0,0,0},(Vec3){9,1.8f,7},id==2?(Color){.33f,.30f,.22f}:wall);
    mesh_box(m,(Vec3){0,1.8f,0},(Vec3){9.15f,.04f,7.15f},accent);
    for(int i=0;i<6;i++) {
        solid(w,(Vec3){0,0,6.25f-i*.5f},(Vec3){3,.3f*(i+1),.5f},wall);
        mesh_box(m,(Vec3){0,.3f*(i+1)+.005f,6.46f-i*.5f},(Vec3){3,.018f,.04f},accent);
    }
    mesh_beveled_box(m,(Vec3){0,1.82f,0},(Vec3){1.85f,.28f,1.85f},.26f,wall);
    for(int side=-1;side<=1;side+=2) {
        mesh_cylinder(m,(Vec3){side*1.1f,2.08f,0},(Vec3){side*1.1f,3.05f,0},.12f,wall,8);
        for(int z=-18;z<=18;z+=36) {
            mesh_box(m,(Vec3){side*23,.016f,z},(Vec3){2,.025f,2},(Color){.1f,.27f,.18f});
            m->glow=.5f;mesh_ring(m,(Vec3){side*23,.028f,z},1.1f,.045f,(Color){.3f,.8f,.45f},12);m->glow=0;
        }
    }
    for(int side=-1;side<=1;side+=2) {
        m->glow=.5f;mesh_box(m,(Vec3){side*15,.018f,0},(Vec3){.055f,.012f,41},accent);m->glow=0;
    }
    wall_label(m,world_map_name(id),(Vec3){0,6.1f,-23.43f},0,accent,.13f);
    wall_label(m,id==0?"COEUR / SORTIE SUD":(id==1?"CONVOYEUR / QUAI SUD":"RUINES / TERRASSES"),
        (Vec3){0,6.1f,23.43f},PI_F,accent,.11f);
    /* Each deck is oriented toward its map's shuttle lane. */
    for(int k=0;k<MAX_DECKS;k++) {
        const Deck *d=&w->decks[k];float hx=d->size.x/2,hz=d->size.z/2;
        deck_box(w,d,(Vec3){0,0,0},d->size,(Color){.20f,.29f,.33f},true);
        for(int side=-1;side<=1;side+=2) {
            deck_box(w,d,(Vec3){(float)side*(hx-.6f),-d->base.y,hz-.5f},
                (Vec3){.3f,d->base.y,.3f},(Color){.13f,.19f,.23f},true);
            deck_box(w,d,(Vec3){0,d->size.y+.005f,(float)side*(hz-.06f)},
                (Vec3){d->size.x-.15f,.015f,.09f},(Color){.35f,.8f,1},false);
        }
        deck_box(w,d,(Vec3){0,d->size.y,hz-.2f},(Vec3){d->size.x-.4f,.75f,.24f},(Color){.13f,.20f,.25f},true);
        for(int cover=0;cover<2;cover++) {
            float x=(d->gate<0?1:-1)*(.7f+(float)cover*3.7f);
            deck_box(w,d,(Vec3){x,d->size.y,-hz+.35f},(Vec3){2.1f,.9f,.65f},(Color){.22f,.30f,.34f},true);
            deck_box(w,d,(Vec3){x,d->size.y+.9f,-hz+.35f},(Vec3){2.15f,.04f,.72f},accent,false);
        }
        m->glow=.65f;
        deck_box(w,d,(Vec3){d->gate,d->size.y+.014f,-hz+.07f},(Vec3){3.2f,.02f,.08f},(Color){1,.72f,.26f},false);
        mesh_ring(m,vadd(d->pickup,(Vec3){0,.02f,0}),1,.065f,(Color){.3f,.8f,.45f},16);m->glow=0;
        const Platform *lift=&w->platforms[k];Vec3 end=vadd(lift->base,lift->travel);
        Vec3 direction=vnormalize((Vec3){d->base.x-end.x,0,d->base.z-end.z});
        Vec3 side={direction.z,0,-direction.x};Vec3 panel=vadd(lift->base,vscale(side,2.35f));panel.y=.55f;
        mesh_beveled_box(m,panel,(Vec3){fabsf(side.x)*.13f+fabsf(direction.x)*.95f,.9f,
            fabsf(side.z)*.13f+fabsf(direction.z)*.95f},.025f,(Color){.10f,.18f,.23f});
        Vec3 label=vsub(panel,vscale(side,.08f));label.y=1.18f;
        wall_label(m,"LIFT",label,atan2f(side.x,-side.z),(Color){.35f,.8f,1},.035f);
    }
    Vec3 spawns[8]={{-21,0,-17},{21,0,-17},{-21,0,17},{21,0,17},{0,0,-19},{23,0,0},{-23,0,0},{0,0,19}};
    memcpy(w->spawns,spawns,sizeof(spawns));
    for(int i=0;i<NAV_W*NAV_H;i++) {
        Vec3 p=nav_center(i);p.y=.35f;
        w->passable[i]=!world_blocked(w,p,.8f,ENEMY_HEIGHT,-1);w->distance[i]=-1;
    }

    memset(w->routes,0xff,sizeof(w->routes));
    for(int k=0;k<w->platform_count;k++)w->platforms[k].solid=w->solid_count++;
    world_platforms_update(w,0);

}
