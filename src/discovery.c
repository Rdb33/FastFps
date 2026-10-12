#define _DEFAULT_SOURCE
#include "network.h"
#include <arpa/inet.h>
#include <ctype.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

/* Petits messages de découverte uniquement. La partie utilise toujours TCP.
   Les réponses doivent reprendre le jeton de notre recherche courante. */
static const uint8_t magic[8]={'F','F','P','S','L','A','N','1'};

bool net_parse_address(Network *n) {
    char text[sizeof(n->address)];
    snprintf(text,sizeof(text),"%s",n->address);
    char *start=text;while(isspace((unsigned char)*start))start++;
    size_t length=strlen(start);
    while(length&&isspace((unsigned char)start[length-1]))start[--length]=0;
    if(strncmp(start,"fastfps://",10)==0)start+=10;
    char *colon=strchr(start,':');
    int port=n->port;
    if(colon) {
        *colon++=0;char *end=NULL;long value=strtol(colon,&end,10);
        if(!*colon||!end||*end||value<1||value>65535)goto invalid;
        port=(int)value;
    }else {
        char *end=NULL;long value=strtol(n->port_text,&end,10);
        if(!n->port_text[0]||!end||*end||value<1||value>65535)goto invalid;
        port=(int)value;
    }
    struct in_addr address;
    if(inet_pton(AF_INET,start,&address)!=1||address.s_addr==htonl(INADDR_ANY)
        ||(ntohl(address.s_addr)>>24)>=224)goto invalid;
    snprintf(n->address,sizeof(n->address),"%s",start);
    n->port=port;snprintf(n->port_text,sizeof(n->port_text),"%d",port);
    return true;
invalid:
    snprintf(n->message,sizeof(n->message),"COLLE UNE INVITATION fastfps://IP:PORT OU UNE IPv4");
    return false;
}

void net_local_addresses(Network *n) {
    n->local_address_count=0;
    struct ifaddrs *interfaces=NULL;
    if(getifaddrs(&interfaces)!=0)return;
    for(struct ifaddrs *it=interfaces;it;it=it->ifa_next) {
        if(!it->ifa_addr||it->ifa_addr->sa_family!=AF_INET||!(it->ifa_flags&IFF_UP)
            ||(it->ifa_flags&IFF_LOOPBACK))continue;
        const struct sockaddr_in *a=(const struct sockaddr_in *)it->ifa_addr;
        uint32_t ip=ntohl(a->sin_addr.s_addr);
        if((ip>>24)==127||(ip>>16)==0xa9fe)continue;
        LocalAddress item={0};
        if(!inet_ntop(AF_INET,&a->sin_addr,item.address,sizeof(item.address)))continue;
        item.vpn=(ip&0xffc00000u)==0x64400000u||strstr(it->ifa_name,"tailscale")
            ||strstr(it->ifa_name,"tun")||strstr(it->ifa_name,"wg")||strstr(it->ifa_name,"zt");
        bool duplicate=false;
        for(int i=0;i<n->local_address_count;i++)if(strcmp(item.address,n->local_addresses[i].address)==0)duplicate=true;
        if(duplicate||n->local_address_count>=8)continue;
        int slot=n->local_address_count++;
        /* L'adresse VPN apparaît en premier pour les invitations Internet. */
        if(item.vpn) {
            memmove(n->local_addresses+1,n->local_addresses,(size_t)slot*sizeof(LocalAddress));slot=0;
        }
        n->local_addresses[slot]=item;
    }
    freeifaddrs(interfaces);
}

bool net_invitation(const Network *n,char *text,size_t capacity) {
    const char *address=n->mode==NET_HOST?
        (n->local_address_count?n->local_addresses[0].address:NULL):n->address;
    if(!address||!*address)return false;
    int size=snprintf(text,capacity,"fastfps://%s:%d",address,n->port);
    return size>0&&(size_t)size<capacity;
}

void net_discovery_close(Network *n) {
    if(n->discovery_fd>=0)close(n->discovery_fd);
    n->discovery_fd=-1;n->discovery_role=0;n->discovery_failed=false;
}

static bool open_discovery(Network *n,int role) {
    int fd=socket(AF_INET,SOCK_DGRAM,0);if(fd<0)return false;
    int yes=1,flags=fcntl(fd,F_GETFL,0);
    if(flags<0||fcntl(fd,F_SETFL,flags|O_NONBLOCK)<0){close(fd);return false;}
    fcntl(fd,F_SETFD,FD_CLOEXEC);
    setsockopt(fd,SOL_SOCKET,SO_BROADCAST,&yes,sizeof(yes));
    if(role==2)setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes));
    struct sockaddr_in address={.sin_family=AF_INET,.sin_port=htons(role==2?NET_DISCOVERY_PORT:0),
        .sin_addr={.s_addr=htonl(INADDR_ANY)}};
    if(bind(fd,(struct sockaddr *)&address,sizeof(address))<0){close(fd);return false;}
    n->discovery_fd=fd;n->discovery_token=SDL_GetTicks()^((uint32_t)getpid()*2654435761u);
    n->discovery_query=-10;return true;
}

