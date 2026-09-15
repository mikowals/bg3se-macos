/**
 * staticdata_manager.c - StaticData Manager Implementation for BG3SE-macOS
 *
 * Captures static data managers via hooks and provides access for Lua API.
 */

#include "staticdata_manager.h"
#include "../core/logging.h"
#include "../core/safe_memory.h"
#include "../core/version_detect.h"
#include "../core/offset_table.h"
#include "../strings/fixed_string.h"
#include "../hooks/arm64_hook.h"
#include "../entity/guid_lookup.h"
#include "staticdata_layouts.h"
#include <dobby.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

// ============================================================================
// Constants and Offsets
// ============================================================================
//
// All address-dependent values (the m_State pointer, the FeatManager::GetFeats
// and GetAllFeats functions, and the ImmutableDataHeadmaster::Get<T> accessors)
// are sourced per game version from src/core/offset_table.c. See the
// VersionOffsets fields: staticdata_mstate_ptr, fn_feat_getfeats,
// fn_getallfeats, fn_get_background/origin/class/progression/actionresource.

// FeatManager structure offsets
//
// TypeContext Metadata (GuidResourceBank HashMap - Dec 20, 2025 discovery):
//   +0x7C: Keys.size_ (entry count, e.g., 37)
//   +0x80: Values.buf_ (array of POINTERS to Feat structs)
//   NOTE: This is a HashMap, so +0x80 contains pointers, NOT flat structs!
//
// Session FeatManager (from GetFeats hook - only during character creation):
//   +0x7C: int32_t count
//   +0x80: Feat* array (flat array of Feat structs, each 0x128 bytes)
//
// The key difference: TypeContext Values.buf_ is an array of POINTERS,
// while Session FeatManager has a flat array of structs.
//
// Sanity ceilings for values read from game memory. A bank or Array<Guid>
// whose header exceeds these is treated as unreadable rather than iterated:
// Progression is the largest bank on 7398727 at 1004 entries, and no
// GuidResource carries more than a handful of tag GUIDs.
#define STATICDATA_MAX_BANK_ENTRIES      65536
#define STATICDATA_MAX_GUID_ARRAY        4096
#define STATICDATA_MAX_HASH_SLOTS        (1 << 20)
#define STATICDATA_MIN_ENTRY_STRIDE      0x18   // vtable + ResourceUUID is the smallest GuidResource

#define FEATMANAGER_REAL_COUNT_OFFSET    0x7C   // GuidResourceBank<T>::Resources.Keys.size
#define FEATMANAGER_REAL_ARRAY_OFFSET    0x80   // GuidResourceBank<T>::Resources.Values.buf (flat T[])

// Structure offsets (verified via Windows BG3SE GuidResources.h)
// Base class GuidResource: VMT (8) + ResourceUUID (16) = 24 bytes (0x18)
// Then type-specific fields follow

// Entry strides are sizeof(T) for the bank's flat Values array. Every value
// below marked "live" was measured on 4.1.1.7398727 (2026-09-15) by walking
// the array for the next repeat of the entry vtable pointer; the runtime
// re-measures on first use and overrides a stale constant (see
// effective_entry_size), so a game update degrades to a logged warning, not
// to garbage entries.

// Feat structure
#define FEAT_SIZE                     0x128  // 296 bytes per feat (Ghidra + live)
#define FEAT_OFFSET_NAME              0x18   // FixedString Name (after GuidResource base)

// Race structure
#define RACE_SIZE                     0x168  // 360 bytes (live; was a 0x200 estimate)
#define RACE_OFFSET_NAME              0x18   // FixedString Name (same as Feat)

// Origin structure
// Has uint8_t AvailableInCharacterCreation at +0x18 before Name
#define ORIGIN_SIZE                   0x190  // 400 bytes (live on 7398727; re-measured at runtime)
#define ORIGIN_OFFSET_NAME            0x1C   // FixedString Name (aligned after uint8_t)

// Background structure - NO Name field, only DisplayName (TranslatedString)
#define BACKGROUND_SIZE               0x70   // 112 bytes (live; was a 0x80 estimate)
#define BACKGROUND_OFFSET_NAME        0      // No FixedString Name field!

// God structure
#define GOD_SIZE                      0x60   // 96 bytes (live)
#define GOD_OFFSET_NAME               0x18   // FixedString Name

// ClassDescription structure
// Has Guid ParentGuid (16 bytes) at +0x18 before Name
#define CLASS_SIZE                    0x110  // 272 bytes (live; was a 0x100 estimate)
#define CLASS_OFFSET_NAME             0x28   // FixedString Name (after ParentGuid)

// ============================================================================
// Manager Configuration (per-type structure info)
// ============================================================================

typedef struct {
    int count_offset;    // Offset to count field in manager
    int array_offset;    // Offset to array pointer in manager
    int entry_size;      // Size of each entry
    int name_offset;     // Offset to Name FixedString in entry (0 = no name)
    const char* capture_file;  // Path to Frida capture file
} ManagerConfig;

static const ManagerConfig g_manager_configs[STATICDATA_COUNT] = {
    // STATICDATA_FEAT
    { 0x7C, 0x80, FEAT_SIZE, FEAT_OFFSET_NAME, "/tmp/bg3se_featmanager.txt" },
    // STATICDATA_RACE
    { 0x7C, 0x80, RACE_SIZE, RACE_OFFSET_NAME, "/tmp/bg3se_racemanager.txt" },
    // STATICDATA_BACKGROUND
    { 0x7C, 0x80, BACKGROUND_SIZE, 0, "/tmp/bg3se_backgroundmanager.txt" },  // No Name
    // STATICDATA_ORIGIN
    { 0x7C, 0x80, ORIGIN_SIZE, ORIGIN_OFFSET_NAME, "/tmp/bg3se_originmanager.txt" },
    // STATICDATA_GOD
    { 0x7C, 0x80, GOD_SIZE, GOD_OFFSET_NAME, "/tmp/bg3se_godmanager.txt" },
    // STATICDATA_CLASS
    { 0x7C, 0x80, CLASS_SIZE, CLASS_OFFSET_NAME, "/tmp/bg3se_classmanager.txt" },
    // STATICDATA_PROGRESSION (estimate, re-measured at runtime)
    { 0x7C, 0x80, 0x148, 0x18, "/tmp/bg3se_progressionmanager.txt" },  // 0x148 live on 7398727
    // STATICDATA_ACTIONRESOURCE (0x60 live; was a 0x80 estimate)
    { 0x7C, 0x80, 0x60, 0x18, "/tmp/bg3se_actionresourcemanager.txt" },
    // STATICDATA_FEATDESCRIPTION (0x60 live; was a 0x80 estimate)
    { 0x7C, 0x80, 0x60, 0, "/tmp/bg3se_featdescmanager.txt" },  // Has TranslatedString, not FixedString
    // STATICDATA_CC_APPEARANCE_VISUAL (#100) — no Name; typed fields in staticdata_layouts.c
    { 0x7C, 0x80, 0xA8, 0, "/tmp/bg3se_ccappearancevisualmanager.txt" },
};

// ============================================================================
// Type Name Table
// ============================================================================

static const char* s_type_names[STATICDATA_COUNT] = {
    "Feat",
    "Race",
    "Background",
    "Origin",
    "God",
    "Class",
    "Progression",
    "ActionResource",
    "FeatDescription",
    "CharacterCreationAppearanceVisual"
};

// Manager type names as they appear in TypeContext (for name-based capture)
// Names sourced from Windows BG3SE GuidResources.h EngineClass definitions
static const char* s_manager_type_names[STATICDATA_COUNT] = {
    "eoc::FeatManager",
    "eoc::RaceManager",
    "eoc::BackgroundManager",
    "eoc::OriginManager",
    "eoc::GodManager",
    "eoc::ClassDescriptions",           // Was ClassManager - corrected per GuidResources.h
    "eoc::ProgressionManager",
    "eoc::ActionResourceTypes",         // Was ActionResourceManager - corrected per GuidResources.h
    "eoc::FeatDescriptionManager",
    "eoc::CharacterCreationAppearanceVisualManager"
};

// ============================================================================
// Module State
// ============================================================================

static struct {
    bool initialized;
    void* main_binary_base;

    // TypeContext slots: the address of ls::TypeId<Manager, ImmutableDataHeadmaster>::
    // m_TypeIndex for each type (an int32 in the game image). Never a bank;
    // used only to resolve the bank through the headmaster hash table.
    void* managers[STATICDATA_COUNT];

    // Real GuidResourceBank<T> pointers (hash lookup, Get<T> hooks, Frida)
    void* real_managers[STATICDATA_COUNT];

    // sizeof(T) measured from the live Values array (0 = not measured yet)
    int entry_stride[STATICDATA_COUNT];

    // Original function pointers (for hooks)
    void* orig_feat_getfeats;

    // ARM64 safe hook handles
    ARM64HookHandle* feat_getfeats_hook;
} g_staticdata = {0};

static int effective_entry_size(StaticDataType type, void* bank);

// ============================================================================
// TypeContext Traversal (Alternative capture method)
// ============================================================================

/**
 * TypeInfo structure in ImmutableDataHeadmaster TypeContext
 */
