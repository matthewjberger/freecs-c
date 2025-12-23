#include "freecs.h"
#include <stdlib.h>
#include <string.h>

static void ensure_capacity_u8(uint8_t** data, size_t* cap, size_t needed) {
    if (needed <= *cap) return;
    size_t new_cap = *cap == 0 ? 16 : *cap * 2;
    while (new_cap < needed) new_cap *= 2;
    *data = realloc(*data, new_cap);
    *cap = new_cap;
}

static void ensure_capacity_entities(freecs_entity_t** data, size_t* cap, size_t needed) {
    if (needed <= *cap) return;
    size_t new_cap = *cap == 0 ? 16 : *cap * 2;
    while (new_cap < needed) new_cap *= 2;
    *data = realloc(*data, new_cap * sizeof(freecs_entity_t));
    *cap = new_cap;
}

static void ensure_capacity_locations(freecs_entity_location_t** data, size_t* cap, size_t needed) {
    if (needed <= *cap) return;
    size_t new_cap = *cap == 0 ? FREECS_MIN_ENTITY_CAPACITY : *cap * 2;
    while (new_cap < needed) new_cap *= 2;
    *data = realloc(*data, new_cap * sizeof(freecs_entity_location_t));
    *cap = new_cap;
}

static void ensure_capacity_archetypes(freecs_archetype_t** data, size_t* cap, size_t needed) {
    if (needed <= *cap) return;
    size_t new_cap = *cap == 0 ? 16 : *cap * 2;
    while (new_cap < needed) new_cap *= 2;
    *data = realloc(*data, new_cap * sizeof(freecs_archetype_t));
    *cap = new_cap;
}

static void ensure_capacity_columns(freecs_component_column_t** data, size_t* cap, size_t needed) {
    if (needed <= *cap) return;
    size_t new_cap = *cap == 0 ? 8 : *cap * 2;
    while (new_cap < needed) new_cap *= 2;
    *data = realloc(*data, new_cap * sizeof(freecs_component_column_t));
    *cap = new_cap;
}

static void ensure_capacity_indices(size_t** data, size_t* cap, size_t needed) {
    if (needed <= *cap) return;
    size_t new_cap = *cap == 0 ? 16 : *cap * 2;
    while (new_cap < needed) new_cap *= 2;
    *data = realloc(*data, new_cap * sizeof(size_t));
    *cap = new_cap;
}

static void ensure_capacity_cache(freecs_cache_entry_t** data, size_t* cap, size_t needed) {
    if (needed <= *cap) return;
    size_t new_cap = *cap == 0 ? 16 : *cap * 2;
    while (new_cap < needed) new_cap *= 2;
    *data = realloc(*data, new_cap * sizeof(freecs_cache_entry_t));
    *cap = new_cap;
}

static void ensure_capacity_commands(freecs_command_t** data, size_t* cap, size_t needed) {
    if (needed <= *cap) return;
    size_t new_cap = *cap == 0 ? 16 : *cap * 2;
    while (new_cap < needed) new_cap *= 2;
    *data = realloc(*data, new_cap * sizeof(freecs_command_t));
    *cap = new_cap;
}



static void ensure_capacity_tag_entries(freecs_tag_entry_t** data, size_t* cap, size_t needed) {
    if (needed <= *cap) return;
    size_t new_cap = *cap == 0 ? 16 : *cap * 2;
    while (new_cap < needed) new_cap *= 2;
    *data = realloc(*data, new_cap * sizeof(freecs_tag_entry_t));
    *cap = new_cap;
}

static size_t cache_find(freecs_cache_entry_t* cache, size_t len, uint64_t key) {
    for (size_t i = 0; i < len; i++) {
        if (cache[i].key == key) return i;
    }
    return (size_t)-1;
}

freecs_world_t freecs_create_world(void) {
    freecs_world_t world = {0};
    world.next_bit = 1;
    return world;
}

