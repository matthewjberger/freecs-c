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

#ifdef FREECS_IMPLEMENTATION

#include <stdlib.h>
#include <string.h>

static void freecs__ensure_capacity_u8(uint8_t** data, size_t* cap, size_t needed) {
    if (needed <= *cap) return;
    size_t new_cap = *cap == 0 ? 16 : *cap * 2;
    while (new_cap < needed) new_cap *= 2;
    *data = realloc(*data, new_cap);
    *cap = new_cap;
}

static void freecs__ensure_capacity_entities(freecs_entity_t** data, size_t* cap, size_t needed) {
    if (needed <= *cap) return;
    size_t new_cap = *cap == 0 ? 16 : *cap * 2;
    while (new_cap < needed) new_cap *= 2;
    *data = realloc(*data, new_cap * sizeof(freecs_entity_t));
    *cap = new_cap;
}

static void freecs__ensure_capacity_locations(freecs_entity_location_t** data, size_t* cap, size_t needed) {
    if (needed <= *cap) return;
    size_t new_cap = *cap == 0 ? FREECS_MIN_ENTITY_CAPACITY : *cap * 2;
    while (new_cap < needed) new_cap *= 2;
    *data = realloc(*data, new_cap * sizeof(freecs_entity_location_t));
    *cap = new_cap;
}

static void freecs__ensure_capacity_archetypes(freecs_archetype_t** data, size_t* cap, size_t needed) {
    if (needed <= *cap) return;
    size_t new_cap = *cap == 0 ? 16 : *cap * 2;
    while (new_cap < needed) new_cap *= 2;
    *data = realloc(*data, new_cap * sizeof(freecs_archetype_t));
    *cap = new_cap;
}

static void freecs__ensure_capacity_columns(freecs_component_column_t** data, size_t* cap, size_t needed) {
    if (needed <= *cap) return;
    size_t new_cap = *cap == 0 ? 8 : *cap * 2;
    while (new_cap < needed) new_cap *= 2;
    *data = realloc(*data, new_cap * sizeof(freecs_component_column_t));
    *cap = new_cap;
}

static void freecs__ensure_capacity_indices(size_t** data, size_t* cap, size_t needed) {
    if (needed <= *cap) return;
    size_t new_cap = *cap == 0 ? 16 : *cap * 2;
    while (new_cap < needed) new_cap *= 2;
    *data = realloc(*data, new_cap * sizeof(size_t));
    *cap = new_cap;
}

static void freecs__ensure_capacity_cache(freecs_cache_entry_t** data, size_t* cap, size_t needed) {
    if (needed <= *cap) return;
    size_t new_cap = *cap == 0 ? 16 : *cap * 2;
    while (new_cap < needed) new_cap *= 2;
    *data = realloc(*data, new_cap * sizeof(freecs_cache_entry_t));
    *cap = new_cap;
}

static void freecs__ensure_capacity_commands(freecs_command_t** data, size_t* cap, size_t needed) {
    if (needed <= *cap) return;
    size_t new_cap = *cap == 0 ? 16 : *cap * 2;
    while (new_cap < needed) new_cap *= 2;
    *data = realloc(*data, new_cap * sizeof(freecs_command_t));
    *cap = new_cap;
}

static void freecs__ensure_capacity_tag_entries(freecs_tag_entry_t** data, size_t* cap, size_t needed) {
    if (needed <= *cap) return;
    size_t new_cap = *cap == 0 ? 16 : *cap * 2;
    while (new_cap < needed) new_cap *= 2;
    *data = realloc(*data, new_cap * sizeof(freecs_tag_entry_t));
    *cap = new_cap;
}

static size_t freecs__cache_find(freecs_cache_entry_t* cache, size_t len, uint64_t key) {
    for (size_t index = 0; index < len; index++) {
        if (cache[index].key == key) return index;
    }
    return (size_t)-1;
}

freecs_world_t freecs_create_world(void) {
    freecs_world_t world = {0};
    world.next_bit = 1;
    return world;
}

void freecs_destroy_world(freecs_world_t* world) {
    for (size_t index = 0; index < world->archetypes_len; index++) {
        freecs_archetype_t* arch = &world->archetypes[index];
        for (size_t column_index = 0; column_index < arch->columns_len; column_index++) {
            free(arch->columns[column_index].data);
        }
        free(arch->columns);
        free(arch->entities);
    }
    free(world->archetypes);
    free(world->locations);
    free(world->free_entities);
    for (size_t index = 0; index < world->archetype_index_len; index++) {
        free(world->archetype_index[index].value.indices);
    }
    free(world->archetype_index);
    for (size_t index = 0; index < world->query_cache_len; index++) {
        free(world->query_cache[index].value.indices);
    }
    free(world->query_cache);
    free(world->despawn_queue);
    memset(world, 0, sizeof(*world));
}

uint64_t freecs_register_component(freecs_world_t* world, size_t size) {
    uint64_t bit = world->next_bit;
    world->next_bit <<= 1;
    world->type_sizes[freecs_bit_index(bit)] = size;
    return bit;
}

