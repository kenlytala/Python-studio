#include "xml_edit.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef enum { TEXT, OPEN, CLOSE, SPECIAL, INVALID, MATCHED } State;
typedef struct {
    char name[XML_EDIT_NAME];
    size_t end;
} OpenTag;
typedef struct {
    State state;
    OpenTag stack[XML_EDIT_DEPTH];
    char names[XML_EDIT_NAMES][XML_EDIT_NAME], name[XML_EDIT_NAME], quote;
    int depth, count;
    size_t start, name_start, name_end;
} Context;
static const char *common[] = {"odoo",   "data",   "record",    "field",  "menuitem", "template", "t",
                               "form",   "list",   "tree",      "search", "group",    "sheet",    "notebook",
                               "page",   "xpath",  "button",    "header", "footer",   "label",    "separator",
                               "filter", "kanban", "templates", "div",    "span",     "p",        "a",
                               "ul",     "li",     "table",     "tr"};
static bool space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}
static bool starts(const char *s, size_t limit, size_t p, const char *literal) {
    size_t n = strlen(literal);
    return p <= limit && n <= limit - p && !memcmp(s + p, literal, n);
}
static size_t find_seq(const char *s, size_t limit, size_t p, const char *literal) {
    size_t n = strlen(literal);
    while (p <= limit && n <= limit - p) {
        const char *hit = memchr(s + p, literal[0], limit - p - n + 1);
        if (!hit)
            break;
        p = (size_t)(hit - s);
        if (!memcmp(hit, literal, n))
            return p;
        p++;
    }
    return SIZE_MAX;
}
static unsigned point(const char *s, size_t p, size_t end) {
    unsigned c = (unsigned char)s[p];
    if (c < 128)
        return c;
    unsigned value = c & (end - p == 2 ? 31u : end - p == 3 ? 15u : 7u);
    for (size_t i = p + 1; i < end; i++)
        value = (value << 6) | ((unsigned char)s[i] & 63u);
    return value;
}
static bool name_start(unsigned c) {
    return c == '_' || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= 0xc0 && c <= 0xd6) ||
           (c >= 0xd8 && c <= 0xf6) || (c >= 0xf8 && c <= 0x2ff) || (c >= 0x370 && c <= 0x37d) ||
           (c >= 0x37f && c <= 0x1fff) || (c >= 0x200c && c <= 0x200d) || (c >= 0x2070 && c <= 0x218f) ||
           (c >= 0x2c00 && c <= 0x2fef) || (c >= 0x3001 && c <= 0xd7ff) || (c >= 0xf900 && c <= 0xfdcf) ||
           (c >= 0xfdf0 && c <= 0xfffd) || (c >= 0x10000 && c <= 0xeffff);
}
static bool name_char(unsigned c) {
    return name_start(c) || c == ':' || c == '-' || c == '.' || (c >= '0' && c <= '9') || c == 0xb7 ||
           (c >= 0x300 && c <= 0x36f) || (c >= 0x203f && c <= 0x2040);
}
static bool valid_name(const char *s) {
    size_t n = strlen(s);
    if (!n || n >= XML_EDIT_NAME || !utf8_valid(s, n))
        return false;
    bool first = true, colon = false;
    for (size_t p = 0; p < n;) {
        size_t end = utf8_next(s, n, p);
        unsigned c = point(s, p, end);
        if (c == ':') {
            if (colon || first || end == n)
                return false;
            colon = true;
            first = true;
        } else {
            if (!(first ? name_start(c) : name_char(c)))
                return false;
            first = false;
        }
        p = end;
    }
    return !first;
}
bool xml_document_name(const char *name) {
    if (!name)
        return false;
    size_t n = strlen(name);
    if (n < 4 || name[n - 4] != '.')
        return false;
    return (name[n - 3] == 'x' || name[n - 3] == 'X') && (name[n - 2] == 'm' || name[n - 2] == 'M') &&
           (name[n - 1] == 'l' || name[n - 1] == 'L');
}
bool xml_edit_enabled(const Buffer *b) {
    return b && !b->xml_auto_disabled && xml_document_name(b->name);
}
/* SIZE_MAX: incomplete; SIZE_MAX-1: ambiguous/malformed. */
static size_t tag_end(const char *s, size_t p, size_t limit, char *quote) {
    *quote = 0;
    while (p < limit) {
        char c = s[p];
        if (*quote) {
            const char *end = memchr(s + p, *quote, limit - p);
            if (!end)
                return SIZE_MAX;
            p = (size_t)(end - s) + 1;
            *quote = 0;
            continue;
        }
        if (c == '\"' || c == '\'')
            *quote = c;
        else if (c == '>')
            return p;
        else if (c == '<')
            return SIZE_MAX - 1;
        p++;
    }
    return SIZE_MAX;
}
static bool scan_from(Context *ctx, const char *s, size_t len, size_t cursor, size_t p, const char *seed) {
    memset(ctx, 0, sizeof(*ctx));
    if (seed) {
        strcpy(ctx->stack[0].name, seed);
        ctx->stack[0].end = p;
        ctx->depth = 1;
    }
    if (!s || cursor > len || len > BUF_LIMIT || (cursor < len && ((unsigned char)s[cursor] & 0xc0) == 0x80))
        goto invalid;
    unsigned events = 0;
    while (p < cursor) {
        const char *hit = memchr(s + p, '<', cursor - p);
        if (!hit)
            break;
        if (++events > XML_EDIT_EVENTS)
            goto invalid;
        size_t begin = (size_t)(hit - s), from = 0;
        const char *ending = NULL;
        if (starts(s, cursor, begin, "<!--")) {
            from = begin + 4;
            ending = "-->";
        } else if (starts(s, cursor, begin, "<![CDATA[")) {
            from = begin + 9;
            ending = "]]>";
        } else if (starts(s, cursor, begin, "<?")) {
            from = begin + 2;
            ending = "?>";
        }
        if (ending) {
            size_t end = find_seq(s, cursor, from, ending);
            if (end == SIZE_MAX) {
                ctx->state = SPECIAL;
                return true;
            }
            p = end + strlen(ending);
            continue;
        }
        if (begin + 1 < cursor && s[begin + 1] == '!') {
            unsigned brackets = 0;
            char quote = 0;
            p = begin + 2;
            while (p < cursor) {
                char c = s[p];
                if (quote) {
                    if (c == quote)
                        quote = 0;
                } else if (c == '\"' || c == '\'')
                    quote = c;
                else if (c == '[')
                    brackets++;
                else if (c == ']' && brackets)
                    brackets--;
                else if (c == '>' && !brackets)
                    break;
                p++;
            }
            if (p == cursor) {
                ctx->state = SPECIAL;
                return true;
            }
            p++;
            continue;
        }
        size_t ns = begin + 1;
        bool closing = ns < cursor && s[ns] == '/';
        if (closing)
            ns++;
        size_t ne = ns;
        while (ne < cursor) {
            size_t next = utf8_next(s, cursor, ne);
            if (!name_char(point(s, ne, next)))
                break;
            ne = next;
            if (ne - ns >= XML_EDIT_NAME)
                goto invalid;
        }
        ctx->start = begin;
        ctx->name_start = ns;
        ctx->name_end = ne;
        memcpy(ctx->name, s + ns, ne - ns);
        ctx->name[ne - ns] = 0;
        ctx->state = closing ? CLOSE : OPEN;
        if (ne == cursor)
            return true;
        if (!valid_name(ctx->name) || !(space(s[ne]) || s[ne] == '/' || s[ne] == '>'))
            goto invalid;
        size_t end = tag_end(s, ne, cursor, &ctx->quote);
        if (end == SIZE_MAX)
            return true;
        if (end == SIZE_MAX - 1)
            goto invalid;
        if (closing) {
            for (size_t i = ne; i < end; i++)
                if (!space(s[i]))
                    goto invalid;
            if (!ctx->depth || strcmp(ctx->stack[ctx->depth - 1].name, ctx->name))
                goto invalid;
            ctx->depth--;
            if (seed && !ctx->depth) {
                ctx->state = MATCHED;
                return true;
            }
        } else {
            bool duplicate = false;
            for (int i = 0; i < ctx->count; i++)
                if (!strcmp(ctx->names[i], ctx->name))
                    duplicate = true;
            if (!duplicate && ctx->count < XML_EDIT_NAMES)
                strcpy(ctx->names[ctx->count++], ctx->name);
            size_t last = end;
            while (last > ne && space(s[last - 1]))
                last--;
            if (last == ne || s[last - 1] != '/') {
                if (ctx->depth == XML_EDIT_DEPTH)
                    goto invalid;
                strcpy(ctx->stack[ctx->depth].name, ctx->name);
                ctx->stack[ctx->depth++].end = end + 1;
            }
        }
        ctx->state = TEXT;
        ctx->quote = 0;
        p = end + 1;
    }
    return true;
invalid:
    ctx->state = INVALID;
    return false;
}
static bool scan(Context *ctx, const char *s, size_t len, size_t cursor) {
    return scan_from(ctx, s, len, cursor, 0, NULL);
}
static size_t adjacent_close(const char *s, size_t len, size_t p, const char *name) {
    while (p < len && space(s[p]))
        p++;
    size_t n = strlen(name);
    if (p > len || len - p < n + 2 || s[p] != '<' || s[p + 1] != '/' || memcmp(s + p + 2, name, n))
        return SIZE_MAX;
    p += n + 2;
    while (p < len && space(s[p]))
        p++;
    return p < len && s[p] == '>' ? p + 1 : SIZE_MAX;
}
static bool has_close(const char *s, size_t len, size_t p, const char *name) {
    if (adjacent_close(s, len, p, name) != SIZE_MAX)
        return true;
    Context ctx;
    return scan_from(&ctx, s, len, len, p, name) && ctx.state == MATCHED;
}
static int plan(XmlEdit *out, size_t start, size_t end, const char *s, size_t n, size_t cursor) {
    if (n > BUF_LIMIT)
        return -1;
    out->text = malloc(n + 1);
    if (!out->text)
        return -1;
    memcpy(out->text, s, n);
    out->text[n] = 0;
    out->start = start;
    out->end = end;
    out->cursor = cursor;
    return 1;
}
int xml_edit_type(const char *s, size_t len, size_t cursor, unsigned c, XmlEdit *out) {
    if (c != '>' && c != '/')
        return 0;
    Context ctx;
    if (!scan(&ctx, s, len, cursor))
        return 0;
    if (c == '/' && ctx.state == OPEN && ctx.name_start == cursor && !*ctx.name && ctx.depth) {
        const char *name = ctx.stack[ctx.depth - 1].name;
        size_t existing = adjacent_close(s, len, cursor, name);
        if (existing != SIZE_MAX)
            return plan(out, ctx.start, cursor, "", 0, existing - (cursor - ctx.start));
        char value[XML_EDIT_NAME + 3];
        int n = snprintf(value, sizeof(value), "/%s>", name);
        return plan(out, cursor, cursor, value, (size_t)n, cursor + (size_t)n);
    }
    if (c != '>' || (ctx.state != OPEN && ctx.state != CLOSE) || ctx.quote || !valid_name(ctx.name))
        return 0;
    if (cursor < len && s[cursor] == '>')
        return plan(out, cursor, cursor, "", 0, cursor + 1);
    size_t last = cursor;
    while (last > ctx.name_end && space(s[last - 1]))
        last--;
    if (ctx.state == CLOSE || (last > ctx.name_end && s[last - 1] == '/'))
        return 0;
    char value[XML_EDIT_NAME + 5] = ">";
    if (!has_close(s, len, cursor, ctx.name))
        snprintf(value, sizeof(value), "></%s>", ctx.name);
    return plan(out, cursor, cursor, value, strlen(value), cursor + 1);
}
int xml_edit_newline(const char *s, size_t len, size_t cursor, XmlEdit *out) {
    if (!s || cursor > len)
        return 0;
    size_t begin = cursor;
    while (begin && s[begin - 1] != '\n')
        begin--;
    size_t end = begin;
    while (end < cursor && (s[end] == ' ' || s[end] == '\t'))
        end++;
    size_t n = end - begin;
    if (n > (BUF_LIMIT - 8u) / 2u)
        return -1;
    char *value = malloc(2 * n + 8);
    if (!value)
        return -1;
    value[0] = '\n';
    memcpy(value + 1, s + begin, n);
    size_t used = n + 1, caret = used;
    Context ctx;
    if (scan(&ctx, s, len, cursor) && ctx.state == TEXT && ctx.depth &&
        ctx.stack[ctx.depth - 1].end == cursor) {
        bool tabs = memchr(s + begin, '\t', n) != NULL;
        memcpy(value + used, tabs ? "\t" : "    ", tabs ? 1u : 4u);
        used += tabs ? 1u : 4u;
        caret = used;
        if (adjacent_close(s, len, cursor, ctx.stack[ctx.depth - 1].name) != SIZE_MAX) {
            value[used++] = '\n';
            memcpy(value + used, s + begin, n);
            used += n;
        }
    }
    int status = plan(out, cursor, cursor, value, used, cursor + caret);
    free(value);
    return status;
}
int xml_tag_completions(const char *s, size_t len, size_t cursor, CompletionItem *out, int capacity) {
    Context ctx;
    if (capacity < 0 || !scan(&ctx, s, len, cursor) || (ctx.state != OPEN && ctx.state != CLOSE) ||
        ctx.quote || ctx.name_end != cursor)
        return -1;
    size_t end = cursor;
    while (end < len) {
        size_t next = utf8_next(s, len, end);
        if (!name_char(point(s, end, next)))
            break;
        end = next;
    }
    const char *names[XML_EDIT_NAMES + sizeof(common) / sizeof(*common) + 1];
    int total = 0;
    if (ctx.state == CLOSE) {
        if (ctx.depth)
            names[total++] = ctx.stack[ctx.depth - 1].name;
    } else {
        for (int i = 0; i < ctx.count; i++)
            names[total++] = ctx.names[i];
        for (size_t i = 0; i < sizeof(common) / sizeof(*common); i++)
            names[total++] = common[i];
        if (valid_name(ctx.name))
            names[total++] = ctx.name;
    }
    int count = 0;
    size_t prefix = strlen(ctx.name);
    for (int i = 0; i < total && count < capacity; i++) {
        const char *name = names[i];
        size_t n = strlen(name);
        if (n >= sizeof(out[0].name) || n < prefix || memcmp(name, ctx.name, prefix))
            continue;
        bool duplicate = false;
        for (int j = 0; j < count; j++)
            if (!strcmp(out[j].name, name))
                duplicate = true;
        if (duplicate)
            continue;
        CompletionItem *item = &out[count++];
        memset(item, 0, sizeof(*item));
        strcpy(item->name, name);
        item->kind = ctx.state == CLOSE ? 'T' : 'X';
        item->snippet = -1;
        item->start = ctx.name_start;
        item->end = end;
        snprintf(item->detail, sizeof(item->detail), "%s",
                 ctx.state == CLOSE ? "XML · fermer la balise" : "XML · créer la balise et sa fermeture");
    }
    return count;
}
int xml_edit_completion(const char *s, size_t len, size_t cursor, const char *name, XmlEdit *out) {
    Context ctx;
    if (!valid_name(name) || !scan(&ctx, s, len, cursor) || (ctx.state != OPEN && ctx.state != CLOSE) ||
        ctx.quote || ctx.name_end != cursor)
        return 0;
    size_t end = cursor;
    while (end < len) {
        size_t next = utf8_next(s, len, end);
        if (!name_char(point(s, end, next)))
            break;
        end = next;
    }
    char quote;
    size_t after = tag_end(s, end, len, &quote), n = strlen(name), used = n;
    size_t suffix = after < SIZE_MAX - 1 ? after + 1 - end : 0;
    if (suffix > BUF_LIMIT - n - n - 5u)
        return -1;
    char *value = malloc(n * 2 + suffix + 6);
    if (!value)
        return -1;
    memcpy(value, name, n);
    size_t caret;
    if (ctx.state == CLOSE) {
        value[used++] = '>';
        if (after < SIZE_MAX - 1) {
            bool whitespace = true;
            for (size_t i = end; i < after; i++)
                if (!space(s[i]))
                    whitespace = false;
            if (whitespace)
                end = after + 1;
        }
        caret = ctx.name_start + used;
    } else if (after < SIZE_MAX - 1) {
        memcpy(value + used, s + end, suffix);
        used += suffix;
        end = after + 1;
        caret = ctx.name_start + used;
        bool self_closed = used >= 2 && value[used - 2] == '/' && value[used - 1] == '>';
        if (!self_closed && !has_close(s, len, end, name)) {
            memcpy(value + used, "</", 2);
            used += 2;
            memcpy(value + used, name, n);
            used += n;
            value[used++] = '>';
        }
    } else if (quote || (end < len && space(s[end]))) {
        caret = ctx.name_start + used;
    } else {
        memcpy(value + used, "></", 3);
        used += 3;
        memcpy(value + used, name, n);
        used += n;
        value[used++] = '>';
        caret = ctx.name_start + n + 1;
    }
    int status = plan(out, ctx.name_start, end, value, used, caret);
    free(value);
    return status;
}
bool xml_edit_apply(Buffer *b, const XmlEdit *edit) {
    if (!b || !edit || !edit->text || edit->start > edit->end || edit->end > b->len)
        return false;
    size_t n = strlen(edit->text), removed = edit->end - edit->start;
    if (n > BUF_LIMIT || b->len - removed > BUF_LIMIT - n || !utf8_valid(edit->text, n) ||
        (edit->start < b->len && ((unsigned char)b->text[edit->start] & 0xc0) == 0x80) ||
        (edit->end < b->len && ((unsigned char)b->text[edit->end] & 0xc0) == 0x80) ||
        edit->cursor > b->len - removed + n)
        return false;
    const char *check;
    size_t position;
    if (edit->cursor < edit->start) {
        check = b->text;
        position = edit->cursor;
    } else if (edit->cursor < edit->start + n) {
        check = edit->text;
        position = edit->cursor - edit->start;
    } else {
        check = b->text;
        position = edit->cursor - n + removed;
    }
    if (((unsigned char)check[position] & 0xc0) == 0x80)
        return false;
    bool same = n == edit->end - edit->start && !memcmp(b->text + edit->start, edit->text, n);
    if (!same && !buf_replace(b, edit->start, edit->end, edit->text))
        return false;
    if (edit->cursor > b->len ||
        (edit->cursor < b->len && ((unsigned char)b->text[edit->cursor] & 0xc0) == 0x80))
        return false;
    b->cursor = edit->cursor;
    b->anchor = BUF_NO_SELECTION;
    return true;
}
