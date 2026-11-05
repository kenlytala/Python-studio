#ifndef PRISME_APP_INTERNAL_H
#define PRISME_APP_INTERNAL_H
#include "app.h"
#include "buffer.h"
#include "ssh.h"
#include "session.h"
#include "completion.h"
#include "snippets.h"
#include "layout.h"
#include "keyboard.h"
#include "linter.h"
#include "file_job.h"
#include "xml_refs.h"
#include "xml_paths.h"
#include "xml_edit.h"
#include "path_complete.h"
#include "browser_complete.h"
#include "folder_search.h"
#include "runner.h"
#include "syntax.h"
#include "ui.h"

#define APP_TABS 24
#define APP_FILES 512
#define APP_CONSOLE 2000

typedef enum {
    A_NONE,
    A_NEW,
    A_OPEN,
    A_FOLDER,
    A_SAVE,
    A_SAVE_AS,
    A_RUN,
    A_STOP,
    A_EFFECTS,
    A_SETTINGS,
    A_HELP,
    A_COMMANDS,
    A_QUICK_OPEN,
    A_FIND,
    A_FOLDER_SEARCH,
    A_FOLDER_SEARCH_START,
    A_FOLDER_SEARCH_STOP,
    A_FOLDER_SEARCH_PICK_ROOT,
    A_FOLDER_SEARCH_FOCUS,
    A_FOLDER_SEARCH_FILE,
    A_FOLDER_SEARCH_NEXT,
    A_FOLDER_SEARCH_PREV,
    A_FOLDER_SEARCH_OPEN,
    A_FOLDER_SEARCH_OPTION,
    A_FOLDER_SEARCH_FILTERS,
    A_FOLDER_SEARCH_MAXIMIZE,
    A_FOLDER_SEARCH_FULLSCREEN,
    A_FIND_CLOSE,
    A_FIND_NEXT,
    A_FIND_PREV,
    A_GOTO,
    A_SIDEBAR,
    A_TERMINAL,
    A_CONSOLE_CLEAR,
    A_CONSOLE_EXPAND,
    A_CONSOLE_TAB,
    A_TAB,
    A_TAB_CLOSE,
    A_TREE,
    A_TREE_ROOT,
    A_MODAL_CLOSE,
    A_BROWSER_UP,
    A_BROWSER_ENTRY,
    A_BROWSER_SUBMIT,
    A_UNSAVED_SAVE,
    A_UNSAVED_DISCARD,
    A_OVERWRITE,
    A_PALETTE_CHOOSE,
    A_PALETTE_FILE,
    A_GOTO_SUBMIT,
    A_MINIMAP,
    A_MINIMAP_TOGGLE,
    A_FONT_UP,
    A_FONT_DOWN,
    A_COMPLETION,
    A_FIELD_FIND,
    A_FIELD_CONSOLE,
    A_SPLIT,
    A_PANE_FOCUS,
    A_FULLSCREEN,
    A_MINIMIZE,
    A_SELECT_NEXT,
    A_SELECT_OCCURRENCES,
    A_SINGLE_CARET,
    A_TOPBAR,
    A_ZEN,
    A_QUIT,
    A_SESSION_KEEP_QUIT,
    A_FS_NEW_FILE,
    A_FS_NEW_FOLDER,
    A_FS_COPY,
    A_FS_PASTE,
    A_FS_DELETE,
    A_FS_CREATE,
    A_FS_CONFIRM_DELETE,
    A_FS_CANCEL,
    A_LINT_RUN,
    A_PROBLEMS,
    A_DIAGNOSTIC,
    A_DIAG_NEXT,
    A_DIAG_PREV,
    A_LINT_AUTO,
    A_LINT_RULES,
    A_LINT_RULES_APPLY,
    A_LINT_RULES_CLEAR,
    A_LINT_IGNORE,
    A_ODOO_SETTINGS,
    A_ODOO_TOGGLE,
    A_ODOO_APPLY,
    A_ODOO_CLEAR,
    A_IMPORTS,
    A_IMPORT_ADD,
    A_IMPORT_REMOVE,
    A_COMPLETION_REFRESH,
    A_SNIPPETS,
    A_SNIPPET_NEW,
    A_SNIPPET_EDIT,
    A_SNIPPET_DELETE,
    A_SNIPPET_CONFIRM_DELETE,
    A_SNIPPET_SAVE,
    A_SNIPPET_DISCARD,
    A_SNIPPET_CONTINUE,
    A_SNIPPET_FOCUS,
    A_UNDO,
    A_REDO,
    A_XML_PATHS,
    A_XML_PATH_ADD,
    A_XML_PATH_REMOVE,
    A_XML_PATH_UP,
    A_XML_PATH_DOWN,
    A_XML_AUTO,
    A_XML_IDS,
    A_XML_PICK,
    A_SSH,
    A_SSH_CONNECT,
    A_SSH_DISCONNECT,
    A_SSH_RECONNECT,
    A_SSH_CANCEL,
    A_SSH_FIELD,
    A_SSH_PROFILES,
    A_SSH_NEW,
    A_SSH_SAVE_PROFILE,
    A_SSH_PROFILE_PICK,
    A_SSH_PROFILE_REMOVE,
    A_SSH_AUTH_YES,
    A_SSH_AUTH_NO,
    A_SSH_AUTH_SEND
} Action;
typedef enum {
    M_NONE,
    M_BROWSER,
    M_PALETTE,
    M_SETTINGS,
    M_LINT_RULES,
    M_ODOO,
    M_HELP,
    M_GOTO,
    M_UNSAVED,
    M_OVERWRITE,
    M_CONTEXT,
    M_FS_CREATE,
    M_FS_DELETE,
    M_FS_BUSY,
    M_IMPORTS,
    M_SNIPPETS,
    M_SNIPPET_EDIT,
    M_SNIPPET_DELETE,
    M_SNIPPET_DIRTY,
    M_XML_PATHS,
    M_XML_IDS,
    M_FOLDER_SEARCH,
    M_SSH,
    M_SSH_PROFILES,
    M_SSH_AUTH
} Modal;
typedef struct {
    char path[BUF_PATH], name[256];
    int depth;
    bool directory;
} Entry;
typedef struct {
    char text[BUF_PATH], path[BUF_PATH], hint[40];
    Action action;
    int arg;
    Icon icon;
} PaletteItem;
typedef struct {
    char *text;
    Tone tone;
} ConsoleLine;
typedef struct {
    size_t start, end;
} Match;
typedef struct {
    float x, y, vx, vy, life;
} Particle;