static void freecs__ensure_entity_slot(freecs_world_t* world, uint32_t id) {
    if (world->locations_len > id) return;

    freecs__ensure_capacity_locations(&world->locations, &world->locations_cap, (size_t)id + 1);
    while (world->locations_len <= id) {
        world->locations[world->locations_len] = (freecs_entity_location_t){0, 0, 0, false};
        world->locations_len++;
    }
}

static freecs_entity_t freecs__alloc_entity(freecs_world_t* world) {
    if (world->free_entities_len > 0) {
        return world->free_entities[--world->free_entities_len];
    }

    uint32_t id = world->next_entity_id++;
    freecs__ensure_entity_slot(world, id);

    return (freecs_entity_t){id, 0};
}

static size_t freecs__find_or_create_archetype(freecs_world_t* world, uint64_t mask, const freecs_type_info_entry_t* type_info, size_t type_info_count) {
    size_t idx = freecs__cache_find(world->archetype_index, world->archetype_index_len, mask);
    if (idx != (size_t)-1) {
        return world->archetype_index[idx].value.indices[0];
    }

    size_t arch_idx = world->archetypes_len;
    freecs__ensure_capacity_archetypes(&world->archetypes, &world->archetypes_cap, arch_idx + 1);

    freecs_archetype_t* arch = &world->archetypes[arch_idx];
    memset(arch, 0, sizeof(*arch));
    arch->mask = mask;

    for (size_t index = 0; index < FREECS_MAX_COMPONENTS; index++) {
        arch->column_bits[index] = -1;
        arch->edges.add_edges[index] = -1;
        arch->edges.remove_edges[index] = -1;
    }

    for (size_t index = 0; index < type_info_count; index++) {
        size_t col_idx = arch->columns_len;
        freecs__ensure_capacity_columns(&arch->columns, &arch->columns_cap, col_idx + 1);

        freecs_component_column_t* col = &arch->columns[col_idx];
        memset(col, 0, sizeof(*col));
        col->elem_size = type_info[index].size;
        col->bit = type_info[index].bit;
        col->type_index = type_info[index].type_index;

        arch->column_bits[freecs_bit_index(type_info[index].bit)] = (int32_t)col_idx;
        arch->columns_len++;
    }

    world->archetypes_len++;

    freecs__ensure_capacity_cache(&world->archetype_index, &world->archetype_index_cap, world->archetype_index_len + 1);
    freecs_cache_entry_t* entry = &world->archetype_index[world->archetype_index_len++];
    entry->key = mask;
    entry->value.indices = malloc(sizeof(size_t));
    entry->value.indices[0] = arch_idx;
    entry->value.len = 1;
    entry->value.cap = 1;

    for (size_t index = 0; index < world->query_cache_len; index++) {
        uint64_t query_mask = world->query_cache[index].key & 0xFFFFFFFF;
        if ((mask & query_mask) == query_mask) {
            freecs_index_array_t* cached = &world->query_cache[index].value;
            freecs__ensure_capacity_indices(&cached->indices, &cached->cap, cached->len + 1);
            cached->indices[cached->len++] = arch_idx;
        }
    }

    for (size_t comp_bit_index = 0; comp_bit_index < FREECS_MAX_COMPONENTS; comp_bit_index++) {
        uint64_t comp_mask = (uint64_t)1 << comp_bit_index;
        if (world->type_sizes[comp_bit_index] == 0) continue;

        for (size_t existing_idx = 0; existing_idx < world->archetypes_len; existing_idx++) {
            freecs_archetype_t* existing = &world->archetypes[existing_idx];
            if ((existing->mask | comp_mask) == mask) {
                existing->edges.add_edges[comp_bit_index] = (int32_t)arch_idx;
            }
            if ((existing->mask & ~comp_mask) == mask) {
                existing->edges.remove_edges[comp_bit_index] = (int32_t)arch_idx;
            }
        }
    }

    return arch_idx;
}

freecs_entity_t freecs_spawn(freecs_world_t* world, uint64_t mask, const freecs_type_info_entry_t* entries, size_t entry_count) {
    if (entry_count == 0 || mask == 0) {
        return FREECS_ENTITY_NIL;
    }

    size_t arch_idx = freecs__find_or_create_archetype(world, mask, entries, entry_count);
    freecs_archetype_t* arch = &world->archetypes[arch_idx];

    freecs_entity_t entity = freecs__alloc_entity(world);
    size_t row = arch->entities_len;

    freecs__ensure_capacity_entities(&arch->entities, &arch->entities_cap, row + 1);
    arch->entities[arch->entities_len++] = entity;

    for (size_t index = 0; index < entry_count; index++) {
        int32_t col_idx = arch->column_bits[freecs_bit_index(entries[index].bit)];
        if (col_idx >= 0) {
            freecs_component_column_t* col = &arch->columns[col_idx];
            size_t old_len = col->data_len;
            freecs__ensure_capacity_u8(&col->data, &col->data_cap, old_len + entries[index].size);
            col->data_len = old_len + entries[index].size;
            if (entries[index].data != NULL && entries[index].size > 0) {
                memcpy(&col->data[old_len], entries[index].data, entries[index].size);
            } else {
                memset(&col->data[old_len], 0, entries[index].size);
            }
        }
    }

    world->locations[entity.id] = (freecs_entity_location_t){
        .generation = entity.generation,
        .archetype_index = (uint32_t)arch_idx,
        .row = (uint32_t)row,
        .alive = true
    };

    return entity;
}

