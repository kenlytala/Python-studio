#include "app_internal.h"

static bool held(App *a, int key) {
    return app_key_held(a, key);
}
static bool pressed(App *a, int key) {
    return app_key_pressed(a, key);
}
void app_multicursor_reset(App *a) {
    a->vertical_document = NULL;
}
bool app_multicursor_keys(App *a, bool ctrl) {
    /* The frame's ctrl flag deliberately excludes AltGr for text entry. For
       this arrow-only chord, inspect actual Control keys separately so an
       explicit Ctrl + right Alt works without changing AltGr character input. */
    bool control = ctrl || held(a, KEY_LEFT_CONTROL) || held(a, KEY_RIGHT_CONTROL);
    bool alt = held(a, KEY_LEFT_ALT) || held(a, KEY_RIGHT_ALT);
    bool shift = held(a, KEY_LEFT_SHIFT) || held(a, KEY_RIGHT_SHIFT);
    if (!control || (!alt && !shift) || a->modal || a->focus || a->active < 0 || a->active >= a->tab_count ||
        !a->tabs[a->active]) {
        app_multicursor_reset(a);
        return false;
    }
    /* GNOME commonly reserves Ctrl+Alt+arrows: the no-Alt fallback does not
       steal ordinary Ctrl+J (console) or Ctrl+K (palette). */
    bool up = alt ? pressed(a, KEY_UP) : pressed(a, KEY_K);
    bool down = alt ? pressed(a, KEY_DOWN) : pressed(a, KEY_J);
    if (!up && !down)
        return false;
    Buffer *b = app_buffer(a);
    if (a->vertical_document != b || a->vertical_version != b->version || a->vertical_cursor != b->cursor ||
        a->vertical_count != buf_caret_count(b) || a->vertical_pane != a->active_pane)
        a->vertical_column = buf_column(b, b->cursor);
    bool added = buf_add_vertical_caret_at_column(b, up ? -1 : 1, a->vertical_column);
    a->vertical_document = b;
    a->vertical_version = b->version;
    a->vertical_cursor = b->cursor;
    a->vertical_count = buf_caret_count(b);
    a->vertical_pane = a->active_pane;
    app_completion_dismiss(a);
    if (!added && buf_caret_count(b) == BUF_MAX_CARETS)
        app_toast(a, "Limite atteinte : 256 curseurs par document.", false);
    a->caret_time = GetTime();
    app_ensure_visible(a);
    /* Own the chord even at the first/last line: never turn it into an
       ordinary arrow movement of every existing caret at a document edge. */
    return true;
}
