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
    fflush(stdout); \
    tests_run++; \
    test_##name(); \
    tests_passed++; \
    printf("PASSED\n"); \
    fflush(stdout); \
} while(0)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf("FAILED\n    Assertion failed: %s\n    at %s:%d\n", #cond, __FILE__, __LINE__); \
        fflush(stdout); \
        exit(1); \
    } \
} while(0)

#define ASSERT_EQ(a, b) ASSERT((a) == (b))
#define ASSERT_NEQ(a, b) ASSERT((a) != (b))
#define ASSERT_FLOAT_EQ(a, b) ASSERT(fabsf((a) - (b)) < 0.001f)

typedef struct {
    float x;
    float y;
} Position;

typedef struct {
    float x;
    float y;
} Velocity;

typedef struct {
    float value;
} Health;

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
        {BIT_POSITION, sizeof(Position), &pos, freecs_bit_index(BIT_POSITION)},
        {BIT_VELOCITY, sizeof(Velocity), &vel, freecs_bit_index(BIT_VELOCITY)}
    };

    freecs_entity_t entity = freecs_spawn(&world, BIT_POSITION | BIT_VELOCITY, entries, 2);

    ASSERT_EQ(entity.id, 0);
    ASSERT_EQ(entity.generation, 0);
    ASSERT_EQ(freecs_entity_count(&world), 1);

    freecs_destroy_world(&world);
}

TEST(get_component) {
    freecs_world_t world = freecs_create_world();
    setup_world(&world);

    Position pos = {1, 2};
    Velocity vel = {3, 4};
    freecs_type_info_entry_t entries[2] = {
        {BIT_POSITION, sizeof(Position), &pos, freecs_bit_index(BIT_POSITION)},
        {BIT_VELOCITY, sizeof(Velocity), &vel, freecs_bit_index(BIT_VELOCITY)}
    };

    freecs_entity_t entity = freecs_spawn(&world, BIT_POSITION | BIT_VELOCITY, entries, 2);

    Position* got_pos = FREECS_GET(&world, entity, Position, BIT_POSITION);
    ASSERT(got_pos != NULL);
    ASSERT_FLOAT_EQ(got_pos->x, 1);
    ASSERT_FLOAT_EQ(got_pos->y, 2);

    Velocity* got_vel = FREECS_GET(&world, entity, Velocity, BIT_VELOCITY);
    ASSERT(got_vel != NULL);
    ASSERT_FLOAT_EQ(got_vel->x, 3);
    ASSERT_FLOAT_EQ(got_vel->y, 4);

    Health* got_health = FREECS_GET(&world, entity, Health, BIT_HEALTH);
    ASSERT(got_health == NULL);

    freecs_destroy_world(&world);
}

TEST(set_component) {
    freecs_world_t world = freecs_create_world();
    setup_world(&world);

    Position pos = {1, 2};
    freecs_type_info_entry_t entries[1] = {
        {BIT_POSITION, sizeof(Position), &pos, freecs_bit_index(BIT_POSITION)}
    };

    freecs_entity_t entity = freecs_spawn(&world, BIT_POSITION, entries, 1);

    Position new_pos = {10, 20};
    freecs_set(&world, entity, BIT_POSITION, &new_pos, sizeof(Position));

    Position* got_pos = FREECS_GET(&world, entity, Position, BIT_POSITION);
    ASSERT_FLOAT_EQ(got_pos->x, 10);
    ASSERT_FLOAT_EQ(got_pos->y, 20);

    freecs_destroy_world(&world);
}