typedef struct TypeInfo {
    void*    manager_ptr;     // +0x00: Pointer to manager instance
    void*    type_name;       // +0x08: FixedString or char* type name
    uint32_t name_length;     // +0x10: Type name length
    uint32_t padding;         // +0x14: Padding
    struct TypeInfo* next;    // +0x18: Next TypeInfo in list
} TypeInfo;

/**
 * Capture all known managers by traversing the ImmutableDataHeadmaster TypeContext.
 * The type_name field is a raw C string pointer (verified at runtime).
 * Returns number of managers captured.
 * Uses safe memory reads to prevent crashes.
 */
static int capture_managers_via_typecontext(void) {
    if (!g_staticdata.main_binary_base) {
        return 0;
    }

    // Safely get pointer to m_State (address from per-version offset table)
    const VersionOffsets *off = offset_table_get();
    if (!off || !off->staticdata_mstate_ptr) {
        log_message("[StaticData] staticdata_mstate_ptr not available for this version");
        return 0;
    }
    void* m_state = NULL;
    mach_vm_address_t ptr_mstate_addr = (mach_vm_address_t)offset_table_resolve(off->staticdata_mstate_ptr);
    if (!safe_memory_read_pointer(ptr_mstate_addr, &m_state)) {
        log_message("[StaticData] Could not read m_State pointer at %p", (void*)ptr_mstate_addr);
        return 0;
    }
    if (!m_state) {
        log_message("[StaticData] m_State is NULL - TypeContext not available yet");
        return 0;
    }

    log_message("[StaticData] TypeContext traversal: m_State at %p", m_state);

    // Safely read TypeInfo head at m_State + 8
    TypeInfo* typeinfo = NULL;
    if (!safe_memory_read_pointer((mach_vm_address_t)m_state + 8, (void**)&typeinfo)) {
        log_message("[StaticData] Could not read TypeInfo head pointer");
        return 0;
    }

    int captured = 0;
    int count = 0;
    while (typeinfo && count < 200) {  // Safety limit (100+ managers exist)
        // Safely read TypeInfo fields
        void* manager_ptr = NULL;
        void* type_name_ptr = NULL;
        TypeInfo* next_ptr = NULL;

        if (!safe_memory_read_pointer((mach_vm_address_t)&typeinfo->manager_ptr, &manager_ptr) ||
            !safe_memory_read_pointer((mach_vm_address_t)&typeinfo->type_name, &type_name_ptr) ||
            !safe_memory_read_pointer((mach_vm_address_t)&typeinfo->next, (void**)&next_ptr)) {
            log_message("[StaticData] Could not read TypeInfo fields at %p, stopping traversal", typeinfo);
            break;
        }

        // Check if this TypeInfo has a valid manager and name
        if (manager_ptr && type_name_ptr) {
            // Safely read type name string (up to 128 chars)
            char name[128] = {0};
            if (safe_memory_read_string((mach_vm_address_t)type_name_ptr, name, sizeof(name))) {
                // Log first 20 entries and any containing "Feat", "Manager", "Class", "Action", or "Descriptions"
                if (count < 20 || strstr(name, "Feat") || strstr(name, "Manager") ||
                    strstr(name, "Class") || strstr(name, "Action") || strstr(name, "Descriptions")) {
                    log_message("[StaticData] TypeInfo[%d]: %s @ %p", count, name, manager_ptr);
                }

                // Try to match against known manager names
                for (int i = 0; i < STATICDATA_COUNT; i++) {
                    // Only capture if not already captured
                    if (!g_staticdata.managers[i] && strcmp(name, s_manager_type_names[i]) == 0) {
                        g_staticdata.managers[i] = manager_ptr;
                        log_message("[StaticData] *** MATCHED *** %s = %s @ %p",
                                    s_type_names[i], s_manager_type_names[i], manager_ptr);

                        // manager_ptr is the type's m_TypeIndex global (2026-09-15,
                        // 7398727: eoc::FeatManager slot == unslid 0x108927c30, the
                        // nm address of ls::TypeId<eoc::FeatManager,
                        // ImmutableDataHeadmaster>::m_TypeIndex). The bank itself is
                        // resolved from that index by capture_bank_via_hash_lookup().

                        captured++;
                        break;
                    }
                }
            }
        }

        typeinfo = next_ptr;
        count++;
    }

    log_message("[StaticData] TypeContext traversal: scanned %d entries, captured %d managers", count, captured);
    return captured;
}

// ============================================================================
// Hook Functions
// ============================================================================

/**
 * Hook for FeatManager::GetFeats
 * Signature: void GetFeats(OutputArray* out, FeatManager* this)
 * On ARM64: x0 = out, x1 = FeatManager*
 */
typedef void (*FeatGetFeats_t)(void* out, void* feat_manager);
static FeatGetFeats_t g_orig_FeatGetFeats = NULL;

static void hook_FeatGetFeats(void* out, void* feat_manager) {
    // Capture REAL FeatManager pointer (the one with count@+0x7C, array@+0x80)
    // This goes to real_managers, not managers (which holds TypeContext metadata)
    if (feat_manager && !g_staticdata.real_managers[STATICDATA_FEAT]) {
        g_staticdata.real_managers[STATICDATA_FEAT] = feat_manager;
        log_message("[StaticData] *** HOOK FIRED *** Captured REAL FeatManager: %p", feat_manager);

        // Log structure info using GetFeats-verified offsets (0x7C, 0x80).
        // The manager is live here (we run inside the engine's accessor), but
        // every game read still goes through safe_memory_* by convention.
        int32_t count = 0;
        void* array = NULL;
        if (safe_memory_read_i32((mach_vm_address_t)feat_manager + FEATMANAGER_REAL_COUNT_OFFSET, &count) &&
            safe_memory_read_pointer((mach_vm_address_t)feat_manager + FEATMANAGER_REAL_ARRAY_OFFSET, &array)) {
            log_message("[StaticData] FeatManager structure: count@+0x7C=%d, array@+0x80=%p", count, array);
        }

        // Verify by reading first feat entry
        uint8_t first_feat[16];
        if (array && count > 0 && safe_memory_read((mach_vm_address_t)array, first_feat, sizeof(first_feat))) {
            log_message("[StaticData] First feat at %p, first 16 bytes: %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X %02X%02X%02X%02X",
                array,
                first_feat[0], first_feat[1], first_feat[2], first_feat[3],
                first_feat[4], first_feat[5], first_feat[6], first_feat[7],
                first_feat[8], first_feat[9], first_feat[10], first_feat[11],
                first_feat[12], first_feat[13], first_feat[14], first_feat[15]);
        }
    }

    // Call original
    if (g_orig_FeatGetFeats) {
        g_orig_FeatGetFeats(out, feat_manager);
    }
}

/**
 * Hook for GetAllFeats (called from character creation UI)
 * Signature: void GetAllFeats(Environment* param_1)
 * The FeatManager is at param_1 + 0x130
 */
typedef void (*GetAllFeats_t)(void* environment);
static GetAllFeats_t g_orig_GetAllFeats = NULL;

static void hook_GetAllFeats(void* environment) {
    log_message("[StaticData] GetAllFeats called with env=%p", environment);

    // Try to capture the real FeatManager from environment + 0x130
    if (environment && !g_staticdata.real_managers[STATICDATA_FEAT]) {
        void* feat_manager = NULL;
        if (safe_memory_read_pointer((mach_vm_address_t)environment + 0x130, &feat_manager) && feat_manager) {
            g_staticdata.real_managers[STATICDATA_FEAT] = feat_manager;
            log_message("[StaticData] Captured FeatManager from env+0x130: %p", feat_manager);

            // Log structure info
            int32_t count = 0;
            void* array = NULL;
            if (safe_memory_read_i32((mach_vm_address_t)feat_manager + FEATMANAGER_REAL_COUNT_OFFSET, &count) &&
                safe_memory_read_pointer((mach_vm_address_t)feat_manager + FEATMANAGER_REAL_ARRAY_OFFSET, &array)) {
                log_message("[StaticData] FeatManager: count=%d, array=%p", count, array);
            }
        }
    }

    // Call original
    if (g_orig_GetAllFeats) {
        g_orig_GetAllFeats(environment);
    }
}

// ============================================================================
// Get<T> Hooks (Dec 22, 2025 - Real Manager Capture)
// ============================================================================

/**
 * Hook typedef for Get<T> functions.
 * Signature: Manager* Get(ImmutableDataHeadmaster* this)
 * ARM64: x0 = this, returns manager in x0
 */
typedef void* (*GetManager_t)(void* headmaster);

// Original function pointers
static GetManager_t g_orig_GetBackground = NULL;
static GetManager_t g_orig_GetOrigin = NULL;
static GetManager_t g_orig_GetClass = NULL;
static GetManager_t g_orig_GetProgression = NULL;
static GetManager_t g_orig_GetActionResource = NULL;

// ImmutableDataHeadmaster pointer (captured from Get<T> hooks)
static void* g_immutable_data_headmaster = NULL;

/**
 * Hook for Get<eoc::BackgroundManager>
 */
