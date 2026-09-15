/*
 * Tier 0 tests for StaticData typed layouts (src/staticdata/staticdata_layouts.c).
 *
 * Pure data audit: every shipped layout must have ascending, non-overlapping,
 * naturally aligned fields that fit inside its entry size, and the
 * CharacterCreationAppearanceVisual table must match the Windows declaration
 * order (GuidResources.h:702) that the 2026-09-15 live probe confirmed.
 * No game process, no game memory.
 */

#include "test_harness.h"
#include "staticdata_layouts.h"

static const StaticDataField *find_field(const StaticDataLayout *layout, const char *name) {
    for (int i = 0; i < layout->field_count; i++) {
        if (strcmp(layout->fields[i].name, name) == 0) {
            return &layout->fields[i];
        }
    }
    return NULL;
}

static size_t field_alignment(StaticDataFieldKind kind) {
    switch (kind) {
        case SD_FIELD_GUID:             return 8;   // two uint64_t
        case SD_FIELD_GUID_ARRAY:       return 8;   // leading pointer
        case SD_FIELD_TRANSLATEDSTRING: return 4;   // FixedString + uint16_t
        case SD_FIELD_U32:              return 4;
        case SD_FIELD_FIXEDSTRING:      return 4;
        case SD_FIELD_U8:               return 1;
        case SD_FIELD_BOOL:             return 1;
    }
    return 1;
}

// ---------------------------------------------------------------------------
// Generic layout audit
// ---------------------------------------------------------------------------

TEST(types_without_layout_return_null) {
    ASSERT_NULL(staticdata_layout_for(STATICDATA_FEAT));
    ASSERT_NULL(staticdata_layout_for(STATICDATA_RACE));
    ASSERT_NULL(staticdata_layout_for(STATICDATA_COUNT));
    ASSERT_NULL(staticdata_layout_for((StaticDataType)-1));
}

TEST(every_layout_is_well_formed) {
    int audited = 0;
    for (int t = 0; t < STATICDATA_COUNT; t++) {
        const StaticDataLayout *layout = staticdata_layout_for((StaticDataType)t);
        if (!layout) continue;
        audited++;

        ASSERT_NOT_NULL(layout->engine_class);
        ASSERT_TRUE(strncmp(layout->engine_class, "eoc::", 5) == 0);
        ASSERT_TRUE(layout->entry_size > 0x18);          // VMT + ResourceUUID
        ASSERT_EQ(layout->entry_size % 8, 0);            // Guid members force 8-byte stride
        ASSERT_TRUE(layout->field_count > 0);

        size_t previous_end = 0x18;                      // first type-specific byte
        for (int i = 0; i < layout->field_count; i++) {
            const StaticDataField *f = &layout->fields[i];
            size_t size = staticdata_field_size(f->kind);
            ASSERT_NOT_NULL(f->name);
            ASSERT_TRUE(size > 0);
            ASSERT_TRUE(f->offset >= previous_end);       // ascending, no overlap
            ASSERT_EQ(f->offset % field_alignment(f->kind), 0);
            ASSERT_TRUE(f->offset + size <= layout->entry_size);
            previous_end = f->offset + size;
        }
    }
    ASSERT_TRUE(audited >= 1);
}

TEST(field_sizes_match_engine_types) {
    ASSERT_EQ(staticdata_field_size(SD_FIELD_GUID), 16);
    ASSERT_EQ(staticdata_field_size(SD_FIELD_U8), 1);
    ASSERT_EQ(staticdata_field_size(SD_FIELD_BOOL), 1);
    ASSERT_EQ(staticdata_field_size(SD_FIELD_U32), 4);
    ASSERT_EQ(staticdata_field_size(SD_FIELD_FIXEDSTRING), 4);
    ASSERT_EQ(staticdata_field_size(SD_FIELD_TRANSLATEDSTRING), 16);
    ASSERT_EQ(staticdata_field_size(SD_FIELD_GUID_ARRAY), 16);
}

// ---------------------------------------------------------------------------
// CharacterCreationAppearanceVisual (#100)
// ---------------------------------------------------------------------------

TEST(cc_appearance_visual_matches_windows_declaration) {
    const StaticDataLayout *layout = staticdata_layout_for(STATICDATA_CC_APPEARANCE_VISUAL);
    ASSERT_NOT_NULL(layout);
    ASSERT_STR_EQ(layout->engine_class, "eoc::CharacterCreationAppearanceVisualManager");
    ASSERT_EQ(layout->entry_size, 0xA8);
    ASSERT_EQ(layout->field_count, 13);

    struct { const char *name; StaticDataFieldKind kind; uint16_t offset; } expected[] = {
        { "RootTemplate",       SD_FIELD_GUID,             0x18 },
        { "RaceUUID",           SD_FIELD_GUID,             0x28 },
        { "BodyType",           SD_FIELD_U8,               0x38 },
        { "BodyShape",          SD_FIELD_U8,               0x39 },
        { "SlotName",           SD_FIELD_FIXEDSTRING,      0x40 },
        { "VisualResource",     SD_FIELD_GUID,             0x48 },
        { "HeadAppearanceUUID", SD_FIELD_GUID,             0x58 },
        { "DefaultSkinColor",   SD_FIELD_GUID,             0x68 },
        { "DisplayName",        SD_FIELD_TRANSLATEDSTRING, 0x78 },
        { "IconIdOverride",     SD_FIELD_FIXEDSTRING,      0x88 },
        { "DefaultForBodyType", SD_FIELD_U8,               0x8C },
        { "TextureEntryPart",   SD_FIELD_FIXEDSTRING,      0x90 },
        { "Tags",               SD_FIELD_GUID_ARRAY,       0x98 },
    };
    for (size_t i = 0; i < sizeof(expected) / sizeof(expected[0]); i++) {
        const StaticDataField *f = find_field(layout, expected[i].name);
        ASSERT_NOT_NULL(f);
        ASSERT_EQ(f->kind, expected[i].kind);
        ASSERT_EQ(f->offset, expected[i].offset);
    }

    // The trailing Array<Guid> ends exactly at the entry stride.
    const StaticDataField *tags = find_field(layout, "Tags");
    ASSERT_EQ(tags->offset + staticdata_field_size(tags->kind), layout->entry_size);
}

void register_staticdata_layout_tests(void) {
    printf("StaticData layouts:\n");
    RUN_TEST(types_without_layout_return_null);
    RUN_TEST(every_layout_is_well_formed);
    RUN_TEST(field_sizes_match_engine_types);
    RUN_TEST(cc_appearance_visual_matches_windows_declaration);
}