TEST(despawn_entity) {
    freecs_world_t world = freecs_create_world();
    setup_world(&world);

    Position pos1 = {1, 1};
    Position pos2 = {2, 2};
    Position pos3 = {3, 3};
    freecs_type_info_entry_t e1[1] = {{BIT_POSITION, sizeof(Position), &pos1, freecs_bit_index(BIT_POSITION)}};
    freecs_type_info_entry_t e2[1] = {{BIT_POSITION, sizeof(Position), &pos2, freecs_bit_index(BIT_POSITION)}};
    freecs_type_info_entry_t e3[1] = {{BIT_POSITION, sizeof(Position), &pos3, freecs_bit_index(BIT_POSITION)}};

    freecs_entity_t ent1 = freecs_spawn(&world, BIT_POSITION, e1, 1);
    freecs_entity_t ent2 = freecs_spawn(&world, BIT_POSITION, e2, 1);
    freecs_entity_t ent3 = freecs_spawn(&world, BIT_POSITION, e3, 1);

    ASSERT_EQ(freecs_entity_count(&world), 3);

    freecs_despawn(&world, ent2);

    ASSERT_EQ(freecs_entity_count(&world), 2);
    ASSERT(freecs_is_alive(&world, ent1));
    ASSERT(!freecs_is_alive(&world, ent2));
    ASSERT(freecs_is_alive(&world, ent3));

    freecs_destroy_world(&world);
}

TEST(generational_indices) {
    freecs_world_t world = freecs_create_world();
    setup_world(&world);

    Position pos1 = {1, 1};
    freecs_type_info_entry_t e1[1] = {{BIT_POSITION, sizeof(Position), &pos1, freecs_bit_index(BIT_POSITION)}};

    freecs_entity_t ent1 = freecs_spawn(&world, BIT_POSITION, e1, 1);
    ASSERT_EQ(ent1.generation, 0);

    uint32_t id = ent1.id;
    freecs_despawn(&world, ent1);

    Position pos2 = {2, 2};
    freecs_type_info_entry_t e2[1] = {{BIT_POSITION, sizeof(Position), &pos2, freecs_bit_index(BIT_POSITION)}};
    freecs_entity_t ent2 = freecs_spawn(&world, BIT_POSITION, e2, 1);

    ASSERT_EQ(ent2.id, id);
    ASSERT_EQ(ent2.generation, 1);

    ASSERT(freecs_get(&world, ent1, BIT_POSITION) == NULL);

    Position* p = FREECS_GET(&world, ent2, Position, BIT_POSITION);
    ASSERT(p != NULL);
    ASSERT_FLOAT_EQ(p->x, 2);

    freecs_destroy_world(&world);
}

TEST(multiple_archetypes) {
    freecs_world_t world = freecs_create_world();
    setup_world(&world);

    Position pos1 = {1, 1};
    freecs_type_info_entry_t e1[1] = {{BIT_POSITION, sizeof(Position), &pos1, freecs_bit_index(BIT_POSITION)}};

    Position pos2 = {2, 2};
    Velocity vel2 = {1, 0};
    freecs_type_info_entry_t e2[2] = {
        {BIT_POSITION, sizeof(Position), &pos2, freecs_bit_index(BIT_POSITION)},
        {BIT_VELOCITY, sizeof(Velocity), &vel2, freecs_bit_index(BIT_VELOCITY)}
    };

    Position pos3 = {3, 3};
    Velocity vel3 = {0, 1};
    Health health3 = {100};
    freecs_type_info_entry_t e3[3] = {
        {BIT_POSITION, sizeof(Position), &pos3, freecs_bit_index(BIT_POSITION)},
        {BIT_VELOCITY, sizeof(Velocity), &vel3, freecs_bit_index(BIT_VELOCITY)},
        {BIT_HEALTH, sizeof(Health), &health3, freecs_bit_index(BIT_HEALTH)}
    };

    freecs_entity_t ent1 = freecs_spawn(&world, BIT_POSITION, e1, 1);
    freecs_entity_t ent2 = freecs_spawn(&world, BIT_POSITION | BIT_VELOCITY, e2, 2);
    freecs_entity_t ent3 = freecs_spawn(&world, BIT_POSITION | BIT_VELOCITY | BIT_HEALTH, e3, 3);

    ASSERT_EQ(world.archetypes_len, 3);

    ASSERT(freecs_has(&world, ent1, BIT_POSITION));
    ASSERT(!freecs_has(&world, ent1, BIT_VELOCITY));

    ASSERT(freecs_has(&world, ent2, BIT_POSITION));
    ASSERT(freecs_has(&world, ent2, BIT_VELOCITY));
    ASSERT(!freecs_has(&world, ent2, BIT_HEALTH));

    ASSERT(freecs_has(&world, ent3, BIT_POSITION));
    ASSERT(freecs_has(&world, ent3, BIT_VELOCITY));
    ASSERT(freecs_has(&world, ent3, BIT_HEALTH));

    freecs_destroy_world(&world);
}

