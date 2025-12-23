#include "../freecs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <raylib.h>

#define GRID_SIZE 12
#define TILE_SIZE 40.0f
#define BASE_WIDTH 1024.0f
#define BASE_HEIGHT 768.0f
#define MAX_PATH_POINTS 32
#define MAX_ENEMIES_TO_SPAWN 128

typedef enum {
    TOWER_BASIC,
    TOWER_FROST,
    TOWER_CANNON,
    TOWER_SNIPER,
    TOWER_POISON
} TowerType;

typedef enum {
    GAME_WAITING_FOR_WAVE,
    GAME_WAVE_IN_PROGRESS,
    GAME_OVER,
    GAME_VICTORY,
    GAME_PAUSED
} GameState;

typedef enum {
    ENEMY_NORMAL,
    ENEMY_FAST,
    ENEMY_TANK,
    ENEMY_FLYING,
    ENEMY_SHIELDED,
    ENEMY_HEALER,
    ENEMY_BOSS
} EnemyType;

typedef enum {
    EFFECT_EXPLOSION,
    EFFECT_POISON_BUBBLE,
    EFFECT_DEATH_PARTICLE
} EffectType;

typedef struct {
    float x;
    float y;
} Position;

typedef struct {
    float x;
    float y;
} Velocity;

typedef struct {
    TowerType tower_type;
    uint32_t level;
    float cooldown;
    freecs_entity_t target;
    bool has_target;
    float fire_animation;
    float tracking_time;
} Tower;

typedef struct {
    float health;
    float max_health;
    float shield_health;
    float max_shield;
    float speed;
    size_t path_index;
    float path_progress;
    uint32_t value;
    EnemyType enemy_type;
    float slow_duration;
    float poison_duration;
    float poison_damage;
    bool is_flying;
} Enemy;

typedef struct {
    float damage;
    freecs_entity_t target;
    float speed;
    TowerType tower_type;
    float start_x;
    float start_y;
    float arc_height;
    float flight_progress;
} Projectile;

typedef struct {
    int x;
    int y;
    bool occupied;
    bool is_path;
} GridCell;

typedef struct {
    int x;
    int y;
} GridPosition;

typedef struct {
    EffectType effect_type;
    float lifetime;
    float age;
    float velocity_x;
    float velocity_y;
} VisualEffect;

typedef struct {
    float lifetime;
    int amount;
} MoneyPopup;

typedef struct {
    EnemyType enemy_type;
    float spawn_time;
} EnemySpawnInfo;

typedef struct {
    uint32_t money;
    uint32_t lives;
    uint32_t wave;
    GameState game_state;
    TowerType selected_tower_type;
    float spawn_timer;
    EnemySpawnInfo enemies_to_spawn[MAX_ENEMIES_TO_SPAWN];
    size_t enemies_to_spawn_count;
    int mouse_grid_x;
    int mouse_grid_y;
    bool mouse_on_grid;
    float path[MAX_PATH_POINTS][2];
    size_t path_count;
    float wave_announce_timer;
    float game_speed;
    uint32_t current_hp;
    uint32_t max_hp;
} GameResources;

static freecs_world_t world;
static GameResources resources;

static uint64_t BIT_POSITION;
static uint64_t BIT_VELOCITY;
static uint64_t BIT_TOWER;
static uint64_t BIT_ENEMY;
static uint64_t BIT_PROJECTILE;
static uint64_t BIT_GRID_CELL;
static uint64_t BIT_GRID_POSITION;
static uint64_t BIT_VISUAL_EFFECT;
static uint64_t BIT_MONEY_POPUP;

static freecs_event_queue_t enemy_died_events;
static freecs_event_queue_t enemy_spawned_events;

typedef struct {
    freecs_entity_t entity;
    float pos_x;
    float pos_y;
    uint32_t reward;
    EnemyType enemy_type;
} EnemyDiedEvent;

typedef struct {
    freecs_entity_t entity;
    EnemyType enemy_type;
} EnemySpawnedEvent;

static float random_float(void) {
    return (float)rand() / (float)RAND_MAX;
}

static float random_range(float min_val, float max_val) {
    return min_val + (max_val - min_val) * random_float();
}

static uint32_t tower_cost(TowerType tower_type) {
    switch (tower_type) {
        case TOWER_BASIC: return 60;
        case TOWER_FROST: return 120;
        case TOWER_CANNON: return 200;
        case TOWER_SNIPER: return 180;
        case TOWER_POISON: return 150;
        default: return 0;
    }
}

static uint32_t tower_upgrade_cost(TowerType tower_type, uint32_t current_level) {
    return (uint32_t)((float)tower_cost(tower_type) * 0.5f * (float)current_level);
}

static float tower_damage(TowerType tower_type, uint32_t level) {
    float base;
    switch (tower_type) {
        case TOWER_BASIC: base = 15.0f; break;
        case TOWER_FROST: base = 8.0f; break;
        case TOWER_CANNON: base = 50.0f; break;
        case TOWER_SNIPER: base = 80.0f; break;
        case TOWER_POISON: base = 5.0f; break;
        default: base = 10.0f;
    }
    return base * (1.0f + 0.25f * (float)(level - 1));
}

static float tower_range(TowerType tower_type, uint32_t level) {
    float base;
    switch (tower_type) {
        case TOWER_BASIC: base = 100.0f; break;
        case TOWER_FROST: base = 80.0f; break;
        case TOWER_CANNON: base = 120.0f; break;
        case TOWER_SNIPER: base = 180.0f; break;
        case TOWER_POISON: base = 90.0f; break;
        default: base = 100.0f;
    }
    return base * (1.0f + 0.15f * (float)(level - 1));
}

static float tower_fire_rate(TowerType tower_type, uint32_t level) {
    float base;
    switch (tower_type) {
        case TOWER_BASIC: base = 0.5f; break;
        case TOWER_FROST: base = 1.0f; break;
        case TOWER_CANNON: base = 2.0f; break;
        case TOWER_SNIPER: base = 3.0f; break;
        case TOWER_POISON: base = 0.8f; break;
        default: base = 0.5f;
    }
    float rate = base * (1.0f - 0.1f * (float)(level - 1));
    return rate < 0.2f ? 0.2f : rate;
}

static Color tower_color(TowerType tower_type) {
    switch (tower_type) {
        case TOWER_BASIC: return GREEN;
        case TOWER_FROST: return (Color){51, 153, 255, 255};
        case TOWER_CANNON: return RED;
        case TOWER_SNIPER: return DARKGRAY;
        case TOWER_POISON: return (Color){153, 51, 204, 255};
        default: return WHITE;
    }
}

static float tower_projectile_speed(TowerType tower_type) {
    switch (tower_type) {
        case TOWER_BASIC: return 300.0f;
        case TOWER_FROST: return 200.0f;
        case TOWER_CANNON: return 250.0f;
        case TOWER_SNIPER: return 500.0f;
        case TOWER_POISON: return 250.0f;
        default: return 300.0f;
    }
}

static float enemy_base_health(EnemyType enemy_type) {
    switch (enemy_type) {
        case ENEMY_NORMAL: return 50.0f;
        case ENEMY_FAST: return 30.0f;
        case ENEMY_TANK: return 150.0f;
        case ENEMY_FLYING: return 40.0f;
        case ENEMY_SHIELDED: return 80.0f;
        case ENEMY_HEALER: return 60.0f;
        case ENEMY_BOSS: return 500.0f;
        default: return 50.0f;
    }
}

static float enemy_health(EnemyType enemy_type, uint32_t wave) {
    float health_multiplier = 1.0f + ((float)wave - 1.0f) * 0.5f;
    return enemy_base_health(enemy_type) * health_multiplier;
}

static float enemy_speed(EnemyType enemy_type) {
    switch (enemy_type) {
        case ENEMY_NORMAL: return 40.0f;
        case ENEMY_FAST: return 80.0f;
        case ENEMY_TANK: return 20.0f;
        case ENEMY_FLYING: return 60.0f;
        case ENEMY_SHIELDED: return 30.0f;
        case ENEMY_HEALER: return 35.0f;
        case ENEMY_BOSS: return 15.0f;
        default: return 40.0f;
    }
}

static uint32_t enemy_value(EnemyType enemy_type, uint32_t wave) {
    uint32_t base;
    switch (enemy_type) {
        case ENEMY_NORMAL: base = 10; break;
        case ENEMY_FAST: base = 15; break;
        case ENEMY_TANK: base = 30; break;
        case ENEMY_FLYING: base = 20; break;
        case ENEMY_SHIELDED: base = 25; break;
        case ENEMY_HEALER: base = 40; break;
        case ENEMY_BOSS: base = 100; break;
        default: base = 10;
    }
    return base + wave * 2;
}

static float enemy_shield(EnemyType enemy_type) {
    switch (enemy_type) {
        case ENEMY_SHIELDED: return 50.0f;
        case ENEMY_BOSS: return 100.0f;
        default: return 0.0f;
    }
}

static Color enemy_color(EnemyType enemy_type) {
    switch (enemy_type) {
        case ENEMY_NORMAL: return RED;
        case ENEMY_FAST: return ORANGE;
        case ENEMY_TANK: return DARKGRAY;
        case ENEMY_FLYING: return SKYBLUE;
        case ENEMY_SHIELDED: return (Color){128, 0, 204, 255};
        case ENEMY_HEALER: return (Color){51, 204, 77, 255};
        case ENEMY_BOSS: return (Color){153, 0, 153, 255};
        default: return RED;
    }
}

