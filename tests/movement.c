#include "network.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int jumps;
void audio_play(Sound s){if(s==SOUND_JUMP)jumps++;}
void audio_play_at(Sound s,Vec3 p){(void)s;(void)p;}
void audio_listener(Vec3 p,float yaw){(void)p;(void)yaw;}
void audio_suppressed(bool s){(void)s;}
static Player falling(float y,float speed) {
    return (Player){.health=100,.body={{0,y,0},{0,speed,0},false}};
}
static Player leave_edge(World *w) {
    w->solid_count=1;w->solids[0]=(Box){{-3,0,-3},{0,2,3}};
    Player p={.health=100,.body={{.2f,2,0},{6,0,0},true}};
    for(int i=0;i<20&&p.body.grounded;i++)update_player(w,&p,(Input){.right=1},FIXED_STEP);
    assert(!p.body.grounded&&p.jump_grace>0);return p;
}
static void ledge_and_buffer(World *w) {
    Player p=leave_edge(w);
    for(int i=0;i<2;i++)update_player(w,&p,(Input){.right=1},FIXED_STEP);
    jumps=0;update_player(w,&p,(Input){.right=1,.jump=true},FIXED_STEP);
    assert(jumps==1&&p.body.velocity.y>7&&p.jump_grace==0);
    float vy=p.body.velocity.y;update_player(w,&p,(Input){.jump=true},FIXED_STEP);
    assert(jumps==1&&p.body.velocity.y<vy);
    p=leave_edge(w);
    for(int i=0;i<8;i++)update_player(w,&p,(Input){.right=1},FIXED_STEP);
    jumps=0;update_player(w,&p,(Input){.jump=true},FIXED_STEP);assert(jumps==0&&p.body.velocity.y<0);
    w->solid_count=0;p=falling(.1f,-4);jumps=0;
    update_player(w,&p,(Input){.jump=true},FIXED_STEP);assert(!p.body.grounded&&p.jump_buffer>0);
    for(int i=0;i<4&&jumps==0;i++)update_player(w,&p,(Input){0},FIXED_STEP);
    assert(jumps==1&&p.body.velocity.y>7);
    p=falling(3,0);jumps=0;update_player(w,&p,(Input){.jump=true},FIXED_STEP);
    for(int i=0;i<80;i++)update_player(w,&p,(Input){0},FIXED_STEP);
    assert(jumps==0&&p.body.grounded&&p.jump_buffer==0);
    p=(Player){.health=100,.body={{0,0,0},{0,0,0},true}};jumps=0;
    for(int i=0;i<80;i++)update_player(w,&p,(Input){.jump=true},FIXED_STEP);
    assert(jumps>=2); /* Le saut maintenu existant reste disponible. */
    puts("OK : tolerance de rebord, saut anticipe, expiration, aucun double saut et saut maintenu");
}
static void movement_interactions(World *w) {
    Player p=falling(2,12);p.jump_grace=.1f;jumps=0;
    update_player(w,&p,(Input){.jump=true},FIXED_STEP);
    assert(jumps==0&&p.body.velocity.y>11); /* Impulsion de roquette / tremplin conservée. */
    p=(Player){.health=100,.web_timer=.03f,.jump_buffer=.1f,.jump_grace=.1f,.body={{0,0,0},{0,0,0},true}};
    update_player(w,&p,(Input){.jump=true},FIXED_STEP);
    assert(jumps==0&&p.body.grounded&&p.jump_buffer==0&&p.jump_grace==0);
    update_player(w,&p,(Input){0},FIXED_STEP);update_player(w,&p,(Input){0},FIXED_STEP);
    assert(jumps==0&&p.body.grounded);
    w->platform_count=w->solid_count=1;
    w->platforms[0]=(Platform){.base={0,0,0},.size={4,.28f,4},.travel={8,3,0},.period=8,.solid=0};
    world_platforms_update(w,1);Box box=w->solids[0];Vec3 ride=platform_velocity(&w->platforms[0],1);
    p=(Player){.health=100,.body={{(box.min.x+box.max.x)*.5f,box.max.y,0},{0,0,0},true}};
    jumps=0;update_player(w,&p,(Input){.jump=true},FIXED_STEP);
    assert(jumps==1&&p.boost_timer>.5f&&p.body.velocity.x>0);
    assert(fabsf(p.body.velocity.y-(PLAYER_JUMP+ride.y-PLAYER_GRAVITY*FIXED_STEP))<.001f);
    w->platform_count=w->solid_count=0;
    p=(Player){.health=100,.body={{0,0,0},{0,0,0},true}};
    update_player(w,&p,(Input){.dash=true,.jump=true},FIXED_STEP);
    assert(p.dash_timer>0&&p.body.velocity.y>7&&hypotf(p.body.velocity.x,p.body.velocity.z)>20);
    puts("OK : toile, impulsion ascendante, esquive et vitesse heritee des plateformes");
}
static void network_timers(World *w) {
    Game host,client;game_reset(&host,w,20);host.wave=1;host.wave_timer=100;host.intro_time=0;
    host.enemy_count=0;host.barrel_count=0;host.pickup_count=0;
    host.player.body=(Body){{0,.1f,0},{0,-4,0},false};
    host.player.jump_buffer=.08f;host.player.jump_grace=0;game_actor_save(&host);
    uint8_t packet[NET_PACKET_MAX];size_t n=protocol_snapshot_write(packet,sizeof(packet),&host);assert(n>0);
    client=host;client.player.jump_buffer=0;client.actors[0].player.jump_buffer=0;
    assert(protocol_snapshot_read(&client,packet,n));assert(client.player.jump_buffer==host.player.jump_buffer);
    for(int i=0;i<6;i++) {
        game_update_coop(&host,(Input[MAX_PLAYERS]){{0}},FIXED_STEP);
        game_predict(&client,(Input){0},FIXED_STEP);
        assert(fabsf(host.player.body.position.y-client.player.body.position.y)<.00001f);
        assert(host.player.jump_buffer==client.player.jump_buffer&&host.player.jump_grace==client.player.jump_grace);
    }
    assert(host.player.body.velocity.y>0);
    host.player.jump_buffer=.06f;host.player.jump_grace=.04f;game_actor_save(&host);
    n=protocol_snapshot_write(packet,sizeof(packet),&host);assert(protocol_snapshot_read(&client,packet,n));
    assert(client.player.jump_buffer==.06f&&client.player.jump_grace==.04f);
    const float invalid[]={-.01f,.2f,NAN,INFINITY};
    for(int field=0;field<2;field++)for(int i=0;i<4;i++) {
        host.player.jump_buffer=.06f;host.player.jump_grace=.04f;
        if(field)host.player.jump_grace=invalid[i];else host.player.jump_buffer=invalid[i];
        game_actor_save(&host);n=protocol_snapshot_write(packet,sizeof(packet),&host);
        assert(n>0&&!protocol_snapshot_read(&client,packet,n));
    }
    puts("OK : timers de saut repliques, prediction equivalente et rejet des timers invalides");
}
int main(void) {
    assert(SDL_Init(SDL_INIT_TIMER)==0);World *w=calloc(1,sizeof(*w));assert(w&&world_init(w));
    w->solid_count=w->platform_count=0;
    ledge_and_buffer(w);movement_interactions(w);network_timers(w);
    mesh_free(&w->mesh);free(w);SDL_Quit();return 0;
}
