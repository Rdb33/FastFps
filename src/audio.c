#include "fastfps.h"
#include <math.h>
#include <string.h>

#define AUDIO_RATE 22050
#define VOICES 8
typedef struct {float start,end,duration,volume,phase,left,right;int sample;bool noise,active;} Voice;
static SDL_AudioDeviceID device=0;
static bool enabled=true;
static bool suppressed=false;
static Voice voices[VOICES];
static uint32_t noise_seed=91723;
static Vec3 listener;
static float listener_yaw;

static void callback(void *userdata,Uint8 *stream,int length) {
    (void)userdata;float *output=(float *)stream;
    int samples=length/(2*(int)sizeof(float));
    for(int i=0;i<samples;i++) {
        float left=0,right=0;
        for(int j=0;j<VOICES;j++)if(voices[j].active) {
            Voice *v=&voices[j];float t=(float)v->sample/AUDIO_RATE;
            if(t>=v->duration){v->active=false;continue;}
            float fraction=t/v->duration;
            float frequency=v->start+(v->end-v->start)*fraction;
            v->phase+=2*PI_F*frequency/AUDIO_RATE;
            if(v->phase>2*PI_F)v->phase-=2*PI_F;
            float sample=sinf(v->phase);
            if(v->noise) {
                noise_seed=noise_seed*1664525u+1013904223u;
                sample=sample*.45f+((float)(noise_seed>>8)/16777216.0f*2-1)*.55f;
            }
            float amplitude=sample*v->volume*(1-fraction)*(1-fraction);
            left+=amplitude*v->left;right+=amplitude*v->right;v->sample++;
        }
        output[i*2]=clampf(left,-.8f,.8f);output[i*2+1]=clampf(right,-.8f,.8f);
    }
}
void audio_init(void) {
    if(SDL_InitSubSystem(SDL_INIT_AUDIO)!=0){SDL_Log("Audio indisponible : %s",SDL_GetError());return;}
    SDL_AudioSpec want;SDL_zero(want);
    want.freq=AUDIO_RATE;want.format=AUDIO_F32SYS;want.channels=2;want.samples=512;want.callback=callback;
    device=SDL_OpenAudioDevice(NULL,0,&want,NULL,0);
    if(device)SDL_PauseAudioDevice(device,0);else SDL_Log("Audio indisponible : %s",SDL_GetError());
}
void audio_enabled(bool value) {
    enabled=value;
    if(device){SDL_LockAudioDevice(device);memset(voices,0,sizeof(voices));SDL_UnlockAudioDevice(device);}
}
static void play(Sound sound,float volume,float pan) {
    if(!enabled||suppressed||!device)return;
    static const Voice sounds[]={
        {.start=180,.end=55,.duration=.08f,.volume=.12f,.noise=true},
        {.start=100,.end=30,.duration=.20f,.volume=.24f,.noise=true},
        {.start=580,.end=250,.duration=.06f,.volume=.07f},
        {.start=240,.end=50,.duration=.18f,.volume=.14f,.noise=true},
        {.start=130,.end=40,.duration=.18f,.volume=.12f,.noise=true},
        {.start=170,.end=370,.duration=.10f,.volume=.06f},
        {.start=430,.end=890,.duration=.20f,.volume=.10f},
        {.start=220,.end=440,.duration=.36f,.volume=.10f},
        {.start=75,.end=32,.duration=.065f,.volume=.05f,.noise=true},
        {.start=110,.end=34,.duration=.11f,.volume=.085f,.noise=true},
        {.start=240,.end=120,.duration=.10f,.volume=.05f,.noise=true},
        {.start=170,.end=65,.duration=.095f,.volume=.09f,.noise=true},
        {.start=120,.end=42,.duration=.12f,.volume=.14f,.noise=true},
        {.start=340,.end=75,.duration=.18f,.volume=.08f,.noise=true},
        {.start=170,.end=610,.duration=.32f,.volume=.12f},
        {.start=90,.end=40,.duration=.25f,.volume=.10f,.noise=true},
        {.start=85,.end=18,.duration=.45f,.volume=.32f,.noise=true},
        {.start=1050,.end=620,.duration=.09f,.volume=.09f},
    };
    if((unsigned)sound>=sizeof(sounds)/sizeof(sounds[0]))return;
    SDL_LockAudioDevice(device);
    int slot=0;
    for(int i=0;i<VOICES;i++)if(!voices[i].active){slot=i;break;}
    voices[slot]=sounds[sound];voices[slot].active=true;
    voices[slot].volume*=volume;
    voices[slot].left=1-fmaxf(0,pan)*.8f;
    voices[slot].right=1-fmaxf(0,-pan)*.8f;
    SDL_UnlockAudioDevice(device);
}
void audio_play(Sound sound){play(sound,1,0);}
void audio_suppressed(bool value){suppressed=value;}
void audio_listener(Vec3 position,float yaw){listener=position;listener_yaw=yaw;}
void audio_play_at(Sound sound,Vec3 position) {
    Vec3 delta=vsub(position,listener);float distance=vlength(delta);
    float pan=distance>.001f?(delta.x*cosf(listener_yaw)+delta.z*sinf(listener_yaw))/distance:0;
    play(sound,1/(1+distance*.12f),clampf(pan,-1,1));
}
void audio_shutdown(void){if(device)SDL_CloseAudioDevice(device);device=0;}
