#include "multicursor.h"
#include "xml_edit.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    BufCaret caret;
    size_t start, end;
    bool primary;
} Range;
static size_t aligned(const Buffer *b, size_t p) {
    if (p > b->len)
        p = b->len;
    while (p && p < b->len && ((unsigned char)b->text[p] & 0xc0) == 0x80)
        p--;
    return p;
}
static Range range(BufCaret c, bool primary) {
    size_t a = c.anchor == BUF_NO_SELECTION ? c.cursor : c.anchor;
    return (Range){c, a < c.cursor ? a : c.cursor, a > c.cursor ? a : c.cursor, primary};
}
static int compare_range(const void *x, const void *y) {
    const Range *a = x, *b = y;
    if (a->start != b->start)
        return a->start < b->start ? -1 : 1;
    return a->end < b->end ? -1 : a->end > b->end;
}
int buf_caret_count(const Buffer *b) {
    return b->extra_count + 1;
}
BufCaret buf_caret(const Buffer *b, int index) {
    return index > 0 && index <= b->extra_count ? b->extra[index - 1] : (BufCaret){b->cursor, b->anchor};
}
void buf_clear_carets(Buffer *b) {
    b->extra_count = 0;
}
void buf_set_carets(Buffer *b, const BufCaret *carets, int count) {
    if (count < 1)
        return;
    if (count > BUF_MAX_CARETS)
        count = BUF_MAX_CARETS;
    Range sorted[BUF_MAX_CARETS];
    for (int i = 0; i < count; i++) {
        BufCaret c = carets[i];
        c.cursor = aligned(b, c.cursor);
        if (c.anchor != BUF_NO_SELECTION)
            c.anchor = aligned(b, c.anchor);
        if (c.anchor == c.cursor)
            c.anchor = BUF_NO_SELECTION;
        sorted[i] = range(c, i == 0);
    }
    qsort(sorted, (size_t)count, sizeof(*sorted), compare_range);
    int used = 0;
    for (int i = 0; i < count; i++) {
        Range current = sorted[i];
        if (used && (current.start < sorted[used - 1].end || current.start == sorted[used - 1].start)) {
            Range *prev = &sorted[used - 1];
            bool reverse = prev->caret.anchor != BUF_NO_SELECTION && prev->caret.cursor < prev->caret.anchor;
            if (current.primary) {
                reverse =
                    current.caret.anchor != BUF_NO_SELECTION && current.caret.cursor < current.caret.anchor;
                prev->primary = true;
            }
            if (current.end > prev->end)
                prev->end = current.end;
            prev->caret.cursor = reverse ? prev->start : prev->end;
            prev->caret.anchor = prev->start == prev->end ? BUF_NO_SELECTION
                                 : reverse                ? prev->end
                                                          : prev->start;
        } else
            sorted[used++] = current;
    }
    b->extra_count = 0;
    for (int i = 0; i < used; i++) {
        if (sorted[i].primary) {
            b->cursor = sorted[i].caret.cursor;
            b->anchor = sorted[i].caret.anchor;
        } else
            b->extra[b->extra_count++] = sorted[i].caret;
    }
}
bool buf_add_caret(Buffer *b, size_t cursor, size_t anchor) {
    cursor = aligned(b, cursor);
    if (anchor != BUF_NO_SELECTION)
        anchor = aligned(b, anchor);
    Range added = range((BufCaret){cursor, anchor}, true);
    for (int i = 0; i < buf_caret_count(b); i++) {
        Range r = range(buf_caret(b, i), false);
        if (r.start == added.start && r.end == added.end)
            return false;
    }
    if (buf_caret_count(b) == BUF_MAX_CARETS)
        return false;
    BufCaret all[BUF_MAX_CARETS];
    int count = buf_caret_count(b);
    all[0] = (BufCaret){cursor, anchor};
    for (int i = 0; i < count; i++)
        all[i + 1] = buf_caret(b, i);
    buf_set_carets(b, all, count + 1);
    return true;
}
void buf_toggle_caret(Buffer *b, size_t p) {
    p = aligned(b, p);
    BufCaret all[BUF_MAX_CARETS];
    int used = 0, count = buf_caret_count(b);
    bool found = false;
    for (int i = 0; i < count; i++) {
        BufCaret c = buf_caret(b, i);
        if (c.cursor == p && (c.anchor == BUF_NO_SELECTION || c.anchor == p) && count > 1)
            found = true;
        else
            all[used++] = c;
    }
    if (found)
        buf_set_carets(b, all, used);
    else
        buf_add_caret(b, p, BUF_NO_SELECTION);
}
bool buf_add_vertical_caret_at_column(Buffer *b, int direction, size_t column) {
    if (!b || !b->text || (direction != -1 && direction != 1) || buf_caret_count(b) >= BUF_MAX_CARETS)
        return false;
    size_t row = buf_row(b, b->cursor);
    for (;;) {
        if (direction < 0) {
            if (!row)
                return false;
            row--;
        } else {
            if (row + 1 >= b->line_count)
                return false;
            row++;
        }
        size_t target = buf_position(b, row, column);
        bool occupied = false;
        for (int i = 0; i < buf_caret_count(b); i++) {
            Range r = range(buf_caret(b, i), false);
            if (target >= r.start && target <= r.end) {
                occupied = true;
                break;
            }
        }
        if (!occupied)
            return buf_add_caret(b, target, BUF_NO_SELECTION);
    }
}
bool buf_add_vertical_caret(Buffer *b, int direction) {
    return b && b->text && buf_add_vertical_caret_at_column(b, direction, buf_column(b, b->cursor));
}
static bool word_char(unsigned char c) {
    return c >= 128 || isalnum(c) || c == '_';
}
static bool select_word(Buffer *b) {
    size_t p = b->cursor;
    if (p == b->len || !word_char((unsigned char)b->text[p])) {
        if (!p || !word_char((unsigned char)b->text[utf8_prev(b->text, p)]))
            return false;
        p = utf8_prev(b->text, p);
    }
    size_t a = p, z = p;
    while (a && word_char((unsigned char)b->text[utf8_prev(b->text, a)]))
        a = utf8_prev(b->text, a);
    while (z < b->len && word_char((unsigned char)b->text[z]))
        z = utf8_next(b->text, b->len, z);
    b->extra_count = 0;
    b->anchor = a;
    b->cursor = z;
    return z > a;
}
bool buf_select_next(Buffer *b) {
    size_t a, z;
    if (!buf_selection(b, &a, &z))
        return select_word(b);
    if (buf_caret_count(b) == BUF_MAX_CARETS)
        return false;
    size_t n = z - a;
    char *query = malloc(n + 1);
    if (!query)
        return false;
    memcpy(query, b->text + a, n);
    query[n] = 0;
    bool added = false;
    for (int pass = 0; pass < 2 && !added; pass++) {
        size_t p = pass ? 0 : z, limit = pass ? a : b->len;
        while (p <= limit && n <= limit - p) {
            const char *match = strstr(b->text + p, query);
            if (!match)
                break;
            p = (size_t)(match - b->text);
            if (p + n > limit)
                break;
            bool overlap = false;
            for (int i = 0; i < buf_caret_count(b); i++) {
                Range r = range(buf_caret(b, i), false);
                if ((p < r.end && p + n > r.start) || (r.start == r.end && r.start >= p && r.start < p + n))
                    overlap = true;
            }
            if (!overlap) {
                added = buf_add_caret(b, p + n, p);
                break;
            }
            p += n;
        }
    }
    free(query);
    return added;
}
void buf_select_occurrences(Buffer *b) {
    size_t a, z;
    if (!buf_selection(b, &a, &z) && !select_word(b))
        return;
    while (buf_select_next(b)) {
    }
}
void multi_move(Buffer *b, int op, int argument, bool select) {
    BufCaret all[BUF_MAX_CARETS];
    int count = buf_caret_count(b);
    for (int i = 0; i < count; i++)
        all[i] = buf_caret(b, i);
    b->extra_count = 0;
    for (int i = 0; i < count; i++) {
        b->cursor = all[i].cursor;
        b->anchor = all[i].anchor;
        if (op == MULTI_LEFT || op == MULTI_RIGHT)
            buf_horizontal(b, op == MULTI_LEFT ? -1 : 1, argument != 0, select);
        else if (op == MULTI_VERTICAL)
            buf_vertical(b, argument, select);
        else
            buf_home_end(b, op == MULTI_END, argument != 0, select);
        all[i] = (BufCaret){b->cursor, b->anchor};
    }
    buf_set_carets(b, all, count);
}
char *multi_selected(const Buffer *b) {
    Range sorted[BUF_MAX_CARETS];
    int count = buf_caret_count(b);
    size_t n = 1;
    for (int i = 0; i < count; i++) {
        sorted[i] = range(buf_caret(b, i), false);
        n += sorted[i].end - sorted[i].start + 1;
    }
    qsort(sorted, (size_t)count, sizeof(*sorted), compare_range);
    char *out = malloc(n);
    if (!out)
        return NULL;
    size_t used = 0;
    bool first = true;
    for (int i = 0; i < count; i++) {
        size_t len = sorted[i].end - sorted[i].start;
        if (!len)
            continue;
        if (!first)
            out[used++] = '\n';
        memcpy(out + used, b->text + sorted[i].start, len);
        used += len;
        first = false;
    }
    out[used] = 0;
    return out;
}
static char close_char(char c) {
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
static char *copy(const char *s, size_t n) {
    char *out = malloc(n + 1);
    if (out) {
        memcpy(out, s, n);
        out[n] = 0;
    }
    return out;
}
static size_t edit_start(const BufEdit *edits, int count, int index) {
    ptrdiff_t p = (ptrdiff_t)edits[index].start;
    for (int i = 0; i < count; i++)
        if (edits[i].start < edits[index].start)
            p += (ptrdiff_t)strlen(edits[i].text) - (ptrdiff_t)(edits[i].end - edits[i].start);
    return (size_t)p;
}
static void clear_anchors(Buffer *b) {
    BufCaret carets[BUF_MAX_CARETS];
    int count = buf_caret_count(b);
    for (int i = 0; i < count; i++) {
        carets[i] = buf_caret(b, i);
        carets[i].anchor = BUF_NO_SELECTION;
    }
    buf_set_carets(b, carets, count);
}
typedef struct {
    size_t first, last;
} Rows;
static int compare_rows(const void *x, const void *y) {
    const Rows *a = x, *b = y;
    return a->first < b->first ? -1 : a->first > b->first;
}
static int selected_rows(Buffer *b, Rows *rows) {
    int count = buf_caret_count(b);
    for (int i = 0; i < count; i++) {
        Range r = range(buf_caret(b, i), false);
        size_t first = buf_row(b, r.start), last = buf_row(b, r.end);
        if (last > first && r.end == b->lines[last])
            last--;
        rows[i] = (Rows){first, last};
    }
    qsort(rows, (size_t)count, sizeof(*rows), compare_rows);
    int used = 0;
    for (int i = 0; i < count; i++) {
        if (used && rows[i].first <= rows[used - 1].last + 1) {
            if (rows[i].last > rows[used - 1].last)
                rows[used - 1].last = rows[i].last;
        } else
            rows[used++] = rows[i];
    }
    return used;
}
static bool transform_lines(Buffer *b, int op) {
    Rows rows[BUF_MAX_CARETS];
    int count = selected_rows(b, rows);
    if (op == MULTI_DUPLICATE) {
        BufEdit edits[BUF_MAX_CARETS] = {0};
        char *owned[BUF_MAX_CARETS] = {0};
        bool ok = false;
        for (int i = 0; i < count; i++) {
            size_t start = b->lines[rows[i].first], end = buf_line_end(b, rows[i].last);
            owned[i] = malloc(end - start + 2);
            if (!owned[i])
                goto duplicated;
            owned[i][0] = '\n';
            memcpy(owned[i] + 1, b->text + start, end - start);
            owned[i][end - start + 1] = 0;
            edits[i] = (BufEdit){end, end, owned[i]};
        }
        ok = buf_apply_edits(b, edits, (size_t)count);
    duplicated:
        for (int i = 0; i < count; i++)
            free(owned[i]);
        return ok;
    }
    bool uncomment = op == MULTI_COMMENT;
    size_t n = 0;
    for (int i = 0; i < count; i++) {
        n += rows[i].last - rows[i].first + 1;
        if (uncomment)
            for (size_t r = rows[i].first; r <= rows[i].last; r++) {
                size_t p = b->lines[r], z = buf_line_end(b, r);
                while (p < z && (b->text[p] == ' ' || b->text[p] == '\t'))
                    p++;
                if (p < z && b->text[p] != '#')
                    uncomment = false;
            }
    }
    /* Prefix-only edits preserve every caret/selection instead of collapsing blocks. */
    BufEdit *prefix = malloc(n * sizeof(*prefix));
    if (!prefix)
        return false;
    size_t used = 0;
    for (int i = 0; i < count; i++)
        for (size_t r = rows[i].first; r <= rows[i].last; r++) {
            size_t p = b->lines[r], z = buf_line_end(b, r), e = p;
            const char *value = "";
            if (op == MULTI_INDENT)
                value = "    ";
            else if (op == MULTI_OUTDENT) {
                if (e < z && b->text[e] == '\t')
                    e++;
                else
                    for (int k = 0; k < 4 && e < z && b->text[e] == ' '; k++)
                        e++;
            } else {
                while (p < z && (b->text[p] == ' ' || b->text[p] == '\t'))
                    p++;
                e = p;
                if (p == z)
                    continue;
                if (uncomment) {
                    e++;
                    if (e < z && b->text[e] == ' ')
                        e++;
                } else
                    value = "# ";
            }
            prefix[used++] = (BufEdit){p, e, value};
        }
    bool ok = buf_apply_edits(b, prefix, used);
    free(prefix);
    return ok;
}
bool multi_edit(Buffer *b, int op, const char *text, unsigned arg) {
    if (op == MULTI_COMMENT || op == MULTI_OUTDENT || op == MULTI_DUPLICATE)
        return transform_lines(b, op);
    if (op == MULTI_INDENT)
        for (int i = 0; i < buf_caret_count(b); i++) {
            Range r = range(buf_caret(b, i), false);
            if (r.start != r.end)
                return transform_lines(b, op);
        }
    int count = buf_caret_count(b);
    BufEdit edits[BUF_MAX_CARETS] = {0};
    BufCaret result[BUF_MAX_CARETS] = {0};
    char *owned[BUF_MAX_CARETS] = {0};
    bool ok = false;
    char encoded[5] = {0};
    if (op == MULTI_TYPE && !utf8_encode(arg, encoded))
        return false;
    for (int i = 0; i < count; i++) {
        BufCaret c = buf_caret(b, i);
        Range r = range(c, false);
        edits[i] = (BufEdit){r.start, r.end, ""};
        result[i] = (BufCaret){0, BUF_NO_SELECTION};
        if (xml_edit_enabled(b) && r.start == r.end && (op == MULTI_TYPE || op == MULTI_NEWLINE)) {
            XmlEdit edit = {0};
            int status = op == MULTI_TYPE ? xml_edit_type(b->text, b->len, c.cursor, arg, &edit)
                                          : xml_edit_newline(b->text, b->len, c.cursor, &edit);
            if (status < 0)
                goto done;
            if (status > 0) {
                owned[i] = edit.text;
                edits[i] = (BufEdit){edit.start, edit.end, edit.text};
                result[i].cursor = edit.cursor - edit.start;
                continue;
            }
        }
        if (op == MULTI_INSERT)
            edits[i].text = text;
        else if (op == MULTI_TYPE) {
            char close = arg < 128 ? close_char((char)arg) : 0;
            if (close && r.start != r.end) {
                size_t n = r.end - r.start;
                owned[i] = malloc(n + 3);
                if (!owned[i])
                    goto done;
                owned[i][0] = (char)arg;
                memcpy(owned[i] + 1, b->text + r.start, n);
                owned[i][n + 1] = close;
                owned[i][n + 2] = 0;
                edits[i].text = owned[i];
                result[i] = (BufCaret){n + 1, 1};
                continue;
            }
            if (r.start == r.end && arg < 128 && strchr(")]}'\"", (int)arg) && c.cursor < b->len &&
                b->text[c.cursor] == (char)arg) {
                result[i].cursor = 1;
                continue;
            }
            if (close && r.start == r.end &&
                (c.cursor == b->len || isspace((unsigned char)b->text[c.cursor]) ||
                 strchr(")]},:", b->text[c.cursor]))) {
                char pair[3] = {(char)arg, close, 0};
                owned[i] = copy(pair, 2);
                if (!owned[i])
                    goto done;
                edits[i].text = owned[i];
                result[i].cursor = 1;
                continue;
            }
            edits[i].text = encoded;
        } else if (op == MULTI_BACKSPACE && r.start == r.end && c.cursor) {
            size_t p = utf8_prev(b->text, c.cursor);
            if (!arg && c.cursor < b->len && close_char(b->text[p]) == b->text[c.cursor])
                edits[i].end = c.cursor + 1;
            if (arg)
                p = buf_word(b, c.cursor, -1);
            else if (edits[i].end == c.cursor) {
                size_t row = buf_row(b, c.cursor), start = b->lines[row];
                bool spaces = true;
                for (size_t k = start; k < c.cursor; k++)
                    if (b->text[k] != ' ')
                        spaces = false;
                if (spaces && c.cursor > start)
                    p = c.cursor - ((c.cursor - start - 1) % 4 + 1);
            }
            edits[i].start = p;
        } else if (op == MULTI_DELETE && r.start == r.end && c.cursor < b->len)
            edits[i].end = arg ? buf_word(b, c.cursor, 1) : utf8_next(b->text, b->len, c.cursor);
        else if (op == MULTI_NEWLINE) {
            size_t row = buf_row(b, c.cursor), start = b->lines[row], end = start;
            while (end < c.cursor && (b->text[end] == ' ' || b->text[end] == '\t'))
                end++;
            size_t n = end - start, used = 1 + n, p = c.cursor;
            owned[i] = malloc(n * 2 + 16);
            if (!owned[i])
                goto done;
            owned[i][0] = '\n';
            memcpy(owned[i] + 1, b->text + start, n);
            while (p > start && b->text[p - 1] == ' ')
                p--;
            if (p > start && b->text[p - 1] == ':') {
                memcpy(owned[i] + used, "    ", 4);
                used += 4;
            }
            bool pair = r.start == r.end && c.cursor > start && c.cursor < b->len &&
                        close_char(b->text[c.cursor - 1]) == b->text[c.cursor] &&
                        strchr("([{", b->text[c.cursor - 1]);
            if (pair) {
                memcpy(owned[i] + used, "    ", 4);
                used += 4;
                result[i].cursor = used;
                owned[i][used++] = '\n';
                memcpy(owned[i] + used, b->text + start, n);
                used += n;
            } else
                result[i].cursor = used;
            owned[i][used] = 0;
            edits[i].text = owned[i];
            continue;
        } else if (op == MULTI_INDENT) {
            size_t n = 4 - buf_column(b, c.cursor) % 4;
            owned[i] = copy("    ", n);
            if (!owned[i])
                goto done;
            edits[i].text = owned[i];
        }
        result[i].cursor = strlen(edits[i].text);
    }
    if (op == MULTI_BACKSPACE || op == MULTI_DELETE) {
        ok = buf_apply_edits(b, edits, (size_t)count);
        if (ok)
            clear_anchors(b);
    } else {
        for (int i = 0; i < count; i++) {
            size_t p = edit_start(edits, count, i);
            result[i].cursor += p;
            if (result[i].anchor != BUF_NO_SELECTION)
                result[i].anchor += p;
        }
        ok = buf_apply_edits(b, edits, (size_t)count);
        if (ok)
            buf_set_carets(b, result, count);
    }
done:
    for (int i = 0; i < count; i++)
        free(owned[i]);
    return ok;
}
bool buf_paste(Buffer *b, const char *text) {
    if (!b->extra_count)
        return buf_insert(b, text);
    size_t n = strlen(text);
    int lines = 1, count = buf_caret_count(b);
    for (size_t i = 0; i < n; i++)
        if (text[i] == '\n')
            lines++;
    if (lines != count)
        return buf_insert(b, text);
    Range sorted[BUF_MAX_CARETS];
    for (int i = 0; i < count; i++)
        sorted[i] = range(buf_caret(b, i), i == 0);
    qsort(sorted, (size_t)count, sizeof(*sorted), compare_range);
    BufEdit edits[BUF_MAX_CARETS] = {0};
    BufCaret results[BUF_MAX_CARETS];
    char *parts[BUF_MAX_CARETS] = {0};
    const char *from = text;
    bool ok = false;
    for (int i = 0; i < count; i++) {
        const char *end = strchr(from, '\n');
        if (!end)
            end = text + n;
        parts[i] = copy(from, (size_t)(end - from));
        if (!parts[i])
            goto done;
        edits[i] = (BufEdit){sorted[i].start, sorted[i].end, parts[i]};
        from = *end ? end + 1 : end;
    }
    int primary = 0;
    for (int i = 0; i < count; i++) {
        results[i] = (BufCaret){edit_start(edits, count, i) + strlen(parts[i]), BUF_NO_SELECTION};
        if (sorted[i].primary)
            primary = i;
    }
    {
        BufCaret swap = results[0];
        results[0] = results[primary];
        results[primary] = swap;
    }
    ok = buf_apply_edits(b, edits, (size_t)count);
    if (ok)
        buf_set_carets(b, results, count);
done:
    for (int i = 0; i < count; i++)
        free(parts[i]);
    return ok;
}
char *buf_copy_carets(Buffer *b, bool cut) {
    bool selected = false;
    for (int i = 0; i < buf_caret_count(b); i++) {
        Range r = range(buf_caret(b, i), false);
        if (r.start != r.end)
            selected = true;
    }
    if (selected) {
        char *text = multi_selected(b);
        if (text && cut)
            multi_edit(b, MULTI_INSERT, "", 0);
        return text;
    }
    Rows rows[BUF_MAX_CARETS];
    int count = selected_rows(b, rows);
    size_t length = 1;
    for (int i = 0; i < count; i++)
        length += buf_line_end(b, rows[i].last) - b->lines[rows[i].first] + 1;
    char *out = malloc(length);
    if (!out)
        return NULL;
    BufEdit edits[BUF_MAX_CARETS] = {0};
    size_t used = 0;
    for (int i = 0; i < count; i++) {
        size_t a = b->lines[rows[i].first], z = buf_line_end(b, rows[i].last);
        memcpy(out + used, b->text + a, z - a);
        used += z - a;
        out[used++] = '\n';
        if (z < b->len)
            z++;
        else if (a)
            a--;
        edits[i] = (BufEdit){a, z, ""};
    }
    out[used] = 0;
    if (cut && buf_apply_edits(b, edits, (size_t)count))
        clear_anchors(b);
    return out;
}
