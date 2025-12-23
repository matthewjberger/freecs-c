#ifndef FREECS_H
#define FREECS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define FREECS_MAX_COMPONENTS 64
#define FREECS_MIN_ENTITY_CAPACITY 64

typedef struct {
    uint32_t id;
    uint32_t generation;
} freecs_entity_t;

#define FREECS_ENTITY_NIL ((freecs_entity_t){0, 0})

typedef struct {
    uint32_t archetype_index;
    uint32_t row;
    uint32_t generation;
    bool alive;
} freecs_entity_location_t;

typedef struct {
    uint8_t* data;
    size_t data_len;
    size_t data_cap;
    size_t elem_size;
    uint64_t bit;
    size_t type_index;
} freecs_component_column_t;

typedef struct {
    int32_t add_edges[FREECS_MAX_COMPONENTS];
    int32_t remove_edges[FREECS_MAX_COMPONENTS];
} freecs_table_edges_t;

typedef struct {
    uint64_t mask;
    freecs_entity_t* entities;
    size_t entities_len;
    size_t entities_cap;
    freecs_component_column_t* columns;
    size_t columns_len;
    size_t columns_cap;
    int32_t column_bits[FREECS_MAX_COMPONENTS];
    freecs_table_edges_t edges;
} freecs_archetype_t;

typedef struct {
    size_t* indices;
    size_t len;
    size_t cap;
} freecs_index_array_t;

typedef struct {
    uint64_t key;
    freecs_index_array_t value;
} freecs_cache_entry_t;

typedef struct {
    freecs_entity_location_t* locations;
    size_t locations_len;
    size_t locations_cap;

    freecs_archetype_t* archetypes;
    size_t archetypes_len;
    size_t archetypes_cap;

    freecs_cache_entry_t* archetype_index;
    size_t archetype_index_len;
    size_t archetype_index_cap;

    size_t type_sizes[FREECS_MAX_COMPONENTS];

    freecs_entity_t* free_entities;
    size_t free_entities_len;
    size_t free_entities_cap;

    uint32_t next_entity_id;
    uint64_t next_bit;

    freecs_cache_entry_t* query_cache;
    size_t query_cache_len;
    size_t query_cache_cap;

    freecs_entity_t* despawn_queue;
    size_t despawn_queue_len;
    size_t despawn_queue_cap;
} freecs_world_t;

typedef struct {
    freecs_world_t* world;
    uint64_t mask;
    uint64_t exclude;
    size_t* indices;
    size_t indices_len;
    size_t current;
} freecs_table_iterator_t;

typedef struct {
    freecs_archetype_t* archetype;
    size_t index;
} freecs_table_iterator_result_t;

typedef struct {
    uint64_t bit;
    size_t size;
    const void* data;
    size_t type_index;
} freecs_type_info_entry_t;

typedef struct {
    uint8_t* data;
    size_t data_len;
    size_t data_cap;
    size_t elem_size;
} freecs_event_queue_t;

typedef struct {
    freecs_entity_t entity;
    uint64_t mask;
} freecs_tag_entry_t;

typedef struct {
    freecs_tag_entry_t* entries;
    size_t entries_len;
    size_t entries_cap;
} freecs_tag_storage_t;

#define FREECS_MAX_TAGS 64

typedef struct {
    freecs_tag_storage_t storage[FREECS_MAX_TAGS];
    int next_tag;
} freecs_tags_t;

typedef enum {
    FREECS_CMD_SPAWN,
    FREECS_CMD_DESPAWN,
    FREECS_CMD_ADD_COMPONENTS,
    FREECS_CMD_REMOVE_COMPONENTS
} freecs_command_type_t;

typedef struct {
    freecs_command_type_t command_type;
    freecs_entity_t entity;
    uint64_t mask;
    uint8_t* component_data;
    size_t component_data_len;
    size_t component_data_cap;
    size_t* component_sizes;
    size_t component_sizes_len;
    uint64_t* component_bits;
    size_t component_bits_len;
} freecs_command_t;

typedef struct {
    freecs_command_t* commands;
    size_t commands_len;
    size_t commands_cap;
    freecs_world_t* world;
} freecs_command_buffer_t;

freecs_world_t freecs_create_world(void);
void freecs_destroy_world(freecs_world_t* world);

uint64_t freecs_register_component(freecs_world_t* world, size_t size);

freecs_entity_t freecs_spawn(freecs_world_t* world, uint64_t mask, const freecs_type_info_entry_t* entries, size_t entry_count);
freecs_entity_t* freecs_spawn_batch(freecs_world_t* world, uint64_t mask, size_t count, size_t* out_count);
freecs_entity_t* freecs_spawn_with_init(freecs_world_t* world, uint64_t mask, size_t count, void (*init_callback)(freecs_archetype_t*, size_t), size_t* out_count);
bool freecs_despawn(freecs_world_t* world, freecs_entity_t entity);
size_t freecs_despawn_batch(freecs_world_t* world, const freecs_entity_t* entities, size_t count);

bool freecs_is_alive(freecs_world_t* world, freecs_entity_t entity);

