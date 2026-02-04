/*
 * disasm_view.c - Disassembly view implementation
 * 
 * Integrates with cosmo-disasm for actual disassembly.
 * Compile with -DHAVE_COSMO_DISASM and link -lcosmo-disasm when available.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "disasm_view.h"
#include "util.h"

#ifdef HAVE_COSMO_DISASM
#include <cosmo_disasm.h>
#endif

/* ============================================================================
 * ELF Header Parsing (minimal for architecture detection)
 * ============================================================================ */

#define ELF_MAGIC     "\x7fELF"
#define ELF_CLASS_64  2
#define ELF_EM_X86_64 62
#define ELF_EM_AARCH64 183

/* PE Header detection */
#define PE_MAGIC      "MZ"
#define PE_AMD64      0x8664
#define PE_ARM64      0xAA64

/* APE magic (Actually Portable Executable) */
#define APE_MAGIC     "MZqFpD"

typedef struct {
    uint8_t  magic[4];
    uint8_t  class_;
    uint8_t  data;
    uint8_t  version;
    uint8_t  osabi;
    uint8_t  pad[8];
    uint16_t type;
    uint16_t machine;
} Elf64_Ehdr_Partial;

static DisasmArch detect_elf_arch(const uint8_t *data, size_t size) {
    if (size < sizeof(Elf64_Ehdr_Partial)) return DISASM_ARCH_UNKNOWN;
    if (memcmp(data, ELF_MAGIC, 4) != 0) return DISASM_ARCH_UNKNOWN;
    
    const Elf64_Ehdr_Partial *hdr = (const Elf64_Ehdr_Partial *)data;
    if (hdr->class_ != ELF_CLASS_64) return DISASM_ARCH_UNKNOWN;
    
    /* Read machine type (handle endianness simply) */
    uint16_t machine = hdr->machine;
    
    switch (machine) {
    case ELF_EM_X86_64:  return DISASM_ARCH_X86_64;
    case ELF_EM_AARCH64: return DISASM_ARCH_AARCH64;
    default:             return DISASM_ARCH_UNKNOWN;
    }
}

static DisasmArch detect_pe_arch(const uint8_t *data, size_t size) {
    if (size < 64) return DISASM_ARCH_UNKNOWN;
    if (memcmp(data, PE_MAGIC, 2) != 0) return DISASM_ARCH_UNKNOWN;
    
    /* Get PE header offset from DOS header */
    uint32_t pe_offset = *(uint32_t *)(data + 60);
    if (pe_offset + 6 > size) return DISASM_ARCH_UNKNOWN;
    
    /* Check PE signature */
    if (memcmp(data + pe_offset, "PE\0\0", 4) != 0) return DISASM_ARCH_UNKNOWN;
    
    /* Machine type is 2 bytes after PE signature */
    uint16_t machine = *(uint16_t *)(data + pe_offset + 4);
    
    switch (machine) {
    case PE_AMD64: return DISASM_ARCH_X86_64;
    case PE_ARM64: return DISASM_ARCH_AARCH64;
    default:       return DISASM_ARCH_UNKNOWN;
    }
}

static DisasmArch detect_arch(const uint8_t *data, size_t size) {
    DisasmArch arch;
    
    /* Check APE first (starts with MZ but has special structure) */
    if (size >= 6 && memcmp(data, APE_MAGIC, 6) == 0) {
        /* APE binaries can be multi-arch; default to x86-64 for viewing */
        return DISASM_ARCH_X86_64;
    }
    
    /* Try ELF */
    arch = detect_elf_arch(data, size);
    if (arch != DISASM_ARCH_UNKNOWN) return arch;
    
    /* Try PE */
    arch = detect_pe_arch(data, size);
    if (arch != DISASM_ARCH_UNKNOWN) return arch;
    
    /* Unknown format - default to x86-64 */
    return DISASM_ARCH_X86_64;
}

/* ============================================================================
 * Section Parsing (ELF only for now)
 * ============================================================================ */

