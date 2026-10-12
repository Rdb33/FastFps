#ifndef FASTFPS_H
#define FASTFPS_H

#include <stdbool.h>
#include <stdint.h>
#include <SDL.h>
#include <GLES2/gl2.h>
#include "config.h"

typedef struct { float x, y, z; } Vec3;
typedef struct { float r, g, b; } Color;
typedef struct { Vec3 min, max; } Box;
typedef struct { float m[16]; } Mat4;
typedef struct { Vec3 position; Color color; Vec3 normal; } Vertex;
typedef struct { Vertex *data; int count, capacity; } Mesh;
typedef struct { Vec3 position, velocity; bool grounded; } Body;
typedef struct {
    Mesh mesh;
    Box solids[MAX_SOLIDS]; int solid_count;
    Vec3 spawns[8];
    uint8_t passable[NAV_W * NAV_H];
    int16_t distance[NAV_W * NAV_H];
} World;
typedef struct {
    Body body;
    float yaw, pitch, health, bob, pad_cooldown;
    float movement_blend, strafe_blend, landing, sway_x, sway_y, step_timer;
    float dash_timer, dash_cooldown, weapon_switch, melee_timer;
    int weapon, weapon_target;
} Player;
typedef struct {
    Player player;
    float shot_cooldown, recoil, hit_time, damage_time, shot_age, combo_timer;
    float respawn_timer, invulnerable;
    int combo;
    uint32_t input_ack, shots;
    bool active, dead, dash_pressed, melee_pressed;
    char name[PLAYER_NAME_LENGTH+1];
} Actor;
typedef enum { ENEMY_RUNNER, ENEMY_CASTER, ENEMY_BRUTE } EnemyType;
typedef struct {
    Body body;
    int id, type;
    float health, flash, shot_timer, touch_timer;
    float max_health, yaw, walk_phase, movement_blend;
    float attack_timer, attack_duration, stagger, spawn_time, death_time;
    Vec3 attack_direction, knockback;
    bool attack_fired;
    int target_actor;
} Enemy;
typedef struct { Vec3 position, velocity; float life; Color color; } Projectile;
typedef struct { Vec3 position, velocity; Color color; float life, max_life, size; } Particle;
typedef struct { Vec3 position; float cooldown; } Pickup;
typedef struct {
    World *world;
    Player player;
    Enemy enemies[MAX_ENEMIES]; int enemy_count;
    Enemy corpses[MAX_CORPSES]; int corpse_count;
    Projectile projectiles[MAX_PROJECTILES]; int projectile_count;
    Particle particles[MAX_PARTICLES]; int particle_count, particle_limit;
    Pickup pickups[MAX_PICKUPS]; int pickup_count;
    int wave, score, kills, next_id;
    float time, wave_timer, nav_timer, shot_cooldown, recoil, hit_time, damage_time;
    float shot_age, footstep_timer, combo_timer;
    int combo;
    char message[96]; float message_time;
    bool dead;
    bool motion, dash_pressed, melee_pressed;
    Actor actors[MAX_PLAYERS];
    int local_actor, current_actor;
    uint32_t tick;
    bool cooperative, round_over, prediction;
} Game;
typedef struct { float forward, right; bool jump, sprint, firing, dash, melee; } Input;
typedef struct { int width, height, fps, particles; const char *name; } Preset;
typedef struct {
    SDL_Window *window; SDL_GLContext context;
    GLuint program, composite_program;
    GLint position, color, normal, matrix, fog, unlit;
    GLint composite_position, composite_uv, composite_texture;
    GLuint world_buffer, dynamic_buffer, gun_buffer, ui_buffer, quad_buffer;
    GLuint framebuffer, texture, depth;
    Mesh dynamic, gun, ui;
    int world_vertices, width, height;
} Renderer;
typedef enum { STATE_MENU, STATE_PLAYING, STATE_PAUSED, STATE_DEAD, STATE_NETWORK, STATE_LOBBY } State;
struct Network;
typedef struct {
    State state;
    bool running, fullscreen, sound, capture_error, motion;
    int selected, quality, best_score, fps;
    float sensitivity;
    struct Network *network;
    int editing;
    bool select_all;
} App;
typedef enum { SOUND_SHOT, SOUND_SHOTGUN, SOUND_HIT, SOUND_KILL,
    SOUND_HURT, SOUND_JUMP, SOUND_PICKUP, SOUND_WAVE,
    SOUND_STEP, SOUND_LAND, SOUND_SWITCH, SOUND_PUMP, SOUND_MELEE,
    SOUND_DASH, SOUND_CAST, SOUND_GROWL } Sound;

