#include "updates.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc,char **argv) {
    assert(argc==3);assert(SDL_Init(SDL_INIT_TIMER)==0);
    UpdateInstaller u={0};bool success=strcmp(argv[2],"success")==0;
    if(strcmp(argv[2],"invalid")==0) {
        assert(!updater_start(&u,argv[1]));assert(!u.restart&&u.error[0]);
        SDL_Quit();return 0;
    }
    assert(updater_start(&u,strcmp(argv[1],"AUTO")==0?NULL:argv[1]));
    /* Une seconde activation ne doit pas démarrer un autre Git. */
    assert(!updater_start(&u,argv[1]));
    if(strcmp(argv[2],"close")==0) {
        Uint32 start=SDL_GetTicks();
        while(!SDL_AtomicGet(&u.ready)&&SDL_GetTicks()-start<10000)SDL_Delay(5);
        assert(SDL_AtomicGet(&u.ready));updater_shutdown(&u);
        assert(u.succeeded&&!u.restart);
        SDL_Quit();puts("OK : fermer le jeu au dernier instant ne provoque pas une relance");return 0;
    }
    if(strcmp(argv[2],"cancel")==0) {
        Uint32 start=SDL_GetTicks();
        while(SDL_AtomicGet(&u.stage)!=3&&!SDL_AtomicGet(&u.ready)&&SDL_GetTicks()-start<10000)SDL_Delay(5);
        assert(SDL_AtomicGet(&u.stage)==3);SDL_Delay(100);
        start=SDL_GetTicks();updater_shutdown(&u);
        assert(SDL_GetTicks()-start<2000&&!u.restart&&!updater_busy(&u));
        SDL_Quit();puts("OK : fermeture pendant compilation sans processus abandonne");return 0;
    }
    Uint32 start=SDL_GetTicks();
    while(updater_busy(&u)&&SDL_GetTicks()-start<15000){updater_poll(&u);SDL_Delay(5);}
    assert(!updater_busy(&u));assert(u.restart==success);
    if(!success){assert(strstr(u.error,argv[2]));puts(u.error);}
    if(success) {
        assert(!u.error[0]);pid_t child=fork();assert(child>=0);
        if(child==0)_exit(updater_relaunch(&u));
        int status;assert(waitpid(child,&status,0)==child);
        assert(WIFEXITED(status)&&WEXITSTATUS(status)==37);
        puts("OK : pull, compilation et relance via lancer.sh");
    }
    updater_shutdown(&u);SDL_Quit();return 0;
}