typedef struct {
    uint32_t name;
    uint32_t type;
    uint64_t flags;
    uint64_t addr;
    uint64_t offset;
    uint64_t size;
    uint32_t link;
    uint32_t info;
    uint64_t addralign;
    uint64_t entsize;
} Elf64_Shdr;

#define ELF_SHT_PROGBITS 1
#define ELF_SHT_SYMTAB   2
#define ELF_SHT_STRTAB   3
#define ELF_SHF_EXECINSTR 0x4

static void parse_elf_sections(DisasmView *view) {
    if (view->data_size < 64) return;
    if (memcmp(view->data, ELF_MAGIC, 4) != 0) return;
    
    /* Basic ELF64 parsing */
    uint64_t shoff = *(uint64_t *)(view->data + 40);
    uint16_t shentsize = *(uint16_t *)(view->data + 58);
    uint16_t shnum = *(uint16_t *)(view->data + 60);
    uint16_t shstrndx = *(uint16_t *)(view->data + 62);
    
    if (shoff == 0 || shnum == 0) return;
    if (shoff + shnum * shentsize > view->data_size) return;
    
    /* Get string table */
    const char *strtab = NULL;
    if (shstrndx < shnum) {
        const Elf64_Shdr *str_sh = (const Elf64_Shdr *)(view->data + shoff + shstrndx * shentsize);
        if (str_sh->offset + str_sh->size <= view->data_size) {
            strtab = (const char *)(view->data + str_sh->offset);
        }
    }
    
    /* Allocate sections */
    view->sections = calloc(shnum, sizeof(DisasmSection));
    if (!view->sections) return;
    
    for (uint16_t i = 0; i < shnum; i++) {
        const Elf64_Shdr *sh = (const Elf64_Shdr *)(view->data + shoff + i * shentsize);
        
        /* Only include PROGBITS sections */
        if (sh->type != ELF_SHT_PROGBITS) continue;
        if (sh->size == 0) continue;
        
        DisasmSection *sec = &view->sections[view->section_count++];
        
        if (strtab && sh->name > 0) {
            strncpy(sec->name, strtab + sh->name, sizeof(sec->name) - 1);
        } else {
            snprintf(sec->name, sizeof(sec->name), "section_%u", i);
        }
        
        sec->vaddr = sh->addr;
        sec->offset = sh->offset;
        sec->size = sh->size;
        sec->executable = (sh->flags & ELF_SHF_EXECINSTR) != 0;
    }
}

/* ============================================================================
 * View Management
 * ============================================================================ */

DisasmView *disasm_view_create(void) {
    DisasmView *view = calloc(1, sizeof(DisasmView));
    if (!view) return NULL;
    
    view->line_capacity = 1024;
    view->lines = calloc(view->line_capacity, sizeof(DisasmLine));
    if (!view->lines) {
        free(view);
        return NULL;
    }
    
    view->format = DISASM_FMT_INTEL;
    return view;
}

void disasm_view_destroy(DisasmView *view) {
    if (!view) return;
    
#ifdef HAVE_COSMO_DISASM
    if (view->disasm) {
        cosmo_disasm_free(view->disasm);
    }
#endif
    
    free(view->data);
    free(view->sections);
    free(view->symbols);
    free(view->lines);
    free(view);
}

int disasm_view_load_file(DisasmView *view, const char *path) {
    /* Load file contents */
    size_t size;
    uint8_t *data = (uint8_t *)file_read_all(path, &size);
    if (!data) {
        snprintf(view->error, sizeof(view->error), "Failed to read file: %s", path);
        return -1;
    }
    
    /* Clear previous state */
    free(view->data);
    free(view->sections);
    free(view->symbols);
    view->data = NULL;
    view->sections = NULL;
    view->symbols = NULL;
    view->section_count = 0;
    view->symbol_count = 0;
    view->line_count = 0;
    
    /* Store new data */
    view->data = data;
    view->data_size = size;
    strncpy(view->file_path, path, sizeof(view->file_path) - 1);
    
    /* Detect architecture */
    view->arch = detect_arch(data, size);
    
    /* Parse sections */
    parse_elf_sections(view);
    
    /* Initialize disassembler */
#ifdef HAVE_COSMO_DISASM
    if (view->disasm) {
        cosmo_disasm_free(view->disasm);
    }
    
    CosmoArch cosmo_arch;
    switch (view->arch) {
    case DISASM_ARCH_X86_64:  cosmo_arch = COSMO_ARCH_X86_64; break;
    case DISASM_ARCH_AARCH64: cosmo_arch = COSMO_ARCH_AARCH64; break;
    default:                  cosmo_arch = COSMO_ARCH_X86_64; break;
    }
    
    view->disasm = cosmo_disasm_create(cosmo_arch);
    if (!view->disasm) {
        snprintf(view->error, sizeof(view->error), "Failed to create disassembler");
        return -1;
    }
#endif
    
    view->loaded = 1;
    view->error[0] = '\0';
    
    /* Select first executable section, or first section */
    for (size_t i = 0; i < view->section_count; i++) {
        if (view->sections[i].executable) {
            view->active_section = i;
            break;
        }
    }
    
    return 0;
}

