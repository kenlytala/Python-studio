#include "ssh_json.h"
#include "buffer.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static void space(const SjDoc *d, size_t *p) {
    while (*p < d->len && strchr(" \t\r\n", d->text[*p]))
        (*p)++;
}
static int hex(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}
static bool u16(const char *s, size_t n, size_t *p, unsigned *v) {
    if (*p + 4 > n)
        return false;
    *v = 0;
    for (int i = 0; i < 4; i++) {
        int digit = hex(s[(*p)++]);
        if (digit < 0)
            return false;
        *v = *v * 16 + (unsigned)digit;
    }
    return true;
}
bool sj_string(const SjDoc *d, int token, char *out, size_t cap) {
    if (!sj_is(d, token, SJ_STRING) || !out || !cap)
        return false;
    const SjToken *t = &d->tokens[token];
    const char *s = d->text + t->start;
    size_t n = 0;
    for (size_t p = 0; p < t->len;) {
        char bytes[5];
        size_t count = 1;
        unsigned char c = (unsigned char)s[p++];
        bytes[0] = (char)c;
        if (c < 32)
            return false;
        if (c == '\\') {
            if (p >= t->len)
                return false;
            char e = s[p++];
            if (e == 'u') {
                unsigned code;
                if (!u16(s, t->len, &p, &code))
                    return false;
                if (code >= 0xd800 && code <= 0xdbff) {
                    unsigned low;
                    if (p + 2 > t->len || s[p++] != '\\' || s[p++] != 'u' || !u16(s, t->len, &p, &low) ||
                        low < 0xdc00 || low > 0xdfff)
                        return false;
                    code = 0x10000 + ((code - 0xd800) << 10) + low - 0xdc00;
                } else if (code >= 0xdc00 && code <= 0xdfff)
                    return false;
                if (!code)
                    return false;
                count = (size_t)utf8_encode(code, bytes);
                if (!count)
                    return false;
            } else {
                const char *a = "\"\\/bfnrt", *b = "\"\\/\b\f\n\r\t", *v = strchr(a, e);
                if (!v)
                    return false;
                bytes[0] = b[v - a];
            }
        }
        if (n + count >= cap)
            return false;
        memcpy(out + n, bytes, count);
        n += count;
    }
    out[n] = 0;
    return utf8_valid(out, n);
}
bool sj_is(const SjDoc *d, int token, SjType type) {
    return token >= 0 && token < d->count && d->tokens[token].type == type;
}
int sj_get(const SjDoc *d, int object, const char *key) {
    if (!sj_is(d, object, SJ_OBJECT))
        return -1;
    int found = -1;
    for (int p = object + 1; p < d->tokens[object].next;) {
        char name[128];
        if (!sj_string(d, p, name, sizeof(name)))
            return -2;
        int value = p + 1;
        if (!strcmp(name, key)) {
            if (found >= 0)
                return -2;
            found = value;
        }
        p = d->tokens[value].next;
    }
    return found;
}
static int value(SjDoc *d, size_t *p, int depth) {
    space(d, p);
    if (*p >= d->len || d->count >= d->capacity || depth > 16)
        return -1;
    int index = d->count++;
    SjToken *t = &d->tokens[index];
    memset(t, 0, sizeof(*t));
    t->start = *p;
    char c = d->text[*p];
    if (c == '{' || c == '[') {
        t->type = c == '{' ? SJ_OBJECT : SJ_ARRAY;
        char end = c == '{' ? '}' : ']';
        (*p)++;
        space(d, p);
        if (*p < d->len && d->text[*p] != end) {
            while (true) {
                if (t->type == SJ_OBJECT) {
                    if (*p >= d->len || d->text[*p] != '"' || value(d, p, depth + 1) < 0)
                        return -1;
                    space(d, p);
                    if (*p >= d->len || d->text[(*p)++] != ':')
                        return -1;
                }
                if (value(d, p, depth + 1) < 0)
                    return -1;
                t->count++;
                space(d, p);
                if (*p < d->len && d->text[*p] == ',') {
                    (*p)++;
                    space(d, p);
                } else
                    break;
            }
        }
        if (*p >= d->len || d->text[(*p)++] != end)
            return -1;
    } else if (c == '"') {
        t->type = SJ_STRING;
        (*p)++;
        t->start = *p;
        while (*p < d->len && d->text[*p] != '"') {
            unsigned char x = (unsigned char)d->text[(*p)++];
            if (x < 32)
                return -1;
            if (x == '\\') {
                if (*p >= d->len)
                    return -1;
                char e = d->text[(*p)++];
                if (e == 'u') {
                    unsigned cp;
                    if (!u16(d->text, d->len, p, &cp))
                        return -1;
                } else if (!strchr("\"\\/bfnrt", e))
                    return -1;
            }
        }
        t->len = *p - t->start;
        if (*p >= d->len)
            return -1;
        (*p)++;
    } else if (c == '-' || (c >= '0' && c <= '9')) {
        t->type = SJ_NUMBER;
        if (c == '-')
            (*p)++;
        if (*p >= d->len)
            return -1;
        if (d->text[*p] == '0')
            (*p)++;
        else {
            if (d->text[*p] < '1' || d->text[*p] > '9')
                return -1;
            while (*p < d->len && isdigit((unsigned char)d->text[*p]))
                (*p)++;
        }
        if (*p < d->len && d->text[*p] == '.') {
            size_t start = ++(*p);
            while (*p < d->len && isdigit((unsigned char)d->text[*p]))
                (*p)++;
            if (*p == start)
                return -1;
        }
        if (*p < d->len && strchr("eE", d->text[*p])) {
            (*p)++;
            if (*p < d->len && strchr("+-", d->text[*p]))
                (*p)++;
            size_t start = *p;
            while (*p < d->len && isdigit((unsigned char)d->text[*p]))
                (*p)++;
            if (*p == start)
                return -1;
        }
    } else {
        const char *word = c == 't' ? "true" : c == 'f' ? "false" : "null";
        t->type = c == 't' ? SJ_TRUE : c == 'f' ? SJ_FALSE : SJ_NULL;
        size_t n = strlen(word);
        if (*p + n > d->len || memcmp(d->text + *p, word, n))
            return -1;
        *p += n;
    }
    if (t->type != SJ_STRING)
        t->len = *p - t->start;
    t->next = d->count;
    return index;
}
bool sj_parse(SjDoc *d, const char *text, size_t len, SjToken *tokens, int capacity) {
    *d = (SjDoc){text, len, tokens, 0, capacity};
    if (memchr(text, 0, len) || !utf8_valid(text, len))
        return false;
    size_t p = 0;
    if (value(d, &p, 0) != 0)
        return false;
    space(d, &p);
    if (p != len)
        return false;
    /* Reject duplicate object keys, including ignored extension fields. */
    for (int i = 0; i < d->count; i++)
        if (tokens[i].type == SJ_OBJECT) {
            for (int j = i + 1; j < tokens[i].next; j = tokens[j + 1].next) {
                char key[128];
                if (!sj_string(d, j, key, sizeof(key)) || sj_get(d, i, key) < 0)
                    return false;
            }
        }
    return true;
}
void sj_out(SjOut *b, char *text, size_t cap) {
    *b = (SjOut){text, 0, cap, cap > 0};
    if (cap)
        text[0] = 0;
}
bool sj_add(SjOut *b, const char *text) {
    size_t n = strlen(text);
    if (!b->ok || n >= b->cap - b->len)
        return b->ok = false;
    memcpy(b->text + b->len, text, n + 1);
    b->len += n;
    return true;
}
bool sj_quote(SjOut *b, const char *text) {
    if (!utf8_valid(text, strlen(text)) || !sj_add(b, "\""))
        return b->ok = false;
    for (const unsigned char *p = (const unsigned char *)text; *p; p++) {
        char escape[8];
        if (*p == '"' || *p == '\\') {
            escape[0] = '\\';
            escape[1] = (char)*p;
            escape[2] = 0;
        } else if (*p < 32)
            snprintf(escape, sizeof(escape), "\\u%04x", *p);
        else {
            escape[0] = (char)*p;
            escape[1] = 0;
        }
        if (!sj_add(b, escape))
            return false;
    }
    return sj_add(b, "\"");
}
