#define _POSIX_C_SOURCE 200809L
/* Production controller functions, with native window/thread side effects mocked. */
#include "app_internal.h"
#include <math.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#include <sys/socket.h>
#endif
static int checks;
#define CHECK(c)                                                                                             \
    do {                                                                                                     \
        checks++;                                                                                            \
        if (!(c)) {                                                                                          \
            fprintf(stderr, "FAIL %d: %s\n", __LINE__, #c);                                                  \
            exit(1);                                                                                         \
        }                                                                                                    \
    } while (0)
static bool maximized, fullscreen, down, mouse_pressed, released, blocked_hit;
static bool key_down[512], key_press[512], key_repeat[512];
static int fullscreen_actions, tab_close_actions, problems_actions;
bool IsKeyDown(int key) {
    return key >= 0 && key < 512 && key_down[key];
}
bool IsKeyPressed(int key) {
    return key >= 0 && key < 512 && key_press[key];
}
bool IsKeyPressedRepeat(int key) {
    return key >= 0 && key < 512 && key_repeat[key];
}
static Hit occupied;
static Vector2 position;
static int width, height, created_jobs, window_mutations;
static int minimize_requests;
static double clock_value = 10;
static void strset(char *out, size_t cap, const char *text) {
    snprintf(out, cap, "%s", text);
}
void app_toast(App *a, const char *message, bool error) {
    strset(a->toast, sizeof(a->toast), message);
    a->toast_error = error;
}
FileJob *file_job_start(const char *root, const char *parent, const char *target, const FsPaths *paths,
                        char *error, size_t cap) {
    (void)parent;
    (void)paths;
    (void)error;
    (void)cap;
    CHECK(!strcmp(root, "/project") && !strcmp(target, "/project/folder"));
    created_jobs++;
    FileJob *job = calloc(1, sizeof(*job));
    CHECK(job);
    return job;
}
bool DirectoryExists(const char *path) {
    return fs_kind(path) == FS_DIRECTORY;
}
int MakeDirectory(const char *path) {
#ifdef _WIN32
    return _mkdir(path);
#else
    return mkdir(path, 0700);
#endif
}
void MinimizeWindow(void) {
    minimize_requests++;
}
void ToggleFullscreen(void) {
    window_mutations++;
    fullscreen = !fullscreen;
}
bool IsWindowFullscreen(void) {
    return fullscreen;
}
bool IsWindowMaximized(void) {
    return maximized;
}
bool IsMouseButtonPressed(int button) {
    (void)button;
    return mouse_pressed;
}
bool IsMouseButtonDown(int button) {
    (void)button;
    return down;
}
bool IsMouseButtonReleased(int button) {
    (void)button;
    return released;
}
Vector2 GetWindowPosition(void) {
    return position;
}
void SetWindowPosition(int x, int y) {
    window_mutations++;
    position = (Vector2){(float)x, (float)y};
}
void SetWindowSize(int w, int h) {
    window_mutations++;
    width = w;
    height = h;
}
void RestoreWindow(void) {
    window_mutations++;
    maximized = false;
}
void MaximizeWindow(void) {
    window_mutations++;
    maximized = true;
}
void SetMouseCursor(int cursor) {
    (void)cursor;
}
double GetTime(void) {
    return clock_value;
}
bool ui_inside(Rectangle r, Vector2 p) {
    return p.x >= r.x && p.x < r.x + r.width && p.y >= r.y && p.y < r.y + r.height;
}
const Hit *ui_clicked(UI *ui, bool modal) {
    (void)ui;
    (void)modal;
    return blocked_hit ? &occupied : NULL;
}
Buffer *app_buffer(App *a) {
    return a->tabs[a->active];
}
static int visible_requests;
void app_layout(App *a) {
    a->layout = layout_workspace(a->w, a->h, true, a->topbar, a->terminal, false, 226, 211);
}
void app_focus_pane(App *a, int pane) {
    a->active_pane = pane;
    a->active = a->panes[pane].tab;
}
void app_ensure_visible(App *a) {
    (void)a;
    visible_requests++;
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
static char system_clipboard[65536];
const char *GetClipboardText(void) {
    return system_clipboard;
}
void SetClipboardText(const char *text) {
    snprintf(system_clipboard, sizeof(system_clipboard), "%s", text);
}
void app_ssh_reset_analysis(App *a) {
    (void)a;
}
void app_ssh_change_project(App *a, const char *path) {
    snprintf(a->project, sizeof(a->project), "%s", path);
}
void app_ssh_drop_remote_tabs(App *a) {
    (void)a;
}
FileJob *file_job_start_guarded(const char *root, const char *parent, const char *target,
                                const FsPaths *sources, FsGuard guard, void *ctx, char *error, size_t cap) {
    (void)guard;
    (void)ctx;
    return file_job_start(root, parent, target, sources, error, cap);
}
#include "prisme_controls_functions.inc"
void app_action(App *a, Action id, int arg) {
    (void)arg;
    if (id == A_MINIMIZE)
        app_minimize_window(a);
    else if (id == A_LINT_IGNORE)
        app_lint_quick_ignore(a, arg);
    else if (id == A_LINT_RULES)
        app_lint_rules_open(a);
    else if (id == A_LINT_RULES_APPLY)
        app_lint_rules_apply(a);
    else if (id == A_LINT_RULES_CLEAR) {
        app_set_field(&a->field, "", false);
        a->lint_rules_error[0] = 0;
    } else if (id == A_QUIT)
        request_quit(a);
    else if (id == A_FULLSCREEN) {
        fullscreen_actions++;
        a->window_drag = false;
        a->window_resize = 0;
        ToggleFullscreen();
        app_layout(a);
        a->caret_x = -1;
    } else if (id == A_TAB_CLOSE)
        tab_close_actions++;
    else if (id == A_PROBLEMS)
        problems_actions++;
    else if (id == A_FOLDER_SEARCH_MAXIMIZE)
        app_folder_search_maximize(a);
    else if (id == A_FOLDER_SEARCH_FOCUS)
        a->folder_search.focus = arg;
    else if (id == A_MODAL_CLOSE)
        close_modal(a);
}
static void frame(App *a, int w, int h) {
    a->w = w;
    a->h = h;
    a->layout = layout_workspace(w, h, true, true, true, false, 226, 211);
}
static void keyboard_clear(App *a) {
    memset(key_down, 0, sizeof(key_down));
    memset(key_press, 0, sizeof(key_press));
    memset(key_repeat, 0, sizeof(key_repeat));
    memset(a->keys, 0, sizeof(a->keys));
}
#include "test_search_frame.h"
#include "test_keyboard_frame.h"
#include "test_lint_rules_frame.h"
#include "test_lint_quick_frame.h"
#include "test_browser_odoo_frame.h"
#include "test_minimize_frame.h"
static void cursor_chord(App *a, int control, int alt, int arrow, bool repeat, bool queued) {
    keyboard_clear(a);
    if (queued)
        a->keys[control] = a->keys[alt] = a->keys[arrow] = true;
    else {
        key_down[control] = key_down[alt] = true;
        if (repeat)
            key_repeat[arrow] = true;
        else
            key_press[arrow] = true;
    }
    CHECK(app_multicursor_keys(a, !queued && alt != KEY_RIGHT_ALT));
}
static void keyboard_document(App *a, Buffer *b, const char *source, size_t row, size_t column) {
    buf_free(b);
    CHECK(buf_init(b, source, "keyboard.py"));
    b->cursor = buf_position(b, row, column);
    a->tabs[0] = b;
    a->active = a->active_pane = a->focus = 0;
    a->tab_count = 1;
    a->modal = M_NONE;
    app_multicursor_reset(a);
    keyboard_clear(a);
}
static void keyboard_multicursor(void) {
    int start = checks;
    App *a = calloc(1, sizeof(*a));
    CHECK(a);
    Buffer b = {0};
    const char *original = "abcd\nabcd\nabcd\nabcd\nabcd";
    keyboard_document(a, &b, original, 2, 2);
    a->completion_count = 1;
    a->completion_pending = a->completion_active = true;
    unsigned long version = b.version;
    cursor_chord(a, KEY_RIGHT_CONTROL, KEY_LEFT_ALT, KEY_DOWN, false, false);
    CHECK(buf_caret_count(&b) == 2 && buf_row(&b, b.cursor) == 3);
    CHECK(!a->completion_count && !a->completion_pending && !a->completion_active);
    cursor_chord(a, KEY_RIGHT_CONTROL, KEY_LEFT_ALT, KEY_DOWN, true, false);
    CHECK(buf_caret_count(&b) == 3 && buf_row(&b, b.cursor) == 4);
    cursor_chord(a, KEY_LEFT_CONTROL, KEY_LEFT_ALT, KEY_UP, false, true);
    CHECK(buf_caret_count(&b) == 4 && buf_row(&b, b.cursor) == 1);
    cursor_chord(a, KEY_LEFT_CONTROL, KEY_RIGHT_ALT, KEY_UP, true, false);
    CHECK(buf_caret_count(&b) == 5 && buf_row(&b, b.cursor) == 0);
    cursor_chord(a, KEY_LEFT_CONTROL, KEY_RIGHT_ALT, KEY_UP, true, false);
    CHECK(buf_caret_count(&b) == 5 && b.version == version && !b.undo.count && !strcmp(b.text, original));
    keyboard_clear(a);
    CHECK(!app_multicursor_keys(a, false) && !a->vertical_document);
    CHECK(buf_type(&b, 0xe9) && !strcmp(b.text, "abécd\nabécd\nabécd\nabécd\nabécd"));
    CHECK(b.undo.count == 1 && buf_caret_count(&b) == 5);
    CHECK(buf_undo(&b) && !strcmp(b.text, original) && buf_caret_count(&b) == 5);
    CHECK(buf_redo(&b) && strstr(b.text, "abécd"));
    buf_clear_carets(&b);
    CHECK(buf_caret_count(&b) == 1);
    /* GNOME fallback: Ctrl+Shift+J/K, with repeated presses and atomic undo. */
    keyboard_document(a, &b, "ab\nab\nab", 1, 1);
    cursor_chord(a, KEY_LEFT_CONTROL, KEY_LEFT_SHIFT, KEY_J, false, false);
    CHECK(buf_caret_count(&b) == 2 && buf_row(&b, b.cursor) == 2);
    cursor_chord(a, KEY_LEFT_CONTROL, KEY_LEFT_SHIFT, KEY_K, true, false);
    CHECK(buf_caret_count(&b) == 3 && buf_row(&b, b.cursor) == 0);
    CHECK(buf_type(&b, 'Z') && !strcmp(b.text, "aZb\naZb\naZb"));
    CHECK(buf_undo(&b) && !strcmp(b.text, "ab\nab\nab") && buf_caret_count(&b) == 3);
    keyboard_clear(a);
    key_down[KEY_LEFT_CONTROL] = true;
    key_press[KEY_K] = true;
    CHECK(!app_multicursor_keys(a, true));
    keyboard_clear(a);
    key_down[KEY_LEFT_SHIFT] = true;
    key_press[KEY_J] = true;
    CHECK(!app_multicursor_keys(a, false));
    /* Desired visual column survives short lines during one Ctrl+Alt gesture. */
    keyboard_document(a, &b, "abcdef\nx\n\tétoile\néclair", 0, 5);
    cursor_chord(a, KEY_LEFT_CONTROL, KEY_LEFT_ALT, KEY_DOWN, false, false);
    CHECK(buf_column(&b, b.cursor) == 1 && a->vertical_column == 5);
    cursor_chord(a, KEY_LEFT_CONTROL, KEY_LEFT_ALT, KEY_DOWN, true, false);
    CHECK(buf_column(&b, b.cursor) == 5 && b.cursor == buf_position(&b, 2, 5));
    cursor_chord(a, KEY_LEFT_CONTROL, KEY_LEFT_ALT, KEY_DOWN, true, false);
    CHECK(b.cursor == buf_position(&b, 3, 5) && buf_caret_count(&b) == 4);
    CHECK(buf_type(&b, 'Z') && !strcmp(b.text, "abcdeZf\nxZ\n\téZtoile\néclaiZr"));
    /* Release/repress, editing, pane change and focus change reset a gesture. */
    keyboard_document(a, &b, "abcdef\nx\nabcdef", 0, 5);
    cursor_chord(a, KEY_LEFT_CONTROL, KEY_LEFT_ALT, KEY_DOWN, false, false);
    keyboard_clear(a);
    CHECK(!app_multicursor_keys(a, false));
    cursor_chord(a, KEY_LEFT_CONTROL, KEY_LEFT_ALT, KEY_DOWN, false, false);
    CHECK(a->vertical_column == 1 && b.cursor == buf_position(&b, 2, 1));
    keyboard_document(a, &b, "abcdef\nx\nabcdef", 0, 5);
    cursor_chord(a, KEY_LEFT_CONTROL, KEY_LEFT_ALT, KEY_DOWN, false, false);
    a->active_pane = 1;
    cursor_chord(a, KEY_LEFT_CONTROL, KEY_LEFT_ALT, KEY_DOWN, false, false);
    CHECK(a->vertical_column == 1);
    keyboard_document(a, &b, "abcdef\nx\nabcdef", 0, 5);
    cursor_chord(a, KEY_LEFT_CONTROL, KEY_LEFT_ALT, KEY_DOWN, false, false);
    CHECK(buf_type(&b, '!'));
    cursor_chord(a, KEY_LEFT_CONTROL, KEY_LEFT_ALT, KEY_DOWN, false, false);
    CHECK(a->vertical_column == 2 && b.cursor == buf_position(&b, 2, 2));
    /* No stealing plain arrows, Ctrl-only, AltGr text, dialogs, search or console. */
    keyboard_document(a, &b, "ab\ncd", 0, 1);
    key_press[KEY_DOWN] = true;
    CHECK(!app_multicursor_keys(a, false));
    key_down[KEY_LEFT_CONTROL] = true;
    CHECK(!app_multicursor_keys(a, true));
    keyboard_clear(a);
    key_down[KEY_RIGHT_ALT] = true;
    key_press[KEY_DOWN] = true;
    CHECK(!app_multicursor_keys(a, false) && buf_caret_count(&b) == 1);
    keyboard_clear(a);
    key_down[KEY_RIGHT_ALT] = true;
    CHECK(!app_multicursor_keys(a, false) && buf_type(&b, 0x20ac) && !strcmp(b.text, "a€b\ncd"));
    keyboard_document(a, &b, "abc\nabc", 0, 1);
    key_down[KEY_LEFT_CONTROL] = key_down[KEY_LEFT_ALT] = key_press[KEY_DOWN] = true;
    for (int focus = 1; focus <= 2; focus++) {
        a->focus = focus;
        CHECK(!app_multicursor_keys(a, true) && buf_caret_count(&b) == 1);
    }
    a->focus = 0;
    const Modal modes[] = {M_SNIPPET_EDIT, M_SETTINGS, M_BROWSER, M_UNSAVED};
    for (size_t i = 0; i < sizeof(modes) / sizeof(modes[0]); i++) {
        a->modal = modes[i];
        CHECK(!app_multicursor_keys(a, true) && buf_caret_count(&b) == 1);
    }
    a->modal = M_NONE;
    a->active = -1;
    CHECK(!app_multicursor_keys(a, true));
    a->active = 0;
    CHECK(app_multicursor_keys(a, true) && buf_caret_count(&b) == 2);
    char many[1201] = "";
    for (int i = 0; i < 300; i++)
        strcat(many, "abc\n");
    keyboard_document(a, &b, many, 0, 2);
    for (int i = 1; i < BUF_MAX_CARETS; i++)
        cursor_chord(a, KEY_LEFT_CONTROL, KEY_LEFT_ALT, KEY_DOWN, true, false);
    version = b.version;
    cursor_chord(a, KEY_LEFT_CONTROL, KEY_LEFT_ALT, KEY_DOWN, true, false);
    CHECK(buf_caret_count(&b) == BUF_MAX_CARETS && b.version == version && !b.undo.count &&
          strstr(a->toast, "256"));
    keyboard_document(a, &b, "", 0, 0);
    cursor_chord(a, KEY_LEFT_CONTROL, KEY_LEFT_ALT, KEY_DOWN, false, false);
    CHECK(b.cursor == 0 && buf_caret_count(&b) == 1 && !b.undo.count);
    keyboard_clear(a);
    buf_free(&b);
    free(a);
    printf("OK %d actual Ctrl+Alt+Up/Down keyboard checks: queued/held/repeat, left/right modifiers, "
           "reverse, visual columns, simultaneous UTF-8 input/undo, AltGr text, focus/modal/pane guards and "
           "256-carets; no GUI\n",
           checks - start);
}
#ifndef _WIN32
static void ssh_controllers(void) {
    char directory[] = "/tmp/prisme-native-ssh-controls-XXXXXX";
    CHECK(mkdtemp(directory));
    const char *previous = getenv("PRISME_CONFIG_DIR");
    char *saved = previous ? strdup(previous) : NULL;
    CHECK(!setenv("PRISME_CONFIG_DIR", directory, 1));
    App *a = calloc(1, sizeof(*a));
    CHECK(a);
    strcpy(a->project, "/project");
    app_ssh_init(a, "/missing/helper");
    app_ssh_action(a, A_SSH, 0);
    const char *fields[] = {"first", "odoo", "2222", "/opt/odoo18", "/unavailable/key"};
    for (int i = 0; i < 5; i++)
        app_set_field(&a->ssh_fields[i], fields[i], false);
    app_ssh_action(a, A_SSH_CONNECT, 0); /* Preflight fails, but profile survives. */
    CHECK(a->ssh.state == SSH_ERROR && a->ssh_profiles.count == 1);
    app_ssh_action(a, A_SSH, 0);
    for (int i = 0; i < 5; i++)
        CHECK(!strcmp(a->ssh_fields[i].text, fields[i]));
    char id[33];
    strcpy(id, a->ssh_selected);
    app_set_field(&a->ssh_fields[2], "2223", false);
    app_ssh_action(a, A_SSH_SAVE_PROFILE, 0);
    CHECK(a->ssh_profiles.count == 1 && !strcmp(a->ssh_selected, id));
    app_ssh_action(a, A_SSH_NEW, 0);
    CHECK(!*a->ssh_selected && !a->ssh_fields[0].len);
    app_set_field(&a->ssh_fields[0], "second", false);
    app_ssh_action(a, A_SSH_SAVE_PROFILE, 0);
    CHECK(a->ssh_profiles.count == 2);
    app_ssh_action(a, A_SSH_PROFILES, 0);
    CHECK(a->modal == M_SSH_PROFILES);
    app_ssh_action(a, A_SSH_PROFILE_PICK, 0);
    CHECK(!strcmp(a->ssh_fields[0].text, "first"));
    keyboard_clear(a);
    a->keys[KEY_TAB] = true;
    app_ssh_keys(a, NULL, 0, false, false);
    CHECK(a->ssh_field == 1);
    keyboard_clear(a);
    a->keys[KEY_TAB] = true;
    app_ssh_keys(a, NULL, 0, false, true);
    CHECK(a->ssh_field == 0);
    /* A burst of queued characters must revert the WHOLE field, not one byte. */
    app_set_field(&a->ssh_fields[0], "first", false);
    a->ssh_field = 0;
    char long_clipboard[600];
    memset(long_clipboard, 'a', 599);
    long_clipboard[599] = 0;
    strcpy(system_clipboard, long_clipboard);
    keyboard_clear(a);
    a->keys[KEY_V] = true;
    app_ssh_keys(a, NULL, 0, true, false);
    CHECK(!strcmp(a->ssh_fields[0].text, "first"));
    app_ssh_action(a, A_SSH_PROFILES, 0);
    app_ssh_action(a, A_SSH_PROFILE_REMOVE, 0);
    CHECK(a->ssh_profiles.count == 1);
    int pair[2];
    CHECK(!socketpair(AF_UNIX, SOCK_STREAM, 0, pair));
    a->ssh.auth_pending = true;
    a->ssh.auth_confirm = false;
    a->ssh.client = pair[0];
    a->ssh.state = SSH_CONNECTING;
    a->modal = M_SSH_AUTH;
    a->ssh_previous_modal = M_SSH;
    const char *secret = "secret-é-ne-pas-copier";
    app_set_field(&a->ssh_auth, secret, true);
    strcpy(system_clipboard, "unchanged");
    keyboard_clear(a);
    a->keys[KEY_C] = true;
    app_ssh_keys(a, NULL, 0, true, false);
    CHECK(!strcmp(system_clipboard, "unchanged"));
    keyboard_clear(a);
    a->keys[KEY_X] = true;
    app_ssh_keys(a, NULL, 0, true, false);
    CHECK(!strcmp(system_clipboard, "unchanged") && !strcmp(a->ssh_auth.text, secret));
    keyboard_clear(a);
    a->keys[KEY_ENTER] = true;
    app_ssh_keys(a, NULL, 0, false, false);
    char reply[1024] = "";
    CHECK(read(pair[1], reply, sizeof(reply) - 1) > 0);
    close(pair[1]);
    CHECK(strstr(reply, secret));
    CHECK(!a->ssh_auth.len && !a->ssh_auth.undo.count && a->modal == M_SSH);
    CHECK(!socketpair(AF_UNIX, SOCK_STREAM, 0, pair));
    a->ssh.auth_pending = true;
    a->ssh.auth_confirm = true;
    a->ssh.client = pair[0];
    a->modal = M_SSH_AUTH;
    a->frame_tick = a->ssh_auth_frame = 3;
    keyboard_clear(a);
    a->keys[KEY_ENTER] = true;
    app_ssh_keys(a, NULL, 0, true, false);
    CHECK(a->ssh.auth_pending);
    a->frame_tick++;
    app_ssh_keys(a, NULL, 0, false, false);
    CHECK(a->ssh.auth_pending);
    key_repeat[KEY_ENTER] = true;
    app_ssh_keys(a, NULL, 0, true, false);
    CHECK(a->ssh.auth_pending);
    key_repeat[KEY_ENTER] = false;
    app_ssh_keys(a, NULL, 0, true, false);
    CHECK(!a->ssh.auth_pending);
    memset(reply, 0, sizeof(reply));
    CHECK(read(pair[1], reply, sizeof(reply) - 1) > 0);
    CHECK(strstr(reply, "yes"));
    close(pair[1]);
    a->ssh.state = SSH_ERROR;
    app_ssh_free(a);
    free(a);
    if (saved) {
        CHECK(!setenv("PRISME_CONFIG_DIR", saved, 1));
        free(saved);
    } else
        CHECK(!unsetenv("PRISME_CONFIG_DIR"));
    char error[512] = "";
    CHECK(fs_remove("/tmp", directory, NULL, NULL, error, sizeof(error)));
}
#endif
#ifndef _WIN32
static void project_session_controller(void) {
    char directory[] = "/tmp/python-studio-controller-XXXXXX";
    CHECK(mkdtemp(directory));
    App *a = calloc(1, sizeof(*a));
    CHECK(a);
    strcpy(a->project, directory);
    strcpy(a->last_local_project, directory);
    char sessions[BUF_PATH], path[BUF_PATH];
    snprintf(sessions, sizeof(sessions), "%s/sessions", directory);
    snprintf(path, sizeof(path), "%s/main.py", directory);
    FILE *f = fopen(path, "w");
    CHECK(f);
    fputs("source\n", f);
    fclose(f);
    a->tabs[0] = calloc(1, sizeof(Buffer));
    CHECK(a->tabs[0]);
    CHECK(buf_init(a->tabs[0], "source\n", "main.py"));
    strcpy(a->tabs[0]->path, path);
    CHECK(buf_insert(a->tabs[0], "draft\n"));
    a->tab_count = 1;
    a->active = 0;
    a->font_size = 20;
    a->sidebar_width = 246;
    a->terminal_h = 230;
    a->split_ratio = .5f;
    a->sidebar = a->topbar = a->terminal = true;
    app_ssh_init(a, "/tmp/unused-helper");
    app_set_field(&a->ssh_auth, "SSH-SECRET-NOT-SERIALIZED", false);
    studio_store_init(&a->session, sessions, true);
    a->session_ready = true;
    a->running = true;
    CHECK(app_session_checkpoint(a, true));
    f = fopen(a->session.path, "rb");
    CHECK(f);
    char bytes[20000];
    size_t size = fread(bytes, 1, sizeof(bytes), f);
    fclose(f);
    const char *secret = "SSH-SECRET-NOT-SERIALIZED";
    for (size_t n = 0; n + strlen(secret) <= size; n++)
        CHECK(memcmp(bytes + n, secret, strlen(secret)));
    CHECK(app_session_restore(a));
    CHECK(a->tab_count == 1 && buf_dirty(a->tabs[0]) && !strcmp(a->tabs[0]->text, "draft\nsource\n"));
    a->session_discard[0] = true;
    a->modal = M_UNSAVED;
    a->pending = 2;
    close_modal(a);
    CHECK(!a->session_discard[0] && !a->session_committing_quit);
    a->session_discard[0] = true;
    a->session_keep = true;
    a->ssh.process = 999999998;
    a->runner.running = true;
    request_quit(a);
    a->ssh.process = 0;
    a->runner.running = false;
    CHECK(a->running && !a->ssh_quit_pending && !a->session_committing_quit && !a->session_keep &&
          !a->session_discard[0]);
    StudioImage *restored = calloc(1, sizeof(*restored));
    char error[512] = "";
    CHECK(restored);
    CHECK(studio_load(&a->session, a->project, restored, error, sizeof(error)) == 1);
    CHECK(!strcmp(restored->tabs[0]->text, a->tabs[0]->text));
    studio_image_free(restored);
    free(restored);
    app_session_keep_quit(a);
    CHECK(!a->running && a->session_committing_quit && a->session_keep);
    studio_store_close(&a->session);
    app_ssh_free(a);
    buf_free(a->tabs[0]);
    free(a->tabs[0]);
    free(a);
    char command[BUF_PATH];
    snprintf(command, sizeof(command), "rm -rf -- '%s'", directory);
    CHECK(!system(command));
}
#endif
#ifndef _WIN32
/* Real search controller + injected native keys; only window calls are mocked. */
static void folder_job_done(App *a) {
    CHECK(a->folder_search.job);
    double until = runner_clock() + 4;
    while (!atomic_load(&a->folder_search.job->done) && runner_clock() < until) {
        struct timespec pause = {0, 1000000};
        nanosleep(&pause, NULL);
    }
    CHECK(atomic_load(&a->folder_search.job->done));
}
typedef struct {
    atomic_bool entered, release;
} FolderBlocked;
static bool folder_blocked_guard(void *user, const char *path, bool leaf, char *error, size_t cap) {
    (void)path;
    (void)leaf;
    (void)error;
    (void)cap;
    FolderBlocked *g = user;
    atomic_store(&g->entered, true);
    while (!atomic_load(&g->release)) {
        struct timespec pause = {0, 1000000};
        nanosleep(&pause, NULL);
    }
    return true;
}
static void folder_search_controller(void) {
    App *a = calloc(1, sizeof(*a));
    Buffer b = {0};
    CHECK(a && buf_init(&b, "source", "dirty.py"));
    CHECK(buf_insert(&b, "unsaved"));
    b.anchor = 2;
    size_t cursor = b.cursor, anchor = b.anchor;
    unsigned long version = b.version;
    int undo = b.undo.count;
    a->tabs[0] = &b;
    a->tab_count = 1;
    atomic_init(&a->ssh.generation, 0);
    atomic_init(&a->ssh.state, SSH_IDLE);
    atomic_init(&a->ssh.process_alive, false);
    char root[] = "/tmp/studio-folder-controller-XXXXXX", prefs[BUF_PATH], sub[BUF_PATH], one[BUF_PATH],
         two[BUF_PATH], profile[BUF_PATH], error[512];
    CHECK(mkdtemp(root));
    CHECK(snprintf(prefs, sizeof(prefs), "%s/prefs", root) < BUF_PATH && !mkdir(prefs, 0700));
    CHECK(snprintf(a->config, sizeof(a->config), "%s/settings.cfg", prefs) < BUF_PATH);
    CHECK(snprintf(sub, sizeof(sub), "%s/sub", root) < BUF_PATH && !mkdir(sub, 0700));
    CHECK(snprintf(one, sizeof(one), "%s/a.txt", root) < BUF_PATH);
    CHECK(snprintf(two, sizeof(two), "%s/b.txt", sub) < BUF_PATH);
    FILE *f = fopen(one, "w");
    CHECK(f && fputs("needle + needle\n", f) >= 0 && !fclose(f));
    f = fopen(two, "w");
    CHECK(f && fputs("α\tneedle\n", f) >= 0 && !fclose(f));
    strcpy(a->project, root);
    a->xml_paths.count = 1;
    strcpy(a->xml_paths.paths[0], "/XML-only");
    a->linter.imports.count = 1;
    strcpy(a->linter.imports.paths[0], "/Python-only");
    keyboard_clear(a);
    a->keys[KEY_F] = true;
    CHECK(!app_folder_search_shortcut(a, false, true));
    CHECK(!app_folder_search_shortcut(a, true, false));
    CHECK(app_folder_search_shortcut(a, true, true));
    FolderSearchState *s = &a->folder_search;
    CHECK(a->modal == M_FOLDER_SEARCH && s->initialized && !strcmp(s->folder.text, root) && !s->query.len &&
          !s->job);
    keyboard_clear(a);
    const unsigned chars[] = {'n', 'e', 'e', 'd', 'l', 'e'};
    app_folder_search_keys(a, chars, 6, false, false);
    CHECK(!strcmp(s->query.text, "needle") && !s->job);
    keyboard_clear(a);
    a->keys[KEY_ENTER] = true;
    app_folder_search_keys(a, NULL, 0, false, false);
    CHECK(s->job && s->active);
    folder_job_done(a);
    app_folder_search_poll(a);
    CHECK(!s->job && s->report && s->report->count == 2 && s->report->hit_count == 3 && s->file == 0 &&
          s->hit == 0);
    CHECK(!strcmp(s->view.text, "needle + needle\n"));
    int expected[][2] = {{0, 1}, {1, 0}, {0, 0}};
    for (unsigned i = 0; i < 3; i++) {
        keyboard_clear(a);
        a->keys[KEY_F3] = true;
        app_folder_search_keys(a, NULL, 0, false, false);
        CHECK(s->file == expected[i][0] && s->hit == expected[i][1]);
    }
    keyboard_clear(a);
    a->keys[KEY_F3] = true;
    app_folder_search_keys(a, NULL, 0, false, true);
    CHECK(s->file == 1 && s->hit == 0 && !strcmp(s->view.text, "α\tneedle\n"));
    /* Real production dialog-only enlargement; native window API is mocked. */
    CHECK(!fullscreen && !s->maximized);
    app_folder_search_maximize(a);
    CHECK(s->maximized && !fullscreen);
    keyboard_clear(a);
    a->keys[KEY_F11] = true;
    app_folder_search_keys(a, NULL, 0, false, false);
    CHECK(!fullscreen && !s->maximized);
    keyboard_clear(a);
    a->keys[KEY_F11] = true;
    app_folder_search_keys(a, NULL, 0, false, false);
    CHECK(!fullscreen && s->maximized);
    app_folder_search_maximize(a);
    CHECK(!s->maximized);
    app_folder_search_fullscreen(a);
    close_modal(a);
    CHECK(!fullscreen && a->modal == M_NONE);
    fullscreen = true;
    app_folder_search_show(a);
    app_folder_search_fullscreen(a);
    close_modal(a);
    CHECK(fullscreen);
    fullscreen = false;
    app_folder_search_show(a);
    keyboard_clear(a);
    a->keys[KEY_TAB] = true;
    app_folder_search_keys(a, NULL, 0, false, false);
    CHECK(s->focus == 8);
    keyboard_clear(a);
    a->keys[KEY_SPACE] = true;
    app_folder_search_keys(a, NULL, 0, false, false);
    CHECK(!s->options.case_sensitive);
    app_folder_search_option(a, 2);
    CHECK(s->options.regex);
    app_set_field(&s->query, "NEEDLE(?: \\+ NEEDLE)?", false);
    app_set_field(&s->filters[0], "*.txt", false);
    app_set_field(&s->filters[1], "sub/*", false);
    app_folder_search_start(a);
    folder_job_done(a);
    app_folder_search_poll(a);
    CHECK(s->report && s->report->count == 1 && s->report->hit_count == 1 &&
          !s->report->options.case_sensitive && s->report->options.regex);
    CHECK(s->report->files[0].hits[0].end == 15 && b.version == version && b.undo.count == undo);
    /* Private preferences retain only the folder, not query/options/globs. */
    FILE *saved = fopen(s->profile, "rb");
    CHECK(saved);
    char preference[8192];
    size_t preference_len = fread(preference, 1, sizeof(preference) - 1, saved);
    preference[preference_len] = 0;
    CHECK(!fclose(saved) && !strstr(preference, "NEEDLE") && !strstr(preference, "*.txt") &&
          !strstr(preference, "regex"));
    app_set_field(&s->filters[0], "", false);
    app_set_field(&s->filters[1], "", false);
    s->options = folder_search_options();
    app_set_field(&s->query, "needle", false);
    app_folder_search_start(a);
    folder_job_done(a);
    app_folder_search_option(a, 2);
    app_folder_search_option(a, 2);
    app_folder_search_poll(a);
    CHECK(!s->report && !s->job); /* changed-back flags cannot publish a cancelled lease */
    app_folder_search_start(a);
    folder_job_done(a);
    app_folder_search_poll(a);
    CHECK(s->report && s->report->hit_count == 3);
    app_folder_search_select(a, 1, 0);
    CHECK(!remove(two));
    app_folder_search_next(a, 1);
    app_folder_search_next(a, 1);
    app_folder_search_next(a, 1);
    CHECK(s->file == 1 && !strcmp(s->view.text, "α\tneedle\n"));
    f = fopen(two, "w");
    CHECK(f && fputs("α\tneedle\n", f) >= 0 && !fclose(f));
    s->focus = 3;
    keyboard_clear(a);
    const unsigned typed[] = {'X'};
    app_folder_search_keys(a, typed, 1, false, false);
    a->keys[KEY_V] = true;
    app_folder_search_keys(a, NULL, 0, true, false);
    keyboard_clear(a);
    a->keys[KEY_Z] = true;
    app_folder_search_keys(a, NULL, 0, true, false);
    CHECK(b.version == version && b.cursor == cursor && b.anchor == anchor && b.undo.count == undo &&
          buf_dirty(&b) && !strcmp(b.text, "unsavedsource"));
    CHECK(!s->view.undo.count && !strcmp(s->view.text, "α\tneedle\n"));
    CHECK(a->xml_paths.count == 1 && !strcmp(a->xml_paths.paths[0], "/XML-only") &&
          a->linter.imports.count == 1 && !strcmp(a->linter.imports.paths[0], "/Python-only"));
    strcpy(profile, s->profile);
    char remembered[FS_PATH];
    CHECK(folder_search_profile_load(remembered, profile, root, error, sizeof(error)) &&
          !strcmp(remembered, root));
    a->modal = M_BROWSER;
    a->browser_mode = 5;
    close_modal(a);
    CHECK(a->modal == M_FOLDER_SEARCH && !strcmp(s->query.text, "needle"));
    close_modal(a);
    CHECK(a->modal == M_NONE && !s->active && s->report);
    app_folder_search_show(a);
    CHECK(s->report && s->query.anchor == 0);
    /* UTF-8 byte limit rejects the whole paste, never the document. */
    s->focus = 1;
    memset(system_clipboard, 'x', 1025);
    system_clipboard[1025] = 0;
    keyboard_clear(a);
    a->keys[KEY_V] = true;
    app_folder_search_keys(a, NULL, 0, true, false);
    CHECK(!strcmp(s->query.text, "needle") && *s->error && b.version == version);
    app_set_field(&s->query, "needle", false);
    keyboard_clear(a);
    app_folder_search_start(a);
    folder_job_done(a);
    atomic_fetch_add(&a->ssh.generation, 1);
    app_folder_search_poll(a);
    CHECK(!s->job && !s->active && !s->report && *s->error);
    /* Install a genuinely outstanding guarded worker, then close/reopen. No
       controller action is allowed to join it or overwrite its lease. */
    FolderBlocked block;
    atomic_init(&block.entered, false);
    atomic_init(&block.release, false);
    s->job = folder_search_start(root, "needle", NULL, folder_blocked_guard, &block, error, sizeof(error));
    CHECK(s->job);
    s->active = true;
    s->lease.generation = atomic_load(&a->ssh.generation);
    double until = runner_clock() + 3;
    while (!atomic_load(&block.entered) && runner_clock() < until) {
        struct timespec pause = {0, 1000000};
        nanosleep(&pause, NULL);
    }
    CHECK(atomic_load(&block.entered));
    FolderSearchJob *outstanding = s->job;
    app_folder_search_fullscreen(a);
    CHECK(!fullscreen && s->maximized);
    double start = runner_clock();
    close_modal(a);
    CHECK(runner_clock() - start < .1 && !s->active && !atomic_load(&s->job->done) && !fullscreen);
    app_folder_search_show(a);
    app_folder_search_start(a);
    CHECK(s->job == outstanding && strstr(s->error, "annulation"));
    atomic_store(&block.release, true);
    folder_job_done(a);
    app_folder_search_poll(a);
    CHECK(!s->job && !s->report);
    /* Stop preserves the partial (possibly empty) snapshot, unlike close. */
    atomic_store(&block.entered, false);
    atomic_store(&block.release, false);
    s->job = folder_search_start(root, "needle", NULL, folder_blocked_guard, &block, error, sizeof(error));
    CHECK(s->job);
    s->active = true;
    until = runner_clock() + 3;
    while (!atomic_load(&block.entered) && runner_clock() < until) {
        struct timespec pause = {0, 1000000};
        nanosleep(&pause, NULL);
    }
    CHECK(atomic_load(&block.entered));
    app_folder_search_stop(a);
    CHECK(s->active && atomic_load(&s->job->cancel));
    atomic_store(&block.release, true);
    folder_job_done(a);
    app_folder_search_poll(a);
    CHECK(s->report && s->report->cancelled && !s->job);
    /* A disconnected SSH cache and a local alias must never be searched as
       ordinary placeholder directories, even when they contain matching text. */
    char reserved[BUF_PATH], placeholder[BUF_PATH], alias[BUF_PATH];
    CHECK(snprintf(reserved, sizeof(reserved), "%s/reserved", prefs) < BUF_PATH && !mkdir(reserved, 0700));
    CHECK(snprintf(placeholder, sizeof(placeholder), "%s/placeholder", reserved) < BUF_PATH);
    f = fopen(placeholder, "w");
    CHECK(f && fputs("needle", f) >= 0 && !fclose(f));
    CHECK(snprintf(alias, sizeof(alias), "%s/offline-alias", root) < BUF_PATH && !symlink(reserved, alias));
    a->ssh.initialized = true;
    strcpy(a->ssh.cache, prefs);
    strcpy(a->ssh.mountpoint, reserved);
    atomic_store(&a->ssh.state, SSH_LOST);
    const char *offline_roots[] = {reserved, alias};
    for (unsigned i = 0; i < 2; i++) {
        app_set_field(&s->folder, offline_roots[i], false);
        app_folder_search_start(a);
        folder_job_done(a);
        app_folder_search_poll(a);
        CHECK(!s->job && !s->report && *s->error);
    }
    CHECK(!remove(alias) && !remove(placeholder) && !rmdir(reserved));
    a->ssh.initialized = false;
    a->ssh.cache[0] = a->ssh.mountpoint[0] = 0;
    atomic_store(&a->ssh.state, SSH_IDLE);
    app_set_field(&s->folder, root, false);
    app_folder_search_start(a);
    folder_job_done(a);
    strcpy(a->project, "/other-folder-project");
    app_folder_search_poll(a);
    CHECK(!s->job && !s->initialized && !s->report && a->modal == M_NONE);
    app_folder_search_show(a);
    CHECK(s->initialized && !s->query.len && !strcmp(s->folder.text, a->project));
    app_folder_search_shutdown(a);
    buf_free(&b);
    free(a);
    CHECK(!remove(profile));
    CHECK(!remove(one));
    CHECK(!remove(two));
    CHECK(!rmdir(sub));
    CHECK(!rmdir(prefs));
    CHECK(!rmdir(root));
    printf(
        "OK actual folder search controller: Ctrl+Maj+F, Enter, global F3/Maj+F3, read-only/carets/history, "
        "private folder, stale generation/project, outstanding-worker close/stop\n");
}
typedef struct {
    atomic_bool entered, release, last_entered, last_release;
    char first[BUF_PATH], last[BUF_PATH];
    bool denied;
} LiveControllerGate;
static bool live_controller_guard(void *user, const char *file, bool leaf, char *error, size_t cap) {
    (void)leaf;
    LiveControllerGate *g = user;
    /* A revoked lease rejects later leaves AND the final root recheck. */
    if (g->denied && atomic_load(&g->entered) && strcmp(file, g->first)) {
        snprintf(error, cap, "Search lease invalidated");
        return false;
    }
    if (!strcmp(file, g->first)) {
        atomic_store(&g->entered, true);
        while (!atomic_load(&g->release)) {
            struct timespec t = {0, 1000000};
            nanosleep(&t, NULL);
        }
        if (g->denied) {
            snprintf(error, cap, "Access invalidated during search");
            return false;
        }
    }
    if (!strcmp(file, g->last)) {
        atomic_store(&g->last_entered, true);
        while (!atomic_load(&g->last_release)) {
            struct timespec t = {0, 1000000};
            nanosleep(&t, NULL);
        }
    }
    return true;
}
static void live_controller_wait(atomic_bool *flag) {
    double until = runner_clock() + 3;
    while (!atomic_load(flag) && runner_clock() < until) {
        struct timespec t = {0, 1000000};
        nanosleep(&t, NULL);
    }
    CHECK(atomic_load(flag));
}
static void folder_search_progress_controller(void) {
    char root[] = "/tmp/studio-live-controller-XXXXXX", directory[BUF_PATH], file[BUF_PATH];
    CHECK(mkdtemp(root));
    CHECK(snprintf(directory, sizeof(directory), "%s/a", root) < BUF_PATH && !mkdir(directory, 0700));
    for (unsigned i = 0; i < 12; i++) {
        CHECK(snprintf(file, sizeof(file), "%s/n%02u.txt", directory, i) < BUF_PATH);
        FILE *f = fopen(file, "w");
        CHECK(f);
        for (unsigned line = 0; line < 80; line++)
            CHECK(fputs("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx needle needle\n",
                        f) >= 0);
        CHECK(!fclose(f));
    }
    const char *last_names[] = {"a.txt", "zz.txt"};
    for (unsigned i = 0; i < 2; i++) {
        CHECK(snprintf(file, sizeof(file), "%s/%s", root, last_names[i]) < BUF_PATH);
        FILE *f = fopen(file, "w");
        CHECK(f && fputs("needle\n", f) >= 0 && !fclose(f));
    }
    for (unsigned mode = 0; mode < 8; mode++) {
        App *a = calloc(1, sizeof(*a));
        Buffer b = {0};
        CHECK(a && buf_init(&b, "source", "dirty.py"));
        CHECK(buf_insert(&b, "unsaved"));
        b.anchor = 2;
        unsigned long version = b.version;
        int undo = b.undo.count;
        a->tabs[0] = &b;
        a->tab_count = 1;
        a->w = 960;
        a->h = 640;
        atomic_init(&a->ssh.generation, 0);
        atomic_init(&a->ssh.state, SSH_IDLE);
        atomic_init(&a->ssh.process_alive, false);
        strcpy(a->project, root);
        CHECK(snprintf(a->config, sizeof(a->config), "%s/settings.cfg", root) < BUF_PATH);
        app_folder_search_show(a);
        FolderSearchState *s = &a->folder_search;
        CHECK(s->initialized);
        app_set_field(&s->query, "needle", false);
        s->rows = s->list_rows = 4;
        s->columns = 50;
        /* Both existing native fullscreen states and maximize states are
           preserved, with ZERO OS mutations, resize, move or layout calls. */
        for (unsigned full = 0; full < 2; full++)
            for (unsigned max = 0; max < 2; max++) {
                fullscreen = full != 0;
                maximized = max != 0;
                width = 960;
                height = 640;
                position = (Vector2){61, 49};
                s->maximized = false;
                int before = window_mutations;
                keyboard_clear(a);
                a->keys[KEY_F11] = true;
                app_folder_search_keys(a, NULL, 0, false, false);
                CHECK(s->maximized && fullscreen == (full != 0) && maximized == (max != 0));
                keyboard_clear(a);
                a->keys[KEY_F11] = true;
                app_folder_search_keys(a, NULL, 0, false, false);
                CHECK(!s->maximized);
                close_modal(a);
                app_folder_search_show(a);
                CHECK(window_mutations == before && width == 960 && height == 640 && position.x == 61 &&
                      position.y == 49);
                CHECK(a->w == 960 && a->h == 640 && fullscreen == (full != 0) && maximized == (max != 0));
            }
        fullscreen = maximized = false;
        app_set_field(&s->query, "needle", false);
        s->focus = 1;
        LiveControllerGate gate;
        atomic_init(&gate.entered, false);
        atomic_init(&gate.release, false);
        atomic_init(&gate.last_entered, false);
        atomic_init(&gate.last_release, false);
        CHECK(snprintf(gate.first, sizeof(gate.first), "%s/a.txt", root) < BUF_PATH);
        CHECK(snprintf(gate.last, sizeof(gate.last), "%s/zz.txt", root) < BUF_PATH);
        gate.denied = mode == 2 || mode == 3;
        char error[512];
        s->job =
            folder_search_start(root, "needle", NULL, live_controller_guard, &gate, error, sizeof(error));
        CHECK(s->job);
        s->active = true;
        s->ssh_state = SSH_IDLE;
        s->lease.owner = &a->ssh;
        s->lease.generation = 0;
        live_controller_wait(&gate.entered);
        CHECK(!atomic_load(&s->job->done));
        if (mode != 3) {
            double start = runner_clock();
            app_folder_search_poll(a);
            CHECK(runner_clock() - start < .1 && s->report && s->report->count == 12 && !s->report->complete);
            CHECK(s->focus == 1 && s->file == 0 && strstr(s->view.text, "needle"));
            app_folder_search_select(a, 11, 159);
            app_folder_search_next(a, 1);
            CHECK(s->file == 0 && s->hit == 0);
            app_folder_search_next(a, -1);
            CHECK(s->file == 11 && s->hit == 159);
            app_folder_search_select(a, 7, 37);
            s->focus = 3;
            s->scroll = 6;
            s->x = 9;
            s->y = 11;
        }
        char *view = s->view.text;
        if (mode == 0) {
            atomic_store(&gate.release, true);
            live_controller_wait(&gate.last_entered);
            app_folder_search_poll(a);
            CHECK(s->report->count == 13 && s->file == 8 && s->hit == 37 && s->focus == 3 && s->scroll == 7 &&
                  s->x == 9 && s->y == 11);
            CHECK(s->view.text == view && !atomic_load(&s->job->done));
        } else if (mode == 1) {
            app_folder_search_stop(a);
            CHECK(s->active && atomic_load(&s->job->cancel));
        } else if (mode == 4) {
            double start = runner_clock();
            close_modal(a);
            CHECK(runner_clock() - start < .1 && !atomic_load(&s->job->done));
            CHECK(!s->active && s->report->cancelled);
            app_folder_search_show(a);
        } else if (mode == 5) {
            app_folder_search_option(a, 0);
            app_folder_search_option(a, 0);
            CHECK(!s->active);
        } else if (mode == 6) {
            atomic_fetch_add(&a->ssh.generation, 1);
            app_folder_search_poll(a);
            CHECK(!s->active);
        } else if (mode == 7) {
            strcpy(a->project, "/different-project");
            app_folder_search_poll(a);
            CHECK(!s->initialized && !s->report);
        }
        atomic_store(&gate.release, true);
        atomic_store(&gate.last_release, true);
        folder_job_done(a);
        app_folder_search_poll(a);
        CHECK(!s->job);
        if (mode == 0) {
            CHECK(s->report && s->report->count == 14 && s->report->complete);
            CHECK(s->file == 8 && s->hit == 37 && s->scroll == 7 && s->x == 9 && s->y == 11 &&
                  s->view.text == view);
        } else if (mode == 1)
            CHECK(s->report->count == 12 && s->report->cancelled && s->hit == 37 && s->view.text == view);
        else if (mode == 2 || mode == 3) {
            CHECK(*s->error && s->report && s->report->count == 12 && !s->report->complete &&
                  strstr(s->note, "partiels"));
            if (mode == 2)
                CHECK(s->file == 7 && s->hit == 37 && s->view.text == view);
            else
                CHECK(s->file == 0 && s->focus == 1);
        } else if (mode != 7)
            CHECK(s->report->count == 12 && s->report->cancelled);
        else
            CHECK(!s->report && !s->initialized);
        CHECK(b.version == version && b.undo.count == undo && b.anchor == 2 &&
              !strcmp(b.text, "unsavedsource"));
        app_folder_search_shutdown(a);
        buf_free(&b);
        free(a);
    }
    for (unsigned i = 0; i < 12; i++) {
        CHECK(snprintf(file, sizeof(file), "%s/n%02u.txt", directory, i) < BUF_PATH && !remove(file));
    }
    for (unsigned i = 0; i < 2; i++)
        CHECK(snprintf(file, sizeof(file), "%s/%s", root, last_names[i]) < BUF_PATH && !remove(file));
    CHECK(!rmdir(directory) && !rmdir(root));
    puts("OK actual live search controller: 12 files before done, sorted insertion, F3, stable "
         "path/hit/focus/viewport/cache, Stop/errors/stale/close, dialog-only F11");
}

#endif

#ifndef _WIN32
static void path_wait(App *a) {
    CHECK(a->path_completion.job);
    for (int i = 0; i < 5000 && !atomic_load(&a->path_completion.job->done); i++) {
        struct timespec pause = {0, 1000000};
        nanosleep(&pause, NULL);
    }
    CHECK(atomic_load(&a->path_completion.job->done));
    app_completion_poll(a);
}
static void directory_completion_controller(void) {
    App *a = calloc(1, sizeof(*a));
    Buffer *b = calloc(1, sizeof(*b));
    CHECK(a && b && buf_init(&a->field, "", "Field") && buf_init(b, "sale/", "draft.xml"));
    atomic_init(&a->ssh.generation, 0);
    atomic_init(&a->ssh.state, SSH_IDLE);
    atomic_init(&a->ssh.process_alive, false);
    a->tabs[0] = b;
    a->tab_count = 1;
    b->cursor = b->len;
    char base[] = "/tmp/studio-path-controller-XXXXXX", sale[BUF_PATH], views[BUF_PATH], xml[BUF_PATH];
    CHECK(mkdtemp(base));
    CHECK(snprintf(sale, sizeof(sale), "%s/sale", base) < BUF_PATH && !mkdir(sale, 0700));
    CHECK(snprintf(views, sizeof(views), "%s/views", sale) < BUF_PATH && !mkdir(views, 0700));
    CHECK(snprintf(xml, sizeof(xml), "%s/sale_views.xml", views) < BUF_PATH);
    const char *source = "<odoo><record id=\"sale_order\" model=\"ir.ui.view\"><field name=\"arch\" "
                         "type=\"xml\"><form/></field></record></odoo>";
    FILE *f = fopen(xml, "wb");
    CHECK(f && fputs(source, f) >= 0 && !fclose(f));
    strcpy(a->project, "/missing-project-directory");
    a->xml_paths.count = 1;
    strcpy(a->xml_paths.paths[0], base);
    CHECK(!app_xml_begin(a, false));
    app_completion_show(a, true);
    CHECK(a->path_completion.active && a->completion_pending);
    path_wait(a);
    CHECK(a->completion_count == 1 && a->completions[0].kind == 'D' &&
          !strcmp(a->completions[0].name, "views/"));
    CHECK(app_completion_key(a, KEY_ENTER, false) && !strcmp(b->text, "sale/views/") && b->undo.count == 1);
    keyboard_clear(a);
    a->keys[KEY_Z] = true;
    CHECK(app_history_keys(a, b, true, false) && !strcmp(b->text, "sale/"));
    if (a->path_completion.job)
        path_wait(a);
    keyboard_clear(a);
    a->keys[KEY_Y] = true;
    CHECK(app_history_keys(a, b, true, false) && !strcmp(b->text, "sale/views/"));
    keyboard_clear(a);
    app_completion_show(a, true);
    path_wait(a);
    CHECK(a->completion_count == 1 && a->completions[0].kind == 'P');
    CHECK(app_completion_key(a, KEY_TAB, false) && !strcmp(b->text, "sale/views/sale_views.xml") &&
          b->undo.count == 2);
    CHECK(app_xml_begin(a, false) && a->modal == M_XML_IDS && a->xml.job);
    for (int i = 0; i < 5000 && !atomic_load(&a->xml.job->done); i++) {
        struct timespec pause = {0, 1000000};
        nanosleep(&pause, NULL);
    }
    CHECK(atomic_load(&a->xml.job->done));
    app_xml_poll(a);
    CHECK(a->xml.count == 1);
    app_xml_choose(a, 0);
    CHECK(a->modal == M_NONE && strstr(b->text, "<field name=\"arch\" type=\"xml\"></field>") &&
          !strstr(b->text, "<form"));
    CHECK(buf_undo(b) && !strcmp(b->text, "sale/views/sale_views.xml") && buf_redo(b));
    f = fopen(xml, "rb");
    char unchanged[256] = "";
    CHECK(f && fread(unchanged, 1, sizeof(unchanged) - 1, f) == strlen(source) && !fclose(f) &&
          !strcmp(unchanged, source));
    char long_name[256], long_path[BUF_PATH], long_expected[BUF_PATH];
    for (int i = 0; i < 100; i++) {
        long_name[2 * i] = (char)0xc3;
        long_name[2 * i + 1] = (char)0xa9;
    }
    strcpy(long_name + 200, ".xml");
    CHECK(snprintf(long_path, sizeof(long_path), "%s/%s", views, long_name) < BUF_PATH);
    f = fopen(long_path, "wb");
    CHECK(f && !fclose(f));
    buf_free(b);
    CHECK(buf_init(b, "sale/views/é", "draft.xml"));
    b->cursor = b->len;
    app_completion_show(a, true);
    path_wait(a);
    CHECK(a->completion_count == 1 && strlen(a->completions[0].name) < 96 &&
          utf8_valid(a->completions[0].name, strlen(a->completions[0].name)));
    CHECK(snprintf(long_expected, sizeof(long_expected), "sale/views/%s", long_name) < BUF_PATH);
    CHECK(app_completion_key(a, KEY_ENTER, false) && !strcmp(b->text, long_expected) && b->undo.count == 1);
    CHECK(!remove(long_path));
    buf_free(b);
    CHECK(buf_init(b, "sale/", "draft.xml"));
    b->cursor = b->len;
    app_completion_show(a, true);
    CHECK(a->path_completion.job);
    for (int i = 0; i < 5000 && !atomic_load(&a->path_completion.job->done); i++) {
        struct timespec pause = {0, 1000000};
        nanosleep(&pause, NULL);
    }
    CHECK(atomic_load(&a->path_completion.job->done));
    a->xml_paths.count = 0;
    app_completion_poll(a);
    CHECK(!a->completion_count && !a->path_completion.active && !strcmp(b->text, "sale/"));
    app_path_completion_shutdown(a);
    app_xml_shutdown(a);
    buf_free(&a->field);
    buf_free(b);
    free(b);
    free(a);
    CHECK(!remove(xml) && !rmdir(views) && !rmdir(sale) && !rmdir(base));
}
#endif
int main(void) {
#ifndef _WIN32
    project_session_controller();
#endif
    App *a = calloc(1, sizeof(*a));
    CHECK(a);
#ifndef _WIN32
    char resolved[BUF_PATH];
    absolute_path(resolved, sizeof(resolved), "/project/a\\b.py", "/none");
    CHECK(!strcmp(resolved, "/project/a\\b.py"));
    absolute_path(resolved, sizeof(resolved), "next.py", "/project/folder\\");
    CHECK(!strcmp(resolved, "/project/folder\\/next.py"));
    absolute_path(resolved, sizeof(resolved), "../next.py", "/project/folder\\");
    CHECK(!strcmp(resolved, "/project/next.py"));
#endif
    Buffer b;
    CHECK(buf_init(&b, "print(1)", "test.py"));
    strcpy(b.path, "/project/folder/test.py");
    a->tabs[0] = &b;
    a->tab_count = 1;
    a->running = true;
    strcpy(a->project, "/project");
    request_quit(a);
    CHECK(!a->running);
    a->running = true;
    CHECK(buf_type(&b, 'x'));
    request_quit(a);
    CHECK(a->running && a->modal == M_UNSAVED && a->pending == 2 && a->active == 0);
    CHECK(buf_init(&a->snippet_trigger, "xml_draft", "trigger"));
    CHECK(buf_init(&a->snippet_title, "", "title"));
    CHECK(buf_init(&a->snippet_body, "<draft/>", "code"));
    a->snippet_index = -1;
    a->modal = M_SNIPPET_EDIT;
    request_quit(a);
    CHECK(a->running && a->modal == M_SNIPPET_DIRTY && a->snippet_quit);
    close_modal(a);
    CHECK(a->modal == M_SNIPPET_EDIT && !a->snippet_quit && !strcmp(a->snippet_body.text, "<draft/>"));
    close_modal(a);
    CHECK(a->modal == M_SNIPPET_DIRTY && !a->snippet_quit);
    close_modal(a);
    CHECK(a->modal == M_SNIPPET_EDIT);
    buf_free(&a->snippet_trigger);
    buf_free(&a->snippet_title);
    buf_free(&a->snippet_body);
    a->modal = M_NONE;
    a->pending = 0;
    FileJob fake = {0};
    a->file_job = &fake;
    request_quit(a);
    CHECK(a->running && strstr(a->toast, "opération"));
    a->file_job = NULL;
    strcpy(a->context_parent, "/project");
    strcpy(a->context_target, "/project");
    CHECK(app_context_enabled(a, 0) && app_context_enabled(a, 1));
    CHECK(!app_context_enabled(a, 4));
    a->clipboard_files.count = 0;
    CHECK(!app_context_enabled(a, 3));
    a->clipboard_files.count = 1;
    CHECK(app_context_enabled(a, 3));
    strcpy(a->context_target, "/project/folder");
    a->modal = M_FS_DELETE;
    context_delete(a);
    CHECK(!created_jobs && strstr(a->fs_error, "modifiés"));
    buf_free(&b);
    CHECK(buf_init(&b, "print(1)", "test.py"));
    strcpy(b.path, "/project/folder/test.py");
    a->runner.running = true;
    strcpy(a->running_path, b.path);
    context_delete(a);
    CHECK(!created_jobs && strstr(a->fs_error, "programme Python"));
    a->runner.running = false;
    context_delete(a);
    CHECK(created_jobs == 1 && a->modal == M_FS_BUSY && a->file_job);
    free(a->file_job);
    a->file_job = NULL;
    a->modal = M_NONE;

    frame(a, 1440, 900);
    a->topbar = true;
    a->windowed_w = 1200;
    a->windowed_h = 750;
    maximized = fullscreen = false;
    position = (Vector2){100, 100};
    CHECK(window_edges(a, (Vector2){1, 1}) == 5);
    CHECK(window_edges(a, (Vector2){1439, 899}) == 10);
    CHECK(window_cursor(5) == MOUSE_CURSOR_RESIZE_NWSE);
    CHECK(window_cursor(6) == MOUSE_CURSOR_RESIZE_NESW);
    fullscreen = true;
    CHECK(window_edges(a, (Vector2){1, 1}) == 0);
    fullscreen = false;
    mouse_pressed = down = true;
    released = false;
    CHECK(window_gestures(a, (Vector2){50, 30}) && a->window_drag);
    mouse_pressed = false;
    CHECK(window_gestures(a, (Vector2){70, 60}));
    CHECK(position.x == 120 && position.y == 130);
    down = false;
    released = true;
    CHECK(window_gestures(a, (Vector2){50, 30}));
    CHECK(!a->window_drag);
    released = false;
    /* Alt-click/code region is not a window drag gesture. */
    clock_value += 1;
    mouse_pressed = down = true;
    CHECK(!window_gestures(a, (Vector2){500, 400}));
    /* Frameless corner resize respects the minimum, preserving the opposite edge. */
    CHECK(window_gestures(a, (Vector2){1, 1}));
    CHECK(a->window_resize == 5);
    mouse_pressed = false;
    CHECK(window_gestures(a, (Vector2){10001, 10001}));
    CHECK(width == 960 && height == 640);
    down = false;
    released = true;
    CHECK(window_gestures(a, (Vector2){1, 1}));
    CHECK(!a->window_resize);
    released = false;
    /* A plain click on a maximized header does not immediately restore it. */
    frame(a, 1920, 1040);
    position = (Vector2){0, 0};
    maximized = true;
    clock_value += 1;
    mouse_pressed = down = true;
    CHECK(window_gestures(a, (Vector2){60, 30}));
    mouse_pressed = false;
    CHECK(window_gestures(a, (Vector2){61, 31}) && maximized);
    CHECK(window_gestures(a, (Vector2){80, 50}) && !maximized);
    CHECK(width == 1200 && height == 750);
    down = false;
    released = true;
    CHECK(window_gestures(a, (Vector2){50, 30}));
    released = false;
    /* Double-click on the header toggles maximization. */
    frame(a, 1200, 750);
    clock_value += 1;
    mouse_pressed = down = true;
    CHECK(window_gestures(a, (Vector2){60, 30}));
    mouse_pressed = false;
    down = false;
    released = true;
    CHECK(window_gestures(a, (Vector2){60, 30}));
    released = false;
    clock_value += .1;
    mouse_pressed = down = true;
    CHECK(window_gestures(a, (Vector2){60, 30}) && maximized);
    /* Footer drag is available without the menu. Quitter still consumes its hit. */
    maximized = false;
    a->window_drag = false;
    a->window_resize = 0;
    a->topbar = false;
    frame(a, 1200, 750);
    clock_value += 1;
    mouse_pressed = down = true;
    released = false;
    CHECK(window_gestures(a, (Vector2){600, 732}) && a->window_drag);
    mouse_pressed = down = false;
    released = true;
    CHECK(window_gestures(a, (Vector2){600, 732}) && !a->window_drag);
    released = false;
    mouse_pressed = down = true;
    blocked_hit = true;
    clock_value += 1;
    CHECK(!window_gestures(a, (Vector2){1140, 732}) && !a->window_drag);
    blocked_hit = false;
    /* Actual analysis controller: per-document validity, navigation and busy guards. */
    lint_init(&a->linter);
    a->active = 0;
    a->active_pane = 0;
    a->panes[0].tab = 0;
    CHECK(app_diagnostics(a) == NULL);
    LintDocument *doc = analysis_document(a, 0, true);
    CHECK(doc);
    lint_observe(doc, b.version, b.path, a->project, runner_clock());
    doc->valid = true;
    doc->analyzed_version = b.version;
    doc->report.backend = LINT_BACKEND_PYLINT;
    doc->report.count = 2;
    doc->report.items[0] = (Diagnostic){.column = 0, .end_column = 5};
    doc->report.items[1] = (Diagnostic){.column = 6, .end_column = 7};
    CHECK(app_diagnostics(a) == &doc->report && doc->report.resolved);
    diagnostic_goto(a, 0);
    CHECK(b.cursor == 0 && a->console_mode == 1 && a->terminal && visible_requests == 1);
    unsigned long version = b.version;
    diagnostic_next(a, false);
    CHECK(b.cursor == 6);
    diagnostic_next(a, false);
    CHECK(b.cursor == 0);
    diagnostic_next(a, true);
    CHECK(b.cursor == 6 && b.version == version);
    CHECK(buf_type(&b, 'x'));
    CHECK(app_diagnostics(a) == NULL);
    request_analysis(a);
    CHECK(a->lint_force == doc && doc->waiting && !doc->valid && a->console_mode == 1);
    a->lint_auto = false;
    a->file_job = &fake;
    analysis_update(a, runner_clock());
    CHECK(!a->linter.process.running && doc->waiting);
    a->file_job = NULL;
    a->lint_force = NULL;
    analysis_update(a, runner_clock() + 2);
    CHECK(!a->linter.process.running);
    lint_observe(doc, b.version, "/project/save-as.py", a->project, runner_clock());
    doc->valid = true;
    doc->analyzed_version = b.version;
    CHECK(app_diagnostics(a) == NULL);
    a->lint_force = doc;
    analysis_release(a, 0);
    CHECK(!a->analyses[0] && !a->lint_force);
    /* Folder changes invalidate all cache owners, without editing user text. */
    a->analyses[0] = calloc(1, sizeof(LintDocument));
    a->analyses[23] = calloc(1, sizeof(LintDocument));
    CHECK(a->analyses[0] && a->analyses[23]);
    a->lint_force = a->analyses[0];
    a->linter.owner = a->analyses[0];
    a->lint_scroll = 80;
    analysis_reset(a);
    CHECK(!a->analyses[0] && !a->analyses[23] && !a->lint_force && !a->linter.owner && a->lint_scroll == 0);
    char directory[BUF_PATH], profile[BUF_PATH], initial_project[BUF_PATH];
#ifdef _WIN32
    char temp[MAX_PATH];
    CHECK(GetTempPathA(MAX_PATH, temp));
    CHECK(GetTempFileNameA(temp, "ctl", 0, directory));
    CHECK(!remove(directory));
    CHECK(!_mkdir(directory));
#else
    strcpy(directory, "/tmp/prisme-controls-imports-XXXXXX");
    CHECK(mkdtemp(directory));
#endif
    CHECK(snprintf(a->config, sizeof(a->config), "%s/settings.cfg", directory) < BUF_PATH);
    strcpy(initial_project, a->project);
    CHECK(add_import(a, directory));
    CHECK(a->linter.imports.count == 1 && !strcmp(a->linter.imports.paths[0], directory) && a->lint_force &&
          a->lint_force->waiting);
    CHECK(b.version == version + 1); /* the only edit was the earlier buf_type. */
    CHECK(import_paths_file(profile, sizeof(profile), a->config, a->project));
    CHECK(!add_import(a, directory) && a->linter.imports.count == 1);
    CHECK(!add_import(a, "/no-such-prisme-import-directory") && a->linter.imports.count == 1);
    a->modal = M_BROWSER;
    a->browser_mode = 3;
    a->focus = 1;
    close_modal(a);
    CHECK(a->modal == M_IMPORTS && !a->focus && a->linter.imports.count == 1);
    close_modal(a);
    CHECK(a->modal == M_NONE);
    strcpy(a->project, "/different-project");
    load_project_imports(a);
    CHECK(!a->linter.imports.count && !a->lint_force && !a->analyses[0]);
    strcpy(a->project, initial_project);
    load_project_imports(a);
    CHECK(a->linter.imports.count == 1 && !strcmp(a->linter.imports.paths[0], directory));
    char config[BUF_PATH];
    strcpy(config, a->config);
    CHECK(snprintf(a->config, sizeof(a->config), "%s/missing/deeper/settings.cfg", directory) < BUF_PATH);
    ImportPaths empty = {0};
    CHECK(!commit_imports(a, &empty) && a->linter.imports.count == 1);
    strcpy(a->config, config);
    CHECK(commit_imports(a, &empty) && !a->linter.imports.count && a->lint_force);
    load_project_imports(a);
    CHECK(!a->linter.imports.count && !a->analyses[0]);
    CHECK(add_xml_path(a, directory));
    CHECK(a->xml_paths.count == 1 && !a->linter.imports.count && !a->lint_force);
    char xml_profile[BUF_PATH];
    CHECK(xml_paths_file(xml_profile, sizeof(xml_profile), a->config, a->project));
    CHECK(strcmp(xml_profile, profile) && !add_xml_path(a, directory) && a->xml_paths.count == 1);
    CHECK(!add_xml_path(a, "/no-such-xml-directory") && a->xml_paths.count == 1);
    a->modal = M_BROWSER;
    a->browser_mode = 4;
    close_modal(a);
    CHECK(a->modal == M_XML_PATHS);
    strcpy(a->project, "/another-xml-project");
    load_project_imports(a);
    CHECK(!a->xml_paths.count);
    strcpy(a->project, initial_project);
    load_project_imports(a);
    CHECK(a->xml_paths.count == 1 && !strcmp(a->xml_paths.paths[0], directory) && !a->linter.imports.count);
    CHECK(snprintf(a->config, sizeof(a->config), "%s/missing/deeper/settings.cfg", directory) < BUF_PATH);
    XmlPaths no_xml = {0};
    CHECK(!commit_xml_paths(a, &no_xml) && a->xml_paths.count == 1);
    strcpy(a->config, config);
    CHECK(commit_xml_paths(a, &no_xml) && !a->xml_paths.count);
    CHECK(!remove(xml_profile));
    Buffer history = {0};
    CHECK(buf_init(&history, "<form", "view.xml"));
    history.cursor = history.len;
    CHECK(buf_type(&history, '>') && !strcmp(history.text, "<form></form>"));
    keyboard_clear(a);
    a->keys[KEY_Z] = true;
    CHECK(app_history_keys(a, &history, true, false));
    CHECK(!strcmp(history.text, "<form"));
    keyboard_clear(a);
    a->keys[KEY_Y] = true;
    CHECK(app_history_keys(a, &history, true, false));
    CHECK(!strcmp(history.text, "<form></form>") && history.cursor == 6);
    keyboard_clear(a);
    a->keys[KEY_Z] = true;
    field_keys(a, &history, NULL, 0, true, false);
    CHECK(!strcmp(history.text, "<form"));
    keyboard_clear(a);
    a->keys[KEY_Y] = true;
    field_keys(a, &history, NULL, 0, true, false);
    CHECK(!strcmp(history.text, "<form></form>"));
    keyboard_clear(a);
    a->keys[KEY_Z] = true;
    CHECK(app_history_keys(a, &history, true, false));
    CHECK(app_history_keys(a, &history, true, true) && !strcmp(history.text, "<form></form>"));
    CHECK(!app_history_keys(a, &history, false, false));
    keyboard_clear(a);
    buf_free(&history);
    CHECK(!remove(profile));
#ifdef _WIN32
    CHECK(!_rmdir(directory));
#else
    CHECK(!rmdir(directory));
#endif
    keyboard_multicursor();
#ifndef _WIN32
    ssh_controllers();
#endif
    a->active = -1;
    CHECK(app_diagnostics(a) == NULL);
    a->active = 0;
    lint_close(&a->linter);
    buf_free(&b);
    free(a);
    search_frame_controller();
    keyboard_frame_fixture();
    lint_rules_frame_fixture();
    lint_quick_frame_fixture();
    browser_odoo_frame_fixture();
    minimize_frame_fixture();
#ifndef _WIN32
    folder_search_progress_controller();
    folder_search_controller();
    directory_completion_controller();
#endif
    printf("OK %d checks: dirty-document Quit, busy guard, deletion safety, context actions, frameless "
           "move/resize/maximize, analysis cache/navigation/manual/disabled/busy guards\n",
           checks);
    return 0;
}
