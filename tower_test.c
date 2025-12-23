#include "freecs.h"
#include <stdio.h>

typedef struct { float x; float y; } Position;
typedef struct { float x; float y; } Velocity;
typedef struct { float value; } Health;

static uint64_t BIT_POSITION;
static uint64_t BIT_VELOCITY;
static uint64_t BIT_HEALTH;

int main(void) {
    printf("Creating world...\n");
    fflush(stdout);
    freecs_world_t world = freecs_create_world();

    printf("Registering components...\n");
    fflush(stdout);
    BIT_POSITION = FREECS_REGISTER(&world, Position);
    BIT_VELOCITY = FREECS_REGISTER(&world, Velocity);
    BIT_HEALTH = FREECS_REGISTER(&world, Health);
    printf("Components registered\n");
    fflush(stdout);

    printf("Spawning 30 entities...\n");
    fflush(stdout);
    for (int i = 0; i < 30; i++) {
        Position pos = {(float)i, (float)i};
        freecs_type_info_entry_t entries[1] = {
            {BIT_POSITION, sizeof(Position), &pos, freecs_bit_index(BIT_POSITION)}
        };
        printf("%d ", i);
        fflush(stdout);
        freecs_spawn(&world, BIT_POSITION, entries, 1);
    }
    printf("\nDone spawning\n");
    fflush(stdout);

    printf("Destroying world...\n");
    fflush(stdout);
    freecs_destroy_world(&world);
    printf("Done!\n");
    fflush(stdout);
    return 0;
}
