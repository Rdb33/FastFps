#include "updates.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

/* Arguments distincts, sans shell. Le processus et ses enfants sont terminés
   ensemble à la fermeture du jeu ou à l'expiration du délai. */
static bool command(UpdateInstaller *u,char *const args[],FILE *log,char *output,size_t capacity,Uint32 timeout) {
    int fd[2];if(pipe(fd)!=0)return false;
    if(fcntl(fd[0],F_SETFD,FD_CLOEXEC)<0||fcntl(fd[1],F_SETFD,FD_CLOEXEC)<0
        ||fcntl(fd[0],F_SETFL,O_NONBLOCK)<0){close(fd[0]);close(fd[1]);return false;}
    size_t count=0;while(environ[count])count++;
    char **env=calloc(count+6,sizeof(*env));
    if(!env){close(fd[0]);close(fd[1]);return false;}
    static char *const overrides[]={"GIT_TERMINAL_PROMPT=0","GIT_ASKPASS=/bin/false",
        "SSH_ASKPASS=/bin/false","GCM_INTERACTIVE=never"};
    size_t used=0;
    for(size_t i=0;i<count;i++) {
        bool replaced=false;
        for(size_t j=0;j<4;j++) {
            size_t length=(size_t)(strchr(overrides[j],'=')-overrides[j])+1;
            if(strncmp(environ[i],overrides[j],length)==0)replaced=true;
        }
        if(!replaced)env[used++]=environ[i];
    }
    for(size_t i=0;i<4;i++)env[used++]=overrides[i];
    posix_spawn_file_actions_t actions;posix_spawnattr_t attr;
    int error=posix_spawn_file_actions_init(&actions);
    if(error){free(env);close(fd[0]);close(fd[1]);return false;}
    error=posix_spawnattr_init(&attr);
    if(error){posix_spawn_file_actions_destroy(&actions);free(env);close(fd[0]);close(fd[1]);return false;}
    error=posix_spawnattr_setflags(&attr,POSIX_SPAWN_SETPGROUP);
    if(!error)error=posix_spawnattr_setpgroup(&attr,0);
    if(!error)error=posix_spawn_file_actions_addopen(&actions,STDIN_FILENO,"/dev/null",O_RDONLY,0);
    if(!error)error=posix_spawn_file_actions_adddup2(&actions,fd[1],STDOUT_FILENO);
    if(!error)error=posix_spawn_file_actions_adddup2(&actions,fd[1],STDERR_FILENO);
    if(!error)error=posix_spawn_file_actions_addclose(&actions,fd[0]);
    if(!error)error=posix_spawn_file_actions_addclose(&actions,fd[1]);
    pid_t child=-1;
    if(!error)error=posix_spawnp(&child,args[0],&actions,&attr,args,env);
    posix_spawnattr_destroy(&attr);posix_spawn_file_actions_destroy(&actions);free(env);close(fd[1]);
    if(error){close(fd[0]);fprintf(log,"Impossible de lancer %s : %s\n",args[0],strerror(error));return false;}
    Uint32 start=SDL_GetTicks();int status=0;bool exited=false,eof=false,failed=false;
    size_t copied=0,logged=0;if(capacity)output[0]='\0';
    while(!SDL_AtomicGet(&u->cancel)&&SDL_GetTicks()-start<timeout) {
        char buffer[4096];ssize_t n=read(fd[0],buffer,sizeof(buffer));
        if(n>0) {
            size_t bytes=(size_t)n;
            if(logged<1024*1024){size_t written=bytes;if(written>1024*1024-logged)written=1024*1024-logged;
                fwrite(buffer,1,written,log);fflush(log);logged+=written;}
            if(capacity&&copied<capacity-1){size_t take=bytes;if(take>capacity-1-copied)take=capacity-1-copied;
                memcpy(output+copied,buffer,take);copied+=take;output[copied]='\0';}
        }else if(n==0)eof=true;
        else if(errno!=EINTR&&errno!=EAGAIN&&errno!=EWOULDBLOCK){failed=true;break;}
        if(!exited) {
            pid_t done=waitpid(child,&status,WNOHANG);
            if(done==child)exited=true;
            else if(done<0&&errno!=EINTR){failed=true;break;}
        }
        if(exited&&eof)break;
        struct pollfd p={.fd=fd[0],.events=POLLIN};
        if(eof)SDL_Delay(20);else if(n<=0)poll(&p,1,50);
    }
    bool complete=exited&&eof&&!failed&&!SDL_AtomicGet(&u->cancel);
    if(!complete) {
        kill(-child,SIGTERM);SDL_Delay(100);kill(-child,SIGKILL);
        if(!exited)while(waitpid(child,&status,0)<0&&errno==EINTR){}
        fprintf(log,"Commande interrompue ou delai depasse.\n");
    }
    close(fd[0]);return complete&&WIFEXITED(status)&&WEXITSTATUS(status)==0;
}
static int install_thread(void *data) {
    UpdateInstaller *u=data;char output[1024];
    int fd=open(u->log_path,O_WRONLY|O_CREAT|O_TRUNC,0600);
    if(fd>=0&&fcntl(fd,F_SETFD,FD_CLOEXEC)<0){close(fd);fd=-1;}
    FILE *log=fd<0?NULL:fdopen(fd,"w");
    if(!log){if(fd>=0)close(fd);snprintf(u->error,sizeof(u->error),"IMPOSSIBLE D'OUVRIR LE JOURNAL DE MISE A JOUR.");goto done;}
    char *branch[]={"git","-C",u->directory,"symbolic-ref","--quiet","HEAD",NULL};
    if(!command(u,branch,log,output,sizeof(output),10000)||strcmp(output,"refs/heads/main\n")!=0) {
        snprintf(u->error,sizeof(u->error),"UTILISE LA BRANCHE MAIN AVANT DE METTRE A JOUR.");goto close_log;
    }
    char *status[]={"git","-C",u->directory,"status","--porcelain","--untracked-files=no",NULL};
    if(!command(u,status,log,output,sizeof(output),10000)||output[0]) {
        snprintf(u->error,sizeof(u->error),"MODIFICATIONS LOCALES : MISE A JOUR NON LANCEE.");goto close_log;
    }
    SDL_AtomicSet(&u->stage,2);
    char *pull[]={"git","-C",u->directory,"pull","--ff-only","origin","main",NULL};
    if(!command(u,pull,log,NULL,0,90000)) {
        snprintf(u->error,sizeof(u->error),"ECHEC GIT : CONNEXION OU HISTORIQUE A VERIFIER.");goto close_log;
    }
    SDL_AtomicSet(&u->stage,3);
    char *build[]={"make","-s","-C",u->directory,NULL};
    if(!command(u,build,log,NULL,0,600000)) {
        snprintf(u->error,sizeof(u->error),"ECHEC DE COMPILATION : CONSULTE LE JOURNAL.");goto close_log;
    }
    char binary[4160];snprintf(binary,sizeof(binary),"%s/build/fastfps",u->directory);
    if(access(binary,X_OK)!=0){snprintf(u->error,sizeof(u->error),"EXECUTABLE ABSENT APRES LA COMPILATION.");goto close_log;}
    u->succeeded=true;
close_log:
    fclose(log);
done:
    SDL_AtomicSet(&u->ready,1);return 0;
}
bool updater_busy(const UpdateInstaller *u){return u->thread!=NULL;}
bool updater_start(UpdateInstaller *u,const char *directory) {
    if(updater_busy(u))return false;
    memset(u,0,sizeof(*u));char *base=NULL;
    if(!directory) {
        base=SDL_GetBasePath();if(!base)goto invalid;
        size_t n=strlen(base);
        if(n<7||strcmp(base+n-7,"/build/")!=0){SDL_free(base);goto invalid;}
        base[n-6]='\0';directory=base;
    }
    if(strlen(directory)>sizeof(u->directory)-64){SDL_free(base);goto invalid;}
    snprintf(u->directory,sizeof(u->directory),"%s",directory);SDL_free(base);
    char path[4160];struct stat info;
    snprintf(path,sizeof(path),"%s/.git",u->directory);
    if(stat(path,&info)!=0)goto invalid;
    snprintf(path,sizeof(path),"%s/lancer.sh",u->directory);if(access(path,R_OK)!=0)goto invalid;
    snprintf(u->log_path,sizeof(u->log_path),"%.*s/build/mise-a-jour.log",(int)sizeof(u->directory)-64,u->directory);
    SDL_AtomicSet(&u->stage,1);u->thread=SDL_CreateThread(install_thread,"mise-a-jour",u);
    if(!u->thread){snprintf(u->error,sizeof(u->error),"IMPOSSIBLE DE LANCER LA MISE A JOUR.");return false;}
    return true;
invalid:
    snprintf(u->error,sizeof(u->error),"INSTALLE DEPUIS UN CLONE GIT POUR CE BOUTON.");return false;
}
void updater_poll(UpdateInstaller *u) {
    if(u->thread&&SDL_AtomicGet(&u->ready)) {
        SDL_WaitThread(u->thread,NULL);u->thread=NULL;
        u->restart=u->succeeded&&!SDL_AtomicGet(&u->cancel);
    }
}
void updater_shutdown(UpdateInstaller *u) {
    SDL_AtomicSet(&u->cancel,1);
    if(u->thread){SDL_WaitThread(u->thread,NULL);u->thread=NULL;}
}
int updater_relaunch(const UpdateInstaller *u) {
    char path[4160];snprintf(path,sizeof(path),"%s/lancer.sh",u->directory);
    char *args[]={"bash",path,NULL};execv("/bin/bash",args);
    fprintf(stderr,"Relance impossible : %s. Lance ./lancer.sh manuellement.\n",strerror(errno));return 1;
}
