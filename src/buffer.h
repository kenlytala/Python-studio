#ifndef PRISME_BUFFER_H
#define PRISME_BUFFER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BUF_NO_SELECTION SIZE_MAX
#define BUF_LIMIT (2u * 1024u * 1024u)
#define BUF_HISTORY 128
#define BUF_PATH 4096
#define BUF_MAX_CARETS 256

typedef struct {
    size_t cursor, anchor;
} BufCaret;
typedef struct {
    size_t start, end;
    const char *text;
} BufEdit;

typedef struct {
    char *text;
    size_t len, cursor, anchor;
    BufCaret *extra;
    int extra_count;
} Snapshot;

typedef struct {
    Snapshot items[BUF_HISTORY];
    int count;
    size_t bytes;
} History;

typedef struct {
    char *text, *saved;
    size_t len, capacity, cursor, anchor;
    size_t *lines, line_count;
    unsigned long version, lines_version;
    History undo, redo;
    BufCaret extra[BUF_MAX_CARETS - 1];
    int extra_count;
    bool crlf, bom, xml_auto_disabled;
    char path[BUF_PATH], name[256], recovery_origin[BUF_PATH];
    bool session_pending_remote;
    float scroll_x, scroll_y;
} Buffer;

size_t utf8_next(const char *s, size_t len, size_t pos);
size_t utf8_prev(const char *s, size_t pos);
int utf8_encode(unsigned codepoint, char out[5]);
bool utf8_valid(const char *s, size_t len);

bool buf_init(Buffer *b, const char *text, const char *name);
void buf_free(Buffer *b);
bool buf_dirty(const Buffer *b);
bool buf_selection(const Buffer *b, size_t *start, size_t *end);
char *buf_selected(const Buffer *b);
bool buf_replace(Buffer *b, size_t start, size_t end, const char *text);
bool buf_insert(Buffer *b, const char *text);
bool buf_paste(Buffer *b, const char *text);
/* Multi-caret edits are one atomic undo transaction, always UTF-8 aligned. */
int buf_caret_count(const Buffer *b);
BufCaret buf_caret(const Buffer *b, int index);
void buf_set_carets(Buffer *b, const BufCaret *carets, int count);
void buf_clear_carets(Buffer *b);
bool buf_add_caret(Buffer *b, size_t cursor, size_t anchor);
void buf_toggle_caret(Buffer *b, size_t position);
/* Add the nearest unoccupied caret above/below (direction -1/+1), without
   moving existing carets. Short lines clamp to EOL; column is visual (tabs).
   The explicit-column variant lets a keyboard gesture keep its initial column. */
bool buf_add_vertical_caret_at_column(Buffer *b, int direction, size_t column);
bool buf_add_vertical_caret(Buffer *b, int direction);
bool buf_select_next(Buffer *b);
void buf_select_occurrences(Buffer *b);
bool buf_apply_edits(Buffer *b, const BufEdit *edits, size_t count);
void buf_home_end(Buffer *b, bool end, bool document, bool select);
char *buf_copy_carets(Buffer *b, bool cut);
bool buf_type(Buffer *b, unsigned codepoint);
void buf_move(Buffer *b, size_t position, bool select);
void buf_horizontal(Buffer *b, int direction, bool word, bool select);
void buf_vertical(Buffer *b, int direction, bool select);
void buf_select_all(Buffer *b);
void buf_backspace(Buffer *b, bool word);
void buf_delete(Buffer *b, bool word);
void buf_newline(Buffer *b);
void buf_indent(Buffer *b, bool outdent);
void buf_comment(Buffer *b);
void buf_duplicate(Buffer *b);
bool buf_undo(Buffer *b);
bool buf_redo(Buffer *b);
void buf_index(Buffer *b);
size_t buf_row(Buffer *b, size_t position);
size_t buf_line_end(Buffer *b, size_t row);
size_t buf_column(Buffer *b, size_t position);
size_t buf_position(Buffer *b, size_t row, size_t visual_column);
size_t buf_word(const Buffer *b, size_t position, int direction);
size_t buf_find(const Buffer *b, const char *query, size_t from, bool backwards);
bool buf_load(Buffer *b, const char *path, char *error, size_t error_size);
typedef bool (*BufSaveGuard)(void *context, const char *path, char *error, size_t cap);
bool buf_save_guarded(Buffer *b, const char *path, BufSaveGuard guard, void *context, char *error,
                      size_t cap);
bool buf_save(Buffer *b, const char *path, char *error, size_t error_size);

#endif
