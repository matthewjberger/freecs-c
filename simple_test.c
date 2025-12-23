#include "freecs.h"
#include <stdio.h>

int main(void) {
    printf("Creating world...\n");
    freecs_world_t world = freecs_create_world();
    printf("World created\n");
    
    printf("Registering Position...\n");
    uint64_t BIT_POSITION = freecs_register_component(&world, sizeof(float) * 2);
    printf("Position registered: bit=%llu\n", (unsigned long long)BIT_POSITION);
    
    printf("Type size at index 0: %zu\n", world.type_sizes[0]);
    
    float pos[2] = {1.0f, 2.0f};
    freecs_type_info_entry_t entries[1] = {
        {BIT_POSITION, sizeof(pos), &pos, 0}
    };
    
    printf("Spawning entity...\n");
    freecs_entity_t entity = freecs_spawn(&world, BIT_POSITION, entries, 1);
    printf("Entity spawned: id=%u, gen=%u\n", entity.id, entity.generation);
    
    printf("Destroying world...\n");
    freecs_destroy_world(&world);
    printf("Done!\n");
    
    return 0;
}
