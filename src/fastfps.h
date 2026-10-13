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
typedef struct { Vec3 position; Color color; Vec3 normal; float glow; } Vertex;
typedef struct { Vertex *data; int count, capacity; float glow; } Mesh;
typedef struct { Vec3 position, velocity; bool grounded; } Body;
typedef enum { UPGRADE_POWER, UPGRADE_HASTE, UPGRADE_MOBILITY,
    UPGRADE_VITALITY, UPGRADE_ARMOR, UPGRADE_VAMPIRE } Upgrade;
typedef struct {
    Mesh mesh;
    int map_id;
    uint32_t revision;
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
    uint8_t ammo[WEAPON_COUNT];
    float reload_timer, reload_duration, upgrade_lock;
    uint8_t upgrades[UPGRADE_COUNT], offers[UPGRADE_CHOICES];
    bool upgrade_pending;
    uint8_t last_upgrade;
    float upgrade_flash;
} Player;
typedef struct {
    Player player;
    float shot_cooldown, recoil, hit_time, damage_time, shot_age, combo_timer;
    float respawn_timer, invulnerable;
    float headshot_time, kill_time, damage_yaw;
    int combo;
    int run_kills, run_headshots, reward_points;
    float reward_time;
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
    bool elite;
} Enemy;
typedef struct { Vec3 position, velocity; float life; Color color; } Projectile;
typedef struct { Vec3 position, velocity; Color color; float life, max_life, size; } Particle;
typedef struct { Vec3 position; float cooldown; } Pickup;
typedef struct { Vec3 position; float health, explosion_time; int owner; } Barrel;
typedef struct {
    World *world;
    Player player;
    Enemy enemies[MAX_ENEMIES]; int enemy_count;
    Enemy corpses[MAX_CORPSES]; int corpse_count;
    Projectile projectiles[MAX_PROJECTILES]; int projectile_count;
    Particle particles[MAX_PARTICLES]; int particle_count, particle_limit;
    Pickup pickups[MAX_PICKUPS]; int pickup_count;
    Barrel barrels[MAX_BARRELS]; int barrel_count;
    int wave, score, kills, next_id, difficulty;
    int completed_waves, map_id, lives;
    float time, wave_timer, nav_timer, shot_cooldown, recoil, hit_time, damage_time;
    float shot_age, footstep_timer, combo_timer;
    float headshot_time, kill_time, damage_yaw;
    int combo;
    int reward_points;
    float reward_time, wave_clear_time;
    char message[96]; float message_time;
    bool dead;
    bool motion, dash_pressed, melee_pressed;
    Actor actors[MAX_PLAYERS];
    int local_actor, current_actor;
    uint32_t tick;
    bool cooperative, round_over, prediction;
} Game;
typedef struct { float forward, right; bool jump, sprint, firing, dash, melee, reload; int upgrade; } Input;
typedef struct { const char *name; int magazine, unlock_wave; float interval, reload, damage; Color color; } Weapon;
extern const Weapon WEAPONS[WEAPON_COUNT];
typedef struct { int width, height, fps, particles; const char *name; } Preset;
typedef struct {
    SDL_Window *window; SDL_GLContext context;
    GLuint program, composite_program;
    GLint position, color, normal, matrix, fog, unlit;
    GLint light_position, light_color, sector_color;
    GLint light_position2, light_color2, glow, eye, detail, surface;
    GLint composite_position, composite_uv, composite_texture;
    GLint composite_texel, composite_effects;
    GLint composite_brightness;
    float brightness;
    uint32_t world_revision;
    GLuint world_buffer, dynamic_buffer, gun_buffer, ui_buffer, quad_buffer;
    GLuint framebuffer, texture, depth;
    Mesh dynamic, gun, ui;
    int world_vertices, width, height;
    int detail_level;
    bool modern;
} Renderer;
typedef enum { STATE_MENU, STATE_PLAYING, STATE_PAUSED, STATE_DEAD, STATE_NETWORK, STATE_LOBBY, STATE_UPGRADE } State;
struct Network;
typedef struct {
    State state;
    bool running, fullscreen, sound, capture_error, motion, modern;
    int selected, quality, best_score, fps, difficulty;
    float sensitivity, brightness;
    int numeric_edit, slider_drag;
    char numeric_text[16];
    Uint32 upgrade_ready_at;
    bool upgrade_released, upgrade_confirm;
    struct Network *network;
    int editing;
    bool select_all, network_help, show_build;
    int upgrade_choice;
    float ui_time;
} App;
typedef enum { SOUND_SHOT, SOUND_SHOTGUN, SOUND_HIT, SOUND_KILL,
    SOUND_HURT, SOUND_JUMP, SOUND_PICKUP, SOUND_WAVE,
    SOUND_STEP, SOUND_LAND, SOUND_SWITCH, SOUND_PUMP, SOUND_MELEE,
    SOUND_DASH, SOUND_CAST, SOUND_GROWL, SOUND_EXPLOSION, SOUND_HEADSHOT,
    SOUND_UPGRADE, SOUND_CHAMPION } Sound;