typedef struct {
    int tab;
    uint8_t *colors;
    size_t capacity;
    Buffer *lex_buffer;
    unsigned long lex_version;
    SyntaxLanguage lex_language;
} EditorPane;

typedef struct {
    const SshSession *owner;
    bool initialized;
    long generation;
    char cache[SSH_PATH], mountpoint[SSH_PATH];
} XmlReadLease;
typedef struct {
    XmlLoadJob *job;
    XmlReadLease *lease;
    XmlReferences refs;
    Buffer *document;
    unsigned long version, filter_version;
    size_t start, end, cursor, anchor;
    int pane, index, count, filtered[XML_REF_IDS_MAX];
    bool visible;
    char *original, project[BUF_PATH], error[512];
} XmlInlineState;

typedef struct {
    PathLoadJob *job;
    PathReport *result;
    PathContext context;
    XmlPaths xml_paths;
    ImportPaths imports;
    XmlReadLease lease;
    SshState ssh_state;
    Buffer *document;
    unsigned long generation;
    bool active;
    char project[BUF_PATH], document_path[BUF_PATH], name[256];
} PathCompletionState;

typedef struct {
    FolderSearchJob *job;
    SearchReport *report;
    XmlReadLease lease;
    SshState ssh_state;
    Buffer folder, query, view, filters[4];
    SearchOptions options;
    bool initialized, active, filters_open, maximized;
    int file, hit, focus;
    unsigned scroll, x, y, rows, columns, list_rows, extent_columns;
    int scroll_drag; /* 0 none, 1 horizontal, 2 vertical. */
    float scroll_grab;
    ScrollAxis horizontal, vertical;
    Rectangle htrack, hthumb, vtrack, vthumb;
    Rectangle list_rect, content_rect, shell_rect;
    char project[BUF_PATH], profile[BUF_PATH], error[512], note[512];
} FolderSearchState;

