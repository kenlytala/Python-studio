#define _POSIX_C_SOURCE 200809L
#include "buffer.h"
#include "multicursor.h"
#include "xml_edit.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#define getpid _getpid
#include <process.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

static char *copy_n(const char *s, size_t n) {
    char *p = malloc(n + 1);
    if (p) {
        memcpy(p, s, n);
        p[n] = 0;
    }
    return p;
}

size_t utf8_next(const char *s, size_t len, size_t p) {
    if (p >= len)
        return len;
    p++;
    while (p < len && ((unsigned char)s[p] & 0xc0) == 0x80)
        p++;
    return p;
}
size_t utf8_prev(const char *s, size_t p) {
    if (!p)
        return 0;
    p--;
    while (p && ((unsigned char)s[p] & 0xc0) == 0x80)
        p--;
    return p;
}
int utf8_encode(unsigned c, char out[5]) {
    int n = 0;
    if (c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff)) {
        out[0] = 0;
        return 0;
    }
    if (c < 0x80)
        out[n++] = (char)c;
    else if (c < 0x800) {
        out[n++] = (char)(0xc0 | (c >> 6));
        out[n++] = (char)(0x80 | (c & 63));
    } else if (c < 0x10000 && !(c >= 0xd800 && c <= 0xdfff)) {
        out[n++] = (char)(0xe0 | (c >> 12));
        out[n++] = (char)(0x80 | ((c >> 6) & 63));
        out[n++] = (char)(0x80 | (c & 63));
    } else if (c <= 0x10ffff) {
        out[n++] = (char)(0xf0 | (c >> 18));
        out[n++] = (char)(0x80 | ((c >> 12) & 63));
        out[n++] = (char)(0x80 | ((c >> 6) & 63));
        out[n++] = (char)(0x80 | (c & 63));
    }
    out[n] = 0;
    return n;
}
bool utf8_valid(const char *s, size_t len) {
    for (size_t p = 0; p < len;) {
        unsigned c = (unsigned char)s[p++], value;
        int n;
        if (!c)
            return false;
        if (c < 128)
            continue;
        if (c >= 0xc2 && c <= 0xdf) {
            n = 1;
            value = c & 31;
        } else if (c >= 0xe0 && c <= 0xef) {
            n = 2;
            value = c & 15;
        } else if (c >= 0xf0 && c <= 0xf4) {
            n = 3;
            value = c & 7;
        } else
            return false;
        if (p + (size_t)n > len)
            return false;
        for (int j = 0; j < n; j++) {
            c = (unsigned char)s[p++];
            if ((c & 0xc0) != 0x80)
                return false;
            value = (value << 6) | (c & 63);
        }
        if ((n == 1 && value < 128) || (n == 2 && value < 2048) || (n == 3 && value < 65536) ||
            value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff))
            return false;
    }
    return true;
}
static size_t boundary(const Buffer *b, size_t p) {
    if (p > b->len)
        p = b->len;
    while (p && p < b->len && ((unsigned char)b->text[p] & 0xc0) == 0x80)
        p--;
    return p;
}
static void hist_clear(History *h) {
    for (int i = 0; i < h->count; i++) {
        free(h->items[i].text);
        free(h->items[i].extra);
    }
    h->count = 0;
    h->bytes = 0;
}
static size_t snapshot_bytes(const Snapshot *s) {
    return s->len + (size_t)s->extra_count * sizeof(BufCaret);
}
static bool hist_push(History *h, const Buffer *b) {
    char *text = copy_n(b->text, b->len);
    BufCaret *extra = b->extra_count ? malloc((size_t)b->extra_count * sizeof(BufCaret)) : NULL;
    if (!text || (b->extra_count && !extra)) {
        free(text);
        free(extra);
        return false;
    }
    if (extra)
        memcpy(extra, b->extra, (size_t)b->extra_count * sizeof(BufCaret));
    Snapshot s = {text, b->len, b->cursor, b->anchor, extra, b->extra_count};
    while (h->count && (h->count == BUF_HISTORY || h->bytes + snapshot_bytes(&s) > 16u * 1024u * 1024u)) {
        h->bytes -= snapshot_bytes(&h->items[0]);
        free(h->items[0].text);
        free(h->items[0].extra);
        memmove(h->items, h->items + 1, (size_t)(--h->count) * sizeof(Snapshot));
    }
    h->items[h->count++] = s;
    h->bytes += snapshot_bytes(&s);
    return true;
}
static bool checkpoint(Buffer *b) {
    if (!hist_push(&b->undo, b))
        return false;
    hist_clear(&b->redo);
    return true;
}
static int edit_compare(const void *a, const void *b) {
    const BufEdit *x = a, *y = b;
    if (x->start != y->start)
        return x->start < y->start ? -1 : 1;
    return x->end < y->end ? -1 : x->end > y->end;
}
static size_t map_edited_position(size_t position, const BufEdit *edits, size_t count) {
    ptrdiff_t shift = 0;
    for (size_t i = 0; i < count; i++) {
        size_t n = strlen(edits[i].text);
        if (position < edits[i].start)
            break;
        if (position < edits[i].end || (position == edits[i].start && edits[i].start == edits[i].end))
            return (size_t)((ptrdiff_t)edits[i].start + shift) + n;
        shift += (ptrdiff_t)n - (ptrdiff_t)(edits[i].end - edits[i].start);
    }
    return (size_t)((ptrdiff_t)position + shift);
}
bool buf_apply_edits(Buffer *b, const BufEdit *edits, size_t count) {
    if (!count)
        return true;
    if (count > BUF_LIMIT + BUF_MAX_CARETS)
        return false;
    BufEdit *sorted = malloc(count * sizeof(*sorted));
    if (!sorted)
        return false;
    memcpy(sorted, edits, count * sizeof(*sorted));
    qsort(sorted, count, sizeof(*sorted), edit_compare);
    size_t used = 0, total = b->len;
    bool changed = false;
    for (size_t i = 0; i < count; i++) {
        BufEdit e = sorted[i];
        if (!e.text || e.start > e.end || e.end > b->len || boundary(b, e.start) != e.start ||
            boundary(b, e.end) != e.end)
            goto fail;
        size_t n = strlen(e.text);
        if (n > BUF_LIMIT || !utf8_valid(e.text, n))
            goto fail;
        if (used && (e.start < sorted[used - 1].end || e.start == sorted[used - 1].start)) {
            /* Adjacent/overlapping backspaces must not delete any byte twice. */
            if (*e.text || *sorted[used - 1].text)
                goto fail;
            if (e.end > sorted[used - 1].end)
                sorted[used - 1].end = e.end;
        } else
            sorted[used++] = e;
    }
    for (size_t i = 0; i < used; i++) {
        size_t n = strlen(sorted[i].text), removed = sorted[i].end - sorted[i].start;
        if (total - removed > BUF_LIMIT - n)
            goto fail;
        total = total - removed + n;
        if (removed || n)
            changed = true;
    }
    if (!changed) {
        free(sorted);
        return true;
    }
    char *out = malloc(total + 1);
    if (!out)
        goto fail;
    size_t from = 0, to = 0;
    for (size_t i = 0; i < used; i++) {
        size_t n = sorted[i].start - from, m = strlen(sorted[i].text);
        memcpy(out + to, b->text + from, n);
        to += n;
        memcpy(out + to, sorted[i].text, m);
        to += m;
        from = sorted[i].end;
    }
    memcpy(out + to, b->text + from, b->len - from + 1);
    if (!checkpoint(b)) {
        free(out);
        goto fail;
    }
    BufCaret carets[BUF_MAX_CARETS];
    int nc = buf_caret_count(b);
    for (int i = 0; i < nc; i++) {
        carets[i] = buf_caret(b, i);
        carets[i].cursor = map_edited_position(carets[i].cursor, sorted, used);
        if (carets[i].anchor != BUF_NO_SELECTION)
            carets[i].anchor = map_edited_position(carets[i].anchor, sorted, used);
    }
    free(b->text);
    b->text = out;
    b->len = total;
    b->capacity = total + 1;
    b->version++;
    buf_set_carets(b, carets, nc);
    free(sorted);
    return true;
fail:
    free(sorted);
    return false;
}
bool buf_init(Buffer *b, const char *text, const char *name) {
    memset(b, 0, sizeof(*b));
    b->len = strlen(text);
    if (b->len > BUF_LIMIT || !utf8_valid(text, b->len))
        return false;
    b->text = copy_n(text, b->len);
    b->saved = copy_n(text, b->len);
    if (!b->text || !b->saved) {
        buf_free(b);
        return false;
    }
    b->capacity = b->len + 1;
    b->anchor = BUF_NO_SELECTION;
    b->version = 1;
    snprintf(b->name, sizeof(b->name), "%s", name ? name : "sans_titre.py");
    return true;
}
void buf_free(Buffer *b) {
    free(b->text);
    free(b->saved);
    free(b->lines);
    hist_clear(&b->undo);
    hist_clear(&b->redo);
    memset(b, 0, sizeof(*b));
}
bool buf_dirty(const Buffer *b) {
    return strcmp(b->text, b->saved) != 0;
}
bool buf_selection(const Buffer *b, size_t *a, size_t *z) {
    if (b->anchor == BUF_NO_SELECTION || b->anchor == b->cursor)
        return false;
    *a = b->anchor < b->cursor ? b->anchor : b->cursor;
    *z = b->anchor > b->cursor ? b->anchor : b->cursor;
    return true;
}
char *buf_selected(const Buffer *b) {
    if (b->extra_count)
        return multi_selected(b);
    size_t a, z;
    return buf_selection(b, &a, &z) ? copy_n(b->text + a, z - a) : copy_n("", 0);
}
static bool replace_raw(Buffer *b, size_t a, size_t z, const char *text, bool history) {
    a = boundary(b, a);
    z = boundary(b, z);
    if (a > z) {
        size_t t = a;
        a = z;
        z = t;
    }
    size_t n = strlen(text), total = b->len - (z - a) + n;
    if (total > BUF_LIMIT || !utf8_valid(text, n))
        return false;
    char *out = malloc(total + 1);
    if (!out)
        return false;
    memcpy(out, b->text, a);
    memcpy(out + a, text, n);
    memcpy(out + a + n, b->text + z, b->len - z + 1);
    if (history && !checkpoint(b)) {
        free(out);
        return false;
    }
    free(b->text);
    b->text = out;
    b->len = total;
    b->capacity = total + 1;
    b->cursor = a + n;
    b->anchor = BUF_NO_SELECTION;
    b->extra_count = 0;
    b->version++;
    return true;
}
bool buf_replace(Buffer *b, size_t a, size_t z, const char *text) {
    return replace_raw(b, a, z, text, true);
}
bool buf_insert(Buffer *b, const char *text) {
    if (b->extra_count)
        return multi_edit(b, MULTI_INSERT, text, 0);
    size_t a = b->cursor, z = a;
    buf_selection(b, &a, &z);
    return buf_replace(b, a, z, text);
}
void buf_move(Buffer *b, size_t p, bool select) {
    b->extra_count = 0;
    if (select) {
        if (b->anchor == BUF_NO_SELECTION)
            b->anchor = b->cursor;
    } else
        b->anchor = BUF_NO_SELECTION;
    b->cursor = boundary(b, p);
}
static bool wordchar(unsigned char c) {
    return c >= 128 || isalnum(c) || c == '_';
}
size_t buf_word(const Buffer *b, size_t p, int direction) {
    if (direction < 0) {
        while (p && isspace((unsigned char)b->text[utf8_prev(b->text, p)]))
            p = utf8_prev(b->text, p);
        if (!p)
            return 0;
        bool word = wordchar((unsigned char)b->text[utf8_prev(b->text, p)]);
        while (p) {
            size_t prev = utf8_prev(b->text, p);
            if (isspace((unsigned char)b->text[prev]) || wordchar((unsigned char)b->text[prev]) != word)
                break;
            p = prev;
        }
    } else {
        if (p < b->len) {
            bool word = wordchar((unsigned char)b->text[p]);
            while (p < b->len && !isspace((unsigned char)b->text[p]) &&
                   wordchar((unsigned char)b->text[p]) == word)
                p = utf8_next(b->text, b->len, p);
        }
        while (p < b->len && isspace((unsigned char)b->text[p]))
            p = utf8_next(b->text, b->len, p);
    }
    return p;
}
void buf_horizontal(Buffer *b, int d, bool word, bool select) {
    if (b->extra_count) {
        multi_move(b, d < 0 ? MULTI_LEFT : MULTI_RIGHT, word ? 1 : 0, select);
        return;
    }
    size_t a, z;
    if (!select && buf_selection(b, &a, &z)) {
        buf_move(b, d < 0 ? a : z, false);
        return;
    }
    size_t p = word    ? buf_word(b, b->cursor, d)
               : d < 0 ? utf8_prev(b->text, b->cursor)
                       : utf8_next(b->text, b->len, b->cursor);
    buf_move(b, p, select);
}
void buf_index(Buffer *b) {
    if (b->lines_version == b->version && b->lines)
        return;
    size_t count = 1;
    for (size_t i = 0; i < b->len; i++)
        if (b->text[i] == '\n')
            count++;
    size_t *lines = realloc(b->lines, count * sizeof(size_t));
    if (!lines)
        return;
    b->lines = lines;
    b->line_count = count;
    count = 0;
    lines[count++] = 0;
    for (size_t i = 0; i < b->len; i++)
        if (b->text[i] == '\n')
            lines[count++] = i + 1;
    b->lines_version = b->version;
}
size_t buf_row(Buffer *b, size_t p) {
    buf_index(b);
    size_t lo = 0, hi = b->line_count;
    while (lo + 1 < hi) {
        size_t m = (lo + hi) / 2;
        if (b->lines[m] <= p)
            lo = m;
        else
            hi = m;
    }
    return lo;
}
size_t buf_line_end(Buffer *b, size_t row) {
    buf_index(b);
    return row + 1 < b->line_count ? b->lines[row + 1] - 1 : b->len;
}
size_t buf_column(Buffer *b, size_t p) {
    size_t row = buf_row(b, p), col = 0;
    for (size_t i = b->lines[row]; i < p; i = utf8_next(b->text, b->len, i))
        col += b->text[i] == '\t' ? 4 - col % 4 : 1;
    return col;
}
size_t buf_position(Buffer *b, size_t row, size_t col) {
    buf_index(b);
    if (row >= b->line_count)
        row = b->line_count - 1;
    size_t p = b->lines[row], end = buf_line_end(b, row), current = 0;
    while (p < end && current < col) {
        current += b->text[p] == '\t' ? 4 - current % 4 : 1;
        p = utf8_next(b->text, b->len, p);
    }
    return p;
}
void buf_vertical(Buffer *b, int direction, bool select) {
    if (b->extra_count) {
        multi_move(b, MULTI_VERTICAL, direction, select);
        return;
    }
    size_t row = buf_row(b, b->cursor), col = buf_column(b, b->cursor);
    long next = (long)row + direction;
    if (next < 0)
        next = 0;
    buf_move(b, buf_position(b, (size_t)next, col), select);
}
void buf_select_all(Buffer *b) {
    b->extra_count = 0;
    b->anchor = 0;
    b->cursor = b->len;
}
void buf_home_end(Buffer *b, bool end, bool document, bool select) {
    if (b->extra_count) {
        multi_move(b, end ? MULTI_END : MULTI_HOME, document, select);
        return;
    }
    size_t row = buf_row(b, b->cursor), p = b->lines[row], z = buf_line_end(b, row);
    while (p < z && (b->text[p] == ' ' || b->text[p] == '\t'))
        p++;
    buf_move(b, document ? (end ? b->len : 0) : end ? z : b->cursor == p ? b->lines[row] : p, select);
}
static char matching(char c) {
    switch (c) {
    case '(':
        return ')';
    case '[':
        return ']';
    case '{':
        return '}';
    case '\"':
        return '\"';
    case '\'':
        return '\'';
    default:
        return 0;
    }
}
bool buf_type(Buffer *b, unsigned c) {
    if (b->extra_count)
        return multi_edit(b, MULTI_TYPE, NULL, c);
    if (xml_edit_enabled(b) && (b->anchor == BUF_NO_SELECTION || b->anchor == b->cursor)) {
        XmlEdit edit = {0};
        int status = xml_edit_type(b->text, b->len, b->cursor, c, &edit);
        if (status) {
            bool ok = status > 0 && xml_edit_apply(b, &edit);
            free(edit.text);
            return ok;
        }
    }
    char encoded[5];
    if (!utf8_encode(c, encoded))
        return false;
    size_t a, z;
    char close = c < 128 ? matching((char)c) : 0;
    if (close && buf_selection(b, &a, &z)) {
        char *wrapped = malloc(z - a + 3);
        if (!wrapped)
            return false;
        wrapped[0] = (char)c;
        memcpy(wrapped + 1, b->text + a, z - a);
        wrapped[z - a + 1] = close;
        wrapped[z - a + 2] = 0;
        bool ok = buf_replace(b, a, z, wrapped);
        free(wrapped);
        if (ok) {
            b->anchor = a + 1;
            b->cursor--;
        }
        return ok;
    }
    if (c < 128 && strchr(")]}'\"", (int)c) && b->cursor < b->len && b->text[b->cursor] == (char)c &&
        !buf_selection(b, &a, &z)) {
        b->cursor++;
        b->anchor = BUF_NO_SELECTION;
        return true;
    }
    if (close && (b->cursor == b->len || isspace((unsigned char)b->text[b->cursor]) ||
                  strchr(")]},:", b->text[b->cursor]))) {
        char pair[3] = {(char)c, close, 0};
        bool ok = buf_insert(b, pair);
        if (ok)
            b->cursor--;
        return ok;
    }
    return buf_insert(b, encoded);
}
void buf_backspace(Buffer *b, bool word) {
    if (b->extra_count) {
        multi_edit(b, MULTI_BACKSPACE, NULL, word);
        return;
    }
    size_t a, z;
    if (buf_selection(b, &a, &z)) {
        buf_insert(b, "");
        return;
    }
    if (!b->cursor)
        return;
    size_t p = utf8_prev(b->text, b->cursor);
    if (!word && b->cursor < b->len && matching(b->text[p]) == b->text[b->cursor]) {
        buf_replace(b, p, b->cursor + 1, "");
        return;
    }
    if (word)
        p = buf_word(b, b->cursor, -1);
    else {
        size_t row = buf_row(b, b->cursor), begin = b->lines[row];
        bool spaces = true;
        for (size_t i = begin; i < b->cursor; i++)
            if (b->text[i] != ' ')
                spaces = false;
        if (spaces && b->cursor > begin)
            p = b->cursor - ((b->cursor - begin - 1) % 4 + 1);
    }
    buf_replace(b, p, b->cursor, "");
}
void buf_delete(Buffer *b, bool word) {
    if (b->extra_count) {
        multi_edit(b, MULTI_DELETE, NULL, word);
        return;
    }
    size_t a, z;
    if (buf_selection(b, &a, &z)) {
        buf_insert(b, "");
        return;
    }
    if (b->cursor == b->len)
        return;
    buf_replace(b, b->cursor, word ? buf_word(b, b->cursor, 1) : utf8_next(b->text, b->len, b->cursor), "");
}
void buf_newline(Buffer *b) {
    if (b->extra_count) {
        multi_edit(b, MULTI_NEWLINE, NULL, 0);
        return;
    }
    if (xml_edit_enabled(b) && (b->anchor == BUF_NO_SELECTION || b->anchor == b->cursor)) {
        XmlEdit edit = {0};
        int status = xml_edit_newline(b->text, b->len, b->cursor, &edit);
        if (status) {
            if (status > 0)
                xml_edit_apply(b, &edit);
            free(edit.text);
            return;
        }
    }
    size_t row = buf_row(b, b->cursor), start = b->lines[row], end = start;
    while (end < b->cursor && (b->text[end] == ' ' || b->text[end] == '\t'))
        end++;
    size_t n = end - start;
    char *value = malloc(n * 2 + 16);
    if (!value)
        return;
    value[0] = '\n';
    memcpy(value + 1, b->text + start, n);
    size_t used = 1 + n, p = b->cursor;
    while (p > start && b->text[p - 1] == ' ')
        p--;
    if (p > start && b->text[p - 1] == ':') {
        memcpy(value + used, "    ", 4);
        used += 4;
    }
    bool pair = b->cursor > start && b->cursor < b->len &&
                matching(b->text[b->cursor - 1]) == b->text[b->cursor] &&
                strchr("([{", b->text[b->cursor - 1]);
    if (pair) {
        memcpy(value + used, "    ", 4);
        used += 4;
        size_t middle = used;
        value[used++] = '\n';
        memcpy(value + used, b->text + start, n);
        used += n;
        value[used] = 0;
        size_t a = b->cursor, z = a;
        buf_selection(b, &a, &z);
        if (buf_insert(b, value))
            b->cursor = a + middle;
    } else {
        value[used] = 0;
        buf_insert(b, value);
    }
    free(value);
}
static void line_transform(Buffer *b, int mode) {
    size_t a = b->cursor, z = a;
    bool selected = buf_selection(b, &a, &z);
    size_t first = buf_row(b, a), last = buf_row(b, z);
    if (last > first && z == b->lines[last])
        last--;
    size_t start = b->lines[first], end = buf_line_end(b, last);
    bool uncomment = mode == 2;
    if (mode == 2)
        for (size_t r = first; r <= last; r++) {
            size_t p = b->lines[r], e = buf_line_end(b, r);
            while (p < e && (b->text[p] == ' ' || b->text[p] == '\t'))
                p++;
            if (p < e && b->text[p] != '#')
                uncomment = false;
        }
    size_t extra = (last - first + 1) * 4;
    char *out = malloc(end - start + extra + 1);
    if (!out)
        return;
    size_t used = 0;
    for (size_t r = first; r <= last; r++) {
        size_t p = b->lines[r], e = buf_line_end(b, r);
        if (mode == 0) {
            memcpy(out + used, "    ", 4);
            used += 4;
        } else if (mode == 1) {
            int n = 0;
            if (p < e && b->text[p] == '\t')
                p++;
            else
                while (p < e && n < 4 && b->text[p] == ' ') {
                    p++;
                    n++;
                }
        }
        if (mode == 2) {
            while (p < e && (b->text[p] == ' ' || b->text[p] == '\t'))
                out[used++] = b->text[p++];
            if (p < e) {
                if (uncomment && b->text[p] == '#') {
                    p++;
                    if (p < e && b->text[p] == ' ')
                        p++;
                } else {
                    out[used++] = '#';
                    out[used++] = ' ';
                }
            }
        }
        memcpy(out + used, b->text + p, e - p);
        used += e - p;
        if (r < last)
            out[used++] = '\n';
    }
    out[used] = 0;
    if (buf_replace(b, start, end, out)) {
        if (selected) {
            b->anchor = start;
            b->cursor = start + used;
        }
    }
    free(out);
}
void buf_indent(Buffer *b, bool outdent) {
    if (b->extra_count) {
        multi_edit(b, outdent ? MULTI_OUTDENT : MULTI_INDENT, NULL, 0);
        return;
    }
    size_t a, z;
    if (!outdent && !buf_selection(b, &a, &z)) {
        char spaces[5] = "    ";
        spaces[4 - buf_column(b, b->cursor) % 4] = 0;
        buf_insert(b, spaces);
    } else
        line_transform(b, outdent ? 1 : 0);
}
void buf_comment(Buffer *b) {
    if (b->extra_count) {
        multi_edit(b, MULTI_COMMENT, NULL, 0);
        return;
    }
    line_transform(b, 2);
}
void buf_duplicate(Buffer *b) {
    if (b->extra_count) {
        multi_edit(b, MULTI_DUPLICATE, NULL, 0);
        return;
    }
    size_t a, z;
    if (buf_selection(b, &a, &z)) {
        char *s = copy_n(b->text + a, z - a);
        if (s) {
            buf_move(b, z, false);
            buf_insert(b, s);
            free(s);
        }
    } else {
        size_t row = buf_row(b, b->cursor), start = b->lines[row], end = buf_line_end(b, row),
               col = buf_column(b, b->cursor);
        char *s = malloc(end - start + 2);
        if (!s)
            return;
        s[0] = '\n';
        memcpy(s + 1, b->text + start, end - start);
        s[end - start + 1] = 0;
        buf_replace(b, end, end, s);
        b->cursor = buf_position(b, row + 1, col);
        free(s);
    }
}
static bool restore(Buffer *b, History *from, History *to) {
    if (!from->count || !hist_push(to, b))
        return false;
    Snapshot s = from->items[--from->count];
    from->bytes -= snapshot_bytes(&s);
    free(b->text);
    b->text = s.text;
    b->len = s.len;
    b->capacity = s.len + 1;
    b->cursor = s.cursor;
    b->anchor = s.anchor;
    b->extra_count = s.extra_count;
    if (s.extra)
        memcpy(b->extra, s.extra, (size_t)s.extra_count * sizeof(BufCaret));
    free(s.extra);
    b->version++;
    return true;
}
bool buf_undo(Buffer *b) {
    return restore(b, &b->undo, &b->redo);
}
bool buf_redo(Buffer *b) {
    return restore(b, &b->redo, &b->undo);
}
size_t buf_find(const Buffer *b, const char *q, size_t from, bool back) {
    size_t n = strlen(q);
    if (!n || n > b->len)
        return SIZE_MAX;
    if (from > b->len - n)
        from = back ? b->len - n : b->len;
    for (size_t p = from; p <= b->len - n;) {
        size_t k = 0;
        while (k < n && tolower((unsigned char)b->text[p + k]) == tolower((unsigned char)q[k]))
            k++;
        if (k == n)
            return p;
        if (back) {
            if (!p)
                break;
            p--;
        } else
            p++;
    }
    return SIZE_MAX;
}

