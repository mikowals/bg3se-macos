/*
 * Tier 0 tests for entity event subscription ID macros.
 *
 * Tests MAKE_SUB_ID / SUB_ID_TYPE / SUB_ID_INDEX from
 * src/entity/entity_events.h. Pure bitwise macros — zero deps.
 */

#include "test_harness.h"
#include "entity_events.h"

/* ── MAKE_SUB_ID roundtrip ───────────────────────────────────────── */

TEST(make_sub_id_roundtrip) {
    EntitySubscriptionId id = MAKE_SUB_ID(2, 0x1234);
    ASSERT_EQ(SUB_ID_TYPE(id), 2u);
    ASSERT_EQ(SUB_ID_INDEX(id), 0x1234u);
}

TEST(sub_id_type_extraction) {
    EntitySubscriptionId id = MAKE_SUB_ID(SUB_TYPE_COMPONENT, 0);
    ASSERT_EQ(SUB_ID_TYPE(id), (uint32_t)SUB_TYPE_COMPONENT);
}

TEST(sub_id_index_extraction) {
    EntitySubscriptionId id = MAKE_SUB_ID(1, 0xDEADBEEF);
    ASSERT_EQ(SUB_ID_INDEX(id), 0xDEADBEEFu);
}

TEST(sub_id_invalid_is_zero) {
    ASSERT_EQ(ENTITY_SUB_INVALID, 0u);
}

TEST(sub_id_all_types) {
    for (uint32_t t = 0; t < 5; t++) {
        EntitySubscriptionId id = MAKE_SUB_ID(t, 42);
        ASSERT_EQ(SUB_ID_TYPE(id), t);
        ASSERT_EQ(SUB_ID_INDEX(id), 42u);
    }
}

TEST(sub_id_max_index) {
    EntitySubscriptionId id = MAKE_SUB_ID(1, 0xFFFFFFFF);
    ASSERT_EQ(SUB_ID_INDEX(id), 0xFFFFFFFFu);
    ASSERT_EQ(SUB_ID_TYPE(id), 1u);
}


/* ── AUDIT ADDITIONS ─────────────────────────────────────────────── */

/* sub_id_type_extraction above feeds SUB_TYPE_COMPONENT into MAKE_SUB_ID and
 * then compares against SUB_TYPE_COMPONENT — it holds for ANY value of the
 * constant. Redefining SUB_TYPE_COMPONENT from 2 to 99 kept all 68 tests
 * green, even though entity_events.c stores that tag in every subscription id
 * it hands to Lua and ecs_system_update.c routes on it. Pin the wire values
 * and their distinctness. */
TEST(sub_type_tags_pinned_and_distinct) {
    ASSERT_EQ((uint32_t)SUB_TYPE_REPLICATION, 1u);
    ASSERT_EQ((uint32_t)SUB_TYPE_COMPONENT,   2u);
    ASSERT_EQ((uint32_t)SUB_TYPE_SYSTEM,      3u);

    /* No tag may collide with the invalid sentinel, and a tagged id is never
     * zero regardless of index — entity_events_unsubscribe() rejects 0 first. */
    ASSERT_NE(MAKE_SUB_ID(SUB_TYPE_REPLICATION, 0), ENTITY_SUB_INVALID);
    ASSERT_NE(MAKE_SUB_ID(SUB_TYPE_COMPONENT, 0),   ENTITY_SUB_INVALID);
    ASSERT_NE(MAKE_SUB_ID(SUB_TYPE_SYSTEM, 0),      ENTITY_SUB_INVALID);

    ASSERT_NE(MAKE_SUB_ID(SUB_TYPE_REPLICATION, 7),
              MAKE_SUB_ID(SUB_TYPE_COMPONENT, 7));
    ASSERT_NE(MAKE_SUB_ID(SUB_TYPE_COMPONENT, 7),
              MAKE_SUB_ID(SUB_TYPE_SYSTEM, 7));
}

/* The id must carry a full 32-bit payload with no cross-talk into the type
 * tag. The payload shape (salt<<16 | index) mirrors what entity_events.c's
 * pool_pack() produces, but pool_pack() itself is not linked into tier 0:
 * this pins the MAKE_SUB_ID/SUB_ID_* macros only. */
TEST(sub_id_carries_full_salt_and_index) {
    const uint32_t packed = (0xBEEFu << 16) | 0x00FFu;   /* salt 0xBEEF, idx 255 */
    EntitySubscriptionId id = MAKE_SUB_ID(SUB_TYPE_COMPONENT, packed);

    ASSERT_EQ(SUB_ID_INDEX(id), packed);
    ASSERT_EQ(SUB_ID_TYPE(id), 2u);
    ASSERT_EQ((SUB_ID_INDEX(id) >> 16) & 0xFFFFu, 0xBEEFu);
    ASSERT_EQ(SUB_ID_INDEX(id) & 0xFFFFu, 0x00FFu);
}

/* A high index must not bleed into the type field (the cast in MAKE_SUB_ID is
 * what prevents sign-extension from an int index). -1 is the all-ones int;
 * (int)0xFFFFFFFF would say the same thing with implementation-defined
 * conversion. */
TEST(sub_id_high_index_does_not_bleed_into_type) {
    EntitySubscriptionId id = MAKE_SUB_ID(SUB_TYPE_SYSTEM, -1);
    ASSERT_EQ(SUB_ID_TYPE(id), 3u);
    ASSERT_EQ(SUB_ID_INDEX(id), 0xFFFFFFFFu);
    ASSERT_EQ(id, ((uint64_t)3 << 32) | 0xFFFFFFFFull);
}

/* ── Registration ────────────────────────────────────────────────── */

void register_entity_events_tests(void) {
    printf("[entity_events]\n");
    RUN_TEST(make_sub_id_roundtrip);
    RUN_TEST(sub_id_type_extraction);
    RUN_TEST(sub_id_index_extraction);
    RUN_TEST(sub_id_invalid_is_zero);
    RUN_TEST(sub_id_all_types);
    RUN_TEST(sub_id_max_index);
    RUN_TEST(sub_type_tags_pinned_and_distinct);
    RUN_TEST(sub_id_carries_full_salt_and_index);
    RUN_TEST(sub_id_high_index_does_not_bleed_into_type);
}