typedef struct {
    BrowserLoadJob *job;
    XmlReadLease lease;
    unsigned long version;
    size_t cursor, anchor;
    int mode;
    SshState ssh_state;
    bool active, suggesting;
    char base[BUF_PATH], project[BUF_PATH], suggested_folder[BUF_PATH], status[512];
} BrowserCompletionState;

typedef struct {
    UI ui;
    BrowserCompletionState browser_completion;
    FolderSearchState folder_search;
    PathCompletionState path_completion;
    XmlInlineState xml;
    XmlPaths xml_paths;
    char xml_paths_error[512];
    bool xml_auto;
    StudioStore session;
    bool session_ready, session_keep, session_committing_quit, session_signature_valid;
    bool session_discard[APP_TABS];
    uint64_t session_signature;
    double session_next;
    char session_error[512];
    Buffer *tabs[APP_TABS];
    int tab_count, active;
    EditorPane panes[2];
    int active_pane, render_pane, pane_x, pane_right, divider_x;
    bool split, resizing_split, resizing_sidebar;
    bool topbar, zen_mode, zen_sidebar, zen_topbar, zen_terminal;
    int sidebar_width;
    WorkspaceLayout layout;
    float split_ratio;
    Buffer field, search, input;
    SshSession ssh;
    SshProfiles ssh_profiles;
    Buffer ssh_fields[5], ssh_auth;
    char ssh_profiles_path[BUF_PATH], ssh_selected[33], last_local_project[BUF_PATH];
    int ssh_field, ssh_profile_index, ssh_profile_scroll, ssh_prompt_scroll;
    bool ssh_reconnect, ssh_quit_pending;
    Modal ssh_previous_modal;
    unsigned long frame_tick, ssh_auth_frame;

    char assets[BUF_PATH], project[BUF_PATH], project_name[256], python[BUF_PATH], config[BUF_PATH];
    char folder[BUF_PATH], save_target[BUF_PATH];
    Entry tree[APP_FILES], browser[APP_FILES];
    int tree_count, browser_count;
    char expanded[48][BUF_PATH];
    int expanded_count;
    int tree_scroll, browser_scroll, browser_index;
    bool tree_root_open;
    PaletteItem palette[APP_FILES];
    int palette_count, filtered[APP_FILES], filtered_count, palette_index;
    Modal modal;
    char context_target[BUF_PATH], context_parent[BUF_PATH], fs_error[512], running_path[BUF_PATH];
    FsPaths clipboard_files;
    FileJob *file_job;
    Vector2 context_position;
    int context_index;
    bool create_directory;
    bool window_drag;
    int window_resize, windowed_w, windowed_h;
    Vector2 window_press, window_origin, window_size;
    double window_click_time;
    Vector2 window_click;

    int browser_mode, pending;
    bool running, effects, minimap, sidebar, terminal, find_open, demo, demo_started;
    int focus, font_size, console_mode, console_scroll, last_exit;
    bool has_run;
    int w, h, ex, code_y, code_bottom, code_h, code_x, code_right, minimap_w;
    float line_h, char_w, terminal_h, terminal_y;
    unsigned long lex_version, search_version, search_field_version;
    Buffer *lex_buffer, *search_buffer;
    uint8_t *colors;
    size_t colors_capacity;
    Match matches[2048];
    int match_count, match_index;
    Buffer *vertical_document;
    unsigned long vertical_version;
    size_t vertical_cursor, vertical_column;
    int vertical_count, vertical_pane;
    CompletionItem completions[COMPLETION_MAX];
    int completion_count, completion_index;
    bool completion_active, completion_pending, completion_manual;
    size_t completion_start, completion_end, completion_cursor;
    unsigned long completion_version, completion_generation, completion_request_generation;
    char completion_status[384];
    int completion_pane;
    Buffer *completion_document;
    double completion_changed;
    Rectangle completion_rect;
    Completer completer;
    Snippets snippets;
    Buffer snippet_trigger, snippet_title, snippet_body;
    int snippet_index, snippet_scroll, snippet_focus;
    bool snippet_quit, snippet_dragging;
    Rectangle snippet_code_rect;
    char snippet_error[512];
    Linter linter;
    LintDocument *analyses[APP_TABS], *lint_force;
    struct {
        Buffer *buffer;
        LintReport *report;
        unsigned long version, revision;
        uint64_t scope;
        int pane;
        char tips[LINT_MAX_ITEMS][64];
    } lint_quick;
    bool lint_auto;
    char lint_rules_error[512];
    bool odoo_draft_enabled;
    char odoo_error[512];
    int lint_scroll;
    char import_error[512];
    Runner runner;
    ConsoleLine console[APP_CONSOLE];
    int console_count;
    char output_partial[16384];
    size_t output_length;
    Particle particles[128];
    int particle_count;
    Keyboard keyboard;
    bool keys[512], dragging, resizing;
    Vector2 last_click;
    double last_click_time;
    float caret_x, caret_y;
    double caret_time, start_time, tree_time;
    char toast[1024];
    double toast_time;
    bool toast_error;
    char title[512];
} App;
/* Queue entries are logical. Native pressed/repeat/held state is projected
   through the inverse map too, so the raw W position cannot still close a tab. */
