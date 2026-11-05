#ifndef PRISME_SSH_JSON_H
#define PRISME_SSH_JSON_H
#include <stdbool.h>
#include <stddef.h>
/* Small bounded JSON codec. No evaluation, external parser or temporary file. */
typedef enum { SJ_OBJECT, SJ_ARRAY, SJ_STRING, SJ_NUMBER, SJ_TRUE, SJ_FALSE, SJ_NULL } SjType;
typedef struct {
    SjType type;
    size_t start, len;
    int count, next;
} SjToken;
typedef struct {
    const char *text;
    size_t len;
    SjToken *tokens;
    int count, capacity;
} SjDoc;
bool sj_parse(SjDoc *doc, const char *text, size_t len, SjToken *tokens, int capacity);
int sj_get(const SjDoc *doc, int object, const char *key);
bool sj_string(const SjDoc *doc, int token, char *out, size_t cap);
bool sj_is(const SjDoc *doc, int token, SjType type);
typedef struct {
    char *text;
    size_t len, cap;
    bool ok;
} SjOut;
void sj_out(SjOut *out, char *text, size_t cap);
bool sj_add(SjOut *out, const char *text);
bool sj_quote(SjOut *out, const char *text);
#endif