extern const Preset PRESETS[3];

/* math.c */
float clampf(float v, float lo, float hi);
Vec3 vadd(Vec3 a, Vec3 b);
Vec3 vsub(Vec3 a, Vec3 b);
Vec3 vscale(Vec3 v, float scale);
float vlength(Vec3 v);
Vec3 vnormalize(Vec3 v);
Vec3 look_direction(float yaw, float pitch);
float ray_box(Vec3 origin, Vec3 direction, Box box, float range);
Mat4 perspective(float fov, float aspect, float near_plane, float far_plane);
Mat4 view_matrix(Vec3 eye, float yaw, float pitch);
Mat4 mat_multiply(Mat4 a, Mat4 b);

/* mesh.c */
bool mesh_init(Mesh *mesh, int capacity);
void mesh_free(Mesh *mesh);
void mesh_triangle(Mesh *mesh, Vec3 a, Vec3 b, Vec3 c, Color color, Vec3 normal);
void mesh_quad(Mesh *mesh, Vec3 a, Vec3 b, Vec3 c, Vec3 d, Color color, Vec3 normal);
void mesh_box(Mesh *mesh, Vec3 base, Vec3 size, Color color);
void mesh_diamond(Mesh *mesh, Vec3 center, float radius, Color color);
void mesh_joint_box(Mesh *mesh, Vec3 base, Vec3 size, Vec3 pivot, float angle, Color color);
void mesh_pose(Mesh *mesh, int first, Vec3 position, float yaw, float pitch, float scale);
void mesh_rect(Mesh *mesh, float x, float y, float w, float h, Color color);
void mesh_text(Mesh *mesh, float x, float y, int scale, Color color, const char *text);

/* world.c */
bool world_init(World *world);
bool world_blocked(const World *world, Vec3 position, float radius, float height, int ignore);
float world_ray(const World *world, Vec3 origin, Vec3 direction, float range);
void navigation_update(World *world, float x, float z);
Vec3 navigation_next(const World *world, float x, float z);

/* physics.c */
void move_horizontal(const World *world, Body *body, float dx, float dz,
    float radius, float height, float step_height);
void move_vertical(const World *world, Body *body, float dt, float radius, float height);
void update_player(World *world, Player *player, Input input, float dt);
Vec3 player_eye(const Player *player, bool motion);

/* game.c */
void game_reset(Game *game, World *world, int particle_limit);
void game_update(Game *game, Input input, float dt);
void game_shoot(Game *game);
void game_burst(Game *game, Vec3 position, Color color, int count);
void game_request_weapon(Game *game, int weapon);
void game_melee(Game *game);
void game_actor_save(Game *game);
void game_actor_select(Game *game, int id);
void game_start_coop(Game *game, const bool active[MAX_PLAYERS], int local_actor);
void game_update_coop(Game *game, const Input inputs[MAX_PLAYERS], float dt);
void game_predict(Game *game, Input input, float dt);

/* audio.c */
void audio_init(void);
void audio_enabled(bool enabled);
void audio_suppressed(bool suppressed);
void audio_play(Sound sound);
void audio_listener(Vec3 position, float yaw);
void audio_play_at(Sound sound, Vec3 position);
void audio_shutdown(void);

/* renderer.c */
bool renderer_init(Renderer *renderer, World *world, const Preset *preset);
bool renderer_resize(Renderer *renderer, const Preset *preset);
void renderer_draw(Renderer *renderer, const Game *game, const App *app);
void renderer_shutdown(Renderer *renderer);
bool renderer_screenshot(Renderer *renderer, const char *path);

/* animation.c : poses visuelles dérivées de l'état du combat. */
void animate_creature(Mesh *mesh, const Enemy *enemy, float time);
void animate_weapon(Mesh *mesh, const Game *game, const App *app);
void animate_teammate(Mesh *mesh, const Actor *actor, int id);

/* ui.c */
void ui_build(Mesh *mesh, const Game *game, const App *app);

#endif
