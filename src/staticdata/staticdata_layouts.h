/**
 * staticdata_layouts.h - Typed field layouts for StaticData entries
 *
 * A layout describes the fields of one GuidResource subclass as (name, kind,
 * offset) rows so the Lua serializer can expose the Windows property surface
 * without a per-type C struct. Offsets are ARM64 offsets from the entry base
 * (VMT at +0x00, ResourceUUID at +0x08, type-specific fields from +0x18).
 *
 * This module is pure data with no game-memory dependencies so tier 0 can
 * audit every table (offsets ascending, non-overlapping, inside entry_size).
 */

#ifndef STATICDATA_LAYOUTS_H
#define STATICDATA_LAYOUTS_H

#include <stddef.h>
#include <stdint.h>

#include "staticdata_manager.h"

typedef enum {
    SD_FIELD_GUID,              // ls::Guid, 16 bytes, formatted canonically
    SD_FIELD_U8,                // uint8_t
    SD_FIELD_BOOL,              // uint8_t read as boolean
    SD_FIELD_U32,               // uint32_t
    SD_FIELD_FIXEDSTRING,       // ls::FixedString index, resolved to text
    SD_FIELD_TRANSLATEDSTRING,  // ls::TranslatedString: Handle + ArgumentString
    SD_FIELD_GUID_ARRAY,        // ls::Array<ls::Guid>: buf, capacity, size
} StaticDataFieldKind;

typedef struct StaticDataField {
    const char *name;           // Lua property name (Windows GuidResources.h spelling)
    StaticDataFieldKind kind;
    uint16_t offset;            // Byte offset from the entry base
} StaticDataField;

typedef struct {
    const char *engine_class;   // Manager class as registered in the TypeContext
    uint16_t entry_size;        // sizeof(T) on ARM64
    const StaticDataField *fields;
    int field_count;
} StaticDataLayout;

/**
 * Typed layout for a StaticData type, or NULL when the type only exposes the
 * generic ResourceUUID/Name surface.
 */
const StaticDataLayout *staticdata_layout_for(StaticDataType type);

/**
 * In-memory size of one field kind (0 for kinds without a fixed size).
 */
size_t staticdata_field_size(StaticDataFieldKind kind);

#endif // STATICDATA_LAYOUTS_H