bool buf_load(Buffer *b, const char *path, char *err, size_t cap) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        snprintf(err, cap, "%s", strerror(errno));
        return false;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        snprintf(err, cap, "Lecture impossible");
        return false;
    }
    long size = ftell(f);
    rewind(f);
    if (size < 0 || size > (long)BUF_LIMIT) {
        fclose(f);
        snprintf(err, cap, "Limite : 2 Mo");
        return false;
    }
    char *raw = malloc((size_t)size + 1);
    if (!raw) {
        fclose(f);
        return false;
    }
    size_t got = fread(raw, 1, (size_t)size, f);
    bool ok = got == (size_t)size && !ferror(f);
    fclose(f);
    raw[got] = 0;
    if (!ok) {
        free(raw);
        snprintf(err, cap, "Lecture interrompue");
        return false;
    }
    bool bom = got >= 3 && !memcmp(raw, "\xef\xbb\xbf", 3), crlf = false;
    size_t j = 0;
    for (size_t i = bom ? 3 : 0; i < got; i++) {
        if (raw[i] == '\r') {
            if (i + 1 < got && raw[i + 1] == '\n') {
                crlf = true;
                i++;
            }
            raw[j++] = '\n';
        } else
            raw[j++] = raw[i];
    }
    raw[j] = 0;
    if (!utf8_valid(raw, j)) {
        free(raw);
        snprintf(err, cap, "Texte UTF-8 requis (fichier binaire ou autre encodage)");
        return false;
    }
    const char *name = strrchr(path, '/');
