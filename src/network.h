#ifndef FASTFPS_NETWORK_H
#define FASTFPS_NETWORK_H
#include "fastfps.h"

#define NET_PROTOCOL 5
#define NET_DEFAULT_PORT 27015
#define NET_DISCOVERY_PORT 27014
#define NET_ROOMS 3
#define NET_PACKET_MAX 16384
#define NET_BUFFER_MAX 65536
#define NET_HISTORY 256
#define NET_SNAPSHOT_INTERVAL .05

typedef enum { NET_OFF, NET_HOST, NET_CLIENT } NetMode;
typedef enum { ROOM_WAITING, ROOM_PLAYING, ROOM_FINISHED } RoomPhase;
typedef enum { PACKET_HELLO=1, PACKET_WELCOME, PACKET_ROSTER, PACKET_READY,
    PACKET_START, PACKET_INPUT, PACKET_SNAPSHOT, PACKET_REJECT, PACKET_PING } PacketType;
typedef struct {
    Input input;
    uint32_t sequence;
    uint32_t round;
    float yaw, pitch;
    int weapon;
} Command;
typedef struct {
    bool present, ready;
    char name[PLAYER_NAME_LENGTH+1];
} Member;
typedef struct {
    int fd;
    bool connecting, accepted, closing;
    double last_receive, started_connect;
    uint8_t receive[NET_BUFFER_MAX], send[NET_BUFFER_MAX];
    size_t received, pending;
    Command commands[NET_HISTORY], last_command;
    unsigned head, count;
} Peer;
typedef struct {
    char address[16], name[PLAYER_NAME_LENGTH+1];
    int port, players;
    RoomPhase phase;
    double seen;
} NearbyRoom;
typedef struct {
    char address[16];
    bool vpn;
} LocalAddress;
typedef struct Network {
    NetMode mode;
    RoomPhase phase;
    int listener, local_id, port;
    bool connecting, has_snapshot;
    char name[PLAYER_NAME_LENGTH+1], address[96], port_text[6], message[128];
    Member members[MAX_PLAYERS];
    Peer peers[MAX_PLAYERS];
    Command history[NET_HISTORY];
    uint32_t sequence;
    uint32_t round;
    double last_snapshot, snapshot_time, last_input;
    Game previous, current;
    int discovery_fd, discovery_role;
    bool discovery_requested, discovery_failed;
    double discovery_query, last_ping;
    uint32_t discovery_token;
    NearbyRoom rooms[NET_ROOMS];
    LocalAddress local_addresses[8];
    int local_address_count;
    float latency_ms;
} Network;

void net_init(Network *net);
void net_close(Network *net);
bool net_host(Network *net);
bool net_join(Network *net);
void net_poll(Network *net, Game *game, double now);
bool net_start(Network *net, Game *game, double now);
void net_ready(Network *net);
void net_tick(Network *net, Game *game, Command local, float dt, double now);
void net_render(const Network *net, const Game *game, Game *view, double now);
bool net_connected(const Network *net);
bool net_parse_address(Network *net);
void net_local_addresses(Network *net);
void net_discovery_close(Network *net);
void net_discovery_poll(Network *net, double now);
bool net_invitation(const Network *net, char *text, size_t capacity);
bool net_feed(Network *net, int peer, Game *game, const uint8_t *data, size_t size, double now);
size_t net_take_output(Network *net, int peer, uint8_t *data, size_t capacity);
void lobby_open(App *app);
bool lobby_event(App *app, Game *game, Renderer *renderer, const SDL_Event *event);
void lobby_sync(App *app, Game *game);
void lobby_build(Mesh *mesh, const Game *game, const App *app);

/* Protocole indépendant des sockets, portable entre ARM et x86. */
size_t protocol_command_write(uint8_t *data, size_t capacity, const Command *cmd);
bool protocol_command_read(Command *cmd, const uint8_t *data, size_t size);
size_t protocol_snapshot_write(uint8_t *data, size_t capacity, const Game *game);
bool protocol_snapshot_read(Game *game, const uint8_t *data, size_t size);

#endif