freecs_entity_t* freecs_spawn_batch(freecs_world_t* world, uint64_t mask, size_t count, size_t* out_count) {
    if (mask == 0 || count == 0) {
        *out_count = 0;
        return NULL;
    }

    freecs_type_info_entry_t type_info[FREECS_MAX_COMPONENTS];
    size_t info_count = 0;

    for (size_t bit_idx = 0; bit_idx < FREECS_MAX_COMPONENTS; bit_idx++) {
        uint64_t comp_bit = (uint64_t)1 << bit_idx;
        if ((mask & comp_bit) != 0) {
            size_t size = world->type_sizes[bit_idx];
            if (size > 0) {
                type_info[info_count].bit = comp_bit;
                type_info[info_count].size = size;
                type_info[info_count].data = NULL;
                type_info[info_count].type_index = bit_idx;
                info_count++;
            }
        }
    }

    if (info_count == 0) {
        *out_count = 0;
        return NULL;
    }

    size_t arch_idx = freecs__find_or_create_archetype(world, mask, type_info, info_count);
    freecs_archetype_t* arch = &world->archetypes[arch_idx];

    size_t start_row = arch->entities_len;
    freecs__ensure_capacity_entities(&arch->entities, &arch->entities_cap, start_row + count);

    for (size_t column_index = 0; column_index < arch->columns_len; column_index++) {
        freecs_component_column_t* col = &arch->columns[column_index];
        freecs__ensure_capacity_u8(&col->data, &col->data_cap, col->data_len + count * col->elem_size);
    }

    freecs_entity_t* entities = malloc(count * sizeof(freecs_entity_t));

    for (size_t index = 0; index < count; index++) {
        freecs_entity_t entity = freecs__alloc_entity(world);
        entities[index] = entity;
        size_t row = start_row + index;
        arch->entities[arch->entities_len++] = entity;

        for (size_t column_index = 0; column_index < arch->columns_len; column_index++) {
            freecs_component_column_t* col = &arch->columns[column_index];
            size_t old_len = col->data_len;
            col->data_len = old_len + col->elem_size;
            memset(&col->data[old_len], 0, col->elem_size);
        }

        world->locations[entity.id] = (freecs_entity_location_t){
            .generation = entity.generation,
            .archetype_index = (uint32_t)arch_idx,
            .row = (uint32_t)row,
            .alive = true
        };
    }

    *out_count = count;
    return entities;
}

freecs_entity_t* freecs_spawn_with_init(freecs_world_t* world, uint64_t mask, size_t count, void (*init_callback)(freecs_archetype_t*, size_t), size_t* out_count) {
    freecs_entity_t* entities = freecs_spawn_batch(world, mask, count, out_count);
    if (entities == NULL || *out_count == 0) return entities;

    size_t idx = freecs__cache_find(world->archetype_index, world->archetype_index_len, mask);
    if (idx == (size_t)-1) return entities;

    size_t arch_idx = world->archetype_index[idx].value.indices[0];
    freecs_archetype_t* arch = &world->archetypes[arch_idx];
    size_t start_row = arch->entities_len - count;

    for (size_t index = 0; index < count; index++) {
        init_callback(arch, start_row + index);
    }

    return entities;
}

bool freecs_despawn(freecs_world_t* world, freecs_entity_t entity) {
    if (entity.id >= world->locations_len) return false;

    freecs_entity_location_t* loc = &world->locations[entity.id];
    if (!loc->alive || loc->generation != entity.generation) return false;

    freecs_archetype_t* arch = &world->archetypes[loc->archetype_index];
    size_t row = loc->row;
    size_t last_row = arch->entities_len - 1;

    if (row < last_row) {
        freecs_entity_t last_entity = arch->entities[last_row];
        arch->entities[row] = last_entity;
        world->locations[last_entity.id].row = (uint32_t)row;

        for (size_t column_index = 0; column_index < arch->columns_len; column_index++) {
            freecs_component_column_t* col = &arch->columns[column_index];
            if (col->elem_size > 0) {
                size_t src_start = last_row * col->elem_size;
                size_t dst_start = row * col->elem_size;
                memcpy(&col->data[dst_start], &col->data[src_start], col->elem_size);
            }
        }
    }

    arch->entities_len--;
    for (size_t column_index = 0; column_index < arch->columns_len; column_index++) {
        freecs_component_column_t* col = &arch->columns[column_index];
        if (col->elem_size > 0) {
            col->data_len -= col->elem_size;
        }
    }

    loc->alive = false;
    loc->generation++;

    freecs__ensure_capacity_entities(&world->free_entities, &world->free_entities_cap, world->free_entities_len + 1);
    world->free_entities[world->free_entities_len++] = (freecs_entity_t){entity.id, loc->generation};

    return true;
}

size_t freecs_despawn_batch(freecs_world_t* world, const freecs_entity_t* entities, size_t count) {
    size_t despawned = 0;
    for (size_t index = 0; index < count; index++) {
        if (freecs_despawn(world, entities[index])) {
            despawned++;
        }
    }
    return despawned;
}

