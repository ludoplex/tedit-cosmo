/*
 * syntax.c - Syntax highlighting
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "syntax.h"
#include "util.h"

Language syntax_detect_language(const char *filename) {
    if (!filename) return LANG_NONE;
    
    const char *ext = path_extension(filename);
    if (!ext) return LANG_NONE;
    
    /* C files */
    if (strcmp(ext, ".c") == 0 || strcmp(ext, ".h") == 0) {
        return LANG_COSMO_C;
    }
    
    /* Assembly files */
    if (strcmp(ext, ".asm") == 0 || strcmp(ext, ".s") == 0 ||
        strcmp(ext, ".S") == 0 || strcmp(ext, ".inc") == 0) {
        /* Try to detect MASM vs GNU as */
        /* Default to MASM64 for .asm files */
        if (strcmp(ext, ".asm") == 0) {
            return LANG_MASM64;
        }
        return LANG_AMD64;
    }
    
    return LANG_NONE;
}

const char *syntax_language_name(Language lang) {
    switch (lang) {
        case LANG_COSMO_C: return "Cosmopolitan C";
        case LANG_AMD64:   return "AMD64 Assembly";
        case LANG_AARCH64: return "AArch64 Assembly";
        case LANG_MASM64:  return "MASM64";
        case LANG_MASM32:  return "MASM32";
        default:           return "Plain Text";
    }
}

/* C keywords */
static const char *c_keywords[] = {
    "auto", "break", "case", "char", "const", "continue", "default", "do",
    "double", "else", "enum", "extern", "float", "for", "goto", "if",
    "inline", "int", "long", "register", "restrict", "return", "short",
    "signed", "sizeof", "static", "struct", "switch", "typedef", "union",
    "unsigned", "void", "volatile", "while", "_Bool", "_Complex", "_Imaginary",
    /* Cosmopolitan-specific */
    "cosmo", "pledge", "unveil",
    NULL
};

/* AMD64 registers */
static const char *amd64_registers[] = {
    "rax", "rbx", "rcx", "rdx", "rsi", "rdi", "rbp", "rsp",
    "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
    "eax", "ebx", "ecx", "edx", "esi", "edi", "ebp", "esp",
    "ax", "bx", "cx", "dx", "si", "di", "bp", "sp",
    "al", "bl", "cl", "dl", "ah", "bh", "ch", "dh",
    NULL
};

/* MASM directives */
static const char *masm_directives[] = {
    ".code", ".data", ".const", ".data?", ".stack",
    "proc", "endp", "end", "include", "includelib",
    "invoke", "addr", "offset", "ptr", "byte", "word", "dword", "qword",
    "local", "macro", "endm", "if", "else", "endif", "ifdef", "ifndef",
    NULL
};

static int is_keyword(const char *word, const char **list) {
    for (int i = 0; list[i]; i++) {
        if (strcasecmp(word, list[i]) == 0) return 1;
    }
    return 0;
}

int syntax_tokenize_line(Language lang, const char *line, size_t len,
                         SyntaxToken *tokens, size_t max_tokens) {
    size_t token_count = 0;
    size_t i = 0;
    
    while (i < len && token_count < max_tokens) {
        /* Skip whitespace */
        while (i < len && isspace((unsigned char)line[i])) i++;
        if (i >= len) break;
        
        SyntaxToken *tok = &tokens[token_count];
        tok->start = i;
        tok->type = TOK_DEFAULT;
        
        /* Comments */
        if (lang == LANG_COSMO_C) {
            if (line[i] == '/' && i + 1 < len) {
                if (line[i+1] == '/') {
                    tok->length = len - i;
                    tok->type = TOK_COMMENT;
                    token_count++;
                    break;
                }
            }
        } else if (lang == LANG_MASM64 || lang == LANG_MASM32 || 
                   lang == LANG_AMD64 || lang == LANG_AARCH64) {
            if (line[i] == ';') {
                tok->length = len - i;
                tok->type = TOK_COMMENT;
                token_count++;
                break;
            }
        }
        
        /* Strings */
        if (line[i] == '"' || line[i] == '\'') {
            char quote = line[i++];
            tok->type = TOK_STRING;
            while (i < len && line[i] != quote) {
                if (line[i] == '\\' && i + 1 < len) i++;
                i++;
            }
            if (i < len) i++; /* closing quote */
            tok->length = i - tok->start;
            token_count++;
            continue;
        }
        
        /* Numbers */
        if (isdigit((unsigned char)line[i]) ||
            (line[i] == '0' && i + 1 < len && tolower(line[i+1]) == 'x')) {
            tok->type = TOK_NUMBER;
            while (i < len && (isxdigit((unsigned char)line[i]) || 
                               line[i] == 'x' || line[i] == 'X' ||
                               line[i] == 'h' || line[i] == 'H')) {
                i++;
            }
            tok->length = i - tok->start;
            token_count++;
            continue;
        }
        
        /* Identifiers/keywords */
        if (isalpha((unsigned char)line[i]) || line[i] == '_' || line[i] == '.') {
            while (i < len && (isalnum((unsigned char)line[i]) || 
                               line[i] == '_' || line[i] == '?' || line[i] == '.')) {
                i++;
            }
            tok->length = i - tok->start;
            
            /* Extract word for keyword check */
            char word[64] = {0};
            size_t wlen = tok->length < 63 ? tok->length : 63;
            memcpy(word, line + tok->start, wlen);
            
            if (lang == LANG_COSMO_C && is_keyword(word, c_keywords)) {
                tok->type = TOK_KEYWORD;
            } else if ((lang == LANG_AMD64 || lang == LANG_MASM64 || lang == LANG_MASM32) 
                       && is_keyword(word, amd64_registers)) {
                tok->type = TOK_REGISTER;
            } else if ((lang == LANG_MASM64 || lang == LANG_MASM32)
                       && is_keyword(word, masm_directives)) {
                tok->type = TOK_DIRECTIVE;
            } else {
                tok->type = TOK_IDENTIFIER;
            }
            
            token_count++;
            continue;
        }
        
        /* Single character operators */
        tok->type = TOK_OPERATOR;
        tok->length = 1;
        i++;
        token_count++;
    }
    
    return (int)token_count;
}