void disasm_view_set_arch(DisasmView *view, DisasmArch arch) {
    view->arch = arch;
    
#ifdef HAVE_COSMO_DISASM
    if (view->disasm) {
        CosmoArch cosmo_arch;
        switch (arch) {
        case DISASM_ARCH_X86_64:  cosmo_arch = COSMO_ARCH_X86_64; break;
        case DISASM_ARCH_AARCH64: cosmo_arch = COSMO_ARCH_AARCH64; break;
        default:                  cosmo_arch = COSMO_ARCH_X86_64; break;
        }
        cosmo_disasm_set_arch(view->disasm, cosmo_arch);
    }
#endif
    
    /* Invalidate cache */
    view->line_count = 0;
}

void disasm_view_set_format(DisasmView *view, DisasmFormat format) {
    view->format = format;
    /* Invalidate cache - lines need reformatting */
    view->line_count = 0;
}

/* ============================================================================
 * Disassembly
 * ============================================================================ */

static int disasm_one_instruction(DisasmView *view, size_t offset, 
                                   uint64_t vaddr, DisasmLine *out) {
    if (offset >= view->data_size) return 0;
    
    memset(out, 0, sizeof(*out));
    out->address = vaddr;
    out->file_offset = offset;
    
    size_t remaining = view->data_size - offset;
    size_t max_bytes = (remaining > 15) ? 15 : remaining;
    
#ifdef HAVE_COSMO_DISASM
    CosmoInsn insn;
    int len = cosmo_disasm_one(view->disasm, 
                               view->data + offset, remaining,
                               vaddr, &insn);
    
    if (len > 0) {
        out->byte_count = len;
        memcpy(out->bytes, view->data + offset, len);
        strncpy(out->mnemonic, insn.mnemonic, sizeof(out->mnemonic) - 1);
        
        /* Format operands from insn.text (skip mnemonic) */
        const char *operand_start = insn.text;
        while (*operand_start && *operand_start != ' ' && *operand_start != '\t') {
            operand_start++;
        }
        while (*operand_start == ' ' || *operand_start == '\t') {
            operand_start++;
        }
        strncpy(out->operands, operand_start, sizeof(out->operands) - 1);
        
        out->is_branch = insn.is_branch;
        out->is_call = insn.is_call;
        out->branch_target = insn.branch_target;
        
        return len;
    }
#else
    /* Stub disassembly without cosmo-disasm */
    /* Just show bytes as data */
    int len;
    
    if (view->arch == DISASM_ARCH_AARCH64) {
        /* ARM64: fixed 4-byte instructions */
        len = 4;
        if (max_bytes < 4) len = max_bytes;
        
        out->byte_count = len;
        memcpy(out->bytes, view->data + offset, len);
        
        if (len == 4) {
            uint32_t word = *(uint32_t *)(view->data + offset);
            snprintf(out->mnemonic, sizeof(out->mnemonic), ".word");
            snprintf(out->operands, sizeof(out->operands), "0x%08X", word);
        } else {
            strcpy(out->mnemonic, ".byte");
            out->operands[0] = '\0';
            for (int i = 0; i < len; i++) {
                char tmp[8];
                snprintf(tmp, sizeof(tmp), "%s0x%02X", i > 0 ? ", " : "", out->bytes[i]);
                strncat(out->operands, tmp, sizeof(out->operands) - strlen(out->operands) - 1);
            }
        }
    } else {
        /* x86-64: variable length, just show 1 byte at a time without real disasm */
        len = 1;
        out->byte_count = 1;
        out->bytes[0] = view->data[offset];
        
        strcpy(out->mnemonic, ".byte");
        snprintf(out->operands, sizeof(out->operands), "0x%02X", out->bytes[0]);
    }
    
    return len;
#endif
    
    /* Fallback: treat as data byte */
    out->byte_count = 1;
    out->bytes[0] = view->data[offset];
    strcpy(out->mnemonic, ".byte");
    snprintf(out->operands, sizeof(out->operands), "0x%02X", out->bytes[0]);
    
    return 1;
}