bool freecs_is_alive(freecs_world_t* world, freecs_entity_t entity) {
    if (entity.id >= world->locations_len) return false;
    freecs_entity_location_t* loc = &world->locations[entity.id];
    return loc->alive && loc->generation == entity.generation;
}

void* freecs_get(freecs_world_t* world, freecs_entity_t entity, uint64_t bit) {
    if (entity.id >= world->locations_len) return NULL;

    freecs_entity_location_t* loc = &world->locations[entity.id];
    if (!loc->alive || loc->generation != entity.generation) return NULL;

    freecs_archetype_t* arch = &world->archetypes[loc->archetype_index];
    int32_t col_idx = arch->column_bits[freecs_bit_index(bit)];
    if (col_idx < 0) return NULL;

    freecs_component_column_t* col = &arch->columns[col_idx];
    size_t offset = loc->row * col->elem_size;
    return &col->data[offset];
}

void* freecs_get_unchecked(freecs_world_t* world, freecs_entity_t entity, uint64_t bit) {
    freecs_entity_location_t* loc = &world->locations[entity.id];
    freecs_archetype_t* arch = &world->archetypes[loc->archetype_index];
    int32_t col_idx = arch->column_bits[freecs_bit_index(bit)];
    freecs_component_column_t* col = &arch->columns[col_idx];
    size_t offset = loc->row * col->elem_size;
    return &col->data[offset];
}

bool freecs_set(freecs_world_t* world, freecs_entity_t entity, uint64_t bit, const void* value, size_t size) {
    void* ptr = freecs_get(world, entity, bit);
    if (ptr == NULL) return false;
    memcpy(ptr, value, size);
    return true;
}

bool freecs_has(freecs_world_t* world, freecs_entity_t entity, uint64_t bit) {
    if (entity.id >= world->locations_len) return false;

    freecs_entity_location_t* loc = &world->locations[entity.id];
    if (!loc->alive || loc->generation != entity.generation) return false;

    freecs_archetype_t* arch = &world->archetypes[loc->archetype_index];
    return (arch->mask & bit) != 0;
}

bool freecs_has_components(freecs_world_t* world, freecs_entity_t entity, uint64_t mask) {
    if (entity.id >= world->locations_len) return false;

    freecs_entity_location_t* loc = &world->locations[entity.id];
    if (!loc->alive || loc->generation != entity.generation) return false;

    freecs_archetype_t* arch = &world->archetypes[loc->archetype_index];
    return (arch->mask & mask) == mask;
}

uint64_t freecs_component_mask(freecs_world_t* world, freecs_entity_t entity, bool* ok) {
    if (entity.id >= world->locations_len) {
        *ok = false;
        return 0;
    }

    freecs_entity_location_t* loc = &world->locations[entity.id];
    if (!loc->alive || loc->generation != entity.generation) {
        *ok = false;
        return 0;
    }

    freecs_archetype_t* arch = &world->archetypes[loc->archetype_index];
    *ok = true;
    return arch->mask;
}

static void freecs__move_entity(freecs_world_t* world, freecs_entity_t entity, size_t from_arch_idx, size_t from_row, size_t to_arch_idx) {
    freecs_archetype_t* from_arch = &world->archetypes[from_arch_idx];
    freecs_archetype_t* to_arch = &world->archetypes[to_arch_idx];

    size_t new_row = to_arch->entities_len;
    freecs__ensure_capacity_entities(&to_arch->entities, &to_arch->entities_cap, new_row + 1);
    to_arch->entities[to_arch->entities_len++] = entity;

    for (size_t column_index = 0; column_index < to_arch->columns_len; column_index++) {
        freecs_component_column_t* to_col = &to_arch->columns[column_index];
        size_t old_len = to_col->data_len;
        freecs__ensure_capacity_u8(&to_col->data, &to_col->data_cap, old_len + to_col->elem_size);
        to_col->data_len = old_len + to_col->elem_size;

        int32_t from_col_idx = from_arch->column_bits[freecs_bit_index(to_col->bit)];
        if (from_col_idx >= 0) {
            freecs_component_column_t* from_col = &from_arch->columns[from_col_idx];
            size_t src_offset = from_row * from_col->elem_size;
            memcpy(&to_col->data[old_len], &from_col->data[src_offset], to_col->elem_size);
        } else {
            memset(&to_col->data[old_len], 0, to_col->elem_size);
        }
    }

    size_t last_row = from_arch->entities_len - 1;
    if (from_row < last_row) {
        freecs_entity_t last_entity = from_arch->entities[last_row];
        from_arch->entities[from_row] = last_entity;
        world->locations[last_entity.id].row = (uint32_t)from_row;

        for (size_t column_index = 0; column_index < from_arch->columns_len; column_index++) {
            freecs_component_column_t* col = &from_arch->columns[column_index];
            if (col->elem_size > 0) {
                size_t src_start = last_row * col->elem_size;
                size_t dst_start = from_row * col->elem_size;
                memcpy(&col->data[dst_start], &col->data[src_start], col->elem_size);
            }
        }
    }

    from_arch->entities_len--;
    for (size_t column_index = 0; column_index < from_arch->columns_len; column_index++) {
        freecs_component_column_t* col = &from_arch->columns[column_index];
        if (col->elem_size > 0) {
            col->data_len -= col->elem_size;
        }
    }

    world->locations[entity.id] = (freecs_entity_location_t){
        .generation = entity.generation,
        .archetype_index = (uint32_t)to_arch_idx,
        .row = (uint32_t)new_row,
        .alive = true
    };
}

