#include "../freecs.h"
#include <raylib.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

typedef struct { float x, y; } Position;
typedef struct { float x, y; } Velocity;
typedef struct { uint8_t _; } Boid;
typedef struct { float r, g, b; } BoidColor;

typedef struct {
    float alignment_weight;
    float cohesion_weight;
    float separation_weight;
    float visual_range;
    float visual_range_sq;
    float min_speed;
    float max_speed;
    bool paused;
    float mouse_attraction_weight;
    float mouse_repulsion_weight;
    float mouse_influence_range;
} BoidParams;

typedef struct {
    float x, y;
    float vx, vy;
} BoidData;

typedef struct {
    BoidData** cells;
    int* cell_counts;
    float cell_size;
    int width;
    int height;
    int total;
    float inv_cell;
    int max_per_cell;
} SpatialGrid;

typedef struct {
    Position* positions;
    Velocity* velocities;
    int capacity;
} BoidCache;

static uint64_t BIT_POSITION;
static uint64_t BIT_VELOCITY;
static uint64_t BIT_BOID;
static uint64_t BIT_COLOR;

static SpatialGrid create_grid(float screen_width, float screen_height, float cell_size, int max_per_cell) {
    int width = (int)ceilf(screen_width / cell_size);
    int height = (int)ceilf(screen_height / cell_size);
    int total = width * height;

    BoidData** cells = malloc(total * sizeof(BoidData*));
    int* cell_counts = malloc(total * sizeof(int));
    for (int i = 0; i < total; i++) {
        cells[i] = malloc(max_per_cell * sizeof(BoidData));
        cell_counts[i] = 0;
    }

    return (SpatialGrid){
        .cells = cells,
        .cell_counts = cell_counts,
        .cell_size = cell_size,
        .width = width,
        .height = height,
        .total = total,
        .inv_cell = 1.0f / cell_size,
        .max_per_cell = max_per_cell
    };
}

static void destroy_grid(SpatialGrid* grid) {
    for (int i = 0; i < grid->total; i++) {
        free(grid->cells[i]);
    }
    free(grid->cells);
    free(grid->cell_counts);
}

static inline void grid_clear(SpatialGrid* grid) {
    memset(grid->cell_counts, 0, grid->total * sizeof(int));
}

static inline void grid_insert(SpatialGrid* grid, float x, float y, float vx, float vy) {
    int cell_x = (int)(x * grid->inv_cell);
    int cell_y = (int)(y * grid->inv_cell);
    if (cell_x < 0) cell_x = 0;
    if (cell_x >= grid->width) cell_x = grid->width - 1;
    if (cell_y < 0) cell_y = 0;
    if (cell_y >= grid->height) cell_y = grid->height - 1;

    int idx = cell_x + cell_y * grid->width;
    int count = grid->cell_counts[idx];
    if (count < grid->max_per_cell) {
        grid->cells[idx][count] = (BoidData){x, y, vx, vy};
        grid->cell_counts[idx] = count + 1;
    }
}

static inline float fast_inv_sqrt(float x) {
    float xhalf = 0.5f * x;
    union { float f; int32_t i; } conv = { .f = x };
    conv.i = 0x5f3759df - (conv.i >> 1);
    conv.f = conv.f * (1.5f - xhalf * conv.f * conv.f);
    return conv.f;
}

static BoidCache create_cache(int capacity) {
    return (BoidCache){
        .positions = malloc(capacity * sizeof(Position)),
        .velocities = malloc(capacity * sizeof(Velocity)),
        .capacity = capacity
    };
}

static void destroy_cache(BoidCache* cache) {
    free(cache->positions);
    free(cache->velocities);
}

static void ensure_cache_capacity(BoidCache* cache, int needed) {
    if (needed > cache->capacity) {
        int new_cap = needed * 2;
        free(cache->positions);
        free(cache->velocities);
        cache->positions = malloc(new_cap * sizeof(Position));
        cache->velocities = malloc(new_cap * sizeof(Velocity));
        cache->capacity = new_cap;
    }
}

