#include "syntax.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *keywords[] = {"and",     "as",       "assert", "async",    "await", "break",  "case",
                                 "class",   "continue", "def",    "del",      "elif",  "else",   "except",
                                 "finally", "for",      "from",   "global",   "if",    "import", "in",
                                 "is",      "lambda",   "match",  "nonlocal", "not",   "or",     "pass",
                                 "raise",   "return",   "try",    "while",    "with",  "yield",  NULL};
static const char *builtins[] = {
    "abs",          "all",        "any",        "bin",          "bool",  "bytes",  "callable", "chr",
    "dict",         "dir",        "divmod",     "enumerate",    "eval",  "filter", "float",    "format",
    "frozenset",    "getattr",    "hasattr",    "hash",         "help",  "hex",    "id",       "input",
    "int",          "isinstance", "issubclass", "iter",         "len",   "list",   "map",      "max",
    "min",          "next",       "object",     "oct",          "open",  "ord",    "pow",      "print",
    "property",     "range",      "repr",       "reversed",     "round", "set",    "slice",    "sorted",
    "staticmethod", "str",        "sum",        "super",        "tuple", "type",   "vars",     "zip",
    "Exception",    "ValueError", "TypeError",  "RuntimeError", NULL};
static bool namechar(unsigned char c) {
    return isalnum(c) || c == '_' || c >= 128;
}
static bool in_list(const char *s, size_t n, const char *const *list) {
    for (int i = 0; list[i]; i++)
        if (strlen(list[i]) == n && !memcmp(s, list[i], n))
            return true;
    return false;
}
void syntax_colorize(const char *s, size_t n, uint8_t *colors) {
    memset(colors, SYN_TEXT, n);
    bool definition = false;
    for (size_t p = 0; p < n;) {
        size_t start = p;
        SyntaxKind kind = SYN_TEXT;
        unsigned char c = (unsigned char)s[p];
        if (c == '#') {
            kind = SYN_COMMENT;
            while (p < n && s[p] != '\n')
                p++;
        } else if (c == '\'' || c == '"') {
            kind = SYN_STRING;
            bool triple = p + 2 < n && s[p + 1] == c && s[p + 2] == c;
            p += triple ? 3 : 1;
            while (p < n) {
                if (s[p] == '\\') {
                    p += p + 1 < n ? 2 : 1;
                    continue;
                }
                if ((unsigned char)s[p] == c && (!triple || (p + 2 < n && s[p + 1] == c && s[p + 2] == c))) {
                    p += triple ? 3 : 1;
                    break;
                }
                if (!triple && s[p] == '\n')
                    break;
                p++;
            }
        } else if (isdigit(c) || (c == '.' && p + 1 < n && isdigit((unsigned char)s[p + 1]))) {
            kind = SYN_NUMBER;
            p++;
            while (p < n && (isalnum((unsigned char)s[p]) || s[p] == '_' || s[p] == '.'))
                p++;
        } else if (isalpha(c) || c == '_' || c >= 128) {
            p++;
            while (p < n && namechar((unsigned char)s[p]))
                p++;
            size_t count = p - start;
            if (in_list(s + start, count, keywords)) {
                kind = SYN_KEYWORD;
                definition = (count == 3 && !memcmp(s + start, "def", 3)) ||
                             (count == 5 && !memcmp(s + start, "class", 5));
            } else if (definition) {
                kind = SYN_DEFINITION;
                definition = false;
            } else if ((count == 4 && (!memcmp(s + start, "True", 4) || !memcmp(s + start, "None", 4))) ||
                       (count == 5 && !memcmp(s + start, "False", 5)))
                kind = SYN_CONSTANT;
            else if (in_list(s + start, count, builtins))
                kind = SYN_BUILTIN;
            else {
                bool upper = true;
                for (size_t j = start; j < p; j++)
                    if (islower((unsigned char)s[j]))
                        upper = false;
                if (upper && count > 1)
                    kind = SYN_CONSTANT;
            }
        } else {
            if (strchr("+-*/%=<>!&|^~:.,()[]{}@", c))
                kind = SYN_OPERATOR;
            if (c == '\n')
                definition = false;
            p++;
        }
        memset(colors + start, kind, p - start);
    }
}
static void add(char words[][96], int *count, int max, const char *s, size_t len, const char *prefix) {
    size_t p = strlen(prefix);
    if (*count >= max || len >= 96 || len <= p || memcmp(s, prefix, p))
        return;
    for (int i = 0; i < *count; i++)
        if (strlen(words[i]) == len && !memcmp(words[i], s, len))
            return;
    memcpy(words[*count], s, len);
    words[*count][len] = 0;
    (*count)++;
}
static int compare_word(const void *a, const void *b) {
    size_t x = strlen(a), y = strlen(b);
    return x == y ? strcmp(a, b) : x < y ? -1 : 1;
}
int syntax_complete(const char *source, const char *prefix, char words[][96], int max) {
    int count = 0;
    if (!*prefix)
        return 0;
    for (int i = 0; keywords[i]; i++)
        add(words, &count, max, keywords[i], strlen(keywords[i]), prefix);
    for (int i = 0; builtins[i]; i++)
        add(words, &count, max, builtins[i], strlen(builtins[i]), prefix);
    for (size_t p = 0; source[p] && count < max;) {
        if (isalpha((unsigned char)source[p]) || source[p] == '_') {
            size_t start = p++;
            while (source[p] && namechar((unsigned char)source[p]))
                p++;
            add(words, &count, max, source + start, p - start, prefix);
        } else
            p++;
    }
    qsort(words, (size_t)count, sizeof(words[0]), compare_word);
    return count;
}