static float enemy_size(EnemyType enemy_type) {
    switch (enemy_type) {
        case ENEMY_NORMAL: return 15.0f;
        case ENEMY_FAST: return 12.0f;
        case ENEMY_TANK: return 20.0f;
        case ENEMY_FLYING: return 15.0f;
        case ENEMY_SHIELDED: return 18.0f;
        case ENEMY_HEALER: return 16.0f;
        case ENEMY_BOSS: return 30.0f;
        default: return 15.0f;
    }
}

static float get_scale(void) {
    float screen_w = (float)GetScreenWidth();
    float screen_h = (float)GetScreenHeight();
    float scale_w = screen_w / BASE_WIDTH;
    float scale_h = screen_h / BASE_HEIGHT;
    return scale_w < scale_h ? scale_w : scale_h;
}

static void get_offset(float* offset_x, float* offset_y) {
    float scale = get_scale();
    float screen_w = (float)GetScreenWidth();
    float screen_h = (float)GetScreenHeight();
    float scaled_width = BASE_WIDTH * scale;
    float scaled_height = BASE_HEIGHT * scale;
    *offset_x = (screen_w - scaled_width) / 2.0f;
    *offset_y = (screen_h - scaled_height) / 2.0f;
}

static void grid_to_base(int grid_x, int grid_y, float* out_x, float* out_y) {
    float num_cells = (float)(GRID_SIZE + 1);
    float grid_width = num_cells * TILE_SIZE;
    float grid_height = num_cells * TILE_SIZE;
    float grid_offset_x = (BASE_WIDTH - grid_width) / 2.0f;
    float grid_offset_y = (BASE_HEIGHT - grid_height) / 2.0f;

    float tile_x = (float)(grid_x + GRID_SIZE / 2);
    float tile_y = (float)(grid_y + GRID_SIZE / 2);

    *out_x = grid_offset_x + (tile_x + 0.5f) * TILE_SIZE;
    *out_y = grid_offset_y + (tile_y + 0.5f) * TILE_SIZE;
}

static void grid_to_screen(int grid_x, int grid_y, float* out_x, float* out_y) {
    float base_x, base_y;
    grid_to_base(grid_x, grid_y, &base_x, &base_y);
    float scale = get_scale();
    float offset_x, offset_y;
    get_offset(&offset_x, &offset_y);
    *out_x = offset_x + base_x * scale;
    *out_y = offset_y + base_y * scale;
}

static bool screen_to_grid(float screen_x, float screen_y, int* out_grid_x, int* out_grid_y) {
    float scale = get_scale();
    float offset_x, offset_y;
    get_offset(&offset_x, &offset_y);

    float num_cells = (float)(GRID_SIZE + 1);
    float grid_width = num_cells * TILE_SIZE;
    float grid_height = num_cells * TILE_SIZE;
    float grid_offset_x = (BASE_WIDTH - grid_width) / 2.0f;
    float grid_offset_y = (BASE_HEIGHT - grid_height) / 2.0f;

    float local_x = (screen_x - offset_x) / scale;
    float local_y = (screen_y - offset_y) / scale;

    float rel_x = local_x - grid_offset_x;
    float rel_y = local_y - grid_offset_y;

    if (rel_x < 0 || rel_y < 0 || rel_x >= grid_width || rel_y >= grid_height) {
        return false;
    }

    int tile_x = (int)floorf(rel_x / TILE_SIZE);
    int tile_y = (int)floorf(rel_y / TILE_SIZE);

    *out_grid_x = tile_x - GRID_SIZE / 2;
    *out_grid_y = tile_y - GRID_SIZE / 2;
    return true;
}

static void initialize_grid(void) {
    for (int x = -GRID_SIZE / 2; x <= GRID_SIZE / 2; x++) {
        for (int y = -GRID_SIZE / 2; y <= GRID_SIZE / 2; y++) {
            GridCell cell = {x, y, false, false};
            freecs_type_info_entry_t entries[1] = {
                {BIT_GRID_CELL, sizeof(GridCell), &cell, freecs_bit_index(BIT_GRID_CELL)}
            };
            freecs_spawn(&world, BIT_GRID_CELL, entries, 1);
        }
    }
}

static void create_path(void) {
    float path_points[][2] = {
        {-6.0f, 0.0f},
        {-3.0f, 0.0f},
        {-3.0f, -4.0f},
        {3.0f, -4.0f},
        {3.0f, 2.0f},
        {-1.0f, 2.0f},
        {-1.0f, 5.0f},
        {6.0f, 5.0f}
    };
    size_t path_point_count = sizeof(path_points) / sizeof(path_points[0]);

    float num_cells = (float)(GRID_SIZE + 1);
    float grid_width = num_cells * TILE_SIZE;
    float grid_height = num_cells * TILE_SIZE;
    float grid_offset_x = (BASE_WIDTH - grid_width) / 2.0f;
    float grid_offset_y = (BASE_HEIGHT - grid_height) / 2.0f;

    resources.path_count = 0;
    for (size_t i = 0; i < path_point_count && resources.path_count < MAX_PATH_POINTS; i++) {
        float screen_x = grid_offset_x + (path_points[i][0] + (float)(GRID_SIZE / 2) + 0.5f) * TILE_SIZE;
        float screen_y = grid_offset_y + (path_points[i][1] + (float)(GRID_SIZE / 2) + 0.5f) * TILE_SIZE;
        resources.path[resources.path_count][0] = screen_x;
        resources.path[resources.path_count][1] = screen_y;
        resources.path_count++;
    }

    size_t matching_count;
    size_t* matching = freecs_get_matching_archetypes(&world, BIT_GRID_CELL, 0, &matching_count);

    for (size_t i = 0; i < matching_count; i++) {
        freecs_archetype_t* arch = &world.archetypes[matching[i]];
        GridCell* cells = freecs_column_unchecked(arch, BIT_GRID_CELL);

        for (size_t j = 0; j < arch->entities_len; j++) {
            for (size_t seg = 0; seg < path_point_count - 1; seg++) {
                float start_x = path_points[seg][0];
                float start_y = path_points[seg][1];
                float end_x = path_points[seg + 1][0];
                float end_y = path_points[seg + 1][1];

                for (int step = 0; step <= 20; step++) {
                    float t = (float)step / 20.0f;
                    float pos_x = start_x + (end_x - start_x) * t;
                    float pos_y = start_y + (end_y - start_y) * t;
                    int grid_x = (int)roundf(pos_x);
                    int grid_y = (int)roundf(pos_y);

                    if (cells[j].x == grid_x && cells[j].y == grid_y) {
                        cells[j].is_path = true;
                        cells[j].occupied = true;
                    }
                }
            }
        }
    }
}

static freecs_entity_t spawn_tower(int grid_x, int grid_y, TowerType tower_type) {
    float pos_x, pos_y;
    grid_to_base(grid_x, grid_y, &pos_x, &pos_y);

    Position position = {pos_x, pos_y};
    GridPosition grid_pos = {grid_x, grid_y};
    Tower tower = {
        tower_type, 1, 0.0f, FREECS_ENTITY_NIL, false, 0.0f, 0.0f
    };

    freecs_type_info_entry_t entries[3] = {
        {BIT_POSITION, sizeof(Position), &position, freecs_bit_index(BIT_POSITION)},
        {BIT_GRID_POSITION, sizeof(GridPosition), &grid_pos, freecs_bit_index(BIT_GRID_POSITION)},
        {BIT_TOWER, sizeof(Tower), &tower, freecs_bit_index(BIT_TOWER)}
    };

    freecs_entity_t entity = freecs_spawn(&world, BIT_POSITION | BIT_GRID_POSITION | BIT_TOWER, entries, 3);

    uint32_t cost = tower_cost(tower_type);
    resources.money -= cost;

    return entity;
}

static freecs_entity_t spawn_enemy(EnemyType type) {
    float start_x = resources.path[0][0];
    float start_y = resources.path[0][1];
    float hp = enemy_health(type, resources.wave);
    float shield_hp = enemy_shield(type);

    Position position = {start_x, start_y};
    Velocity velocity = {0.0f, 0.0f};
    Enemy enemy = {
        hp, hp, shield_hp, shield_hp,
        enemy_speed(type),
        0, 0.0f,
        enemy_value(type, resources.wave),
        type,
        0.0f, 0.0f, 0.0f,
        type == ENEMY_FLYING
    };

    freecs_type_info_entry_t entries[3] = {
        {BIT_POSITION, sizeof(Position), &position, freecs_bit_index(BIT_POSITION)},
        {BIT_VELOCITY, sizeof(Velocity), &velocity, freecs_bit_index(BIT_VELOCITY)},
        {BIT_ENEMY, sizeof(Enemy), &enemy, freecs_bit_index(BIT_ENEMY)}
    };

    freecs_entity_t entity = freecs_spawn(&world, BIT_POSITION | BIT_VELOCITY | BIT_ENEMY, entries, 3);

    EnemySpawnedEvent event = {entity, type};
    freecs_send_event(&enemy_spawned_events, &event);

    return entity;
}