static void* hook_GetBackground(void* headmaster) {
    // Capture ImmutableDataHeadmaster pointer
    if (headmaster && !g_immutable_data_headmaster) {
        g_immutable_data_headmaster = headmaster;
        log_message("[StaticData] Captured ImmutableDataHeadmaster: %p", headmaster);
    }

    void* result = g_orig_GetBackground ? g_orig_GetBackground(headmaster) : NULL;

    // Capture real manager
    if (result && !g_staticdata.real_managers[STATICDATA_BACKGROUND]) {
        g_staticdata.real_managers[STATICDATA_BACKGROUND] = result;
        log_message("[StaticData] *** GET<T> HOOK *** Captured real BackgroundManager: %p", result);

        // Verify structure
        int32_t count = 0;
        void* array = NULL;
        if (safe_memory_read_i32((mach_vm_address_t)result + 0x7C, &count) &&
            safe_memory_read_pointer((mach_vm_address_t)result + 0x80, &array)) {
            log_message("[StaticData] BackgroundManager: count@+0x7C=%d, array@+0x80=%p", count, array);
        }
    }

    return result;
}

/**
 * Hook for Get<eoc::OriginManager>
 */
static void* hook_GetOrigin(void* headmaster) {
    if (headmaster && !g_immutable_data_headmaster) {
        g_immutable_data_headmaster = headmaster;
    }

    void* result = g_orig_GetOrigin ? g_orig_GetOrigin(headmaster) : NULL;

    if (result && !g_staticdata.real_managers[STATICDATA_ORIGIN]) {
        g_staticdata.real_managers[STATICDATA_ORIGIN] = result;
        log_message("[StaticData] *** GET<T> HOOK *** Captured real OriginManager: %p", result);

        int32_t count = 0;
        if (safe_memory_read_i32((mach_vm_address_t)result + 0x7C, &count)) {
            log_message("[StaticData] OriginManager: count@+0x7C=%d", count);
        }
    }

    return result;
}

/**
 * Hook for Get<eoc::ClassDescriptions>
 */
static void* hook_GetClass(void* headmaster) {
    if (headmaster && !g_immutable_data_headmaster) {
        g_immutable_data_headmaster = headmaster;
    }

    void* result = g_orig_GetClass ? g_orig_GetClass(headmaster) : NULL;

    if (result && !g_staticdata.real_managers[STATICDATA_CLASS]) {
        g_staticdata.real_managers[STATICDATA_CLASS] = result;
        log_message("[StaticData] *** GET<T> HOOK *** Captured real ClassDescriptions: %p", result);

        int32_t count = 0;
        if (safe_memory_read_i32((mach_vm_address_t)result + 0x7C, &count)) {
            log_message("[StaticData] ClassDescriptions: count@+0x7C=%d", count);
        }
    }

    return result;
}

/**
 * Hook for Get<eoc::ProgressionManager>
 */
static void* hook_GetProgression(void* headmaster) {
    if (headmaster && !g_immutable_data_headmaster) {
        g_immutable_data_headmaster = headmaster;
    }

    void* result = g_orig_GetProgression ? g_orig_GetProgression(headmaster) : NULL;

    if (result && !g_staticdata.real_managers[STATICDATA_PROGRESSION]) {
        g_staticdata.real_managers[STATICDATA_PROGRESSION] = result;
        log_message("[StaticData] *** GET<T> HOOK *** Captured real ProgressionManager: %p", result);

        int32_t count = 0;
        if (safe_memory_read_i32((mach_vm_address_t)result + 0x7C, &count)) {
            log_message("[StaticData] ProgressionManager: count@+0x7C=%d", count);
        }
    }

    return result;
}

/**
 * Hook for Get<eoc::ActionResourceTypes>
 */
static void* hook_GetActionResource(void* headmaster) {
    if (headmaster && !g_immutable_data_headmaster) {
        g_immutable_data_headmaster = headmaster;
    }

    void* result = g_orig_GetActionResource ? g_orig_GetActionResource(headmaster) : NULL;

    if (result && !g_staticdata.real_managers[STATICDATA_ACTION_RESOURCE]) {
        g_staticdata.real_managers[STATICDATA_ACTION_RESOURCE] = result;
        log_message("[StaticData] *** GET<T> HOOK *** Captured real ActionResourceTypes: %p", result);

        int32_t count = 0;
        if (safe_memory_read_i32((mach_vm_address_t)result + 0x7C, &count)) {
            log_message("[StaticData] ActionResourceTypes: count@+0x7C=%d", count);
        }
    }

    return result;
}

/**
 * Install hooks for Get<T> functions.
 * Uses standard Dobby hooks (Get<T> functions are small and typically safe).
 */
static void install_get_manager_hooks(void* main_binary_base) {
    (void)main_binary_base;  // address now sourced from offset_table
    const VersionOffsets *off = offset_table_get();
    if (!off) {
        log_message("[StaticData] Get<T> hooks SKIPPED (no offset table entry for this version)");
        return;
    }

    // CRASH GUARD: these Get<T> accessors are hooked with PLAIN DobbyHook, which
    // mangles their PC-relative (adrp/cbz) prologue on shifted versions — the
    // trampoline then returns a garbage manager and the character-progression
    // validator calls through a bad pointer (e.g. Barbarian level 2->3 subclass
    // pick -> SIGBUS, with zero SE frames in the stack). Only install on the
    // exact baseline version where DobbyHook is known-safe. On other versions the
    // TypeContext traversal (capture_managers_via_typecontext) already captures
    // these managers without hooking, so StaticData degrades gracefully.
    // (Proper future fix: give Get<T> the same ADRP-safe install FeatManager uses.)
    if (!version_detect_matches()) {
        log_message("[StaticData] Get<T> code hooks SKIPPED on this version "
                    "(plain DobbyHook is prologue-unsafe) — using TypeContext traversal only");
        return;
    }

    log_message("[StaticData] Installing Get<T> hooks for real manager capture...");
    void* target;

#define INSTALL_HOOK(field, fn_hook, fn_orig, label) \
    if (off->field) { \
        target = offset_table_fn(off->field); \
        if (DobbyHook(target, (void*)(fn_hook), (void**)(fn_orig)) == 0) \
            log_message("[StaticData] Installed " label " hook at %p", target); \
        else \
            log_message("[StaticData] WARNING: Failed to hook " label); \
    }

    INSTALL_HOOK(fn_get_background,    hook_GetBackground,    &g_orig_GetBackground,    "Get<BackgroundManager>")
    INSTALL_HOOK(fn_get_origin,        hook_GetOrigin,        &g_orig_GetOrigin,        "Get<OriginManager>")
    INSTALL_HOOK(fn_get_class,         hook_GetClass,         &g_orig_GetClass,         "Get<ClassDescriptions>")
    INSTALL_HOOK(fn_get_progression,   hook_GetProgression,   &g_orig_GetProgression,   "Get<ProgressionManager>")
    INSTALL_HOOK(fn_get_actionresource,hook_GetActionResource,&g_orig_GetActionResource,"Get<ActionResourceTypes>")

#undef INSTALL_HOOK

    log_message("[StaticData] Get<T> hooks installation complete");
}

/**
 * Force capture managers by calling Get<T> functions directly.
 * Uses captured ImmutableDataHeadmaster to trigger manager capture
 * for types where hooks are installed but haven't fired yet.
 * Returns number of managers newly captured.
 */
int staticdata_force_capture(void) {
    if (!g_immutable_data_headmaster) {
        log_message("[StaticData] Cannot force capture - no ImmutableDataHeadmaster captured yet");
        return 0;
    }

    int captured = 0;
    log_message("[StaticData] Force capturing managers via ImmutableDataHeadmaster %p",
                g_immutable_data_headmaster);

    // Background - call original if not captured
    if (!g_staticdata.real_managers[STATICDATA_BACKGROUND] && g_orig_GetBackground) {
        void* result = g_orig_GetBackground(g_immutable_data_headmaster);
        if (result) {
            g_staticdata.real_managers[STATICDATA_BACKGROUND] = result;
            int32_t count = 0;
            safe_memory_read_i32((mach_vm_address_t)result + 0x7C, &count);
            log_message("[StaticData] Force captured BackgroundManager: %p (count=%d)", result, count);
            captured++;
        }
    }

    // Origin - call original if not captured
    if (!g_staticdata.real_managers[STATICDATA_ORIGIN] && g_orig_GetOrigin) {
        void* result = g_orig_GetOrigin(g_immutable_data_headmaster);
        if (result) {
            g_staticdata.real_managers[STATICDATA_ORIGIN] = result;
            int32_t count = 0;
            safe_memory_read_i32((mach_vm_address_t)result + 0x7C, &count);
            log_message("[StaticData] Force captured OriginManager: %p (count=%d)", result, count);
            captured++;
        }
    }

    // Class - call original if not captured
    if (!g_staticdata.real_managers[STATICDATA_CLASS] && g_orig_GetClass) {
        void* result = g_orig_GetClass(g_immutable_data_headmaster);
        if (result) {
            g_staticdata.real_managers[STATICDATA_CLASS] = result;
            int32_t count = 0;
            safe_memory_read_i32((mach_vm_address_t)result + 0x7C, &count);
            log_message("[StaticData] Force captured ClassDescriptions: %p (count=%d)", result, count);
            captured++;
        }
    }

    // Progression - call original if not captured
    if (!g_staticdata.real_managers[STATICDATA_PROGRESSION] && g_orig_GetProgression) {
        void* result = g_orig_GetProgression(g_immutable_data_headmaster);
        if (result) {
            g_staticdata.real_managers[STATICDATA_PROGRESSION] = result;
            int32_t count = 0;
            safe_memory_read_i32((mach_vm_address_t)result + 0x7C, &count);
            log_message("[StaticData] Force captured ProgressionManager: %p (count=%d)", result, count);
            captured++;
        }
    }

    // ActionResource - call original if not captured
    if (!g_staticdata.real_managers[STATICDATA_ACTION_RESOURCE] && g_orig_GetActionResource) {
        void* result = g_orig_GetActionResource(g_immutable_data_headmaster);
        if (result) {
            g_staticdata.real_managers[STATICDATA_ACTION_RESOURCE] = result;
            int32_t count = 0;
            safe_memory_read_i32((mach_vm_address_t)result + 0x7C, &count);
            log_message("[StaticData] Force captured ActionResourceTypes: %p (count=%d)", result, count);
            captured++;
        }
    }

    log_message("[StaticData] Force capture complete: %d managers newly captured", captured);
    return captured;
}

