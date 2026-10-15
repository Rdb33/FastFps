#ifndef FASTFPS_UPDATES_H
#define FASTFPS_UPDATES_H

#include "fastfps.h"

typedef struct UpdateChecker {
    SDL_Thread *thread;
    SDL_atomic_t ready, cancel;
    char latest[24];
    bool available, dismissed, open, details, released, copied, copy_error;
    int selected;
    Uint32 ready_at;
} UpdateChecker;

void updates_start(UpdateChecker *checker);
void updates_poll(App *app);
bool updates_event(App *app,Renderer *renderer,const SDL_Event *event);
void updates_build(Mesh *mesh,const App *app);
void updates_shutdown(UpdateChecker *checker);

#endif