static freecs_entity_t spawn_projectile(float from_x, float from_y, freecs_entity_t target, TowerType tower_type, uint32_t level) {
    float arc_height = (tower_type == TOWER_CANNON) ? 50.0f : 0.0f;

    Position position = {from_x, from_y};
    Velocity velocity = {0.0f, 0.0f};
    Projectile projectile = {
        tower_damage(tower_type, level),
        target,
        tower_projectile_speed(tower_type),
        tower_type,
        from_x, from_y,
        arc_height, 0.0f
    };

    freecs_type_info_entry_t entries[3] = {
        {BIT_POSITION, sizeof(Position), &position, freecs_bit_index(BIT_POSITION)},
        {BIT_VELOCITY, sizeof(Velocity), &velocity, freecs_bit_index(BIT_VELOCITY)},
        {BIT_PROJECTILE, sizeof(Projectile), &projectile, freecs_bit_index(BIT_PROJECTILE)}
    };

    return freecs_spawn(&world, BIT_POSITION | BIT_VELOCITY | BIT_PROJECTILE, entries, 3);
}

static void spawn_visual_effect(float pos_x, float pos_y, EffectType effect_type, float velocity_x, float velocity_y, float lifetime) {
    Position position = {pos_x, pos_y};
    VisualEffect effect = {effect_type, lifetime, 0.0f, velocity_x, velocity_y};

    freecs_type_info_entry_t entries[2] = {
        {BIT_POSITION, sizeof(Position), &position, freecs_bit_index(BIT_POSITION)},
        {BIT_VISUAL_EFFECT, sizeof(VisualEffect), &effect, freecs_bit_index(BIT_VISUAL_EFFECT)}
    };

    freecs_spawn(&world, BIT_POSITION | BIT_VISUAL_EFFECT, entries, 2);
}

static void spawn_money_popup(float pos_x, float pos_y, int amount) {
    Position position = {pos_x, pos_y};
    MoneyPopup popup = {0.0f, amount};

    freecs_type_info_entry_t entries[2] = {
        {BIT_POSITION, sizeof(Position), &position, freecs_bit_index(BIT_POSITION)},
        {BIT_MONEY_POPUP, sizeof(MoneyPopup), &popup, freecs_bit_index(BIT_MONEY_POPUP)}
    };

    freecs_spawn(&world, BIT_POSITION | BIT_MONEY_POPUP, entries, 2);
}

static bool can_place_tower_at(int x, int y) {
    size_t matching_count;
    size_t* matching = freecs_get_matching_archetypes(&world, BIT_TOWER | BIT_GRID_POSITION, 0, &matching_count);

    for (size_t i = 0; i < matching_count; i++) {
        freecs_archetype_t* arch = &world.archetypes[matching[i]];
        GridPosition* grid_positions = freecs_column_unchecked(arch, BIT_GRID_POSITION);

        for (size_t j = 0; j < arch->entities_len; j++) {
            if (grid_positions[j].x == x && grid_positions[j].y == y) {
                return false;
            }
        }
    }

    matching = freecs_get_matching_archetypes(&world, BIT_GRID_CELL, 0, &matching_count);
    for (size_t i = 0; i < matching_count; i++) {
        freecs_archetype_t* arch = &world.archetypes[matching[i]];
        GridCell* cells = freecs_column_unchecked(arch, BIT_GRID_CELL);

        for (size_t j = 0; j < arch->entities_len; j++) {
            if (cells[j].x == x && cells[j].y == y && !cells[j].occupied) {
                return true;
            }
        }
    }
    return false;
}

static void mark_cell_occupied(int x, int y) {
    size_t matching_count;
    size_t* matching = freecs_get_matching_archetypes(&world, BIT_GRID_CELL, 0, &matching_count);

    for (size_t i = 0; i < matching_count; i++) {
        freecs_archetype_t* arch = &world.archetypes[matching[i]];
        GridCell* cells = freecs_column_unchecked(arch, BIT_GRID_CELL);

        for (size_t j = 0; j < arch->entities_len; j++) {
            if (cells[j].x == x && cells[j].y == y) {
                cells[j].occupied = true;
                return;
            }
        }
    }
}

static void plan_wave(void) {
    resources.wave++;
    uint32_t wave = resources.wave;

    float spawn_interval;
    if (wave <= 3) spawn_interval = 1.0f;
    else if (wave <= 6) spawn_interval = 0.8f;
    else if (wave <= 9) spawn_interval = 0.6f;
    else spawn_interval = 0.5f;

    size_t enemy_count = 5 + wave * 2;
    float spawn_time = 0.0f;

    resources.enemies_to_spawn_count = 0;

    for (size_t i = 0; i < enemy_count && resources.enemies_to_spawn_count < MAX_ENEMIES_TO_SPAWN; i++) {
        EnemyType selected_type;
        float roll = random_float();

        if (wave <= 2) {
            selected_type = ENEMY_NORMAL;
        } else if (wave <= 4) {
            selected_type = (roll < 0.7f) ? ENEMY_NORMAL : ENEMY_FAST;
        } else if (wave <= 6) {
            if (roll < 0.5f) selected_type = ENEMY_NORMAL;
            else if (roll < 0.8f) selected_type = ENEMY_FAST;
            else selected_type = ENEMY_TANK;
        } else if (wave <= 8) {
            if (roll < 0.3f) selected_type = ENEMY_NORMAL;
            else if (roll < 0.6f) selected_type = ENEMY_FAST;
            else if (roll < 0.8f) selected_type = ENEMY_TANK;
            else selected_type = ENEMY_FLYING;
        } else if (wave <= 10) {
            if (roll < 0.2f) selected_type = ENEMY_NORMAL;
            else if (roll < 0.4f) selected_type = ENEMY_FAST;
            else if (roll < 0.6f) selected_type = ENEMY_TANK;
            else if (roll < 0.8f) selected_type = ENEMY_FLYING;
            else selected_type = ENEMY_SHIELDED;
        } else if (wave <= 12) {
            if (roll < 0.2f) selected_type = ENEMY_FAST;
            else if (roll < 0.4f) selected_type = ENEMY_TANK;
            else if (roll < 0.6f) selected_type = ENEMY_FLYING;
            else if (roll < 0.8f) selected_type = ENEMY_SHIELDED;
            else selected_type = ENEMY_HEALER;
        } else if (wave <= 14) {
            if (roll < 0.2f) selected_type = ENEMY_TANK;
            else if (roll < 0.4f) selected_type = ENEMY_FLYING;
            else if (roll < 0.6f) selected_type = ENEMY_SHIELDED;
            else if (roll < 0.8f) selected_type = ENEMY_HEALER;
            else selected_type = ENEMY_BOSS;
        } else {
            if (roll < 0.15f) selected_type = ENEMY_TANK;
            else if (roll < 0.35f) selected_type = ENEMY_FLYING;
            else if (roll < 0.55f) selected_type = ENEMY_SHIELDED;
            else if (roll < 0.75f) selected_type = ENEMY_HEALER;
            else selected_type = ENEMY_BOSS;
        }

        resources.enemies_to_spawn[resources.enemies_to_spawn_count].enemy_type = selected_type;
        resources.enemies_to_spawn[resources.enemies_to_spawn_count].spawn_time = spawn_time;
        resources.enemies_to_spawn_count++;
        spawn_time += spawn_interval;
    }

    resources.spawn_timer = 0.0f;
    resources.game_state = GAME_WAVE_IN_PROGRESS;
    resources.wave_announce_timer = 3.0f;
}

static void wave_spawning_system(float delta_time) {
    if (resources.game_state != GAME_WAVE_IN_PROGRESS) return;

    resources.spawn_timer += delta_time;

    size_t spawned = 0;
    for (size_t i = 0; i < resources.enemies_to_spawn_count; ) {
        if (resources.enemies_to_spawn[i].spawn_time <= resources.spawn_timer) {
            spawn_enemy(resources.enemies_to_spawn[i].enemy_type);

            for (size_t j = i; j < resources.enemies_to_spawn_count - 1; j++) {
                resources.enemies_to_spawn[j] = resources.enemies_to_spawn[j + 1];
            }
            resources.enemies_to_spawn_count--;
            spawned++;
        } else {
            i++;
        }
    }

    size_t enemy_count = freecs_query_count(&world, BIT_ENEMY, 0);

    if (resources.enemies_to_spawn_count == 0 && enemy_count == 0) {
        if (resources.wave >= 20) {
            resources.game_state = GAME_VICTORY;
        } else {
            uint32_t bonus = 20 + resources.wave * 5;
            resources.money += bonus;
            plan_wave();
        }
    }
}

static void apply_damage_to_enemy(freecs_entity_t enemy_entity, float damage, float pos_x, float pos_y);

