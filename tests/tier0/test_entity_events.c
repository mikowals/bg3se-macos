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

/* ── Registration ────────────────────────────────────────────────── */

void register_entity_events_tests(void) {
    printf("[entity_events]\n");
    RUN_TEST(make_sub_id_roundtrip);
    RUN_TEST(sub_id_type_extraction);
    RUN_TEST(sub_id_index_extraction);
    RUN_TEST(sub_id_invalid_is_zero);
    RUN_TEST(sub_id_all_types);
    RUN_TEST(sub_id_max_index);
}
