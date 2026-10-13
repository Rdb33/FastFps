#ifndef FASTFPS_CONFIG_H
#define FASTFPS_CONFIG_H

/* Réglages de la première base : gameplay proche d'un Doom moderne. */
#define GAME_VERSION "0.6.0"
#define PRESET_COUNT 4
#define MAX_PLAYERS 4
#define PLAYER_NAME_LENGTH 16
#define FIXED_STEP (1.0f / 60.0f)
#define MAX_CATCHUP_STEPS 6
#define MAX_ENEMIES 12
#define MAX_PROJECTILES 72
#define MAX_PARTICLES 160
#define MAX_CORPSES 8
#define MAX_SOLIDS 64
#define MAX_PICKUPS 8
#define MAX_BARRELS 6
#define UPGRADE_COUNT 6
#define UPGRADE_MAX_LEVEL 5
#define UPGRADE_CHOICES 3
#define INTERMISSION_TIME 15.0f
#define NAV_W 28
#define NAV_H 24
#define PLAYER_RADIUS 0.38f
#define PLAYER_HEIGHT 1.70f
#define PLAYER_EYE 1.48f
#define PLAYER_SPEED 9.0f
#define PLAYER_SPRINT 13.0f
#define PLAYER_ACCELERATION 70.0f
#define PLAYER_GRAVITY 25.0f
#define PLAYER_JUMP 8.0f
#define PLAYER_STEP 0.34f
#define DASH_SPEED 22.0f
#define DASH_DURATION 0.14f
#define DASH_COOLDOWN 1.25f
#define WEAPON_SWITCH_TIME 0.32f
#define MELEE_COOLDOWN 0.8f
#define MELEE_RANGE 2.5f
#define ENEMY_RADIUS 0.55f
#define ENEMY_HEIGHT 1.65f
#define SHOT_RANGE 85.0f
#define PI_F 3.14159265358979323846f

#endif
