#include "fastfps.h"
#include "network.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void settings_path(char *out,size_t size) {
    char *directory=SDL_GetPrefPath("fastfps","fastfps");
    if(directory){snprintf(out,size,"%ssettings.txt",directory);SDL_free(directory);}else out[0]='\0';
}
static void settings_read(App *app) {
    char path[2048];settings_path(path,sizeof(path));if(!path[0])return;
    FILE *file=fopen(path,"r");if(!file)return;
    int quality=0,sound=1,best=0,motion=1;float sensitivity=1;
    if(fscanf(file,"%d %f %d %d %d",&quality,&sensitivity,&sound,&best,&motion)>=4) {
        app->quality=quality>=0&&quality<3?quality:0;
        app->sensitivity=isfinite(sensitivity)?clampf(sensitivity,.3f,3):1;
        app->sound=sound!=0;app->best_score=best>=0?best:0;
        app->motion=motion!=0;
    }
    fclose(file);
}
static void settings_write(const App *app) {
    char path[2048];settings_path(path,sizeof(path));if(!path[0])return;
    FILE *file=fopen(path,"w");if(!file)return;
    fprintf(file,"%d %.2f %d %d %d\n",app->quality,app->sensitivity,app->sound,app->best_score,app->motion);fclose(file);
}
static void pause_game(App *app) {
    if(app->state!=STATE_PLAYING)return;
    app->state=STATE_PAUSED;app->selected=0;SDL_SetRelativeMouseMode(SDL_FALSE);
}
static void start_game(App *app,Game *game,bool restart,bool demo) {
    if(restart||app->state!=STATE_PAUSED)game_reset(game,game->world,PRESETS[app->quality].particles);
    game->motion=app->motion;
    if(!demo&&SDL_SetRelativeMouseMode(SDL_TRUE)!=0) {
        app->capture_error=true;SDL_Log("Capture souris : %s",SDL_GetError());return;
    }
    app->capture_error=false;app->state=STATE_PLAYING;
    int x,y;SDL_GetRelativeMouseState(&x,&y);
}
static void toggle_fullscreen(App *app,Renderer *renderer) {
    bool target=!app->fullscreen;
    if(SDL_SetWindowFullscreen(renderer->window,target?SDL_WINDOW_FULLSCREEN_DESKTOP:0)==0)app->fullscreen=target;
    else SDL_Log("Plein ecran : %s",SDL_GetError());
}
static void change_quality(App *app,Game *game,Renderer *renderer,int direction) {
    int previous=app->quality;
    app->quality=(app->quality+direction+3)%3;
    if(!renderer_resize(renderer,&PRESETS[app->quality])) {
        app->quality=previous;
        if(!renderer_resize(renderer,&PRESETS[previous]))app->running=false;
    }
    game->particle_limit=PRESETS[app->quality].particles;
    if(game->particle_count>game->particle_limit)game->particle_count=game->particle_limit;
}
static void menu_action(App *app,Game *game,Renderer *renderer,int direction) {
    switch(app->selected) {
        case 0:start_game(app,game,false,false);break;
        case 1:
            if(game->cooperative){net_close(app->network);game_reset(game,game->world,game->particle_limit);lobby_open(app);}
            else if(app->state==STATE_PAUSED)start_game(app,game,true,false);
            else lobby_open(app);
            break;
        case 2:change_quality(app,game,renderer,direction);break;
        case 3:app->sensitivity+=.1f*(float)direction;
            if(app->sensitivity>3.01f)app->sensitivity=.3f;
            if(app->sensitivity<.29f)app->sensitivity=3;
            break;
        case 4:app->sound=!app->sound;audio_enabled(app->sound);break;
        case 5:toggle_fullscreen(app,renderer);break;
        case 6:app->motion=!app->motion;game->motion=app->motion;break;
        case 7:app->running=false;break;
    }
}
static int menu_hit(Renderer *r,int x,int y) {
    int width,height;SDL_GetWindowSize(r->window,&width,&height);
    float draw_width=(float)width,draw_height=draw_width*9/16;
    if(draw_height>height){draw_height=(float)height;draw_width=draw_height*16/9;}
    float px=((float)x-((float)width-draw_width)/2)*640/draw_width;
    float py=((float)y-((float)height-draw_height)/2)*360/draw_height;
    if(px<76||px>564||py<136||py>=280)return -1;
    return (int)((py-136)/18);
}
static Input read_input(void) {
    const Uint8 *keys=SDL_GetKeyboardState(NULL);
    /* Les scancodes désignent les positions physiques : AZERTY et QWERTY. */
    bool forward=keys[SDL_SCANCODE_W]||keys[SDL_SCANCODE_UP];
    bool back=keys[SDL_SCANCODE_S]||keys[SDL_SCANCODE_DOWN];
    bool left=keys[SDL_SCANCODE_A]||keys[SDL_SCANCODE_LEFT];
    bool right=keys[SDL_SCANCODE_D]||keys[SDL_SCANCODE_RIGHT];
    return (Input){(float)forward-(float)back,(float)right-(float)left,
        keys[SDL_SCANCODE_SPACE]!=0,keys[SDL_SCANCODE_LSHIFT]||keys[SDL_SCANCODE_RSHIFT],
        (SDL_GetMouseState(NULL,NULL)&SDL_BUTTON(SDL_BUTTON_LEFT))!=0,
        keys[SDL_SCANCODE_LALT]||(SDL_GetMouseState(NULL,NULL)&SDL_BUTTON(SDL_BUTTON_RIGHT))!=0,
        keys[SDL_SCANCODE_F]!=0};
}
static Input demo_input(Game *game) {
    Player *p=&game->player;
    if(game->enemy_count) {
        Enemy *nearest=&game->enemies[0];float distance=1e9f;
        for(int i=0;i<game->enemy_count;i++) {
            float d=vlength(vsub(game->enemies[i].body.position,p->body.position));
            if(d<distance){distance=d;nearest=&game->enemies[i];}
        }
        Vec3 delta=vsub(nearest->body.position,p->body.position);delta.y+=.9f-PLAYER_EYE;
        p->yaw=atan2f(delta.x,-delta.z);p->pitch=atan2f(delta.y,hypotf(delta.x,delta.z));
    }
    if(((int)game->time/5)%2!=game->player.weapon)
        game_request_weapon(game,((int)game->time/5)%2);
    return (Input){.forward=0,.right=.35f,.firing=true};
}
static void help(void) {
    puts("FASTFPS " GAME_VERSION " - FPS 3D natif\n"
        "Usage : fastfps [options]\n"
        "  --quality pi|balanced|desktop  Profil de rendu\n"
        "  --host                        Creer un salon cooperatif\n"
        "  --join IP                     Rejoindre une adresse IPv4\n"
        "  --port N                      Port TCP du salon (27015)\n"
        "  --name PSEUDO                 Nom affiche (16 caracteres)\n"
        "  --fullscreen                  Plein ecran\n"
        "  --mute                        Sans son\n"
        "  --demo                        Demonstration automatique\n"
        "  --frames N                    Quitter apres N images rendues\n"
        "  --screenshot chemin.bmp       Enregistrer la derniere image\n"
        "  --help                        Afficher cette aide\n"
        "  --version                     Afficher la version\n");
}
int main(int argc,char **argv) {
    App app={.state=STATE_MENU,.running=true,.sound=true,.motion=true,.sensitivity=1};
    bool demo=false,cli_mute=false,cli_fullscreen=false;int cli_quality=-1,frame_limit=0;
    const char *screenshot=NULL,*join_address=NULL,*player_name="JOUEUR";
    bool host=false;int room_port=NET_DEFAULT_PORT;
    for(int i=1;i<argc;i++) {
        if(strcmp(argv[i],"--help")==0){help();return 0;}
        if(strcmp(argv[i],"--version")==0){puts("FASTFPS " GAME_VERSION);return 0;}
        if(strcmp(argv[i],"--host")==0)host=true;
        else if(strcmp(argv[i],"--join")==0&&i+1<argc)join_address=argv[++i];
        else if(strcmp(argv[i],"--name")==0&&i+1<argc)player_name=argv[++i];
        else if(strcmp(argv[i],"--port")==0&&i+1<argc) {
            char *end=NULL;long value=strtol(argv[++i],&end,10);
            if(*end||value<1||value>65535){fprintf(stderr,"Port invalide.\n");return 1;}
            room_port=(int)value;
        }
        else if(strcmp(argv[i],"--demo")==0)demo=true;
        else if(strcmp(argv[i],"--mute")==0)cli_mute=true;
        else if(strcmp(argv[i],"--fullscreen")==0)cli_fullscreen=true;
        else if(strcmp(argv[i],"--quality")==0&&i+1<argc) {
            const char *value=argv[++i];
            if(strcmp(value,"pi")==0)cli_quality=0;
            else if(strcmp(value,"balanced")==0)cli_quality=1;
            else if(strcmp(value,"desktop")==0)cli_quality=2;
            else {fprintf(stderr,"Profil inconnu : %s\n",value);return 1;}
        }else if(strcmp(argv[i],"--frames")==0&&i+1<argc) {
            char *end=NULL;long value=strtol(argv[++i],&end,10);
            if(*end||value<1||value>1000000){fprintf(stderr,"Nombre d'images invalide.\n");return 1;}
            frame_limit=(int)value;
        }else if(strcmp(argv[i],"--screenshot")==0&&i+1<argc)screenshot=argv[++i];
        else {fprintf(stderr,"Option inconnue ou incomplete : %s\n",argv[i]);help();return 1;}
    }
    if((host&&join_address)||(demo&&(host||join_address))) {fprintf(stderr,"Choisis un seul mode : host, join ou demo.\n");return 1;}
    if(strlen(player_name)>PLAYER_NAME_LENGTH||(join_address&&strlen(join_address)>=64)){fprintf(stderr,"Pseudo ou adresse trop longue.\n");return 1;}
    Network *network=calloc(1,sizeof(*network));
    if(!network){fprintf(stderr,"Memoire insuffisante.\n");return 1;}
    net_init(network);app.network=network;app.editing=-1;
    snprintf(network->name,sizeof(network->name),"%s",player_name);
    snprintf(network->port_text,sizeof(network->port_text),"%d",room_port);
    if(join_address)snprintf(network->address,sizeof(network->address),"%s",join_address);
    srand(demo?1u:(unsigned)time(NULL));
    SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR,"0");
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS|SDL_INIT_TIMER)!=0) {
        fprintf(stderr,"Initialisation SDL : %s\nLance le jeu depuis une session graphique.\n",SDL_GetError());free(network);return 1;
    }
    settings_read(&app);
    if(cli_quality>=0)app.quality=cli_quality;
    if(cli_mute)app.sound=false;
    World world;Renderer renderer;Game game;memset(&renderer,0,sizeof(renderer));
    if(!world_init(&world)) {fprintf(stderr,"Memoire insuffisante.\n");SDL_Quit();free(network);return 1;}
    if(!renderer_init(&renderer,&world,&PRESETS[app.quality])) {
        fprintf(stderr,"Impossible de creer le rendu OpenGL ES 2 : %s\n",SDL_GetError());
        renderer_shutdown(&renderer);mesh_free(&world.mesh);SDL_Quit();free(network);return 1;
    }
    if(!cli_mute){audio_init();audio_enabled(app.sound);}
    game_reset(&game,&world,PRESETS[app.quality].particles);
    if(cli_fullscreen)toggle_fullscreen(&app,&renderer);
    if(demo)start_game(&app,&game,true,true);
    if(host||join_address) {
        lobby_open(&app);
        if(host?net_host(network):net_join(network))app.state=STATE_LOBBY;
    }
    double frequency=(double)SDL_GetPerformanceFrequency();
    double previous=(double)SDL_GetPerformanceCounter()/frequency,last_render=previous-1,fps_start=previous;
    double accumulator=0;int rendered=0,fps_frames=0;bool show_fps=true;
    while(app.running) {
        SDL_Event event;
        while(SDL_PollEvent(&event)) {
            if(lobby_event(&app,&game,&renderer,&event))continue;
            if(event.type==SDL_QUIT)app.running=false;
            else if(event.type==SDL_WINDOWEVENT&&event.window.event==SDL_WINDOWEVENT_FOCUS_LOST&&!demo)pause_game(&app);
            else if(event.type==SDL_MOUSEMOTION) {
                if(app.state==STATE_PLAYING&&!demo) {
                    game.player.yaw=fmodf(game.player.yaw+(float)event.motion.xrel*.002f*app.sensitivity,2*PI_F);
                    game.player.pitch=clampf(game.player.pitch-(float)event.motion.yrel*.002f*app.sensitivity,-1.45f,1.45f);
                    game.player.sway_x=clampf(game.player.sway_x+(float)event.motion.xrel*.00012f,-.028f,.028f);
                    game.player.sway_y=clampf(game.player.sway_y+(float)event.motion.yrel*.00009f,-.02f,.02f);
                }else if(app.state!=STATE_PLAYING) {
                    int hit=menu_hit(&renderer,event.motion.x,event.motion.y);if(hit>=0)app.selected=hit;
                }
            }else if(event.type==SDL_MOUSEBUTTONDOWN&&event.button.button==SDL_BUTTON_LEFT&&app.state!=STATE_PLAYING) {
                int hit=menu_hit(&renderer,event.button.x,event.button.y);
                if(hit>=0){app.selected=hit;menu_action(&app,&game,&renderer,1);}
            }else if(event.type==SDL_MOUSEWHEEL&&app.state==STATE_PLAYING) {
                game_request_weapon(&game,1-game.player.weapon);
            }else if(event.type==SDL_KEYDOWN&&!event.key.repeat) {
                SDL_Scancode key=event.key.keysym.scancode;
                if(key==SDL_SCANCODE_F11)toggle_fullscreen(&app,&renderer);
                else if(key==SDL_SCANCODE_F2)change_quality(&app,&game,&renderer,1);
                else if(key==SDL_SCANCODE_F3){app.sound=!app.sound;audio_enabled(app.sound);}
                else if(key==SDL_SCANCODE_F1)show_fps=!show_fps;
                else if(key==SDL_SCANCODE_F4){app.motion=!app.motion;game.motion=app.motion;}
                else if(key==SDL_SCANCODE_ESCAPE) {
                    if(app.state==STATE_PLAYING)pause_game(&app);
                    else if(app.state==STATE_PAUSED)start_game(&app,&game,false,demo);
                    else app.running=false;
                }else if(app.state!=STATE_PLAYING) {
                    if(key==SDL_SCANCODE_UP)app.selected=(app.selected+7)%8;
                    else if(key==SDL_SCANCODE_DOWN)app.selected=(app.selected+1)%8;
                    else if(key==SDL_SCANCODE_RETURN||key==SDL_SCANCODE_KP_ENTER||key==SDL_SCANCODE_SPACE)menu_action(&app,&game,&renderer,1);
                    else if((key==SDL_SCANCODE_LEFT||key==SDL_SCANCODE_RIGHT)&&app.selected>=2&&app.selected<=6)
                        menu_action(&app,&game,&renderer,key==SDL_SCANCODE_LEFT?-1:1);
                }else if(key==SDL_SCANCODE_1||key==SDL_SCANCODE_KP_1)game_request_weapon(&game,0);
                else if(key==SDL_SCANCODE_2||key==SDL_SCANCODE_KP_2)game_request_weapon(&game,1);
            }
        }
        double now=(double)SDL_GetPerformanceCounter()/frequency;
        net_poll(network,&game,now);lobby_sync(&app,&game);
        double elapsed=fmin(now-previous,.1);previous=now;
        if(app.state==STATE_PLAYING||network->phase==ROOM_PLAYING) {
            accumulator+=elapsed;int steps=0;
            while(accumulator>=FIXED_STEP&&steps<MAX_CATCHUP_STEPS) {
                Input input=app.state==STATE_PLAYING?(demo?demo_input(&game):read_input()):(Input){0};
                if(game.dead)input=(Input){0};
                if(network->mode!=NET_OFF) {
                    Command command={.input=input,.yaw=game.player.yaw,.pitch=game.player.pitch,
                        .weapon=game.player.weapon_switch>0?game.player.weapon_target:game.player.weapon};
                    net_tick(network,&game,command,FIXED_STEP,now);lobby_sync(&app,&game);
                }else game_update(&game,input,FIXED_STEP);
                accumulator-=FIXED_STEP;steps++;
                if(game.dead&&!game.cooperative) {
                    app.state=STATE_DEAD;app.selected=0;SDL_SetRelativeMouseMode(SDL_FALSE);
                    if(game.score>app.best_score)app.best_score=game.score;
                    break;
                }
            }
            if(steps==MAX_CATCHUP_STEPS)accumulator=0;
        }else accumulator=0;
        double interval=1.0/(app.state==STATE_PLAYING?PRESETS[app.quality].fps:30);
        if(now-last_render>=interval-.0005) {
            last_render=now;
            int actual_fps=app.fps;if(!show_fps)app.fps=-1;
            Game view;net_render(network,&game,&view,now);
            renderer_draw(&renderer,&view,&app);app.fps=actual_fps;
            rendered++;fps_frames++;
            if(now-fps_start>=1){app.fps=(int)lround((double)fps_frames/(now-fps_start));fps_start=now;fps_frames=0;}
            if(frame_limit>0&&rendered>=frame_limit)app.running=false;
        }
        double until_draw=interval-((double)SDL_GetPerformanceCounter()/frequency-last_render);
        double until_update=(app.state==STATE_PLAYING||network->phase==ROOM_PLAYING)?FIXED_STEP-accumulator:.008;
        double wait=fmin(.008,fmin(until_draw,until_update));if(wait>.001)SDL_Delay((Uint32)(wait*1000));
    }
    int exit_code=0;
    if(screenshot) {
        if(renderer_screenshot(&renderer,screenshot))SDL_Log("Capture : %s",screenshot);
        else {SDL_Log("Capture impossible : %s",SDL_GetError());exit_code=1;}
    }
    if(!demo&&frame_limit==0)settings_write(&app);
    if(frame_limit)SDL_Log("%d images / vague %d / score %d / %d creatures / FPS %d",rendered,game.wave,game.score,game.enemy_count,app.fps);
    net_close(network);free(network);audio_shutdown();renderer_shutdown(&renderer);mesh_free(&world.mesh);SDL_Quit();return exit_code;
}