static void enemy_movement_system(float delta_time) {
    if (resources.path_count < 2) return;

    uint32_t hp_damage = 0;

    size_t matching_count;
    size_t* matching = freecs_get_matching_archetypes(&world, BIT_ENEMY | BIT_POSITION, 0, &matching_count);

    for (size_t i = 0; i < matching_count; i++) {
        freecs_archetype_t* arch = &world.archetypes[matching[i]];
        Position* positions = freecs_column_unchecked(arch, BIT_POSITION);
        Enemy* enemies = freecs_column_unchecked(arch, BIT_ENEMY);

        for (size_t j = 0; j < arch->entities_len; j++) {
            freecs_entity_t entity = arch->entities[j];
            Enemy* enemy = &enemies[j];
            Position* pos = &positions[j];

            if (enemy->health <= 0) continue;

            float speed_multiplier = (enemy->slow_duration > 0) ? 0.5f : 1.0f;
            float spd = enemy->speed * speed_multiplier;

            enemy->path_progress += spd * delta_time;

            if (enemy->slow_duration > 0) {
                enemy->slow_duration -= delta_time;
            }

            if (enemy->poison_duration > 0) {
                enemy->poison_duration -= delta_time;
                enemy->health -= enemy->poison_damage * delta_time;
                if (enemy->health <= 0) {
                    freecs_queue_despawn(&world, entity);
                    resources.money += enemy->value;

                    EnemyDiedEvent event = {entity, pos->x, pos->y, enemy->value, enemy->enemy_type};
                    freecs_send_event(&enemy_died_events, &event);
                    continue;
                }
            }

            if (enemy->path_index < resources.path_count - 1) {
                float current_x = resources.path[enemy->path_index][0];
                float current_y = resources.path[enemy->path_index][1];
                float next_x = resources.path[enemy->path_index + 1][0];
                float next_y = resources.path[enemy->path_index + 1][1];
                float dx = next_x - current_x;
                float dy = next_y - current_y;
                float segment_length = sqrtf(dx * dx + dy * dy);

                if (enemy->path_progress >= segment_length) {
                    enemy->path_progress -= segment_length;
                    enemy->path_index++;

                    if (enemy->path_index >= resources.path_count - 1) {
                        freecs_queue_despawn(&world, entity);
                        hp_damage++;
                        continue;
                    }
                }

                float cur_x = resources.path[enemy->path_index][0];
                float cur_y = resources.path[enemy->path_index][1];
                float nxt_x = resources.path[enemy->path_index + 1][0];
                float nxt_y = resources.path[enemy->path_index + 1][1];
                float dir_x = nxt_x - cur_x;
                float dir_y = nxt_y - cur_y;
                float len = sqrtf(dir_x * dir_x + dir_y * dir_y);
                if (len > 0) {
                    pos->x = cur_x + (dir_x / len) * enemy->path_progress;
                    pos->y = cur_y + (dir_y / len) * enemy->path_progress;
                }
            }
        }
    }

    if (hp_damage > 0) {
        if (resources.current_hp >= hp_damage) {
            resources.current_hp -= hp_damage;
        } else {
            resources.current_hp = 0;
        }

        if (resources.current_hp == 0) {
            resources.current_hp = resources.max_hp;
            if (resources.lives > 0) resources.lives--;

            if (resources.lives == 0) {
                resources.game_state = GAME_OVER;
            }
        }
    }

    freecs_apply_despawns(&world);
}

static void tower_targeting_system(void) {
    typedef struct { freecs_entity_t entity; float x; float y; } EnemyData;
    EnemyData enemy_data[1024];
    size_t enemy_data_count = 0;

    size_t enemy_matching_count;
    size_t* enemy_matching = freecs_get_matching_archetypes(&world, BIT_ENEMY | BIT_POSITION, 0, &enemy_matching_count);

    for (size_t i = 0; i < enemy_matching_count; i++) {
        freecs_archetype_t* arch = &world.archetypes[enemy_matching[i]];
        Position* positions = freecs_column_unchecked(arch, BIT_POSITION);

        for (size_t j = 0; j < arch->entities_len && enemy_data_count < 1024; j++) {
            enemy_data[enemy_data_count].entity = arch->entities[j];
            enemy_data[enemy_data_count].x = positions[j].x;
            enemy_data[enemy_data_count].y = positions[j].y;
            enemy_data_count++;
        }
    }

    size_t tower_matching_count;
    size_t* tower_matching = freecs_get_matching_archetypes(&world, BIT_TOWER | BIT_POSITION, 0, &tower_matching_count);

    for (size_t i = 0; i < tower_matching_count; i++) {
        freecs_archetype_t* arch = &world.archetypes[tower_matching[i]];
        Tower* towers = freecs_column_unchecked(arch, BIT_TOWER);
        Position* positions = freecs_column_unchecked(arch, BIT_POSITION);

        for (size_t j = 0; j < arch->entities_len; j++) {
            Tower* tower = &towers[j];
            Position* pos = &positions[j];

            float range = tower_range(tower->tower_type, tower->level);
            float range_sq = range * range;

            tower->has_target = false;
            float closest_dist_sq = 1e30f;

            for (size_t k = 0; k < enemy_data_count; k++) {
                float dx = enemy_data[k].x - pos->x;
                float dy = enemy_data[k].y - pos->y;
                float dist_sq = dx * dx + dy * dy;
                if (dist_sq <= range_sq && dist_sq < closest_dist_sq) {
                    closest_dist_sq = dist_sq;
                    tower->target = enemy_data[k].entity;
                    tower->has_target = true;
                }
            }

            if (tower->has_target) {
                tower->tracking_time += GetFrameTime();
            } else {
                tower->tracking_time = 0.0f;
            }
        }
    }
}

static void tower_shooting_system(float delta_time) {
    typedef struct { float x; float y; freecs_entity_t target; TowerType tower_type; uint32_t level; } ProjectileSpawn;
    ProjectileSpawn spawns[256];
    size_t spawn_count = 0;

    size_t matching_count;
    size_t* matching = freecs_get_matching_archetypes(&world, BIT_TOWER | BIT_POSITION, 0, &matching_count);

    for (size_t i = 0; i < matching_count; i++) {
        freecs_archetype_t* arch = &world.archetypes[matching[i]];
        Tower* towers = freecs_column_unchecked(arch, BIT_TOWER);
        Position* positions = freecs_column_unchecked(arch, BIT_POSITION);

        for (size_t j = 0; j < arch->entities_len; j++) {
            Tower* tower = &towers[j];
            Position* pos = &positions[j];

            tower->cooldown -= delta_time;

            if (tower->fire_animation > 0) {
                tower->fire_animation -= delta_time * 3.0f;
            }

            if (tower->cooldown <= 0 && tower->has_target && spawn_count < 256) {
                bool can_fire = (tower->tower_type != TOWER_SNIPER) || (tower->tracking_time >= 2.0f);

                if (can_fire) {
                    spawns[spawn_count].x = pos->x;
                    spawns[spawn_count].y = pos->y;
                    spawns[spawn_count].target = tower->target;
                    spawns[spawn_count].tower_type = tower->tower_type;
                    spawns[spawn_count].level = tower->level;
                    spawn_count++;

                    tower->cooldown = tower_fire_rate(tower->tower_type, tower->level);
                    tower->fire_animation = 1.0f;
                    tower->tracking_time = 0.0f;
                }
            }
        }
    }

    for (size_t i = 0; i < spawn_count; i++) {
        spawn_projectile(spawns[i].x, spawns[i].y, spawns[i].target, spawns[i].tower_type, spawns[i].level);

        if (spawns[i].tower_type == TOWER_CANNON) {
            for (int k = 0; k < 6; k++) {
                float offset_x = random_range(-5, 5);
                float offset_y = random_range(-5, 5);
                spawn_visual_effect(spawns[i].x + offset_x, spawns[i].y + offset_y, EFFECT_EXPLOSION, 0, 0, 0.3f);
            }
        }
    }
}

static void apply_damage_to_enemy(freecs_entity_t enemy_entity, float damage, float pos_x, float pos_y) {
    Enemy* enemy = FREECS_GET(&world, enemy_entity, Enemy, BIT_ENEMY);
    if (enemy == NULL) return;

    bool was_alive = enemy->health > 0;

    if (enemy->shield_health > 0) {
        float shield_damage = (damage < enemy->shield_health) ? damage : enemy->shield_health;
        enemy->shield_health -= shield_damage;
        float remaining = damage - shield_damage;
        if (remaining > 0) {
            enemy->health -= remaining;
        }
    } else {
        enemy->health -= damage;
    }

    if (was_alive && enemy->health <= 0) {
        EnemyDiedEvent event = {enemy_entity, pos_x, pos_y, enemy->value, enemy->enemy_type};
        freecs_send_event(&enemy_died_events, &event);
        freecs_queue_despawn(&world, enemy_entity);
    }
}

