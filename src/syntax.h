#ifndef PRISME_SYNTAX_H
#define PRISME_SYNTAX_H
#include <stddef.h>
#include <stdint.h>

typedef enum {
    SYN_TEXT,
    SYN_KEYWORD,
    SYN_STRING,
    SYN_NUMBER,
    SYN_COMMENT,
    SYN_DEFINITION,
    SYN_BUILTIN,
    SYN_OPERATOR,
    SYN_CONSTANT
} SyntaxKind;
typedef enum { SYNTAX_PLAIN, SYNTAX_PYTHON, SYNTAX_JAVASCRIPT, SYNTAX_XML } SyntaxLanguage;
SyntaxLanguage syntax_language(const char *filename);
void syntax_colorize_language(const char *source, size_t len, uint8_t *colors, SyntaxLanguage language);
void syntax_colorize(const char *source, size_t len, uint8_t *colors);
int syntax_complete(const char *source, const char *prefix, char words[][96], int max_words);
#endif
