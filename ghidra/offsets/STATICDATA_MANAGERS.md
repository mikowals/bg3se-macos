# StaticData Manager Structure Analysis

> **Revision 2026-09-15 (4.1.1.7398727, live):** the TypeContext `manager_ptr`
> is **not** a bank and never was on this build. It is the address of
> `ls::TypeId<Manager, ls::ImmutableDataHeadmaster>::m_TypeIndex` (the
> `eoc::FeatManager` slot at runtime `0x10cd9fc30` is unslid `0x108927c30`,
> the `nm` address of that global; `eoc::CharacterCreationAppearanceVisualManager`
> `0x10cd9fac0` ↔ `0x108927ac0`). Reading `+0x7C`/`+0x80` from it returns
> neighbouring globals, which is why the "HASHMAP" status lines showed
> `Feat count=0`, `Class count=24576, ptr_array=0x75`, and why `GetAll()` was
> empty or garbage for eight of nine types (only `ActionResource`, captured by
> its Get<T> hook, held real entries). The bank is resolved from the
> type index through the headmaster's `HashMap<StaticDataTypeIndex,
> GuidResourceBankBase*>` (`lookup_manager_by_type_index`), for every type,
> at post-init and lazily on access.
>
> **Bank layout (matches Windows `GuidResourceBank<T>`):** `GuidResourceBankBase`
> is 0x50 bytes (vtable, `LSXRegionName`, `LSXResourceNodeName`,
> `ResourceGuidsByMod` HashMap 0x40); `HashMap<Guid, T> Resources` follows at
> +0x50, so `Keys.size` is at **+0x7C** and `Values.buf` at **+0x80**, a flat
> `T[]` with stride `sizeof(T)`. Every entry starts with the resource vtable
> pointer, so the stride is measurable at runtime as the distance to its next
> repeat (`detect_entry_stride`). A candidate must also repeat at entry 2
> when the bank holds three or more entries, so a field inside entry 0 that
> happens to hold the vtable word is not taken for the stride, and nothing
> below 0x18 (vtable + ResourceUUID) is accepted. Cached banks are
> re-resolved through the table at every SessionLoaded and replaced if they
> moved. Live strides on 7398727:
>
> | Type | Count | Stride | Previous constant |
> |------|-------|--------|-------------------|
> | Feat | 41 | 0x128 | 0x128 |
> | Race | 156 | 0x168 | 0x200 (estimate, wrong) |
> | Background | 22 | 0x70 | 0x80 (estimate, wrong) |
> | God | 24 | 0x60 | 0x60 |
> | Class | 70 | 0x110 | 0x100 (estimate, wrong) |
> | ActionResource | 87 | 0x60 | 0x80 (estimate, wrong) |
> | FeatDescription | 41 | 0x60 | 0x80 (estimate, wrong) |
> | Origin, Progression, CharacterCreationAppearanceVisual | see the Phase 6 section of `docs/parity-100/LIVE-VERIFICATION-2026-09-14.md` | | |
>
> The sections below are the December 2025 analysis and are kept for the
> FeatManager::GetFeats access-path evidence; their "TypeContext metadata IS
> a bank" conclusion is superseded.


## Summary

Analysis of FeatManager and other ImmutableDataHeadmaster-managed types revealed two distinct structures:

1. **TypeContext Metadata** (what we capture via hooks)
2. **Real GuidResourceBank** (what GetFeats/GetAll uses)

## Key Discovery

**TypeContext gives registration metadata, NOT the real manager instances.**

### TypeContext Structure (Metadata)
Found via `ImmutableDataHeadmaster::m_State` traversal:
```
TypeInfo at +0x00: manager_ptr (metadata)
TypeInfo at +0x08: type_name (C string)
TypeInfo at +0x18: next pointer
```

The `manager_ptr` points to metadata with:
- `+0x00`: int32 count (e.g., 37 feats)
- `+0x08`: unknown
- This is NOT the real GuidResourceBank!

### Real FeatManager Structure
Discovered via Ghidra decompilation of `eoc::FeatManager::GetFeats()` at `0x101b752b4`:
```c
// x1 = FeatManager* (real instance)
count = *(int*)(x1 + 0x7C);   // Feat count at +0x7C
array = *(Feat**)(x1 + 0x80); // Feat array at +0x80
// Each Feat is 0x128 (296) bytes
```

### Access Pattern
```
Environment* env         // Passed to ApplyAndValidateLevelUp
  +0x130: FeatManager*   // Real FeatManager instance