static inline bool app_key_pressed(App *a, int key) {
    int source = keyboard_source(&a->keyboard, key);
    return (key > 0 && key < KEYBOARD_KEYS && a->keys[key]) ||
           (source >= 0 && (IsKeyPressed(source) || IsKeyPressedRepeat(source)));
}
static inline bool app_key_held(App *a, int key) {
    int source = keyboard_source(&a->keyboard, key);
    return (key > 0 && key < KEYBOARD_KEYS && a->keys[key]) || (source >= 0 && IsKeyDown(source));
}

bool app_folder_search_shortcut(App *a, bool ctrl, bool shift);
void app_folder_search_option(App *a, int index);
void app_folder_search_maximize(App *a);
void app_folder_search_fullscreen(App *a);
void app_folder_search_restore_window(App *a);
SearchOptions app_folder_search_options(const App *a);
void app_folder_search_show(App *a);
void app_folder_search_start(App *a);
void app_folder_search_cancel(App *a);
void app_folder_search_stop(App *a);
void app_folder_search_poll(App *a);
void app_folder_search_shutdown(App *a);
void app_folder_search_select(App *a, int file, int hit);
void app_folder_search_next(App *a, int direction);
void app_folder_search_picked(App *a, const char *root);
void app_folder_search_keys(App *a, const unsigned *chars, int count, bool ctrl, bool shift);
void app_folder_search_scroll(App *a, Vector2 position, float wheel, bool shift);
bool app_folder_search_mouse(App *a, Vector2 position, bool pressed, bool down, bool released);
void app_folder_search_draw(App *a);
void app_folder_search_browse(App *a);
void app_folder_search_open_editor(App *a);
size_t app_folder_search_line(Buffer *b, unsigned row, unsigned left, unsigned columns, char *out,
                              size_t cap);

bool app_path_completion_begin(App *a);
void app_path_completion_poll(App *a);
void app_path_completion_cancel(App *a);
void app_path_completion_shutdown(App *a);
bool app_path_completion_apply(App *a, int index);

