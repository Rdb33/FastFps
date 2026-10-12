#include "fastfps.h"
#include <math.h>
#include <float.h>

float clampf(float v,float lo,float hi) {return fmaxf(lo,fminf(hi,v));}
Vec3 vadd(Vec3 a,Vec3 b) {return (Vec3){a.x+b.x,a.y+b.y,a.z+b.z};}
Vec3 vsub(Vec3 a,Vec3 b) {return (Vec3){a.x-b.x,a.y-b.y,a.z-b.z};}
Vec3 vscale(Vec3 v,float s) {return (Vec3){v.x*s,v.y*s,v.z*s};}
float vlength(Vec3 v) {return sqrtf(v.x*v.x+v.y*v.y+v.z*v.z);}
Vec3 vnormalize(Vec3 v) {float len=vlength(v);return len>0.00001f?vscale(v,1.0f/len):(Vec3){0,0,0};}
Vec3 look_direction(float yaw,float pitch) {
    float cp=cosf(pitch);
    return (Vec3){sinf(yaw)*cp,sinf(pitch),-cosf(yaw)*cp};
}
float ray_box(Vec3 origin,Vec3 direction,Box box,float range) {
    float enter=0,leave=range;
    float o[3]={origin.x,origin.y,origin.z},d[3]={direction.x,direction.y,direction.z};
    float low[3]={box.min.x,box.min.y,box.min.z},high[3]={box.max.x,box.max.y,box.max.z};
    for (int i=0;i<3;i++) {
        if (fabsf(d[i])<0.000001f) {
            if (o[i]<low[i] || o[i]>high[i]) return FLT_MAX;
        } else {
            float a=(low[i]-o[i])/d[i],b=(high[i]-o[i])/d[i];
            enter=fmaxf(enter,fminf(a,b));leave=fminf(leave,fmaxf(a,b));
            if (enter>leave) return FLT_MAX;
        }
    }
    return enter<=range?enter:FLT_MAX;
}
Mat4 perspective(float fov,float aspect,float near_plane,float far_plane) {
    Mat4 out={{0}};float f=1.0f/tanf(fov/2);
    out.m[0]=f/aspect;out.m[5]=f;out.m[10]=(far_plane+near_plane)/(near_plane-far_plane);
    out.m[11]=-1;out.m[14]=2*far_plane*near_plane/(near_plane-far_plane);return out;
}
Mat4 view_matrix(Vec3 eye,float yaw,float pitch) {
    float sy=sinf(yaw),cy=cosf(yaw),sp=sinf(pitch),cp=cosf(pitch);
    Vec3 r={cy,0,sy},u={-sy*sp,cp,cy*sp},f={sy*cp,sp,-cy*cp};
    Mat4 out={{r.x,u.x,-f.x,0,r.y,u.y,-f.y,0,r.z,u.z,-f.z,0,
        -(r.x*eye.x+r.z*eye.z),-(u.x*eye.x+u.y*eye.y+u.z*eye.z),
        f.x*eye.x+f.y*eye.y+f.z*eye.z,1}};
    return out;
}
Mat4 mat_multiply(Mat4 a,Mat4 b) {
    Mat4 out={{0}};
    for (int c=0;c<4;c++) for (int r=0;r<4;r++)
        for (int k=0;k<4;k++) out.m[c*4+r]+=a.m[k*4+r]*b.m[c*4+k];
    return out;
}
