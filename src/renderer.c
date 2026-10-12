#include "fastfps.h"
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const Preset PRESETS[3]={
    {640,360,30,28,"PI 4"},{960,540,60,64,"EQUILIBRE"},{1280,720,60,100,"PC"}
};
static const char *vertex_source=
    "attribute vec3 aPosition;attribute vec3 aColor;attribute vec3 aNormal;"
    "uniform mat4 uMatrix;uniform float uFog;uniform float uUnlit;"
    "varying vec3 vColor;varying float vFog;"
    "void main(){gl_Position=uMatrix*vec4(aPosition,1.0);"
    "float light=0.64+0.36*max(dot(aNormal,normalize(vec3(-0.4,0.9,0.25))),0.0);"
    "vColor=aColor*mix(light,1.0,uUnlit);"
    "vFog=clamp((gl_Position.w-16.0)/65.0,0.0,0.75)*uFog;}";
static const char *fragment_source=
    "precision mediump float;varying vec3 vColor;varying float vFog;"
    "void main(){gl_FragColor=vec4(mix(vColor,vec3(0.045,0.04,0.032),vFog),1.0);}";
static const char *composite_vertex=
    "attribute vec2 aPosition;attribute vec2 aUV;varying vec2 vUV;"
    "void main(){vUV=aUV;gl_Position=vec4(aPosition,0.0,1.0);}";
static const char *composite_fragment=
    "precision mediump float;uniform sampler2D uTexture;varying vec2 vUV;"
    "void main(){gl_FragColor=texture2D(uTexture,vUV);}";