/**
 * Look up a manager in ImmutableDataHeadmaster using type index from TypeContext.
 *
 * ImmutableDataHeadmaster hash table structure (from Get<T> decompilation):
 *   +0x00: buckets array (uint32_t*) - initial bucket indices
 *   +0x08: bucket_count (int32_t) - number of buckets
 *   +0x10: next array (uint32_t*) - chain for collision resolution
 *   +0x20: keys array (int32_t*) - type indices
 *   +0x2c: size (int32_t) - number of entries
 *   +0x30: values array (void**) - manager pointers
 *
 * TypeContext slot structure (what managers[] contains):
 *   +0x00: type_index (int32_t) - used for hash lookup
 *   +0x08: flags/padding
 *   +0x38: type name string pointer
 *
 * @param type_index The type index to look up
 * @return Manager pointer, or NULL if not found
 */
static void* lookup_manager_by_type_index(int32_t type_index) {
    if (!g_immutable_data_headmaster || type_index < 0) {
        return NULL;
    }

    void* headmaster = g_immutable_data_headmaster;

    // Read hash table structure
    void* buckets = NULL;
    int32_t bucket_count = 0;
    void* next_chain = NULL;
    void* keys = NULL;
    int32_t size = 0;
    void* values = NULL;

    if (!safe_memory_read_pointer((mach_vm_address_t)headmaster + 0x00, &buckets) ||
        !safe_memory_read_i32((mach_vm_address_t)headmaster + 0x08, &bucket_count) ||
        !safe_memory_read_pointer((mach_vm_address_t)headmaster + 0x10, &next_chain) ||
        !safe_memory_read_pointer((mach_vm_address_t)headmaster + 0x20, &keys) ||
        !safe_memory_read_i32((mach_vm_address_t)headmaster + 0x2c, &size) ||
        !safe_memory_read_pointer((mach_vm_address_t)headmaster + 0x30, &values)) {
        log_message("[StaticData] Hash lookup: failed to read headmaster structure");
        return NULL;
    }

    if (!buckets || !keys || !values ||
        bucket_count <= 0 || bucket_count > STATICDATA_MAX_HASH_SLOTS ||
        size < 0 || size > STATICDATA_MAX_HASH_SLOTS) {
        log_message("[StaticData] Hash lookup: invalid headmaster structure (buckets=%d, size=%d)",
                    bucket_count, size);
        return NULL;
    }

    // Compute bucket index: type_index % bucket_count
    int32_t bucket_idx = type_index % bucket_count;
    if (bucket_idx < 0) bucket_idx += bucket_count;  // Handle negative modulo

    // Read initial index from bucket
    uint32_t idx = 0;
    if (!safe_memory_read_u32((mach_vm_address_t)buckets + (mach_vm_address_t)bucket_idx * 4, &idx)) {
        return NULL;
    }

    // Walk the chain. Every index must address the Keys/Values arrays
    // (idx < size), and a chain can never be longer than the table, so a
    // stale or cyclic NextIds entry terminates instead of spinning.
    for (int32_t hops = 0; (int32_t)idx >= 0 && hops <= size; hops++) {
        if (idx >= (uint32_t)size) {
            log_message("[StaticData] Hash lookup: chain index %u exceeds table size %d", idx, size);
            break;
        }

        // Read key at this index
        int32_t key = 0;
        if (!safe_memory_read_i32((mach_vm_address_t)keys + (mach_vm_address_t)idx * 4, &key)) {
            break;
        }

        if (key == type_index) {
            // Found it - read value
            void* manager = NULL;
            if (safe_memory_read_pointer((mach_vm_address_t)values + (mach_vm_address_t)idx * 8, &manager)) {
                return manager;
            }
            break;
        }

        // Follow next chain
        if (!next_chain) break;
        if (!safe_memory_read_u32((mach_vm_address_t)next_chain + (mach_vm_address_t)idx * 4, &idx)) {
            break;
        }
    }

    return NULL;
}

/**
 * Resolve one type's GuidResourceBank through the ImmutableDataHeadmaster
 * hash table, keyed by the m_TypeIndex the TypeContext slot points at.
 *
 * @return true if the bank is (now) captured
 */
static bool capture_bank_via_hash_lookup(StaticDataType type) {
    if (type < 0 || type >= STATICDATA_COUNT) return false;
    if (g_staticdata.real_managers[type]) return true;
    if (!g_immutable_data_headmaster) return false;

    void* slot_ptr = g_staticdata.managers[type];
    if (!slot_ptr) return false;

    int32_t type_index = 0;
    if (!safe_memory_read_i32((mach_vm_address_t)slot_ptr, &type_index)) {
        log_message("[StaticData] Hash lookup: cannot read type_index for %s at %p",
                    s_type_names[type], slot_ptr);
        return false;
    }

    void* bank = lookup_manager_by_type_index(type_index);
    if (!bank) {
        log_message("[StaticData] Hash lookup: %s (type_index=%d) not in the headmaster table",
                    s_type_names[type], type_index);
        return false;
    }

    g_staticdata.real_managers[type] = bank;
    int32_t count = 0;
    safe_memory_read_i32((mach_vm_address_t)bank + FEATMANAGER_REAL_COUNT_OFFSET, &count);
    log_message("[StaticData] Hash lookup captured %s: %p (type_index=%d, count=%d, stride=0x%x)",
                s_type_names[type], bank, type_index, count, effective_entry_size(type, bank));
    return true;
}

/**
 * Re-resolve every cached bank through the headmaster table and replace any
 * whose address moved. Banks live in the headmaster for the process lifetime
 * on every build probed so far, but a session reload onto a different mod
 * list is exactly the case where the engine could rebuild one; a cached
 * pointer into freed-but-mapped memory would read plausible garbage rather
 * than fail. Runs at every SessionLoaded from staticdata_post_init_capture.
 *
 * @return Number of banks whose address changed
 */
static int revalidate_cached_banks(void) {
    if (!g_immutable_data_headmaster) return 0;

    int moved = 0;
    for (int i = 0; i < STATICDATA_COUNT; i++) {
        StaticDataType type = (StaticDataType)i;
        void* cached = g_staticdata.real_managers[type];
        void* slot_ptr = g_staticdata.managers[type];
        if (!cached || !slot_ptr) continue;

        int32_t type_index = 0;
        if (!safe_memory_read_i32((mach_vm_address_t)slot_ptr, &type_index)) continue;
        void* current = lookup_manager_by_type_index(type_index);
        if (!current) {
            log_message("[StaticData] Revalidate: %s no longer in the headmaster table; keeping %p",
                        s_type_names[type], cached);
            continue;
        }
        if (current != cached) {
            log_message("[StaticData] Revalidate: %s bank moved %p -> %p; stride will be re-measured",
                        s_type_names[type], cached, current);
            g_staticdata.real_managers[type] = current;
            g_staticdata.entry_stride[type] = 0;
            moved++;
        }
    }
    return moved;
}

/**
 * Resolve every bank that is not captured yet. Runs at post-init and on demand;
 * needs the ImmutableDataHeadmaster (captured by any Get<T> hook) and the
 * TypeContext slots.
 *
 * @return Number of banks newly captured
 */
int staticdata_hash_lookup_capture(void) {
    if (!g_immutable_data_headmaster) {
        log_message("[StaticData] Hash lookup: no ImmutableDataHeadmaster captured yet");
        return 0;
    }

    bool need_slots = false;
    for (int i = 0; i < STATICDATA_COUNT; i++) {
        if (!g_staticdata.real_managers[i] && !g_staticdata.managers[i]) need_slots = true;
    }
    if (need_slots && g_staticdata.initialized) {
        capture_managers_via_typecontext();
    }

    int captured = 0;
    for (int i = 0; i < STATICDATA_COUNT; i++) {
        StaticDataType type = (StaticDataType)i;
        if (g_staticdata.real_managers[type]) continue;
        if (capture_bank_via_hash_lookup(type)) captured++;
    }

    log_message("[StaticData] Hash lookup complete: %d banks newly captured", captured);
    return captured;
}