void app_xml_paths_load(App *a);
bool app_xml_begin(App *a, bool required);
void app_xml_poll(App *a);
void app_xml_cancel(App *a);
void app_xml_shutdown(App *a);
int app_xml_filter(App *a);
void app_xml_choose(App *a, int index);
void app_xml_keys(App *a, const unsigned *chars, int count, bool ctrl, bool shift);
void app_session_capture(App *a, StudioImage *image);
bool app_session_checkpoint(App *a, bool force);
bool app_session_restore(App *a);
void app_session_remote_ready(App *a);
void app_session_keep_quit(App *a);
void app_ssh_init(App *a, const char *helper);
void app_ssh_free(App *a);
void app_ssh_action(App *a, Action id, int arg);
void app_ssh_poll(App *a);
bool app_ssh_close_modal(App *a);
bool app_ssh_request_quit(App *a);
void app_ssh_keys(App *a, const unsigned *chars, int count, bool ctrl, bool shift);
void app_ssh_draw(App *a);
Rectangle app_ssh_shell(App *a, const char *title, const char *subtitle, float w, float h);
bool app_history_keys(App *a, Buffer *b, bool ctrl, bool shift);
void app_edit_field(App *a, Buffer *field, const unsigned *chars, int count, bool ctrl, bool shift);
void app_ssh_change_project(App *a, const char *path);
void app_ssh_drop_remote_tabs(App *a);
void app_ssh_reset_analysis(App *a);
void app_browser_completion_begin(App *a);
void app_browser_completion_cancel(App *a);
void app_browser_completion_poll(App *a);
void app_browser_completion_shutdown(App *a);
bool app_browser_completion_accept(App *a, int index);
void app_odoo_load(App *a);
void app_odoo_open(App *a);
void app_odoo_apply(App *a);
void app_odoo_ignore(App *a, const char *code, const char *symbol);
void app_odoo_keys(App *a, const unsigned *chars, int count, bool ctrl, bool shift);
void app_lint_rules_load(App *a);
void app_lint_rules_open(App *a);
void app_lint_rules_apply(App *a);
void app_lint_quick_ignore(App *a, int action);
void app_lint_rules_keys(App *a, const unsigned *chars, int count, bool ctrl, bool shift);
void app_lint_rules_invalidate(App *a);
bool app_path_usable(App *a, const char *path);
Buffer *app_buffer(App *a);
LintReport *app_diagnostics(App *a);
void app_layout(App *a);
void app_view_pane(App *a, int pane);
void app_focus_pane(App *a, int pane);
void app_prepare_syntax(App *a, int pane);
void app_draw(App *a, float dt);
void app_toast(App *a, const char *message, bool error);
void app_set_field(Buffer *field, const char *text, bool select);
void app_action(App *a, Action action, int arg);
void app_minimize_window(App *a);
void app_multicursor_reset(App *a);
/* Consume only Ctrl+Alt+Up/Down in the editor, including queued/repeat events. */
bool app_multicursor_keys(App *a, bool ctrl);
void app_completion_dismiss(App *a);
void app_completion_show(App *a, bool manual);
void app_completion_poll(App *a);
bool app_completion_apply(App *a, int index);
bool app_completion_key(App *a, int key, bool shift);
void app_snippets_load(App *a);
bool app_snippet_dirty(const App *a);
void app_snippet_edit(App *a, int index);
void app_snippet_leave(App *a);
bool app_snippet_save(App *a);
bool app_snippet_delete(App *a);
void app_snippet_keys(App *a, const unsigned *chars, int count, bool ctrl, bool shift);
bool app_snippet_mouse(App *a, float wheel, bool shift);
bool app_context_enabled(App *a, int index);
Action app_context_action(int index);
void app_ensure_visible(App *a);
Vector2 app_caret(App *a, size_t position);
int app_filter_palette(App *a);
void app_update_matches(App *a);
void app_field_draw(App *a, Buffer *b, Rectangle rect, bool focus, const char *placeholder, Action action,
                    bool subtle);
/* Immutable report/document binding shared by the real view and CPU event harness. */
static inline void app_lint_quick_bind(App *a, LintReport *report) {
    Buffer *source = app_buffer(a);
    a->lint_quick.buffer = report ? source : NULL;
    a->lint_quick.report = report;
    a->lint_quick.version = source->version;
    a->lint_quick.revision = report ? report->revision : 0;
    a->lint_quick.pane = a->active_pane;
    a->lint_quick.scope = lint_rules_scope(a->project, source->path, source->name);
}
#endif
