/**
 * ARM64 Instruction Decoder
 * Issue #44 - ARM64-Safe Inline Hooking
 *
 * Provides utilities to decode ARM64 instructions for safe hook placement.
 * Key use case: Detecting ADRP+LDR patterns that are unsafe to relocate.
 */

#ifndef ARM64_DECODE_H
#define ARM64_DECODE_H

#include <stdint.h>
#include <stdbool.h>

// =============================================================================
// ARM64 Instruction Masks and Opcodes
// =============================================================================

// ADRP - Address of 4KB page at PC-relative offset
#define ARM64_ADRP_MASK     0x9F000000
#define ARM64_ADRP_OP       0x90000000

// LDR immediate (unsigned offset, 64-bit)
#define ARM64_LDR_IMM64_MASK    0xFFC00000
#define ARM64_LDR_IMM64_OP      0xF9400000

// LDR immediate (unsigned offset, 32-bit)
#define ARM64_LDR_IMM32_MASK    0xFFC00000
#define ARM64_LDR_IMM32_OP      0xB9400000

// STP pre-index (store pair, common in prologues)
#define ARM64_STP_PRE_MASK      0x7FC00000
#define ARM64_STP_PRE_OP        0x29800000

// SUB immediate (stack frame setup)
#define ARM64_SUB_IMM_MASK      0xFF000000
#define ARM64_SUB_IMM_OP        0xD1000000

// ADD immediate
#define ARM64_ADD_IMM_MASK      0xFF000000
#define ARM64_ADD_IMM_OP        0x91000000

// B - Unconditional branch
#define ARM64_B_MASK            0xFC000000
#define ARM64_B_OP              0x14000000

// BL - Branch with link (call)
#define ARM64_BL_MASK           0xFC000000
#define ARM64_BL_OP             0x94000000

// BR - Branch to register
#define ARM64_BR_MASK           0xFFFFFC1F
#define ARM64_BR_OP             0xD61F0000

// BLR - Branch with link to register
#define ARM64_BLR_MASK          0xFFFFFC1F
#define ARM64_BLR_OP            0xD63F0000

// RET - Return from subroutine
#define ARM64_RET_MASK          0xFFFFFC1F
#define ARM64_RET_OP            0xD65F0000

// CBZ/CBNZ - Compare and branch if zero/non-zero
#define ARM64_CBZ_MASK          0x7F000000
#define ARM64_CBZ_OP            0x34000000
#define ARM64_CBNZ_OP           0x35000000

// TBZ/TBNZ - Test bit and branch
#define ARM64_TBZ_MASK          0x7E000000   // bit 24 selects TBZ (0) / TBNZ (1)
#define ARM64_TBZ_OP            0x36000000

// B.cond - Conditional branch
#define ARM64_BCOND_MASK        0xFF000010
#define ARM64_BCOND_OP          0x54000000

// MOV register (actually ORR with XZR)
#define ARM64_MOV_REG_MASK      0xFF800000
#define ARM64_MOV_REG_OP        0xAA000000

// =============================================================================
// Instruction Type Enumeration
// =============================================================================

typedef enum {
    ARM64_INSN_UNKNOWN = 0,
    ARM64_INSN_ADRP,
    ARM64_INSN_LDR_IMM64,
    ARM64_INSN_LDR_IMM32,
    ARM64_INSN_STP_PRE,
    ARM64_INSN_SUB_IMM,
    ARM64_INSN_ADD_IMM,
    ARM64_INSN_B,
    ARM64_INSN_BL,
    ARM64_INSN_BR,
    ARM64_INSN_BLR,
    ARM64_INSN_RET,
    ARM64_INSN_CBZ,
    ARM64_INSN_CBNZ,
    ARM64_INSN_TBZ,
    ARM64_INSN_TBNZ,
    ARM64_INSN_BCOND,
    ARM64_INSN_MOV_REG,
    ARM64_INSN_STR_IMM,
} ARM64InsnType;

// =============================================================================
// Decoded Instruction Structure
// =============================================================================

typedef struct {
    uint32_t raw;           // Raw instruction bytes
    ARM64InsnType type;     // Decoded instruction type

    // Register operands (0-30 = X0-X30, 31 = SP or XZR depending on context)
    uint8_t rd;             // Destination register
    uint8_t rn;             // First source/base register
    uint8_t rm;             // Second source register (if applicable)
    uint8_t rt2;            // Second target for STP/LDP

    // Immediate values
    int64_t imm;            // Decoded immediate (sign-extended if applicable)

    // For ADRP: computed target page address
    uint64_t target_page;

    // For LDR: computed offset
    uint32_t ldr_offset;

    // Is this instruction PC-relative?
    bool is_pc_relative;
} ARM64DecodedInsn;

// =============================================================================
// Prologue Analysis Structure
// =============================================================================

#define MAX_ADRP_PATTERNS 8

typedef struct {
    void* func_addr;                    // Function start address
    int num_instructions;               // Number of instructions analyzed

    // ADRP+LDR pattern tracking
    int adrp_pattern_count;             // Number of ADRP+LDR pairs found
    struct {
        int adrp_offset;                // Byte offset of ADRP instruction
        int ldr_offset;                 // Byte offset of paired LDR instruction
        uint8_t reg;                    // Register used (e.g., x8)
        uint64_t resolved_target;       // Computed target address
    } adrp_patterns[MAX_ADRP_PATTERNS];

    // Safe hook point (first instruction after all ADRP+LDR sequences)
    int safe_hook_offset;               // Byte offset, -1 if none found
    int first_unsafe_offset;            // First PC-relative instruction offset

    // Prologue characteristics
    int stack_frame_size;               // Bytes allocated for stack frame
    bool has_frame_pointer;             // Uses x29 as frame pointer
} ARM64PrologueAnalysis;

