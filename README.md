# freECS-C

A high-performance, archetype-based Entity Component System (ECS) for C.

**Key Features**:

- Archetype-based storage with bitmask queries
- Generational entity handles (prevents ABA problem)
- Contiguous component storage for cache-friendly iteration
- O(1) bit indexing via count trailing zeros
- Query caching for repeated iteration patterns
- `freecs_column_unchecked` for zero-overhead inner loops
- Batch spawning with pre-allocated capacity
- Command buffers for deferred structural changes
- Sparse set tags that don't fragment archetypes
- Event queues for decoupled communication
- Simple, data-oriented API

This is a C port of [freecs](https://github.com/matthewjberger/freecs), a Rust ECS library.

## Quick Start

Copy `freecs.h` and `freecs.c` into your project:

```c
#include "freecs.h"
#include <stdio.h>

typedef struct { float x, y; } Position;
typedef struct { float x, y; } Velocity;

static uint64_t BIT_POSITION;
static uint64_t BIT_VELOCITY;

int main(void) {
    freecs_world_t world = freecs_create_world();

    // Register component types (returns bitmask for queries)
    BIT_POSITION = FREECS_REGISTER(&world, Position);
    BIT_VELOCITY = FREECS_REGISTER(&world, Velocity);

    // Spawn entity with components
    Position pos = {1.0f, 2.0f};
    Velocity vel = {3.0f, 4.0f};
    freecs_type_info_entry_t entries[] = {
        {BIT_POSITION, sizeof(Position), &pos, freecs_bit_index(BIT_POSITION)},
        {BIT_VELOCITY, sizeof(Velocity), &vel, freecs_bit_index(BIT_VELOCITY)}
    };
    freecs_entity_t entity = freecs_spawn(&world, BIT_POSITION | BIT_VELOCITY, entries, 2);

    // Get components
    Position* p = FREECS_GET(&world, entity, Position, BIT_POSITION);
    if (p != NULL) {
        printf("Position: (%f, %f)\n", p->x, p->y);
    }

    // Set components
    FREECS_SET(&world, entity, Position, BIT_POSITION, ((Position){10.0f, 20.0f}));

    // Check if entity has a component
    if (freecs_has(&world, entity, BIT_POSITION)) {
        printf("Entity has position\n");
    }

    // Despawn entities
    freecs_despawn(&world, entity);

    freecs_destroy_world(&world);
    return 0;
}
```

## Systems

Systems iterate over archetypes and process entities with matching components:

```c
void update_positions(freecs_world_t* world, float dt) {
    uint64_t move_mask = BIT_POSITION | BIT_VELOCITY;

    // Cached query - archetypes matching this mask are remembered
    size_t matching_count;
    size_t* matching = freecs_get_matching_archetypes(world, move_mask, 0, &matching_count);

    for (size_t i = 0; i < matching_count; i++) {
        freecs_archetype_t* arch = &world->archetypes[matching[i]];

        // Get typed slices - O(1) lookup via bit index
        Position* positions = FREECS_COLUMN(arch, Position, BIT_POSITION);
        Velocity* velocities = FREECS_COLUMN(arch, Velocity, BIT_VELOCITY);

        // Process all entities in this archetype
        for (size_t j = 0; j < arch->entities_len; j++) {
            positions[j].x += velocities[j].x * dt;
            positions[j].y += velocities[j].y * dt;
        }
    }
}
```

### Column Access

Two methods are available:

```c
// Fast path - O(1) via bit index array lookup
Position* positions = FREECS_COLUMN(arch, Position, BIT_POSITION);

// Function call with count output
size_t count;
Position* positions = (Position*)freecs_column(arch, BIT_POSITION, &count);
```

Use the macro version in performance-critical code.

### Batch Spawning

Spawn many entities efficiently with pre-allocated capacity:

```c
// Spawns 1000 entities with component defaults (zeroed)
size_t count;
freecs_entity_t* entities = freecs_spawn_batch(&world, BIT_POSITION | BIT_VELOCITY, 1000, &count);
free(entities);  // Caller owns the returned array

// Spawn with custom initialization callback
freecs_entity_t* entities = freecs_spawn_with_init(&world, mask, 1000, init_callback, &count);
```

### Table Iterator

Use the table iterator for cleaner archetype traversal:

```c
freecs_table_iterator_t iter = freecs_table_iterator(&world, BIT_POSITION | BIT_VELOCITY, 0);
freecs_table_iterator_result_t result;
while (freecs_table_iterator_next(&iter, &result)) {
    freecs_archetype_t* arch = result.archetype;
    Position* positions = FREECS_COLUMN(arch, Position, BIT_POSITION);
    Velocity* velocities = FREECS_COLUMN(arch, Velocity, BIT_VELOCITY);
    // ...
}
```

### For-Each Callbacks

Process entities with callback functions:

```c
// Per-entity callback
void process_entity(freecs_archetype_t* arch, size_t row) {
    Position* positions = FREECS_COLUMN(arch, Position, BIT_POSITION);
    positions[row].x += 1.0f;
}
freecs_for_each(&world, BIT_POSITION, 0, process_entity);

// Per-table callback
void process_table(freecs_archetype_t* arch) {
    Position* positions = FREECS_COLUMN(arch, Position, BIT_POSITION);
    for (size_t i = 0; i < arch->entities_len; i++) {
        positions[i].x += 1.0f;
    }
}
freecs_for_each_table(&world, BIT_POSITION, 0, process_table);
```

## API Reference

### World Management

```c
freecs_world_t world = freecs_create_world();  // Create a new world
freecs_destroy_world(&world);                   // Clean up world resources
size_t count = freecs_entity_count(&world);     // Get total entity count
```

### Component Registration

```c
// Register returns a bitmask for the component type
uint64_t BIT_POSITION = FREECS_REGISTER(&world, Position);
uint64_t BIT_VELOCITY = FREECS_REGISTER(&world, Velocity);

// Use masks for queries
uint64_t MOVABLE = BIT_POSITION | BIT_VELOCITY;
```

### Entity Operations

```c
// Spawn with components
freecs_entity_t entity = freecs_spawn(&world, mask, entries, entry_count);

// Check if entity is alive
if (freecs_is_alive(&world, entity)) { ... }

// Despawn entity (slot reused with new generation)
freecs_despawn(&world, entity);

// Batch despawn
size_t despawned = freecs_despawn_batch(&world, entities, count);

// Queue despawn for deferred removal
freecs_queue_despawn(&world, entity);
freecs_apply_despawns(&world);
```

### Component Access

```c
// Get component (returns NULL if not present)
Position* pos = FREECS_GET(&world, entity, Position, BIT_POSITION);

// Get unchecked (when you know the component exists)
Position* pos = (Position*)freecs_get_unchecked(&world, entity, BIT_POSITION);

// Set component value
FREECS_SET(&world, entity, Position, BIT_POSITION, ((Position){10, 20}));

// Check if entity has component
if (freecs_has(&world, entity, BIT_POSITION)) { ... }

// Check multiple components
if (freecs_has_components(&world, entity, BIT_POSITION | BIT_VELOCITY)) { ... }

// Get entity's component mask
bool ok;
uint64_t mask = freecs_component_mask(&world, entity, &ok);
```

### Adding/Removing Components

```c
// Add component to existing entity (moves to new archetype)
Velocity vel = {1.0f, 0.0f};
freecs_add_component(&world, entity, BIT_VELOCITY, &vel, sizeof(Velocity));

// Or use the macro
FREECS_ADD(&world, entity, Velocity, BIT_VELOCITY, ((Velocity){1.0f, 0.0f}));

// Remove component from entity
freecs_remove_component(&world, entity, BIT_VELOCITY);
```

### Query Operations

```c
// Count entities matching query
size_t count = freecs_query_count(&world, BIT_POSITION | BIT_VELOCITY, 0);

// Get first entity matching query
bool found;
freecs_entity_t first = freecs_query_first(&world, BIT_POSITION, 0, &found);

// Get all entities matching query
size_t entity_count;
freecs_entity_t* entities = freecs_query_entities(&world, BIT_POSITION, 0, &entity_count);
free(entities);  // Caller owns the returned array
```

## Command Buffers

Queue structural changes for deferred execution:

```c
freecs_command_buffer_t buffer = freecs_create_command_buffer(&world);

// Queue spawns
freecs_queue_spawn(&buffer, mask, entries, entry_count);

// Queue despawns
freecs_cmd_queue_despawn(&buffer, entity);

// Queue component changes
freecs_queue_add_components(&buffer, entity, BIT_HEALTH);
freecs_queue_remove_components(&buffer, entity, BIT_VELOCITY);

// Apply all queued commands
freecs_apply_commands(&buffer);

freecs_destroy_command_buffer(&buffer);
```

## Tags

Sparse set tags for lightweight markers that don't fragment archetypes:

```c
freecs_tags_t tags = freecs_create_tags();

int TAG_PLAYER = freecs_register_tag(&tags, "player");
int TAG_ENEMY = freecs_register_tag(&tags, "enemy");

// Add/remove tags
freecs_add_tag(&tags, TAG_PLAYER, entity);
freecs_remove_tag(&tags, TAG_PLAYER, entity);

// Check if entity has tag
if (freecs_has_tag(&tags, TAG_PLAYER, entity)) { ... }

// Query entities by tag
size_t count;
freecs_entity_t* players = freecs_query_tag(&tags, TAG_PLAYER, &count);
free(players);

freecs_destroy_tags(&tags);
```

## Events

Event queues for decoupled communication between systems:

```c
typedef struct {
    freecs_entity_t entity_a;
    freecs_entity_t entity_b;
} CollisionEvent;

freecs_event_queue_t collision_events = FREECS_CREATE_EVENT_QUEUE(CollisionEvent);

// Send events
FREECS_SEND_EVENT(&collision_events, CollisionEvent,
    ((CollisionEvent){entity_a, entity_b}));

// Process events
size_t count;
CollisionEvent* events = FREECS_READ_EVENTS(&collision_events, CollisionEvent, &count);
for (size_t i = 0; i < count; i++) {
    // Handle collision
}

// Clear events after processing
freecs_clear_events(&collision_events);

freecs_destroy_event_queue(&collision_events);
```

## Examples

### Boids Simulation

See `examples/boids.c` for a complete boids flocking simulation using raylib:

```bash
make boids
./boids
```

Controls:
- **Space**: Pause/unpause
- **+/-**: Add/remove 1000 boids
- **Arrow keys**: Adjust alignment/cohesion weights
- **Left mouse**: Attract boids
- **Right mouse**: Repel boids

### Tower Defense

See `examples/tower_defense.c` for a complete tower defense game using raylib:

```bash
make tower_defense
./tower_defense
```

## Running Tests

```bash
make tests
./tests
```

All 14 tests verify:
- Entity spawn/despawn
- Component get/set/has
- Generational indices
- Archetype management
- Query iteration
- Batch operations
- Tags and events

## Building

The library is just two files: `freecs.h` and `freecs.c`. Copy them into your project and compile:

```bash
gcc -Wall -Wextra -std=c11 -O2 -c freecs.c -o freecs.o
```

Or use the provided Makefile.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE.md) file for details.