bool freecs_add_component(freecs_world_t* world, freecs_entity_t entity, uint64_t bit, const void* value, size_t size) {
    if (entity.id >= world->locations_len) return false;

    freecs_entity_location_t* loc = &world->locations[entity.id];
    if (!loc->alive || loc->generation != entity.generation) return false;

    size_t bit_idx = freecs_bit_index(bit);
    freecs_archetype_t* arch = &world->archetypes[loc->archetype_index];

    if ((arch->mask & bit) != 0) {
        int32_t col_idx = arch->column_bits[bit_idx];
        freecs_component_column_t* col = &arch->columns[col_idx];
        size_t offset = loc->row * col->elem_size;
        memcpy(&col->data[offset], value, size);
        return true;
    }

    uint64_t new_mask = arch->mask | bit;
    int32_t target_arch_idx_signed = arch->edges.add_edges[bit_idx];

    if (target_arch_idx_signed < 0) {
        freecs_type_info_entry_t type_info[FREECS_MAX_COMPONENTS];
        size_t info_count = 0;

        for (size_t column_index = 0; column_index < arch->columns_len; column_index++) {
            freecs_component_column_t* col = &arch->columns[column_index];
            type_info[info_count].bit = col->bit;
            type_info[info_count].size = col->elem_size;
            type_info[info_count].data = NULL;
            type_info[info_count].type_index = col->type_index;
            info_count++;
        }
        type_info[info_count].bit = bit;
        type_info[info_count].size = size;
        type_info[info_count].data = NULL;
        type_info[info_count].type_index = bit_idx;
        info_count++;

        target_arch_idx_signed = (int32_t)freecs__find_or_create_archetype(world, new_mask, type_info, info_count);
        world->archetypes[loc->archetype_index].edges.add_edges[bit_idx] = target_arch_idx_signed;
    }

    size_t target_arch_idx = (size_t)target_arch_idx_signed;
    freecs__move_entity(world, entity, loc->archetype_index, loc->row, target_arch_idx);

    if (size > 0) {
        freecs_entity_location_t* new_loc = &world->locations[entity.id];
        freecs_archetype_t* to_arch = &world->archetypes[new_loc->archetype_index];
        int32_t col_idx = to_arch->column_bits[bit_idx];
        freecs_component_column_t* col = &to_arch->columns[col_idx];
        size_t offset = new_loc->row * col->elem_size;
        memcpy(&col->data[offset], value, size);
    }

    return true;
}

bool freecs_remove_component(freecs_world_t* world, freecs_entity_t entity, uint64_t bit) {
    if (entity.id >= world->locations_len) return false;

    freecs_entity_location_t* loc = &world->locations[entity.id];
    if (!loc->alive || loc->generation != entity.generation) return false;

    size_t bit_idx = freecs_bit_index(bit);
    freecs_archetype_t* arch = &world->archetypes[loc->archetype_index];

    if ((arch->mask & bit) == 0) return false;

    uint64_t new_mask = arch->mask & ~bit;

    if (new_mask == 0) {
        freecs_despawn(world, entity);
        return true;
    }

    int32_t target_arch_idx_signed = arch->edges.remove_edges[bit_idx];

    if (target_arch_idx_signed < 0) {
        freecs_type_info_entry_t type_info[FREECS_MAX_COMPONENTS];
        size_t info_count = 0;

        for (size_t column_index = 0; column_index < arch->columns_len; column_index++) {
            freecs_component_column_t* col = &arch->columns[column_index];
            if (col->bit != bit) {
                type_info[info_count].bit = col->bit;
                type_info[info_count].size = col->elem_size;
                type_info[info_count].data = NULL;
                type_info[info_count].type_index = col->type_index;
                info_count++;
            }
        }

        target_arch_idx_signed = (int32_t)freecs__find_or_create_archetype(world, new_mask, type_info, info_count);
        world->archetypes[loc->archetype_index].edges.remove_edges[bit_idx] = target_arch_idx_signed;
    }

    size_t target_arch_idx = (size_t)target_arch_idx_signed;
    freecs__move_entity(world, entity, loc->archetype_index, loc->row, target_arch_idx);

    return true;
}