// ============================================================================
// ARM64 Safe Hook Installation (Issue #44)
// ============================================================================

/**
 * Install ARM64-safe hook for FeatManager::GetFeats.
 * Uses skip-and-redirect strategy to avoid ADRP corruption.
 * Returns true if hook was installed successfully.
 */
static bool install_feat_getfeats_safe_hook(void* main_binary_base) {
    const VersionOffsets *off = offset_table_get();
    if (!off || !off->fn_feat_getfeats) {
        log_message("[StaticData] FeatGetFeats offset not available for this version");
        return false;
    }

    // CRASH GUARD: this hook fires during character level-up (it captures the
    // session FeatManager). Even the ARM64-safe hook's trampoline is not proven
    // correct on shifted prologues — a corrupted accessor crashes the level-up
    // validator (ApplyAndValidateLevelUp -> indirect call to garbage; zero SE
    // frames). Only install on the exact baseline version where it's verified.
    // On other versions StaticData captures FeatManager via the TypeContext
    // traversal instead (no hooking). Restores the pre-offset-table-fill behavior.
    if (!version_detect_matches()) {
        log_message("[StaticData] FeatManager::GetFeats hook SKIPPED on this version "
                    "(hook trampoline unverified) — using TypeContext traversal only");
        return false;
    }

    void* target = offset_table_fn(off->fn_feat_getfeats);

    // First, analyze the prologue to understand the ADRP patterns
    log_message("[StaticData] Analyzing FeatManager::GetFeats prologue at %p", target);
    arm64_analyze_and_log(target, "FeatManager::GetFeats");

    // Check if it has ADRP in prologue
    if (arm64_has_prologue_adrp(target)) {
        log_message("[StaticData] ADRP detected in prologue - using ARM64 safe hook");

        // Get recommended hook offset
        int safe_offset = arm64_get_recommended_hook_offset(target);
        if (safe_offset < 0) {
            log_message("[StaticData] WARNING: No safe hook point found, falling back to TypeContext");
            return false;
        }

        log_message("[StaticData] Safe hook point at +%d (0x%x)", safe_offset, safe_offset);

        // Install the safe hook
        void* original = NULL;
        g_staticdata.feat_getfeats_hook = arm64_safe_hook(target, (void*)hook_FeatGetFeats, &original);

        if (g_staticdata.feat_getfeats_hook) {
            g_orig_FeatGetFeats = (FeatGetFeats_t)original;
            log_message("[StaticData] ARM64 safe hook installed successfully!");
            log_message("[StaticData]   Original function trampoline: %p", original);
            return true;
        } else {
            log_message("[StaticData] WARNING: ARM64 safe hook installation failed");
            return false;
        }
    } else {
        // No ADRP in prologue - safe to use standard Dobby hook!
        log_message("[StaticData] No ADRP in prologue - installing standard Dobby hook");

        void* original = NULL;
        int result = DobbyHook(target, (void*)hook_FeatGetFeats, (void**)&original);

        if (result == 0 && original) {
            g_orig_FeatGetFeats = (FeatGetFeats_t)original;
            g_staticdata.feat_getfeats_hook = target;
            log_message("[StaticData] Dobby hook installed successfully!");
            log_message("[StaticData]   Original function trampoline: %p", original);
            return true;
        } else {
            log_message("[StaticData] WARNING: Dobby hook installation failed (result=%d)", result);
            return false;
        }
    }
}

// ============================================================================
// Initialization
// ============================================================================

bool staticdata_manager_init(void *main_binary_base) {
    if (g_staticdata.initialized) {
        return true;
    }

    g_staticdata.main_binary_base = main_binary_base;

    // Clear manager pointers
    memset(g_staticdata.managers, 0, sizeof(g_staticdata.managers));
    memset(g_staticdata.real_managers, 0, sizeof(g_staticdata.real_managers));

    // Manager capture installs inline Dobby CODE hooks at function addresses.
    // Addresses are sourced from the offset table (keyed by game version).
    // A missing table entry means 0 offsets → hooks are individually skipped.
    // This replaced the old exact-version-match gate (Issue #78) with a
    // per-version table so new BG3 versions can be supported without source changes.
    const VersionOffsets *off = offset_table_get();
    if (!off || (!off->fn_feat_getfeats && !off->fn_get_background)) {
        log_message("[StaticData] Manager hooks SKIPPED (no function offsets for version %s — "
                    "add to offset_table.c to enable; Ext.StaticData in degraded mode)",
                    version_detect_get_version() ? version_detect_get_version() : "unknown");
        g_staticdata.initialized = true;
        return true;
    }

    // Try to install ARM64-safe hook for FeatManager::GetFeats
    // This uses the skip-and-redirect strategy from Issue #44
    bool hook_installed = install_feat_getfeats_safe_hook(main_binary_base);

    if (hook_installed) {
        log_message("[StaticData] FeatManager hook: ARM64 safe hook active");
    } else {
        log_message("[StaticData] FeatManager hook: Using TypeContext capture (hook not installed)");
    }

    // Install Get<T> hooks for other manager types (Dec 22, 2025)
    // These capture real manager pointers from ImmutableDataHeadmaster
    install_get_manager_hooks(main_binary_base);

    g_staticdata.initialized = true;
    log_message("[StaticData] Static data manager initialized");

    return true;
}

bool staticdata_manager_ready(void) {
    if (!g_staticdata.initialized) {
        return false;
    }

    // Ready once at least one bank resolves to real entries
    for (int i = 0; i < STATICDATA_COUNT; i++) {
        if (g_staticdata.real_managers[i]) {
            return true;
        }
    }

    return false;
}

const char* staticdata_type_name(StaticDataType type) {
    if (type < 0 || type >= STATICDATA_COUNT) {
        return NULL;
    }
    return s_type_names[type];
}

int staticdata_type_from_name(const char* name) {
    if (!name) return -1;

    for (int i = 0; i < STATICDATA_COUNT; i++) {
        if (strcasecmp(s_type_names[i], name) == 0) {
            return i;
        }
    }

    return -1;
}

// ============================================================================
// Manager Access
// ============================================================================

bool staticdata_has_manager(StaticDataType type) {
    if (type < 0 || type >= STATICDATA_COUNT) {
        return false;
    }

    // Lazy: resolve the TypeContext slot, then the bank behind it
    if (!g_staticdata.managers[type] && g_staticdata.initialized) {
        capture_managers_via_typecontext();
    }
    if (!g_staticdata.real_managers[type]) {
        capture_bank_via_hash_lookup(type);
    }

    return g_staticdata.real_managers[type] != NULL;
}

void* staticdata_get_manager(StaticDataType type) {
    if (type < 0 || type >= STATICDATA_COUNT) {
        return NULL;
    }
    return g_staticdata.real_managers[type];
}

bool staticdata_capture_manager(StaticDataType type) {
    // Triggers capture for a specific type via TypeContext traversal
    if (type < 0 || type >= STATICDATA_COUNT) return false;

    // Try TypeContext capture if not already captured
    if (!g_staticdata.managers[type] && g_staticdata.initialized) {
        capture_managers_via_typecontext();
    }

    return staticdata_has_manager(type);
}

/**
 * Post-initialization capture attempt.
 * Call this after game data is loaded (e.g., after SessionLoaded event).
 * Attempts to capture all manager types via TypeContext traversal + probing.
 *
 * @return Number of managers successfully captured
 */
int staticdata_post_init_capture(void) {
    if (!g_staticdata.initialized) {
        log_message("[StaticData] Post-init capture skipped - not initialized");
        return 0;
    }

    log_message("[StaticData] Post-init capture starting...");
    int captured = 0;

    // 1. Resolve the TypeContext slots (m_TypeIndex globals) for every type
    int tc_captured = capture_managers_via_typecontext();
    log_message("[StaticData] TypeContext resolved %d slots", tc_captured);

    // 1b. Banks cached by an earlier session: confirm they still resolve to
    //     the same address, replace any that moved
    int moved = revalidate_cached_banks();
    if (moved > 0) {
        log_message("[StaticData] Revalidate: %d cached banks replaced", moved);
    }

    // 2. Resolve the banks behind those slots through the headmaster hash table
    int hl_captured = staticdata_hash_lookup_capture();
    log_message("[StaticData] Hash lookup resolved %d banks", hl_captured);

    // 3. Load any existing Frida captures as fallback
    for (int i = 0; i < STATICDATA_COUNT; i++) {
        if (!g_staticdata.real_managers[i]) {
            if (staticdata_frida_capture_available_type((StaticDataType)i)) {
                if (staticdata_load_frida_capture_type((StaticDataType)i)) {
                    log_message("[StaticData] Loaded Frida capture for %s", s_type_names[i]);
                }
            }
        }
    }

    // Count banks with real entries
    for (int i = 0; i < STATICDATA_COUNT; i++) {
        if (g_staticdata.real_managers[i]) {
            captured++;
        }
    }

    log_message("[StaticData] Post-init capture complete: %d/%d managers ready", captured, STATICDATA_COUNT);
    staticdata_dump_status();

    return captured;
}