```

## Function Addresses

| Function | Address | Purpose |
|----------|---------|---------|
| `FeatManager::GetFeats` | `0x101b752b4` | Receives FeatManager* in x1, outputs Feat** |
| `GetAllFeats` | `0x10120b3e8` | Calls GetFeats, extracts GUIDs |
| `ApplyAndValidateLevelUp` | `0x1011f344c` | Level-up controller, loads FeatManager from env+0x130 |
| `RegisterType<FeatManager>` | `0x100c64b14` | TypeContext registration |

## Critical Insight

The disassembly at the GetFeats call site:
```asm
1011f4c84: ldr x1,[x22, #0x130]    ; Load FeatManager from Environment+0x130
1011f4c88: add x0,sp,#0x3e0        ; Output buffer
1011f4c8c: bl 0x101b752b4          ; Call GetFeats
```

Where `x22` = first parameter to ApplyAndValidateLevelUp (Environment*).

## Why Our Hooks Failed

1. **TypeContext capture** gives metadata (count at +0x00), not real manager
2. **GetFeats hook with Dobby** breaks the original function (trampoline corrupts PC-relative instructions)
3. **The real FeatManager** is accessed via Environment, not a global singleton

## Recommended Approaches

### Option 1: Frida Interceptor (onEnter only)
Hook GetFeats with `Interceptor.attach`, capture FeatManager* from x1 without replacing the function.
```javascript
Interceptor.attach(getFeatsAddr, {
    onEnter: function(args) {
        var featMgr = args[1];  // x1 = FeatManager*
        // Capture and store for later use
    }
});
```

### Option 2: Pre-call Hook (HookZz style)
Use a hooking framework that supports pre-call only (no trampoline replacement).

### Option 3: Alternative Capture Point
Find a function called during initialization (not UI display) where FeatManager is accessible and hook that instead.

## Data Structure Summary

```
ImmutableDataHeadmaster (singleton via TypeContext)
  └─ m_State (+0x083c4a68): TypeInfo linked list head
       └─ TypeInfo for each manager type:
            +0x00: metadata_ptr (NOT real manager)
            +0x08: type_name_ptr
            +0x18: next TypeInfo*

Environment (passed as parameter)
  +0x130: FeatManager* (REAL instance)
           +0x7C: int32 count
           +0x80: Feat* array
                  Each Feat: 0x128 bytes
                    +0x08: GUID part 1 (8 bytes)
                    +0x10: GUID part 2 (8 bytes)
```

## Verification

Created Frida script at `tools/frida/capture_environment_featmgr.js` to verify these findings.
Run with: `frida -l capture_environment_featmgr.js bg3`

Then trigger feat selection (level up or respec) to capture the real FeatManager address.

## Frida Capture Workflow (Dec 2025)

Since the real FeatManager is not accessible via a global symbol and Dobby hooks break the UI,
the recommended workflow uses Frida to capture the pointer at runtime:

### Step-by-Step Guide

1. **Launch BG3 with BG3SE:**
   ```bash
   ./scripts/launch_bg3.sh
   ```

2. **In a separate terminal, run the Frida capture script:**
   ```bash
   frida -U -n "Baldur's Gate 3" -l tools/frida/capture_featmanager_live.js
   ```

3. **In game:** Open respec (Withers) or level-up and navigate to the feat selection screen.
   This triggers GetFeats which the Frida script intercepts.

4. **Frida outputs:**
   ```
   [+] GetFeats called with FeatManager: 0x600012345678
   [+] Count (+0x7C) = 37
   [+] Array (+0x80) = 0x600098765432
   [+] Wrote FeatManager info to /tmp/bg3se_featmanager.txt
   ```

5. **In BG3SE console, load the capture:**
   ```lua
   Ext.StaticData.LoadFridaCapture()  -- Returns true on success
   ```

6. **Now GetAll returns real data:**
   ```lua
   local feats = Ext.StaticData.GetAll("Feat")
   for i, feat in ipairs(feats) do
       Ext.Print(feat.ResourceUUID)
   end
   ```

### API Functions (historical, Dec 2025)

> The Frida-capture workflow below predates the headmaster resolution described
> at the top of this file and is kept for the record only. `GetAll`/`Get` no
> longer need a capture; `Ext.StaticData.ProbeRaw(type)` reports the live bank.


| Function | Description |
|----------|-------------|
| `Ext.StaticData.LoadFridaCapture()` | Load captured FeatManager from /tmp/bg3se_featmanager.txt |
| `Ext.StaticData.FridaCaptureAvailable()` | Check if capture file exists |
| `Ext.StaticData.GetCount("Feat")` | Get feat count (works with metadata only) |
| `Ext.StaticData.GetAll("Feat")` | Get all feats (requires Frida capture) |
| `Ext.StaticData.Get("Feat", guid)` | Get feat by GUID (requires Frida capture) |

### Limitations

- Frida capture must be redone after game restart (pointers change)
- Only FeatManager is currently supported via Frida capture
- TypeContext capture gives count but not actual data
