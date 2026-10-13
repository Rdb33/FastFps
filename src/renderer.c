#include "fastfps.h"
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const Preset PRESETS[PRESET_COUNT]={
    {640,360,30,64,"PI 4"},{960,540,60,100,"EQUILIBRE"},
    {1280,720,60,160,"PC"},{854,480,30,80,"PI+"}
};
static const char *vertex_source=
    "attribute vec3 aPosition;attribute vec3 aColor;attribute vec3 aNormal;attribute float aGlow;"
    "uniform mat4 uMatrix;uniform float uFog;uniform mediump float uUnlit;"
    "uniform mediump vec3 uLightPosition;uniform mediump vec3 uLightColor;uniform mediump vec3 uLightPosition2;uniform mediump vec3 uLightColor2;uniform mediump vec3 uSectorColor;"
    "varying mediump vec3 vColor;varying mediump vec3 vBase;varying mediump vec3 vPosition;varying mediump vec3 vNormal;varying mediump float vGlow;varying mediump float vFog;"
    "void main(){gl_Position=uMatrix*vec4(aPosition,1.0);"
    "vBase=aColor;vPosition=aPosition;vNormal=aNormal;vGlow=aGlow;"
    "if(uUnlit>0.5){vColor=aColor;vFog=0.0;return;}"
    "float light=0.38+0.14*max(aNormal.y,0.0)+0.48*max(dot(aNormal,normalize(vec3(-0.4,0.9,0.25))),0.0);"
    "float flash=max(0.0,1.0-length(aPosition-uLightPosition)/7.0);"
    "float flash2=max(0.0,1.0-length(aPosition-uLightPosition2)/8.0);"
    "float core=max(0.0,1.0-length(aPosition-vec3(0.0,4.15,0.0))/11.0);"
    "vec3 lit=aColor*light*(vec3(0.85)+uSectorColor*0.15);"
    "lit+=flash*flash*uLightColor*0.6+flash2*flash2*uLightColor2*0.5+core*core*uSectorColor*0.085;"
    "lit=mix(lit,aColor,aGlow)+aColor*aGlow*0.25;"
    "vColor=lit;"
    "vFog=clamp((gl_Position.w-13.0)/65.0,0.0,0.7)*uFog;}";
static const char *fragment_source=
    "precision mediump float;"
    "uniform mediump float uUnlit;uniform float uDetail;uniform float uSurface;uniform vec3 uEye;"
    "uniform mediump vec3 uLightPosition;uniform mediump vec3 uLightColor;uniform mediump vec3 uLightPosition2;uniform mediump vec3 uLightColor2;uniform mediump vec3 uSectorColor;"
    "varying mediump vec3 vColor;varying mediump vec3 vBase;varying mediump vec3 vPosition;varying mediump vec3 vNormal;varying mediump float vGlow;varying mediump float vFog;"
    "float noise(vec2 p){p=mod(p,vec2(32.0));return fract(sin(dot(p,vec2(17.13,43.71)))*157.37);}"
    "void main(){vec3 color=vColor;"
    "if(uDetail>0.5&&uUnlit<0.5){"
    "vec3 n=normalize(vNormal);vec3 view=normalize(uEye-vPosition+vec3(0.0001));"
    "vec3 key=normalize(vec3(-0.4,0.9,0.25));"
    "float diffuse=max(dot(n,key),0.0);"
    "float spec=pow(max(dot(n,normalize(key+view)),0.0),24.0);"
    "vec3 delta=uLightPosition-vPosition;vec3 delta2=uLightPosition2-vPosition;"
    "float attenuation=max(0.0,1.0-length(delta)/7.0);float attenuation2=max(0.0,1.0-length(delta2)/8.0);"
    "float core=max(0.0,1.0-length(vPosition-vec3(0.0,4.15,0.0))/11.0);"
    "color=vBase*(0.35+0.14*max(n.y,0.0)+diffuse*0.50)*(vec3(0.85)+uSectorColor*0.15);"
    "color+=uLightColor*attenuation*attenuation*(0.22+max(dot(n,normalize(delta+vec3(0.0001))),0.0))*0.65;"
    "color+=uLightColor2*attenuation2*attenuation2*(0.22+max(dot(n,normalize(delta2+vec3(0.0001))),0.0))*0.55;"
    "color+=core*core*uSectorColor*0.085+vec3(spec*0.13);"
    "float rim=pow(1.0-max(dot(n,view),0.0),3.0);color+=uSectorColor*rim*0.035;"
    "color=mix(color,vBase,vGlow)+vBase*vGlow*0.25;}"
    "if(uSurface>0.5&&uUnlit<0.5&&vGlow<0.1){"
    "vec2 uv=abs(vNormal.y)>0.65?vPosition.xz:(abs(vNormal.x)>0.65?vPosition.zy:vPosition.xy);"
    "vec2 tile=fract(uv*0.5);vec2 edge=min(tile,vec2(1.0)-tile);"
    "float seam=1.0-smoothstep(0.006,0.022,min(edge.x,edge.y));"
    "color*=1.0-seam*0.35;"
    "if(uDetail>0.5){color*=0.94+noise(floor(uv*8.0))*0.10;"
    "float panel=noise(floor(uv*0.5));color*=0.92+panel*0.14;}}"
    "vec3 fogColor=vec3(0.034,0.044,0.057)+uSectorColor*0.012;"
    "gl_FragColor=vec4(mix(color,fogColor,vFog),1.0);}";