TEST(query_count) {
    freecs_world_t world = freecs_create_world();
    setup_world(&world);

    Position pos1 = {1, 1};
    freecs_type_info_entry_t e1[1] = {{BIT_POSITION, sizeof(Position), &pos1, freecs_bit_index(BIT_POSITION)}};
    freecs_spawn(&world, BIT_POSITION, e1, 1);

    Position pos2 = {2, 2};
    freecs_type_info_entry_t e2[1] = {{BIT_POSITION, sizeof(Position), &pos2, freecs_bit_index(BIT_POSITION)}};
    freecs_spawn(&world, BIT_POSITION, e2, 1);

    Position pos3 = {3, 3};
    Velocity vel3 = {1, 0};
    freecs_type_info_entry_t e3[2] = {
        {BIT_POSITION, sizeof(Position), &pos3, freecs_bit_index(BIT_POSITION)},
        {BIT_VELOCITY, sizeof(Velocity), &vel3, freecs_bit_index(BIT_VELOCITY)}
    };
    freecs_spawn(&world, BIT_POSITION | BIT_VELOCITY, e3, 2);

    Position pos4 = {4, 4};
    Velocity vel4 = {0, 1};
    Health health4 = {100};
    freecs_type_info_entry_t e4[3] = {
        {BIT_POSITION, sizeof(Position), &pos4, freecs_bit_index(BIT_POSITION)},
        {BIT_VELOCITY, sizeof(Velocity), &vel4, freecs_bit_index(BIT_VELOCITY)},
        {BIT_HEALTH, sizeof(Health), &health4, freecs_bit_index(BIT_HEALTH)}
    };
    freecs_spawn(&world, BIT_POSITION | BIT_VELOCITY | BIT_HEALTH, e4, 3);

    ASSERT_EQ(freecs_query_count(&world, BIT_POSITION, 0), 4);
    ASSERT_EQ(freecs_query_count(&world, BIT_VELOCITY, 0), 2);
    ASSERT_EQ(freecs_query_count(&world, BIT_HEALTH, 0), 1);
    ASSERT_EQ(freecs_query_count(&world, BIT_POSITION | BIT_VELOCITY, 0), 2);

    freecs_destroy_world(&world);
}

TEST(add_component) {
    freecs_world_t world = freecs_create_world();
    setup_world(&world);

    Position pos = {1, 2};
    freecs_type_info_entry_t e[1] = {{BIT_POSITION, sizeof(Position), &pos, freecs_bit_index(BIT_POSITION)}};
    freecs_entity_t entity = freecs_spawn(&world, BIT_POSITION, e, 1);

    ASSERT(!freecs_has(&world, entity, BIT_VELOCITY));

    Velocity vel = {5, 6};
    freecs_add_component(&world, entity, BIT_VELOCITY, &vel, sizeof(Velocity));

    ASSERT(freecs_has(&world, entity, BIT_VELOCITY));
    Velocity* got_vel = FREECS_GET(&world, entity, Velocity, BIT_VELOCITY);
    ASSERT(got_vel != NULL);
    ASSERT_FLOAT_EQ(got_vel->x, 5);
    ASSERT_FLOAT_EQ(got_vel->y, 6);

    Position* got_pos = FREECS_GET(&world, entity, Position, BIT_POSITION);
    ASSERT(got_pos != NULL);
    ASSERT_FLOAT_EQ(got_pos->x, 1);
    ASSERT_FLOAT_EQ(got_pos->y, 2);

    freecs_destroy_world(&world);
}

