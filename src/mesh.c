#include "fastfps.h"
#include <stdlib.h>
#include <ctype.h>
#include <math.h>

bool mesh_init(Mesh *m,int capacity) {
    m->data=calloc((size_t)capacity,sizeof(Vertex));m->capacity=capacity;m->count=0;m->glow=0;
    return m->data!=NULL;
}
void mesh_free(Mesh *m) {free(m->data);m->data=NULL;m->count=0;m->capacity=0;}
void mesh_triangle(Mesh *m,Vec3 a,Vec3 b,Vec3 c,Color color,Vec3 normal) {
    if (m->count+3>m->capacity) return;
    m->data[m->count++]=(Vertex){a,color,normal,m->glow};
    m->data[m->count++]=(Vertex){b,color,normal,m->glow};
    m->data[m->count++]=(Vertex){c,color,normal,m->glow};
}
void mesh_quad(Mesh *m,Vec3 a,Vec3 b,Vec3 c,Vec3 d,Color color,Vec3 normal) {
    mesh_triangle(m,a,b,c,color,normal);mesh_triangle(m,a,c,d,color,normal);
}
void mesh_box(Mesh *m,Vec3 p,Vec3 s,Color color) {
    float a=p.x-s.x/2,b=p.x+s.x/2,c=p.y,d=p.y+s.y,e=p.z-s.z/2,f=p.z+s.z/2;
    mesh_quad(m,(Vec3){a,c,f},(Vec3){b,c,f},(Vec3){b,d,f},(Vec3){a,d,f},color,(Vec3){0,0,1});
    mesh_quad(m,(Vec3){b,c,e},(Vec3){a,c,e},(Vec3){a,d,e},(Vec3){b,d,e},color,(Vec3){0,0,-1});
    mesh_quad(m,(Vec3){b,c,f},(Vec3){b,c,e},(Vec3){b,d,e},(Vec3){b,d,f},color,(Vec3){1,0,0});
    mesh_quad(m,(Vec3){a,c,e},(Vec3){a,c,f},(Vec3){a,d,f},(Vec3){a,d,e},color,(Vec3){-1,0,0});
    mesh_quad(m,(Vec3){a,d,f},(Vec3){b,d,f},(Vec3){b,d,e},(Vec3){a,d,e},color,(Vec3){0,1,0});
    mesh_quad(m,(Vec3){a,c,e},(Vec3){b,c,e},(Vec3){b,c,f},(Vec3){a,c,f},color,(Vec3){0,-1,0});
}
void mesh_diamond(Mesh *m,Vec3 p,float r,Color color) {
    Vec3 v[4]={{p.x+r,p.y,p.z},{p.x,p.y,p.z+r},{p.x-r,p.y,p.z},{p.x,p.y,p.z-r}};
    for (int i=0;i<4;i++) {
        float angle=((float)i+.5f)*PI_F/2;
        mesh_triangle(m,(Vec3){p.x,p.y+r,p.z},v[(i+1)%4],v[i],color,
            vnormalize((Vec3){cosf(angle),1,sinf(angle)}));
        mesh_triangle(m,(Vec3){p.x,p.y-r,p.z},v[i],v[(i+1)%4],color,
            vnormalize((Vec3){cosf(angle),-1,sinf(angle)}));
    }
}
static Vec3 cross(Vec3 a,Vec3 b){return (Vec3){a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
void mesh_beveled_box(Mesh *m,Vec3 p,Vec3 size,float bevel,Color color) {
    float x=size.x/2,z=size.z/2,b=clampf(bevel,0,fminf(x,z)*.7f);
    Vec3 points[8]={{x-b,0,z},{x,0,z-b},{x,0,-z+b},{x-b,0,-z},
        {-x+b,0,-z},{-x,0,-z+b},{-x,0,z-b},{-x+b,0,z}};
    Vec3 top=vadd(p,(Vec3){0,size.y,0});
    for(int i=0;i<8;i++) {
        Vec3 a=vadd(p,points[i]),d=vadd(p,points[(i+1)%8]);
        Vec3 normal=vnormalize(cross(vsub(d,a),(Vec3){0,1,0}));
        mesh_quad(m,a,d,vadd(d,(Vec3){0,size.y,0}),vadd(a,(Vec3){0,size.y,0}),color,normal);
        mesh_triangle(m,top,vadd(a,(Vec3){0,size.y,0}),vadd(d,(Vec3){0,size.y,0}),color,(Vec3){0,1,0});
        mesh_triangle(m,p,d,a,color,(Vec3){0,-1,0});
    }
}
void mesh_cylinder(Mesh *m,Vec3 start,Vec3 end,float radius,Color color,int sides) {
    Vec3 direction=vsub(end,start);if(vlength(direction)<.0001f||radius<=0||sides<3)return;
    direction=vnormalize(direction);
    Vec3 axis=fabsf(direction.y)<.9f?(Vec3){0,1,0}:(Vec3){1,0,0};
    Vec3 u=vnormalize(cross(direction,axis)),v=cross(direction,u);
    for(int i=0;i<sides;i++) {
        float a=(float)i*2*PI_F/(float)sides,b=(float)(i+1)*2*PI_F/(float)sides;
        Vec3 na=vadd(vscale(u,cosf(a)),vscale(v,sinf(a))),nb=vadd(vscale(u,cosf(b)),vscale(v,sinf(b)));
        Vec3 p=vadd(start,vscale(na,radius)),q=vadd(start,vscale(nb,radius));
        Vec3 r=vadd(end,vscale(nb,radius)),s=vadd(end,vscale(na,radius));
        mesh_quad(m,p,q,r,s,color,vnormalize(vadd(na,nb)));
        mesh_triangle(m,start,q,p,color,vscale(direction,-1));
        mesh_triangle(m,end,s,r,color,direction);
    }
}
/* Un joint simple suffit pour des bras et jambes articulés sans squelette GPU. */
void mesh_joint_box(Mesh *m,Vec3 base,Vec3 size,Vec3 pivot,float angle,Color color) {
    int first=m->count;
    mesh_beveled_box(m,base,size,fminf(.045f,fminf(size.x,size.z)*.13f),color);
    float c=cosf(angle),s=sinf(angle);
    for(int i=first;i<m->count;i++) {
        Vertex *v=&m->data[i];
        float y=v->position.y-pivot.y,z=v->position.z-pivot.z;
        v->position.y=pivot.y+y*c-z*s;v->position.z=pivot.z+y*s+z*c;
        y=v->normal.y;z=v->normal.z;
        v->normal.y=y*c-z*s;v->normal.z=y*s+z*c;
    }
}
void mesh_pose(Mesh *m,int first,Vec3 position,float yaw,float pitch,float scale) {
    float cy=cosf(yaw),sy=sinf(yaw),cp=cosf(pitch),sp=sinf(pitch);
    for(int i=first;i<m->count;i++) {
        Vertex *v=&m->data[i];Vec3 p=v->position,n=v->normal;
        float y=p.y*cp-p.z*sp,z=p.y*sp+p.z*cp;
        v->position=(Vec3){position.x+(p.x*cy-z*sy)*scale,
            position.y+y*scale,position.z+(p.x*sy+z*cy)*scale};
        y=n.y*cp-n.z*sp;z=n.y*sp+n.z*cp;
        v->normal=(Vec3){n.x*cy-z*sy,y,n.x*sy+z*cy};
    }
}
void mesh_rect(Mesh *m,float x,float y,float w,float h,Color c) {
    mesh_quad(m,(Vec3){x,y,0},(Vec3){x+w,y,0},(Vec3){x+w,y+h,0},(Vec3){x,y+h,0},c,(Vec3){0,1,0});
}

void mesh_rotate(Mesh *m,int first,Vec3 pivot,float pitch,float yaw,float roll) {
    float cp=cosf(pitch),sp=sinf(pitch),cy=cosf(yaw),sy=sinf(yaw),cr=cosf(roll),sr=sinf(roll);
    for(int i=first;i<m->count;i++) {
        Vertex *v=&m->data[i];
        Vec3 p=vsub(v->position,pivot),n=v->normal;
        float y=p.y*cp-p.z*sp,z=p.y*sp+p.z*cp,x=p.x*cy-z*sy;
        z=p.x*sy+z*cy;v->position=vadd(pivot,(Vec3){x*cr-y*sr,x*sr+y*cr,z});
        y=n.y*cp-n.z*sp;z=n.y*sp+n.z*cp;x=n.x*cy-z*sy;
        z=n.x*sy+z*cy;v->normal=(Vec3){x*cr-y*sr,x*sr+y*cr,z};
    }
}

void mesh_ring(Mesh *m,Vec3 p,float radius,float width,Color color,int segments) {
    if(radius<=0||width<=0||segments<3)return;
    float inner=fmaxf(0,radius-width);
    for(int i=0;i<segments;i++) {
        float a=(float)i*2*PI_F/(float)segments,b=(float)(i+1)*2*PI_F/(float)segments;
        Vec3 v0={p.x+sinf(a)*radius,p.y,p.z+cosf(a)*radius};
        Vec3 v1={p.x+sinf(b)*radius,p.y,p.z+cosf(b)*radius};
        Vec3 v2={p.x+sinf(b)*inner,p.y,p.z+cosf(b)*inner};
        Vec3 v3={p.x+sinf(a)*inner,p.y,p.z+cosf(a)*inner};
        mesh_quad(m,v0,v1,v2,v3,color,(Vec3){0,1,0});
        mesh_quad(m,v3,v2,v1,v0,color,(Vec3){0,-1,0});
    }
}

/* Police 5 x 7 locale : pas de dépendance à SDL_ttf ni à des fichiers de police. */
static const uint8_t letters[26][7]={
 {14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{14,17,16,16,16,17,14},
 {30,17,17,17,17,17,30},{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},
 {14,17,16,23,17,17,15},{17,17,17,31,17,17,17},{14,4,4,4,4,4,14},
 {7,2,2,2,2,18,12},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},
 {17,27,21,21,17,17,17},{17,25,21,19,17,17,17},{14,17,17,17,17,17,14},
 {30,17,17,30,16,16,16},{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},
 {15,16,16,14,1,1,30},{31,4,4,4,4,4,4},{17,17,17,17,17,17,14},
 {17,17,17,17,17,10,4},{17,17,17,21,21,21,10},{17,17,10,4,10,17,17},
 {17,17,10,4,4,4,4},{31,1,2,4,8,16,31}
};
static const uint8_t numbers[10][7]={
 {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},
 {30,1,1,14,1,1,30},{2,6,10,18,31,2,2},{31,16,16,30,1,1,30},
 {14,16,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},
 {14,17,17,15,1,1,14}
};
static uint8_t punctuation(unsigned char c,int row) {
    switch(c) {
        case '-':return row==3?14:0;
        case '_':return row==6?31:0;
        case ':':return row==2||row==5?4:0;
        case '.':return row==6?4:0;
        case '/':return row>0&&row<6?(uint8_t)(1<<(row-1)):0;
        case '+':return row==3?31:(row==1||row==2||row==4||row==5?4:0);
        case '>':return row<4?(uint8_t)(1<<(4-row)):(uint8_t)(1<<(row-2));
        case '[':return row==0||row==6?14:8;
        case ']':return row==0||row==6?14:2;
        case '!':return row<4||row==6?4:0;
        case '(':return row==0||row==6?2:4;
        case ')':return row==0||row==6?8:4;
        case '%':{static const uint8_t bits[7]={25,26,2,4,8,11,19};return bits[row];}
        default:return 0;
    }
}
void mesh_text(Mesh *m,float x,float y,int scale,Color color,const char *text) {
    float start=x;
    for (;*text;text++) {
        unsigned char c=(unsigned char)toupper((unsigned char)*text);
        if(c=='\n'){x=start;y+=9*(float)scale;continue;}
        for(int row=0;row<7;row++) {
            uint8_t bits=c>='A'&&c<='Z'?letters[c-'A'][row]:
                (c>='0'&&c<='9'?numbers[c-'0'][row]:punctuation(c,row));
            for(int col=0;col<5;col++) if(bits&(1<<(4-col)))
                mesh_rect(m,x+col*(float)scale,y+row*(float)scale,(float)scale,(float)scale,color);
        }
        x+=6*(float)scale;
    }
}