static void projectile_movement_system(float delta_time) {
    typedef struct { freecs_entity_t entity; float x; float y; } EnemyPos;
    EnemyPos enemy_positions[1024];
    size_t enemy_pos_count = 0;

    size_t enemy_matching_count;
    size_t* enemy_matching = freecs_get_matching_archetypes(&world, BIT_ENEMY | BIT_POSITION, 0, &enemy_matching_count);

    for (size_t i = 0; i < enemy_matching_count; i++) {
        freecs_archetype_t* arch = &world.archetypes[enemy_matching[i]];
        Position* positions = freecs_column_unchecked(arch, BIT_POSITION);

        for (size_t j = 0; j < arch->entities_len && enemy_pos_count < 1024; j++) {
            enemy_positions[enemy_pos_count].entity = arch->entities[j];
            enemy_positions[enemy_pos_count].x = positions[j].x;
            enemy_positions[enemy_pos_count].y = positions[j].y;
            enemy_pos_count++;
        }
    }

    typedef struct { freecs_entity_t enemy; float damage; TowerType tower_type; float x; float y; } Hit;
    Hit hits[256];
    size_t hit_count = 0;

    size_t proj_matching_count;
    size_t* proj_matching = freecs_get_matching_archetypes(&world, BIT_PROJECTILE | BIT_POSITION, 0, &proj_matching_count);

    for (size_t i = 0; i < proj_matching_count; i++) {
        freecs_archetype_t* arch = &world.archetypes[proj_matching[i]];
        Projectile* projectiles = freecs_column_unchecked(arch, BIT_PROJECTILE);
        Position* positions = freecs_column_unchecked(arch, BIT_POSITION);

        for (size_t j = 0; j < arch->entities_len; j++) {
            freecs_entity_t entity = arch->entities[j];
            Projectile* proj = &projectiles[j];
            Position* pos = &positions[j];

            EnemyPos* target_pos = NULL;
            for (size_t k = 0; k < enemy_pos_count; k++) {
                if (enemy_positions[k].entity.id == proj->target.id &&
                    enemy_positions[k].entity.generation == proj->target.generation) {
                    target_pos = &enemy_positions[k];
                    break;
                }
            }

            if (target_pos != NULL) {
                float dx = target_pos->x - proj->start_x;
                float dy = target_pos->y - proj->start_y;
                float total_distance = sqrtf(dx * dx + dy * dy);

                float to_target_x = target_pos->x - pos->x;
                float to_target_y = target_pos->y - pos->y;
                float distance_to_target = sqrtf(to_target_x * to_target_x + to_target_y * to_target_y);

                if (proj->arc_height > 0) {
                    float max_dist = (total_distance > 1.0f) ? total_distance : 1.0f;
                    proj->flight_progress += (proj->speed * delta_time) / max_dist;
                    if (proj->flight_progress > 1.0f) proj->flight_progress = 1.0f;
                    pos->x = proj->start_x + dx * proj->flight_progress;
                    pos->y = proj->start_y + dy * proj->flight_progress;
                } else {
                    if (distance_to_target > 0) {
                        float dir_x = to_target_x / distance_to_target;
                        float dir_y = to_target_y / distance_to_target;
                        pos->x += dir_x * proj->speed * delta_time;
                        pos->y += dir_y * proj->speed * delta_time;
                    }
                }

                if (distance_to_target < 10.0f || proj->flight_progress >= 1.0f) {
                    if (hit_count < 256) {
                        hits[hit_count].enemy = proj->target;
                        hits[hit_count].damage = proj->damage;
                        hits[hit_count].tower_type = proj->tower_type;
                        hits[hit_count].x = target_pos->x;
                        hits[hit_count].y = target_pos->y;
                        hit_count++;
                    }
                    freecs_queue_despawn(&world, entity);
                }
            } else {
                freecs_queue_despawn(&world, entity);
            }
        }
    }

    for (size_t i = 0; i < hit_count; i++) {
        Hit* hit = &hits[i];

        switch (hit->tower_type) {
            case TOWER_FROST: {
                Enemy* enemy = FREECS_GET(&world, hit->enemy, Enemy, BIT_ENEMY);
                if (enemy) enemy->slow_duration = 2.0f;
                apply_damage_to_enemy(hit->enemy, hit->damage, hit->x, hit->y);
                break;
            }
            case TOWER_POISON: {
                Enemy* enemy = FREECS_GET(&world, hit->enemy, Enemy, BIT_ENEMY);
                if (enemy) {
                    enemy->poison_duration = 3.0f;
                    enemy->poison_damage = 5.0f;
                }
                apply_damage_to_enemy(hit->enemy, hit->damage, hit->x, hit->y);
                for (int k = 0; k < 3; k++) {
                    float velocity_x = random_range(-20, 20);
                    float velocity_y = random_range(-20, 20);
                    spawn_visual_effect(hit->x, hit->y, EFFECT_POISON_BUBBLE, velocity_x, velocity_y, 2.0f);
                }
                break;
            }
            case TOWER_CANNON: {
                for (int k = 0; k < 8; k++) {
                    float velocity_x = random_range(-30, 30);
                    float velocity_y = random_range(-30, 30);
                    spawn_visual_effect(hit->x, hit->y, EFFECT_EXPLOSION, velocity_x, velocity_y, 0.5f);
                }

                size_t aoe_matching_count;
                size_t* aoe_matching = freecs_get_matching_archetypes(&world, BIT_ENEMY | BIT_POSITION, 0, &aoe_matching_count);

                for (size_t m = 0; m < aoe_matching_count; m++) {
                    freecs_archetype_t* arch = &world.archetypes[aoe_matching[m]];
                    Position* positions = freecs_column_unchecked(arch, BIT_POSITION);

                    for (size_t n = 0; n < arch->entities_len; n++) {
                        float dx = positions[n].x - hit->x;
                        float dy = positions[n].y - hit->y;
                        float distance = sqrtf(dx * dx + dy * dy);
                        if (distance < 60.0f) {
                            float damage_falloff = 1.0f - (distance / 60.0f);
                            apply_damage_to_enemy(arch->entities[n], hit->damage * damage_falloff, positions[n].x, positions[n].y);
                        }
                    }
                }
                break;
            }
            default:
                apply_damage_to_enemy(hit->enemy, hit->damage, hit->x, hit->y);
                break;
        }
    }

    freecs_apply_despawns(&world);
}

static void visual_effects_system(float delta_time) {
    size_t matching_count;
    size_t* matching = freecs_get_matching_archetypes(&world, BIT_VISUAL_EFFECT | BIT_POSITION, 0, &matching_count);

    for (size_t i = 0; i < matching_count; i++) {
        freecs_archetype_t* arch = &world.archetypes[matching[i]];
        VisualEffect* effects = freecs_column_unchecked(arch, BIT_VISUAL_EFFECT);
        Position* positions = freecs_column_unchecked(arch, BIT_POSITION);

        for (size_t j = 0; j < arch->entities_len; j++) {
            VisualEffect* effect = &effects[j];
            Position* pos = &positions[j];
            freecs_entity_t entity = arch->entities[j];

            effect->age += delta_time;

            if (effect->age >= effect->lifetime) {
                freecs_queue_despawn(&world, entity);
            } else {
                pos->x += effect->velocity_x * delta_time;
                pos->y += effect->velocity_y * delta_time;
            }
        }
    }

    freecs_apply_despawns(&world);
}

static void update_money_popups(float delta_time) {
    size_t matching_count;
    size_t* matching = freecs_get_matching_archetypes(&world, BIT_MONEY_POPUP | BIT_POSITION, 0, &matching_count);

    for (size_t i = 0; i < matching_count; i++) {
        freecs_archetype_t* arch = &world.archetypes[matching[i]];
        MoneyPopup* popups = freecs_column_unchecked(arch, BIT_MONEY_POPUP);
        Position* positions = freecs_column_unchecked(arch, BIT_POSITION);

        for (size_t j = 0; j < arch->entities_len; j++) {
            MoneyPopup* popup = &popups[j];
            Position* pos = &positions[j];
            freecs_entity_t entity = arch->entities[j];

            popup->lifetime += delta_time;

            if (popup->lifetime > 2.0f) {
                freecs_queue_despawn(&world, entity);
            } else {
                pos->y -= delta_time * 30.0f;
            }
        }
    }

    freecs_apply_despawns(&world);
}

static void enemy_died_event_handler(void) {
    size_t event_count;
    EnemyDiedEvent* events = FREECS_READ_EVENTS(&enemy_died_events, EnemyDiedEvent, &event_count);

    for (size_t i = 0; i < event_count; i++) {
        resources.money += events[i].reward;

        for (int k = 0; k < 6; k++) {
            float velocity_x = random_range(-40, 40);
            float velocity_y = random_range(-40, 40);
            spawn_visual_effect(events[i].pos_x, events[i].pos_y, EFFECT_DEATH_PARTICLE, velocity_x, velocity_y, 0.8f);
        }

        if (events[i].reward > 0) {
            spawn_money_popup(events[i].pos_x, events[i].pos_y, (int)events[i].reward);
        }
    }
    freecs_clear_events(&enemy_died_events);
}

static void enemy_spawned_event_handler(void) {
    size_t event_count;
    EnemySpawnedEvent* events = FREECS_READ_EVENTS(&enemy_spawned_events, EnemySpawnedEvent, &event_count);

    for (size_t i = 0; i < event_count; i++) {
        Position* pos = FREECS_GET(&world, events[i].entity, Position, BIT_POSITION);
        if (pos) {
            for (int k = 0; k < 4; k++) {
                float velocity_x = random_range(-30, 30);
                float velocity_y = random_range(-30, 30);
                spawn_visual_effect(pos->x, pos->y, EFFECT_DEATH_PARTICLE, velocity_x, velocity_y, 0.5f);
            }
        }
    }
    freecs_clear_events(&enemy_spawned_events);
}

