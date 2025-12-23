#include "freecs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { \
    printf("  Running %s... ", #name); \
    tests_run++; \
    test_##name(); \
    tests_passed++; \
    printf("PASSED\n"); \
} while(0)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf("FAILED\n    Assertion failed: %s\n    at %s:%d\n", #cond, __FILE__, __LINE__); \
        exit(1); \
    } \
} while(0)

#define ASSERT_EQ(a, b) ASSERT((a) == (b))
#define ASSERT_FLOAT_EQ(a, b) ASSERT(fabsf((a) - (b)) < 0.001f)

typedef struct { float x; float y; } Position;
typedef struct { float x; float y; } Velocity;
typedef struct { float value; } Health;

static uint64_t BIT_POSITION;
static uint64_t BIT_VELOCITY;
static uint64_t BIT_HEALTH;

static void setup_world(freecs_world_t* world) {
    BIT_POSITION = FREECS_REGISTER(world, Position);
    BIT_VELOCITY = FREECS_REGISTER(world, Velocity);
    BIT_HEALTH = FREECS_REGISTER(world, Health);
}

TEST(spawn_entity) {
    freecs_world_t world = freecs_create_world();
    setup_world(&world);
    Position pos = {1, 2};
    Velocity vel = {3, 4};
    freecs_type_info_entry_t entries[2] = {
        {BIT_POSITION, sizeof(Position), &pos, 0},
        {BIT_VELOCITY, sizeof(Velocity), &vel, 1}
    };
    freecs_entity_t entity = freecs_spawn(&world, BIT_POSITION | BIT_VELOCITY, entries, 2);
    ASSERT_EQ(entity.id, 0);
    freecs_destroy_world(&world);
}

TEST(get_component) {
    freecs_world_t world = freecs_create_world();
    setup_world(&world);
    Position pos = {1, 2};
    Velocity vel = {3, 4};
    freecs_type_info_entry_t entries[2] = {
        {BIT_POSITION, sizeof(Position), &pos, 0},
        {BIT_VELOCITY, sizeof(Velocity), &vel, 1}
    };
    freecs_entity_t entity = freecs_spawn(&world, BIT_POSITION | BIT_VELOCITY, entries, 2);
    Position* got_pos = (Position*)freecs_get(&world, entity, BIT_POSITION);
    ASSERT(got_pos != NULL);
    ASSERT_FLOAT_EQ(got_pos->x, 1);
    freecs_destroy_world(&world);
}

static float callback_sum = 0;
static uint64_t callback_position_bit;

static void sum_callback(freecs_archetype_t* arch, size_t index) {
    Position* positions = freecs_column_unchecked(arch, callback_position_bit);
    callback_sum += positions[index].x;
}

TEST(for_each_callback) {
    freecs_world_t world = freecs_create_world();
    setup_world(&world);
    callback_position_bit = BIT_POSITION;
    
    Position pos1 = {1, 0};
    freecs_type_info_entry_t e1[1] = {{BIT_POSITION, sizeof(Position), &pos1, 0}};
    freecs_spawn(&world, BIT_POSITION, e1, 1);
    
    callback_sum = 0;
    freecs_for_each(&world, BIT_POSITION, 0, sum_callback);
    ASSERT_FLOAT_EQ(callback_sum, 1);
    
    freecs_destroy_world(&world);
}

int main(void) {
    printf("Running freecs tests...\n\n");
    RUN_TEST(spawn_entity);
    RUN_TEST(get_component);
    RUN_TEST(for_each_callback);
    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