// ============================================================================
// Data Access
// ============================================================================
//
// Only a real GuidResourceBank<T> is dereferenced for entries:
//   +0x7C  Resources.Keys.size   live entry count
//   +0x80  Resources.Values.buf  flat T[] with stride sizeof(T)
// (GuidResourceBankBase is 0x50 bytes: vtable, two FixedStrings, and the
// ResourceGuidsByMod HashMap; HashMap<Guid, T> Resources follows at +0x50 with
// Keys at +0x70 and Values at +0x80.)

static bool read_bank_array(void* bank, int32_t* out_count, void** out_array) {
    if (!bank) return false;
    if (!safe_memory_read_i32((mach_vm_address_t)bank + FEATMANAGER_REAL_COUNT_OFFSET, out_count) ||
        !safe_memory_read_pointer((mach_vm_address_t)bank + FEATMANAGER_REAL_ARRAY_OFFSET, out_array)) {
        return false;
    }
    if (*out_count < 0 || *out_count > STATICDATA_MAX_BANK_ENTRIES) {
        return false;  // garbage header, not a bank
    }
    // An initialized but empty bank has no Values buffer yet; that is a valid
    // count of 0, distinct from "bank unavailable".
    return *out_count == 0 || *out_array != NULL;
}

/**
 * Measure sizeof(T) from the live Values array. Every entry begins with the
 * same vtable pointer (GuidResource is polymorphic), so the distance to the
 * next occurrence of entry 0's first word is the stride.
 *
 * @return stride in bytes, or 0 when the bank has < 2 entries or no repeat
 *         appears within 4 KiB
 */
static int detect_entry_stride(void* bank) {
    int32_t count = 0;
    void* array = NULL;
    if (!read_bank_array(bank, &count, &array) || count < 2) return 0;

    uint64_t vmt = 0;
    if (!safe_memory_read_u64((mach_vm_address_t)array, &vmt) || vmt == 0) return 0;

    // A field inside entry 0 could coincidentally hold the vtable word, so a
    // candidate must also land on entry 2 when the bank has one: the true
    // stride S repeats at 2S, while a false hit at k < S reads a field of
    // entry 1 at 2k, which is not the vtable. Nothing smaller than a bare
    // GuidResource (vtable + ResourceUUID) can be a stride.
    for (int off = STATICDATA_MIN_ENTRY_STRIDE; off <= 0x1000; off += 8) {
        uint64_t word = 0;
        if (!safe_memory_read_u64((mach_vm_address_t)array + off, &word)) return 0;
        if (word != vmt) continue;
        if (count >= 3) {
            uint64_t third = 0;
            if (!safe_memory_read_u64((mach_vm_address_t)array + 2 * off, &third) || third != vmt) {
                continue;
            }
        }
        return off;
    }
    return 0;
}

/**
 * Stride used for index arithmetic: the live measurement when the bank holds
 * at least two entries, otherwise the configured constant. Logged once per
 * type when the two disagree.
 */
static int effective_entry_size(StaticDataType type, void* bank) {
    if (type < 0 || type >= STATICDATA_COUNT) return 0;
    if (g_staticdata.entry_stride[type] == 0) {
        const ManagerConfig* config = &g_manager_configs[type];
        int detected = detect_entry_stride(bank);
        if (detected > 0) {
            if (detected != config->entry_size) {
                log_message("[StaticData] %s stride is 0x%x on this build (configured 0x%x); using the live value",
                            s_type_names[type], detected, config->entry_size);
            }
            g_staticdata.entry_stride[type] = detected;
        } else {
            g_staticdata.entry_stride[type] = config->entry_size;
        }
    }
    return g_staticdata.entry_stride[type];
}

/**
 * The bank for a type, resolving it lazily through the headmaster hash table.
 */
static void* get_real_manager(StaticDataType type) {
    if (type < 0 || type >= STATICDATA_COUNT) return NULL;
    if (!g_staticdata.real_managers[type]) {
        capture_bank_via_hash_lookup(type);
    }
    return g_staticdata.real_managers[type];
}

static void* entry_at(StaticDataType type, void* bank, void* array, int32_t count, int index) {
    if (index < 0 || index >= count) return NULL;
    int stride = effective_entry_size(type, bank);
    if (stride <= 0) return NULL;

    void* entry = (uint8_t*)array + ((size_t)index * (size_t)stride);
    int32_t test_read = 0;
    if (!safe_memory_read_i32((mach_vm_address_t)entry, &test_read)) {
        return NULL;
    }
    return entry;
}

int staticdata_get_count(StaticDataType type) {
    void* bank = get_real_manager(type);
    int32_t count = 0;
    void* array = NULL;
    if (!read_bank_array(bank, &count, &array)) return -1;
    return count;
}

StaticDataPtr staticdata_get_by_index(StaticDataType type, int index) {
    void* bank = get_real_manager(type);
    int32_t count = 0;
    void* array = NULL;
    if (!read_bank_array(bank, &count, &array)) return NULL;
    return entry_at(type, bank, array, count, index);
}

StaticDataPtr staticdata_get_by_guid(StaticDataType type, const StaticDataGuid* guid) {
    if (!guid) return NULL;
    void* bank = get_real_manager(type);
    int32_t count = 0;
    void* array = NULL;
    if (!read_bank_array(bank, &count, &array)) return NULL;

    // Linear scan comparing the ResourceUUID at +0x08 (after the vtable)
    for (int i = 0; i < count; i++) {
        void* entry = entry_at(type, bank, array, count, i);
        if (!entry) continue;

        uint8_t entry_guid[16];
        if (!safe_memory_read((mach_vm_address_t)entry + 0x08, entry_guid, sizeof(entry_guid))) {
            continue;
        }
        if (memcmp(entry_guid, guid, sizeof(StaticDataGuid)) == 0) {
            return entry;
        }
    }
    return NULL;
}

// ============================================================================
// GUID Parsing
// ============================================================================

/**
 * Canonical GUID text -> in-memory ls::Guid bytes.
 *
 * ls::Guid stores the last two textual groups with adjacent byte pairs
 * swapped (guid_lookup.c). A memory-order sscanf produced ResourceUUIDs whose
 * D and E groups were pair-swapped, so canonical GUIDs from Races.lsx never
 * matched Ext.StaticData.Get(). Both directions now go through the entity
 * system's guid_parse()/guid_to_string(), which Phase 5 verified against the
 * host character on 7398727.
 */
static bool parse_guid(const char* str, StaticDataGuid* out) {
    if (!str || !out) return false;

    Guid guid;
    if (!guid_parse(str, &guid)) {
        return false;
    }
    memcpy(out, &guid, sizeof(*out));
    return true;
}

/**
 * In-memory ls::Guid bytes -> canonical GUID text (37-byte buffer).
 */
static void format_guid(const StaticDataGuid* guid, char* out_buf) {
    Guid engine_guid;
    memcpy(&engine_guid, guid, sizeof(engine_guid));
    guid_to_string(&engine_guid, out_buf);
}

StaticDataPtr staticdata_get_by_guid_string(StaticDataType type, const char* guid_str) {
    StaticDataGuid guid;
    if (!parse_guid(guid_str, &guid)) {
        return NULL;
    }
    return staticdata_get_by_guid(type, &guid);
}

// ============================================================================
// Entry Property Access
// ============================================================================

bool staticdata_get_guid(StaticDataType type, StaticDataPtr entry, StaticDataGuid* out_guid) {
    if (!entry || !out_guid) return false;

    // GUID is at +0x08 in the Feat structure (after 8-byte VMT header)
    // Different static data types may have different layouts
    int guid_offset = 0x08;  // Default: after VMT
    switch (type) {
        case STATICDATA_FEAT:
            guid_offset = 0x08;  // Verified via Ghidra
            break;
        default:
            guid_offset = 0x08;  // TODO: Verify for other types
            break;
    }

    // Use safe memory read to prevent crashes
    uint8_t guid_bytes[16];
    mach_vm_address_t guid_addr = (mach_vm_address_t)entry + guid_offset;

    // Read GUID bytes safely (16 bytes = sizeof(StaticDataGuid))
    for (int i = 0; i < 16; i++) {
        uint8_t byte = 0;
        if (!safe_memory_read_u8(guid_addr + i, &byte)) {
            log_message("[StaticData] Cannot read GUID byte %d at %p", i, (void*)(guid_addr + i));
            return false;
        }
        guid_bytes[i] = byte;
    }

    memcpy(out_guid, guid_bytes, sizeof(StaticDataGuid));
    return true;
}

bool staticdata_get_guid_string(StaticDataType type, StaticDataPtr entry, char* out_buf, size_t buf_size) {
    if (!entry || !out_buf || buf_size < 37) return false;

    StaticDataGuid guid;
    if (!staticdata_get_guid(type, entry, &guid)) {
        return false;
    }

    format_guid(&guid, out_buf);
    return true;
}

