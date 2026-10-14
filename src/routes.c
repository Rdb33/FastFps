#include "fastfps.h"
#include <string.h>

/* Map geometry, pickups and predicted movement share these definitions. */
void world_routes(World *w) {
    static const Deck decks[MAP_COUNT][MAX_DECKS]={
        {{{-21,3.2f,0},{12,.28f,7},{-22,3.48f,2},PI_F*.5f,-4},
         {{21,3.2f,0},{12,.28f,7},{22,3.48f,2},-PI_F*.5f,4}},
        {{{0,3.52f,-19},{12,.28f,6},{-2,3.8f,-20},PI_F,-3.8f},
         {{0,3.52f,19},{12,.28f,6},{-2,3.8f,20},0,3.8f}},
        {{{-21,3.2f,-3},{12,.28f,7},{-22,3.48f,-1},PI_F*.5f,-4},
         {{21,4,3},{12,.28f,7},{22,4.28f,5},-PI_F*.5f,4}}
    };
    static const Platform routes[MAP_COUNT][MAX_PLATFORMS]={
        {
            {{-21,0,-7.75f},{3.4f,.28f,3.4f},{0,3.2f,0},8,0,{0,0,0},.16f,0},
            {{21,0,7.75f},{3.4f,.28f,3.4f},{0,3.2f,0},9,0,{0,0,0},.16f,0},
            {{-15.3f,3.2f,-4},{4.2f,.28f,3.2f},{30.6f,0,0},12,0,{0,0,0},.16f,0}
        },{
            {{-7.75f,0,-19},{3.4f,.28f,3.4f},{0,3.52f,0},9,0,{0,0,0},.23f,.12f},
            {{7.75f,0,19},{3.4f,.28f,3.4f},{0,3.52f,0},7,0,{0,0,0},.18f,.58f},
            {{3.8f,3.52f,-14.3f},{3.2f,.28f,3.2f},{0,0,28.6f},12,0,{0,0,0},.22f,0}
        },{
            {{-22,0,6.75f},{3.4f,.28f,3.4f},{1,3.2f,-2},10,0,{0,0,0},.18f,.20f},
            {{22,0,-6.75f},{3.4f,.28f,3.4f},{-1,4,2},11,0,{0,0,0},.14f,.60f},
            {{-15.3f,3.2f,-7},{4.2f,.28f,3.2f},{30.6f,.8f,6},14,0,{0,1.6f,0},.12f,0}
        }
    };
    memcpy(w->decks,decks[w->map_id],sizeof(w->decks));
    memcpy(w->platforms,routes[w->map_id],sizeof(w->platforms));w->platform_count=MAX_PLATFORMS;
}
const char *world_route_name(int map) {
    static const char *names[MAP_COUNT]={"TRAVERSEE LATERALE", "CONVOYEUR NORD-SUD", "PASSERELLE EN ARC"};
    return names[map>=0&&map<MAP_COUNT?map:0];
}
