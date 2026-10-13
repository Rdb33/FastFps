#include "fastfps.h"
#include <math.h>
#include <string.h>

static void solid(World *w,Vec3 p,Vec3 size,Color color) {
    if(w->solid_count>=MAX_SOLIDS)return;
    w->solids[w->solid_count++]=(Box){
        {p.x-size.x/2,p.y,p.z-size.z/2},{p.x+size.x/2,p.y+size.y,p.z+size.z/2}};
    mesh_beveled_box(&w->mesh,p,size,.06f,color);
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
void navigation_update(World *w,float x,float z) {
    int target=nav_index(x,z);
    if(!w->passable[target]) {
        float best=1e9f;
        for(int i=0;i<NAV_W*NAV_H;i++)if(w->passable[i]) {
            Vec3 p=nav_center(i);float d=(p.x-x)*(p.x-x)+(p.z-z)*(p.z-z);
            if(d<best){best=d;target=i;}
        }
    }
    for(int i=0;i<NAV_W*NAV_H;i++)w->distance[i]=-1;
    int16_t queue[NAV_W*NAV_H];int head=0,tail=0;
    queue[tail++]=(int16_t)target;w->distance[target]=0;
    while(head<tail) {
        int current=queue[head++],next[4],n=neighbours(current,next);
        for(int k=0;k<n;k++)if(w->passable[next[k]]&&w->distance[next[k]]<0) {
            w->distance[next[k]]=(int16_t)(w->distance[current]+1);queue[tail++]=(int16_t)next[k];
        }
    }
}
Vec3 navigation_next(const World *w,float x,float z) {
    int current=nav_index(x,z),best=current,d=w->distance[current],next[4],n=neighbours(current,next);
    for(int k=0;k<n;k++)if(w->distance[next[k]]>=0&&(d<0||w->distance[next[k]]<d)) {
        best=next[k];d=w->distance[next[k]];
    }
    return nav_center(best);
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
    w->mesh.count=0;w->mesh.glow=0;w->solid_count=0;w->map_id=id;w->revision++;
    Mesh *m=&w->mesh;
    const Color accents[MAP_COUNT]={{.9f,.39f,.15f},{.63f,.3f,.93f},{.26f,.76f,.66f}};
    Color accent=accents[id];
    for(int x=-28;x<28;x+=4)for(int z=-24;z<24;z+=4) {
        Color c=((x+z)/4)%2?(Color){.15f,.17f,.14f}:(Color){.19f,.21f,.18f};
        if(id==1)c=(Color){c.r*.85f,c.g*.83f,c.b*1.25f};
        if(id==2)c=(Color){c.r*.80f,c.g*1.15f,c.b*1.15f};
        mesh_quad(m,(Vec3){x,0,z+4},(Vec3){x+4,0,z+4},(Vec3){x+4,0,z},(Vec3){x,0,z},c,(Vec3){0,1,0});
    }
    solid(w,(Vec3){0,0,-24},(Vec3){56,7,1},(Color){.26f,.25f,.21f});
    solid(w,(Vec3){0,0,24},(Vec3){56,7,1},(Color){.26f,.25f,.21f});
    solid(w,(Vec3){-28,0,0},(Vec3){1,7,48},(Color){.21f,.22f,.18f});
    solid(w,(Vec3){28,0,0},(Vec3){1,7,48},(Color){.21f,.22f,.18f});
    for(int side=-1;side<=1;side+=2) {
        for(int x=-24;x<=24;x+=4) {
            Color panel=(x%8==0)?(Color){.21f,.27f,.30f}:(Color){.25f,.28f,.28f};
            wall_panel(m,(Vec3){x,1.55f,side*23.41f},(Vec3){3.72f,2.75f,.10f},panel);
            mesh_beveled_box(m,(Vec3){x,4.56f,side*23.40f},(Vec3){1.9f,.54f,.10f},.04f,(Color){.10f,.13f,.15f});
            m->glow=.8f;
            mesh_box(m,(Vec3){x,4.77f,side*23.32f},(Vec3){1.5f,.075f,.04f},(Color){.42f,.77f,.93f});
            m->glow=0;
        }
        for(int z=-20;z<=20;z+=4) {
            wall_panel(m,(Vec3){side*27.41f,1.55f,z},(Vec3){.10f,2.75f,3.72f},(Color){.20f,.25f,.28f});
            mesh_box(m,(Vec3){side*27.32f,1.58f,z},(Vec3){.04f,.32f,2.7f},(Color){.10f,.13f,.16f});
        }
        for(int z=-19;z<=19;z+=38) {
            mesh_cylinder(m,(Vec3){side*26.75f,.4f,z},(Vec3){side*26.75f,6.6f,z},.20f,(Color){.21f,.25f,.28f},8);
            for(int y=1;y<=6;y+=2)mesh_box(m,(Vec3){side*26.75f,y,z},(Vec3){.58f,.13f,.58f},(Color){.10f,.13f,.15f});
        }
    }
    for(int sign=-1;sign<=1;sign+=2) {
        mesh_box(m,(Vec3){0,1.1f,sign*23.45f},(Vec3){55,.10f,.04f},accent);
        mesh_box(m,(Vec3){0,5,sign*23.45f},(Vec3){55,.15f,.04f},accent);
        mesh_box(m,(Vec3){sign*27.45f,1.1f,0},(Vec3){.04f,.1f,47},accent);
        mesh_box(m,(Vec3){sign*27.45f,5,0},(Vec3){.04f,.15f,47},accent);
        for(int x=-24;x<=24;x+=8)mesh_box(m,(Vec3){x,0,sign*23.45f},(Vec3){.14f,7,.05f},(Color){.36f,.34f,.26f});
    }
    if(id==0)for(int x=-10;x<=10;x+=20)for(int z=-8;z<=8;z+=16) {
        solid(w,(Vec3){x,0,z},(Vec3){3.2f,4.2f,3.2f},(Color){.3f,.29f,.24f});
        mesh_box(m,(Vec3){x,4.2f,z},(Vec3){3.35f,.1f,3.35f},accent);
        mesh_box(m,(Vec3){x,.8f,z},(Vec3){3.28f,.14f,3.28f},(Color){.55f,.2f,.1f});
        for(int side=-1;side<=1;side+=2) {
            mesh_beveled_box(m,(Vec3){x+side*1.61f,1.5f,z},(Vec3){.06f,1.5f,1.7f},.015f,(Color){.12f,.17f,.18f});
            m->glow=.55f;mesh_box(m,(Vec3){x+side*1.65f,1.85f,z},(Vec3){.025f,.8f,.075f},(Color){.38f,.79f,.95f});m->glow=0;
        }
    }
    solid(w,(Vec3){0,0,0},(Vec3){9,1.8f,7},(Color){.27f,.29f,.24f});
    mesh_box(m,(Vec3){0,1.8f,0},(Vec3){9.15f,.04f,7.15f},(Color){.47f,.49f,.3f});
    for(int i=0;i<6;i++) {
        solid(w,(Vec3){0,0,6.25f-i*.5f},(Vec3){3,.3f*(i+1),.5f},(Color){.28f,.29f,.25f});
        mesh_box(m,(Vec3){0,.3f*(i+1)+.005f,6.46f-i*.5f},(Vec3){3,.018f,.04f},accent);
    }
    if(id==0)for(int x=-19;x<=19;x+=38)for(int z=-12;z<=12;z+=24) {
        solid(w,(Vec3){x,0,z},(Vec3){3,1.15f,2.5f},(Color){.32f,.35f,.25f});
        mesh_beveled_box(m,(Vec3){x,.025f,z},(Vec3){3.1f,.12f,2.6f},.1f,(Color){.12f,.16f,.16f});
        mesh_beveled_box(m,(Vec3){x,1.05f,z},(Vec3){3.07f,.15f,2.57f},.1f,(Color){.15f,.20f,.20f});
        for(int side=-1;side<=1;side+=2) {
            mesh_box(m,(Vec3){x+side*.86f,.2f,z+1.27f},(Vec3){.10f,.73f,.06f},(Color){.14f,.18f,.19f});
            mesh_box(m,(Vec3){x+side*.86f,.2f,z-1.27f},(Vec3){.10f,.73f,.06f},(Color){.14f,.18f,.19f});
        }
        mesh_beveled_box(m,(Vec3){x,.40f,z+1.3f},(Vec3){.6f,.3f,.07f},.03f,(Color){.71f,.58f,.22f});
    }
    if(id==1) {
        /* Fonderie : deux lignes de machines, passages aux extrémités et au centre. */
        for(int side=-1;side<=1;side+=2)for(int z=-11;z<=11;z+=22) {
            Vec3 p={(float)side*11,0,(float)z};
            solid(w,p,(Vec3){3,3.5f,10},(Color){.27f,.21f,.34f});
            m->glow=.7f;mesh_box(m,vadd(p,(Vec3){0,3.5f,0}),(Vec3){3.1f,.09f,10.1f},accent);m->glow=0;
            for(int k=-3;k<=3;k+=3)mesh_cylinder(m,vadd(p,(Vec3){0,3.6f,(float)k}),vadd(p,(Vec3){0,5,(float)k}),.5f,(Color){.20f,.17f,.24f},8);
        }
        for(int side=-1;side<=1;side+=2) {
            solid(w,(Vec3){(float)side*20,0,0},(Vec3){4,1.2f,5},(Color){.35f,.25f,.31f});
            solid(w,(Vec3){0,0,(float)side*13},(Vec3){7,1.1f,2},(Color){.31f,.22f,.35f});
        }
    }else if(id==2) {
        /* Bastion : murs décalés, traversées latérales et couvertures basses. */
        for(int side=-1;side<=1;side+=2) {
            for(int lane=-1;lane<=1;lane+=2) {
                Vec3 p={(float)side*13,0,(float)lane*10};
                solid(w,p,(Vec3){11,2.7f,2},(Color){.19f,.32f,.31f});
                mesh_box(m,vadd(p,(Vec3){0,2.7f,0}),(Vec3){11.1f,.08f,2.1f},accent);
            }
            solid(w,(Vec3){(float)side*8,0,0},(Vec3){2,4.5f,3},(Color){.24f,.36f,.33f});
            solid(w,(Vec3){(float)side*19,0,0},(Vec3){3,1.1f,3},(Color){.23f,.29f,.27f});
            solid(w,(Vec3){0,0,(float)side*15},(Vec3){5,1.1f,2},(Color){.22f,.32f,.28f});
        }
    }
    for(int x=-15;x<=15;x+=30)mesh_box(m,(Vec3){x,.003f,0},(Vec3){.055f,.006f,41},(Color){.44f,.4f,.2f});
    for(int z=-17;z<=17;z+=34)mesh_box(m,(Vec3){0,.003f,z},(Vec3){45,.006f,.055f},(Color){.44f,.4f,.2f});
    for(int side=-1;side<=1;side+=2) {
        m->glow=.5f;
        mesh_box(m,(Vec3){side*15,.018f,0},(Vec3){.055f,.012f,41},(Color){.23f,.61f,.75f});
        m->glow=0;
        for(int z=-16;z<=16;z+=8) {
            mesh_box(m,(Vec3){side*15,.014f,z},(Vec3){1.1f,.02f,1.5f},(Color){.08f,.11f,.12f});
            for(int i=0;i<6;i++)mesh_box(m,(Vec3){side*15-.44f+(float)i*.175f,.033f,z},(Vec3){.035f,.018f,1.25f},(Color){.23f,.28f,.29f});
        }
    }
    for(int z=-18;z<=18;z+=36) {
        mesh_beveled_box(m,(Vec3){0,6.5f,z},(Vec3){52,.42f,.65f},.10f,(Color){.13f,.18f,.22f});
        mesh_box(m,(Vec3){0,6.93f,z},(Vec3){52,.12f,.88f},(Color){.25f,.30f,.31f});
    }
    mesh_beveled_box(m,(Vec3){0,1.82f,0},(Vec3){1.85f,.28f,1.85f},.26f,(Color){.14f,.20f,.24f});
    for(int side=-1;side<=1;side+=2) {
        mesh_cylinder(m,(Vec3){side*1.1f,2.08f,0},(Vec3){side*1.1f,3.05f,0},.12f,(Color){.24f,.33f,.35f},8);
        mesh_beveled_box(m,(Vec3){side*1.1f,2.95f,0},(Vec3){.38f,.15f,.44f},.05f,(Color){.13f,.19f,.22f});
    }
    wall_label(m,world_map_name(id),(Vec3){0,6.1f,-23.43f},0,(Color){.95f,.58f,.25f},.13f);
    wall_label(m,"SURVIE",(Vec3){0,6.1f,23.43f},PI_F,(Color){.35f,.75f,.9f},.15f);
    for(int side=-1;side<=1;side+=2) {
        for(int z=-18;z<=18;z+=36) {
            mesh_box(m,(Vec3){side*27.38f,2.8f,z},(Vec3){.08f,1.1f,2.2f},(Color){.10f,.14f,.13f});
            wall_label(m,side<0?(z<0?"01":"02"):(z<0?"03":"04"),
                (Vec3){side*27.31f,3.7f,z},side<0?PI_F*.5f:-PI_F*.5f,(Color){.4f,.85f,.68f},.16f);
            mesh_box(m,(Vec3){side*23,.016f,z},(Vec3){2,.025f,2},(Color){.10f,.27f,.18f});
            mesh_box(m,(Vec3){side*23,.02f,z},(Vec3){2.1f,.015f,.08f},(Color){.22f,.65f,.35f});
            mesh_box(m,(Vec3){side*23,.02f,z},(Vec3){.08f,.015f,2.1f},(Color){.22f,.65f,.35f});
        }
        mesh_box(m,(Vec3){side*25.9f,5.8f,0},(Vec3){.45f,.45f,43},(Color){.19f,.24f,.24f});
        for(int z=-16;z<=16;z+=16) {
            mesh_box(m,(Vec3){side*26.0f,4.9f,z},(Vec3){.65f,.55f,2},(Color){.1f,.12f,.12f});
            mesh_box(m,(Vec3){side*25.65f,5.04f,z},(Vec3){.04f,.25f,1.55f},(Color){.67f,.77f,.57f});
        }
    }
    Vec3 spawns[8]={{-21,0,-17},{21,0,-17},{-21,0,17},{21,0,17},{0,0,-19},{23,0,0},{-23,0,0},{0,0,19}};
    memcpy(w->spawns,spawns,sizeof(spawns));
    for(int i=0;i<NAV_W*NAV_H;i++) {
        Vec3 p=nav_center(i);p.y=.35f;
        w->passable[i]=!world_blocked(w,p,.8f,ENEMY_HEIGHT,-1);w->distance[i]=-1;
    }

}
