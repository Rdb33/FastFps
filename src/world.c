#include "fastfps.h"
#include <math.h>
#include <string.h>

static void solid(World *w,Vec3 p,Vec3 size,Color color) {
    if(w->solid_count>=MAX_SOLIDS)return;
    w->solids[w->solid_count++]=(Box){
        {p.x-size.x/2,p.y,p.z-size.z/2},{p.x+size.x/2,p.y+size.y,p.z+size.z/2}};
    mesh_box(&w->mesh,p,size,color);
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
bool world_init(World *w) {
    memset(w,0,sizeof(*w));if(!mesh_init(&w->mesh,12000))return false;
    Mesh *m=&w->mesh;Color accent={.9f,.39f,.15f};
    for(int x=-28;x<28;x+=4)for(int z=-24;z<24;z+=4) {
        Color c=((x+z)/4)%2?(Color){.15f,.17f,.14f}:(Color){.19f,.21f,.18f};
        mesh_quad(m,(Vec3){x,0,z+4},(Vec3){x+4,0,z+4},(Vec3){x+4,0,z},(Vec3){x,0,z},c,(Vec3){0,1,0});
    }
    solid(w,(Vec3){0,0,-24},(Vec3){56,7,1},(Color){.26f,.25f,.21f});
    solid(w,(Vec3){0,0,24},(Vec3){56,7,1},(Color){.26f,.25f,.21f});
    solid(w,(Vec3){-28,0,0},(Vec3){1,7,48},(Color){.21f,.22f,.18f});
    solid(w,(Vec3){28,0,0},(Vec3){1,7,48},(Color){.21f,.22f,.18f});
    for(int sign=-1;sign<=1;sign+=2) {
        mesh_box(m,(Vec3){0,1.1f,sign*23.45f},(Vec3){55,.10f,.04f},accent);
        mesh_box(m,(Vec3){0,5,sign*23.45f},(Vec3){55,.15f,.04f},accent);
        mesh_box(m,(Vec3){sign*27.45f,1.1f,0},(Vec3){.04f,.1f,47},accent);
        mesh_box(m,(Vec3){sign*27.45f,5,0},(Vec3){.04f,.15f,47},accent);
        for(int x=-24;x<=24;x+=8)mesh_box(m,(Vec3){x,0,sign*23.45f},(Vec3){.14f,7,.05f},(Color){.36f,.34f,.26f});
    }
    for(int x=-10;x<=10;x+=20)for(int z=-8;z<=8;z+=16) {
        solid(w,(Vec3){x,0,z},(Vec3){3.2f,4.2f,3.2f},(Color){.3f,.29f,.24f});
        mesh_box(m,(Vec3){x,4.2f,z},(Vec3){3.35f,.1f,3.35f},accent);
        mesh_box(m,(Vec3){x,.8f,z},(Vec3){3.28f,.14f,3.28f},(Color){.55f,.2f,.1f});
    }
    solid(w,(Vec3){0,0,0},(Vec3){9,1.8f,7},(Color){.27f,.29f,.24f});
    mesh_box(m,(Vec3){0,1.8f,0},(Vec3){9.15f,.04f,7.15f},(Color){.47f,.49f,.3f});
    for(int i=0;i<6;i++) {
        solid(w,(Vec3){0,0,6.25f-i*.5f},(Vec3){3,.3f*(i+1),.5f},(Color){.28f,.29f,.25f});
        mesh_box(m,(Vec3){0,.3f*(i+1)+.005f,6.46f-i*.5f},(Vec3){3,.018f,.04f},accent);
    }
    for(int x=-19;x<=19;x+=38)for(int z=-12;z<=12;z+=24)
        solid(w,(Vec3){x,0,z},(Vec3){3,1.15f,2.5f},(Color){.32f,.35f,.25f});
    for(int x=-15;x<=15;x+=30)mesh_box(m,(Vec3){x,.003f,0},(Vec3){.055f,.006f,41},(Color){.44f,.4f,.2f});
    for(int z=-17;z<=17;z+=34)mesh_box(m,(Vec3){0,.003f,z},(Vec3){45,.006f,.055f},(Color){.44f,.4f,.2f});
    Vec3 spawns[8]={{-21,0,-17},{21,0,-17},{-21,0,17},{21,0,17},{0,0,-19},{23,0,0},{-23,0,0},{0,0,19}};
    memcpy(w->spawns,spawns,sizeof(spawns));
    for(int i=0;i<NAV_W*NAV_H;i++) {
        Vec3 p=nav_center(i);p.y=.35f;
        w->passable[i]=!world_blocked(w,p,.8f,ENEMY_HEIGHT,-1);w->distance[i]=-1;
    }
    return true;
}
