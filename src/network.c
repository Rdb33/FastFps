#include "network.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static uint32_t get32(const uint8_t *p){return (uint32_t)p[0]<<24|(uint32_t)p[1]<<16|(uint32_t)p[2]<<8|p[3];}
static void put32(uint8_t *p,uint32_t v){p[0]=(uint8_t)(v>>24);p[1]=(uint8_t)(v>>16);p[2]=(uint8_t)(v>>8);p[3]=(uint8_t)v;}
static bool newer(uint32_t a,uint32_t b){return (int32_t)(a-b)>0;}
static void message(Network *n,const char *text){snprintf(n->message,sizeof(n->message),"%s",text);}
static bool valid_name(const char *name) {
    size_t len=strnlen(name,PLAYER_NAME_LENGTH+1);
    if(!len||len>PLAYER_NAME_LENGTH)return false;
    for(size_t i=0;i<len;i++)if((unsigned char)name[i]<32||(unsigned char)name[i]>126)return false;
    return true;
}
static void peer_clear(Peer *p) {
    if(p->fd>=0)close(p->fd);
    memset(p,0,sizeof(*p));p->fd=-1;
}
void net_init(Network *n) {
    memset(n,0,sizeof(*n));n->listener=-1;n->discovery_fd=-1;n->port=NET_DEFAULT_PORT;n->latency_ms=-1;
    snprintf(n->port_text,sizeof(n->port_text),"%d",NET_DEFAULT_PORT);
    snprintf(n->name,sizeof(n->name),"JOUEUR");
    for(int i=0;i<MAX_PLAYERS;i++)n->peers[i].fd=-1;
}
void net_close(Network *n) {
    net_discovery_close(n);
    if(n->listener>=0)close(n->listener);
    n->listener=-1;
    for(int i=0;i<MAX_PLAYERS;i++)peer_clear(&n->peers[i]);
    memset(n->members,0,sizeof(n->members));memset(n->history,0,sizeof(n->history));
    n->mode=NET_OFF;n->phase=ROOM_WAITING;n->connecting=false;n->has_snapshot=false;n->sequence=0;n->local_id=0;
    n->latency_ms=-1;n->last_ping=0;
}
static bool settings(Network *n) {
    char *end=NULL;long port=strtol(n->port_text,&end,10);
    if(!end||*end||port<1||port>65535){message(n,"PORT INVALIDE (1 A 65535)");return false;}
    n->port=(int)port;
    if(!valid_name(n->name)){message(n,"CHOISIS UN PSEUDO DE 1 A 16 CARACTERES ASCII");return false;}
    return true;
}
static bool socket_setup(int fd) {
    int flags=fcntl(fd,F_GETFL,0),yes=1;
    if(flags<0||fcntl(fd,F_SETFL,flags|O_NONBLOCK)<0)return false;
    setsockopt(fd,IPPROTO_TCP,TCP_NODELAY,&yes,sizeof(yes));
    setsockopt(fd,SOL_SOCKET,SO_KEEPALIVE,&yes,sizeof(yes));
    fcntl(fd,F_SETFD,FD_CLOEXEC);return true;
}
bool net_host(Network *n) {
    net_close(n);if(!settings(n))return false;
    net_local_addresses(n);
    int fd=socket(AF_INET,SOCK_STREAM,0);
    if(fd<0){snprintf(n->message,sizeof(n->message),"RESEAU INDISPONIBLE : %s",strerror(errno));return false;}
    int yes=1;setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes));
    struct sockaddr_in address={.sin_family=AF_INET,.sin_port=htons((uint16_t)n->port),.sin_addr={.s_addr=htonl(INADDR_ANY)}};
    if(!socket_setup(fd)||bind(fd,(struct sockaddr *)&address,sizeof(address))<0||listen(fd,MAX_PLAYERS)<0) {
        snprintf(n->message,sizeof(n->message),"CREATION IMPOSSIBLE : %s",strerror(errno));close(fd);return false;
    }
    n->listener=fd;n->mode=NET_HOST;n->members[0].present=true;n->members[0].ready=true;
    snprintf(n->members[0].name,sizeof(n->members[0].name),"%s",n->name);
    message(n,"SALON OUVERT - COPIE L'INVITATION POUR TES AMIS");return true;
}
static bool queue(Network *n,int id,PacketType type,const uint8_t *body,size_t size) {
    Peer *p=&n->peers[id];
    if(p->fd==-1||size+1>NET_PACKET_MAX)return false;
    if(type==PACKET_SNAPSHOT&&p->pending>NET_PACKET_MAX*2)return true;
    if(p->pending+size+5>NET_BUFFER_MAX)return false;
    put32(p->send+p->pending,(uint32_t)size+1);p->send[p->pending+4]=(uint8_t)type;
    if(size)memcpy(p->send+p->pending+5,body,size);
    p->pending+=size+5;return true;
}
static void hello(Network *n) {
    uint8_t body[2+PLAYER_NAME_LENGTH+1]={NET_PROTOCOL,1};
    memcpy(body+2,n->name,strlen(n->name));
    queue(n,0,PACKET_HELLO,body,sizeof(body));
}
bool net_join(Network *n) {
    net_close(n);if(!net_parse_address(n)||!settings(n))return false;
    struct sockaddr_in address={.sin_family=AF_INET,.sin_port=htons((uint16_t)n->port)};
    if(inet_pton(AF_INET,n->address,&address.sin_addr)!=1){message(n,"ENTRE UNE ADRESSE IPv4, PAR EXEMPLE 192.168.1.20");return false;}
    int fd=socket(AF_INET,SOCK_STREAM,0);
    if(fd<0){snprintf(n->message,sizeof(n->message),"RESEAU INDISPONIBLE : %s",strerror(errno));return false;}
    if(!socket_setup(fd)){close(fd);message(n,"INITIALISATION RESEAU IMPOSSIBLE");return false;}
    int result=connect(fd,(struct sockaddr *)&address,sizeof(address));
    if(result<0&&errno!=EINPROGRESS){snprintf(n->message,sizeof(n->message),"CONNEXION IMPOSSIBLE : %s",strerror(errno));close(fd);return false;}
    n->mode=NET_CLIENT;n->connecting=true;n->peers[0].fd=fd;n->peers[0].connecting=result<0;
    n->peers[0].started_connect=(double)SDL_GetPerformanceCounter()/(double)SDL_GetPerformanceFrequency();
    n->peers[0].last_receive=n->peers[0].started_connect;
    message(n,"CONNEXION AU SALON...");
    if(result==0)hello(n);
    return true;
}
static void roster(Network *n,Game *g) {
    uint8_t body[9+MAX_PLAYERS*(2+PLAYER_NAME_LENGTH+1)]={0};
    body[0]=(uint8_t)n->phase;put32(body+1,(uint32_t)g->score);put32(body+5,(uint32_t)g->wave);
    size_t pos=9;
    for(int i=0;i<MAX_PLAYERS;i++) {
        body[pos++]=n->members[i].present;body[pos++]=n->members[i].ready;
        memcpy(body+pos,n->members[i].name,PLAYER_NAME_LENGTH+1);pos+=PLAYER_NAME_LENGTH+1;
    }
    for(int i=1;i<MAX_PLAYERS;i++)if(n->peers[i].accepted&&!n->peers[i].closing)
        if(!queue(n,i,PACKET_ROSTER,body,pos))n->peers[i].closing=true;
}
static void disconnect(Network *n,int id,Game *g,const char *reason) {
    if(n->mode==NET_CLIENT){net_close(n);message(n,reason);return;}
    peer_clear(&n->peers[id]);n->members[id]=(Member){0};g->actors[id].active=false;
    roster(n,g);
}
static void reject(Network *n,int id,const char *reason) {
    queue(n,id,PACKET_REJECT,(const uint8_t *)reason,strlen(reason));n->peers[id].closing=true;
}
static void reconcile(Network *n,Game *g,const uint8_t *data,size_t size,double now) {
    Game snapshot=*g;snapshot.local_actor=n->local_id;
    if(!protocol_snapshot_read(&snapshot,data,size)||!snapshot.actors[n->local_id].active) {
        disconnect(n,0,g,"ETAT RESEAU INVALIDE");return;
    }
    float yaw=g->player.yaw,pitch=g->player.pitch,sway_x=g->player.sway_x,sway_y=g->player.sway_y;
    n->previous=n->has_snapshot?n->current:snapshot;n->current=snapshot;n->snapshot_time=now;n->has_snapshot=true;
    for(int i=0;i<MAX_PLAYERS;i++)if(i!=n->local_id&&snapshot.actors[i].shots!=n->previous.actors[i].shots)
        audio_play_at(snapshot.actors[i].player.weapon?SOUND_SHOTGUN:SOUND_SHOT,snapshot.actors[i].player.body.position);
    const Actor *local=&snapshot.actors[n->local_id],*old=&n->previous.actors[n->local_id];
    if(local->player.health<old->player.health)audio_play(SOUND_HURT);
    if(local->player.health>old->player.health&&!old->dead)audio_play(SOUND_PICKUP);
    if(player_level(&local->player)>player_level(&old->player))audio_play(SOUND_UPGRADE);
    if(local->kill_time>old->kill_time+.02f)audio_play(SOUND_KILL);
    else if(local->headshot_time>old->headshot_time+.02f)audio_play(SOUND_HEADSHOT);
    else if(local->hit_time>old->hit_time+.02f)audio_play(SOUND_HIT);
    for(int i=0;i<snapshot.barrel_count;i++)if(snapshot.barrels[i].health<=0
        &&i<n->previous.barrel_count&&n->previous.barrels[i].health>0)
        audio_play_at(SOUND_EXPLOSION,snapshot.barrels[i].position);
    for(int i=0;i<snapshot.enemy_count;i++) {
        const Enemy *e=&snapshot.enemies[i];
        for(int j=0;j<n->previous.enemy_count;j++)if(n->previous.enemies[j].id==e->id
            &&e->attack_timer>n->previous.enemies[j].attack_timer+.04f)
            audio_play_at(e->type==ENEMY_CASTER?SOUND_CAST:SOUND_GROWL,e->body.position);
    }
    if(snapshot.wave>n->previous.wave)audio_play(snapshot.wave%5==0?SOUND_CHAMPION:SOUND_WAVE);
    if(snapshot.completed_waves>n->previous.completed_waves)audio_play(SOUND_UPGRADE);
    if(snapshot.event_serial!=n->previous.event_serial)audio_play(SOUND_CHAMPION);
    if(snapshot.unlock_time>n->previous.unlock_time+.2f)audio_play(SOUND_UPGRADE);
    if(old->dead&&!local->dead)audio_play(SOUND_PICKUP);
    for(int i=0;i<snapshot.blast_count;i++)if(snapshot.blasts[i].time>.72f) {
        bool seen=false;for(int j=0;j<n->previous.blast_count;j++)if(vlength(vsub(snapshot.blasts[i].position,n->previous.blasts[j].position))<.1f
            &&n->previous.blasts[j].time>=snapshot.blasts[i].time-.1f)seen=true;
        if(!seen)audio_play_at(SOUND_EXPLOSION,snapshot.blasts[i].position);
    }
    for(int i=0;i<snapshot.enemy_count;i++)for(int j=0;j<n->previous.enemy_count;j++)if(snapshot.enemies[i].id==n->previous.enemies[j].id) {
        if(snapshot.enemies[i].slam_state==1&&n->previous.enemies[j].slam_state==0)audio_play_at(SOUND_CHAMPION,snapshot.enemies[i].body.position);
        if(snapshot.enemies[i].slam_flash>n->previous.enemies[j].slam_flash+.2f)audio_play_at(SOUND_EXPLOSION,snapshot.enemies[i].body.position);
    }
    *g=snapshot;
    uint32_t ack=g->actors[n->local_id].input_ack,count=n->sequence-ack;
    if(snapshot.map_id!=n->previous.map_id){count=0;n->previous=snapshot;}
    audio_suppressed(true);
    if(!g->round_over&&count<NET_HISTORY)for(uint32_t i=1;i<=count;i++) {
        uint32_t seq=ack+i;const Command *c=&n->history[seq%NET_HISTORY];
        if(c->sequence!=seq)continue;
        g->player.yaw=c->yaw;g->player.pitch=c->pitch;
        game_request_weapon(g,c->weapon);game_predict(g,c->input,FIXED_STEP);
    }
    audio_suppressed(false);
    g->player.yaw=yaw;g->player.pitch=pitch;g->player.sway_x=sway_x;g->player.sway_y=sway_y;
    game_actor_save(g);
    if(g->round_over)n->phase=ROOM_FINISHED;
}
static bool packet(Network *n,int id,Game *g,uint8_t type,const uint8_t *body,size_t size,double now) {
    Peer *p=&n->peers[id];
    if(type==PACKET_PING) {
        if(size!=4||!p->accepted)return false;
        if(n->mode==NET_HOST)queue(n,id,PACKET_PING,body,size);
        else {
            uint32_t elapsed=(uint32_t)(uint64_t)(now*1000)-get32(body);
            if(elapsed<12000)n->latency_ms=n->latency_ms<0?(float)elapsed:n->latency_ms*.7f+(float)elapsed*.3f;
        }
        return true;
    }
    if(n->mode==NET_HOST) {
        if(type==PACKET_HELLO) {
            if(p->accepted||size!=2+PLAYER_NAME_LENGTH+1)return false;
            if(body[0]!=NET_PROTOCOL||body[1]!=1){reject(n,id,"VERSION INCOMPATIBLE - UTILISE FASTFPS " GAME_VERSION);return true;}
            if(n->phase==ROOM_PLAYING){reject(n,id,"PARTIE DEJA EN COURS");return true;}
            char name[PLAYER_NAME_LENGTH+1];memcpy(name,body+2,sizeof(name));
            if(!valid_name(name))return false;
            p->accepted=true;n->members[id].present=true;snprintf(n->members[id].name,sizeof(n->members[id].name),"%s",name);
            uint8_t assigned=(uint8_t)id;queue(n,id,PACKET_WELCOME,&assigned,1);roster(n,g);return true;
        }
        if(!p->accepted)return false;
        if(type==PACKET_READY) {
            if(size!=1||body[0]>1||n->phase==ROOM_PLAYING)return false;
            n->members[id].ready=body[0]!=0;roster(n,g);return true;
        }
        if(type==PACKET_INPUT) {
            Command command;if(!protocol_command_read(&command,body,size))return false;
            if(n->phase!=ROOM_PLAYING)return true;
            if(command.round!=n->round)return true;
            uint32_t latest=p->count?p->commands[(p->head+p->count-1)%NET_HISTORY].sequence:p->last_command.sequence;
            if(!newer(command.sequence,latest))return true;
            if(p->count>=NET_HISTORY)return false;
            p->commands[(p->head+p->count)%NET_HISTORY]=command;p->count++;return true;
        }
        return false;
    }
    if(type==PACKET_REJECT) {
        if(size==0||size>=sizeof(n->message))return false;
        char reason[128];memcpy(reason,body,size);reason[size]=0;disconnect(n,0,g,reason);return true;
    }
    if(type==PACKET_WELCOME) {
        if(size!=1||body[0]==0||body[0]>=MAX_PLAYERS||p->accepted)return false;
        n->local_id=body[0];p->accepted=true;n->connecting=false;
        uint8_t ready=1;queue(n,0,PACKET_READY,&ready,1);
        message(n,"SALON REJOINT - TU ES PRET, L'HOTE PEUT LANCER");return true;
    }
    if(!p->accepted)return false;
    if(type==PACKET_ROSTER) {
        if(size!=9+MAX_PLAYERS*(2+PLAYER_NAME_LENGTH+1)||body[0]>ROOM_FINISHED)return false;
        n->phase=(RoomPhase)body[0];g->score=(int)get32(body+1);g->wave=(int)get32(body+5);
        size_t pos=9;
        for(int i=0;i<MAX_PLAYERS;i++) {
            uint8_t present=body[pos++],ready=body[pos++];if(present>1||ready>1)return false;
            n->members[i].present=present!=0;n->members[i].ready=ready!=0;
            memcpy(n->members[i].name,body+pos,PLAYER_NAME_LENGTH+1);pos+=PLAYER_NAME_LENGTH+1;
            if(present&&!valid_name(n->members[i].name))return false;
        }
        return true;
    }
    if(type==PACKET_START) {
        if(size!=4)return false;
        n->round=get32(body);
        bool active[MAX_PLAYERS];for(int i=0;i<MAX_PLAYERS;i++)active[i]=n->members[i].present;
        game_start_coop(g,active,n->local_id);
        for(int i=0;i<MAX_PLAYERS;i++)snprintf(g->actors[i].name,sizeof(g->actors[i].name),"%s",n->members[i].name);
        n->phase=ROOM_PLAYING;n->sequence=0;n->has_snapshot=false;memset(n->history,0,sizeof(n->history));
        return true;
    }
    if(type==PACKET_SNAPSHOT) {
        if(n->phase!=ROOM_PLAYING&&n->phase!=ROOM_FINISHED)return false;
        reconcile(n,g,body,size,now);return true;
    }
    return false;
}
bool net_feed(Network *n,int id,Game *g,const uint8_t *data,size_t size,double now) {
    if(id<0||id>=MAX_PLAYERS)return false;
    Peer *p=&n->peers[id];
    if(p->received>NET_BUFFER_MAX||size>NET_BUFFER_MAX-p->received)return false;
    if(size)memcpy(p->receive+p->received,data,size);
    p->received+=size;p->last_receive=now;
    size_t used=0;
    while(p->received-used>=4) {
        uint32_t length=get32(p->receive+used);
        if(length<1||length>NET_PACKET_MAX)return false;
        if(p->received-used<(size_t)length+4)break;
        uint8_t type=p->receive[used+4];
        if(!packet(n,id,g,type,p->receive+used+5,length-1,now))return false;
        if(n->mode==NET_OFF)return true;
        used+=(size_t)length+4;if(p->closing)break;
    }
    if(used){memmove(p->receive,p->receive+used,p->received-used);p->received-=used;}
    return true;
}
size_t net_take_output(Network *n,int id,uint8_t *data,size_t capacity) {
    if(id<0||id>=MAX_PLAYERS)return 0;
    Peer *p=&n->peers[id];
    size_t size=p->pending<capacity?p->pending:capacity;
    if(size==0)return 0;
    memcpy(data,p->send,size);memmove(p->send,p->send+size,p->pending-size);p->pending-=size;return size;
}
static void snapshots(Network *n,Game *g,double now) {
    uint8_t data[NET_PACKET_MAX];game_actor_save(g);
    size_t size=protocol_snapshot_write(data,sizeof(data),g);
    if(!size){message(n,"ETAT TROP VOLUMINEUX");return;}
    for(int i=1;i<MAX_PLAYERS;i++)if(n->peers[i].accepted&&!n->peers[i].closing)
        if(!queue(n,i,PACKET_SNAPSHOT,data,size))n->peers[i].closing=true;
    n->last_snapshot=now;
}
bool net_start(Network *n,Game *g,double now) {
    if(n->mode!=NET_HOST||n->phase==ROOM_PLAYING)return false;
    bool active[MAX_PLAYERS];
    for(int i=0;i<MAX_PLAYERS;i++) {
        active[i]=n->members[i].present;
        if(active[i]&&!n->members[i].ready){message(n,"ATTENDS QUE TOUS LES JOUEURS SOIENT PRETS");return false;}
    }
    game_start_coop(g,active,0);n->phase=ROOM_PLAYING;
    n->round++;uint8_t round[4];put32(round,n->round);
    for(int i=0;i<MAX_PLAYERS;i++) {
        snprintf(g->actors[i].name,sizeof(g->actors[i].name),"%s",n->members[i].name);
        n->peers[i].head=0;n->peers[i].count=0;n->peers[i].last_command=(Command){0};
        if(i>0&&n->peers[i].accepted)queue(n,i,PACKET_START,round,sizeof(round));
    }
    snapshots(n,g,now);return true;
}
void net_ready(Network *n) {
    if(n->mode!=NET_CLIENT||n->phase==ROOM_PLAYING||n->connecting)return;
    uint8_t ready=!n->members[n->local_id].ready;queue(n,0,PACKET_READY,&ready,1);
}
void net_poll(Network *n,Game *g,double now) {
    if(n->mode==NET_OFF)return;
    if(n->mode==NET_HOST&&n->listener>=0)for(int attempts=0;attempts<MAX_PLAYERS;attempts++) {
        int fd=accept(n->listener,NULL,NULL);
        if(fd<0)break;
        int slot=-1;for(int i=1;i<MAX_PLAYERS;i++)if(n->peers[i].fd==-1){slot=i;break;}
        if(slot<0){uint8_t full[32];const char *text="SALON COMPLET";size_t len=strlen(text);put32(full,(uint32_t)len+1);full[4]=PACKET_REJECT;memcpy(full+5,text,len);send(fd,full,len+5,MSG_DONTWAIT|MSG_NOSIGNAL);close(fd);continue;}
        if(!socket_setup(fd)){close(fd);continue;}
        n->peers[slot].fd=fd;n->peers[slot].last_receive=now;n->peers[slot].started_connect=now;
    }
    for(int id=0;id<MAX_PLAYERS;id++) {
        Peer *p=&n->peers[id];if(p->fd<0)continue;
        if(p->connecting) {
            struct pollfd wait={.fd=p->fd,.events=POLLOUT};
            if(poll(&wait,1,0)>0) {
                int error=0;socklen_t length=sizeof(error);
                if(getsockopt(p->fd,SOL_SOCKET,SO_ERROR,&error,&length)<0||error) {
                    char text[128];snprintf(text,sizeof(text),"CONNEXION REFUSEE : %s",strerror(error?error:errno));disconnect(n,id,g,text);return;
                }
                p->connecting=false;hello(n);
            }else if(now-p->started_connect>8){disconnect(n,id,g,"AUCUNE REPONSE - VERIFIE L'INVITATION ET LE VPN / PARE-FEU");return;}
            else continue;
        }
        bool lost=false;
        for(int attempt=0;attempt<16;attempt++) {
            uint8_t data[8192];ssize_t count=recv(p->fd,data,sizeof(data),MSG_DONTWAIT);
            if(count>0){if(!net_feed(n,id,g,data,(size_t)count,now)){lost=true;break;}if(n->mode==NET_OFF)return;}
            else if(count==0){lost=true;break;}
            else if(errno==EINTR)continue;
            else if(errno!=EAGAIN&&errno!=EWOULDBLOCK){lost=true;break;}
            else break;
        }
        if(!lost)for(int attempt=0;attempt<16&&p->pending;attempt++) {
            ssize_t count=send(p->fd,p->send,p->pending,MSG_DONTWAIT|MSG_NOSIGNAL);
            if(count>0){memmove(p->send,p->send+count,p->pending-(size_t)count);p->pending-=(size_t)count;}
            else if(count<0&&errno==EINTR)continue;
            else if(count<0&&(errno==EAGAIN||errno==EWOULDBLOCK))break;
            else {lost=true;break;}
        }
        if(lost||(p->closing&&p->pending==0)||now-p->last_receive>12
            ||(n->mode==NET_HOST&&!p->accepted&&now-p->started_connect>8)) {
            disconnect(n,id,g,"CONNEXION AU SALON PERDUE");if(n->mode==NET_OFF)return;
        }
    }
    if(n->mode==NET_CLIENT&&n->peers[0].accepted&&now-n->last_ping>1) {
        uint8_t data[4];put32(data,(uint32_t)(uint64_t)(now*1000));queue(n,0,PACKET_PING,data,sizeof(data));n->last_ping=now;
    }
}
void net_tick(Network *n,Game *g,Command local,float dt,double now) {
    if(n->phase!=ROOM_PLAYING)return;
    if(n->mode==NET_CLIENT) {
        local.sequence=++n->sequence;local.round=n->round;n->history[local.sequence%NET_HISTORY]=local;
        uint8_t data[64];size_t size=protocol_command_write(data,sizeof(data),&local);
        if(!queue(n,0,PACKET_INPUT,data,size)){disconnect(n,0,g,"CONNEXION TROP LENTE");return;}
        n->last_input=now;game_predict(g,local.input,dt);return;
    }
    Input inputs[MAX_PLAYERS]={0};inputs[0]=local.input;game_actor_save(g);
    uint32_t acknowledgements[MAX_PLAYERS]={0};
    for(int i=1;i<MAX_PLAYERS;i++)if(n->members[i].present) {
        Peer *p=&n->peers[i];
        if(p->count>6){unsigned drop=p->count-4;p->head=(p->head+drop)%NET_HISTORY;p->count-=drop;}
        Command c=p->last_command;
        if(p->count){c=p->commands[p->head];p->head=(p->head+1)%NET_HISTORY;p->count--;p->last_command=c;}
        if(now-p->last_receive>.25)c.input=(Input){0};
        inputs[i]=c.input;g->actors[i].player.yaw=c.yaw;g->actors[i].player.pitch=c.pitch;
        acknowledgements[i]=c.sequence;
        game_actor_select(g,i);game_request_weapon(g,c.weapon);game_actor_save(g);
    }
    game_actor_select(g,0);game_update_coop(g,inputs,dt);
    for(int i=1;i<MAX_PLAYERS;i++)if(n->members[i].present)g->actors[i].input_ack=acknowledgements[i];
    if(g->round_over){n->phase=ROOM_FINISHED;snapshots(n,g,now);roster(n,g);}
    else if(now-n->last_snapshot>=NET_SNAPSHOT_INTERVAL)snapshots(n,g,now);
}
bool net_connected(const Network *n){return n->mode!=NET_OFF&&!n->connecting;}
static float angle_lerp(float a,float b,float t){return a+atan2f(sinf(b-a),cosf(b-a))*t;}
static Vec3 position_lerp(Vec3 a,Vec3 b,float t){return vadd(a,vscale(vsub(b,a),t));}
void net_render(const Network *n,const Game *g,Game *view,double now) {
    *view=*g;
    if(n->mode!=NET_CLIENT||!n->has_snapshot)return;
    float age=clampf((float)(now-n->snapshot_time),0,.1f);
    view->time=n->current.time+age;
    for(int i=0;i<view->barrel_count;i++)view->barrels[i].explosion_time=fmaxf(0,view->barrels[i].explosion_time-age);
    for(int i=0;i<view->particle_count;i++) {
        Particle *p=&view->particles[i];p->life-=age;p->position=vadd(p->position,vscale(p->velocity,age));
    }
    float alpha=clampf((float)((now-n->snapshot_time)/NET_SNAPSHOT_INTERVAL),0,1);
    for(int i=0;i<MAX_PLAYERS;i++)if(i!=n->local_id&&view->actors[i].active) {
        const Actor *a=&n->previous.actors[i],*b=&n->current.actors[i];
        if(a->active&&!a->dead&&!b->dead&&vlength(vsub(a->player.body.position,b->player.body.position))<5) {
            view->actors[i].player.body.position=position_lerp(a->player.body.position,b->player.body.position,alpha);
            view->actors[i].player.yaw=angle_lerp(a->player.yaw,b->player.yaw,alpha);
            view->actors[i].player.bob=a->player.bob+(b->player.bob-a->player.bob)*alpha;
        }
    }
    for(int i=0;i<view->enemy_count;i++)for(int j=0;j<n->previous.enemy_count;j++) {
        const Enemy *a=&n->previous.enemies[j];Enemy *b=&view->enemies[i];
        if(a->id!=b->id)continue;
        b->body.position=position_lerp(a->body.position,b->body.position,alpha);
        b->yaw=angle_lerp(a->yaw,b->yaw,alpha);b->walk_phase=a->walk_phase+(b->walk_phase-a->walk_phase)*alpha;
        break;
    }
}