TEST(remove_component) {
    freecs_world_t world = freecs_create_world();
    setup_world(&world);

    Position pos = {1, 2};
    Velocity vel = {3, 4};
    freecs_type_info_entry_t e[2] = {
        {BIT_POSITION, sizeof(Position), &pos, freecs_bit_index(BIT_POSITION)},
        {BIT_VELOCITY, sizeof(Velocity), &vel, freecs_bit_index(BIT_VELOCITY)}
    };
    freecs_entity_t entity = freecs_spawn(&world, BIT_POSITION | BIT_VELOCITY, e, 2);

    ASSERT(freecs_has(&world, entity, BIT_VELOCITY));

    freecs_remove_component(&world, entity, BIT_VELOCITY);

    ASSERT(!freecs_has(&world, entity, BIT_VELOCITY));
    ASSERT(freecs_has(&world, entity, BIT_POSITION));

    Position* got_pos = FREECS_GET(&world, entity, Position, BIT_POSITION);
    ASSERT(got_pos != NULL);
    ASSERT_FLOAT_EQ(got_pos->x, 1);
    ASSERT_FLOAT_EQ(got_pos->y, 2);

    freecs_destroy_world(&world);
}

TEST(spawn_batch) {
    freecs_world_t world = freecs_create_world();
    setup_world(&world);

    size_t count;
    freecs_entity_t* entities = freecs_spawn_batch(&world, BIT_POSITION | BIT_VELOCITY, 5, &count);

    ASSERT_EQ(count, 5);
    ASSERT_EQ(freecs_entity_count(&world), 5);

    for (size_t i = 0; i < count; i++) {
        ASSERT(freecs_has(&world, entities[i], BIT_POSITION));
        ASSERT(freecs_has(&world, entities[i], BIT_VELOCITY));
    }

    free(entities);
    freecs_destroy_world(&world);
}

TEST(event_queue) {
    typedef struct {
        uint32_t entity_id;
        uint32_t reward;
    } EnemyDiedEvent;

    freecs_event_queue_t queue = FREECS_CREATE_EVENT_QUEUE(EnemyDiedEvent);

    EnemyDiedEvent ev1 = {1, 10};
    EnemyDiedEvent ev2 = {2, 20};
    freecs_send_event(&queue, &ev1);
    freecs_send_event(&queue, &ev2);

    ASSERT_EQ(freecs_event_count(&queue), 2);

    size_t count;
    EnemyDiedEvent* events = FREECS_READ_EVENTS(&queue, EnemyDiedEvent, &count);
    ASSERT_EQ(count, 2);
    ASSERT_EQ(events[0].entity_id, 1);
    ASSERT_EQ(events[0].reward, 10);
    ASSERT_EQ(events[1].entity_id, 2);
    ASSERT_EQ(events[1].reward, 20);

    freecs_clear_events(&queue);
    ASSERT_EQ(freecs_event_count(&queue), 0);

    freecs_destroy_event_queue(&queue);
}

TEST(tags) {
    freecs_world_t world = freecs_create_world();
    setup_world(&world);
    freecs_tags_t tags = freecs_create_tags();

    int tag_selected = freecs_register_tag(&tags, "selected");
    int tag_highlighted = freecs_register_tag(&tags, "highlighted");

    Position pos1 = {1, 1};
    Position pos2 = {2, 2};
    freecs_type_info_entry_t e1[1] = {{BIT_POSITION, sizeof(Position), &pos1, freecs_bit_index(BIT_POSITION)}};
    freecs_type_info_entry_t e2[1] = {{BIT_POSITION, sizeof(Position), &pos2, freecs_bit_index(BIT_POSITION)}};
    freecs_entity_t entity1 = freecs_spawn(&world, BIT_POSITION, e1, 1);
    freecs_entity_t entity2 = freecs_spawn(&world, BIT_POSITION, e2, 1);

    freecs_add_tag(&tags, tag_selected, entity1);
    freecs_add_tag(&tags, tag_highlighted, entity1);
    freecs_add_tag(&tags, tag_highlighted, entity2);

    ASSERT(freecs_has_tag(&tags, tag_selected, entity1));
    ASSERT(!freecs_has_tag(&tags, tag_selected, entity2));
    ASSERT(freecs_has_tag(&tags, tag_highlighted, entity1));
    ASSERT(freecs_has_tag(&tags, tag_highlighted, entity2));

    ASSERT_EQ(freecs_tag_count(&tags, tag_selected), 1);
    ASSERT_EQ(freecs_tag_count(&tags, tag_highlighted), 2);

    freecs_remove_tag(&tags, tag_highlighted, entity1);
    ASSERT(!freecs_has_tag(&tags, tag_highlighted, entity1));
    ASSERT_EQ(freecs_tag_count(&tags, tag_highlighted), 1);

    freecs_destroy_tags(&tags);
    freecs_destroy_world(&world);
}

