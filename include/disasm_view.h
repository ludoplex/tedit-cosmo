/*
 * disasm_view.h - Disassembly view for binary files
 * 
 * Integrates with cosmo-disasm library for x86-64 and ARM64 disassembly.
 * Used for viewing compiled binaries, shared objects, and APE executables.
 */
#ifndef TEDIT_DISASM_VIEW_H
#define TEDIT_DISASM_VIEW_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declare - from cosmo-disasm */
struct CosmoDisasm;
struct CosmoInsn;

/* Architecture enum (mirrors cosmo-disasm) */
typedef enum {
    DISASM_ARCH_UNKNOWN = 0,
    DISASM_ARCH_X86_64  = 1,
    DISASM_ARCH_AARCH64 = 2,
} DisasmArch;

/* Display format options */
typedef enum {
    DISASM_FMT_INTEL = 0,       /* Intel syntax (default for x86) */
    DISASM_FMT_ATT   = 1,       /* AT&T syntax */
    DISASM_FMT_ARM   = 2,       /* ARM syntax (for aarch64) */
} DisasmFormat;

/* A cached line of disassembly for display */
typedef struct {
    uint64_t address;           /* Virtual address */
    size_t file_offset;         /* Offset in file */
    uint8_t bytes[16];          /* Instruction bytes */
    int byte_count;             /* Number of bytes */
    char mnemonic[32];          /* Instruction mnemonic */
    char operands[96];          /* Formatted operands */
    char comment[64];           /* Optional comment (symbol, etc.) */
    bool is_branch;             /* Is this a branch/jump? */
    bool is_call;               /* Is this a call? */
    uint64_t branch_target;     /* Branch target address if applicable */
} DisasmLine;

/* Section information from ELF/PE/APE */
typedef struct {
    char name[64];
    uint64_t vaddr;             /* Virtual address */
    size_t offset;              /* File offset */
    size_t size;                /* Section size */
    bool executable;            /* Contains code? */
} DisasmSection;

/* Symbol for display */
typedef struct {
    char name[128];
    uint64_t address;
    size_t size;
    bool is_function;
} DisasmSymbol;

/* Disassembly view state */
typedef struct DisasmView {
    /* File data */
    uint8_t *data;              /* Loaded file bytes */
    size_t data_size;
    char file_path[260];
    
    /* Architecture */
    DisasmArch arch;
    DisasmFormat format;
    
    /* Sections */
    DisasmSection *sections;
    size_t section_count;
    size_t active_section;      /* Currently viewed section */
    
    /* Symbols */
    DisasmSymbol *symbols;
    size_t symbol_count;
    
    /* Disassembly cache */
    DisasmLine *lines;          /* Cached disassembled lines */
    size_t line_count;
    size_t line_capacity;
    
    /* View state */
    size_t scroll_line;         /* First visible line */
    size_t cursor_line;         /* Selected line */
    uint64_t goto_address;      /* Address to jump to (0 if none) */
    
    /* Disassembler context (from cosmo-disasm) */
    struct CosmoDisasm *disasm;
    
    /* Status */
    int loaded;
    char error[256];
} DisasmView;

/* ============================================================================
 * Core Functions
 * ============================================================================ */

/* Create/destroy view */
DisasmView *disasm_view_create(void);
void disasm_view_destroy(DisasmView *view);

/* Load a binary file (ELF, PE, APE, raw) */
int disasm_view_load_file(DisasmView *view, const char *path);

/* Force specific architecture (for raw binaries) */
void disasm_view_set_arch(DisasmView *view, DisasmArch arch);

/* Set display format */
void disasm_view_set_format(DisasmView *view, DisasmFormat format);

/* ============================================================================
 * Navigation
 * ============================================================================ */

/* Jump to address */
int disasm_view_goto_address(DisasmView *view, uint64_t address);

/* Jump to symbol by name */
int disasm_view_goto_symbol(DisasmView *view, const char *name);

/* Get line at index (with lazy disassembly) */
const DisasmLine *disasm_view_get_line(DisasmView *view, size_t index);

/* Get visible lines for rendering */
size_t disasm_view_get_visible_lines(DisasmView *view, 
                                      size_t start, size_t count,
                                      DisasmLine *out);

/* ============================================================================
 * Section Navigation
 * ============================================================================ */

/* Get section count */
size_t disasm_view_section_count(DisasmView *view);

/* Get section by index */
const DisasmSection *disasm_view_get_section(DisasmView *view, size_t index);

/* Switch to section */
int disasm_view_select_section(DisasmView *view, size_t index);

/* ============================================================================
 * Symbol Lookup
 * ============================================================================ */

/* Find symbol at address */
const DisasmSymbol *disasm_view_symbol_at(DisasmView *view, uint64_t address);

/* Find symbol by name (prefix match) */
size_t disasm_view_find_symbols(DisasmView *view, const char *prefix,
                                 DisasmSymbol *out, size_t max);

/* ============================================================================
 * Analysis Helpers
 * ============================================================================ */

/* Check if address is a function start */
bool disasm_view_is_function_start(DisasmView *view, uint64_t address);

/* Get xrefs to address (basic) */
size_t disasm_view_get_xrefs_to(DisasmView *view, uint64_t address,
                                 uint64_t *out, size_t max);

/* ============================================================================
 * Export
 * ============================================================================ */

/* Export disassembly to text file */
int disasm_view_export(DisasmView *view, const char *path,
                        uint64_t start, uint64_t end);

#ifdef __cplusplus
}
#endif

#endif /* TEDIT_DISASM_VIEW_H */