static const char *composite_vertex=
    "attribute vec2 aPosition;attribute vec2 aUV;varying mediump vec2 vUV;"
    "void main(){vUV=aUV;gl_Position=vec4(aPosition,0.0,1.0);}";
static const char *composite_fragment=
    "precision mediump float;uniform sampler2D uTexture;uniform vec2 uTexel;uniform vec2 uEffects;uniform float uBrightness;varying mediump vec2 vUV;"
    "void main(){vec3 color=texture2D(uTexture,vUV).rgb;"
    "if(uEffects.y>0.0){vec2 d=uTexel*2.5;vec3 glow=vec3(0.0);"
    "glow+=max(texture2D(uTexture,vUV+vec2(d.x,0.0)).rgb-vec3(0.68),vec3(0.0));"
    "glow+=max(texture2D(uTexture,vUV-vec2(d.x,0.0)).rgb-vec3(0.68),vec3(0.0));"
    "glow+=max(texture2D(uTexture,vUV+vec2(0.0,d.y)).rgb-vec3(0.68),vec3(0.0));"
    "glow+=max(texture2D(uTexture,vUV-vec2(0.0,d.y)).rgb-vec3(0.68),vec3(0.0));"
    "color+=glow*uEffects.y;}"
    "if(uEffects.x>0.5){color=pow(max(color*1.08,vec3(0.0)),vec3(0.80));color=(color-vec3(0.5))*1.05+vec3(0.5);}"
    "vec2 edge=vUV-vec2(0.5);color*=1.0-dot(edge,edge)*0.22;gl_FragColor=vec4(pow(max(color,vec3(0.0)),vec3(1.0/uBrightness)),1.0);}";

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
    r->detail_level=preset==&PRESETS[0]?0:(preset==&PRESETS[2]?2:1);
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
    r->glow=glGetAttribLocation(r->program,"aGlow");
    r->normal=glGetAttribLocation(r->program,"aNormal");r->matrix=glGetUniformLocation(r->program,"uMatrix");
    r->fog=glGetUniformLocation(r->program,"uFog");r->unlit=glGetUniformLocation(r->program,"uUnlit");
    r->light_position=glGetUniformLocation(r->program,"uLightPosition");r->light_color=glGetUniformLocation(r->program,"uLightColor");
    r->sector_color=glGetUniformLocation(r->program,"uSectorColor");
    r->light_position2=glGetUniformLocation(r->program,"uLightPosition2");r->light_color2=glGetUniformLocation(r->program,"uLightColor2");
    r->eye=glGetUniformLocation(r->program,"uEye");r->detail=glGetUniformLocation(r->program,"uDetail");r->surface=glGetUniformLocation(r->program,"uSurface");
    r->composite_position=glGetAttribLocation(r->composite_program,"aPosition");
    r->composite_uv=glGetAttribLocation(r->composite_program,"aUV");
    r->composite_texture=glGetUniformLocation(r->composite_program,"uTexture");
    r->composite_brightness=glGetUniformLocation(r->composite_program,"uBrightness");
    r->composite_texel=glGetUniformLocation(r->composite_program,"uTexel");r->composite_effects=glGetUniformLocation(r->composite_program,"uEffects");
    if(!mesh_init(&r->dynamic,90000)||!mesh_init(&r->gun,3200)||!mesh_init(&r->ui,131072))return false;
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
    glEnableVertexAttribArray((GLuint)r->glow);
    glVertexAttribPointer((GLuint)r->position,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void *)offsetof(Vertex,position));
    glVertexAttribPointer((GLuint)r->color,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void *)offsetof(Vertex,color));
    glVertexAttribPointer((GLuint)r->normal,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void *)offsetof(Vertex,normal));
    glVertexAttribPointer((GLuint)r->glow,1,GL_FLOAT,GL_FALSE,sizeof(Vertex),(void *)offsetof(Vertex,glow));
    glUniformMatrix4fv(r->matrix,1,GL_FALSE,matrix.m);glUniform1f(r->fog,fog);glUniform1f(r->unlit,unlit);
    glDrawArrays(GL_TRIANGLES,0,count);
}
static void upload(GLuint buffer,const Mesh *m) {
    glBindBuffer(GL_ARRAY_BUFFER,buffer);glBufferSubData(GL_ARRAY_BUFFER,0,(GLsizeiptr)m->count*(GLsizeiptr)sizeof(Vertex),m->data);
}
static Mat4 ui_matrix(void){return (Mat4){{2.0f/640,0,0,0,0,-2.0f/360,0,0,0,0,-1,0,-1,1,0,1}};}
static void composite(Renderer *r,GLuint target,int x,int y,int width,int height) {
    glBindFramebuffer(GL_FRAMEBUFFER,target);glViewport(x,y,width,height);
    glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glDisable(GL_BLEND);
    glUseProgram(r->composite_program);glBindBuffer(GL_ARRAY_BUFFER,r->quad_buffer);
    for(GLuint i=0;i<8;i++)glDisableVertexAttribArray(i);
    glEnableVertexAttribArray((GLuint)r->composite_position);glEnableVertexAttribArray((GLuint)r->composite_uv);
    glVertexAttribPointer((GLuint)r->composite_position,2,GL_FLOAT,GL_FALSE,4*sizeof(float),NULL);
    glVertexAttribPointer((GLuint)r->composite_uv,2,GL_FLOAT,GL_FALSE,4*sizeof(float),(void *)(2*sizeof(float)));
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,r->texture);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,r->modern?GL_LINEAR:GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,r->modern?GL_LINEAR:GL_NEAREST);
    glUniform1i(r->composite_texture,0);glUniform2f(r->composite_texel,1.0f/(float)r->width,1.0f/(float)r->height);
    glUniform1f(r->composite_brightness,r->brightness);
    glUniform2f(r->composite_effects,r->modern?1:0,r->modern&&r->detail_level>0?.16f:0);
    glDrawArrays(GL_TRIANGLES,0,6);
}
void renderer_draw(Renderer *r,const Game *g,const App *app) {
    r->modern=app->modern;r->brightness=app->brightness;
    if(r->world_revision!=g->world->revision) {
        glBindBuffer(GL_ARRAY_BUFFER,r->world_buffer);
        glBufferData(GL_ARRAY_BUFFER,(GLsizeiptr)g->world->mesh.count*(GLsizeiptr)sizeof(Vertex),g->world->mesh.data,GL_STATIC_DRAW);
        r->world_vertices=g->world->mesh.count;r->world_revision=g->world->revision;
    }
    const Player *p=&g->player;float speed=hypotf(p->body.velocity.x,p->body.velocity.z);
    float fov=(87+(app->motion?speed*.2f:0))*PI_F/180;
    Mat4 projection=perspective(fov,(float)r->width/(float)r->height,.05f,100);
    Mat4 view=view_matrix(player_eye(p,app->motion),p->yaw,p->pitch);
    Mat4 matrix=mat_multiply(projection,view);
    glBindFramebuffer(GL_FRAMEBUFFER,r->framebuffer);glViewport(0,0,r->width,r->height);
    glEnable(GL_DEPTH_TEST);glEnable(GL_CULL_FACE);glDisable(GL_BLEND);
    glClearColor(.045f,.04f,.032f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);glUseProgram(r->program);
    Color theme=sector_theme(g->wave);glUniform3f(r->sector_color,theme.r,theme.g,theme.b);
    Vec3 eye=player_eye(p,app->motion);glUniform3f(r->eye,eye.x,eye.y,eye.z);
    glUniform1f(r->detail,r->modern&&r->detail_level>0?1:0);glUniform1f(r->surface,1);
    Vec3 light=player_eye(p,app->motion);Color flash={0,0,0};float strength=0;
    if(g->shot_age<.10f) {
        strength=1-g->shot_age/.10f;light=vadd(light,vscale(look_direction(p->yaw,p->pitch),.65f));
        flash=(Color){strength,strength*.57f,strength*.23f};
    }
    for(int i=0;i<g->barrel_count;i++)if(g->barrels[i].explosion_time>.4f) {
        float blast=(g->barrels[i].explosion_time-.4f)*4;
        if(blast>strength){strength=blast;light=vadd(g->barrels[i].position,(Vec3){0,1,0});flash=(Color){blast,blast*.46f,blast*.12f};}
    }
    glUniform3f(r->light_position,light.x,light.y,light.z);glUniform3f(r->light_color,flash.r,flash.g,flash.b);
    Vec3 light2={0,4.15f,0};Color flash2={0,0,0};float energy=0;
    for(int i=0;i<MAX_PLAYERS;i++)if(i!=g->local_actor&&g->actors[i].active&&!g->actors[i].dead&&g->actors[i].shot_age<.1f) {
        float candidate=1-g->actors[i].shot_age/.1f;
        if(candidate>energy){energy=candidate;light2=player_eye(&g->actors[i].player,false);flash2=(Color){candidate,candidate*.6f,candidate*.26f};}
    }
    for(int i=0;i<g->enemy_count;i++)if(g->enemies[i].type==ENEMY_CASTER&&g->enemies[i].attack_timer>0&&!g->enemies[i].attack_fired) {
        const Enemy *e=&g->enemies[i];float charge=(1-e->attack_timer/e->attack_duration)*.8f;
        if(charge>energy){energy=charge;light2=vadd(e->body.position,(Vec3){0,1.3f,0});flash2=(Color){charge*.25f,charge,charge*.7f};}
    }
    glUniform3f(r->light_position2,light2.x,light2.y,light2.z);glUniform3f(r->light_color2,flash2.r,flash2.g,flash2.b);
    draw(r,r->world_buffer,r->world_vertices,matrix,1,0);
    Mesh *m=&r->dynamic;m->count=0;m->glow=0;glUniform1f(r->surface,0);
    animate_arena(m,g,app);
    for(int i=0;i<g->barrel_count;i++)animate_barrel(m,&g->barrels[i],g->time);
    for(int i=0;i<g->enemy_count;i++)animate_creature(m,&g->enemies[i],g->time);
    for(int i=0;i<g->corpse_count;i++)animate_creature(m,&g->corpses[i],g->time);
    if(g->cooperative)for(int i=0;i<MAX_PLAYERS;i++)if(g->actors[i].active&&i!=g->local_actor)
        animate_teammate(m,&g->actors[i],i);
    for(int i=0;i<g->projectile_count;i++) {
        const Projectile *bolt=&g->projectiles[i];
        m->glow=1;
        mesh_diamond(m,bolt->position,.17f,bolt->color);
        if(i<(r->detail_level?32:12)) {
            Vec3 end=vsub(bolt->position,vscale(vnormalize(bolt->velocity),.48f));
            mesh_cylinder(m,end,bolt->position,.035f,bolt->color,6);
            mesh_diamond(m,end,.07f,bolt->color);
        }
        m->glow=0;
    }
    for(int i=0;i<g->particle_count;i++) {
        const Particle *particle=&g->particles[i];
        if(particle->life<=0)continue;
        m->glow=.35f;
        mesh_diamond(m,particle->position,particle->size*particle->life/particle->max_life,particle->color);
        m->glow=0;
    }
    for(int i=0;i<g->pickup_count;i++) {
        if(g->pickups[i].cooldown>0) {
            Vec3 base=g->pickups[i].position;
            mesh_box(m,vadd(base,(Vec3){0,.045f,0}),(Vec3){.18f,.04f,.6f},(Color){.15f,.32f,.22f});
            mesh_box(m,vadd(base,(Vec3){0,.05f,0}),(Vec3){.6f,.04f,.18f},(Color){.15f,.32f,.22f});
            float progress=1-clampf(g->pickups[i].cooldown/12,0,1);
            mesh_box(m,vadd(base,(Vec3){0,.04f,.8f}),(Vec3){1.4f*progress+.01f,.03f,.08f},(Color){.25f,.7f,.4f});
            continue;
        }
        int first=m->count;
        m->glow=.65f;
        Vec3 base=g->pickups[i].position;base.y=.45f+sinf(g->time*2+base.x)*.1f;
        mesh_box(m,(Vec3){0,0,0},(Vec3){.23f,.65f,.23f},(Color){.3f,1,.4f});
        mesh_box(m,(Vec3){0,.2f,0},(Vec3){.65f,.23f,.23f},(Color){.3f,1,.4f});
        mesh_pose(m,first,base,g->time*.7f,0,1);
        m->glow=0;
    }
    upload(r->dynamic_buffer,m);draw(r,r->dynamic_buffer,m->count,matrix,1,0);
    m=&r->gun;animate_weapon(m,g,app);
    glUniform3f(r->light_color,0,0,0);glUniform3f(r->sector_color,.75f,.85f,1);
    glUniform3f(r->light_color2,0,0,0);glUniform3f(r->eye,0,0,0);
    glClear(GL_DEPTH_BUFFER_BIT);upload(r->gun_buffer,m);draw(r,r->gun_buffer,m->count,projection,0,0);
    ui_build(&r->ui,g,app);upload(r->ui_buffer,&r->ui);
    /* L'interface est dessinée après l'agrandissement, à la résolution de la fenêtre. */
    glBindFramebuffer(GL_FRAMEBUFFER,0);
    int width,height;SDL_GL_GetDrawableSize(r->window,&width,&height);
    glViewport(0,0,width,height);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);
    int target_width=width,target_height=width*9/16;
    if(target_height>height){target_height=height;target_width=height*16/9;}
    composite(r,0,(width-target_width)/2,(height-target_height)/2,target_width,target_height);
    glUseProgram(r->program);draw(r,r->ui_buffer,r->ui.count,ui_matrix(),0,1);
    SDL_GL_SwapWindow(r->window);
}
bool renderer_screenshot(Renderer *r,const char *path) {
    size_t row=(size_t)r->width*4,size=row*(size_t)r->height;
    uint8_t *pixels=malloc(size),*flipped=malloc(size);
    if(!pixels||!flipped){free(pixels);free(flipped);return false;}
    GLuint capture=0,texture=0;glGenFramebuffers(1,&capture);glGenTextures(1,&texture);
    glBindTexture(GL_TEXTURE_2D,texture);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,r->width,r->height,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glBindFramebuffer(GL_FRAMEBUFFER,capture);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture,0);
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE) {
        glDeleteFramebuffers(1,&capture);glDeleteTextures(1,&texture);glBindFramebuffer(GL_FRAMEBUFFER,0);free(pixels);free(flipped);return false;
    }
    composite(r,capture,0,0,r->width,r->height);glUseProgram(r->program);draw(r,r->ui_buffer,r->ui.count,ui_matrix(),0,1);
    glPixelStorei(GL_PACK_ALIGNMENT,1);
    glReadPixels(0,0,r->width,r->height,GL_RGBA,GL_UNSIGNED_BYTE,pixels);
    for(int y=0;y<r->height;y++)memcpy(flipped+(size_t)y*row,pixels+(size_t)(r->height-y-1)*row,row);
    SDL_Surface *surface=SDL_CreateRGBSurfaceWithFormatFrom(flipped,r->width,r->height,32,(int)row,SDL_PIXELFORMAT_RGBA32);
    bool ok=surface&&SDL_SaveBMP(surface,path)==0;
    if(surface)SDL_FreeSurface(surface);
    free(pixels);free(flipped);glDeleteFramebuffers(1,&capture);glDeleteTextures(1,&texture);glBindFramebuffer(GL_FRAMEBUFFER,0);return ok;
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
