#include "network.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void audio_play(Sound s){(void)s;}
void audio_play_at(Sound s,Vec3 p){(void)s;(void)p;}
void audio_listener(Vec3 p,float yaw){(void)p;(void)yaw;}
void audio_suppressed(bool s){(void)s;}

static size_t frame(uint8_t *out,uint8_t type,const uint8_t *body,size_t size) {
    uint32_t len=(uint32_t)size+1;
    out[0]=(uint8_t)(len>>24);out[1]=(uint8_t)(len>>16);out[2]=(uint8_t)(len>>8);out[3]=(uint8_t)len;
    out[4]=type;if(size)memcpy(out+5,body,size);return size+5;
}
static void transfer(Network *from,int sender,Network *to,int receiver,Game *g,double now) {
    uint8_t bytes[31];size_t count;
    while((count=net_take_output(from,sender,bytes,sizeof(bytes)))>0)
        assert(net_feed(to,receiver,g,bytes,count,now));
}
static void protocol_test(World *world) {
    Command input={.sequence=0x12345678,.round=3,.yaw=.6f,.pitch=-.2f,.weapon=1,
        .input={.forward=1,.right=-1,.jump=true,.firing=true,.dash=true}};
    uint8_t buffer[NET_PACKET_MAX];Command decoded;
    size_t size=protocol_command_write(buffer,sizeof(buffer),&input);
    assert(size>0&&buffer[0]==0x12&&buffer[3]==0x78);
    assert(protocol_command_read(&decoded,buffer,size));
    assert(decoded.sequence==input.sequence&&decoded.round==3&&decoded.weapon==1&&decoded.input.dash);
    for(size_t i=0;i<size;i++)assert(!protocol_command_read(&decoded,buffer,i));
    buffer[8]=0x7f;buffer[9]=0x80;buffer[10]=0;buffer[11]=0;
    assert(!protocol_command_read(&decoded,buffer,size));
    Game game;game_reset(&game,world,28);bool active[MAX_PLAYERS]={true,true,true,true};
    game_start_coop(&game,active,0);game.wave=12;game.map_id=2;world_set_map(world,2);game.score=5678;game_actor_save(&game);
    size=protocol_snapshot_write(buffer,sizeof(buffer),&game);assert(size>0&&size<NET_PACKET_MAX);
    Game copy;game_reset(&copy,world,28);copy.local_actor=2;
    assert(protocol_snapshot_read(&copy,buffer,size));
    assert(copy.score==5678&&copy.wave==12&&copy.current_actor==2&&copy.local_actor==2);
    assert(copy.player.body.position.x==game.actors[2].player.body.position.x);
    assert(copy.world==world&&copy.actors[3].active);
    for(size_t i=0;i<size;i+=17){int score=copy.score;assert(!protocol_snapshot_read(&copy,buffer,i));assert(copy.score==score);}
    uint8_t fuzz[100];
    for(int i=0;i<2000;i++) {
        size_t len=(size_t)(rand()%100);for(size_t j=0;j<len;j++)fuzz[j]=(uint8_t)rand();
        protocol_snapshot_read(&copy,fuzz,len);protocol_command_read(&decoded,fuzz,len);
    }
}
static void gameplay_test(World *world) {
    Game g;bool active[MAX_PLAYERS]={true,true,false,false};game_reset(&g,world,28);game_start_coop(&g,active,0);
    g.wave_timer=100;Input inputs[MAX_PLAYERS]={0};inputs[1].right=1;
    for(int i=0;i<30;i++)game_update_coop(&g,inputs,FIXED_STEP);
    assert(g.player.body.position.x==0&&g.actors[1].player.body.position.x>3);
    g.actors[1].dead=true;g.actors[1].player.health=0;g.actors[1].respawn_timer=.03f;
    game_update_coop(&g,inputs,FIXED_STEP);game_update_coop(&g,inputs,FIXED_STEP);
    assert(!g.actors[1].dead&&g.actors[1].player.health==100&&g.actors[1].invulnerable>0);
    g.dead=true;g.player.health=0;g.actors[0].respawn_timer=.03f;game_actor_save(&g);
    game_update_coop(&g,inputs,FIXED_STEP);game_update_coop(&g,inputs,FIXED_STEP);
    assert(!g.dead&&g.player.health==100);
    g.pickups[0].cooldown=12;game_update_coop(&g,inputs,FIXED_STEP);
    assert(fabsf(g.pickups[0].cooldown-(12-FIXED_STEP))<.0001f);
    g.enemy_count=1;g.enemies[0]=(Enemy){.body={{15,0,15},{0,0,0},true},.health=80,.max_health=80};
    g.actors[1].player.body.position=(Vec3){15,0,18};
    game_update_coop(&g,inputs,FIXED_STEP);assert(g.enemies[0].target_actor==1||g.enemies[0].attack_timer==0);
    g.dead=true;g.player.health=0;game_actor_save(&g);
    g.actors[1].dead=true;g.actors[1].player.health=0;g.actors[1].respawn_timer=5;
    g.actors[0].respawn_timer=5;game_update_coop(&g,inputs,FIXED_STEP);assert(g.round_over);
}
static void virtual_sessions(World *world) {
    Network *host=calloc(1,sizeof(*host)),*clients[3];assert(host);net_init(host);
    host->mode=NET_HOST;host->members[0]=(Member){.present=true,.ready=true};strcpy(host->members[0].name,"HOTE");
    Game server,games[3];game_reset(&server,world,28);
    uint8_t bytes[NET_PACKET_MAX];
    for(int i=0;i<3;i++) {
        clients[i]=calloc(1,sizeof(Network));assert(clients[i]);net_init(clients[i]);
        clients[i]->mode=NET_CLIENT;clients[i]->connecting=true;clients[i]->peers[0].fd=-2;
        host->peers[i+1].fd=-2;game_reset(&games[i],world,28);
        uint8_t hello[2+PLAYER_NAME_LENGTH+1]={NET_PROTOCOL,1};snprintf((char *)hello+2,PLAYER_NAME_LENGTH+1,"AMI %d",i+1);
        size_t size=frame(bytes,PACKET_HELLO,hello,sizeof(hello));
        for(size_t j=0;j<size;j++)assert(net_feed(host,i+1,&server,bytes+j,1,0));
        for(int j=0;j<=i;j++)transfer(host,j+1,clients[j],0,&games[j],0);
        assert(clients[i]->local_id==i+1&&host->members[i+1].present);
    }
    assert(!net_start(host,&server,0));
    for(int i=0;i<3;i++){net_ready(clients[i]);transfer(clients[i],0,host,i+1,&server,0);}
    for(int i=0;i<3;i++)transfer(host,i+1,clients[i],0,&games[i],0);
    assert(net_start(host,&server,0));
    for(int i=0;i<3;i++) {
        transfer(host,i+1,clients[i],0,&games[i],0);
        assert(clients[i]->phase==ROOM_PLAYING&&games[i].cooperative&&games[i].local_actor==i+1);
    }
    for(int tick=1;tick<=240;tick++) {
        double now=(double)tick/60;
        for(int i=0;i<3;i++) {
            Command cmd={.yaw=.2f*(float)i,.pitch=0,.weapon=0,.input={.right=(i%2?-.3f:.3f)}};
            games[i].player.yaw=cmd.yaw;games[i].player.pitch=cmd.pitch;
            net_tick(clients[i],&games[i],cmd,FIXED_STEP,now);transfer(clients[i],0,host,i+1,&server,now);
        }
        net_tick(host,&server,(Command){0},FIXED_STEP,now);
        for(int i=0;i<3;i++)transfer(host,i+1,clients[i],0,&games[i],now);
    }
    for(int i=0;i<3;i++) {
        assert(games[i].actors[i+1].input_ack>200);
        assert(vlength(vsub(games[i].player.body.position,server.actors[i+1].player.body.position))<.05f);
    }
    /* Transition d'arène et rechargement propagés à trois invités. */
    server.wave=5;server.completed_waves=4;server.enemy_count=0;server.projectile_count=0;server.wave_timer=.001f;
    game_actor_save(&server);
    for(int i=0;i<MAX_PLAYERS;i++){server.actors[i].player.upgrade_pending=false;server.actors[i].player.ammo[0]=17;}
    server.current_actor=-1;game_actor_select(&server,0);
    for(int i=0;i<3;i++) {
        net_tick(clients[i],&games[i],(Command){.input={.reload=true}},FIXED_STEP,4.5);
        transfer(clients[i],0,host,i+1,&server,4.5);
    }
    net_tick(host,&server,(Command){.input={.reload=true}},FIXED_STEP,4.5);
    for(int i=0;i<3;i++) {
        transfer(host,i+1,clients[i],0,&games[i],4.5);
        assert(games[i].wave==6&&games[i].map_id==1&&games[i].world->map_id==1);
        assert(games[i].player.ammo[0]==17&&games[i].player.reload_timer>1);
    }
    server.round_over=true;net_tick(host,&server,(Command){0},FIXED_STEP,5);
    for(int i=0;i<3;i++){transfer(host,i+1,clients[i],0,&games[i],5);assert(clients[i]->phase==ROOM_FINISHED);}
    assert(net_start(host,&server,6));
    for(int i=0;i<3;i++){transfer(host,i+1,clients[i],0,&games[i],6);assert(clients[i]->round==2&&clients[i]->sequence==0);}
    Command stale={.sequence=900,.round=1};uint8_t body[64];size_t len=protocol_command_write(body,sizeof(body),&stale);
    size_t size=frame(bytes,PACKET_INPUT,body,len);assert(net_feed(host,1,&server,bytes,size,6));assert(host->peers[1].count==0);
    uint8_t invalid[4]={0,0,0,0};assert(!net_feed(host,1,&server,invalid,sizeof(invalid),6));
    for(int i=0;i<3;i++){net_close(clients[i]);free(clients[i]);}net_close(host);free(host);
}
static int tcp_session(World *world) {
    Network *host=calloc(1,sizeof(*host)),*client=calloc(1,sizeof(*client));assert(host&&client);
    net_init(host);net_init(client);strcpy(host->port_text,"37991");strcpy(client->port_text,"37991");
    strcpy(client->address,"127.0.0.1");
    Game server,guest;game_reset(&server,world,28);game_reset(&guest,world,28);
    if(!net_host(host)||!net_join(client)){fprintf(stderr,"TCP non teste : %s / %s\n",host->message,client->message);net_close(host);net_close(client);free(host);free(client);return 77;}
    bool ready=false,started=false;
    double initial=(double)SDL_GetPerformanceCounter()/(double)SDL_GetPerformanceFrequency();
    for(int i=0;i<6000;i++) {
        double now=(double)SDL_GetPerformanceCounter()/(double)SDL_GetPerformanceFrequency();
        net_poll(host,&server,now);net_poll(client,&guest,now);
        if(client->mode==NET_OFF){fprintf(stderr,"Client deconnecte : %s\n",client->message);assert(false);}
        if(net_connected(client)&&client->peers[0].accepted&&!ready){net_ready(client);ready=true;}
        if(host->members[1].ready&&!started){assert(net_start(host,&server,now));started=true;}
        if(host->phase==ROOM_PLAYING&&client->phase==ROOM_PLAYING) {
            Command command={.input={.right=.25f}};net_tick(client,&guest,command,FIXED_STEP,now);
            net_poll(host,&server,now);net_tick(host,&server,(Command){0},FIXED_STEP,now);
        }
        if(started&&guest.actors[1].input_ack>60){puts("OK : salon TCP reel sur loopback");break;}
        assert(now-initial<8);SDL_Delay(1);
    }
    assert(guest.actors[1].input_ack>60);net_close(host);net_close(client);free(host);free(client);return 0;
}
int main(int argc,char **argv) {
    assert(SDL_Init(SDL_INIT_TIMER)==0);srand(1);World world;assert(world_init(&world));
    int result=0;
    if(argc>1&&strcmp(argv[1],"--tcp")==0)result=tcp_session(&world);
    else {protocol_test(&world);gameplay_test(&world);virtual_sessions(&world);puts("OK : protocole, paquets fragmentes, salon a 4, prediction, manche suivante et cooperation");}
    mesh_free(&world.mesh);SDL_Quit();return result;
}