static void ensure_lines_disassembled(DisasmView *view, size_t up_to) {
    if (!view->loaded || view->section_count == 0) return;
    
    const DisasmSection *sec = &view->sections[view->active_section];
    size_t offset = sec->offset;
    uint64_t vaddr = sec->vaddr;
    
    /* Skip to where we left off */
    for (size_t i = 0; i < view->line_count && offset < sec->offset + sec->size; i++) {
        offset = view->lines[i].file_offset + view->lines[i].byte_count;
        vaddr = view->lines[i].address + view->lines[i].byte_count;
    }
    
    /* Disassemble more lines */
    while (view->line_count < up_to && offset < sec->offset + sec->size) {
        /* Grow buffer if needed */
        if (view->line_count >= view->line_capacity) {
            size_t new_cap = view->line_capacity * 2;
            DisasmLine *new_lines = realloc(view->lines, new_cap * sizeof(DisasmLine));
            if (!new_lines) break;
            view->lines = new_lines;
            view->line_capacity = new_cap;
        }
        
        DisasmLine *line = &view->lines[view->line_count];
        int len = disasm_one_instruction(view, offset, vaddr, line);
        
        if (len <= 0) break;
        
        /* Add symbol comment if at function start */
        const DisasmSymbol *sym = disasm_view_symbol_at(view, vaddr);
        if (sym && sym->is_function) {
            snprintf(line->comment, sizeof(line->comment), "<%s>", sym->name);
        }
        
        view->line_count++;
        offset += len;
        vaddr += len;
    }
}

/* ============================================================================
 * Navigation
 * ============================================================================ */

const DisasmLine *disasm_view_get_line(DisasmView *view, size_t index) {
    ensure_lines_disassembled(view, index + 1);
    
    if (index >= view->line_count) return NULL;
    return &view->lines[index];
}

size_t disasm_view_get_visible_lines(DisasmView *view, 
                                      size_t start, size_t count,
                                      DisasmLine *out) {
    ensure_lines_disassembled(view, start + count);
    
    size_t actual = 0;
    for (size_t i = start; i < start + count && i < view->line_count; i++) {
        out[actual++] = view->lines[i];
    }
    return actual;
}

int disasm_view_goto_address(DisasmView *view, uint64_t address) {
    /* Find which section contains this address */
    for (size_t i = 0; i < view->section_count; i++) {
        const DisasmSection *sec = &view->sections[i];
        if (address >= sec->vaddr && address < sec->vaddr + sec->size) {
            /* Switch to this section */
            if (i != view->active_section) {
                view->active_section = i;
                view->line_count = 0;  /* Invalidate cache */
            }
            
            /* Find line with this address */
            ensure_lines_disassembled(view, 10000);  /* Disassemble up to 10k lines */
            
            for (size_t j = 0; j < view->line_count; j++) {
                if (view->lines[j].address == address) {
                    view->scroll_line = j;
                    view->cursor_line = j;
                    return 0;
                }
                if (view->lines[j].address > address) {
                    /* Passed it - select previous */
                    view->scroll_line = (j > 0) ? j - 1 : 0;
                    view->cursor_line = view->scroll_line;
                    return 0;
                }
            }
            break;
        }
    }
    
    snprintf(view->error, sizeof(view->error), 
             "Address 0x%llx not found", (unsigned long long)address);
    return -1;
}

