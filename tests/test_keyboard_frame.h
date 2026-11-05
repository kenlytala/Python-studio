/* Actual production update -> editor_keys -> buffer history. OS only is fake. */
#include "test_keyboard_layouts.h"
static void keyboard_frame_input(App *a, int raw, int channel, int modifier, bool shift, bool altgr,
                                 unsigned ch) {
    keyboard_clear(a);
    frame_queued_key = 0;
    frame_queued_char = (int)ch;
    if (modifier)
        key_down[modifier] = true;
    if (shift)
        key_down[KEY_LEFT_SHIFT] = true;
    if (altgr)
        key_down[KEY_RIGHT_ALT] = true;
    if (raw) {
        if (channel == 0 || channel == 3)
            frame_queued_key = raw;
        if (channel == 1 || channel == 3)
            key_press[raw] = true;
        if (channel == 2)
            key_repeat[raw] = true;
    }
    update(a, .016f);
    frame_queued_key = frame_queued_char = 0;
    keyboard_clear(a);
}
static void keyboard_frame_reset(Buffer *b, const char *text, const char *name) {
    buf_free(b);
    CHECK(buf_init(b, text, name));
}
static void keyboard_frame_fixture(void) {
    int start = checks;
    App *a = calloc(1, sizeof(*a));
    CHECK(a);
    Buffer b, other;
    CHECK(buf_init(&b, "abc", "keyboard.py") && buf_init(&other, "other", "other.py"));
    CHECK(buf_init(&a->field, "champ", "champ") && buf_init(&a->search, "", "recherche") &&
          buf_init(&a->input, "", "console"));
    a->tabs[0] = &b;
    a->tabs[1] = &other;
    a->tab_count = 2;
    a->running = true;
    a->xml_auto = true;
    a->font_size = 18;
    a->line_h = 20;
    a->char_w = 10;
    a->code_h = 500;
    a->code_y = 72;
    a->code_bottom = 572;
    a->code_x = 240;
    a->code_right = 1240;
    a->panes[0].tab = 0;
    snprintf(a->project, sizeof(a->project), "/keyboard-cpu-no-project");
    frame(a, 1280, 720);
    frame_pointer = (Vector2){900, 500};
    frame_wheel = 0;
    down = mouse_pressed = released = false;
    frame_keyboard_available = true;
    tab_close_actions = problems_actions = 0;
    const int undo[] = {KEY_Z, KEY_W, KEY_Y, KEY_SLASH}, redo[] = {KEY_Y, KEY_Y, KEY_Z, KEY_T},
              close[] = {KEY_W, KEY_Z, KEY_W, KEY_COMMA};
    for (int layout = 0; layout < 4; layout++)
        for (int channel = 0; channel < 4; channel++) {
            keyboard_test_layout(frame_keyboard_symbols, layout);
            memset(&a->keyboard, 0, sizeof(a->keyboard));
            keyboard_frame_reset(&b, "abc", "keyboard.py");
            CHECK(buf_insert(&b, "older"));
            CHECK(buf_insert(&b, "X"));
            free(b.saved);
            b.saved = strdup(b.text);
            CHECK(b.saved);
            int history = b.undo.count;
            size_t cursor = b.cursor;
            char after[64];
            snprintf(after, sizeof(after), "%s", b.text);
            int closed = tab_close_actions;
            keyboard_frame_input(a, undo[layout], channel, KEY_LEFT_CONTROL, false, false, 0);
            CHECK(tab_close_actions == closed && a->tab_count == 2 && app_buffer(a) == &b &&
                  a->modal == M_NONE);
            CHECK(!strcmp(b.text, "olderabc") && b.undo.count == history - 1 && b.redo.count == 1);
            keyboard_frame_input(a, undo[layout], channel, KEY_LEFT_CONTROL, true, false, 0);
            CHECK(!strcmp(b.text, after) && b.cursor == cursor && b.undo.count == history && !b.redo.count);
            keyboard_frame_input(a, undo[layout], channel, KEY_LEFT_CONTROL, false, false, 0);
            CHECK(!strcmp(b.text, "olderabc"));
            keyboard_frame_input(a, redo[layout], channel, KEY_LEFT_CONTROL, false, false, 0);
            CHECK(!strcmp(b.text, after) && b.cursor == cursor && b.undo.count == history && !b.redo.count &&
                  tab_close_actions == closed);
            keyboard_frame_input(a, close[layout], channel, KEY_LEFT_CONTROL, false, false, 0);
            CHECK(tab_close_actions == closed + 1 && !strcmp(b.text, after) && b.undo.count == history);
        }
    /* Layout changes even while Ctrl stays held; missing maps retain last good. */
    keyboard_test_layout(frame_keyboard_symbols, 1);
    memset(&a->keyboard, 0, sizeof(a->keyboard));
    keyboard_frame_reset(&b, "abc", "keyboard.py");
    CHECK(buf_insert(&b, "X"));
    int closed = tab_close_actions;
    keyboard_frame_input(a, KEY_W, 0, KEY_LEFT_CONTROL, false, false, 0);
    CHECK(!strcmp(b.text, "abc"));
    keyboard_test_layout(frame_keyboard_symbols, 2);
    keyboard_frame_input(a, KEY_Z, 0, KEY_LEFT_CONTROL, false, false, 0);
    CHECK(!strcmp(b.text, "Xabc"));
    keyboard_frame_input(a, KEY_Y, 0, KEY_LEFT_CONTROL, false, false, 0);
    CHECK(!strcmp(b.text, "abc"));
    keyboard_test_layout(frame_keyboard_symbols, 1);
    keyboard_frame_input(a, KEY_Y, 0, KEY_LEFT_CONTROL, false, false, 0);
    CHECK(!strcmp(b.text, "Xabc"));
    frame_keyboard_available = false;
    keyboard_frame_input(a, KEY_W, 0, KEY_LEFT_CONTROL, false, false, 0);
    CHECK(!strcmp(b.text, "abc") && tab_close_actions == closed);
    keyboard_frame_input(a, KEY_Y, 0, KEY_LEFT_CONTROL, false, false, 0);
    CHECK(!strcmp(b.text, "Xabc"));
    /* Periodic refresh also covers native-only repeated-state channels. */
    frame_keyboard_available = true;
    keyboard_test_layout(frame_keyboard_symbols, 2);
    clock_value += .8;
    keyboard_frame_input(a, KEY_Y, 2, KEY_LEFT_CONTROL, false, false, 0);
    CHECK(!strcmp(b.text, "abc"));
    /* Missing initial OS provider retains identity; no accidental stale swap. */
    memset(&a->keyboard, 0, sizeof(a->keyboard));
    frame_keyboard_available = false;
    keyboard_frame_input(a, KEY_Y, 0, KEY_LEFT_CONTROL, false, false, 0);
    CHECK(!strcmp(b.text, "Xabc"));
    keyboard_frame_input(a, KEY_Z, 0, KEY_LEFT_CONTROL, false, false, 0);
    CHECK(!strcmp(b.text, "abc"));
    frame_keyboard_available = true;
    keyboard_test_layout(frame_keyboard_symbols, 1);
    keyboard_frame_input(a, KEY_Y, 0, KEY_LEFT_CONTROL, false, false, 0);
    CHECK(!strcmp(b.text, "Xabc"));
    keyboard_frame_input(a, KEY_W, 0, KEY_LEFT_SUPER, false, false, 0);
    CHECK(!strcmp(b.text, "abc"));
    keyboard_frame_input(a, KEY_Y, 0, KEY_RIGHT_CONTROL, false, false, 0);
    CHECK(!strcmp(b.text, "Xabc"));
    /* A, M and their moved punctuation positions; obsolete aliases cannot fire. */
    for (int channel = 0; channel < 4; channel++) {
        b.anchor = BUF_NO_SELECTION;
        b.cursor = 0;
        keyboard_frame_input(a, KEY_Q, channel, KEY_LEFT_CONTROL, false, false, 0);
        CHECK(b.anchor == 0 && b.cursor == b.len && a->running);
        int problems = problems_actions;
        keyboard_frame_input(a, KEY_SEMICOLON, channel, KEY_LEFT_CONTROL, true, false, 0);
        CHECK(problems_actions == problems + 1);
        keyboard_frame_input(a, KEY_M, channel, KEY_LEFT_CONTROL, true, false, 0);
        CHECK(problems_actions == problems + 1);
    }
    keyboard_clear(a);
    key_down[KEY_SEMICOLON] = true;
    CHECK(app_key_held(a, KEY_M) && !app_key_held(a, KEY_SEMICOLON));
    keyboard_clear(a);
    key_down[KEY_M] = true;
    CHECK(!app_key_held(a, KEY_M));
    keyboard_clear(a);
    /* Each field keeps its own history, independently from the source. */
    char source[64];
    snprintf(source, sizeof(source), "%s", b.text);
    int source_history = b.undo.count;
    for (int context = 0; context < 3; context++)
        for (int channel = 0; channel < 4; channel++) {
            Buffer *field = context == 0 ? &a->search : context == 1 ? &a->field : &a->folder_search.query;
            keyboard_frame_reset(field, "field", "champ");
            field->cursor = field->len;
            CHECK(buf_insert(field, "X"));
            a->focus = context == 0 ? 1 : 0;
            a->find_open = context == 0;
            a->modal = context == 1 ? M_GOTO : context == 2 ? M_FOLDER_SEARCH : M_NONE;
            if (context == 2) {
                a->folder_search.initialized = true;
                snprintf(a->folder_search.project, sizeof(a->folder_search.project), "%s", a->project);
                a->folder_search.focus = 1;
            }
            keyboard_frame_input(a, KEY_W, channel, KEY_LEFT_CONTROL, false, false, 0);
            CHECK(!strcmp(field->text, "field"));
            keyboard_frame_input(a, KEY_Y, channel, KEY_LEFT_CONTROL, false, false, 0);
            CHECK(!strcmp(field->text, "fieldX"));
            CHECK(!strcmp(b.text, source) && b.undo.count == source_history && tab_close_actions == closed);
        }
    a->modal = M_NONE;
    a->focus = 0;
    a->find_open = false;
    /* Simultaneous UTF-8 edit/history retains the complete multicursor set. */
    keyboard_frame_reset(&b, "ab\ncd", "keyboard.py");
    BufCaret carets[2] = {{1, BUF_NO_SELECTION}, {4, BUF_NO_SELECTION}};
    buf_set_carets(&b, carets, 2);
    keyboard_frame_input(a, 0, 0, 0, false, false, 0xe9);
    CHECK(!strcmp(b.text, "aéb\ncéd") && b.extra_count == 1);
    BufCaret after0 = buf_caret(&b, 0), after1 = buf_caret(&b, 1);
    keyboard_frame_input(a, KEY_W, 0, KEY_LEFT_CONTROL, false, false, 0);
    CHECK(!strcmp(b.text, "ab\ncd") && b.extra_count == 1);
    CHECK(b.cursor == 1 && b.extra[0].cursor == 4);
    keyboard_frame_input(a, KEY_Y, 0, KEY_LEFT_CONTROL, false, false, 0);
    CHECK(!strcmp(b.text, "aéb\ncéd") && b.extra_count == 1);
    CHECK(b.cursor == after0.cursor && b.extra[0].cursor == after1.cursor);
    /* Native text/AltGr is separate, not transliterated or interpreted as Ctrl. */
    keyboard_frame_reset(&b, "", "keyboard.py");
    const unsigned text[] = {122, 0xe9, 0x648, 0x627, 0x732b, 0x1f600};
    for (size_t i = 0; i < sizeof(text) / sizeof(*text); i++)
        keyboard_frame_input(a, KEY_W, 0, 0, false, false, text[i]);
    keyboard_frame_input(a, KEY_W, 0, KEY_LEFT_CONTROL, false, true, '@');
    keyboard_frame_input(a, KEY_W, 0, KEY_LEFT_CONTROL, false, true, 0x20ac);
    CHECK(!strcmp(b.text, "zéوا猫😀@€") && tab_close_actions == closed && a->tab_count == 2);
    /* Undo of automatic XML remains the existing single transaction. */
    keyboard_frame_reset(&b, "<form", "example.xml");
    b.cursor = b.len;
    keyboard_frame_input(a, 0, 0, 0, false, false, '>');
    CHECK(!strcmp(b.text, "<form></form>"));
    keyboard_frame_input(a, KEY_W, 0, KEY_LEFT_CONTROL, false, false, 0);
    CHECK(!strcmp(b.text, "<form"));
    keyboard_frame_input(a, KEY_Y, 0, KEY_LEFT_CONTROL, false, false, 0);
    CHECK(!strcmp(b.text, "<form></form>"));
    buf_free(&a->field);
    buf_free(&a->search);
    buf_free(&a->input);
    buf_free(&a->folder_search.query);
    buf_free(&b);
    buf_free(&other);
    free(a);
    frame_keyboard_available = false;
    frame_queued_key = frame_queued_char = 0;
    frame_editor_calls = 0;
    memset(key_down, 0, sizeof(key_down));
    memset(key_press, 0, sizeof(key_press));
    memset(key_repeat, 0, sizeof(key_repeat));
    clock_value = 10;
    printf("OK %d REAL keyboard frame checks: actual update/editor/history, 4 layouts x "
           "queue/pressed/repeat/both, no false tab-close, real W close action, A/M, live switch/fallback, "
           "fields, multicursors/XML, Unicode/AltGr; OS/services simulated\n",
           checks - start);
}