size_t* freecs_get_matching_archetypes(freecs_world_t* world, uint64_t mask, uint64_t exclude, size_t* out_count) {
    uint64_t cache_key = mask | (exclude << 32);
    size_t idx = freecs__cache_find(world->query_cache, world->query_cache_len, cache_key);
    if (idx != (size_t)-1) {
        *out_count = world->query_cache[idx].value.len;
        return world->query_cache[idx].value.indices;
    }

    freecs_index_array_t matching = {0};

    for (size_t index = 0; index < world->archetypes_len; index++) {
        freecs_archetype_t* arch = &world->archetypes[index];
        if ((arch->mask & mask) == mask && (exclude == 0 || (arch->mask & exclude) == 0)) {
            freecs__ensure_capacity_indices(&matching.indices, &matching.cap, matching.len + 1);
            matching.indices[matching.len++] = index;
        }
    }

    freecs__ensure_capacity_cache(&world->query_cache, &world->query_cache_cap, world->query_cache_len + 1);
    world->query_cache[world->query_cache_len].key = cache_key;
    world->query_cache[world->query_cache_len].value = matching;
    world->query_cache_len++;

    *out_count = matching.len;
    return matching.indices;
}

size_t freecs_query_count(freecs_world_t* world, uint64_t mask, uint64_t exclude) {
    size_t count = 0;
    size_t matching_count;
    size_t* matching = freecs_get_matching_archetypes(world, mask, exclude, &matching_count);
    for (size_t index = 0; index < matching_count; index++) {
        count += world->archetypes[matching[index]].entities_len;
    }
    return count;
}

freecs_entity_t* freecs_query_entities(freecs_world_t* world, uint64_t mask, uint64_t exclude, size_t* out_count) {
    size_t total = freecs_query_count(world, mask, exclude);
    if (total == 0) {
        *out_count = 0;
        return NULL;
    }

    freecs_entity_t* entities = malloc(total * sizeof(freecs_entity_t));
    size_t idx = 0;

    size_t matching_count;
    size_t* matching = freecs_get_matching_archetypes(world, mask, exclude, &matching_count);
    for (size_t index = 0; index < matching_count; index++) {
        freecs_archetype_t* arch = &world->archetypes[matching[index]];
        for (size_t entity_index = 0; entity_index < arch->entities_len; entity_index++) {
            entities[idx++] = arch->entities[entity_index];
        }
    }

    *out_count = total;
    return entities;
}

freecs_entity_t freecs_query_first(freecs_world_t* world, uint64_t mask, uint64_t exclude, bool* found) {
    size_t matching_count;
    size_t* matching = freecs_get_matching_archetypes(world, mask, exclude, &matching_count);
    for (size_t index = 0; index < matching_count; index++) {
        freecs_archetype_t* arch = &world->archetypes[matching[index]];
        if (arch->entities_len > 0) {
            *found = true;
            return arch->entities[0];
        }
    }
    *found = false;
    return FREECS_ENTITY_NIL;
}

size_t freecs_entity_count(freecs_world_t* world) {
    size_t count = 0;
    for (size_t index = 0; index < world->archetypes_len; index++) {
        count += world->archetypes[index].entities_len;
    }
    return count;
}

void* freecs_column(freecs_archetype_t* arch, uint64_t bit, size_t* out_count) {
    int32_t col_idx = arch->column_bits[freecs_bit_index(bit)];
    if (col_idx < 0) {
        *out_count = 0;
        return NULL;
    }

    freecs_component_column_t* col = &arch->columns[col_idx];
    *out_count = arch->entities_len;
    if (arch->entities_len == 0 || col->data_len == 0) {
        return NULL;
    }

    return col->data;
}

void* freecs_column_unchecked(freecs_archetype_t* arch, uint64_t bit) {
    if (bit == 0) return NULL;
    int32_t col_idx = arch->column_bits[freecs_bit_index(bit)];
    if (col_idx < 0 || (size_t)col_idx >= arch->columns_len) return NULL;
    return arch->columns[col_idx].data;
}

freecs_table_iterator_t freecs_table_iterator(freecs_world_t* world, uint64_t mask, uint64_t exclude) {
    size_t count;
    size_t* indices = freecs_get_matching_archetypes(world, mask, exclude, &count);
    return (freecs_table_iterator_t){
        .world = world,
        .mask = mask,
        .exclude = exclude,
        .indices = indices,
        .indices_len = count,
        .current = 0
    };
}

bool freecs_table_iterator_next(freecs_table_iterator_t* iter, freecs_table_iterator_result_t* result) {
    if (iter->current >= iter->indices_len) return false;
    size_t arch_idx = iter->indices[iter->current++];
    result->archetype = &iter->world->archetypes[arch_idx];
    result->index = arch_idx;
    return true;
}

void freecs_for_each(freecs_world_t* world, uint64_t mask, uint64_t exclude, void (*callback)(freecs_archetype_t*, size_t)) {
    size_t matching_count;
    size_t* matching = freecs_get_matching_archetypes(world, mask, exclude, &matching_count);
    for (size_t index = 0; index < matching_count; index++) {
        freecs_archetype_t* arch = &world->archetypes[matching[index]];
        for (size_t entity_index = 0; entity_index < arch->entities_len; entity_index++) {
            callback(arch, entity_index);
        }
    }
}

void freecs_for_each_table(freecs_world_t* world, uint64_t mask, uint64_t exclude, void (*callback)(freecs_archetype_t*)) {
    size_t matching_count;
    size_t* matching = freecs_get_matching_archetypes(world, mask, exclude, &matching_count);
    for (size_t index = 0; index < matching_count; index++) {
        callback(&world->archetypes[matching[index]]);
    }
}

