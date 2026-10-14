#ifndef FASTFPS_MUSIC_H
#define FASTFPS_MUSIC_H
#include <SDL2/SDL.h>
#include <stdbool.h>

/* Called with the audio device paused for init/shutdown. Set locks the device;
 * render runs only from its callback, with no file I/O or allocation. */
void music_init(SDL_AudioDeviceID device);
void music_set(float volume, bool gameplay, float focus);
void music_render(float *stereo, int frames, float duck);
void music_fadeout(void);
void music_shutdown(void);
#endif