static void sell_tower(freecs_entity_t tower_entity, int grid_x, int grid_y) {
    Tower* tower = FREECS_GET(&world, tower_entity, Tower, BIT_TOWER);
    if (tower == NULL) return;

    uint32_t refund = (uint32_t)((float)tower_cost(tower->tower_type) * 0.7f);
    resources.money += refund;

    float pos_x, pos_y;
    grid_to_base(grid_x, grid_y, &pos_x, &pos_y);
    spawn_money_popup(pos_x, pos_y, (int)refund);

    size_t matching_count;
    size_t* matching = freecs_get_matching_archetypes(&world, BIT_GRID_CELL, 0, &matching_count);

    for (size_t i = 0; i < matching_count; i++) {
        freecs_archetype_t* arch = &world.archetypes[matching[i]];
        GridCell* cells = freecs_column_unchecked(arch, BIT_GRID_CELL);

        for (size_t j = 0; j < arch->entities_len; j++) {
            if (cells[j].x == grid_x && cells[j].y == grid_y) {
                cells[j].occupied = false;
            }
        }
    }

    freecs_queue_despawn(&world, tower_entity);
    freecs_apply_despawns(&world);
}

static bool upgrade_tower(freecs_entity_t tower_entity, int grid_x, int grid_y) {
    Tower* tower = FREECS_GET(&world, tower_entity, Tower, BIT_TOWER);
    if (tower == NULL) return false;
    if (tower->level >= 4) return false;

    uint32_t cost = tower_upgrade_cost(tower->tower_type, tower->level);
    if (resources.money < cost) return false;

    resources.money -= cost;
    tower->level++;

    float pos_x, pos_y;
    grid_to_base(grid_x, grid_y, &pos_x, &pos_y);
    spawn_money_popup(pos_x, pos_y, -(int)cost);

    for (int k = 0; k < 12; k++) {
        float angle = random_float() * 3.14159f * 2.0f;
        float spd = random_range(20, 60);
        float velocity_x = cosf(angle) * spd;
        float velocity_y = sinf(angle) * spd;
        spawn_visual_effect(pos_x, pos_y, EFFECT_EXPLOSION, velocity_x, velocity_y, 0.8f);
    }

    return true;
}

static void restart_game(void) {
    uint64_t masks_to_clear[] = { BIT_TOWER, BIT_ENEMY, BIT_PROJECTILE, BIT_VISUAL_EFFECT, BIT_MONEY_POPUP };

    for (size_t m = 0; m < sizeof(masks_to_clear) / sizeof(masks_to_clear[0]); m++) {
        size_t matching_count;
        size_t* matching = freecs_get_matching_archetypes(&world, masks_to_clear[m], 0, &matching_count);

        for (size_t i = 0; i < matching_count; i++) {
            freecs_archetype_t* arch = &world.archetypes[matching[i]];
            for (size_t j = 0; j < arch->entities_len; j++) {
                freecs_queue_despawn(&world, arch->entities[j]);
            }
        }
    }

    freecs_apply_despawns(&world);

    resources.money = 200;
    resources.lives = 1;
    resources.wave = 0;
    resources.current_hp = 20;
    resources.max_hp = 20;
    resources.game_state = GAME_WAITING_FOR_WAVE;
    resources.game_speed = 1.0f;
    resources.spawn_timer = 0.0f;
    resources.enemies_to_spawn_count = 0;
    resources.wave_announce_timer = 0.0f;
}

static void input_system(void) {
    Vector2 mouse_pos = GetMousePosition();
    resources.mouse_on_grid = screen_to_grid(mouse_pos.x, mouse_pos.y, &resources.mouse_grid_x, &resources.mouse_grid_y);

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && resources.mouse_on_grid) {
        if (can_place_tower_at(resources.mouse_grid_x, resources.mouse_grid_y)) {
            TowerType tower_type = resources.selected_tower_type;
            if (resources.money >= tower_cost(tower_type)) {
                spawn_tower(resources.mouse_grid_x, resources.mouse_grid_y, tower_type);
                mark_cell_occupied(resources.mouse_grid_x, resources.mouse_grid_y);
                float pos_x, pos_y;
                grid_to_base(resources.mouse_grid_x, resources.mouse_grid_y, &pos_x, &pos_y);
                spawn_money_popup(pos_x, pos_y, -(int)tower_cost(tower_type));
            }
        }
    }

    if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON) && resources.mouse_on_grid) {
        size_t matching_count;
        size_t* matching = freecs_get_matching_archetypes(&world, BIT_TOWER | BIT_GRID_POSITION, 0, &matching_count);

        for (size_t i = 0; i < matching_count; i++) {
            freecs_archetype_t* arch = &world.archetypes[matching[i]];
            GridPosition* grid_positions = freecs_column_unchecked(arch, BIT_GRID_POSITION);

            for (size_t j = 0; j < arch->entities_len; j++) {
                if (grid_positions[j].x == resources.mouse_grid_x &&
                    grid_positions[j].y == resources.mouse_grid_y) {
                    sell_tower(arch->entities[j], resources.mouse_grid_x, resources.mouse_grid_y);
                    goto done_sell;
                }
            }
        }
        done_sell:;
    }

    if ((IsMouseButtonPressed(MOUSE_MIDDLE_BUTTON) || IsKeyPressed(KEY_U)) && resources.mouse_on_grid) {
        size_t matching_count;
        size_t* matching = freecs_get_matching_archetypes(&world, BIT_TOWER | BIT_GRID_POSITION, 0, &matching_count);

        for (size_t i = 0; i < matching_count; i++) {
            freecs_archetype_t* arch = &world.archetypes[matching[i]];
            GridPosition* grid_positions = freecs_column_unchecked(arch, BIT_GRID_POSITION);

            for (size_t j = 0; j < arch->entities_len; j++) {
                if (grid_positions[j].x == resources.mouse_grid_x &&
                    grid_positions[j].y == resources.mouse_grid_y) {
                    upgrade_tower(arch->entities[j], resources.mouse_grid_x, resources.mouse_grid_y);
                    goto done_upgrade;
                }
            }
        }
        done_upgrade:;
    }

    if (IsKeyPressed(KEY_ONE)) resources.selected_tower_type = TOWER_BASIC;
    if (IsKeyPressed(KEY_TWO)) resources.selected_tower_type = TOWER_FROST;
    if (IsKeyPressed(KEY_THREE)) resources.selected_tower_type = TOWER_CANNON;
    if (IsKeyPressed(KEY_FOUR)) resources.selected_tower_type = TOWER_SNIPER;
    if (IsKeyPressed(KEY_FIVE)) resources.selected_tower_type = TOWER_POISON;

    if (IsKeyPressed(KEY_LEFT_BRACKET)) {
        resources.game_speed -= 0.5f;
        if (resources.game_speed < 0.5f) resources.game_speed = 0.5f;
    }
    if (IsKeyPressed(KEY_RIGHT_BRACKET)) {
        resources.game_speed += 0.5f;
        if (resources.game_speed > 3.0f) resources.game_speed = 3.0f;
    }
    if (IsKeyPressed(KEY_BACKSLASH)) {
        resources.game_speed = 1.0f;
    }

    if (IsKeyPressed(KEY_P)) {
        if (resources.game_state == GAME_WAVE_IN_PROGRESS) {
            resources.game_state = GAME_PAUSED;
        } else if (resources.game_state == GAME_PAUSED) {
            resources.game_state = GAME_WAVE_IN_PROGRESS;
        }
    }

    if (IsKeyPressed(KEY_R)) {
        if (resources.game_state == GAME_OVER || resources.game_state == GAME_VICTORY) {
            restart_game();
        }
    }

    if (IsKeyPressed(KEY_SPACE) && resources.game_state == GAME_WAITING_FOR_WAVE) {
        plan_wave();
    }
}

static void render_grid(void) {
    float scale = get_scale();
    float offset_x, offset_y;
    get_offset(&offset_x, &offset_y);

    size_t matching_count;
    size_t* matching = freecs_get_matching_archetypes(&world, BIT_GRID_CELL, 0, &matching_count);

    for (size_t i = 0; i < matching_count; i++) {
        freecs_archetype_t* arch = &world.archetypes[matching[i]];
        GridCell* cells = freecs_column_unchecked(arch, BIT_GRID_CELL);

        for (size_t j = 0; j < arch->entities_len; j++) {
            GridCell* cell = &cells[j];

            float base_x, base_y;
            grid_to_base(cell->x, cell->y, &base_x, &base_y);
            float pos_x = offset_x + base_x * scale;
            float pos_y = offset_y + base_y * scale;

            float start_x = resources.path[0][0];
            float start_y = resources.path[0][1];
            float end_x = resources.path[resources.path_count - 1][0];
            float end_y = resources.path[resources.path_count - 1][1];

            float start_screen_x = offset_x + start_x * scale;
            float start_screen_y = offset_y + start_y * scale;
            float end_screen_x = offset_x + end_x * scale;
            float end_screen_y = offset_y + end_y * scale;

            float to_start_x = pos_x - start_screen_x;
            float to_start_y = pos_y - start_screen_y;
            float to_end_x = pos_x - end_screen_x;
            float to_end_y = pos_y - end_screen_y;

            bool is_start = sqrtf(to_start_x * to_start_x + to_start_y * to_start_y) < TILE_SIZE * scale / 2.0f;
            bool is_end = sqrtf(to_end_x * to_end_x + to_end_y * to_end_y) < TILE_SIZE * scale / 2.0f;

            Color color;
            if (is_start) color = ORANGE;
            else if (is_end) color = BLUE;
            else if (cell->is_path) color = (Color){128, 77, 26, 255};
            else color = (Color){26, 77, 26, 255};

            int rect_x = (int)(pos_x - TILE_SIZE * scale / 2.0f + scale);
            int rect_y = (int)(pos_y - TILE_SIZE * scale / 2.0f + scale);
            int rect_w = (int)((TILE_SIZE - 2.0f) * scale);
            int rect_h = (int)((TILE_SIZE - 2.0f) * scale);

            DrawRectangle(rect_x, rect_y, rect_w, rect_h, color);
        }
    }

    if (resources.mouse_on_grid && can_place_tower_at(resources.mouse_grid_x, resources.mouse_grid_y)) {
        TowerType tower_type = resources.selected_tower_type;
        if (resources.money >= tower_cost(tower_type)) {
            float pos_x, pos_y;
            grid_to_screen(resources.mouse_grid_x, resources.mouse_grid_y, &pos_x, &pos_y);
            Color tcolor = tower_color(tower_type);

            int rect_x = (int)(pos_x - TILE_SIZE * scale / 2.0f + scale);
            int rect_y = (int)(pos_y - TILE_SIZE * scale / 2.0f + scale);
            int rect_w = (int)((TILE_SIZE - 2.0f) * scale);
            int rect_h = (int)((TILE_SIZE - 2.0f) * scale);

            DrawRectangle(rect_x, rect_y, rect_w, rect_h, (Color){tcolor.r, tcolor.g, tcolor.b, 77});
            DrawCircleLines((int)pos_x, (int)pos_y, tower_range(tower_type, 1) * scale, (Color){tcolor.r, tcolor.g, tcolor.b, 128});
        }
    }
}