void freecs_queue_despawn(freecs_world_t* world, freecs_entity_t entity) {
    freecs__ensure_capacity_entities(&world->despawn_queue, &world->despawn_queue_cap, world->despawn_queue_len + 1);
    world->despawn_queue[world->despawn_queue_len++] = entity;
}

void freecs_apply_despawns(freecs_world_t* world) {
    for (size_t index = 0; index < world->despawn_queue_len; index++) {
        freecs_despawn(world, world->despawn_queue[index]);
    }
    world->despawn_queue_len = 0;
}

freecs_command_buffer_t freecs_create_command_buffer(freecs_world_t* world) {
    return (freecs_command_buffer_t){
        .commands = NULL,
        .commands_len = 0,
        .commands_cap = 0,
        .world = world
    };
}

void freecs_destroy_command_buffer(freecs_command_buffer_t* buffer) {
    for (size_t index = 0; index < buffer->commands_len; index++) {
        free(buffer->commands[index].component_data);
        free(buffer->commands[index].component_sizes);
        free(buffer->commands[index].component_bits);
    }
    free(buffer->commands);
    memset(buffer, 0, sizeof(*buffer));
}

void freecs_clear_command_buffer(freecs_command_buffer_t* buffer) {
    for (size_t index = 0; index < buffer->commands_len; index++) {
        free(buffer->commands[index].component_data);
        free(buffer->commands[index].component_sizes);
        free(buffer->commands[index].component_bits);
    }
    buffer->commands_len = 0;
}

void freecs_queue_spawn(freecs_command_buffer_t* buffer, uint64_t mask, const freecs_type_info_entry_t* entries, size_t entry_count) {
    freecs__ensure_capacity_commands(&buffer->commands, &buffer->commands_cap, buffer->commands_len + 1);

    freecs_command_t* cmd = &buffer->commands[buffer->commands_len++];
    memset(cmd, 0, sizeof(*cmd));
    cmd->command_type = FREECS_CMD_SPAWN;
    cmd->mask = mask;

    size_t total_size = 0;
    for (size_t index = 0; index < entry_count; index++) {
        total_size += entries[index].size;
    }

    if (total_size > 0) {
        cmd->component_data = malloc(total_size);
        cmd->component_data_cap = total_size;
    }

    cmd->component_sizes = malloc(entry_count * sizeof(size_t));
    cmd->component_bits = malloc(entry_count * sizeof(uint64_t));

    size_t offset = 0;
    for (size_t index = 0; index < entry_count; index++) {
        cmd->component_bits[index] = entries[index].bit;
        cmd->component_sizes[index] = entries[index].size;
        if (entries[index].data && entries[index].size > 0) {
            memcpy(&cmd->component_data[offset], entries[index].data, entries[index].size);
        }
        offset += entries[index].size;
    }
    cmd->component_data_len = offset;
    cmd->component_sizes_len = entry_count;
    cmd->component_bits_len = entry_count;
}

void freecs_cmd_queue_despawn(freecs_command_buffer_t* buffer, freecs_entity_t entity) {
    freecs__ensure_capacity_commands(&buffer->commands, &buffer->commands_cap, buffer->commands_len + 1);

    freecs_command_t* cmd = &buffer->commands[buffer->commands_len++];
    memset(cmd, 0, sizeof(*cmd));
    cmd->command_type = FREECS_CMD_DESPAWN;
    cmd->entity = entity;
}

void freecs_queue_add_components(freecs_command_buffer_t* buffer, freecs_entity_t entity, uint64_t mask) {
    freecs__ensure_capacity_commands(&buffer->commands, &buffer->commands_cap, buffer->commands_len + 1);

    freecs_command_t* cmd = &buffer->commands[buffer->commands_len++];
    memset(cmd, 0, sizeof(*cmd));
    cmd->command_type = FREECS_CMD_ADD_COMPONENTS;
    cmd->entity = entity;
    cmd->mask = mask;
}

void freecs_queue_remove_components(freecs_command_buffer_t* buffer, freecs_entity_t entity, uint64_t mask) {
    freecs__ensure_capacity_commands(&buffer->commands, &buffer->commands_cap, buffer->commands_len + 1);

    freecs_command_t* cmd = &buffer->commands[buffer->commands_len++];
    memset(cmd, 0, sizeof(*cmd));
    cmd->command_type = FREECS_CMD_REMOVE_COMPONENTS;
    cmd->entity = entity;
    cmd->mask = mask;
}

