#include "syntax.h"
#include <stdbool.h>
#include <string.h>

/* The shared lexer vocabulary is inserted below; all scanning is bounded by n. */
static const char *const js_keywords[] = {
    "as",       "async",    "await",  "break",  "case",   "catch",  "class",      "const",   "continue",
    "debugger", "default",  "delete", "do",     "else",   "export", "extends",    "finally", "for",
    "from",     "function", "get",    "if",     "import", "in",     "instanceof", "let",     "new",
    "of",       "return",   "set",    "static", "super",  "switch", "throw",      "try",     "typeof",
    "var",      "void",     "while",  "with",   "yield",  NULL};
static const char *const js_builtins[] = {
    "Array",   "ArrayBuffer", "BigInt",   "Boolean",    "Date",   "Error",  "Function",   "Infinity",
    "Intl",    "JSON",        "Map",      "Math",       "NaN",    "Number", "Object",     "Promise",
    "Proxy",   "Reflect",     "RegExp",   "Set",        "String", "Symbol", "Uint8Array", "WeakMap",
    "WeakSet", "console",     "document", "globalThis", "window", NULL};
static const char *const js_values[] = {"Infinity", "NaN",  "false",     "null",
                                        "this",     "true", "undefined", NULL};
static const char *const js_expression[] = {"await", "case",       "delete", "do",    "else",
                                            "in",    "instanceof", "new",    "of",    "return",
                                            "throw", "typeof",     "void",   "yield", NULL};
static const char *const js_definitions[] = {"class", "const", "function", "let", "var", NULL};

