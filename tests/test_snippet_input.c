#define _POSIX_C_SOURCE 200809L
#include "app_internal.h"
#include "test_keyboard_layouts.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif
static int checks, visible, saves;
#define CHECK(c)                                                                                             \
    do {                                                                                                     \
        checks++;                                                                                            \
        if (!(c)) {                                                                                          \
            fprintf(stderr, "FAIL %d: %s\n", __LINE__, #c);                                                  \
            exit(1);                                                                                         \
        }                                                                                                    \
    } while (0)
static char *clipboard;
static Vector2 mouse;
static bool mouse_pressed, mouse_down;
static bool native_press[512], native_repeat[512];
bool IsKeyPressed(int key) {
    return key > 0 && key < 512 && native_press[key];
}
bool IsKeyPressedRepeat(int key) {
    return key > 0 && key < 512 && native_repeat[key];
}
const char *GetClipboardText(void) {
    return clipboard ? clipboard : "";
}
void SetClipboardText(const char *text) {
    char *next = malloc(strlen(text) + 1);
    CHECK(next);
    strcpy(next, text);
    free(clipboard);
    clipboard = next;
}
Vector2 GetMousePosition(void) {
    return mouse;
}
bool IsMouseButtonPressed(int button) {
    (void)button;
    return mouse_pressed;
}
bool IsMouseButtonDown(int button) {
    (void)button;
    return mouse_down;
}
float ui_width(UI *u, const char *text, float size, bool mono) {
    (void)u;
    (void)size;
    (void)mono;
    return (float)strlen(text) * 10;
}
bool ui_inside(Rectangle r, Vector2 p) {
    return p.x >= r.x && p.x < r.x + r.width && p.y >= r.y && p.y < r.y + r.height;
}
int MakeDirectory(const char *path) {
#ifdef _WIN32
    return _mkdir(path);
#else
    return mkdir(path, 0700);
#endif
}
Buffer *app_buffer(App *a) {
    return a->tabs[a->active];
}
void app_ensure_visible(App *a) {
    (void)a;
    visible++;
}
void app_toast(App *a, const char *text, bool error) {
    snprintf(a->toast, sizeof(a->toast), "%s", text);
    a->toast_error = error;
}
void app_set_field(Buffer *b, const char *text, bool select) {
    unsigned long version = b->version;
    buf_free(b);
    CHECK(buf_init(b, text, "champ"));
    b->version = version + 1;
    b->cursor = b->len;
    if (select)
        b->anchor = 0;
}
void app_action(App *a, Action action, int arg) {
    (void)arg;
    CHECK(action == A_SNIPPET_SAVE);
    saves++;
    app_snippet_save(a);
}
static void key(App *a, int code, bool ctrl, bool shift) {
    memset(a->keys, 0, sizeof(a->keys));
    a->keys[code] = true;
    app_snippet_keys(a, NULL, 0, ctrl, shift);
    memset(a->keys, 0, sizeof(a->keys));
}
static void set_document(App *a, Buffer *b, const char *text, const char *name) {
    buf_free(b);
    CHECK(buf_init(b, text, name));
    a->tabs[0] = b;
    a->tab_count = 1;
    a->active = a->active_pane = 0;
    a->modal = M_NONE;
    a->focus = 0;
    b->cursor = b->len;
}
int main(void) {
    App *a = calloc(1, sizeof(*a));
    CHECK(a);
    completion_init(&a->completer);
    Buffer document = {0};
    char dir[BUF_PATH], profile[BUF_PATH], config[BUF_PATH];
#ifdef _WIN32
    char temp[MAX_PATH];
    CHECK(GetTempPathA(MAX_PATH, temp) && GetTempFileNameA(temp, "sui", 0, dir));
    CHECK(!remove(dir) && !_mkdir(dir));
#else
    strcpy(dir, "/tmp/prisme-snippet-ui-XXXXXX");
    CHECK(mkdtemp(dir));
#endif
    CHECK(snprintf(a->config, sizeof(a->config), "%s/settings.cfg", dir) < BUF_PATH);
    strcpy(config, a->config);
    strcpy(a->project, dir);
    app_snippets_load(a);
    CHECK(a->snippets.count == 1 && !*a->snippet_error);
    CHECK(snippets_file(profile, sizeof(profile), a->config));
    app_snippet_edit(a, 0);
    CHECK(a->modal == M_SNIPPET_EDIT && !app_snippet_dirty(a) && a->snippet_body.cursor == 0);
    a->snippet_focus = 2;
    key(a, KEY_A, true, false);
    SetClipboardText("<form>\r\n    <field name=\"été\"/>\r\n</form>");
    key(a, KEY_V, true, false);
    CHECK(!strcmp(a->snippet_body.text, "<form>\n    <field name=\"été\"/>\n</form>") &&
          app_snippet_dirty(a));
    CHECK(strstr(a->snippets.items[0].body, "ir.ui.view"));
    key(a, KEY_S, true, false);
    CHECK(saves == 1 && a->modal == M_SNIPPETS && !strcmp(a->snippets.items[0].body, a->snippet_body.text));
    app_snippet_edit(a, 0);
    a->snippet_focus = 2;
    key(a, KEY_END, true, false);
    key(a, KEY_ENTER, false, false);
    unsigned text[] = {' ', ' ', ' ', ' ', '<', 'x', '/', '>'};
    app_snippet_keys(a, text, (int)(sizeof(text) / sizeof(text[0])), false, false);
    CHECK(strstr(a->snippet_body.text, "\n    <x/>") && app_snippet_dirty(a));
    key(a, KEY_ENTER, false, false);
    CHECK(!strcmp(a->snippet_body.text + a->snippet_body.len - 5, "\n    "));
    key(a, KEY_TAB, false, false);
    CHECK(!strcmp(a->snippet_body.text + a->snippet_body.len - 9, "\n        "));
    key(a, KEY_TAB, true, false);
    CHECK(a->snippet_focus == 0);
    key(a, KEY_TAB, false, false);
    CHECK(a->snippet_focus == 1);
    key(a, KEY_TAB, false, false);
    CHECK(a->snippet_focus == 2);
    app_snippet_leave(a);
    CHECK(a->modal == M_SNIPPET_DIRTY && app_snippet_dirty(a));
    a->modal = M_SNIPPET_EDIT;
    app_set_field(&a->snippet_body, "updated", false);
    CHECK(snprintf(a->config, sizeof(a->config), "%s/missing/deeper/settings.cfg", dir) < BUF_PATH);
    char *before = a->snippets.items[0].body;
    CHECK(!app_snippet_save(a) && a->snippets.items[0].body == before && a->modal == M_SNIPPET_EDIT &&
          *a->snippet_error);
    CHECK(!strcmp(a->snippet_body.text, "updated"));
    strcpy(a->config, config);
    app_snippet_edit(a, -1);
    app_set_field(&a->snippet_trigger, "xml_custom", false);
    app_set_field(&a->snippet_title, "Extrait global personnalisé", false);
    app_set_field(&a->snippet_body, "<custom>\n    été\n</custom>", false);
    CHECK(app_snippet_save(a) && a->snippets.count == 2);
    strcpy(a->project, "/another-project");
    app_snippets_load(a);
    CHECK(a->snippets.count == 2 && !strcmp(a->snippets.items[1].trigger, "xml_custom"));
    strcpy(a->project, dir);
    /* Automatic proposals and actual Tab/Enter handler, without Python or XML execution. */
    a->xml_auto = true;
    set_document(a, &document, "<fi", "view.xml");
    app_completion_show(a, true);
    CHECK(a->completion_count >= 1 && !strcmp(a->completions[0].name, "field") &&
          a->completions[0].kind == 'X');
    CHECK(app_completion_key(a, KEY_ENTER, false) && !strcmp(document.text, "<field></field>") &&
          document.cursor == 7 && document.undo.count == 1);
    CHECK(buf_undo(&document) && !strcmp(document.text, "<fi") && buf_redo(&document) &&
          document.cursor == 7);
    set_document(a, &document, "<r><foo></f", "view.xml");
    app_completion_show(a, true);
    CHECK(a->completion_count == 1 && a->completions[0].kind == 'T' && app_completion_key(a, KEY_TAB, false));
    CHECK(!strcmp(document.text, "<r><foo></foo>") && document.cursor == document.len);
    set_document(a, &document, "<fi", "view.xml");
    a->xml_auto = false;
    app_completion_show(a, false);
    CHECK(!a->completion_count);
    app_completion_show(a, true);
    CHECK(a->completion_count >= 1);
    buf_move(&document, 1, false);
    CHECK(!app_completion_apply(a, 0) && !strcmp(document.text, "<fi"));
    a->xml_auto = true;
    set_document(a, &document, "    xml_form", "form.xml");
    unsigned long version = document.version;
    app_completion_show(a, false);
    CHECK(a->completion_count == 1 && a->completions[0].kind == 'S' && !a->completion_pending);
    CHECK(app_completion_key(a, KEY_TAB, false));
    CHECK(!strcmp(document.text, "    <form>\n        <field name=\"été\"/>\n    </form>") &&
          document.version == version + 1 && visible);
    CHECK(buf_undo(&document) && !strcmp(document.text, "    xml_form"));
    app_completion_show(a, false);
    CHECK(!app_completion_key(a, KEY_TAB, true) && !strcmp(document.text, "    xml_form"));
    CHECK(app_completion_key(a, KEY_ENTER, false) && strstr(document.text, "<form>"));
    set_document(a, &document, "xml_custom_suffix tail", "form.xml");
    buf_move(&document, strlen("xml_cu"), false);
    app_completion_show(a, false);
    CHECK(a->completion_count >= 1 && a->completions[0].kind == 'S' &&
          app_completion_key(a, KEY_KP_ENTER, false));
    CHECK(!strcmp(document.text, "<custom>\n    été\n</custom> tail"));
    set_document(a, &document, "xml_form", "form.xml");
    app_completion_show(a, false);
    CHECK(app_completion_key(a, KEY_ESCAPE, false) && !a->completion_active && !a->completion_count);
    CHECK(buf_add_caret(&document, 2, BUF_NO_SELECTION));
    app_completion_show(a, true);
    CHECK(!a->completion_count && !a->completion_active && document.extra_count == 1 &&
          strstr(a->toast, "curseur"));
    buf_clear_carets(&document);
    /* A stale worker result cannot resurrect a popup or edit a different document. */
    set_document(a, &document, "from odoo import fields\nfields.Ch", "test.py");
    CHECK(snprintf(document.path, sizeof(document.path), "%s/test.py", dir) < BUF_PATH);
    app_completion_show(a, false);
    CHECK(a->completion_pending && a->completion_active);
    CHECK(!app_completion_key(a, KEY_TAB, false));
    a->completer.version = document.version;
    a->completer.cursor = document.cursor;
    snprintf(a->completer.filename, sizeof(a->completer.filename), "%s", document.path);
    a->completer.report.count = 1;
    a->completer.report.items[0] =
        (CompletionItem){.kind = 'C', .start = a->completion_start, .end = a->completion_end, .snippet = -1};
    strcpy(a->completer.report.items[0].name, "Char");
    strcpy(a->completer.report.items[0].detail, "Classe · Odoo");
    a->completion_request_generation = a->completion_generation;
    a->completer.ready = true;
    app_completion_poll(a);
    CHECK(a->completion_count == 1 && !a->completion_pending);
    CHECK(app_completion_key(a, KEY_TAB, false) && strstr(document.text, "fields.Char"));
    set_document(a, &document, "from odoo import fields\nfields.Ch", "test.py");
    CHECK(snprintf(document.path, sizeof(document.path), "%s/test.py", dir) < BUF_PATH);
    app_completion_show(a, false);
    a->completer.version = document.version;
    a->completer.cursor = document.cursor;
    a->completer.ready = true;
    CHECK(buf_type(&document, 'x'));
    app_completion_poll(a);
    CHECK(!a->completion_count && !a->completion_active && !a->completer.ready &&
          strstr(document.text, "Chx"));
    app_completion_show(a, false);
    app_completion_dismiss(a);
    a->completer.ready = true;
    app_completion_poll(a);
    CHECK(!a->completion_count && !a->completion_active);
    /* Same metadata is not enough: a new popup generation, pane or selection owns its result. */
    set_document(a, &document, "from odoo import fields\nfields.Ch", "test.py");
    CHECK(snprintf(document.path, sizeof(document.path), "%s/test.py", dir) < BUF_PATH);
    app_completion_show(a, false);
    a->completer.version = document.version;
    a->completer.cursor = document.cursor;
    a->completion_request_generation = a->completion_generation;
    app_completion_dismiss(a);
    app_completion_show(a, false);
    a->completer.ready = true;
    app_completion_poll(a);
    CHECK(!a->completion_count && a->completion_pending && !a->completer.ready);
    a->completion_request_generation = a->completion_generation;
    a->active_pane = 1;
    a->completer.ready = true;
    app_completion_poll(a);
    CHECK(!a->completion_active && !a->completion_count);
    a->active_pane = 0;
    app_completion_show(a, false);
    document.anchor = 0;
    a->completer.ready = true;
    app_completion_poll(a);
    CHECK(!a->completion_active && !a->completion_count);
    document.anchor = BUF_NO_SELECTION;
    app_completion_show(a, false);
    a->modal = M_SETTINGS;
    a->completer.ready = true;
    app_completion_poll(a);
    CHECK(!a->completion_active && !a->completion_count);
    a->modal = M_NONE;
    app_completion_show(a, false);
    a->focus = 2;
    a->completer.ready = true;
    app_completion_poll(a);
    CHECK(!a->completion_active && !a->completion_count);
    a->focus = 0;
    /* Draft limits reject oversized pastes/indentation without changing the code. */
    app_snippet_edit(a, 0);
    a->snippet_focus = 2;
    char *maximum = malloc(SNIPPET_BODY_MAX + 1);
    CHECK(maximum);
    memset(maximum, 'x', SNIPPET_BODY_MAX);
    maximum[SNIPPET_BODY_MAX] = 0;
    app_set_field(&a->snippet_body, maximum, false);
    unsigned long body_version = a->snippet_body.version;
    SetClipboardText("é");
    key(a, KEY_V, true, false);
    CHECK(a->snippet_body.version == body_version && a->snippet_body.len == SNIPPET_BODY_MAX &&
          *a->snippet_error);
    key(a, KEY_TAB, false, false);
    CHECK(a->snippet_body.version == body_version && a->snippet_body.len == SNIPPET_BODY_MAX);
    key(a, KEY_ENTER, false, false);
    CHECK(a->snippet_body.version == body_version);
    key(a, KEY_A, true, false);
    SetClipboardText("été");
    key(a, KEY_V, true, false);
    CHECK(!strcmp(a->snippet_body.text, "été") && !*a->snippet_error);
    key(a, KEY_Z, true, false);
    CHECK(a->snippet_body.len == SNIPPET_BODY_MAX);
    key(a, KEY_Z, true, true);
    CHECK(!strcmp(a->snippet_body.text, "été"));
    free(maximum);
    /* Native multiline mouse editing/selection and scrolling use the draft, never the source document. */
    app_snippet_edit(a, 0);
    app_set_field(&a->snippet_body, "hello\nété\nlast", false);
    a->snippet_code_rect = (Rectangle){100, 100, 600, 260};
    mouse = (Vector2){130, 110};
    mouse_pressed = mouse_down = true;
    CHECK(app_snippet_mouse(a, 0, false) && a->snippet_focus == 2 && a->snippet_body.cursor == 2);
    mouse_pressed = false;
    mouse = (Vector2){140, 135};
    CHECK(app_snippet_mouse(a, 0, false));
    size_t aa, zz;
    CHECK(buf_selection(&a->snippet_body, &aa, &zz) && aa == 2 && zz > aa);
    mouse_down = false;
    CHECK(app_snippet_mouse(a, 0, false) && !a->snippet_dragging);
    key(a, KEY_C, true, false);
    CHECK(*GetClipboardText());
    CHECK(!strcmp(document.text, "from odoo import fields\nfields.Ch"));
    /* Actual snippet input's native pressed/repeat queries use the inverse map. */
    unsigned keyboard_symbols[KEYBOARD_KEYS];
    keyboard_test_layout(keyboard_symbols, 1);
    CHECK(keyboard_configure(&a->keyboard, keyboard_symbols));
    for (int channel = 0; channel < 2; channel++) {
        app_snippet_edit(a, 0);
        a->snippet_focus = 2;
        app_set_field(&a->snippet_body, "base", false);
        CHECK(buf_insert(&a->snippet_body, "X"));
        bool *native = channel ? native_repeat : native_press;
        native[KEY_W] = true;
        app_snippet_keys(a, NULL, 0, true, false);
        native[KEY_W] = false;
        CHECK(!strcmp(a->snippet_body.text, "base"));
        native[KEY_Y] = true;
        app_snippet_keys(a, NULL, 0, true, false);
        native[KEY_Y] = false;
        CHECK(!strcmp(a->snippet_body.text, "baseX"));
        CHECK(!strcmp(document.text, "from odoo import fields\nfields.Ch"));
    }
    memset(&a->keyboard, 0, sizeof(a->keyboard));
    a->modal = M_SNIPPETS;
    a->snippet_index = 1;
    CHECK(app_snippet_delete(a) && a->snippets.count == 1);
    a->snippet_index = 0;
    CHECK(snprintf(a->config, sizeof(a->config), "%s/missing/deeper/settings.cfg", dir) < BUF_PATH);
    before = a->snippets.items[0].body;
    CHECK(!app_snippet_delete(a) && a->snippets.count == 1 && a->snippets.items[0].body == before);
    strcpy(a->config, config);
    a->snippet_index = 0;
    CHECK(app_snippet_delete(a) && !a->snippets.count);
    app_snippets_load(a);
    CHECK(!a->snippets.count); /* explicitly empty library is not silently reseeded */
    CHECK(!remove(profile));
#ifdef _WIN32
    CHECK(!_rmdir(dir));
#else
    CHECK(!rmdir(dir));
#endif
    buf_free(&document);
    buf_free(&a->snippet_trigger);
    buf_free(&a->snippet_title);
    buf_free(&a->snippet_body);
    snippets_free(&a->snippets);
    completion_close(&a->completer);
    free(clipboard);
    free(a);
    printf("OK %d actual controller/input checks: multiline draft/clipboard/keys/mouse, global persistence, "
           "failed-save rollback, Tab/Enter/indentation/undo, stale results, selection and document safety; "
           "no GUI\n",
           checks);
    return 0;
}