void freecs_destroy_world(freecs_world_t* world) {
    for (size_t i = 0; i < world->archetypes_len; i++) {
        freecs_archetype_t* arch = &world->archetypes[i];
        for (size_t j = 0; j < arch->columns_len; j++) {
            free(arch->columns[j].data);
        }
        free(arch->columns);
        free(arch->entities);
    }
    free(world->archetypes);
    free(world->locations);
    free(world->free_entities);
    for (size_t i = 0; i < world->archetype_index_len; i++) {
        free(world->archetype_index[i].value.indices);
    }
    free(world->archetype_index);
    for (size_t i = 0; i < world->query_cache_len; i++) {
        free(world->query_cache[i].value.indices);
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

static void ensure_entity_slot(freecs_world_t* world, uint32_t id) {
    if (world->locations_len > id) return;


    ensure_capacity_locations(&world->locations, &world->locations_cap, (size_t)id + 1);
    while (world->locations_len <= id) {
        world->locations[world->locations_len] = (freecs_entity_location_t){0, 0, 0, false};
        world->locations_len++;
    }
}

static freecs_entity_t alloc_entity(freecs_world_t* world) {
    if (world->free_entities_len > 0) {
        return world->free_entities[--world->free_entities_len];
    }

    uint32_t id = world->next_entity_id++;
    ensure_entity_slot(world, id);

    return (freecs_entity_t){id, 0};
}

static size_t find_or_create_archetype(freecs_world_t* world, uint64_t mask, const freecs_type_info_entry_t* type_info, size_t type_info_count) {
    size_t idx = cache_find(world->archetype_index, world->archetype_index_len, mask);
    if (idx != (size_t)-1) {
        return world->archetype_index[idx].value.indices[0];
    }

    size_t arch_idx = world->archetypes_len;
    ensure_capacity_archetypes(&world->archetypes, &world->archetypes_cap, arch_idx + 1);

    freecs_archetype_t* arch = &world->archetypes[arch_idx];
    memset(arch, 0, sizeof(*arch));
    arch->mask = mask;

    for (size_t i = 0; i < FREECS_MAX_COMPONENTS; i++) {
        arch->column_bits[i] = -1;
        arch->edges.add_edges[i] = -1;
        arch->edges.remove_edges[i] = -1;
    }

    for (size_t i = 0; i < type_info_count; i++) {
        size_t col_idx = arch->columns_len;
        ensure_capacity_columns(&arch->columns, &arch->columns_cap, col_idx + 1);

        freecs_component_column_t* col = &arch->columns[col_idx];
        memset(col, 0, sizeof(*col));
        col->elem_size = type_info[i].size;
        col->bit = type_info[i].bit;
        col->type_index = type_info[i].type_index;

        arch->column_bits[freecs_bit_index(type_info[i].bit)] = (int32_t)col_idx;
        arch->columns_len++;
    }

    world->archetypes_len++;

    ensure_capacity_cache(&world->archetype_index, &world->archetype_index_cap, world->archetype_index_len + 1);
    freecs_cache_entry_t* entry = &world->archetype_index[world->archetype_index_len++];
    entry->key = mask;
    entry->value.indices = malloc(sizeof(size_t));
    entry->value.indices[0] = arch_idx;
    entry->value.len = 1;
    entry->value.cap = 1;

    for (size_t i = 0; i < world->query_cache_len; i++) {
        uint64_t query_mask = world->query_cache[i].key & 0xFFFFFFFF;
        if ((mask & query_mask) == query_mask) {
            freecs_index_array_t* cached = &world->query_cache[i].value;
            ensure_capacity_indices(&cached->indices, &cached->cap, cached->len + 1);
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

    size_t arch_idx = find_or_create_archetype(world, mask, entries, entry_count);
    freecs_archetype_t* arch = &world->archetypes[arch_idx];

    freecs_entity_t entity = alloc_entity(world);
    size_t row = arch->entities_len;

    ensure_capacity_entities(&arch->entities, &arch->entities_cap, row + 1);
    arch->entities[arch->entities_len++] = entity;

    for (size_t i = 0; i < entry_count; i++) {
        int32_t col_idx = arch->column_bits[freecs_bit_index(entries[i].bit)];
        if (col_idx >= 0) {
            freecs_component_column_t* col = &arch->columns[col_idx];
            size_t old_len = col->data_len;
            ensure_capacity_u8(&col->data, &col->data_cap, old_len + entries[i].size);
            col->data_len = old_len + entries[i].size;
            if (entries[i].data != NULL && entries[i].size > 0) {
                memcpy(&col->data[old_len], entries[i].data, entries[i].size);
            } else {
                memset(&col->data[old_len], 0, entries[i].size);
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

    size_t arch_idx = find_or_create_archetype(world, mask, type_info, info_count);
    freecs_archetype_t* arch = &world->archetypes[arch_idx];

    size_t start_row = arch->entities_len;
    ensure_capacity_entities(&arch->entities, &arch->entities_cap, start_row + count);

    for (size_t c = 0; c < arch->columns_len; c++) {
        freecs_component_column_t* col = &arch->columns[c];
        ensure_capacity_u8(&col->data, &col->data_cap, col->data_len + count * col->elem_size);
    }

    freecs_entity_t* entities = malloc(count * sizeof(freecs_entity_t));

    for (size_t i = 0; i < count; i++) {
        freecs_entity_t entity = alloc_entity(world);
        entities[i] = entity;
        size_t row = start_row + i;
        arch->entities[arch->entities_len++] = entity;

        for (size_t c = 0; c < arch->columns_len; c++) {
            freecs_component_column_t* col = &arch->columns[c];
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

    size_t idx = cache_find(world->archetype_index, world->archetype_index_len, mask);
    if (idx == (size_t)-1) return entities;

    size_t arch_idx = world->archetype_index[idx].value.indices[0];
    freecs_archetype_t* arch = &world->archetypes[arch_idx];
    size_t start_row = arch->entities_len - count;

    for (size_t i = 0; i < count; i++) {
        init_callback(arch, start_row + i);
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

        for (size_t c = 0; c < arch->columns_len; c++) {
            freecs_component_column_t* col = &arch->columns[c];
            if (col->elem_size > 0) {
                size_t src_start = last_row * col->elem_size;
                size_t dst_start = row * col->elem_size;
                memcpy(&col->data[dst_start], &col->data[src_start], col->elem_size);
            }
        }
    }

    arch->entities_len--;
    for (size_t c = 0; c < arch->columns_len; c++) {
        freecs_component_column_t* col = &arch->columns[c];
        if (col->elem_size > 0) {
            col->data_len -= col->elem_size;
        }
    }

    loc->alive = false;
    loc->generation++;

    ensure_capacity_entities(&world->free_entities, &world->free_entities_cap, world->free_entities_len + 1);
    world->free_entities[world->free_entities_len++] = (freecs_entity_t){entity.id, loc->generation};

    return true;
}

size_t freecs_despawn_batch(freecs_world_t* world, const freecs_entity_t* entities, size_t count) {
    size_t despawned = 0;
    for (size_t i = 0; i < count; i++) {
        if (freecs_despawn(world, entities[i])) {
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

static void move_entity(freecs_world_t* world, freecs_entity_t entity, size_t from_arch_idx, size_t from_row, size_t to_arch_idx) {
    freecs_archetype_t* from_arch = &world->archetypes[from_arch_idx];
    freecs_archetype_t* to_arch = &world->archetypes[to_arch_idx];

    size_t new_row = to_arch->entities_len;
    ensure_capacity_entities(&to_arch->entities, &to_arch->entities_cap, new_row + 1);
    to_arch->entities[to_arch->entities_len++] = entity;

    for (size_t c = 0; c < to_arch->columns_len; c++) {
        freecs_component_column_t* to_col = &to_arch->columns[c];
        size_t old_len = to_col->data_len;
        ensure_capacity_u8(&to_col->data, &to_col->data_cap, old_len + to_col->elem_size);
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

        for (size_t c = 0; c < from_arch->columns_len; c++) {
            freecs_component_column_t* col = &from_arch->columns[c];
            if (col->elem_size > 0) {
                size_t src_start = last_row * col->elem_size;
                size_t dst_start = from_row * col->elem_size;
                memcpy(&col->data[dst_start], &col->data[src_start], col->elem_size);
            }
        }
    }

    from_arch->entities_len--;
    for (size_t c = 0; c < from_arch->columns_len; c++) {
        freecs_component_column_t* col = &from_arch->columns[c];
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

        for (size_t c = 0; c < arch->columns_len; c++) {
            freecs_component_column_t* col = &arch->columns[c];
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

        target_arch_idx_signed = (int32_t)find_or_create_archetype(world, new_mask, type_info, info_count);
        world->archetypes[loc->archetype_index].edges.add_edges[bit_idx] = target_arch_idx_signed;
    }

    size_t target_arch_idx = (size_t)target_arch_idx_signed;
    move_entity(world, entity, loc->archetype_index, loc->row, target_arch_idx);

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

        for (size_t c = 0; c < arch->columns_len; c++) {
            freecs_component_column_t* col = &arch->columns[c];
            if (col->bit != bit) {
                type_info[info_count].bit = col->bit;
                type_info[info_count].size = col->elem_size;
                type_info[info_count].data = NULL;
                type_info[info_count].type_index = col->type_index;
                info_count++;
            }
        }

        target_arch_idx_signed = (int32_t)find_or_create_archetype(world, new_mask, type_info, info_count);
        world->archetypes[loc->archetype_index].edges.remove_edges[bit_idx] = target_arch_idx_signed;
    }

    size_t target_arch_idx = (size_t)target_arch_idx_signed;
    move_entity(world, entity, loc->archetype_index, loc->row, target_arch_idx);

    return true;
}

size_t* freecs_get_matching_archetypes(freecs_world_t* world, uint64_t mask, uint64_t exclude, size_t* out_count) {
    uint64_t cache_key = mask | (exclude << 32);
    size_t idx = cache_find(world->query_cache, world->query_cache_len, cache_key);
    if (idx != (size_t)-1) {
        *out_count = world->query_cache[idx].value.len;
        return world->query_cache[idx].value.indices;
    }

    freecs_index_array_t matching = {0};

    for (size_t i = 0; i < world->archetypes_len; i++) {
        freecs_archetype_t* arch = &world->archetypes[i];
        if ((arch->mask & mask) == mask && (exclude == 0 || (arch->mask & exclude) == 0)) {
            ensure_capacity_indices(&matching.indices, &matching.cap, matching.len + 1);
            matching.indices[matching.len++] = i;
        }
    }

    ensure_capacity_cache(&world->query_cache, &world->query_cache_cap, world->query_cache_len + 1);
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
    for (size_t i = 0; i < matching_count; i++) {
        count += world->archetypes[matching[i]].entities_len;
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
    for (size_t i = 0; i < matching_count; i++) {
        freecs_archetype_t* arch = &world->archetypes[matching[i]];
        for (size_t j = 0; j < arch->entities_len; j++) {
            entities[idx++] = arch->entities[j];
        }
    }

    *out_count = total;
    return entities;
}

freecs_entity_t freecs_query_first(freecs_world_t* world, uint64_t mask, uint64_t exclude, bool* found) {
    size_t matching_count;
    size_t* matching = freecs_get_matching_archetypes(world, mask, exclude, &matching_count);
    for (size_t i = 0; i < matching_count; i++) {
        freecs_archetype_t* arch = &world->archetypes[matching[i]];
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
    for (size_t i = 0; i < world->archetypes_len; i++) {
        count += world->archetypes[i].entities_len;
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
    for (size_t i = 0; i < matching_count; i++) {
        freecs_archetype_t* arch = &world->archetypes[matching[i]];
        for (size_t j = 0; j < arch->entities_len; j++) {
            callback(arch, j);
        }
    }
}

void freecs_for_each_table(freecs_world_t* world, uint64_t mask, uint64_t exclude, void (*callback)(freecs_archetype_t*)) {
    size_t matching_count;
    size_t* matching = freecs_get_matching_archetypes(world, mask, exclude, &matching_count);
    for (size_t i = 0; i < matching_count; i++) {
        callback(&world->archetypes[matching[i]]);
    }
}

void freecs_queue_despawn(freecs_world_t* world, freecs_entity_t entity) {
    ensure_capacity_entities(&world->despawn_queue, &world->despawn_queue_cap, world->despawn_queue_len + 1);
    world->despawn_queue[world->despawn_queue_len++] = entity;
}

void freecs_apply_despawns(freecs_world_t* world) {
    for (size_t i = 0; i < world->despawn_queue_len; i++) {
        freecs_despawn(world, world->despawn_queue[i]);
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
    for (size_t i = 0; i < buffer->commands_len; i++) {
        free(buffer->commands[i].component_data);
        free(buffer->commands[i].component_sizes);
        free(buffer->commands[i].component_bits);
    }
    free(buffer->commands);
    memset(buffer, 0, sizeof(*buffer));
}

void freecs_clear_command_buffer(freecs_command_buffer_t* buffer) {
    for (size_t i = 0; i < buffer->commands_len; i++) {
        free(buffer->commands[i].component_data);
        free(buffer->commands[i].component_sizes);
        free(buffer->commands[i].component_bits);
    }
    buffer->commands_len = 0;
}

void freecs_queue_spawn(freecs_command_buffer_t* buffer, uint64_t mask, const freecs_type_info_entry_t* entries, size_t entry_count) {
    ensure_capacity_commands(&buffer->commands, &buffer->commands_cap, buffer->commands_len + 1);

    freecs_command_t* cmd = &buffer->commands[buffer->commands_len++];
    memset(cmd, 0, sizeof(*cmd));
    cmd->command_type = FREECS_CMD_SPAWN;
    cmd->mask = mask;

    size_t total_size = 0;
    for (size_t i = 0; i < entry_count; i++) {
        total_size += entries[i].size;
    }

    if (total_size > 0) {
        cmd->component_data = malloc(total_size);
        cmd->component_data_cap = total_size;
    }

    cmd->component_sizes = malloc(entry_count * sizeof(size_t));
    cmd->component_bits = malloc(entry_count * sizeof(uint64_t));

    size_t offset = 0;
    for (size_t i = 0; i < entry_count; i++) {
        cmd->component_bits[i] = entries[i].bit;
        cmd->component_sizes[i] = entries[i].size;
        if (entries[i].data && entries[i].size > 0) {
            memcpy(&cmd->component_data[offset], entries[i].data, entries[i].size);
        }
        offset += entries[i].size;
    }
    cmd->component_data_len = offset;
    cmd->component_sizes_len = entry_count;
    cmd->component_bits_len = entry_count;
}

void freecs_cmd_queue_despawn(freecs_command_buffer_t* buffer, freecs_entity_t entity) {
    ensure_capacity_commands(&buffer->commands, &buffer->commands_cap, buffer->commands_len + 1);

    freecs_command_t* cmd = &buffer->commands[buffer->commands_len++];
    memset(cmd, 0, sizeof(*cmd));
    cmd->command_type = FREECS_CMD_DESPAWN;
    cmd->entity = entity;
}

void freecs_queue_add_components(freecs_command_buffer_t* buffer, freecs_entity_t entity, uint64_t mask) {
    ensure_capacity_commands(&buffer->commands, &buffer->commands_cap, buffer->commands_len + 1);

    freecs_command_t* cmd = &buffer->commands[buffer->commands_len++];
    memset(cmd, 0, sizeof(*cmd));
    cmd->command_type = FREECS_CMD_ADD_COMPONENTS;
    cmd->entity = entity;
    cmd->mask = mask;
}

void freecs_queue_remove_components(freecs_command_buffer_t* buffer, freecs_entity_t entity, uint64_t mask) {
    ensure_capacity_commands(&buffer->commands, &buffer->commands_cap, buffer->commands_len + 1);

    freecs_command_t* cmd = &buffer->commands[buffer->commands_len++];
    memset(cmd, 0, sizeof(*cmd));
    cmd->command_type = FREECS_CMD_REMOVE_COMPONENTS;
    cmd->entity = entity;
    cmd->mask = mask;
}

void freecs_apply_commands(freecs_command_buffer_t* buffer) {
    for (size_t i = 0; i < buffer->commands_len; i++) {
        freecs_command_t* cmd = &buffer->commands[i];

        switch (cmd->command_type) {
            case FREECS_CMD_SPAWN: {
                freecs_type_info_entry_t entries[FREECS_MAX_COMPONENTS];
                size_t offset = 0;
                for (size_t j = 0; j < cmd->component_bits_len; j++) {
                    entries[j].bit = cmd->component_bits[j];
                    entries[j].size = cmd->component_sizes[j];
                    entries[j].data = cmd->component_sizes[j] > 0 ? &cmd->component_data[offset] : NULL;
                    entries[j].type_index = freecs_bit_index(cmd->component_bits[j]);
                    offset += cmd->component_sizes[j];
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
    for (int i = 0; i < FREECS_MAX_TAGS; i++) {
        free(tags->storage[i].entries);
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

    for (size_t i = 0; i < storage->entries_len; i++) {
        if (storage->entries[i].entity.id == entity.id) {
            storage->entries[i].entity = entity;
            return;
        }
    }

    ensure_capacity_tag_entries(&storage->entries, &storage->entries_cap, storage->entries_len + 1);
    storage->entries[storage->entries_len++] = (freecs_tag_entry_t){entity, 0};
}

void freecs_remove_tag(freecs_tags_t* tags, int tag_id, freecs_entity_t entity) {
    if (tag_id < 0 || tag_id >= FREECS_MAX_TAGS) return;

    freecs_tag_storage_t* storage = &tags->storage[tag_id];

    for (size_t i = 0; i < storage->entries_len; i++) {
        if (storage->entries[i].entity.id == entity.id) {
            storage->entries[i] = storage->entries[storage->entries_len - 1];
            storage->entries_len--;
            return;
        }
    }
}

bool freecs_has_tag(freecs_tags_t* tags, int tag_id, freecs_entity_t entity) {
    if (tag_id < 0 || tag_id >= FREECS_MAX_TAGS) return false;

    freecs_tag_storage_t* storage = &tags->storage[tag_id];

    for (size_t i = 0; i < storage->entries_len; i++) {
        if (storage->entries[i].entity.id == entity.id &&
            storage->entries[i].entity.generation == entity.generation) {
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
    for (size_t i = 0; i < storage->entries_len; i++) {
        entities[i] = storage->entries[i].entity;
    }

    *out_count = storage->entries_len;
    return entities;
}

size_t freecs_tag_count(freecs_tags_t* tags, int tag_id) {
    if (tag_id < 0 || tag_id >= FREECS_MAX_TAGS) return 0;
    return tags->storage[tag_id].entries_len;
}

void freecs_clear_entity_tags(freecs_tags_t* tags, freecs_entity_t entity) {
    for (int i = 0; i < FREECS_MAX_TAGS; i++) {
        freecs_remove_tag(tags, i, entity);
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
    ensure_capacity_u8(&queue->data, &queue->data_cap, queue->data_len + queue->elem_size);
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