#ifdef _WIN32
    const char *wname = strrchr(path, '\\');
    if (wname && (!name || wname > name))
        name = wname;
#endif
    ok = buf_init(b, raw, name ? name + 1 : path);
    free(raw);
    if (ok) {
        snprintf(b->path, sizeof(b->path), "%s", path);
        b->bom = bom;
        b->crlf = crlf;
    }
    return ok;
}
bool buf_save_guarded(Buffer *b, const char *path, BufSaveGuard guard, void *context, char *err, size_t cap) {
    if (b->session_pending_remote && path && !strcmp(path, b->path)) {
        snprintf(err, cap,
                 "Reconnectez SSH pour vérifier le fichier avant d’enregistrer le brouillon restauré.");
        return false;
    }
    if (!path || !*path || strlen(path) + 60 >= BUF_PATH) {
        snprintf(err, cap, "Chemin trop long ou vide");
        return false;
    }
    if (guard && !guard(context, path, err, cap))
        return false;
    char destination[BUF_PATH];
    snprintf(destination, sizeof(destination), "%s", path);
    path = destination; /* autorise buf_save(b, b->path, ...) */
    char temporary[BUF_PATH];
    static unsigned serial = 0;
    size_t prefix = strlen(path);
    memcpy(temporary, path, prefix);
    int tail = snprintf(temporary + prefix, sizeof(temporary) - prefix, ".prisme-%ld-%u.tmp", (long)getpid(),
                        ++serial);
    if (tail < 0 || (size_t)tail >= sizeof(temporary) - prefix) {
        snprintf(err, cap, "Chemin temporaire trop long");
        return false;
    }
#ifdef _WIN32
    FILE *f = fopen(temporary, "wbx");
#else
    int fd = open(temporary, O_WRONLY | O_CREAT | O_EXCL, 0600);
    FILE *f = fd >= 0 ? fdopen(fd, "wb") : NULL;
#endif
    if (!f) {
        snprintf(err, cap, "%s", strerror(errno));
        return false;
    }
    bool ok = !guard || guard(context, path, err, cap);
    if (ok && b->bom && fwrite("\xef\xbb\xbf", 1, 3, f) != 3)
        ok = false;
    for (size_t i = 0; i < b->len && ok; i++) {
        if (b->crlf && b->text[i] == '\n' && fputc('\r', f) == EOF)
            ok = false;
        if (fputc((unsigned char)b->text[i], f) == EOF)
            ok = false;
    }
    if (fflush(f) != 0)
        ok = false;
#ifndef _WIN32
    struct stat st;
    if (stat(path, &st) == 0)
        fchmod(fileno(f), st.st_mode & 0777);
    if (fsync(fileno(f)) != 0)
        ok = false;
#endif
    if (fclose(f) != 0)
        ok = false;
    if (ok && guard && !guard(context, path, err, cap))
        ok = false;
#ifdef _WIN32
    if (ok)
        ok = MoveFileExA(temporary, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
    if (ok)
        ok = rename(temporary, path) == 0;
#endif
    if (!ok) {
        if (!*err)
            snprintf(err, cap, "%s", strerror(errno));
        if (!guard || guard(context, temporary, err, cap))
            remove(temporary);
        return false;
    }
    if (guard && !guard(context, path, err, cap))
        return false; /* remplacement incertain : le brouillon reste modifié */
    char *saved = copy_n(b->text, b->len);
    if (!saved) {
        snprintf(err, cap, "Mémoire insuffisante après sauvegarde");
        return false;
    }
    free(b->saved);
    b->saved = saved;
    snprintf(b->path, sizeof(b->path), "%s", path);
    const char *name = strrchr(path, '/');
#ifdef _WIN32
    const char *wname = strrchr(path, '\\');
    if (wname && (!name || wname > name))
        name = wname;
#endif
    snprintf(b->name, sizeof(b->name), "%.255s", name ? name + 1 : path);
    b->session_pending_remote = false;
    *b->recovery_origin = 0;
    return true;
}

bool buf_save(Buffer *b, const char *path, char *error, size_t cap) {
    return buf_save_guarded(b, path, NULL, NULL, error, cap);
}
