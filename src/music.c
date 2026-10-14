#include "music.h"
#include <mpg123.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define MUSIC_RATE 22050
#define MUSIC_RING 16384
#define MUSIC_CHUNK 2048
#define MUSIC_DELAY 2048
typedef struct {
    mpg123_handle *decoder;
    float ring[MUSIC_RING][2];
    unsigned read, write, count;
    float gain, target;
    off_t position, length;
    bool failed;
} MusicTrack;
static struct {
    SDL_AudioDeviceID device;
    SDL_Thread *thread;
    MusicTrack tracks[2];
    bool initialized, stop;
    float volume, target_volume, volume_step, focus, target_focus, duck;
    float filter[2], delay[MUSIC_DELAY][2];
    unsigned delay_at;
} music;

static float bounded(float n,float lo,float hi){return fmaxf(lo,fminf(hi,n));}
static float towards(float n,float target,float step) {
    return n<target?fminf(n+step,target):fmaxf(n-step,target);
}
static bool open_track(MusicTrack *t,const char *name) {
    int error; t->decoder=mpg123_new(NULL,&error);
    if(!t->decoder)return false;
    if(mpg123_param(t->decoder,MPG123_FORCE_RATE,MUSIC_RATE,0)!=MPG123_OK
        ||mpg123_param(t->decoder,MPG123_ADD_FLAGS,MPG123_FORCE_STEREO,0)!=MPG123_OK
        ||mpg123_format_none(t->decoder)!=MPG123_OK
        ||mpg123_format(t->decoder,MUSIC_RATE,MPG123_STEREO,MPG123_ENC_SIGNED_16)!=MPG123_OK)return false;
    char path[4096]; char *base=SDL_GetBasePath();bool found=false;
    if(base) {
        const char *paths[]={"../assets/music/","assets/music/"};
        for(int i=0;i<2&&!found;i++) {
            snprintf(path,sizeof(path),"%s%s%s",base,paths[i],name);
            found=mpg123_open(t->decoder,path)==MPG123_OK;
        }
        SDL_free(base);
    }
    if(!found) {
        snprintf(path,sizeof(path),"assets/music/%s",name);
        found=mpg123_open(t->decoder,path)==MPG123_OK;
    }
    long rate=0;int channels=0,encoding=0;
    if(!found||mpg123_getformat(t->decoder,&rate,&channels,&encoding)!=MPG123_OK
        ||rate!=MUSIC_RATE||channels!=2||encoding!=MPG123_ENC_SIGNED_16) {
        SDL_Log("Musique indisponible (%s) : %s",name,mpg123_strerror(t->decoder));return false;
    }
    t->length=mpg123_length(t->decoder);return true;
}

/* Decoder handles belong to this producer. Device locking covers only the
 * ring copies and shared controls, never decoding, seeking or disk access. */
static int producer(void *unused) {
    (void)unused;short pcm[MUSIC_CHUNK*2];float decoded[MUSIC_CHUNK][2];
    for(;;) {
        SDL_LockAudioDevice(music.device);bool stop=music.stop;SDL_UnlockAudioDevice(music.device);
        if(stop)break;
        bool filled=false;
        for(int i=0;i<2;i++) {
            MusicTrack *t=&music.tracks[i];if(!t->decoder||t->failed)continue;
            SDL_LockAudioDevice(music.device);
            unsigned free_frames=MUSIC_RING-t->count;
            SDL_UnlockAudioDevice(music.device);
            if(free_frames<MUSIC_CHUNK)continue;
            size_t bytes=0;int result=mpg123_read(t->decoder,(unsigned char *)pcm,sizeof(pcm),&bytes);
            unsigned frames=(unsigned)(bytes/(2*sizeof(short)));
            for(unsigned f=0;f<frames;f++) {
                /* Short fades at loop boundaries avoid discontinuities. */
                float edge=bounded((float)(t->position+f)/(MUSIC_RATE*.06f),0,1);
                if(t->length>0)edge=fminf(edge,bounded((float)(t->length-t->position-f)/(MUSIC_RATE*.06f),0,1));
                decoded[f][0]=(float)pcm[f*2]/32768*edge;
                decoded[f][1]=(float)pcm[f*2+1]/32768*edge;
            }
            t->position+=frames;
            SDL_LockAudioDevice(music.device);
            for(unsigned f=0;f<frames;f++) {
                music.tracks[i].ring[t->write][0]=decoded[f][0];
                music.tracks[i].ring[t->write][1]=decoded[f][1];
                t->write=(t->write+1)%MUSIC_RING;
            }
            t->count+=frames;SDL_UnlockAudioDevice(music.device);
            if(result==MPG123_DONE) {
                if(mpg123_seek(t->decoder,0,SEEK_SET)<0)t->failed=true;
                else t->position=0;
            }else if(result!=MPG123_OK&&result!=MPG123_NEW_FORMAT) {
                SDL_Log("Lecture musique interrompue : %s",mpg123_strerror(t->decoder));t->failed=true;
            }
            filled|=frames>0;
        }
        SDL_Delay(filled?1:8);
    }
    return 0;
}

