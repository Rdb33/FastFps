#ifndef FASTFPS_UPDATES_H
#define FASTFPS_UPDATES_H

#include "fastfps.h"

typedef struct {
    SDL_Thread *thread;
    SDL_atomic_t cancel, ready, stage;
    bool succeeded, restart;
    char directory[4096], log_path[4096], error[160];
} UpdateInstaller;

typedef struct UpdateChecker {
    SDL_Thread *thread;
    SDL_atomic_t ready, cancel;
    char latest[24];
    bool available, dismissed, open, details, released, copied, copy_error;
    int selected;
    Uint32 ready_at;
    UpdateInstaller installer;
} UpdateChecker;

void updates_start(UpdateChecker *checker);
void updates_poll(App *app);
bool updates_event(App *app,Renderer *renderer,const SDL_Event *event);
void updates_build(Mesh *mesh,const App *app);
void updates_shutdown(UpdateChecker *checker);
bool updater_start(UpdateInstaller *installer, const char *directory);
bool updater_busy(const UpdateInstaller *installer);
void updater_poll(UpdateInstaller *installer);
void updater_shutdown(UpdateInstaller *installer);
int updater_relaunch(const UpdateInstaller *installer);

#endif