static float rand_float(void) {
    return (float)rand() / (float)RAND_MAX;
}

static float rand_range(float min, float max) {
    return min + rand_float() * (max - min);
}

static void spawn_boids(freecs_world_t* world, int count, float screen_w, float screen_h) {
    for (int i = 0; i < count; i++) {
        float angle = rand_float() * 3.14159265f * 2.0f;
        float speed = rand_range(100.0f, 200.0f);

        Position pos = {rand_range(0, screen_w), rand_range(0, screen_h)};
        Velocity vel = {cosf(angle) * speed, sinf(angle) * speed};
        Boid boid = {0};
        BoidColor color = {rand_range(0.5f, 1.0f), rand_range(0.5f, 1.0f), rand_range(0.5f, 1.0f)};

        freecs_type_info_entry_t entries[] = {
            {BIT_POSITION, sizeof(Position), &pos, freecs_bit_index(BIT_POSITION)},
            {BIT_VELOCITY, sizeof(Velocity), &vel, freecs_bit_index(BIT_VELOCITY)},
            {BIT_BOID, sizeof(Boid), &boid, freecs_bit_index(BIT_BOID)},
            {BIT_COLOR, sizeof(BoidColor), &color, freecs_bit_index(BIT_COLOR)}
        };
        freecs_spawn(world, BIT_POSITION | BIT_VELOCITY | BIT_BOID | BIT_COLOR, entries, 4);
    }
}

