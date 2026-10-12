#include "fastfps.h"
#include <stdlib.h>
#include <ctype.h>
#include <math.h>

bool mesh_init(Mesh *m,int capacity) {
    m->data=calloc((size_t)capacity,sizeof(Vertex));m->capacity=capacity;m->count=0;
    return m->data!=NULL;
}
void mesh_free(Mesh *m) {free(m->data);m->data=NULL;m->count=0;m->capacity=0;}
void mesh_triangle(Mesh *m,Vec3 a,Vec3 b,Vec3 c,Color color,Vec3 normal) {
    if (m->count+3>m->capacity) return;
    m->data[m->count++]=(Vertex){a,color,normal};
    m->data[m->count++]=(Vertex){b,color,normal};
    m->data[m->count++]=(Vertex){c,color,normal};
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
        mesh_triangle(m,(Vec3){p.x,p.y+r,p.z},v[(i+1)%4],v[i],color,(Vec3){0,1,0});
        mesh_triangle(m,(Vec3){p.x,p.y-r,p.z},v[i],v[(i+1)%4],color,(Vec3){0,-1,0});
    }
}
/* Un joint simple suffit pour des bras et jambes articulés sans squelette GPU. */
void mesh_joint_box(Mesh *m,Vec3 base,Vec3 size,Vec3 pivot,float angle,Color color) {
    int first=m->count;
    mesh_box(m,base,size,color);
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