static GLuint compile(GLenum type,const char *source) {
    GLuint shader=glCreateShader(type);glShaderSource(shader,1,&source,NULL);glCompileShader(shader);
    GLint ok=0;glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);
    if(!ok){char log[2048];glGetShaderInfoLog(shader,sizeof(log),NULL,log);SDL_Log("Shader : %s",log);glDeleteShader(shader);return 0;}
    return shader;
}
static GLuint program(const char *vs,const char *fs) {
    GLuint v=compile(GL_VERTEX_SHADER,vs),f=compile(GL_FRAGMENT_SHADER,fs);
    if(!v||!f){if(v)glDeleteShader(v);if(f)glDeleteShader(f);return 0;}
    GLuint p=glCreateProgram();glAttachShader(p,v);glAttachShader(p,f);glLinkProgram(p);
    glDeleteShader(v);glDeleteShader(f);GLint ok=0;glGetProgramiv(p,GL_LINK_STATUS,&ok);
    if(!ok){char log[2048];glGetProgramInfoLog(p,sizeof(log),NULL,log);SDL_Log("Programme : %s",log);glDeleteProgram(p);return 0;}
    return p;
}
static void allocate_buffer(GLuint *buffer,int vertices,GLenum usage,const void *data) {
    glGenBuffers(1,buffer);glBindBuffer(GL_ARRAY_BUFFER,*buffer);
    glBufferData(GL_ARRAY_BUFFER,(GLsizeiptr)vertices*(GLsizeiptr)sizeof(Vertex),data,usage);
}
bool renderer_resize(Renderer *r,const Preset *preset) {
    r->width=preset->width;r->height=preset->height;
    glBindTexture(GL_TEXTURE_2D,r->texture);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,r->width,r->height,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glBindRenderbuffer(GL_RENDERBUFFER,r->depth);
    glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT16,r->width,r->height);
    glBindFramebuffer(GL_FRAMEBUFFER,r->framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,r->texture,0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,r->depth);
    bool ok=glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER,0);
    if(!ok)SDL_Log("Impossible de creer la cible de rendu %dx%d",r->width,r->height);
    return ok;
}
bool renderer_init(Renderer *r,World *world,const Preset *preset) {
    memset(r,0,sizeof(*r));
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,16);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS,0);
    r->window=SDL_CreateWindow("FASTFPS - Secteur zero",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,
        960,540,SDL_WINDOW_OPENGL|SDL_WINDOW_RESIZABLE|SDL_WINDOW_ALLOW_HIGHDPI);
    if(!r->window){SDL_Log("Fenetre : %s",SDL_GetError());return false;}
    SDL_SetWindowMinimumSize(r->window,640,360);
    r->context=SDL_GL_CreateContext(r->window);
    if(!r->context){SDL_Log("OpenGL ES 2 : %s",SDL_GetError());return false;}
    SDL_GL_SetSwapInterval(0);
    SDL_Log("GPU : %s / %s",glGetString(GL_RENDERER),glGetString(GL_VERSION));
    r->program=program(vertex_source,fragment_source);r->composite_program=program(composite_vertex,composite_fragment);
    if(!r->program||!r->composite_program)return false;
    r->position=glGetAttribLocation(r->program,"aPosition");r->color=glGetAttribLocation(r->program,"aColor");
    r->normal=glGetAttribLocation(r->program,"aNormal");r->matrix=glGetUniformLocation(r->program,"uMatrix");
    r->fog=glGetUniformLocation(r->program,"uFog");r->unlit=glGetUniformLocation(r->program,"uUnlit");
    r->composite_position=glGetAttribLocation(r->composite_program,"aPosition");
    r->composite_uv=glGetAttribLocation(r->composite_program,"aUV");
    r->composite_texture=glGetUniformLocation(r->composite_program,"uTexture");
    if(!mesh_init(&r->dynamic,20000)||!mesh_init(&r->gun,800)||!mesh_init(&r->ui,65536))return false;
    r->world_vertices=world->mesh.count;
    allocate_buffer(&r->world_buffer,world->mesh.count,GL_STATIC_DRAW,world->mesh.data);
    allocate_buffer(&r->dynamic_buffer,r->dynamic.capacity,GL_DYNAMIC_DRAW,NULL);
    allocate_buffer(&r->gun_buffer,r->gun.capacity,GL_DYNAMIC_DRAW,NULL);
    allocate_buffer(&r->ui_buffer,r->ui.capacity,GL_DYNAMIC_DRAW,NULL);
    static const float quad[]={-1,-1,0,0,1,-1,1,0,1,1,1,1,-1,-1,0,0,1,1,1,1,-1,1,0,1};
    glGenBuffers(1,&r->quad_buffer);glBindBuffer(GL_ARRAY_BUFFER,r->quad_buffer);
    glBufferData(GL_ARRAY_BUFFER,sizeof(quad),quad,GL_STATIC_DRAW);
    glGenFramebuffers(1,&r->framebuffer);glGenTextures(1,&r->texture);glGenRenderbuffers(1,&r->depth);
    return renderer_resize(r,preset);
}
static void draw(Renderer *r,GLuint buffer,int count,Mat4 matrix,float fog,float unlit) {
    glBindBuffer(GL_ARRAY_BUFFER,buffer);
    glEnableVertexAttribArray((GLuint)r->position);glEnableVertexAttribArray((GLuint)r->color);glEnableVertexAttribArray((GLuint)r->normal);
    glVertexAttribPointer((GLuint)r->position,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void *)offsetof(Vertex,position));
    glVertexAttribPointer((GLuint)r->color,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void *)offsetof(Vertex,color));
    glVertexAttribPointer((GLuint)r->normal,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void *)offsetof(Vertex,normal));
    glUniformMatrix4fv(r->matrix,1,GL_FALSE,matrix.m);glUniform1f(r->fog,fog);glUniform1f(r->unlit,unlit);
    glDrawArrays(GL_TRIANGLES,0,count);
}
static void upload(GLuint buffer,const Mesh *m) {
    glBindBuffer(GL_ARRAY_BUFFER,buffer);glBufferSubData(GL_ARRAY_BUFFER,0,(GLsizeiptr)m->count*(GLsizeiptr)sizeof(Vertex),m->data);
}
void renderer_draw(Renderer *r,const Game *g,const App *app) {
    const Player *p=&g->player;float speed=hypotf(p->body.velocity.x,p->body.velocity.z);
    float fov=(87+(app->motion?speed*.2f:0))*PI_F/180;
    Mat4 projection=perspective(fov,(float)r->width/(float)r->height,.05f,100);
    Mat4 view=view_matrix(player_eye(p,app->motion),p->yaw,p->pitch);
    Mat4 matrix=mat_multiply(projection,view);
    glBindFramebuffer(GL_FRAMEBUFFER,r->framebuffer);glViewport(0,0,r->width,r->height);
    glEnable(GL_DEPTH_TEST);glEnable(GL_CULL_FACE);glDisable(GL_BLEND);
    glClearColor(.045f,.04f,.032f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);glUseProgram(r->program);
    draw(r,r->world_buffer,r->world_vertices,matrix,1,0);
    Mesh *m=&r->dynamic;m->count=0;
    for(int i=0;i<g->enemy_count;i++)animate_creature(m,&g->enemies[i],g->time);
    for(int i=0;i<g->corpse_count;i++)animate_creature(m,&g->corpses[i],g->time);
    if(g->cooperative)for(int i=0;i<MAX_PLAYERS;i++)if(g->actors[i].active&&i!=g->local_actor)
        animate_teammate(m,&g->actors[i],i);
    for(int i=0;i<g->projectile_count;i++) {
        const Projectile *bolt=&g->projectiles[i];
        mesh_diamond(m,bolt->position,.17f,bolt->color);
        if(i<12)mesh_diamond(m,vsub(bolt->position,vscale(bolt->velocity,.018f)),.075f,bolt->color);
    }
    for(int i=0;i<g->particle_count;i++) {
        const Particle *particle=&g->particles[i];
        mesh_diamond(m,particle->position,particle->size*particle->life/particle->max_life,particle->color);
    }
    for(int i=0;i<g->pickup_count;i++)if(g->pickups[i].cooldown==0) {
        int first=m->count;
        Vec3 base=g->pickups[i].position;base.y=.45f+sinf(g->time*2+base.x)*.1f;
        mesh_box(m,(Vec3){0,0,0},(Vec3){.23f,.65f,.23f},(Color){.3f,1,.4f});
        mesh_box(m,(Vec3){0,.2f,0},(Vec3){.65f,.23f,.23f},(Color){.3f,1,.4f});
        mesh_pose(m,first,base,g->time*.7f,0,1);
    }
    upload(r->dynamic_buffer,m);draw(r,r->dynamic_buffer,m->count,matrix,1,0);
    m=&r->gun;animate_weapon(m,g,app);
    glClear(GL_DEPTH_BUFFER_BIT);upload(r->gun_buffer,m);draw(r,r->gun_buffer,m->count,projection,0,0);
    glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);
    ui_build(&r->ui,g,app);upload(r->ui_buffer,&r->ui);
    Mat4 ortho={{2.0f/640,0,0,0,0,-2.0f/360,0,0,0,0,-1,0,-1,1,0,1}};
    draw(r,r->ui_buffer,r->ui.count,ortho,0,1);
    /* L'image basse résolution est agrandie sans augmenter la charge 3D. */
    glBindFramebuffer(GL_FRAMEBUFFER,0);
    int width,height;SDL_GL_GetDrawableSize(r->window,&width,&height);
    glViewport(0,0,width,height);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);
    int target_width=width,target_height=width*9/16;
    if(target_height>height){target_height=height;target_width=height*16/9;}
    glViewport((width-target_width)/2,(height-target_height)/2,target_width,target_height);
    glUseProgram(r->composite_program);glBindBuffer(GL_ARRAY_BUFFER,r->quad_buffer);
    for(GLuint i=0;i<3;i++)glDisableVertexAttribArray(i);
    glEnableVertexAttribArray((GLuint)r->composite_position);glEnableVertexAttribArray((GLuint)r->composite_uv);
    glVertexAttribPointer((GLuint)r->composite_position,2,GL_FLOAT,GL_FALSE,4*sizeof(float),NULL);
    glVertexAttribPointer((GLuint)r->composite_uv,2,GL_FLOAT,GL_FALSE,4*sizeof(float),(void *)(2*sizeof(float)));
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,r->texture);
    glUniform1i(r->composite_texture,0);glDrawArrays(GL_TRIANGLES,0,6);
    SDL_GL_SwapWindow(r->window);
}
bool renderer_screenshot(Renderer *r,const char *path) {
    size_t row=(size_t)r->width*4,size=row*(size_t)r->height;
    uint8_t *pixels=malloc(size),*flipped=malloc(size);
    if(!pixels||!flipped){free(pixels);free(flipped);return false;}
    glBindFramebuffer(GL_FRAMEBUFFER,r->framebuffer);glPixelStorei(GL_PACK_ALIGNMENT,1);
    glReadPixels(0,0,r->width,r->height,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
    for(int y=0;y<r->height;y++)memcpy(flipped+(size_t)y*row,pixels+(size_t)(r->height-y-1)*row,row);
    SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormatFrom(flipped,r->width,r->height,32,(int)row,SDL_PIXELFORMAT_RGBA32);
    bool ok=surface&&SDL_SaveBMP(surface,path)==0;
    if(surface)SDL_FreeSurface(surface);
    free(pixels);free(flipped);glBindFramebuffer(GL_FRAMEBUFFER,0);return ok;
}
void renderer_shutdown(Renderer *r) {
    if(r->context) {
        GLuint buffers[]={r->world_buffer,r->dynamic_buffer,r->gun_buffer,r->ui_buffer,r->quad_buffer};glDeleteBuffers(5,buffers);
        glDeleteFramebuffers(1,&r->framebuffer);glDeleteRenderbuffers(1,&r->depth);glDeleteTextures(1,&r->texture);
        if(r->program)glDeleteProgram(r->program);
        if(r->composite_program)glDeleteProgram(r->composite_program);
        SDL_GL_DeleteContext(r->context);
    }
    mesh_free(&r->dynamic);mesh_free(&r->gun);mesh_free(&r->ui);
    if(r->window)SDL_DestroyWindow(r->window);
    memset(r,0,sizeof(*r));
}