static void process_boids(freecs_world_t* world, SpatialGrid* grid, BoidCache* cache,
                         BoidParams* params, float mouse_x, float mouse_y,
                         bool mouse_attract, bool mouse_repel) {
    const int MAX_NEIGHBORS = 7;
    uint64_t boid_mask = BIT_POSITION | BIT_VELOCITY | BIT_BOID;

    size_t entity_total = freecs_entity_count(world);
    ensure_cache_capacity(cache, (int)entity_total);

    grid_clear(grid);

    size_t matching_count;
    size_t* matching = freecs_get_matching_archetypes(world, boid_mask, 0, &matching_count);
    int boid_count = 0;

    for (size_t m = 0; m < matching_count; m++) {
        freecs_archetype_t* arch = &world->archetypes[matching[m]];
        Position* positions = FREECS_COLUMN(arch, Position, BIT_POSITION);
        Velocity* velocities = FREECS_COLUMN(arch, Velocity, BIT_VELOCITY);
        size_t count = arch->entities_len;

        for (size_t i = 0; i < count; i++) {
            Position p = positions[i];
            Velocity v = velocities[i];
            cache->positions[boid_count] = p;
            cache->velocities[boid_count] = v;
            grid_insert(grid, p.x, p.y, v.x, v.y);
            boid_count++;
        }
    }

    float visual_range_sq = params->visual_range_sq;
    int range_cells = (int)ceilf(params->visual_range * grid->inv_cell);
    float mouse_range_sq = params->mouse_influence_range * params->mouse_influence_range;

    int boid_idx = 0;
    for (size_t m = 0; m < matching_count; m++) {
        freecs_archetype_t* arch = &world->archetypes[matching[m]];
        Velocity* velocities = FREECS_COLUMN(arch, Velocity, BIT_VELOCITY);
        size_t count = arch->entities_len;

        for (size_t i = 0; i < count; i++) {
            Position pos = cache->positions[boid_idx];
            Velocity vel = cache->velocities[boid_idx];

            float align_x = 0, align_y = 0;
            float cohesion_x = 0, cohesion_y = 0;
            float sep_x = 0, sep_y = 0;
            int neighbors = 0;

            int cell_x = (int)(pos.x * grid->inv_cell);
            int cell_y = (int)(pos.y * grid->inv_cell);

            for (int dy = -range_cells; dy <= range_cells && neighbors < MAX_NEIGHBORS; dy++) {
                int cy = cell_y + dy;
                if (cy < 0 || cy >= grid->height) continue;

                for (int dx = -range_cells; dx <= range_cells && neighbors < MAX_NEIGHBORS; dx++) {
                    int cx = cell_x + dx;
                    if (cx < 0 || cx >= grid->width) continue;

                    int cell_idx = cx + cy * grid->width;
                    int cell_count = grid->cell_counts[cell_idx];
                    BoidData* cell = grid->cells[cell_idx];

                    for (int j = 0; j < cell_count && neighbors < MAX_NEIGHBORS; j++) {
                        BoidData b = cell[j];
                        float bx = b.x - pos.x;
                        float by = b.y - pos.y;
                        float dist_sq = bx * bx + by * by;

                        if (dist_sq > 0 && dist_sq < visual_range_sq) {
                            align_x += b.vx;
                            align_y += b.vy;
                            cohesion_x += b.x;
                            cohesion_y += b.y;
                            float inv_dist = fast_inv_sqrt(dist_sq);
                            sep_x -= bx * inv_dist;
                            sep_y -= by * inv_dist;
                            neighbors++;
                        }
                    }
                }
            }

            float mouse_dx = mouse_x - pos.x;
            float mouse_dy = mouse_y - pos.y;
            float mouse_dist_sq = mouse_dx * mouse_dx + mouse_dy * mouse_dy;

            if (mouse_dist_sq < mouse_range_sq) {
                float mouse_inv = fast_inv_sqrt(mouse_range_sq);
                float mouse_influence = 1.0f - sqrtf(mouse_dist_sq) * mouse_inv;
                if (mouse_attract) {
                    vel.x += mouse_dx * mouse_influence * params->mouse_attraction_weight;
                    vel.y += mouse_dy * mouse_influence * params->mouse_attraction_weight;
                }
                if (mouse_repel) {
                    vel.x -= mouse_dx * mouse_influence * params->mouse_repulsion_weight;
                    vel.y -= mouse_dy * mouse_influence * params->mouse_repulsion_weight;
                }
            }

            if (neighbors > 0) {
                float inv = 1.0f / (float)neighbors;
                vel.x += (align_x * inv) * params->alignment_weight;
                vel.y += (align_y * inv) * params->alignment_weight;
                vel.x += (cohesion_x * inv - pos.x) * params->cohesion_weight;
                vel.y += (cohesion_y * inv - pos.y) * params->cohesion_weight;
                vel.x += sep_x * params->separation_weight;
                vel.y += sep_y * params->separation_weight;
            }

            float speed_sq = vel.x * vel.x + vel.y * vel.y;
            float max_sq = params->max_speed * params->max_speed;
            float min_sq = params->min_speed * params->min_speed;

            if (speed_sq > max_sq) {
                float f = params->max_speed * fast_inv_sqrt(speed_sq);
                vel.x *= f;
                vel.y *= f;
            } else if (speed_sq < min_sq && speed_sq > 0) {
                float f = params->min_speed * fast_inv_sqrt(speed_sq);
                vel.x *= f;
                vel.y *= f;
            }

            velocities[i] = vel;
            boid_idx++;
        }
    }
}

static void update_positions(freecs_world_t* world, float dt) {
    uint64_t move_mask = BIT_POSITION | BIT_VELOCITY;
    size_t matching_count;
    size_t* matching = freecs_get_matching_archetypes(world, move_mask, 0, &matching_count);

    for (size_t m = 0; m < matching_count; m++) {
        freecs_archetype_t* arch = &world->archetypes[matching[m]];
        Position* positions = FREECS_COLUMN(arch, Position, BIT_POSITION);
        Velocity* velocities = FREECS_COLUMN(arch, Velocity, BIT_VELOCITY);
        size_t count = arch->entities_len;

        for (size_t i = 0; i < count; i++) {
            positions[i].x += velocities[i].x * dt;
            positions[i].y += velocities[i].y * dt;
        }
    }
}