int disasm_view_goto_symbol(DisasmView *view, const char *name) {
    for (size_t i = 0; i < view->symbol_count; i++) {
        if (strcmp(view->symbols[i].name, name) == 0) {
            return disasm_view_goto_address(view, view->symbols[i].address);
        }
    }
    
    snprintf(view->error, sizeof(view->error), "Symbol '%s' not found", name);
    return -1;
}

/* ============================================================================
 * Section Navigation
 * ============================================================================ */

size_t disasm_view_section_count(DisasmView *view) {
    return view->section_count;
}

const DisasmSection *disasm_view_get_section(DisasmView *view, size_t index) {
    if (index >= view->section_count) return NULL;
    return &view->sections[index];
}

int disasm_view_select_section(DisasmView *view, size_t index) {
    if (index >= view->section_count) return -1;
    
    view->active_section = index;
    view->line_count = 0;  /* Invalidate cache */
    view->scroll_line = 0;
    view->cursor_line = 0;
    
    return 0;
}

/* ============================================================================
 * Symbol Lookup
 * ============================================================================ */

const DisasmSymbol *disasm_view_symbol_at(DisasmView *view, uint64_t address) {
    for (size_t i = 0; i < view->symbol_count; i++) {
        if (view->symbols[i].address == address) {
            return &view->symbols[i];
        }
    }
    return NULL;
}

size_t disasm_view_find_symbols(DisasmView *view, const char *prefix,
                                 DisasmSymbol *out, size_t max) {
    size_t len = strlen(prefix);
    size_t found = 0;
    
    for (size_t i = 0; i < view->symbol_count && found < max; i++) {
        if (strncmp(view->symbols[i].name, prefix, len) == 0) {
            out[found++] = view->symbols[i];
        }
    }
    
    return found;
}

/* ============================================================================
 * Analysis
 * ============================================================================ */

bool disasm_view_is_function_start(DisasmView *view, uint64_t address) {
    const DisasmSymbol *sym = disasm_view_symbol_at(view, address);
    return sym && sym->is_function;
}

size_t disasm_view_get_xrefs_to(DisasmView *view, uint64_t address,
                                 uint64_t *out, size_t max) {
    size_t found = 0;
    
    ensure_lines_disassembled(view, 50000);  /* Scan up to 50k lines */
    
    for (size_t i = 0; i < view->line_count && found < max; i++) {
        if (view->lines[i].branch_target == address) {
            out[found++] = view->lines[i].address;
        }
    }
    
    return found;
}

/* ============================================================================
 * Export
 * ============================================================================ */

int disasm_view_export(DisasmView *view, const char *path,
                        uint64_t start, uint64_t end) {
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    
    const char *arch_name = (view->arch == DISASM_ARCH_AARCH64) ? "aarch64" : "x86-64";
    fprintf(f, "; Disassembly of %s\n", view->file_path);
    fprintf(f, "; Architecture: %s\n", arch_name);
    fprintf(f, "; Range: 0x%llx - 0x%llx\n\n", 
            (unsigned long long)start, (unsigned long long)end);
    
    for (size_t i = 0; i < view->line_count; i++) {
        const DisasmLine *line = &view->lines[i];
        
        if (line->address < start) continue;
        if (line->address >= end) break;
        
        /* Address */
        fprintf(f, "%016llx:  ", (unsigned long long)line->address);
        
        /* Bytes */
        for (int j = 0; j < line->byte_count; j++) {
            fprintf(f, "%02x ", line->bytes[j]);
        }
        /* Pad to consistent width */
        for (int j = line->byte_count; j < 8; j++) {
            fprintf(f, "   ");
        }
        
        /* Mnemonic and operands */
        fprintf(f, "%-8s %s", line->mnemonic, line->operands);
        
        /* Comment */
        if (line->comment[0]) {
            fprintf(f, "    ; %s", line->comment);
        }
        
        fprintf(f, "\n");
    }
    
    fclose(f);
    return 0;
}