static void render_towers(void) {
    float scale = get_scale();
    float offset_x, offset_y;
    get_offset(&offset_x, &offset_y);

    size_t matching_count;
    size_t* matching = freecs_get_matching_archetypes(&world, BIT_TOWER | BIT_POSITION, 0, &matching_count);

    for (size_t i = 0; i < matching_count; i++) {
        freecs_archetype_t* arch = &world.archetypes[matching[i]];
        Tower* towers = freecs_column_unchecked(arch, BIT_TOWER);
        Position* positions = freecs_column_unchecked(arch, BIT_POSITION);

        for (size_t j = 0; j < arch->entities_len; j++) {
            Tower* tower = &towers[j];
            Position* pos = &positions[j];

            float screen_x = offset_x + pos->x * scale;
            float screen_y = offset_y + pos->y * scale;

            float base_size = 20.0f + tower->fire_animation * 4.0f;
            float size = base_size * (1.0f + 0.15f * (float)(tower->level - 1)) * scale;

            Color color = tower_color(tower->tower_type);
            float level_brightness = 1.0f + 0.2f * (float)(tower->level - 1);
            Color upgraded_color = {
                (unsigned char)fminf((float)color.r * level_brightness, 255.0f),
                (unsigned char)fminf((float)color.g * level_brightness, 255.0f),
                (unsigned char)fminf((float)color.b * level_brightness, 255.0f),
                255
            };

            DrawCircle((int)screen_x, (int)screen_y, size / 2.0f, upgraded_color);
            DrawCircleLines((int)screen_x, (int)screen_y, size / 2.0f, BLACK);

            for (uint32_t ring = 1; ring < tower->level; ring++) {
                float ring_radius = size / 2.0f + (float)ring * 3.0f * scale;
                DrawCircleLines((int)screen_x, (int)screen_y, ring_radius, upgraded_color);
            }
        }
    }
}

static void render_enemies(void) {
    float scale = get_scale();
    float offset_x, offset_y;
    get_offset(&offset_x, &offset_y);

    size_t matching_count;
    size_t* matching = freecs_get_matching_archetypes(&world, BIT_ENEMY | BIT_POSITION, 0, &matching_count);

    for (size_t i = 0; i < matching_count; i++) {
        freecs_archetype_t* arch = &world.archetypes[matching[i]];
        Enemy* enemies = freecs_column_unchecked(arch, BIT_ENEMY);
        Position* positions = freecs_column_unchecked(arch, BIT_POSITION);

        for (size_t j = 0; j < arch->entities_len; j++) {
            Enemy* enemy = &enemies[j];
            Position* pos = &positions[j];

            float screen_x = offset_x + pos->x * scale;
            float screen_y = offset_y + pos->y * scale;
            float size = enemy_size(enemy->enemy_type) * scale;

            DrawCircle((int)screen_x, (int)screen_y, size, enemy_color(enemy->enemy_type));
            DrawCircleLines((int)screen_x, (int)screen_y, size, BLACK);

            if (enemy->shield_health > 0) {
                unsigned char shield_alpha = (unsigned char)((enemy->shield_health / enemy->max_shield) * 255.0f);
                DrawCircleLines((int)screen_x, (int)screen_y, size + 3.0f * scale, (Color){128, 128, 255, shield_alpha});
            }

            float health_percent = enemy->health / enemy->max_health;
            float bar_width = size * 2.0f;
            float bar_height = 4.0f * scale;
            float bar_y = screen_y - size - 10.0f * scale;

            DrawRectangle((int)(screen_x - bar_width / 2.0f), (int)bar_y, (int)bar_width, (int)bar_height, BLACK);

            Color health_color;
            if (health_percent > 0.5f) health_color = GREEN;
            else if (health_percent > 0.25f) health_color = YELLOW;
            else health_color = RED;

            DrawRectangle((int)(screen_x - bar_width / 2.0f), (int)bar_y, (int)(bar_width * health_percent), (int)bar_height, health_color);
        }
    }
}

static void render_projectiles(void) {
    float scale = get_scale();
    float offset_x, offset_y;
    get_offset(&offset_x, &offset_y);

    size_t matching_count;
    size_t* matching = freecs_get_matching_archetypes(&world, BIT_PROJECTILE | BIT_POSITION, 0, &matching_count);

    for (size_t i = 0; i < matching_count; i++) {
        freecs_archetype_t* arch = &world.archetypes[matching[i]];
        Projectile* projectiles = freecs_column_unchecked(arch, BIT_PROJECTILE);
        Position* positions = freecs_column_unchecked(arch, BIT_POSITION);

        for (size_t j = 0; j < arch->entities_len; j++) {
            Projectile* proj = &projectiles[j];
            Position* pos = &positions[j];

            float screen_x = offset_x + pos->x * scale;
            float screen_y = offset_y + pos->y * scale;

            Color color;
            switch (proj->tower_type) {
                case TOWER_BASIC: color = YELLOW; break;
                case TOWER_FROST: color = SKYBLUE; break;
                case TOWER_CANNON: color = ORANGE; break;
                case TOWER_SNIPER: color = LIGHTGRAY; break;
                case TOWER_POISON: color = (Color){128, 0, 204, 255}; break;
                default: color = WHITE;
            }

            float base_size;
            if (proj->tower_type == TOWER_CANNON) base_size = 8.0f;
            else if (proj->tower_type == TOWER_SNIPER) base_size = 10.0f;
            else base_size = 5.0f;
            float size = base_size * scale;

            DrawCircle((int)screen_x, (int)screen_y, size, color);
        }
    }
}

static void render_visual_effects(void) {
    float scale = get_scale();
    float offset_x, offset_y;
    get_offset(&offset_x, &offset_y);

    size_t matching_count;
    size_t* matching = freecs_get_matching_archetypes(&world, BIT_VISUAL_EFFECT | BIT_POSITION, 0, &matching_count);

    for (size_t i = 0; i < matching_count; i++) {
        freecs_archetype_t* arch = &world.archetypes[matching[i]];
        VisualEffect* effects = freecs_column_unchecked(arch, BIT_VISUAL_EFFECT);
        Position* positions = freecs_column_unchecked(arch, BIT_POSITION);

        for (size_t j = 0; j < arch->entities_len; j++) {
            VisualEffect* effect = &effects[j];
            Position* pos = &positions[j];

            float screen_x = offset_x + pos->x * scale;
            float screen_y = offset_y + pos->y * scale;
            float progress = effect->age / effect->lifetime;
            unsigned char alpha = (unsigned char)((1.0f - progress) * 255.0f);

            switch (effect->effect_type) {
                case EFFECT_EXPLOSION: {
                    float size = (1.0f - progress) * 10.0f * scale;
                    DrawCircle((int)screen_x, (int)screen_y, size, (Color){255, 128, 0, alpha});
                    break;
                }
                case EFFECT_POISON_BUBBLE: {
                    float size = 5.0f * (1.0f + progress * 0.5f) * scale;
                    unsigned char bubble_alpha = (unsigned char)((float)alpha * 0.6f);
                    DrawCircle((int)screen_x, (int)screen_y, size, (Color){128, 0, 204, bubble_alpha});
                    break;
                }
                case EFFECT_DEATH_PARTICLE: {
                    float size = (1.0f - progress) * 5.0f * scale;
                    DrawCircle((int)screen_x, (int)screen_y, size, (Color){255, 0, 0, alpha});
                    break;
                }
            }
        }
    }
}