TEST(matching_archetypes_and_columns) {
    freecs_world_t world = freecs_create_world();
    setup_world(&world);

    Position pos1 = {1.0f, 2.0f};
    Position pos2 = {3.0f, 4.0f};
    Position pos3 = {5.0f, 6.0f};
    Velocity vel1 = {10.0f, 20.0f};

    freecs_type_info_entry_t e1[1] = {{BIT_POSITION, sizeof(Position), &pos1, freecs_bit_index(BIT_POSITION)}};
    freecs_type_info_entry_t e2[1] = {{BIT_POSITION, sizeof(Position), &pos2, freecs_bit_index(BIT_POSITION)}};
    freecs_type_info_entry_t e3[2] = {
        {BIT_POSITION, sizeof(Position), &pos3, freecs_bit_index(BIT_POSITION)},
        {BIT_VELOCITY, sizeof(Velocity), &vel1, freecs_bit_index(BIT_VELOCITY)}
    };

    freecs_spawn(&world, BIT_POSITION, e1, 1);
    freecs_spawn(&world, BIT_POSITION, e2, 1);
    freecs_spawn(&world, BIT_POSITION | BIT_VELOCITY, e3, 2);

    size_t matching_count;
    size_t* matching = freecs_get_matching_archetypes(&world, BIT_POSITION, 0, &matching_count);

    ASSERT(matching_count > 0);

    size_t total_entities = 0;
    for (size_t i = 0; i < matching_count; i++) {
        freecs_archetype_t* arch = &world.archetypes[matching[i]];
        Position* positions = freecs_column_unchecked(arch, BIT_POSITION);

        ASSERT(positions != NULL);

        for (size_t j = 0; j < arch->entities_len; j++) {
            total_entities++;
            ASSERT(positions[j].x >= 1.0f && positions[j].x <= 5.0f);
        }
    }

    ASSERT_EQ(total_entities, 3);

    freecs_destroy_world(&world);
}

TEST(queue_despawn) {
    freecs_world_t world = freecs_create_world();
    setup_world(&world);

    Position pos1 = {1.0f, 2.0f};
    Position pos2 = {3.0f, 4.0f};

    freecs_type_info_entry_t e1[1] = {{BIT_POSITION, sizeof(Position), &pos1, freecs_bit_index(BIT_POSITION)}};
    freecs_type_info_entry_t e2[1] = {{BIT_POSITION, sizeof(Position), &pos2, freecs_bit_index(BIT_POSITION)}};

    freecs_entity_t ent1 = freecs_spawn(&world, BIT_POSITION, e1, 1);
    freecs_entity_t ent2 = freecs_spawn(&world, BIT_POSITION, e2, 1);

    ASSERT_EQ(freecs_entity_count(&world), 2);

    freecs_queue_despawn(&world, ent1);

    ASSERT_EQ(freecs_entity_count(&world), 2);
    ASSERT(freecs_is_alive(&world, ent1));

    freecs_apply_despawns(&world);

    ASSERT_EQ(freecs_entity_count(&world), 1);
    ASSERT(!freecs_is_alive(&world, ent1));
    ASSERT(freecs_is_alive(&world, ent2));

    freecs_destroy_world(&world);
}

int main(void) {
    printf("Running freecs tests...\n\n");
    fflush(stdout);

    RUN_TEST(spawn_entity);
    RUN_TEST(get_component);
    RUN_TEST(set_component);
    RUN_TEST(despawn_entity);
    RUN_TEST(generational_indices);
    RUN_TEST(multiple_archetypes);
    RUN_TEST(query_count);
    RUN_TEST(add_component);
    RUN_TEST(remove_component);
    RUN_TEST(spawn_batch);
    RUN_TEST(event_queue);
    RUN_TEST(tags);
    RUN_TEST(matching_archetypes_and_columns);
    RUN_TEST(queue_despawn);

    printf("\n%d/%d tests passed\n", tests_passed, tests_run);

    return tests_passed == tests_run ? 0 : 1;
}