void* freecs_get(freecs_world_t* world, freecs_entity_t entity, uint64_t bit);
void* freecs_get_unchecked(freecs_world_t* world, freecs_entity_t entity, uint64_t bit);
bool freecs_set(freecs_world_t* world, freecs_entity_t entity, uint64_t bit, const void* value, size_t size);
bool freecs_has(freecs_world_t* world, freecs_entity_t entity, uint64_t bit);
bool freecs_has_components(freecs_world_t* world, freecs_entity_t entity, uint64_t mask);
uint64_t freecs_component_mask(freecs_world_t* world, freecs_entity_t entity, bool* ok);

bool freecs_add_component(freecs_world_t* world, freecs_entity_t entity, uint64_t bit, const void* value, size_t size);
bool freecs_remove_component(freecs_world_t* world, freecs_entity_t entity, uint64_t bit);

size_t* freecs_get_matching_archetypes(freecs_world_t* world, uint64_t mask, uint64_t exclude, size_t* out_count);
size_t freecs_query_count(freecs_world_t* world, uint64_t mask, uint64_t exclude);
freecs_entity_t* freecs_query_entities(freecs_world_t* world, uint64_t mask, uint64_t exclude, size_t* out_count);
freecs_entity_t freecs_query_first(freecs_world_t* world, uint64_t mask, uint64_t exclude, bool* found);
size_t freecs_entity_count(freecs_world_t* world);

void* freecs_column(freecs_archetype_t* arch, uint64_t bit, size_t* out_count);
void* freecs_column_unchecked(freecs_archetype_t* arch, uint64_t bit);

freecs_table_iterator_t freecs_table_iterator(freecs_world_t* world, uint64_t mask, uint64_t exclude);
bool freecs_table_iterator_next(freecs_table_iterator_t* iter, freecs_table_iterator_result_t* result);

void freecs_for_each(freecs_world_t* world, uint64_t mask, uint64_t exclude, void (*callback)(freecs_archetype_t*, size_t));
void freecs_for_each_table(freecs_world_t* world, uint64_t mask, uint64_t exclude, void (*callback)(freecs_archetype_t*));

void freecs_queue_despawn(freecs_world_t* world, freecs_entity_t entity);
void freecs_apply_despawns(freecs_world_t* world);

freecs_command_buffer_t freecs_create_command_buffer(freecs_world_t* world);
void freecs_destroy_command_buffer(freecs_command_buffer_t* buffer);
void freecs_clear_command_buffer(freecs_command_buffer_t* buffer);
void freecs_queue_spawn(freecs_command_buffer_t* buffer, uint64_t mask, const freecs_type_info_entry_t* entries, size_t entry_count);
void freecs_cmd_queue_despawn(freecs_command_buffer_t* buffer, freecs_entity_t entity);
void freecs_queue_add_components(freecs_command_buffer_t* buffer, freecs_entity_t entity, uint64_t mask);
void freecs_queue_remove_components(freecs_command_buffer_t* buffer, freecs_entity_t entity, uint64_t mask);
void freecs_apply_commands(freecs_command_buffer_t* buffer);

freecs_tags_t freecs_create_tags(void);
void freecs_destroy_tags(freecs_tags_t* tags);
int freecs_register_tag(freecs_tags_t* tags, const char* name);
void freecs_add_tag(freecs_tags_t* tags, int tag_id, freecs_entity_t entity);
void freecs_remove_tag(freecs_tags_t* tags, int tag_id, freecs_entity_t entity);
bool freecs_has_tag(freecs_tags_t* tags, int tag_id, freecs_entity_t entity);
freecs_entity_t* freecs_query_tag(freecs_tags_t* tags, int tag_id, size_t* out_count);
size_t freecs_tag_count(freecs_tags_t* tags, int tag_id);
void freecs_clear_entity_tags(freecs_tags_t* tags, freecs_entity_t entity);

freecs_event_queue_t freecs_create_event_queue(size_t elem_size);
void freecs_destroy_event_queue(freecs_event_queue_t* queue);
void freecs_send_event(freecs_event_queue_t* queue, const void* event);
void* freecs_read_events(freecs_event_queue_t* queue, size_t* out_count);
void freecs_clear_events(freecs_event_queue_t* queue);
size_t freecs_event_count(freecs_event_queue_t* queue);

static inline size_t freecs_bit_index(uint64_t bit) {
    size_t count = 0;
    while ((bit & 1) == 0) {
        bit >>= 1;
        count++;
    }
    return count;
}

#define FREECS_REGISTER(world, type) freecs_register_component(world, sizeof(type))

#define FREECS_GET(world, entity, type, bit) ((type*)freecs_get(world, entity, bit))

#define FREECS_SET(world, entity, type, bit, value) \
    do { \
        type _val = (value); \
        freecs_set(world, entity, bit, &_val, sizeof(type)); \
    } while(0)

#define FREECS_ADD(world, entity, type, bit, value) \
    do { \
        type _val = (value); \
        freecs_add_component(world, entity, bit, &_val, sizeof(type)); \
    } while(0)

#define FREECS_COLUMN(arch, type, bit) ((type*)freecs_column_unchecked(arch, bit))

#define FREECS_CREATE_EVENT_QUEUE(type) freecs_create_event_queue(sizeof(type))

#define FREECS_SEND_EVENT(queue, type, event) \
    do { \
        type _ev = (event); \
        freecs_send_event(queue, &_ev); \
    } while(0)

#define FREECS_READ_EVENTS(queue, type, out_count) ((type*)freecs_read_events(queue, out_count))

#endif
