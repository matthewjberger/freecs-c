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
    printf("Starting...\n");
    
    freecs_world_t world = freecs_create_world();
    printf("World created\n");
    
    BIT_POSITION = freecs_register_component(&world, sizeof(Position));
    BIT_VELOCITY = freecs_register_component(&world, sizeof(Velocity));
    printf("Components registered: P=%llu V=%llu\n", (unsigned long long)BIT_POSITION, (unsigned long long)BIT_VELOCITY);
    
    Position pos = {1, 2};
    Velocity vel = {3, 4};
    freecs_type_info_entry_t entries[2] = {
        {BIT_POSITION, sizeof(Position), &pos, 0},
        {BIT_VELOCITY, sizeof(Velocity), &vel, 1}
    };
    
    printf("Spawning entity...\n");
    freecs_entity_t entity = freecs_spawn(&world, BIT_POSITION | BIT_VELOCITY, entries, 2);
    printf("Entity spawned: id=%u gen=%u\n", entity.id, entity.generation);
    
    printf("Getting position...\n");
    Position* got_pos = (Position*)freecs_get(&world, entity, BIT_POSITION);
    printf("Position ptr: %p\n", (void*)got_pos);
    if (got_pos) {
        printf("Position: x=%f y=%f\n", got_pos->x, got_pos->y);
    }
    
    printf("Destroying world...\n");
    freecs_destroy_world(&world);
    printf("Done!\n");
    
    return 0;
}