const char* staticdata_get_name(StaticDataType type, StaticDataPtr entry) {
    if (!entry) return NULL;

    // Name offset depends on type - different layouts per GuidResource subclass
    int name_offset = 0;

    switch (type) {
        case STATICDATA_FEAT:
            name_offset = FEAT_OFFSET_NAME;  // 0x18
            break;
        case STATICDATA_RACE:
            name_offset = RACE_OFFSET_NAME;  // 0x18
            break;
        case STATICDATA_ORIGIN:
            name_offset = ORIGIN_OFFSET_NAME;  // 0x1C (after uint8_t)
            break;
        case STATICDATA_BACKGROUND:
            // Background has no FixedString Name - only TranslatedString DisplayName
            return NULL;
        case STATICDATA_CC_APPEARANCE_VISUAL:
            // RootTemplate GUID sits at +0x18; the type has no FixedString Name
            return NULL;
        case STATICDATA_GOD:
            name_offset = GOD_OFFSET_NAME;  // 0x18
            break;
        case STATICDATA_CLASS:
            name_offset = CLASS_OFFSET_NAME;  // 0x28 (after ParentGuid)
            break;
        default:
            name_offset = 0x18;  // Default assumption
            break;
    }

    if (name_offset == 0) {
        return NULL;  // Type has no Name field
    }

    // Read FixedString index safely
    uint32_t fs_index = 0;
    if (!safe_memory_read_u32((mach_vm_address_t)entry + name_offset, &fs_index)) {
        log_message("[StaticData] Cannot read name FixedString at %p+0x%x", entry, name_offset);
        return NULL;
    }

    // Check for null/invalid index
    if (fs_index == 0 || fs_index == 0xFFFFFFFF) {
        return NULL;
    }

    // Resolve via GlobalStringTable
    const char* name = fixed_string_resolve(fs_index);
    if (!name) {
        // Log once per session for debugging
        static int logged_failures = 0;
        if (logged_failures < 5) {
            log_message("[StaticData] Failed to resolve FixedString 0x%08X for %s entry at %p",
                        fs_index, staticdata_type_name(type), entry);
            logged_failures++;
        }
    }

    return name;
}

const char* staticdata_get_display_name(StaticDataType type, StaticDataPtr entry) {
    // TODO: Implement localized name lookup
    (void)type;
    (void)entry;
    return NULL;
}

// ============================================================================
// Typed Field Access
// ============================================================================

/**
 * Read a FixedString index at `addr` and resolve it. Returns false only when
 * the index itself is unreadable; an unset index yields *out == NULL.
 */
static bool read_fixed_string_at(mach_vm_address_t addr, const char** out) {
    uint32_t fs_index = 0;
    if (!safe_memory_read_u32(addr, &fs_index)) {
        return false;
    }
    *out = (fs_index == 0 || fs_index == 0xFFFFFFFF) ? NULL : fixed_string_resolve(fs_index);
    return true;
}

static bool read_guid_at(mach_vm_address_t addr, char* out_buf) {
    Guid guid;
    if (!safe_memory_read(addr, &guid, sizeof(guid))) {
        return false;
    }
    guid_to_string(&guid, out_buf);
    return true;
}

/**
 * Read and validate an Array<Guid> header { Guid* buf; uint32_t capacity;
 * uint32_t size; }. A header whose size exceeds its capacity, exceeds
 * STATICDATA_MAX_GUID_ARRAY, or has entries but no buffer is garbage and is
 * reported as unreadable so the caller skips the field instead of iterating it.
 */
static bool read_guid_array_header(mach_vm_address_t addr, void** out_buf, uint32_t* out_size) {
    void* buf = NULL;
    uint32_t capacity = 0, size = 0;
    if (!safe_memory_read_pointer(addr, &buf) ||
        !safe_memory_read_u32(addr + 0x08, &capacity) ||
        !safe_memory_read_u32(addr + 0x0C, &size)) {
        return false;
    }
    if (size > capacity || size > STATICDATA_MAX_GUID_ARRAY || (size > 0 && !buf)) {
        return false;
    }
    *out_buf = buf;
    *out_size = size;
    return true;
}

bool staticdata_read_field(StaticDataPtr entry, const StaticDataField* field,
                           StaticDataFieldValue* out) {
    if (!entry || !field || !out) return false;

    memset(out, 0, sizeof(*out));
    out->kind = (int)field->kind;
    mach_vm_address_t addr = (mach_vm_address_t)entry + field->offset;

    switch (field->kind) {
        case SD_FIELD_GUID:
            return read_guid_at(addr, out->guid);
        case SD_FIELD_U8:
            return safe_memory_read_u8(addr, &out->u8);
        case SD_FIELD_BOOL: {
            uint8_t raw = 0;
            if (!safe_memory_read_u8(addr, &raw)) return false;
            out->boolean = raw != 0;
            return true;
        }
        case SD_FIELD_U32:
            return safe_memory_read_u32(addr, &out->u32);
        case SD_FIELD_FIXEDSTRING:
            return read_fixed_string_at(addr, &out->str);
        case SD_FIELD_TRANSLATEDSTRING: {
            // TranslatedString = { RuntimeStringHandle Handle; RuntimeStringHandle ArgumentString; }
            // RuntimeStringHandle = { FixedString Handle; uint16_t Version; } (8 bytes with padding)
            if (!read_fixed_string_at(addr, &out->translated.handle) ||
                !safe_memory_read(addr + 0x04, &out->translated.version, sizeof(uint16_t)) ||
                !read_fixed_string_at(addr + 0x08, &out->translated.argument) ||
                !safe_memory_read(addr + 0x0C, &out->translated.argument_version, sizeof(uint16_t))) {
                return false;
            }
            return true;
        }
        case SD_FIELD_GUID_ARRAY: {
            void* buf = NULL;
            return read_guid_array_header(addr, &buf, &out->guid_array.size);
        }
    }
    return false;
}

bool staticdata_read_guid_array_at(StaticDataPtr entry, const StaticDataField* field,
                                   uint32_t index, char* out_buf, size_t buf_size) {
    if (!entry || !field || !out_buf || buf_size < 37 ||
        field->kind != SD_FIELD_GUID_ARRAY) {
        return false;
    }

    mach_vm_address_t addr = (mach_vm_address_t)entry + field->offset;
    void* buf = NULL;
    uint32_t size = 0;
    if (!read_guid_array_header(addr, &buf, &size) || index >= size) {
        return false;
    }
    return read_guid_at((mach_vm_address_t)buf + (mach_vm_address_t)index * sizeof(Guid), out_buf);
}

// ============================================================================
// File-Based Frida Capture Integration
// ============================================================================

/**
 * Generic capture loader for any manager type.
 * File format:
 *   Line 1: Manager pointer (hex)
 *   Line 2: Count
 *   Line 3: Array pointer (hex)
 */
static bool load_captured_manager(StaticDataType type) {
    if (type < 0 || type >= STATICDATA_COUNT) return false;

    const ManagerConfig* config = &g_manager_configs[type];
    const char* capture_file = config->capture_file;

    FILE* f = fopen(capture_file, "r");
    if (!f) {
        return false;
    }

    char line1[64], line2[64], line3[64];
    if (!fgets(line1, sizeof(line1), f) ||
        !fgets(line2, sizeof(line2), f) ||
        !fgets(line3, sizeof(line3), f)) {
        fclose(f);
        return false;
    }
    fclose(f);

    // Parse pointer addresses
    void* mgr = NULL;
    int count = 0;
    void* array = NULL;

    if (sscanf(line1, "%p", &mgr) != 1 && sscanf(line1, "0x%lx", (unsigned long*)&mgr) != 1) {
        log_message("[StaticData] Failed to parse %s pointer from capture file", s_type_names[type]);
        return false;
    }
    count = atoi(line2);
    if (sscanf(line3, "%p", &array) != 1 && sscanf(line3, "0x%lx", (unsigned long*)&array) != 1) {
        log_message("[StaticData] Failed to parse %s array pointer from capture file", s_type_names[type]);
        return false;
    }

    // Validate the captured data
    if (!mgr || count <= 0 || count > 10000 || !array) {
        log_message("[StaticData] Invalid %s captured data: mgr=%p count=%d array=%p",
                    s_type_names[type], mgr, count, array);
        return false;
    }

    // Verify the pointers are still valid using type-specific offsets
    int32_t verify_count = 0;
    void* verify_array = NULL;
    if (!safe_memory_read_i32((mach_vm_address_t)mgr + config->count_offset, &verify_count) ||
        !safe_memory_read_pointer((mach_vm_address_t)mgr + config->array_offset, &verify_array)) {
        log_message("[StaticData] Captured %s pointer no longer valid (game restarted?)", s_type_names[type]);
        return false;
    }

    if (verify_count != count || verify_array != array) {
        log_message("[StaticData] Captured %s data mismatch (count=%d vs %d, array=%p vs %p)",
                    s_type_names[type], verify_count, count, verify_array, array);
        // Use the verified values instead
        count = verify_count;
        array = verify_array;
    }

    // Store as real manager
    g_staticdata.real_managers[type] = mgr;
    log_message("[StaticData] Loaded REAL %s from capture file: %p (count=%d, array=%p)",
                s_type_names[type], mgr, count, array);

    return true;
}

// Legacy wrapper for FeatManager
static bool load_captured_featmanager(void) {
    return load_captured_manager(STATICDATA_FEAT);
}

