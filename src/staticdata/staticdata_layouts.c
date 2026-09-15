/**
 * staticdata_layouts.c - Typed field layouts for StaticData entries
 *
 * Layouts follow the Windows reference (BG3Extender/GameDefinitions/
 * GuidResources.h). Every field is naturally aligned on both platforms, so
 * the ARM64 offsets match the Windows declaration order; the entry stride
 * and the first entry's fields are confirmed live before a layout ships
 * (docs/parity-100/LIVE-VERIFICATION-2026-09-14.md).
 */

#include "staticdata_layouts.h"

// ----------------------------------------------------------------------------
// eoc::CharacterCreationAppearanceVisual (GuidResources.h:702, issue #100)
// ----------------------------------------------------------------------------
//
//   +0x00 VMT                       +0x58 Guid HeadAppearanceUUID
//   +0x08 Guid ResourceUUID         +0x68 Guid DefaultSkinColor
//   +0x18 Guid RootTemplate         +0x78 TranslatedString DisplayName
//   +0x28 Guid RaceUUID             +0x88 FixedString IconIdOverride
//   +0x38 uint8_t BodyType          +0x8C uint8_t DefaultForBodyType
//   +0x39 uint8_t BodyShape         +0x90 FixedString TextureEntryPart
//   +0x3C uint32_t field_3C         +0x98 Array<Guid> Tags
//   +0x40 FixedString SlotName      = 0xA8 bytes
//   +0x48 Guid VisualResource
//
static const StaticDataField s_cc_appearance_visual_fields[] = {
    { "RootTemplate",        SD_FIELD_GUID,             0x18 },
    { "RaceUUID",            SD_FIELD_GUID,             0x28 },
    { "BodyType",            SD_FIELD_U8,               0x38 },
    { "BodyShape",           SD_FIELD_U8,               0x39 },
    { "field_3C",            SD_FIELD_U32,              0x3C },
    { "SlotName",            SD_FIELD_FIXEDSTRING,      0x40 },
    { "VisualResource",      SD_FIELD_GUID,             0x48 },
    { "HeadAppearanceUUID",  SD_FIELD_GUID,             0x58 },
    { "DefaultSkinColor",    SD_FIELD_GUID,             0x68 },
    { "DisplayName",         SD_FIELD_TRANSLATEDSTRING, 0x78 },
    { "IconIdOverride",      SD_FIELD_FIXEDSTRING,      0x88 },
    { "DefaultForBodyType",  SD_FIELD_U8,               0x8C },
    { "TextureEntryPart",    SD_FIELD_FIXEDSTRING,      0x90 },
    { "Tags",                SD_FIELD_GUID_ARRAY,       0x98 },
};

static const StaticDataLayout s_cc_appearance_visual_layout = {
    .engine_class = "eoc::CharacterCreationAppearanceVisualManager",
    .entry_size   = 0xA8,
    .fields       = s_cc_appearance_visual_fields,
    .field_count  = (int)(sizeof(s_cc_appearance_visual_fields) /
                          sizeof(s_cc_appearance_visual_fields[0])),
};

const StaticDataLayout *staticdata_layout_for(StaticDataType type) {
    switch (type) {
        case STATICDATA_CC_APPEARANCE_VISUAL:
            return &s_cc_appearance_visual_layout;
        default:
            return NULL;
    }
}

size_t staticdata_field_size(StaticDataFieldKind kind) {
    switch (kind) {
        case SD_FIELD_GUID:             return 16;
        case SD_FIELD_U8:               return 1;
        case SD_FIELD_BOOL:             return 1;
        case SD_FIELD_U32:              return 4;
        case SD_FIELD_FIXEDSTRING:      return 4;
        case SD_FIELD_TRANSLATEDSTRING: return 16;  // two RuntimeStringHandles
        case SD_FIELD_GUID_ARRAY:       return 16;  // buf + capacity + size
    }
    return 0;
}
