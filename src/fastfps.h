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
typedef struct { Vec3 base, size, travel; float period; int solid; Vec3 arc; float dwell, offset; } Platform;
typedef struct { Vec3 base, size, pickup; float yaw, gate; } Deck;
typedef struct { Vec3 position; float time, radius; } Blast;
typedef struct { Vec3 target; float height, time; int source, target_actor; } Bomb;
typedef struct { float m[16]; } Mat4;
typedef struct { Vec3 position; Color color; Vec3 normal; float glow; } Vertex;
typedef struct { Vertex *data; int count, capacity; float glow; } Mesh;
typedef struct { Vec3 position, velocity; bool grounded; } Body;
typedef enum { UPGRADE_POWER, UPGRADE_HASTE, UPGRADE_MOBILITY,
    UPGRADE_VITALITY, UPGRADE_ARMOR, UPGRADE_VAMPIRE,
    UPGRADE_PRECISION, UPGRADE_DASH_POWER, UPGRADE_SWITCH_POWER, UPGRADE_PIERCE } Upgrade;
typedef enum { CHALLENGE_NONE, CHALLENGE_SURVIVE, CHALLENGE_INTERRUPT, CHALLENGE_CHAMPION } Challenge;
typedef enum { BONUS_SHIELD, BONUS_POWER, BONUS_SPEED } Bonus;
typedef struct { Vec3 position; float cooldown; int kind; } Supply;
/* État d'IA privé à l'autorité, jamais encodé dans les paquets. */
#define AI_NAV_NODES 3072
#define AI_NAV_EDGES 24
#define AI_PATH_LENGTH 64
#define AI_HISTORY 16
typedef enum { AI_PRESSURE, AI_FLANK, AI_SUPPORT, AI_COVER, AI_SEARCH } AiRole;
typedef struct { int16_t to; uint8_t mask; int8_t lift; float cost; } AiEdge;
typedef struct {
    Vec3 position;
    int16_t next;
    int8_t platform;
    uint8_t mask, count;
    AiEdge edges[AI_NAV_EDGES];
} AiNode;
typedef struct {
    Vec3 position, velocity, acceleration, history[AI_HISTORY];
    float seen_at, sample_at, confidence, reversals, strafe_bias, jump_bias;
    int samples, cursor;
    bool known;
} AiContact;
typedef struct {
    int id, target, role, path_count, path_cursor;
    int16_t path[AI_PATH_LENGTH];
    Vec3 goal, destination, last_position;
    float decide_at, plan_at, progress_at;
    uint32_t revision;
    bool used, riding;
} AiBrain;
typedef struct {
    AiNode nodes[AI_NAV_NODES];int node_count;
    int16_t cells[NAV_W*NAV_H];
    AiContact contacts[MAX_PLAYERS];AiBrain brains[MAX_ENEMIES];
    float costs[AI_NAV_NODES];int16_t parents[AI_NAV_NODES],heap[AI_NAV_NODES],heap_pos[AI_NAV_NODES];
    uint8_t closed[AI_NAV_NODES];
    float next_observation;
    uint32_t revision, searches, expansions;
    bool initialized;
} AiState;
typedef struct {
    Mesh mesh;
    int map_id;
    uint32_t revision;
    Box solids[MAX_SOLIDS]; int solid_count;
    Vec3 spawns[8];
    uint8_t passable[NAV_W * NAV_H];
    int16_t distance[NAV_W * NAV_H];
    int16_t routes[MAX_PLAYERS][NAV_W * NAV_H];
    Platform platforms[MAX_PLATFORMS];
    Deck decks[MAX_DECKS];
    int platform_count;
    float platform_time;
    AiState ai;
} World;
typedef struct {
    Body body;
    float yaw, pitch, health, bob, pad_cooldown;
    float movement_blend, strafe_blend, landing, sway_x, sway_y, step_timer;
    float dash_timer, dash_cooldown, weapon_switch, melee_timer;
    int weapon, weapon_target;
    uint8_t ammo[WEAPON_COUNT];
    float reload_timer, reload_duration, upgrade_lock;
    float boost_timer;
    float jump_buffer, jump_grace;
    float shield, shield_time, speed_time, power_time, reactor_time;
    float dash_power_time, switch_power_time;
    float web_timer, web_immunity;
    uint8_t previous_weapon;
    uint8_t upgrades[UPGRADE_COUNT], offers[UPGRADE_CHOICES];
    bool upgrade_pending;
    uint8_t last_upgrade;
    float upgrade_flash;
} Player;
typedef struct {
    Player player;
    float shot_cooldown, recoil, hit_time, damage_time, shot_age, combo_timer;
    float respawn_timer, invulnerable;
    float revive_progress;
    bool interact_held;
    float heavy_cooldown;
    float damage_angles[DAMAGE_DIRECTIONS], damage_timers[DAMAGE_DIRECTIONS];
    bool revive_held;
    int revive_target;
    float headshot_time, kill_time, damage_yaw;
    int combo;
    int run_kills, run_headshots, reward_points;
    float reward_time;
    uint32_t input_ack, shots;
    bool active, dead, dash_pressed, melee_pressed;
    char name[PLAYER_NAME_LENGTH+1];
} Actor;
typedef enum { ENEMY_RUNNER, ENEMY_CASTER, ENEMY_BRUTE, ENEMY_BOMBER, ENEMY_WEAVER } EnemyType;
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
    int slam_state;
    float slam_timer, slam_flash, stuck_time;
    Vec3 slam_target;
    float climb_timer;
    Vec3 climb_start, climb_target;
    int climb_solid;
} Enemy;
typedef struct { Vec3 position, velocity; float life; Color color; int kind, owner; float damage; } Projectile;
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
    Blast blasts[MAX_BLASTS]; int blast_count;
    Bomb bombs[MAX_BOMBS]; int bomb_count;
    Supply supplies[MAX_DECKS];
    int wave_total, wave_spawned, reinforcements;
    float reinforce_timer;
    int reactor_charge;
    float reactor_cooldown;
    int challenge, challenge_state, challenge_progress;
    float challenge_deadline, challenge_notice;
    bool rocket_event;
    int event_last_wave, unlock_weapon;
    uint32_t event_rng, event_serial;
    float unlock_time, event_notice_time;
    float intro_time;
    int intro_enemy, intro_last_wave;
    uint32_t intro_serial;
    uint8_t intro_skip_ready; /* Front de touche, uniquement sur l'autorité. */
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
typedef struct { float forward, right; bool jump, sprint, firing, dash, melee, reload, revive; int upgrade; } Input;
typedef struct { const char *name; int magazine, unlock_wave; float interval, reload, damage; Color color; } Weapon;
extern const Weapon WEAPONS[WEAPON_COUNT];
typedef struct { float progress, lower, extract, seat, rack; bool active; } ReloadPose;
typedef struct { int width, height, fps, particles; const char *name; } Preset;
struct Feedback;
typedef struct {
    SDL_Window *window; SDL_GLContext context;
    GLuint program, composite_program;
    GLint position, color, normal, matrix, fog, unlit;
    GLint light_position, light_color, sector_color, fog_color;
    GLint light_position2, light_color2, glow, eye, detail, surface;
    GLint composite_position, composite_uv, composite_texture;
    GLint composite_texel, composite_effects;
    GLint composite_brightness, composite_impact, composite_impact_phase, composite_impact_focus, pulse;
    struct Feedback *feedback;
    float brightness;
    uint32_t world_revision;
    GLuint world_buffer, dynamic_buffer, gun_buffer, ui_buffer, quad_buffer;
    GLuint framebuffer, texture, depth;
    Mesh dynamic, gun, ui;
    int world_vertices, width, height;
    int detail_level;
    bool modern;
    float pose_time, down_blend[MAX_PLAYERS];
    uint32_t pose_tick;
} Renderer;
typedef enum { STATE_MENU, STATE_PLAYING, STATE_PAUSED, STATE_DEAD, STATE_NETWORK, STATE_LOBBY, STATE_UPGRADE } State;
typedef enum { CONFIRM_NONE, CONFIRM_SOLO, CONFIRM_COOP } GameConfirmation;
struct Network;
struct UpdateChecker;
typedef struct {
    State state;
    bool running, fullscreen, sound, capture_error, motion, modern, menu_motion;
    int selected, quality, best_score, fps, difficulty;
    float sensitivity, brightness, music_volume, impact_intensity;
    int numeric_edit, slider_drag;
    char numeric_text[16];
    Uint32 upgrade_ready_at;
    bool upgrade_released, upgrade_confirm;
    struct Network *network;
    struct UpdateChecker *updates;
    int editing;
    bool select_all, network_help, show_build, music_notice_shown;
    int upgrade_choice;
    int menu_page;
    float ui_time, music_notice_time, menu_age, menu_highlight;
    float menu_pointer_x, menu_pointer_y, menu_parallax_x, menu_parallax_y;
    float music_bands[MUSIC_VISUAL_BANDS];
    float music_peaks[MUSIC_VISUAL_BANDS];
    int menu_visual_page;
    State menu_visual_state;
    GameConfirmation game_confirmation;
    State confirmation_state;
    bool confirmation_yes, confirmation_released;
    Uint32 confirmation_ready_at;
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
bool enemy_try_climb(Game *game, Enemy *enemy, Vec3 direction);
bool enemy_update_climb(Game *game, Enemy *enemy, float dt);
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
void mesh_rotate(Mesh *mesh, int first, Vec3 pivot, float pitch, float yaw, float roll);
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
void navigation_actor(World *world, int actor, Vec3 target);
Vec3 navigation_for_actor(const World *world, int actor, float x, float z);
void world_platforms_update(World *world, float time);
Box platform_box(const Platform *platform, float time);
Vec3 platform_position(const Platform *platform, float time);
Vec3 platform_velocity(const Platform *platform, float time);
void world_routes(World *world);
const char *world_route_name(int map);
void game_prepare_mobility(Game *game, float dt);
void ai_reset(World *world);
void ai_update(Game *game, float dt);
AiBrain *ai_brain(Game *game, const Enemy *enemy);
void ai_nav_build(World *world);
int ai_profile(const Enemy *enemy);
bool ai_nav_segment(const World *world, Vec3 from, Vec3 to, int profile);
bool ai_plan(Game *game, Enemy *enemy, AiBrain *brain, Vec3 goal);
Vec3 ai_waypoint(Game *game, Enemy *enemy, AiBrain *brain);
Vec3 ai_predict(const Game *game, const Enemy *enemy, const Player *player, float projectile_speed);
int ai_target(Game *game, const Enemy *enemy);
bool ai_can_fire(const Game *game, const Enemy *enemy, Vec3 aim);
Vec3 ai_move(Game *game, Enemy *enemy, const Player *target, bool visible, float dt);

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
bool game_weapon_available(const Game *game, int weapon);
void game_begin_rocket_event(Game *game);
void game_end_rocket_event(Game *game);
void game_roll_event(Game *game);
int game_revive_target(const Game *game, int actor);
void game_cycle_weapon(Game *game, int direction);
int game_melee_target(const Game *game);
void game_melee(Game *game);
void game_actor_save(Game *game);
void game_actor_select(Game *game, int id);
void game_start_coop(Game *game, const bool active[MAX_PLAYERS], int local_actor);
void game_update_coop(Game *game, const Input inputs[MAX_PLAYERS], float dt);
void game_predict(Game *game, Input input, float dt);

/* encounters.c : objectifs et bonus décidés par l'hôte. */
void encounters_map(Game *game);
void encounters_start(Game *game);
void encounters_update(Game *game, float dt);
void encounters_finish(Game *game);
void encounters_interrupt(Game *game);
void encounters_kill(Game *game, bool champion);
void encounters_down(Game *game);
void encounters_actor(Game *game, float dt);
void encounters_reactor(Game *game);
bool reactor_in_range(const Player *player);
bool game_heavy_available(const Game *game, int target);
float game_threat(const Game *game);
const char *challenge_name(int challenge);
Color supply_color(int kind);
void encounters_draw(Mesh *mesh, const Game *game);
void encounters_ui(Mesh *mesh, const Game *game, const App *app);

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
void audio_music(float volume, bool gameplay, float focus);
void audio_music_levels(float levels[MUSIC_VISUAL_BANDS]);
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
Color teammate_color(int id);
void animate_teammate(Mesh *mesh, const Actor *actor, int id, float time, int detail, float down_blend, float effects);
void animate_weapon_effects(Mesh *mesh, const Player *player, Vec3 origin, float muzzle,
    float time, int detail, float intensity);
ReloadPose weapon_reload_pose(const Player *player);
void animate_barrel(Mesh *mesh, const Barrel *barrel, float time);
void animate_arena(Mesh *mesh, const Game *game, const App *app);
Color sector_theme(int wave);
int graphics_detail(int quality);

/* ui.c */
void ui_build(Mesh *mesh, const Game *game, const App *app);
void ui_teammates(Mesh *mesh, const Game *game, const App *app, Mat4 view_projection);
void settings_read(App *app);
void settings_write(const App *app);
bool settings_event(App *app, Renderer *renderer, const SDL_Event *event);
void settings_controls(Mesh *mesh, const App *app, int row, float y);
int menu_count(const App *app);
int menu_row_id(const App *app, int row);
void menu_move(App *app, int direction);
void menu_animate(App *app, float dt);
void menu_pointer_update(App *app, const Renderer *renderer);
bool menu_visible(const App *app);
Vec3 menu_offset(const App *app, float depth);
void menu_layer(Mesh *mesh, int first, float depth);
void menu_parallax_apply(Mesh *mesh, const App *app);
bool menu_coordinates(const App *app, const Renderer *renderer, int x, int y, float *px, float *py);
void ui_music_visualizer(Mesh *mesh, const App *app, float x, float baseline, float width, float height);
int menu_hit(const App *app, Renderer *renderer, int x, int y);
void menu_build(Mesh *mesh, const Game *game, const App *app);
void menu_confirm_open(App *app, GameConfirmation kind);
bool menu_confirm_event(App *app, Renderer *renderer, const SDL_Event *event, GameConfirmation *accepted);
void menu_confirm_build(Mesh *mesh, const App *app);

#endif