static bool starts(const char *s, size_t n, size_t p, const char *word) {
    size_t k = strlen(word);
    return k <= n - p && !memcmp(s + p, word, k);
}
static bool listed(const char *s, size_t n, const char *const *words) {
    for (int i = 0; words[i]; i++)
        if (strlen(words[i]) == n && !memcmp(s, words[i], n))
            return true;
    return false;
}
static bool alpha(unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}
static bool digit(unsigned char c) {
    return c >= '0' && c <= '9';
}
static bool name_start(unsigned char c) {
    return alpha(c) || c == '_' || c == '$' || c >= 128;
}
static bool name_part(unsigned char c) {
    return name_start(c) || digit(c);
}
static size_t character(const char *s, size_t n, size_t p, unsigned *value) {
    unsigned char c = (unsigned char)s[p];
    size_t size = c < 128 ? 1 : c < 224 ? 2 : c < 240 ? 3 : 4;
    if (p + size > n)
        size = 1;
    unsigned result = size == 1 ? c : c & ((1u << (7 - size)) - 1);
    for (size_t i = 1; i < size; i++) {
        unsigned char q = (unsigned char)s[p + i];
        if ((q & 192) != 128) {
            size = 1;
            result = c;
            break;
        }
        result = (result << 6) | (q & 63);
    }
    *value = result;
    return size;
}
static size_t space_size(const char *s, size_t n, size_t p) {
    unsigned c;
    size_t size = character(s, n, p, &c);
    return (c == 32 || (c >= 9 && c <= 13) || (c >= 28 && c <= 31) || c == 0x85 || c == 0xa0 || c == 0x1680 ||
            (c >= 0x2000 && c <= 0x200a) || c == 0x2028 || c == 0x2029 || c == 0x202f || c == 0x205f ||
            c == 0x3000)
               ? size
               : 0;
}
static void paint(uint8_t *colors, size_t start, size_t end, SyntaxKind kind) {
    if (end > start)
        memset(colors + start, kind, end - start);
}
static void javascript(const char *s, size_t n, uint8_t *colors) {
    struct {
        bool expression;
        unsigned depth;
    } templates[256];
    unsigned level = 0;
    size_t p = 0;
    bool expect = true, definition = false;
    while (p < n) {
        if (level && !templates[level - 1].expression) {
            size_t start = p;
            bool finished = false;
            while (p < n) {
                if (s[p] == '\\') {
                    p = p + 1 < n ? p + 2 : n;
                    continue;
                }
                if (s[p] == '`') {
                    p++;
                    paint(colors, start, p, SYN_STRING);
                    level--;
                    expect = false;
                    finished = true;
                    break;
                }
                if (starts(s, n, p, "${")) {
                    paint(colors, start, p, SYN_STRING);
                    paint(colors, p, p + 2, SYN_OPERATOR);
                    p += 2;
                    templates[level - 1].expression = true;
                    templates[level - 1].depth = 0;
                    expect = true;
                    finished = true;
                    break;
                }
                p++;
            }
            if (!finished)
                paint(colors, start, p, SYN_STRING);
            continue;
        }
        size_t start = p, space = space_size(s, n, p);
        unsigned char c = (unsigned char)s[p];
        if (space) {
            p += space;
            continue;
        }
        if (starts(s, n, p, "//") || (!p && starts(s, n, p, "#!"))) {
            while (p < n && s[p] != '\n')
                p++;
            paint(colors, start, p, SYN_COMMENT);
            continue;
        }
        if (starts(s, n, p, "/*")) {
            p += 2;
            while (p < n && !starts(s, n, p, "*/"))
                p++;
            if (p < n)
                p += 2;
            paint(colors, start, p, SYN_COMMENT);
            continue;
        }
        if (c == '\'' || c == '"') {
            p++;
            while (p < n) {
                if (s[p] == '\\') {
                    p = p + 1 < n ? p + 2 : n;
                    continue;
                }
                if ((unsigned char)s[p] == c) {
                    p++;
                    break;
                }
                if (s[p] == '\n')
                    break;
                p++;
            }
            paint(colors, start, p, SYN_STRING);
            expect = false;
            definition = false;
            continue;
        }
        if (c == '`') {
            if (level == 256) {
                paint(colors, p, n, SYN_STRING);
                break;
            }
            paint(colors, p, p + 1, SYN_STRING);
            p++;
            templates[level].expression = false;
            templates[level].depth = 0;
            level++;
            definition = false;
            continue;
        }
        if (c == '/' && expect) {
            bool group = false;
            p++;
            while (p < n && s[p] != '\n') {
                if (s[p] == '\\') {
                    p = p + 1 < n ? p + 2 : n;
                    continue;
                }
                if (s[p] == '[')
                    group = true;
                else if (s[p] == ']')
                    group = false;
                else if (s[p] == '/' && !group) {
                    p++;
                    while (p < n && alpha((unsigned char)s[p]))
                        p++;
                    break;
                }
                p++;
            }
            paint(colors, start, p, SYN_STRING);
            expect = false;
            definition = false;
            continue;
        }
        if (digit(c) || (c == '.' && p + 1 < n && digit((unsigned char)s[p + 1]))) {
            p++;
            while (p < n &&
                   (alpha((unsigned char)s[p]) || digit((unsigned char)s[p]) || s[p] == '_' || s[p] == '.'))
                p++;
            paint(colors, start, p, SYN_NUMBER);
            expect = false;
            definition = false;
            continue;
        }
        if (name_start(c)) {
            p++;
            while (p < n && name_part((unsigned char)s[p]))
                p++;
            size_t count = p - start;
            SyntaxKind kind = listed(s + start, count, js_keywords)   ? SYN_KEYWORD
                              : listed(s + start, count, js_values)   ? SYN_CONSTANT
                              : definition                            ? SYN_DEFINITION
                              : listed(s + start, count, js_builtins) ? SYN_BUILTIN
                                                                      : SYN_TEXT;
            paint(colors, start, p, kind);
            definition = listed(s + start, count, js_definitions);
            expect = listed(s + start, count, js_expression);
            continue;
        }
        if (c == '}' && level && templates[level - 1].expression) {
            if (!templates[level - 1].depth) {
                paint(colors, p, p + 1, SYN_OPERATOR);
                p++;
                templates[level - 1].expression = false;
                continue;
            }
            templates[level - 1].depth--;
        } else if (c == '{' && level && templates[level - 1].expression)
            templates[level - 1].depth++;
        p++;
        if ((c == '+' || c == '-') && p < n && (unsigned char)s[p] == c) {
            p++;
            expect = false;
        } else
            expect = !c || !strchr(")]}.", c);
        paint(colors, start, p, (c && strchr("+-*/%=<>!&|^~:.,?()[]{}@#", c)) ? SYN_OPERATOR : SYN_TEXT);
    }
}
static bool xml_name(unsigned char c) {
    return alpha(c) || digit(c) || c == '_' || c == '-' || c == ':' || c == '.' || c >= 128;
}
static size_t entity(const char *s, size_t n, size_t p) {
    size_t q = p + 1;
    unsigned count = 1;
    while (q < n && count <= 40 && !strchr(";<>&\"' \t\r\n", s[q])) {
        unsigned value;
        q += character(s, n, q, &value);
        count++;
    }
    return q < n && s[q] == ';' && q > p + 1 ? q + 1 : p;
}
static void xml(const char *s, size_t n, uint8_t *colors) {
    size_t p = 0;
    bool inside = false, tag = false;
    while (p < n) {
        size_t start = p;
        if (!inside) {
            if (starts(s, n, p, "<!--")) {
                p += 4;
                while (p < n && !starts(s, n, p, "-->"))
                    p++;
                if (p < n)
                    p += 3;
                paint(colors, start, p, SYN_COMMENT);
                continue;
            }
            if (starts(s, n, p, "<![CDATA[")) {
                p += 9;
                while (p < n && !starts(s, n, p, "]]>"))
                    p++;
                if (p < n)
                    p += 3;
                paint(colors, start, p, SYN_STRING);
                continue;
            }
            if (starts(s, n, p, "<!")) {
                p += 2;
                char quote = 0;
                unsigned brackets = 0;
                while (p < n) {
                    char c = s[p++];
                    if (quote) {
                        if (c == quote)
                            quote = 0;
                    } else if (c == '\'' || c == '"')
                        quote = c;
                    else if (c == '[')
                        brackets++;
                    else if (c == ']') {
                        if (brackets)
                            brackets--;
                    } else if (c == '>' && !brackets)
                        break;
                }
                paint(colors, start, p, SYN_CONSTANT);
                continue;
            }
            if (s[p] == '<') {
                p++;
                if (p < n && (s[p] == '/' || s[p] == '?'))
                    p++;
                paint(colors, start, p, SYN_OPERATOR);
                inside = true;
                tag = true;
                continue;
            }
            if (s[p] == '&') {
                size_t end = entity(s, n, p);
                if (end > p) {
                    paint(colors, p, end, SYN_CONSTANT);
                    p = end;
                    continue;
                }
            }
            p++;
            continue;
        }
        size_t space = space_size(s, n, p);
        unsigned char c = (unsigned char)s[p];
        if (space) {
            p += space;
            continue;
        }
        if (c == '\'' || c == '"') {
            p++;
            while (p < n && (unsigned char)s[p] != c)
                p++;
            if (p < n)
                p++;
            paint(colors, start, p, SYN_STRING);
            size_t q = start + 1;
            while (q < p) {
                if (s[q] == '&') {
                    size_t end = entity(s, p, q);
                    if (end > q) {
                        paint(colors, q, end, SYN_CONSTANT);
                        q = end;
                        continue;
                    }
                }
                q++;
            }
            continue;
        }
        if (c == '>' || ((c == '/' || c == '?') && p + 1 < n && s[p + 1] == '>')) {
            p += c == '>' ? 1 : 2;
            paint(colors, start, p, SYN_OPERATOR);
            inside = false;
            tag = false;
            continue;
        }
        if (xml_name(c)) {
            p++;
            while (p < n && xml_name((unsigned char)s[p]))
                p++;
            paint(colors, start, p, tag ? SYN_DEFINITION : SYN_BUILTIN);
            tag = false;
            continue;
        }
        paint(colors, p, p + 1, (c && strchr("=</?", c)) ? SYN_OPERATOR : SYN_TEXT);
        p++;
    }
}
static bool extension(const char *value, const char *expected) {
    while (*value && *expected) {
        unsigned char c = (unsigned char)*value++;
        if (c >= 'A' && c <= 'Z')
            c = (unsigned char)(c + 32);
        if (c != (unsigned char)*expected++)
            return false;
    }
    return !*value && !*expected;
}
SyntaxLanguage syntax_language(const char *filename) {
    const char *dot = filename ? strrchr(filename, '.') : NULL;
    if (!dot)
        return SYNTAX_PLAIN;
    if (extension(dot, ".py") || extension(dot, ".pyw") || extension(dot, ".pyi"))
        return SYNTAX_PYTHON;
    if (extension(dot, ".js") || extension(dot, ".mjs") || extension(dot, ".cjs"))
        return SYNTAX_JAVASCRIPT;
    if (extension(dot, ".xml") || extension(dot, ".xsl") || extension(dot, ".xslt") || extension(dot, ".svg"))
        return SYNTAX_XML;
    return SYNTAX_PLAIN;
}
void syntax_colorize_language(const char *s, size_t n, uint8_t *colors, SyntaxLanguage language) {
    if (language == SYNTAX_PYTHON) {
        syntax_colorize(s, n, colors);
        return;
    }
    memset(colors, SYN_TEXT, n);
    if (language == SYNTAX_JAVASCRIPT)
        javascript(s, n, colors);
    else if (language == SYNTAX_XML)
        xml(s, n, colors);
}