extern const Preset PRESETS[PRESET_COUNT];

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
void mesh_beveled_box(Mesh *mesh, Vec3 base, Vec3 size, float bevel, Color color);
void mesh_cylinder(Mesh *mesh, Vec3 start, Vec3 end, float radius, Color color, int sides);
void mesh_diamond(Mesh *mesh, Vec3 center, float radius, Color color);
void mesh_joint_box(Mesh *mesh, Vec3 base, Vec3 size, Vec3 pivot, float angle, Color color);
void mesh_pose(Mesh *mesh, int first, Vec3 position, float yaw, float pitch, float scale);
void mesh_rect(Mesh *mesh, float x, float y, float w, float h, Color color);
void mesh_text(Mesh *mesh, float x, float y, int scale, Color color, const char *text);
void mesh_ring(Mesh *mesh, Vec3 center, float radius, float width, Color color, int segments);

/* world.c */
bool world_init(World *world);
void world_set_map(World *world, int id);
const char *world_map_name(int id);
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
void game_reload(Game *game);
int game_unlocked_weapons(const Game *game);
void game_cycle_weapon(Game *game, int direction);
void game_melee(Game *game);
void game_actor_save(Game *game);
void game_actor_select(Game *game, int id);
void game_start_coop(Game *game, const bool active[MAX_PLAYERS], int local_actor);
void game_update_coop(Game *game, const Input inputs[MAX_PLAYERS], float dt);
void game_predict(Game *game, Input input, float dt);

/* progression.c : améliorations remises à zéro à chaque partie. */
float player_max_health(const Player *player);
float player_damage_multiplier(const Player *player);
float player_fire_interval(const Player *player);
float player_dash_cooldown(const Player *player);
int player_level(const Player *player);
float enemy_size(const Enemy *enemy);
void game_offer_upgrades(Game *game);
bool game_choose_upgrade(Game *game, int choice);
const char *upgrade_name(int upgrade);
const char *upgrade_description(int upgrade);
Color upgrade_color(int upgrade);
void upgrade_sync(App *app, const Game *game, bool demo);
bool upgrade_event(App *app, const Game *game, Renderer *renderer, const SDL_Event *event);
void upgrade_ui(Mesh *mesh, const Game *game, const App *app);

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
void animate_barrel(Mesh *mesh, const Barrel *barrel, float time);
void animate_arena(Mesh *mesh, const Game *game, const App *app);
Color sector_theme(int wave);
int graphics_detail(int quality);

/* ui.c */
void ui_build(Mesh *mesh, const Game *game, const App *app);
bool settings_event(App *app, Renderer *renderer, const SDL_Event *event);
void settings_controls(Mesh *mesh, const App *app, int row, float y);

#endif
