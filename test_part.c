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
    ASSERT_EQ(entity.generation, 0);
    ASSERT_EQ(freecs_entity_count(&world), 1);

    freecs_destroy_world(&world);
}

int main(void) {
    printf("Running freecs tests...\n\n");
    RUN_TEST(spawn_entity);
    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
