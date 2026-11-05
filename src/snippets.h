#ifndef PRISME_SNIPPETS_H
#define PRISME_SNIPPETS_H
#include "buffer.h"

#define SNIPPETS_MAX 64
#define SNIPPET_TRIGGER 64
#define SNIPPET_TITLE 96
#define SNIPPET_BODY_MAX 16384u

typedef struct {
    char trigger[SNIPPET_TRIGGER], title[SNIPPET_TITLE];
    char *body;
} Snippet;
typedef struct {
    Snippet items[SNIPPETS_MAX];
    int count;
} Snippets;

void snippets_free(Snippets *library);
bool snippets_valid(const Snippets *library);
bool snippets_clone(Snippets *out, const Snippets *library);
bool snippets_put(Snippets *library, int index, const char *trigger, const char *title, const char *body,
                  char *error, size_t cap);
bool snippets_remove(Snippets *library, int index);
bool snippets_default(Snippets *library);
bool snippets_file(char *out, size_t cap, const char *settings);
/* Missing files return false and an empty error. All loads are transactional. */
bool snippets_load(Snippets *out, const char *file, char *error, size_t cap);
bool snippets_save(const Snippets *library, const char *file, char *error, size_t cap);
int snippets_match(const Snippets *library, const char *prefix, int *indices, int capacity);
/* One undo transaction. Literal text; no execution or variable substitution.
   Subsequent lines inherit the document line's exact leading spaces/tabs. */
bool snippet_expand(Buffer *document, const Snippet *snippet, size_t start, size_t end, char *error,
                    size_t cap);
#endif