void music_init(SDL_AudioDeviceID device) {
    memset(&music,0,sizeof(music));music.device=device;music.focus=music.target_focus=1;
    music.volume_step=1.0f/(MUSIC_RATE*.4f);
    if(!device||mpg123_init()!=MPG123_OK)return;
    music.initialized=true;
    const char *names[]={"menu.mp3","gameplay.mp3"};
    for(int i=0;i<2;i++)if(!open_track(&music.tracks[i],names[i])) {
        if(music.tracks[i].decoder)mpg123_delete(music.tracks[i].decoder);
        music.tracks[i].decoder=NULL;
    }
    music.tracks[0].target=1;
    music.thread=SDL_CreateThread(producer,"fastfps-music",NULL);
    if(!music.thread)SDL_Log("Lecture musique indisponible : %s",SDL_GetError());
}
void music_set(float volume,bool gameplay,float focus) {
    if(!music.initialized)return;
    SDL_LockAudioDevice(music.device);
    music.target_volume=isfinite(volume)?bounded(volume,0,1):0;
    music.target_focus=isfinite(focus)?bounded(focus,0,1):1;
    music.tracks[0].target=gameplay?0:1;music.tracks[1].target=gameplay?1:0;
    SDL_UnlockAudioDevice(music.device);
}
void music_render(float *stereo,int frames,float duck) {
    memset(stereo,0,(size_t)frames*2*sizeof(float));
    if(!music.initialized||!music.thread)return;
    for(int f=0;f<frames;f++) {
        music.volume=towards(music.volume,music.target_volume,music.volume_step);
        music.focus+=(music.target_focus-music.focus)*(1.0f/(MUSIC_RATE*.3f));
        float target_duck=bounded(duck,0,.32f);
        music.duck+=(target_duck-music.duck)*(1.0f/(MUSIC_RATE*(target_duck>music.duck?.025f:.22f)));
        float input[2]={0,0};
        for(int i=0;i<2;i++) {
            MusicTrack *t=&music.tracks[i];
            t->gain=towards(t->gain,t->target,1.0f/(MUSIC_RATE*1.3f));
            /* Muted or inactive tracks keep their place instead of decoding
             * silently forever. The producer fills a fixed amount ahead. */
            if(music.volume<=0||t->gain<=0||!t->count)continue;
            input[0]+=t->ring[t->read][0]*t->gain;input[1]+=t->ring[t->read][1]*t->gain;
            t->read=(t->read+1)%MUSIC_RING;t->count--;
        }
        /* Pause/downed state rolls off the highs and lowers the music, with
         * a small stereo echo that carries through the crossfade. */
        float filter=.09f+.79f*music.focus,wet=.06f+.06f*(1-music.focus);
        float level=.5f*music.volume*(.4f+.6f*music.focus)*(1-music.duck);
        for(int c=0;c<2;c++) {
            music.filter[c]+=(input[c]-music.filter[c])*filter;
            float tail=music.delay[music.delay_at][c];
            music.delay[music.delay_at][c]=music.filter[c]+tail*.18f;
            stereo[f*2+c]=(music.filter[c]+tail*wet)*level;
        }
        music.delay_at=(music.delay_at+1)%MUSIC_DELAY;
    }
}
void music_fadeout(void) {
    if(!music.initialized)return;
    SDL_LockAudioDevice(music.device);
    bool audible=music.volume>0;music.target_volume=0;music.volume_step=1.0f/(MUSIC_RATE*.18f);
    SDL_UnlockAudioDevice(music.device);
    if(audible)SDL_Delay(200);
}
void music_shutdown(void) {
    if(!music.initialized)return;
    SDL_LockAudioDevice(music.device);music.stop=true;SDL_UnlockAudioDevice(music.device);
    if(music.thread)SDL_WaitThread(music.thread,NULL);
    for(int i=0;i<2;i++)if(music.tracks[i].decoder)mpg123_delete(music.tracks[i].decoder);
    mpg123_exit();memset(&music,0,sizeof(music));
}
