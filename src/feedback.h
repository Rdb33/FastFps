#ifndef FASTFPS_FEEDBACK_H
#define FASTFPS_FEEDBACK_H

#include "fastfps.h"

#define FEEDBACK_PARTICLES 112
#define FEEDBACK_MARKS 20
#define FEEDBACK_RINGS 6

typedef struct {
    Vec3 position, velocity;
    Color color;
    float life, duration, size, angle, spin;
    int kind;
} FeedbackParticle;
typedef struct {
    Vec3 position, normal, offset;
    float life, size;
    int platform;
} FeedbackMark;
typedef struct { Vec3 position; Color color; float life, duration, radius; } FeedbackRing;
typedef struct {
    Vec3 position;
    float fall_speed, dust_cooldown, health, shield, damage_time;
    uint32_t shots;
    int kills, heads, weapon;
    bool grounded, active;
} FeedbackActor;

typedef struct Feedback {
    bool initialized, impact_drawn, hold_drawn;
    Uint64 clock;
    uint32_t rng, tick, revision;
    int wave, local_actor, enemy_count, corpse_count;
    Enemy enemies[MAX_ENEMIES], corpses[MAX_CORPSES];
    FeedbackActor actors[MAX_PLAYERS];
    FeedbackParticle particles[FEEDBACK_PARTICLES]; int particle_count;
    FeedbackMark marks[FEEDBACK_MARKS]; int mark_count;
    FeedbackRing rings[FEEDBACK_RINGS]; int ring_count;
    float impact_time, impact_duration, impact_cooldown, hold_time;
    float reactor_cooldown, reactor_pulse;
    float shake_time, shake_duration, shake_strength;
    float health_trail, health_delay, heal_pop, reload_pop;
    int cached_ammo, cached_weapon;
    float cached_reload, cached_dash;
    float dash_ready, weapon_ready, land_time, land_strength, speed_fov;
    float unlock_notice, barrel_times[MAX_BARRELS];
    uint32_t event_serial;
    int completed_waves, blast_count;
    Blast blasts[MAX_BLASTS];
    int impact_kind, impact_phase, impact_ids[2], held_weapon;
    Vec3 impact_point;
    Color impact_color;
} Feedback;

void feedback_update(Feedback *feedback, const Game *game, const App *app);
void feedback_draw(Feedback *feedback, Mesh *mesh, const Game *game, const App *app);
void feedback_ui(const Feedback *feedback, Mesh *mesh, const Game *game, const App *app);
void feedback_tint(const Feedback *feedback, Mesh *mesh, int first, int enemy, float intensity);
bool feedback_hold_weapon(const Feedback *feedback, const Game *game, const App *app);
float feedback_impact(const Feedback *feedback, const App *app);
Vec3 feedback_camera_shake(const Feedback *feedback, const App *app);
float feedback_landing(const Feedback *feedback, const App *app);
Color feedback_enemy_color(const Enemy *enemy);

#endif
