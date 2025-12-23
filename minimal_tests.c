#include "freecs.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

typedef struct { float x; float y; } Position;
typedef struct { float x; float y; } Velocity;

static uint64_t BIT_POSITION;
static uint64_t BIT_VELOCITY;

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("Starting test...\n");

    freecs_world_t world = freecs_create_world();
    printf("World created\n");

    BIT_POSITION = freecs_register_component(&world, sizeof(Position));
    BIT_VELOCITY = freecs_register_component(&world, sizeof(Velocity));
    printf("Components registered\n");

    Position pos = {1, 2};
    Velocity vel = {3, 4};
    freecs_type_info_entry_t entries[2] = {
        {BIT_POSITION, sizeof(Position), &pos, 0},
        {BIT_VELOCITY, sizeof(Velocity), &vel, 1}
    };

    printf("Spawning entity...\n");
    freecs_entity_t entity = freecs_spawn(&world, BIT_POSITION | BIT_VELOCITY, entries, 2);
    printf("Entity spawned: id=%u gen=%u\n", entity.id, entity.generation);

    if (entity.id != 0) {
        printf("FAIL: entity.id expected 0, got %u\n", entity.id);
        return 1;
    }
    printf("PASS: spawn_entity\n");

    printf("Getting position...\n");
    Position* got_pos = (Position*)freecs_get(&world, entity, BIT_POSITION);
    if (got_pos == NULL) {
        printf("FAIL: Position is NULL\n");
        return 1;
    }
    if (fabsf(got_pos->x - 1.0f) > 0.001f || fabsf(got_pos->y - 2.0f) > 0.001f) {
        printf("FAIL: Position expected (1,2), got (%f,%f)\n", got_pos->x, got_pos->y);
        return 1;
    }
    printf("PASS: get_component position\n");

    Velocity* got_vel = (Velocity*)freecs_get(&world, entity, BIT_VELOCITY);
    if (got_vel == NULL) {
        printf("FAIL: Velocity is NULL\n");
        return 1;
    }
    if (fabsf(got_vel->x - 3.0f) > 0.001f || fabsf(got_vel->y - 4.0f) > 0.001f) {
        printf("FAIL: Velocity expected (3,4), got (%f,%f)\n", got_vel->x, got_vel->y);
        return 1;
    }
    printf("PASS: get_component velocity\n");

    printf("Testing despawn...\n");
    Position pos2 = {5, 6};
    freecs_type_info_entry_t entries2[1] = {{BIT_POSITION, sizeof(Position), &pos2, 0}};
    freecs_entity_t entity2 = freecs_spawn(&world, BIT_POSITION, entries2, 1);

    if (freecs_entity_count(&world) != 2) {
        printf("FAIL: entity count expected 2, got %zu\n", freecs_entity_count(&world));
        return 1;
    }

    freecs_despawn(&world, entity);

    if (freecs_entity_count(&world) != 1) {
        printf("FAIL: entity count after despawn expected 1, got %zu\n", freecs_entity_count(&world));
        return 1;
    }

    if (freecs_is_alive(&world, entity)) {
        printf("FAIL: entity should not be alive after despawn\n");
        return 1;
    }

    if (!freecs_is_alive(&world, entity2)) {
        printf("FAIL: entity2 should still be alive\n");
        return 1;
    }
    printf("PASS: despawn\n");

    printf("Destroying world...\n");
    freecs_destroy_world(&world);
    printf("All tests passed!\n");

    return 0;
}