// =============================================================================
// Instruction Detection Functions
// =============================================================================

/**
 * Check if instruction is ADRP
 */
static inline bool arm64_is_adrp(uint32_t insn) {
    return (insn & ARM64_ADRP_MASK) == ARM64_ADRP_OP;
}

/**
 * Check if instruction is LDR immediate (64-bit)
 */
static inline bool arm64_is_ldr_imm64(uint32_t insn) {
    return (insn & ARM64_LDR_IMM64_MASK) == ARM64_LDR_IMM64_OP;
}

/**
 * Check if instruction is LDR immediate (32-bit)
 */
static inline bool arm64_is_ldr_imm32(uint32_t insn) {
    return (insn & ARM64_LDR_IMM32_MASK) == ARM64_LDR_IMM32_OP;
}

/**
 * Check if instruction is any LDR immediate
 */
static inline bool arm64_is_ldr_imm(uint32_t insn) {
    return arm64_is_ldr_imm64(insn) || arm64_is_ldr_imm32(insn);
}

/**
 * Check if instruction is PC-relative (cannot be safely relocated)
 */
static inline bool arm64_is_pc_relative(uint32_t insn) {
    // ADRP is PC-relative
    if (arm64_is_adrp(insn)) return true;

    // ADR is also PC-relative (less common)
    if ((insn & 0x9F000000) == 0x10000000) return true;

    // Conditional branches are PC-relative
    if ((insn & ARM64_B_MASK) == ARM64_B_OP) return true;
    if ((insn & ARM64_BL_MASK) == ARM64_BL_OP) return true;
    if ((insn & ARM64_CBZ_MASK) == ARM64_CBZ_OP) return true;
    if ((insn & ARM64_CBZ_MASK) == ARM64_CBNZ_OP) return true;
    if ((insn & ARM64_TBZ_MASK) == ARM64_TBZ_OP) return true;
    if ((insn & ARM64_BCOND_MASK) == ARM64_BCOND_OP) return true;

    // LDR literal is PC-relative
    if ((insn & 0x3B000000) == 0x18000000) return true;

    return false;
}

/**
 * Check if instruction is a branch (any type)
 */
static inline bool arm64_is_branch(uint32_t insn) {
    if ((insn & ARM64_B_MASK) == ARM64_B_OP) return true;
    if ((insn & ARM64_BL_MASK) == ARM64_BL_OP) return true;
    if ((insn & ARM64_BR_MASK) == ARM64_BR_OP) return true;
    if ((insn & ARM64_BLR_MASK) == ARM64_BLR_OP) return true;
    if ((insn & ARM64_RET_MASK) == ARM64_RET_OP) return true;
    if ((insn & ARM64_CBZ_MASK) == ARM64_CBZ_OP) return true;
    if ((insn & ARM64_CBZ_MASK) == ARM64_CBNZ_OP) return true;
    if ((insn & ARM64_TBZ_MASK) == ARM64_TBZ_OP) return true;
    if ((insn & ARM64_BCOND_MASK) == ARM64_BCOND_OP) return true;
    return false;
}

// =============================================================================
// Decoding Functions
// =============================================================================

/**
 * Decode ADRP target page address
 *
 * @param insn  The ADRP instruction
 * @param pc    The program counter where this instruction resides
 * @return      The computed target page address
 */
uint64_t arm64_decode_adrp_target(uint32_t insn, uint64_t pc);

/**
 * Decode LDR immediate offset
 *
 * @param insn  The LDR instruction
 * @return      The decoded offset in bytes
 */
uint32_t arm64_decode_ldr_offset(uint32_t insn);

/**
 * Get destination register from ADRP/LDR instruction
 */
static inline uint8_t arm64_get_rd(uint32_t insn) {
    return insn & 0x1F;
}

/**
 * Get base register from LDR instruction
 */
static inline uint8_t arm64_get_rn(uint32_t insn) {
    return (insn >> 5) & 0x1F;
}

/**
 * Fully decode an ARM64 instruction
 *
 * @param insn  The raw instruction bytes
 * @param pc    The program counter (for PC-relative calculations)
 * @param out   Output structure for decoded instruction
 */
void arm64_decode_instruction(uint32_t insn, uint64_t pc, ARM64DecodedInsn* out);

/**
 * Get instruction type name as string
 */
const char* arm64_insn_type_name(ARM64InsnType type);

// =============================================================================
// Prologue Analysis Functions
// =============================================================================

/**
 * Analyze a function's prologue for ADRP+LDR patterns
 *
 * @param func_addr         Function start address
 * @param max_instructions  Maximum instructions to analyze (typically 16-32)
 * @param out               Output analysis structure
 * @return                  true on success, false on failure
 */
bool arm64_analyze_prologue(void* func_addr, int max_instructions, ARM64PrologueAnalysis* out);

/**
 * Find safe hook point in function prologue
 *
 * @param func_addr         Function start address
 * @param max_search        Maximum bytes to search
 * @return                  Safe hook offset in bytes, or -1 if none found
 */
int arm64_find_safe_hook_point(void* func_addr, int max_search);

/**
 * Check if a function is safe to hook at its entry point
 *
 * @param func_addr  Function start address
 * @return           true if safe, false if ADRP in first 16 bytes
 */
bool arm64_is_safe_to_hook_at_entry(void* func_addr);

/**
 * Print prologue analysis to log
 */
void arm64_print_prologue_analysis(const ARM64PrologueAnalysis* analysis);

#endif // ARM64_DECODE_H