void freecs_apply_commands(freecs_command_buffer_t* buffer) {
    for (size_t index = 0; index < buffer->commands_len; index++) {
        freecs_command_t* cmd = &buffer->commands[index];

        switch (cmd->command_type) {
            case FREECS_CMD_SPAWN: {
                freecs_type_info_entry_t entries[FREECS_MAX_COMPONENTS];
                size_t offset = 0;
                for (size_t entry_index = 0; entry_index < cmd->component_bits_len; entry_index++) {
                    entries[entry_index].bit = cmd->component_bits[entry_index];
                    entries[entry_index].size = cmd->component_sizes[entry_index];
                    entries[entry_index].data = cmd->component_sizes[entry_index] > 0 ? &cmd->component_data[offset] : NULL;
                    entries[entry_index].type_index = freecs_bit_index(cmd->component_bits[entry_index]);
                    offset += cmd->component_sizes[entry_index];
                }
                freecs_spawn(buffer->world, cmd->mask, entries, cmd->component_bits_len);
                break;
            }
            case FREECS_CMD_DESPAWN:
                freecs_despawn(buffer->world, cmd->entity);
                break;
            case FREECS_CMD_ADD_COMPONENTS:
            case FREECS_CMD_REMOVE_COMPONENTS:
                break;
        }
    }

    freecs_clear_command_buffer(buffer);
}

freecs_tags_t freecs_create_tags(void) {
    freecs_tags_t tags = {0};
    return tags;
}

void freecs_destroy_tags(freecs_tags_t* tags) {
    for (int index = 0; index < FREECS_MAX_TAGS; index++) {
        free(tags->storage[index].entries);
    }
    memset(tags, 0, sizeof(*tags));
}

int freecs_register_tag(freecs_tags_t* tags, const char* name) {
    (void)name;
    int tag_id = tags->next_tag++;
    return tag_id;
}

void freecs_add_tag(freecs_tags_t* tags, int tag_id, freecs_entity_t entity) {
    if (tag_id < 0 || tag_id >= FREECS_MAX_TAGS) return;

    freecs_tag_storage_t* storage = &tags->storage[tag_id];

    for (size_t index = 0; index < storage->entries_len; index++) {
        if (storage->entries[index].entity.id == entity.id) {
            storage->entries[index].entity = entity;
            return;
        }
    }

    freecs__ensure_capacity_tag_entries(&storage->entries, &storage->entries_cap, storage->entries_len + 1);
    storage->entries[storage->entries_len++] = (freecs_tag_entry_t){entity, 0};
}

void freecs_remove_tag(freecs_tags_t* tags, int tag_id, freecs_entity_t entity) {
    if (tag_id < 0 || tag_id >= FREECS_MAX_TAGS) return;

    freecs_tag_storage_t* storage = &tags->storage[tag_id];

    for (size_t index = 0; index < storage->entries_len; index++) {
        if (storage->entries[index].entity.id == entity.id) {
            storage->entries[index] = storage->entries[storage->entries_len - 1];
            storage->entries_len--;
            return;
        }
    }
}

bool freecs_has_tag(freecs_tags_t* tags, int tag_id, freecs_entity_t entity) {
    if (tag_id < 0 || tag_id >= FREECS_MAX_TAGS) return false;

    freecs_tag_storage_t* storage = &tags->storage[tag_id];

    for (size_t index = 0; index < storage->entries_len; index++) {
        if (storage->entries[index].entity.id == entity.id &&
            storage->entries[index].entity.generation == entity.generation) {
            return true;
        }
    }
    return false;
}

freecs_entity_t* freecs_query_tag(freecs_tags_t* tags, int tag_id, size_t* out_count) {
    if (tag_id < 0 || tag_id >= FREECS_MAX_TAGS) {
        *out_count = 0;
        return NULL;
    }

    freecs_tag_storage_t* storage = &tags->storage[tag_id];
    if (storage->entries_len == 0) {
        *out_count = 0;
        return NULL;
    }

    freecs_entity_t* entities = malloc(storage->entries_len * sizeof(freecs_entity_t));
    for (size_t index = 0; index < storage->entries_len; index++) {
        entities[index] = storage->entries[index].entity;
    }

    *out_count = storage->entries_len;
    return entities;
}

size_t freecs_tag_count(freecs_tags_t* tags, int tag_id) {
    if (tag_id < 0 || tag_id >= FREECS_MAX_TAGS) return 0;
    return tags->storage[tag_id].entries_len;
}

void freecs_clear_entity_tags(freecs_tags_t* tags, freecs_entity_t entity) {
    for (int index = 0; index < FREECS_MAX_TAGS; index++) {
        freecs_remove_tag(tags, index, entity);
    }
}

freecs_event_queue_t freecs_create_event_queue(size_t elem_size) {
    return (freecs_event_queue_t){
        .data = NULL,
        .data_len = 0,
        .data_cap = 0,
        .elem_size = elem_size
    };
}

void freecs_destroy_event_queue(freecs_event_queue_t* queue) {
    free(queue->data);
    memset(queue, 0, sizeof(*queue));
}

void freecs_send_event(freecs_event_queue_t* queue, const void* event) {
    freecs__ensure_capacity_u8(&queue->data, &queue->data_cap, queue->data_len + queue->elem_size);
    memcpy(&queue->data[queue->data_len], event, queue->elem_size);
    queue->data_len += queue->elem_size;
}

void* freecs_read_events(freecs_event_queue_t* queue, size_t* out_count) {
    *out_count = queue->data_len / queue->elem_size;
    return queue->data;
}

void freecs_clear_events(freecs_event_queue_t* queue) {
    queue->data_len = 0;
}

size_t freecs_event_count(freecs_event_queue_t* queue) {
    return queue->data_len / queue->elem_size;
}

#endif
