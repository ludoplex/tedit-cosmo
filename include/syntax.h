/*
 * syntax.h - Syntax highlighting
 */
#ifndef TEDIT_SYNTAX_H
#define TEDIT_SYNTAX_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum Language {
    LANG_NONE = 0,
    LANG_COSMO_C,
    LANG_AMD64,
    LANG_AARCH64,
    LANG_MASM64,
    LANG_MASM32
} Language;

typedef enum TokenType {
    TOK_DEFAULT = 0,
    TOK_KEYWORD,
    TOK_REGISTER,
    TOK_DIRECTIVE,
    TOK_NUMBER,
    TOK_STRING,
    TOK_COMMENT,
    TOK_OPERATOR,
    TOK_IDENTIFIER
} TokenType;

typedef struct SyntaxToken {
    size_t start;
    size_t length;
    TokenType type;
} SyntaxToken;

Language syntax_detect_language(const char *filename);
const char *syntax_language_name(Language lang);

/* Tokenize a line for syntax highlighting */
int syntax_tokenize_line(Language lang, const char *line, size_t len,
                         SyntaxToken *tokens, size_t max_tokens);

#ifdef __cplusplus
}
#endif

#endif /* TEDIT_SYNTAX_H */