static void wrap_positions(freecs_world_t* world, float screen_w, float screen_h) {
    size_t matching_count;
    size_t* matching = freecs_get_matching_archetypes(world, BIT_POSITION, 0, &matching_count);

    for (size_t m = 0; m < matching_count; m++) {
        freecs_archetype_t* arch = &world->archetypes[matching[m]];
        Position* positions = FREECS_COLUMN(arch, Position, BIT_POSITION);
        size_t count = arch->entities_len;

        for (size_t i = 0; i < count; i++) {
            Position* p = &positions[i];
            if (p->x < 0) p->x += screen_w;
            else if (p->x > screen_w) p->x -= screen_w;
            if (p->y < 0) p->y += screen_h;
            else if (p->y > screen_h) p->y -= screen_h;
        }
    }
}

static void render_boids(freecs_world_t* world) {
    uint64_t render_mask = BIT_POSITION | BIT_VELOCITY | BIT_COLOR;
    size_t matching_count;
    size_t* matching = freecs_get_matching_archetypes(world, render_mask, 0, &matching_count);

    for (size_t m = 0; m < matching_count; m++) {
        freecs_archetype_t* arch = &world->archetypes[matching[m]];
        Position* positions = FREECS_COLUMN(arch, Position, BIT_POSITION);
        Velocity* velocities = FREECS_COLUMN(arch, Velocity, BIT_VELOCITY);
        BoidColor* colors = FREECS_COLUMN(arch, BoidColor, BIT_COLOR);
        size_t count = arch->entities_len;

        for (size_t i = 0; i < count; i++) {
            Position pos = positions[i];
            Velocity vel = velocities[i];
            BoidColor col = colors[i];

            float speed_sq = vel.x * vel.x + vel.y * vel.y;
            if (speed_sq < 0.01f) continue;

            float inv_speed = fast_inv_sqrt(speed_sq);
            float dx = vel.x * inv_speed;
            float dy = vel.y * inv_speed;

            float px = -dy * 4;
            float py = dx * 4;

            Vector2 p1 = {pos.x + dx * 6, pos.y + dy * 6};
            Vector2 p2 = {pos.x - dx * 4 + px, pos.y - dy * 4 + py};
            Vector2 p3 = {pos.x - dx * 4 - px, pos.y - dy * 4 - py};

            Color color = {(unsigned char)(col.r * 255), (unsigned char)(col.g * 255),
                          (unsigned char)(col.b * 255), 255};
            DrawTriangle(p1, p3, p2, color);
        }
    }
}

static void remove_boids(freecs_world_t* world, int count) {
    freecs_entity_t* to_despawn = malloc(count * sizeof(freecs_entity_t));
    int despawn_count = 0;

    for (size_t a = 0; a < world->archetypes_len && despawn_count < count; a++) {
        freecs_archetype_t* arch = &world->archetypes[a];
        for (size_t i = 0; i < arch->entities_len && despawn_count < count; i++) {
            to_despawn[despawn_count++] = arch->entities[i];
        }
    }

    for (int i = 0; i < despawn_count; i++) {
        freecs_despawn(world, to_despawn[i]);
    }

    free(to_despawn);
}