static void query(Network *n) {
    uint8_t data[13];memcpy(data,magic,8);data[8]='Q';
    for(int i=0;i<4;i++)data[9+i]=(uint8_t)(n->discovery_token>>(24-i*8));
    struct sockaddr_in target={.sin_family=AF_INET,.sin_port=htons(NET_DISCOVERY_PORT),
        .sin_addr={.s_addr=htonl(INADDR_BROADCAST)}};
    sendto(n->discovery_fd,data,sizeof(data),MSG_DONTWAIT,(struct sockaddr *)&target,sizeof(target));
    struct ifaddrs *interfaces=NULL;
    if(getifaddrs(&interfaces)!=0)return;
    for(struct ifaddrs *it=interfaces;it;it=it->ifa_next) {
        if(!it->ifa_broadaddr||it->ifa_broadaddr->sa_family!=AF_INET
            ||!(it->ifa_flags&IFF_UP)||!(it->ifa_flags&IFF_BROADCAST))continue;
        target.sin_addr=((struct sockaddr_in *)it->ifa_broadaddr)->sin_addr;
        sendto(n->discovery_fd,data,sizeof(data),MSG_DONTWAIT,(struct sockaddr *)&target,sizeof(target));
    }
    freeifaddrs(interfaces);
}

static void response(Network *n,const uint8_t *request,const struct sockaddr_in *sender) {
    uint8_t data[36]={0};memcpy(data,magic,8);data[8]='S';memcpy(data+9,request+9,4);
    data[13]=NET_PROTOCOL;data[14]=(uint8_t)n->phase;data[16]=MAX_PLAYERS;
    for(int i=0;i<MAX_PLAYERS;i++)if(n->members[i].present)data[15]++;
    data[17]=(uint8_t)(n->port>>8);data[18]=(uint8_t)n->port;
    memcpy(data+19,n->name,PLAYER_NAME_LENGTH);
    sendto(n->discovery_fd,data,sizeof(data),MSG_DONTWAIT,(const struct sockaddr *)sender,sizeof(*sender));
}

static void remember(Network *n,const uint8_t *data,const struct sockaddr_in *sender,double now) {
    uint32_t token=0;for(int i=0;i<4;i++)token=(token<<8)|data[9+i];
    int port=(int)data[17]*256+data[18];
    if(token!=n->discovery_token||data[13]!=NET_PROTOCOL||data[14]>ROOM_FINISHED
        ||data[15]<1||data[15]>MAX_PLAYERS||data[16]!=MAX_PLAYERS||!port||data[35]!=0)return;
    for(int i=19;i<35&&data[i];i++)if(data[i]<32||data[i]>126)return;
    NearbyRoom room={.port=port,.players=data[15],.phase=(RoomPhase)data[14],.seen=now};
    if(!inet_ntop(AF_INET,&sender->sin_addr,room.address,sizeof(room.address)))return;
    memcpy(room.name,data+19,sizeof(room.name));
    if(!room.name[0])return;
    int slot=-1;
    for(int i=0;i<NET_ROOMS;i++)if(n->rooms[i].port==port&&strcmp(n->rooms[i].address,room.address)==0){slot=i;break;}
    if(slot<0)for(int i=0;i<NET_ROOMS;i++)if(!n->rooms[i].name[0]){slot=i;break;}
    if(slot>=0)n->rooms[slot]=room;
}

void net_discovery_poll(Network *n,double now) {
    int role=n->mode==NET_HOST?2:(n->mode==NET_OFF&&n->discovery_requested?1:0);
    if(role!=n->discovery_role) {
        net_discovery_close(n);n->discovery_role=role;
        if(role&&!open_discovery(n,role))n->discovery_failed=true;
    }
    for(int i=0;i<NET_ROOMS;i++)if(n->rooms[i].name[0]&&now-n->rooms[i].seen>6)n->rooms[i]=(NearbyRoom){0};
    if(n->discovery_fd<0)return;
    if(role==1&&now-n->discovery_query>=2){query(n);n->discovery_query=now;}
    /* Budget fixe : un réseau bavard ne peut pas monopoliser une image. */
    for(int i=0;i<12;i++) {
        uint8_t data[64];struct sockaddr_in sender;socklen_t length=sizeof(sender);
        ssize_t size=recvfrom(n->discovery_fd,data,sizeof(data),MSG_DONTWAIT,(struct sockaddr *)&sender,&length);
        if(size<0)break;
        if(size<13||memcmp(data,magic,8)!=0||sender.sin_family!=AF_INET)continue;
        if(role==2&&size==13&&data[8]=='Q')response(n,data,&sender);
        else if(role==1&&size==36&&data[8]=='S')remember(n,data,&sender,now);
    }
}