static void render_money_popups(void) {
    float scale = get_scale();
    float offset_x, offset_y;
    get_offset(&offset_x, &offset_y);

    size_t matching_count;
    size_t* matching = freecs_get_matching_archetypes(&world, BIT_MONEY_POPUP | BIT_POSITION, 0, &matching_count);

    for (size_t i = 0; i < matching_count; i++) {
        freecs_archetype_t* arch = &world.archetypes[matching[i]];
        MoneyPopup* popups = freecs_column_unchecked(arch, BIT_MONEY_POPUP);
        Position* positions = freecs_column_unchecked(arch, BIT_POSITION);

        for (size_t j = 0; j < arch->entities_len; j++) {
            MoneyPopup* popup = &popups[j];
            Position* pos = &positions[j];

            float screen_x = offset_x + pos->x * scale;
            float screen_y = offset_y + pos->y * scale;
            float progress = popup->lifetime / 2.0f;
            if (progress > 1.0f) progress = 1.0f;
            unsigned char alpha = (unsigned char)((1.0f - progress) * 255.0f);

            char text[32];
            if (popup->amount > 0) {
                snprintf(text, sizeof(text), "+$%d", popup->amount);
            } else {
                snprintf(text, sizeof(text), "-$%d", -popup->amount);
            }

            Color color = (popup->amount > 0) ? (Color){0, 255, 0, alpha} : (Color){255, 0, 0, alpha};
            int font_size = (int)(20.0f * scale);
            DrawText(text, (int)(screen_x - 20.0f * scale), (int)screen_y, font_size, color);
        }
    }
}

static void render_ui(void) {
    float screen_w = (float)GetScreenWidth();
    float screen_h = (float)GetScreenHeight();
    char buf[64];

    snprintf(buf, sizeof(buf), "Money: $%u", resources.money);
    DrawText(buf, 10, 30, 30, GREEN);

    snprintf(buf, sizeof(buf), "Lives: %u", resources.lives);
    DrawText(buf, 10, 60, 25, RED);

    snprintf(buf, sizeof(buf), "HP: %u/%u", resources.current_hp, resources.max_hp);
    DrawText(buf, 10, 90, 25, YELLOW);

    snprintf(buf, sizeof(buf), "Wave: %u", resources.wave);
    DrawText(buf, (int)(screen_w - 150), 30, 30, SKYBLUE);

    snprintf(buf, sizeof(buf), "Speed: %.1fx", resources.game_speed);
    DrawText(buf, (int)(screen_w - 150), 60, 20, WHITE);

    float bar_width = 200.0f;
    float bar_height = 20.0f;
    float bar_x = 10.0f;
    float bar_y = 100.0f;

    DrawRectangle((int)bar_x, (int)bar_y, (int)bar_width, (int)bar_height, BLACK);

    uint32_t total_hp = (resources.lives - 1) * resources.max_hp + resources.current_hp;
    uint32_t max_total_hp = resources.lives * resources.max_hp;
    float health_percentage = (float)total_hp / (float)max_total_hp;

    Color health_color;
    if (health_percentage > 0.5f) health_color = GREEN;
    else if (health_percentage > 0.25f) health_color = YELLOW;
    else health_color = RED;
    DrawRectangle((int)bar_x, (int)bar_y, (int)(bar_width * health_percentage), (int)bar_height, health_color);

    int tower_ui_y = 140;
    struct { TowerType t; const char* key; } tower_types[] = {
        {TOWER_BASIC, "1"},
        {TOWER_FROST, "2"},
        {TOWER_CANNON, "3"},
        {TOWER_SNIPER, "4"},
        {TOWER_POISON, "5"}
    };

    for (size_t i = 0; i < 5; i++) {
        int x = 10 + (int)i * 60;
        bool is_selected = resources.selected_tower_type == tower_types[i].t;
        bool can_afford = resources.money >= tower_cost(tower_types[i].t);

        Color base_color = tower_color(tower_types[i].t);
        Color color;
        if (is_selected) {
            color = base_color;
        } else if (can_afford) {
            color = (Color){
                (unsigned char)((float)base_color.r * 0.7f),
                (unsigned char)((float)base_color.g * 0.7f),
                (unsigned char)((float)base_color.b * 0.7f),
                255
            };
        } else {
            color = DARKGRAY;
        }

        DrawRectangle(x, tower_ui_y, 50, 50, color);
        DrawRectangleLines(x, tower_ui_y, 50, 50, BLACK);

        DrawText(tower_types[i].key, x + 5, tower_ui_y + 5, 20, BLACK);
        snprintf(buf, sizeof(buf), "$%u", tower_cost(tower_types[i].t));
        DrawText(buf, x + 5, tower_ui_y + 30, 15, BLACK);
    }

    if (resources.wave_announce_timer > 0) {
        unsigned char alpha = (resources.wave_announce_timer < 1.0f) ?
            (unsigned char)(resources.wave_announce_timer * 255.0f) : 255;
        snprintf(buf, sizeof(buf), "WAVE %u", resources.wave);
        int text_width = MeasureText(buf, 60);
        DrawText(buf, (int)(screen_w / 2.0f - (float)text_width / 2.0f), (int)(screen_h / 2.0f - 100), 60, (Color){255, 204, 0, alpha});
    }

    switch (resources.game_state) {
        case GAME_WAITING_FOR_WAVE: {
            const char* text = "Press SPACE to start wave";
            int text_width = MeasureText(text, 40);
            DrawText(text, (int)(screen_w / 2.0f - (float)text_width / 2.0f), (int)(screen_h / 2.0f), 40, WHITE);
            break;
        }
        case GAME_PAUSED: {
            const char* text = "PAUSED - Press P to resume";
            int text_width = MeasureText(text, 50);
            DrawText(text, (int)(screen_w / 2.0f - (float)text_width / 2.0f), (int)(screen_h / 2.0f), 50, YELLOW);
            break;
        }
        case GAME_OVER: {
            const char* text = "GAME OVER - Press R to restart";
            int text_width = MeasureText(text, 50);
            DrawText(text, (int)(screen_w / 2.0f - (float)text_width / 2.0f), (int)(screen_h / 2.0f), 50, RED);
            break;
        }
        case GAME_VICTORY: {
            const char* text = "VICTORY! Press R to restart";
            int text_width = MeasureText(text, 50);
            DrawText(text, (int)(screen_w / 2.0f - (float)text_width / 2.0f), (int)(screen_h / 2.0f), 50, GREEN);
            break;
        }
        default:
            break;
    }

    const char* controls_text = "Controls: 1-5: Tower | LClick: Place | RClick: Sell | U: Upgrade | [/]: Speed | P: Pause";
    DrawText(controls_text, 10, (int)(screen_h - 25), 15, LIGHTGRAY);
}

int main(void) {
    srand((unsigned int)time(NULL));

    InitWindow(1024, 768, "Tower Defense - freecs-c");
    SetTargetFPS(60);

    world = freecs_create_world();

    BIT_POSITION = FREECS_REGISTER(&world, Position);
    BIT_VELOCITY = FREECS_REGISTER(&world, Velocity);
    BIT_TOWER = FREECS_REGISTER(&world, Tower);
    BIT_ENEMY = FREECS_REGISTER(&world, Enemy);
    BIT_PROJECTILE = FREECS_REGISTER(&world, Projectile);
    BIT_GRID_CELL = FREECS_REGISTER(&world, GridCell);
    BIT_GRID_POSITION = FREECS_REGISTER(&world, GridPosition);
    BIT_VISUAL_EFFECT = FREECS_REGISTER(&world, VisualEffect);
    BIT_MONEY_POPUP = FREECS_REGISTER(&world, MoneyPopup);

    enemy_died_events = FREECS_CREATE_EVENT_QUEUE(EnemyDiedEvent);
    enemy_spawned_events = FREECS_CREATE_EVENT_QUEUE(EnemySpawnedEvent);

    resources.money = 200;
    resources.lives = 1;
    resources.wave = 0;
    resources.game_state = GAME_WAITING_FOR_WAVE;
    resources.selected_tower_type = TOWER_BASIC;
    resources.spawn_timer = 0.0f;
    resources.enemies_to_spawn_count = 0;
    resources.mouse_on_grid = false;
    resources.path_count = 0;
    resources.wave_announce_timer = 0.0f;
    resources.game_speed = 1.0f;
    resources.current_hp = 20;
    resources.max_hp = 20;

    initialize_grid();
    create_path();

    while (!WindowShouldClose()) {
        float base_dt = GetFrameTime();
        float dt = base_dt * resources.game_speed;

        input_system();

        if (resources.game_state != GAME_PAUSED) {
            wave_spawning_system(dt);
            enemy_movement_system(dt);
            tower_targeting_system();
            tower_shooting_system(dt);
            projectile_movement_system(dt);
            visual_effects_system(dt);
            update_money_popups(dt);

            enemy_died_event_handler();
            enemy_spawned_event_handler();
        }

        if (resources.wave_announce_timer > 0) {
            resources.wave_announce_timer -= base_dt;
        }

        BeginDrawing();
        ClearBackground((Color){13, 13, 13, 255});

        render_grid();
        render_towers();
        render_enemies();
        render_projectiles();
        render_visual_effects();
        render_money_popups();
        render_ui();

        EndDrawing();
    }

    freecs_destroy_event_queue(&enemy_died_events);
    freecs_destroy_event_queue(&enemy_spawned_events);
    freecs_destroy_world(&world);
    CloseWindow();

    return 0;
}
