#include "app_internal.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static bool key(App *a, int code) {
    return app_key_pressed(a, code);
}
static bool insert(App *a, Buffer *b, const char *text, size_t limit) {
    size_t start, end, removed = buf_selection(b, &start, &end) ? end - start : 0;
    size_t n = strlen(text);
    if (n > limit || b->len - removed > limit - n || !buf_insert(b, text)) {
        snprintf(a->snippet_error, sizeof(a->snippet_error),
                 "Saisie refusée : UTF-8 requis, déclencheur 63 octets, titre 95 octets, code 16 Ko.");
        return false;
    }
    a->snippet_error[0] = 0;
    return true;
}
static void ensure_visible(App *a) {
    Buffer *b = &a->snippet_body;
    float height = a->snippet_code_rect.height > 40 ? a->snippet_code_rect.height - 30 : 220;
    float width = a->snippet_code_rect.width > 40 ? a->snippet_code_rect.width - 30 : 750;
    float y = (float)buf_row(b, b->cursor) * 25;
    float x = (float)buf_column(b, b->cursor) * ui_width(&a->ui, "M", 17, true);
    if (y < b->scroll_y)
        b->scroll_y = y;
    if (y > b->scroll_y + height)
        b->scroll_y = y - height;
    if (x < b->scroll_x)
        b->scroll_x = x;
    if (x > b->scroll_x + width)
        b->scroll_x = x - width;
}
void app_snippet_keys(App *a, const unsigned *chars, int count, bool ctrl, bool shift) {
    if (ctrl && key(a, KEY_S)) {
        app_action(a, A_SNIPPET_SAVE, 0);
        return;
    }
    if (key(a, KEY_TAB) && (ctrl || a->snippet_focus != 2)) {
        a->snippet_focus = (a->snippet_focus + (shift ? 2 : 1)) % 3;
        return;
    }
    Buffer *b = a->snippet_focus == 0   ? &a->snippet_trigger
                : a->snippet_focus == 1 ? &a->snippet_title
                                        : &a->snippet_body;
    bool body = a->snippet_focus == 2;
    size_t limit = body                    ? SNIPPET_BODY_MAX
                   : a->snippet_focus == 0 ? SNIPPET_TRIGGER - 1u
                                           : SNIPPET_TITLE - 1u;
    if (ctrl) {
        if (key(a, KEY_A))
            buf_select_all(b);
        else if (key(a, KEY_C) || key(a, KEY_X)) {
            char *text = buf_selected(b);
            if (text) {
                SetClipboardText(text);
                free(text);
                if (key(a, KEY_X))
                    buf_insert(b, "");
            }
        } else if (key(a, KEY_V)) {
            const char *clip = GetClipboardText();
            if (clip) {
                size_t n = strlen(clip);
                if (n > 2u * SNIPPET_BODY_MAX) {
                    snprintf(a->snippet_error, sizeof(a->snippet_error),
                             "Presse-papiers trop volumineux pour un extrait (16 Ko maximum).");
                } else {
                    char *text = malloc(n + 1);
                    if (text) {
                        size_t used = 0;
                        for (size_t i = 0; i < n; i++) {
                            char c = clip[i];
                            if (c == '\r') {
                                if (clip[i + 1] == '\n')
                                    i++;
                                c = '\n';
                            }
                            text[used++] = !body && (c == '\n' || c == '\t') ? ' ' : c;
                        }
                        text[used] = 0;
                        insert(a, b, text, limit);
                        free(text);
                    }
                }
            }
        } else if (key(a, KEY_Z)) {
            if (shift)
                buf_redo(b);
            else
                buf_undo(b);
        } else if (key(a, KEY_Y))
            buf_redo(b);
    } else {
        for (int i = 0; i < count; i++) {
            char text[5];
            if (utf8_encode(chars[i], text))
                insert(a, b, text, limit);
        }
        if (body && (key(a, KEY_ENTER) || key(a, KEY_KP_ENTER))) {
            size_t row = buf_row(b, b->cursor), first = b->lines[row], n = 0;
            while (first + n < b->cursor && (b->text[first + n] == ' ' || b->text[first + n] == '\t'))
                n++;
            char *line = malloc(n + 2);
            if (line) {
                line[0] = '\n';
                memcpy(line + 1, b->text + first, n);
                line[n + 1] = 0;
                insert(a, b, line, limit);
                free(line);
            }
        }
        if (body && key(a, KEY_TAB)) {
            buf_index(b);
            size_t aa, zz;
            size_t lines = buf_selection(b, &aa, &zz) ? buf_row(b, zz) - buf_row(b, aa) + 1 : 1;
            if (shift || b->len + lines * 4u <= limit)
                buf_indent(b, shift);
            else
                snprintf(a->snippet_error, sizeof(a->snippet_error),
                         "Indentation refusée : le code dépasserait 16 Ko.");
        }
    }
    if (key(a, KEY_BACKSPACE))
        buf_backspace(b, ctrl);
    if (key(a, KEY_DELETE))
        buf_delete(b, ctrl);
    if (key(a, KEY_LEFT))
        buf_horizontal(b, -1, ctrl, shift);
    if (key(a, KEY_RIGHT))
        buf_horizontal(b, 1, ctrl, shift);
    if (body && key(a, KEY_UP))
        buf_vertical(b, -1, shift);
    if (body && key(a, KEY_DOWN))
        buf_vertical(b, 1, shift);
    if (key(a, KEY_HOME))
        buf_home_end(b, false, !body || ctrl, shift);
    if (key(a, KEY_END))
        buf_home_end(b, true, !body || ctrl, shift);
    if (body && (key(a, KEY_PAGE_UP) || key(a, KEY_PAGE_DOWN)))
        buf_vertical(b, key(a, KEY_PAGE_UP) ? -8 : 8, shift);
    if (body)
        ensure_visible(a);
}
static size_t position(App *a, Vector2 mouse) {
    Buffer *b = &a->snippet_body;
    Rectangle r = a->snippet_code_rect;
    int row = (int)floorf((mouse.y - r.y - 8 + b->scroll_y) / 25);
    int col = (int)floorf((mouse.x - r.x - 10 + b->scroll_x) / ui_width(&a->ui, "M", 17, true) + 0.5f);
    return buf_position(b, (size_t)(row < 0 ? 0 : row), (size_t)(col < 0 ? 0 : col));
}
bool app_snippet_mouse(App *a, float wheel, bool shift) {
    if (a->modal == M_SNIPPETS && wheel) {
        a->snippet_scroll -= (int)(wheel * 3);
        int max = a->snippets.count > 8 ? a->snippets.count - 8 : 0;
        if (a->snippet_scroll < 0)
            a->snippet_scroll = 0;
        if (a->snippet_scroll > max)
            a->snippet_scroll = max;
        return true;
    }
    if (a->modal != M_SNIPPET_EDIT)
        return false;
    Buffer *b = &a->snippet_body;
    Rectangle r = a->snippet_code_rect;
    Vector2 mouse = GetMousePosition();
    if (wheel && ui_inside(r, mouse)) {
        if (shift)
            b->scroll_x = fmaxf(0, b->scroll_x - wheel * 45);
        else {
            buf_index(b);
            b->scroll_y = fmaxf(
                0, fminf(fmaxf(0, (float)b->line_count * 25 - r.height + 20), b->scroll_y - wheel * 75));
        }
        return true;
    }
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && ui_inside(r, mouse)) {
        a->snippet_focus = 2;
        buf_move(b, position(a, mouse), shift);
        if (!shift)
            b->anchor = b->cursor;
        a->snippet_dragging = true;
        return true;
    }
    if (a->snippet_dragging) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            if (mouse.y < r.y)
                b->scroll_y = fmaxf(0, b->scroll_y - 15);
            else if (mouse.y > r.y + r.height)
                b->scroll_y += 15;
            b->cursor = position(a, mouse);
        } else
            a->snippet_dragging = false;
        return true;
    }
    return false;
}