/**
 * Lua-callable function to load captured managers from Frida.
 * Call this after running the Frida capture script.
 */
bool staticdata_load_frida_capture(void) {
    const char* capture_file = g_manager_configs[STATICDATA_FEAT].capture_file;
    log_message("[StaticData] Attempting to load Frida capture from %s", capture_file);
    return load_captured_featmanager();
}

/**
 * Load captured manager pointers for a specific type.
 */
bool staticdata_load_frida_capture_type(StaticDataType type) {
    if (type < 0 || type >= STATICDATA_COUNT) {
        log_message("[StaticData] Invalid type %d for LoadFridaCapture", type);
        return false;
    }
    const char* capture_file = g_manager_configs[type].capture_file;
    log_message("[StaticData] Attempting to load %s capture from %s",
                s_type_names[type], capture_file);
    return load_captured_manager(type);
}

/**
 * Check if Frida capture is available (file exists and is recent).
 */
bool staticdata_frida_capture_available(void) {
    const char* capture_file = g_manager_configs[STATICDATA_FEAT].capture_file;
    FILE* f = fopen(capture_file, "r");
    if (f) {
        fclose(f);
        return true;
    }
    return false;
}

/**
 * Check if Frida capture is available for a specific type.
 */
bool staticdata_frida_capture_available_type(StaticDataType type) {
    if (type < 0 || type >= STATICDATA_COUNT) {
        return false;
    }
    const char* capture_file = g_manager_configs[type].capture_file;
    FILE* f = fopen(capture_file, "r");
    if (f) {
        fclose(f);
        return true;
    }
    return false;
}

// ============================================================================
// Debugging
// ============================================================================

void staticdata_try_typecontext_capture(void) {
    log_message("[StaticData] Attempting TypeContext capture...");
    int captured = capture_managers_via_typecontext();
    log_message("[StaticData] Captured %d managers via TypeContext", captured);

    // Also try to load Frida capture if available
    if (staticdata_frida_capture_available()) {
        load_captured_featmanager();
    }
}

void staticdata_dump_status(void) {
    log_message("[StaticData] Manager Status:");
    log_message("  Initialized: %s", g_staticdata.initialized ? "yes" : "no");
    log_message("  Base: %p", g_staticdata.main_binary_base);

    for (int i = 0; i < STATICDATA_COUNT; i++) {
        void* meta = g_staticdata.managers[i];
        void* real = g_staticdata.real_managers[i];

        if (real) {
            int32_t count = 0;
            void* array = NULL;
            safe_memory_read_i32((mach_vm_address_t)real + FEATMANAGER_REAL_COUNT_OFFSET, &count);
            safe_memory_read_pointer((mach_vm_address_t)real + FEATMANAGER_REAL_ARRAY_OFFSET, &array);
            log_message("  %s: BANK %p (count=%d, array=%p, stride=0x%x) [slot=%p]",
                        s_type_names[i], real, count, array,
                        effective_entry_size((StaticDataType)i, real), meta);
        } else if (meta) {
            int32_t type_index = -1;
            safe_memory_read_i32((mach_vm_address_t)meta, &type_index);
            log_message("  %s: slot %p (type_index=%d), bank not resolved",
                        s_type_names[i], meta, type_index);
        } else {
            log_message("  %s: not captured", s_type_names[i]);
        }
    }
}

bool staticdata_get_raw_info(StaticDataType type, StaticDataRawInfo* out) {
    if (!out || type < 0 || type >= STATICDATA_COUNT) {
        return false;
    }

    memset(out, 0, sizeof(*out));

    void* real = get_real_manager(type);
    if (!real) {
        return false;
    }

    out->manager_ptr = (uintptr_t)real;
    out->is_session = true;
    safe_memory_read_i32((mach_vm_address_t)real + FEATMANAGER_REAL_COUNT_OFFSET, &out->count);
    safe_memory_read_pointer((mach_vm_address_t)real + FEATMANAGER_REAL_ARRAY_OFFSET, (void**)&out->array_ptr);
    out->count_offset = FEATMANAGER_REAL_COUNT_OFFSET;
    out->array_offset = FEATMANAGER_REAL_ARRAY_OFFSET;
    out->entry_stride = effective_entry_size(type, real);
    return true;
}

void staticdata_dump_entries(StaticDataType type, int max_entries) {
    int count = staticdata_get_count(type);
    if (count < 0) {
        log_message("[StaticData] Type %s not available", staticdata_type_name(type));
        return;
    }

    int to_dump = (max_entries < 0 || max_entries > count) ? count : max_entries;
    log_message("[StaticData] Dumping %d of %d %s entries:", to_dump, count, staticdata_type_name(type));

    for (int i = 0; i < to_dump; i++) {
        void* entry = staticdata_get_by_index(type, i);
        if (!entry) continue;

        char guid_str[40];
        if (staticdata_get_guid_string(type, entry, guid_str, sizeof(guid_str))) {
            log_message("  [%d] GUID=%s", i, guid_str);
        } else {
            log_message("  [%d] ptr=%p", i, entry);
        }
    }
}

/**
 * Diagnostic: Dump the first few entries of the feat array to understand memory layout.
 */
void staticdata_dump_feat_memory(void) {
    void* mgr = g_staticdata.real_managers[STATICDATA_FEAT];
    if (!mgr) {
        log_message("[StaticData] No real FeatManager captured for memory dump");
        return;
    }

    int32_t count = 0;
    void* array = NULL;

    if (!safe_memory_read_i32((mach_vm_address_t)mgr + FEATMANAGER_REAL_COUNT_OFFSET, &count)) {
        log_message("[StaticData] Cannot read count for memory dump");
        return;
    }
    if (!safe_memory_read_pointer((mach_vm_address_t)mgr + FEATMANAGER_REAL_ARRAY_OFFSET, &array)) {
        log_message("[StaticData] Cannot read array pointer for memory dump");
        return;
    }

    log_message("[StaticData] MEMORY DUMP: mgr=%p, count=%d, array=%p", mgr, count, array);
    log_message("[StaticData] FEAT_SIZE=%d (0x%X), expected array range: %p - %p",
                FEAT_SIZE, FEAT_SIZE, array, (uint8_t*)array + (count * FEAT_SIZE));

    // Check if array pointer itself is readable
    int32_t test = 0;
    if (!safe_memory_read_i32((mach_vm_address_t)array, &test)) {
        log_message("[StaticData] ARRAY BASE IS NOT READABLE at %p!", array);
        return;
    }

    // Dump first 64 bytes of array base
    log_message("[StaticData] First 64 bytes at array base %p:", array);
    for (int row = 0; row < 4; row++) {
        char hex[128] = {0};
        char* p = hex;
        for (int col = 0; col < 16; col++) {
            uint8_t byte = 0;
            mach_vm_address_t addr = (mach_vm_address_t)array + (row * 16) + col;
            if (safe_memory_read_u8(addr, &byte)) {
                p += sprintf(p, "%02X ", byte);
            } else {
                p += sprintf(p, "?? ");
            }
        }
        log_message("  +0x%02X: %s", row * 16, hex);
    }

    // Try to dump first 3 "entries" at different sizes to help identify structure
    log_message("[StaticData] Testing different entry sizes:");
    int test_sizes[] = {8, 16, 32, 64, 128, 256, FEAT_SIZE};
    for (int s = 0; s < sizeof(test_sizes)/sizeof(test_sizes[0]); s++) {
        int size = test_sizes[s];
        log_message("  Entry size=%d:", size);

        for (int i = 0; i < 3 && i < count; i++) {
            void* entry = (uint8_t*)array + (i * size);
            uint64_t val = 0;
            if (safe_memory_read_u64((mach_vm_address_t)entry, &val)) {
                log_message("    [%d] %p: first8=0x%016llx", i, entry, (unsigned long long)val);
            } else {
                log_message("    [%d] %p: UNREADABLE", i, entry);
            }
        }
    }
}

void staticdata_probe_manager(StaticDataType type, int probe_range) {
    void* mgr = g_staticdata.managers[type];
    if (!mgr) {
        log_message("[StaticData] Cannot probe %s - manager not captured", staticdata_type_name(type));
        return;
    }

    log_message("[StaticData] Probing %s manager at %p (range: 0x%X):",
                staticdata_type_name(type), mgr, probe_range);

    // Dump hex view of manager structure (using safe reads)
    for (int offset = 0; offset < probe_range; offset += 16) {
        char hex[64] = {0};
        char ascii[20] = {0};
        uint8_t chunk[16] = {0};

        int chunk_size = 16 < (probe_range - offset) ? 16 : (probe_range - offset);
        if (!safe_memory_read(
                (mach_vm_address_t)((uint8_t*)mgr + offset),
                chunk, chunk_size)) {
            log_message("  +0x%02X: <unreadable>", offset);
            continue;
        }

        for (int i = 0; i < chunk_size; i++) {
            sprintf(hex + strlen(hex), "%02X ", chunk[i]);
            ascii[i] = isprint(chunk[i]) ? chunk[i] : '.';
        }

        log_message("  +0x%02X: %-48s %s", offset, hex, ascii);
    }
}