int main(void) {
    int screen_w = 1280;
    int screen_h = 720;

    InitWindow(screen_w, screen_h, "Boids - C ECS");
    SetTargetFPS(60);

    freecs_world_t world = freecs_create_world();

    BIT_POSITION = FREECS_REGISTER(&world, Position);
    BIT_VELOCITY = FREECS_REGISTER(&world, Velocity);
    BIT_BOID = FREECS_REGISTER(&world, Boid);
    BIT_COLOR = FREECS_REGISTER(&world, BoidColor);

    float visual_range = 50.0f;
    BoidParams params = {
        .alignment_weight = 0.5f,
        .cohesion_weight = 0.3f,
        .separation_weight = 0.4f,
        .visual_range = visual_range,
        .visual_range_sq = visual_range * visual_range,
        .min_speed = 100.0f,
        .max_speed = 300.0f,
        .paused = false,
        .mouse_attraction_weight = 0.96f,
        .mouse_repulsion_weight = 1.2f,
        .mouse_influence_range = 150.0f
    };

    SpatialGrid grid = create_grid((float)screen_w, (float)screen_h, visual_range / 2.0f, 64);
    BoidCache cache = create_cache(2000);

    spawn_boids(&world, 1000, (float)screen_w, (float)screen_h);

    while (!WindowShouldClose()) {
        float dt = params.paused ? 0 : GetFrameTime();

        Vector2 mouse = GetMousePosition();
        bool mouse_attract = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
        bool mouse_repel = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);

        if (IsKeyPressed(KEY_SPACE)) params.paused = !params.paused;

        if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD)) {
            spawn_boids(&world, 1000, (float)screen_w, (float)screen_h);
        }
        if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT)) {
            remove_boids(&world, 1000);
        }

        float speed = IsKeyDown(KEY_LEFT_SHIFT) ? 0.01f : 0.001f;
        if (IsKeyDown(KEY_LEFT)) {
            params.alignment_weight -= speed;
            if (params.alignment_weight < 0) params.alignment_weight = 0;
        }
        if (IsKeyDown(KEY_RIGHT)) {
            params.alignment_weight += speed;
            if (params.alignment_weight > 1) params.alignment_weight = 1;
        }
        if (IsKeyDown(KEY_DOWN)) {
            params.cohesion_weight -= speed;
            if (params.cohesion_weight < 0) params.cohesion_weight = 0;
        }
        if (IsKeyDown(KEY_UP)) {
            params.cohesion_weight += speed;
            if (params.cohesion_weight > 1) params.cohesion_weight = 1;
        }

        process_boids(&world, &grid, &cache, &params, mouse.x, mouse.y, mouse_attract, mouse_repel);
        update_positions(&world, dt);
        wrap_positions(&world, (float)screen_w, (float)screen_h);

        BeginDrawing();
        ClearBackground(BLACK);

        render_boids(&world);

        if (mouse_attract || mouse_repel) {
            Color circle_color = mouse_attract ? (Color){0, 255, 0, 50} : (Color){255, 0, 0, 50};
            DrawCircleLines((int)mouse.x, (int)mouse.y, params.mouse_influence_range, circle_color);
        }

        size_t entity_count = freecs_entity_count(&world);
        DrawRectangle(screen_w - 260, 0, 260, 280, (Color){0, 0, 0, 180});

        int y = 20;
        DrawText(TextFormat("Entities: %zu", entity_count), screen_w - 250, y, 20, WHITE); y += 25;
        DrawText(TextFormat("FPS: %d", GetFPS()), screen_w - 250, y, 20, WHITE); y += 35;
        DrawText("[Space] Pause", screen_w - 250, y, 18, WHITE); y += 22;
        DrawText("[+/-] Add/Remove 1000", screen_w - 250, y, 18, WHITE); y += 22;
        DrawText("[Arrows] Adjust params", screen_w - 250, y, 18, WHITE); y += 35;
        DrawText(TextFormat("Alignment: %.2f", params.alignment_weight), screen_w - 250, y, 18, WHITE); y += 22;
        DrawText(TextFormat("Cohesion: %.2f", params.cohesion_weight), screen_w - 250, y, 18, WHITE); y += 22;
        DrawText(TextFormat("Separation: %.2f", params.separation_weight), screen_w - 250, y, 18, WHITE); y += 35;
        DrawText("[Left Mouse] Attract", screen_w - 250, y, 18, WHITE); y += 22;
        DrawText("[Right Mouse] Repel", screen_w - 250, y, 18, WHITE);

        EndDrawing();
    }

    destroy_cache(&cache);
    destroy_grid(&grid);
    freecs_destroy_world(&world);
    CloseWindow();

    return 0;
}
