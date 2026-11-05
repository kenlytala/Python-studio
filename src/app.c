#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L
#include "app_internal.h"
#include "graphics.h"
#include "file_clipboard.h"
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#endif

static void action(App *a, Action id, int arg);
static void refresh_tree(App *a);
static void request_quit(App *a);
static void finish_pending(App *a);

static void strset(char *out, size_t cap, const char *text) {
    if (!cap)
        return;
    if (!text)
        text = "";
    size_t n = strlen(text);
    if (n >= cap)
        n = cap - 1;
    memmove(out, text, n);
    out[n] = 0;
}
/* Jointure bornée : ne transforme jamais silencieusement un chemin trop long. */
static bool path_join(char *out, size_t capacity, const char *base, const char *leaf) {
    size_t n = strlen(base), m = strlen(leaf);
    bool separator = n && base[n - 1] != '/';
    if (n + m + (separator ? 1u : 0u) + 1u > capacity) {
        if (capacity)
            out[0] = 0;
        return false;
    }
    memmove(out, base, n);
    if (separator)
        out[n++] = '/';
    memcpy(out + n, leaf, m + 1);
    return true;
}
static bool suffix(const char *s, const char *end) {
    size_t a = strlen(s), b = strlen(end);
    return a >= b && !strcmp(s + a - b, end);
}
static bool is_python(Buffer *b) {
    return suffix(b->name, ".py") || suffix(b->name, ".pyw");
}
static bool is_analysis(Buffer *b) {
    SyntaxLanguage language = syntax_language(b->name);
    return language == SYNTAX_PYTHON || language == SYNTAX_XML;
}
static bool contains_ci(const char *text, const char *q) {
    if (!*q)
        return true;
    for (size_t i = 0; text[i]; i++) {
        size_t j = 0;
        while (q[j] && text[i + j] && tolower((unsigned char)q[j]) == tolower((unsigned char)text[i + j]))
            j++;
        if (!q[j])
            return true;
    }
    return false;
}
static void absolute_path(char *out, size_t cap, const char *path, const char *base) {
    char raw[BUF_PATH], normalized[BUF_PATH];
    bool absolute = path[0] == '/' || (isalpha((unsigned char)path[0]) && path[1] == ':');
    if (absolute)
        strset(raw, sizeof(raw), path);
    else if (!path_join(raw, sizeof(raw), base, path)) {
        out[0] = 0;
        return;
    }
#ifdef _WIN32
    for (size_t i = 0; raw[i]; i++)
        if (raw[i] == '\\')
            raw[i] = '/';
#endif
    size_t used = 0, positions[512];
    int count = 0;
    const char *p = raw;
    if (isalpha((unsigned char)p[0]) && p[1] == ':') {
        normalized[used++] = p[0];
        normalized[used++] = ':';
        p += 2;
    }
    if (*p == '/') {
        normalized[used++] = '/';
        while (*p == '/')
            p++;
    }
    while (*p) {
        const char *start = p;
        while (*p && *p != '/')
            p++;
        size_t len = (size_t)(p - start);
        while (*p == '/')
            p++;
        if (len == 1 && start[0] == '.')
            continue;
        if (len == 2 && start[0] == '.' && start[1] == '.') {
            if (count)
                used = positions[--count];
            continue;
        }
        if (used + len + 2 >= sizeof(normalized) || count == 512)
            break;
        positions[count++] = used;
        if (used && normalized[used - 1] != '/')
            normalized[used++] = '/';
        memcpy(normalized + used, start, len);
        used += len;
    }
    if (!used)
        normalized[used++] = '/';
    normalized[used] = 0;
    strset(out, cap, normalized);
}
static const char *native_parent(const char *path) {
    static char parent[BUF_PATH];
    if (!fs_parent(path, parent))
        strset(parent, sizeof(parent), ".");
    return parent;
}
Buffer *app_buffer(App *a) {
    return a->tabs[a->active];
}
void app_toast(App *a, const char *s, bool error) {
    strset(a->toast, sizeof(a->toast), s);
    a->toast_time = GetTime();
    a->toast_error = error;
}
void app_set_field(Buffer *field, const char *text, bool select) {
    unsigned long version = field->version;
    buf_free(field);
    buf_init(field, text, "champ");
    field->version = version + 1;
    field->cursor = field->len;
    if (select)
        field->anchor = 0;
}
static void close_modal(App *a) {
    if (a->modal == M_BROWSER)
        app_browser_completion_cancel(a);
    bool cancel_session_quit = a->modal == M_UNSAVED && a->pending == 2;
    if (a->modal == M_LINT_RULES || a->modal == M_ODOO) {
        a->modal = M_SETTINGS;
        return;
    }
    if (a->modal == M_FOLDER_SEARCH) {
        app_folder_search_restore_window(a);
        app_folder_search_cancel(a);
        a->modal = M_NONE;
        a->focus = 0;
        return;
    }
    if (a->modal == M_BROWSER && a->browser_mode == 5) {
        app_folder_search_show(a);
        return;
    }
    if (app_ssh_close_modal(a))
        return;
    if (a->modal == M_XML_IDS) {
        app_xml_cancel(a);
        return;
    }
    if (a->modal == M_FS_BUSY && a->file_job && !atomic_load(&a->file_job->done)) {
        if (!a->file_job->deleting)
            atomic_store(&a->file_job->cancel, true);
        return;
    }
    if (a->modal == M_SNIPPET_EDIT) {
        app_snippet_leave(a);
        return;
    }
    if (a->modal == M_SNIPPET_DIRTY || a->modal == M_SNIPPET_DELETE) {
        a->modal = a->modal == M_SNIPPET_DIRTY ? M_SNIPPET_EDIT : M_SNIPPETS;
        a->snippet_quit = false;
        return;
    }
    bool imports = a->modal == M_BROWSER && a->browser_mode == 3;
    bool xml_roots = a->modal == M_BROWSER && a->browser_mode == 4;
    a->modal = imports ? M_IMPORTS : xml_roots ? M_XML_PATHS : M_NONE;
    if (cancel_session_quit) {
        memset(a->session_discard, 0, sizeof(a->session_discard));
        a->session_keep = false;
        a->session_committing_quit = false;
    }
    a->focus = 0;
    a->pending = 0;
}
static void invalidate(App *a) {
    app_multicursor_reset(a);
    a->lex_buffer = NULL;
    a->panes[0].lex_buffer = a->panes[1].lex_buffer = NULL;
    a->search_buffer = NULL;
    app_completion_dismiss(a);
    a->caret_x = -1;
}
static void pane_geometry(App *a, int pane) {
    int width = a->w - a->ex;
    float ratio = a->split_ratio > 0 ? a->split_ratio : 0.5f;
    int min_width = width / 2 < 260 ? width / 2 : 260;
    a->divider_x = a->ex + (int)((float)width * ratio);
    if (a->divider_x < a->ex + min_width)
        a->divider_x = a->ex + min_width;
    if (a->divider_x > a->w - min_width)
        a->divider_x = a->w - min_width;
    a->pane_x = a->split && pane == 1 ? a->divider_x + 3 : a->ex;
    a->pane_right = a->split && pane == 0 ? a->divider_x - 3 : a->w;
    a->code_x = a->pane_x + 76;
    a->minimap_w = a->minimap && a->pane_right - a->pane_x > 760 ? 91 : 15;
    a->code_right = a->pane_right - a->minimap_w - 14;
}
void app_focus_pane(App *a, int pane) {
    if (pane < 0 || pane > 1 || (pane && !a->split))
        return;
    a->panes[a->active_pane].tab = a->active;
    a->active_pane = pane;
    a->active = a->panes[pane].tab;
    a->focus = 0;
    a->dragging = false;
    app_completion_dismiss(a);
    a->search_buffer = NULL;
    a->particle_count = 0;
    a->caret_x = -1;
    a->caret_time = GetTime();
    app_layout(a);
}
void app_prepare_syntax(App *a, int pane) {
    EditorPane *view = &a->panes[pane];
    Buffer *b = app_buffer(a);
    buf_index(b);
    SyntaxLanguage language = syntax_language(b->name);
    if (view->lex_buffer != b || view->lex_version != b->version || view->lex_language != language) {
        if (b->len + 1 > view->capacity) {
            uint8_t *colors = realloc(view->colors, b->len + 1);
            if (colors) {
                view->colors = colors;
                view->capacity = b->len + 1;
            }
        }
        if (view->capacity >= b->len + 1) {
            syntax_colorize_language(b->text, b->len, view->colors, language);
            view->lex_language = language;
            view->lex_buffer = b;
            view->lex_version = b->version;
        }
    }
    a->colors = view->colors;
    a->colors_capacity = view->capacity;
}
void app_view_pane(App *a, int pane) {
    a->render_pane = pane;
    a->active = a->panes[pane].tab;
    pane_geometry(a, pane);
    app_prepare_syntax(a, pane);
}
void app_layout(App *a) {
    a->w = GetScreenWidth();
    a->h = GetScreenHeight();
    if (!IsWindowFullscreen() && !IsWindowMaximized() && !a->window_resize && a->w >= 960 && a->h >= 640) {
        a->windowed_w = a->w;
        a->windowed_h = a->h;
    }
    a->layout = layout_workspace(a->w, a->h, a->sidebar, a->topbar, a->terminal, a->find_open,
                                 a->sidebar_width, (int)a->terminal_h);
    a->tree_scroll = layout_tree_scroll(&a->layout, a->tree_count, a->tree_scroll);
    a->ex = a->layout.sidebar_w;
    a->code_y = a->layout.code_y;
    a->terminal_y = (float)a->layout.console_y;
    a->code_bottom = a->layout.code_bottom;
    a->code_h = a->layout.code_h;
    a->panes[a->active_pane].tab = a->active;
    a->render_pane = a->active_pane;
    pane_geometry(a, a->active_pane);
    a->line_h = roundf((float)a->font_size * 1.5f);
    a->char_w = ui_width(&a->ui, "M", (float)a->font_size, true);
}
void app_ensure_visible(App *a) {
    Buffer *b = app_buffer(a);
    float y = (float)buf_row(b, b->cursor) * a->line_h;
    if (y < b->scroll_y + a->line_h)
        b->scroll_y = fmaxf(0, y - a->line_h);
    if (y > b->scroll_y + (float)a->code_h - a->line_h * 2)
        b->scroll_y = fmaxf(0, y - (float)a->code_h + a->line_h * 2);
    float x = (float)buf_column(b, b->cursor) * a->char_w;
    if (x < b->scroll_x)
        b->scroll_x = fmaxf(0, x - a->char_w * 4);
    if (x > b->scroll_x + (float)(a->code_right - a->code_x - 30))
        b->scroll_x = fmaxf(0, x - (float)(a->code_right - a->code_x) + 60);
    a->caret_time = GetTime();
}
Vector2 app_caret(App *a, size_t p) {
    Buffer *b = app_buffer(a);
    return (Vector2){(float)a->code_x + (float)buf_column(b, p) * a->char_w - b->scroll_x,
                     (float)a->code_y + 13 + (float)buf_row(b, p) * a->line_h - b->scroll_y};
}
/* Per-document caches, one independent, asynchronous analysis process. */
static bool analysis_filename(App *a, int index, char *out) {
    if (index < 0 || index >= a->tab_count || !is_analysis(a->tabs[index]))
        return false;
    Buffer *b = a->tabs[index];
    if (*b->path)
        strset(out, BUF_PATH, b->path);
    else if (!path_join(out, BUF_PATH, a->project, b->name))
        return false;
    char access_error[512];
    return ssh_guard(&a->ssh, out, false, access_error, sizeof(access_error));
}
static LintDocument *analysis_document(App *a, int index, bool create) {
    if (index < 0 || index >= a->tab_count || !is_analysis(a->tabs[index]))
        return NULL;
    if (create && !a->analyses[index])
        a->analyses[index] = calloc(1, sizeof(LintDocument));
    return a->analyses[index];
}
LintReport *app_diagnostics(App *a) {
    if (a->active < 0 || a->active >= a->tab_count)
        return NULL;
    LintDocument *doc = analysis_document(a, a->active, false);
    Buffer *b = app_buffer(a);
    char filename[BUF_PATH];
    if (!doc || !doc->valid || doc->analyzed_version != b->version ||
        !analysis_filename(a, a->active, filename) || strcmp(doc->filename, filename) ||
        strcmp(doc->project, a->project))
        return NULL;
    lint_resolve(&doc->report, b);
    return &doc->report;
}
static void analysis_release(App *a, int index) {
    if (a->lint_quick.buffer == a->tabs[index]) {
        a->lint_quick.buffer = NULL;
        a->lint_quick.report = NULL;
    }
    LintDocument *doc = a->analyses[index];
    if (!doc)
        return;
    if (a->lint_force == doc)
        a->lint_force = NULL;
    lint_forget(&a->linter, doc);
    free(doc);
    a->analyses[index] = NULL;
}
static void analysis_reset(App *a) {
    a->lint_quick.buffer = NULL;
    a->lint_quick.report = NULL;
    for (int i = 0; i < APP_TABS; i++)
        analysis_release(a, i);
    a->lint_force = NULL;
    a->lint_scroll = 0;
}
void app_lint_rules_invalidate(App *a) {
    analysis_reset(a);
    LintDocument *doc = analysis_document(a, a->active, true);
    char filename[BUF_PATH];
    double now = runner_clock();
    if (doc && analysis_filename(a, a->active, filename)) {
        lint_observe(doc, app_buffer(a)->version, filename, a->project, now);
        doc->changed_at = now - LINT_DEBOUNCE;
        a->lint_force = doc;
    }
}
static void analysis_update(App *a, double now) {
    for (int i = 0; i < a->tab_count; i++) {
        if (!is_analysis(a->tabs[i])) {
            analysis_release(a, i);
            continue;
        }
        bool visible = i == a->active || (a->split && (i == a->panes[0].tab || i == a->panes[1].tab));
        LintDocument *doc = analysis_document(a, i, visible);
        char filename[BUF_PATH];
        if (doc && analysis_filename(a, i, filename))
            lint_observe(doc, a->tabs[i]->version, filename, a->project, now);
    }
    lint_poll(&a->linter, now);
    char access_error[512];
    if (a->linter.process.running || a->file_job ||
        !ssh_guard(&a->ssh, a->project, false, access_error, sizeof(access_error)))
        return;
    int order[3] = {a->active, a->split ? a->panes[1 - a->active_pane].tab : -1, -1};
    if (a->lint_force)
        for (int i = 0; i < a->tab_count; i++)
            if (a->analyses[i] == a->lint_force) {
                order[2] = order[1];
                order[1] = order[0];
                order[0] = i;
                break;
            }
    for (int n = 0; n < 3; n++) {
        int i = order[n];
        LintDocument *doc = analysis_document(a, i, false);
        if (!doc || (!a->lint_auto && a->lint_force != doc) || !lint_ready(doc, now))
            continue;
        char error[512] = {0};
        bool ok = lint_start(&a->linter, doc, a->python, a->tabs[i]->text, error, sizeof(error));
        if (a->lint_force == doc)
            a->lint_force = NULL;
        if (!ok)
            app_toast(a, error, true);
        break;
    }
}
static void show_problems(App *a) {
    a->zen_mode = false;
    a->terminal = true;
    a->console_mode = 1;
    a->focus = 0;
    app_completion_dismiss(a);
    app_layout(a);
}
static void request_analysis(App *a) {
    LintDocument *doc = analysis_document(a, a->active, true);
    if (!doc) {
        app_toast(a, "L’analyse concerne les fichiers Python et XML.", false);
        return;
    }
    double now = runner_clock();
    char filename[BUF_PATH];
    if (!analysis_filename(a, a->active, filename))
        return;
    lint_observe(doc, app_buffer(a)->version, filename, a->project, now);
    if (a->linter.process.running && a->linter.owner)
        lint_forget(&a->linter, a->linter.owner);
    doc->valid = false;
    doc->waiting = true;
    doc->changed_at = now - LINT_DEBOUNCE;
    a->lint_force = doc;
    a->lint_scroll = 0;
    show_problems(a);
}
static void diagnostic_goto(App *a, int arg) {
    if (arg < 0 || arg >= LINT_MAX_ITEMS * 2)
        return;
    int pane = arg / LINT_MAX_ITEMS, index = arg % LINT_MAX_ITEMS;
    if (pane && !a->split)
        return;
    if (pane != a->active_pane)
        app_focus_pane(a, pane);
    LintReport *report = app_diagnostics(a);
    if (!report || index >= report->count)
        return;
    Buffer *b = app_buffer(a);
    buf_clear_carets(b);
    buf_move(b, report->items[index].start, false);
    a->dragging = false;
    app_completion_dismiss(a);
    a->lint_scroll = index;
    show_problems(a);
    app_ensure_visible(a);
}
static void diagnostic_next(App *a, bool backwards) {
    LintReport *report = app_diagnostics(a);
    if (!report || !report->count) {
        show_problems(a);
        return;
    }
    size_t p = app_buffer(a)->cursor;
    int index = backwards ? report->count - 1 : 0;
    if (backwards) {
        for (int i = report->count - 1; i >= 0; i--)
            if (report->items[i].start < p) {
                index = i;
                break;
            }
    } else
        for (int i = 0; i < report->count; i++)
            if (report->items[i].start > p) {
                index = i;
                break;
            }
    diagnostic_goto(a, a->active_pane * LINT_MAX_ITEMS + index);
}

static bool open_file(App *a, const char *path, bool quiet) {
    char full[BUF_PATH], error[512] = "";
    absolute_path(full, sizeof(full), path, a->project);
    if (!app_path_usable(a, full))
        return false;
#ifndef _WIN32
    /* Store the actual mounted path, not a local symlink alias to it. */
    char actual[BUF_PATH];
    if (realpath(full, actual))
        strset(full, sizeof(full), actual);
    if (!app_path_usable(a, full))
        return false;
#endif
    for (int i = 0; i < a->tab_count; i++)
        if (!strcmp(a->tabs[i]->path, full)) {
            a->active = i;
            close_modal(a);
            invalidate(a);
            return true;
        }
    if (a->tab_count == APP_TABS) {
        app_toast(a, "Limite de 24 onglets atteinte.", true);
        return false;
    }
    Buffer *b = calloc(1, sizeof(*b));
    if (!b)
        return false;
    if (!buf_load(b, full, error, sizeof(error))) {
        free(b);
        if (!quiet)
            app_toast(a, error, true);
        return false;
    }
    if (!app_path_usable(a, full)) {
        buf_free(b);
        free(b);
        return false;
    }
    a->analyses[a->tab_count] = NULL;
    a->tabs[a->tab_count++] = b;
    a->active = a->tab_count - 1;
    close_modal(a);
    invalidate(a);
    return true;
}
static void new_file(App *a) {
    if (a->tab_count == APP_TABS) {
        app_toast(a, "Fermez un onglet avant de continuer.", true);
        return;
    }
    Buffer *b = calloc(1, sizeof(*b));
    if (!b)
        return;
    int count = 1;
    for (int i = 0; i < a->tab_count; i++)
        if (!*a->tabs[i]->path)
            count++;
    char name[64];
    snprintf(name, sizeof(name), count == 1 ? "sans_titre.py" : "sans_titre_%d.py", count);
    if (!buf_init(b, "", name)) {
        free(b);
        return;
    }
    a->analyses[a->tab_count] = NULL;
    a->tabs[a->tab_count++] = b;
    a->active = a->tab_count - 1;
    close_modal(a);
    invalidate(a);
}
static void remove_tab(App *a, int index) {
    if (index < 0 || index >= a->tab_count)
        return;
    analysis_release(a, index);
    memmove(a->session_discard + index, a->session_discard + index + 1,
            (size_t)(a->tab_count - index - 1) * sizeof(bool));
    a->session_discard[a->tab_count - 1] = false;
    memmove(a->analyses + index, a->analyses + index + 1,
            (size_t)(a->tab_count - index - 1) * sizeof(*a->analyses));
    a->analyses[a->tab_count - 1] = NULL;
    buf_free(a->tabs[index]);
    free(a->tabs[index]);
    memmove(a->tabs + index, a->tabs + index + 1, (size_t)(a->tab_count - index - 1) * sizeof(Buffer *));
    a->tab_count--;
    for (int pane = 0; pane < 2; pane++) {
        if (a->panes[pane].tab > index)
            a->panes[pane].tab--;
        else if (a->panes[pane].tab == index)
            a->panes[pane].tab = index < a->tab_count ? index : a->tab_count - 1;
    }
    a->active = a->panes[a->active_pane].tab;
    if (!a->tab_count) {
        a->split = false;
        a->active_pane = 0;
        new_file(a);
        a->panes[0].tab = a->panes[1].tab = 0;
    }
    close_modal(a);
    invalidate(a);
}
static void close_tab(App *a, int index) {
    if (index < 0 || index >= a->tab_count)
        return;
    a->active = index;
    a->panes[a->active_pane].tab = index;
    if (buf_dirty(app_buffer(a))) {
        a->pending = 1;
        a->modal = M_UNSAVED;
    } else
        remove_tab(a, index);
}
static void request_quit(App *a) {
    if ((a->modal == M_SNIPPET_EDIT || a->modal == M_SNIPPET_DIRTY) && app_snippet_dirty(a)) {
        a->snippet_quit = true;
        a->modal = M_SNIPPET_DIRTY;
        return;
    }
    if (a->file_job) {
        app_toast(a, "Attendez la fin de l’opération ou annulez la copie.", false);
        return;
    }
    for (int i = 0; i < a->tab_count; i++)
        if (!a->session_keep && !a->session_discard[i] && buf_dirty(a->tabs[i])) {
            a->active = i;
            a->pending = 2;
            a->modal = M_UNSAVED;
            return;
        }
    a->session_committing_quit = true;
    if (!app_session_checkpoint(a, true)) {
        a->session_committing_quit = false;
        return;
    }
    if (!app_ssh_request_quit(a))
        a->running = false;
}
static void finish_pending(App *a) {
    int pending = a->pending;
    a->pending = 0;
    a->modal = M_NONE;
    a->focus = 0;
    if (pending == 1)
        remove_tab(a, a->active);
    else if (pending == 2) {
        a->session_discard[a->active] = true;
        request_quit(a);
    }
}
static bool ignored(const char *name) {
    return name[0] == '.' || !strcmp(name, "__pycache__") || !strcmp(name, "node_modules") ||
           !strcmp(name, "venv") || !strcmp(name, "build");
}
static int entry_compare(const void *x, const void *y) {
    const Entry *a = x, *b = y;
    if (a->directory != b->directory)
        return a->directory ? -1 : 1;
    const unsigned char *p = (const unsigned char *)a->name, *q = (const unsigned char *)b->name;
    while (*p && *q) {
        int d = tolower(*p) - tolower(*q);
        if (d)
            return d;
        p++;
        q++;
    }
    return (int)*p - (int)*q;
}
static int list_folder(App *a, const char *folder, Entry *entries, int max, bool only_directories,
                       bool show_ignored) {
    char error[512];
    if (!ssh_guard(&a->ssh, folder, false, error, sizeof(error)))
        return 0;
    FilePathList paths = LoadDirectoryFiles(folder);
    int count = 0;
    for (unsigned i = 0; i < paths.count && count < max; i++) {
        const char *name = fs_basename(paths.paths[i]);
        if (!strcmp(name, ".") || !strcmp(name, "..") || (!show_ignored && ignored(name)))
            continue;
        bool dir = ssh_is_dir(&a->ssh, paths.paths[i]);
        if (only_directories && !dir)
            continue;
        Entry *e = &entries[count++];
        strset(e->path, sizeof(e->path), paths.paths[i]);
        strset(e->name, sizeof(e->name), name);
        e->directory = dir;
        e->depth = 0;
    }
    UnloadDirectoryFiles(paths);
    qsort(entries, (size_t)count, sizeof(*entries), entry_compare);
    return count;
}
static bool expanded(App *a, const char *path) {
    for (int i = 0; i < a->expanded_count; i++)
        if (!strcmp(path, a->expanded[i]))
            return true;
    return false;
}
static void visit_tree(App *a, const char *folder, int depth) {
    if (depth > 4 || a->tree_count >= APP_FILES)
        return;
    Entry *local = calloc(APP_FILES, sizeof(Entry));
    if (!local)
        return;
    int n = list_folder(a, folder, local, APP_FILES, false, false);
    for (int i = 0; i < n && a->tree_count < APP_FILES; i++) {
        a->tree[a->tree_count] = local[i];
        a->tree[a->tree_count++].depth = depth;
        if (local[i].directory && expanded(a, local[i].path))
            visit_tree(a, local[i].path, depth + 1);
    }
    free(local);
}
static void refresh_tree(App *a) {
    a->tree_count = 0;
    visit_tree(a, a->project, 0);
    a->tree_time = GetTime();
}
/* Explorer operations. Paths are captured on right click, never stale indices. */
Action app_context_action(int index) {
    const Action items[] = {A_FS_NEW_FILE, A_FS_NEW_FOLDER, A_FS_COPY, A_FS_PASTE, A_FS_DELETE};
    return index >= 0 && index < 5 ? items[index] : A_NONE;
}
bool app_context_enabled(App *a, int index) {
    if (index == 0 || index == 1)
        return *a->context_parent != 0;
    if (index == 2)
        return *a->context_target != 0;
    if (index == 3)
        return a->clipboard_files.count > 0;
    if (index == 4)
        return *a->context_target && strcmp(a->context_target, a->project);
    return false;
}
static void context_expand(App *a, const char *path) {
    a->tree_root_open = true;
    if (strcmp(path, a->project) && !expanded(a, path) && a->expanded_count < 48)
        strset(a->expanded[a->expanded_count++], BUF_PATH, path);
}
static void clipboard_files(App *a) {
    int result = file_clipboard_read(&a->clipboard_files);
    if (!result)
        result = fs_paths_parse(&a->clipboard_files, GetClipboardText());
    if (result < 0) {
        a->clipboard_files.count = 0;
        app_toast(a, "Presse-papiers trop volumineux (64 éléments / 512 Ko) ou chemin trop long.", true);
    }
}
static void context_open(App *a, Vector2 mouse) {
    a->context_target[0] = 0;
    strset(a->context_parent, BUF_PATH, a->project);
    const Hit *h = ui_clicked(&a->ui, false);
    if (h && h->action == A_TREE && h->arg >= 0 && h->arg < a->tree_count) {
        Entry *e = &a->tree[h->arg];
        strset(a->context_target, BUF_PATH, e->path);
        strset(a->context_parent, BUF_PATH, e->directory ? e->path : native_parent(e->path));
    } else if (h && h->action == A_TREE_ROOT)
        strset(a->context_target, BUF_PATH, a->project);
    a->context_position = mouse;
    a->context_index = 0;
    a->fs_error[0] = 0;
    a->dragging = a->resizing = a->resizing_sidebar = a->resizing_split = false;
    app_completion_dismiss(a);
    a->window_drag = false;
    a->window_resize = 0;
    clipboard_files(a);
    a->modal = M_CONTEXT;
}
static void context_copy(App *a) {
    if (!*a->context_target)
        return;
    FsPaths *paths = calloc(1, sizeof(*paths));
    char *text = malloc(FS_CLIP_LIMIT + 1);
    if (!paths || !text) {
        free(paths);
        free(text);
        app_toast(a, "Mémoire insuffisante.", true);
        return;
    }
    paths->count = 1;
    strset(paths->paths[0], FS_PATH, a->context_target);
    bool ok = file_clipboard_write(paths);
    if (!ok && fs_paths_text(paths, text, FS_CLIP_LIMIT + 1)) {
        SetClipboardText(text);
        ok = true;
    }
    free(paths);
    free(text);
    close_modal(a);
    app_toast(a,
              ok ? "Fichier ou dossier copié. Utilisez Coller ici dans le dossier cible."
                 : "Presse-papiers indisponible.",
              !ok);
}
static void context_create(App *a) {
    char created[BUF_PATH];
    a->fs_error[0] = 0;
    if (!fs_create(a->project, a->context_parent, a->field.text, a->create_directory, created, a->fs_error,
                   sizeof(a->fs_error)))
        return;
    context_expand(a, a->context_parent);
    refresh_tree(a);
    close_modal(a);
    if (!a->create_directory)
        open_file(a, created, false);
    app_toast(a, a->create_directory ? "Dossier créé." : "Fichier créé.", false);
}
static void context_delete(App *a) {
    if (!app_context_enabled(a, 4))
        return;
    a->fs_error[0] = 0;
    for (int i = 0; i < a->tab_count; i++)
        if (*a->tabs[i]->path && fs_under(a->context_target, a->tabs[i]->path) && buf_dirty(a->tabs[i])) {
            strset(a->fs_error, sizeof(a->fs_error),
                   "Enregistrez ou fermez les fichiers modifiés avant de supprimer.");
            return;
        }
    if (a->runner.running && fs_under(a->context_target, a->running_path)) {
        strset(a->fs_error, sizeof(a->fs_error), "Arrêtez d’abord le programme Python concerné (Maj+F5).");
        return;
    }
    a->file_job = file_job_start_guarded(a->project, NULL, a->context_target, NULL, ssh_guard, &a->ssh,
                                         a->fs_error, sizeof(a->fs_error));
    if (a->file_job)
        a->modal = M_FS_BUSY;
}
static void context_paste(App *a) {
    if (!a->clipboard_files.count)
        return;
    a->fs_error[0] = 0;
    a->file_job = file_job_start_guarded(a->project, a->context_parent, NULL, &a->clipboard_files, ssh_guard,
                                         &a->ssh, a->fs_error, sizeof(a->fs_error));
    if (a->file_job)
        a->modal = M_FS_BUSY;
    else
        app_toast(a, a->fs_error, true);
}
static void poll_files(App *a) {
    FileJob *job = a->file_job;
    if (!job || !atomic_load(&job->done))
        return;
    bool success = job->success, deleting = job->deleting, cancelled = atomic_load(&job->cancel);
    char message[512];
    strset(message, sizeof(message),
           success ? (deleting ? "Suppression terminée." : "Copie terminée.") : job->error);
    if (deleting)
        for (int i = a->tab_count - 1; i >= 0; i--)
            if (*a->tabs[i]->path && fs_under(job->target, a->tabs[i]->path) &&
                fs_kind(a->tabs[i]->path) == FS_MISSING && !buf_dirty(a->tabs[i]))
                remove_tab(a, i);
    if (success && !deleting)
        context_expand(a, job->parent);
    file_job_free(job);
    a->file_job = NULL;
    close_modal(a);
    refresh_tree(a);
    app_layout(a);
    app_toast(a, message, !success && !cancelled);
}

/* Import folders are scoped to the explorer project and used only by PyLint. */
static void load_project_imports(App *a) {
    app_odoo_load(a);
    app_xml_paths_load(a);
    completion_reset(&a->completer);
    app_completion_dismiss(a);
    /* Project switching respects manual/no-lint mode. Explicit import edits
       below force a fresh active-document analysis after saving. */
    analysis_reset(a);
    memset(&a->linter.imports, 0, sizeof(a->linter.imports));
    a->import_error[0] = 0;
    char file[BUF_PATH];
    if (!import_paths_file(file, sizeof(file), a->config, a->project))
        strset(a->import_error, sizeof(a->import_error), "Chemin de préférences d’import invalide.");
    else
        import_paths_load(&a->linter.imports, file, a->project, a->import_error, sizeof(a->import_error));
    if (*a->import_error) {
        if (a->running)
            app_toast(a, a->import_error, true);
        else
            fprintf(stderr, "%s\n", a->import_error);
    }
}
static bool commit_imports(App *a, const ImportPaths *next) {
    char file[BUF_PATH];
    if (!import_paths_file(file, sizeof(file), a->config, a->project)) {
        strset(a->import_error, sizeof(a->import_error), "Chemin de préférences d’import trop long.");
        app_toast(a, a->import_error, true);
        return false;
    }
    MakeDirectory(native_parent(file));
    if (!import_paths_save(next, file, a->project, a->import_error, sizeof(a->import_error))) {
        app_toast(a, a->import_error, true);
        return false;
    }
    analysis_reset(a);
    completion_reset(&a->completer);
    app_completion_dismiss(a);
    a->linter.imports = *next;
    if (a->tab_count)
        request_analysis(a);
    app_toast(a, "Dossiers d’import enregistrés pour ce projet · analyse relancée.", false);
    return true;
}
static bool add_import(App *a, const char *path) {
    if (!app_path_usable(a, path))
        return false;
    if (!DirectoryExists(path)) {
        strset(a->import_error, sizeof(a->import_error), "Choisissez un dossier d’import existant.");
        app_toast(a, a->import_error, true);
        return false;
    }
    ImportPaths next = a->linter.imports;
    if (!import_paths_add(&next, path, a->import_error, sizeof(a->import_error))) {
        app_toast(a, a->import_error, true);
        return false;
    }
    return commit_imports(a, &next);
}
static bool commit_xml_paths(App *a, const XmlPaths *next) {
    char file[BUF_PATH];
    if (!xml_paths_file(file, sizeof(file), a->config, a->project)) {
        app_toast(a, "Chemin de préférences XML trop long.", true);
        return false;
    }
    MakeDirectory(native_parent(file));
    if (!xml_paths_save(next, file, a->project, a->xml_paths_error, sizeof(a->xml_paths_error))) {
        app_toast(a, a->xml_paths_error, true);
        return false;
    }
    app_completion_dismiss(a);
    a->xml_paths = *next;
    app_toast(a, "Dossiers XML enregistrés pour ce projet.", false);
    return true;
}
static bool add_xml_path(App *a, const char *path) {
    if (!app_path_usable(a, path))
        return false;
    if (!DirectoryExists(path)) {
        app_toast(a, "Choisissez un dossier XML existant.", true);
        return false;
    }
    XmlPaths next = a->xml_paths;
    if (!xml_paths_add(&next, path, a->xml_paths_error, sizeof(a->xml_paths_error))) {
        app_toast(a, a->xml_paths_error, true);
        return false;
    }
    return commit_xml_paths(a, &next);
}
static void refresh_browser(App *a) {
    a->browser_completion.suggesting = false;
    snprintf(a->browser_completion.status, sizeof(a->browser_completion.status),
             "Ctrl+Espace : suggérer le contenu du chemin");
    a->browser_count = list_folder(a, a->folder, a->browser, APP_FILES,
                                   a->browser_mode == 1 || a->browser_mode == 3 || a->browser_mode == 4 ||
                                       a->browser_mode == 5,
                                   a->browser_mode == 3 || a->browser_mode == 4);
    a->browser_index = 0;
    a->browser_scroll = 0;
}
static void browser_navigate(App *a, const char *path) {
    app_browser_completion_cancel(a);
    char full[BUF_PATH];
    absolute_path(full, sizeof(full), path, a->folder);
    if (!app_path_usable(a, full))
        return;
    strset(a->folder, sizeof(a->folder), full);
    char value[BUF_PATH];
    path_join(value, sizeof(value), full, "");
    app_set_field(&a->field, value, false);
    refresh_browser(a);
}
static void open_browser(App *a, int mode) {
    a->browser_mode = mode;
    a->modal = M_BROWSER;
    app_completion_dismiss(a);
    const char *base = mode == 2 && *app_buffer(a)->path ? native_parent(app_buffer(a)->path) : a->project;
    if (ssh_owns(&a->ssh, base) && (a->ssh.state != SSH_MOUNTED || !ssh_mounted(&a->ssh)))
        base = a->last_local_project;
    strset(a->folder, sizeof(a->folder), base);
    refresh_browser(a);
    char value[BUF_PATH];
    if (mode == 2)
        path_join(value, sizeof(value), a->folder, app_buffer(a)->name);
    else
        path_join(value, sizeof(value), a->folder, "");
    app_set_field(&a->field, value, false);
}
void app_folder_search_browse(App *a) {
    app_folder_search_restore_window(a);
    app_folder_search_cancel(a);
    open_browser(a, 5);
    char root[FS_PATH], error[512];
    if (a->folder_search.initialized &&
        folder_search_root(root, a->folder_search.folder.text, a->project, error, sizeof(error)) &&
        app_path_usable(a, root) && DirectoryExists(root))
        browser_navigate(a, root);
}
void app_folder_search_open_editor(App *a) {
    FolderSearchState *s = &a->folder_search;
    if (s->report && s->file >= 0 && (unsigned)s->file < s->report->count)
        open_file(a, s->report->files[s->file].path, false);
}
static void save_to(App *a, const char *path) {
    char error[512] = "";
    if (buf_save_guarded(app_buffer(a), path, ssh_save_guard, &a->ssh, error, sizeof(error))) {
        completion_reset(&a->completer);
        app_completion_dismiss(a);
        char note[400];
        snprintf(note, sizeof(note), "%s enregistré", app_buffer(a)->name);
        app_toast(a, note, false);
        refresh_tree(a);
        finish_pending(a);
    } else
        app_toast(a, error, true);
}
static void save(App *a, bool as) {
    if (as || !*app_buffer(a)->path)
        open_browser(a, 2);
    else {
        char path[BUF_PATH];
        strset(path, sizeof(path), app_buffer(a)->path);
        save_to(a, path);
    }
}
static void browser_submit(App *a) {
    if ((a->browser_mode == 3 || a->browser_mode == 4 || a->browser_mode == 5) && a->field.len >= BUF_PATH) {
        app_toast(a, "Chemin de dossier trop long.", true);
        return;
    }
    char path[BUF_PATH];
    if (a->browser_mode == 5) {
        char error[512];
        if (!folder_search_root(path, a->field.text, a->folder, error, sizeof(error))) {
            app_toast(a, error, true);
            return;
        }
    } else
        absolute_path(path, sizeof(path), a->field.text, a->folder);
    if (!app_path_usable(a, path))
        return;
    if (a->browser_mode == 3) {
        if (add_import(a, path)) {
            a->modal = M_IMPORTS;
            a->focus = 0;
        }
        return;
    }
    if (a->browser_mode == 4) {
        if (add_xml_path(a, path)) {
            a->modal = M_XML_PATHS;
            a->focus = 0;
        }
        return;
    }
    if (a->browser_mode == 5) {
        if (!DirectoryExists(path)) {
            app_toast(a, "Choisissez un dossier existant.", true);
            return;
        }
        app_folder_search_picked(a, path);
        return;
    }
    if (a->browser_mode == 1) {
        if (!DirectoryExists(path)) {
            app_toast(a, "Choisissez un dossier existant.", true);
            return;
        }
        bool local = a->session_ready && !ssh_owns(&a->ssh, path) && !ssh_owns(&a->ssh, a->project) &&
                     strcmp(path, a->project);
        if (local && !app_session_checkpoint(a, true))
            return;
        if (local && *a->ssh.mountpoint) {
            if (a->ssh.process > 0 || a->ssh.probe > 0 || a->ssh.unmount_process > 0 || a->ssh.lock_fd >= 0 ||
                a->ssh.listener >= 0 || a->ssh.client >= 0) {
                app_toast(a, "Déconnectez SSH avant de changer d’atelier local.", true);
                return;
            }
            ssh_profile_init(&a->ssh.profile);
            *a->ssh.mountpoint = 0;
            a->ssh.state = SSH_IDLE;
            a->ssh.generation = 0;
            *a->ssh.error = 0;
        }
        app_ssh_change_project(a, path);
        if (local) {
            if (!app_session_restore(a)) {
                analysis_reset(a);
                for (int n = 0; n < a->tab_count; n++) {
                    buf_free(a->tabs[n]);
                    free(a->tabs[n]);
                    a->tabs[n] = NULL;
                }
                a->tab_count = 0;
                a->active = a->active_pane = 0;
                a->panes[0].tab = a->panes[1].tab = 0;
                new_file(a);
            }
            invalidate(a);
            app_layout(a);
            refresh_tree(a);
        }
        close_modal(a);
    } else if (DirectoryExists(path))
        browser_navigate(a, path);
    else if (a->browser_mode == 2) {
        if (FileExists(path) && strcmp(path, app_buffer(a)->path)) {
            strset(a->save_target, sizeof(a->save_target), path);
            a->modal = M_OVERWRITE;
        } else
            save_to(a, path);
    } else
        open_file(a, path, false);
}
static void console_add(App *a, const char *text, Tone tone) {
    if (a->console_count == APP_CONSOLE) {
        free(a->console[0].text);
        memmove(a->console, a->console + 1, (APP_CONSOLE - 1) * sizeof(ConsoleLine));
        a->console_count--;
    }
    size_t n = strlen(text);
    if (n > 12000)
        n = 12000;
    char *copy = malloc(n + 1);
    if (!copy)
        return;
    memcpy(copy, text, n);
    copy[n] = 0;
    a->console[a->console_count++] = (ConsoleLine){copy, tone};
}
static void output_data(App *a, const char *data, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (data[i] == '\r')
            continue;
        if (data[i] == '\n' || a->output_length == sizeof(a->output_partial) - 1) {
            a->output_partial[a->output_length] = 0;
            console_add(a, a->output_partial,
                        strstr(a->output_partial, "Error") || strstr(a->output_partial, "Traceback")
                            ? C_RED
                            : C_TEXT);
            a->output_length = 0;
        }
        if (data[i] != '\n' && data[i])
            a->output_partial[a->output_length++] = data[i];
    }
    a->output_partial[a->output_length] = 0;
}
static void run_code(App *a) {
    if (!app_path_usable(a, *app_buffer(a)->path ? app_buffer(a)->path : a->project))
        return;
    close_modal(a);
    Buffer *b = app_buffer(a);
    if (!is_python(b)) {
        app_toast(a, "Sélectionnez un fichier Python (.py).", true);
        return;
    }
    if (a->runner.running) {
        app_toast(a, "Arrêtez d'abord le programme avec Maj+F5.", false);
        return;
    }
    char filename[BUF_PATH], cwd[BUF_PATH], error[512] = "";
    if (*b->path)
        strset(filename, sizeof(filename), b->path);
    else
        path_join(filename, sizeof(filename), a->project, b->name);
    strset(cwd, sizeof(cwd), native_parent(filename));
    if (!runner_start(&a->runner, a->python, b->text, filename, cwd, error, sizeof(error))) {
        app_toast(a, error, true);
        return;
    }
    strset(a->running_path, sizeof(a->running_path), filename);
    if (a->console_count)
        console_add(a, "", C_DIM);
    char command[512];
    snprintf(command, sizeof(command), "› python %s", b->name);
    console_add(a, command, C_TEXT);
    if (!a->zen_mode)
        a->terminal = true;
    a->console_mode = 0;
    a->console_scroll = 0;
    a->has_run = true;
    app_layout(a);
}
static void poll_runner(App *a) {
    if (!a->runner.running)
        return;
    bool was = true;
    char data[16384];
    for (int i = 0; i < 4; i++) {
        size_t n = runner_poll(&a->runner, data, sizeof(data));
        if (n)
            output_data(a, data, n);
        else
            break;
    }
    if (was && !a->runner.running) {
        if (a->output_length) {
            console_add(a, a->output_partial, C_TEXT);
            a->output_length = 0;
            a->output_partial[0] = 0;
        }
        a->last_exit = a->runner.exit_code;
        char note[256];
        snprintf(note, sizeof(note), "%s · code %d · %.2f s",
                 a->runner.stopped ? "■ Programme arrêté"
                 : a->last_exit    ? "× Terminé"
                                   : "✓ Terminé",
                 a->last_exit, runner_clock() - a->runner.started);
        console_add(a, note, a->last_exit && !a->runner.stopped ? C_RED : C_ACCENT);
    }
}
static void palette_add(App *a, const char *name, const char *hint, Action id, Icon icon) {
    if (a->palette_count == APP_FILES)
        return;
    PaletteItem *p = &a->palette[a->palette_count++];
    memset(p, 0, sizeof(*p));
    strset(p->text, sizeof(p->text), name);
    strset(p->hint, sizeof(p->hint), hint);
    p->action = id;
    p->icon = icon;
}
static void palette_files(App *a, const char *folder, int depth) {
    if (depth > 3 || a->palette_count >= 400)
        return;
    Entry *local = calloc(APP_FILES, sizeof(Entry));
    if (!local)
        return;
    int n = list_folder(a, folder, local, APP_FILES, false, false);
    for (int i = 0; i < n && a->palette_count < 400; i++) {
        if (local[i].directory)
            palette_files(a, local[i].path, depth + 1);
        else {
            size_t prefix = strlen(a->project);
            const char *rel = local[i].path + prefix;
            if (*rel == '/')
                rel++;
            palette_add(a, rel, "Fichier", A_PALETTE_FILE, suffix(local[i].name, ".py") ? I_PYTHON : I_FILE);
            strset(a->palette[a->palette_count - 1].path, BUF_PATH, local[i].path);
        }
    }
    free(local);
}
static void show_palette(App *a, bool files) {
    a->modal = M_PALETTE;
    a->palette_count = 0;
    a->palette_index = 0;
    app_completion_dismiss(a);
    a->browser_mode = files ? 1 : 0;
    app_set_field(&a->field, "", false);
    if (files)
        palette_files(a, a->project, 0);
    else {
        palette_add(a, "Nouveau fichier", "Ctrl N", A_NEW, I_PLUS);
        palette_add(a, "Ouvrir un fichier", "Ctrl O", A_OPEN, I_FILE);
        palette_add(a, "Ouvrir un dossier", "Ctrl Maj O", A_FOLDER, I_FOLDER);
        palette_add(a, "Ouvrir un dossier distant — SSH / SFTP", "Profils mémorisés", A_SSH, I_FOLDER);
        palette_add(a, "Déconnecter le dossier SSH", "", A_SSH_DISCONNECT, I_CLOSE);
        palette_add(a, "Enregistrer le fichier", "Ctrl S", A_SAVE, I_SAVE);
        palette_add(a, "Annuler le changement", "Ctrl Z", A_UNDO, I_CODE);
        palette_add(a, "Rétablir le changement", "Ctrl Y / Ctrl Maj Z", A_REDO, I_CODE);
        palette_add(a, "Configurer les dossiers XML", "", A_XML_PATHS, I_FOLDER);
        palette_add(a, "Analyser le fichier avec PyLint", "F4", A_LINT_RUN, I_CODE);
        palette_add(a, "Afficher les diagnostics Python", "Ctrl Maj M", A_PROBLEMS, I_WARNING);
        palette_add(a, "Diagnostic suivant / précédent", "F2 / Maj F2", A_DIAG_NEXT, I_RIGHT);
        palette_add(a, "Activer / désactiver l’analyse automatique", "", A_LINT_AUTO, I_CHECK);
        palette_add(a, "Configurer les règles PyLint ignorées", "", A_LINT_RULES, I_CODE);
        palette_add(a, "Vérification Odoo19 Python/XML — Ce projet", "", A_ODOO_SETTINGS, I_CODE);
        palette_add(a, "Exécuter le fichier Python", "F5", A_RUN, I_PLAY);
        palette_add(a, "Arrêter l'exécution", "Maj F5", A_STOP, I_STOP);
        palette_add(a, "Activer / désactiver les effets", "F6", A_EFFECTS, I_SPARK);
        palette_add(a, "Afficher / masquer l'explorateur", "Ctrl B", A_SIDEBAR, I_SPLIT);
        palette_add(a, "Afficher / masquer la console", "Ctrl J", A_TERMINAL, I_TERMINAL);
        palette_add(a, "Afficher / masquer le menu du haut", "F8", A_TOPBAR, I_UP);
        palette_add(a, "Tout replier / restaurer les panneaux", "F9", A_ZEN, I_CODE);
        palette_add(a, "Rechercher dans le fichier", "Ctrl F", A_FIND, I_SEARCH);
        palette_add(a, "Rechercher dans un dossier", "Ctrl Maj F", A_FOLDER_SEARCH, I_SEARCH);
        palette_add(a, "Aller à une ligne…", "Ctrl G", A_GOTO, I_CODE);
        palette_add(a, "Insérer un bloc depuis un chemin XML", "Ctrl Espace", A_XML_IDS, I_CODE);
        palette_add(a, "Paramètres de l'éditeur", "", A_SETTINGS, I_GEAR);
        palette_add(a, "Configurer les imports Python / complétion Odoo", "", A_IMPORTS, I_FOLDER);
        palette_add(a, "Gérer les macros / extraits de code", "Bibliothèque commune", A_SNIPPETS, I_CODE);
        palette_add(a, "Actualiser la complétion du framework", "", A_COMPLETION_REFRESH, I_CODE);
        palette_add(a, "Vue fractionnée / deux fichiers côte à côte", "F7 / Ctrl \\", A_SPLIT, I_SPLIT);
        palette_add(a, "Plein écran / revenir à la fenêtre", "F11", A_FULLSCREEN, I_EXPAND);
        palette_add(a, "Multi-curseurs : occurrence suivante", "Ctrl D", A_SELECT_NEXT, I_CODE);
        palette_add(a, "Multi-curseurs : toutes les occurrences", "Ctrl Maj L", A_SELECT_OCCURRENCES,
                    I_SEARCH);
        palette_add(a, "Revenir à un seul curseur", "Échap", A_SINGLE_CARET, I_CODE);
        palette_add(a, "Raccourcis clavier", "F1", A_HELP, I_BOOK);
        palette_add(a, "Quitter l’éditeur", "Alt F4", A_QUIT, I_CLOSE);
    }
}
int app_filter_palette(App *a) {
    a->filtered_count = 0;
    for (int i = 0; i < a->palette_count; i++)
        if (contains_ci(a->palette[i].text, a->field.text))
            a->filtered[a->filtered_count++] = i;
    if (a->palette_index >= a->filtered_count)
        a->palette_index = a->filtered_count ? a->filtered_count - 1 : 0;
    return a->filtered_count;
}
void app_update_matches(App *a) {
    Buffer *b = app_buffer(a);
    if (a->search_buffer == b && a->search_version == b->version &&
        a->search_field_version == a->search.version)
        return;
    a->search_buffer = b;
    a->search_version = b->version;
    a->search_field_version = a->search.version;
    a->match_count = 0;
    a->match_index = 0;
    size_t n = a->search.len, p = 0;
    if (n)
        while (a->match_count < 2048 && (p = buf_find(b, a->search.text, p, false)) != SIZE_MAX) {
            a->matches[a->match_count++] = (Match){p, p + n};
            p += n;
        }
}
static void select_match(App *a, int direction, bool first) {
    app_update_matches(a);
    if (!a->match_count)
        return;
    if (!first)
        a->match_index = (a->match_index + direction + a->match_count) % a->match_count;
    Match m = a->matches[a->match_index];
    buf_clear_carets(app_buffer(a));
    app_buffer(a)->anchor = m.start;
    app_buffer(a)->cursor = m.end;
    app_ensure_visible(a);
}
static void show_find(App *a) {
    a->find_open = true;
    a->focus = 1;
    app_completion_dismiss(a);
    char *selected = buf_selected(app_buffer(a));
    if (selected && *selected && !strchr(selected, '\n'))
        app_set_field(&a->search, selected, true);
    else {
        a->search.cursor = a->search.len;
        a->search.anchor = 0;
    }
    free(selected);
    app_layout(a);
    app_update_matches(a);
}
static void goto_submit(App *a) {
    char *end = NULL;
    long n = strtol(a->field.text, &end, 10);
    if (!*a->field.text || *end || n < 1) {
        app_toast(a, "Saisissez un numéro de ligne positif.", true);
        return;
    }
    buf_move(app_buffer(a), buf_position(app_buffer(a), (size_t)(n - 1), 0), false);
    close_modal(a);
    app_ensure_visible(a);
}
static void font_change(App *a, int delta) {
    a->font_size += delta;
    if (a->font_size < UI_CODE_MIN)
        a->font_size = UI_CODE_MIN;
    if (a->font_size > UI_CODE_MAX)
        a->font_size = UI_CODE_MAX;
    app_layout(a);
    app_ensure_visible(a);
}
static void panels_changed(App *a) {
    a->dragging = false;
    a->resizing = false;
    a->resizing_split = false;
    a->resizing_sidebar = false;
    a->caret_x = -1;
    a->particle_count = 0;
    app_layout(a);
    app_ensure_visible(a);
}
void app_minimize_window(App *a) {
    if (a && a->running)
        MinimizeWindow();
}
static void action(App *a, Action id, int arg) {
    if (id >= A_SSH && id <= A_SSH_AUTH_SEND) {
        app_ssh_action(a, id, arg);
        return;
    }
    switch (id) {
    case A_MINIMIZE:
        app_minimize_window(a);
        break;
    case A_UNDO:
    case A_REDO:
        if (id == A_UNDO)
            buf_undo(app_buffer(a));
        else
            buf_redo(app_buffer(a));
        a->focus = 0;
        app_completion_dismiss(a);
        app_ensure_visible(a);
        break;
    case A_XML_PATHS:
        a->modal = M_XML_PATHS;
        a->focus = 0;
        app_completion_dismiss(a);
        break;
    case A_XML_PATH_ADD:
        if (a->xml_paths.count < XML_PATHS_MAX)
            open_browser(a, 4);
        else
            app_toast(a, "Limite : huit dossiers XML par projet.", true);
        break;
    case A_XML_PATH_REMOVE:
    case A_XML_PATH_UP:
    case A_XML_PATH_DOWN: {
        XmlPaths next = a->xml_paths;
        bool changed = id == A_XML_PATH_REMOVE ? xml_paths_remove(&next, arg)
                                               : xml_paths_move(&next, arg, id == A_XML_PATH_UP ? -1 : 1);
        if (changed)
            commit_xml_paths(a, &next);
        break;
    }
    case A_XML_AUTO:
        a->xml_auto = !a->xml_auto;
        app_completion_dismiss(a);
        break;
    case A_XML_IDS:
        app_xml_begin(a, true);
        break;
    case A_XML_PICK:
        app_xml_choose(a, arg);
        break;
    case A_LINT_RUN:
        request_analysis(a);
        break;
    case A_PROBLEMS:
        show_problems(a);
        break;
    case A_DIAGNOSTIC:
        diagnostic_goto(a, arg);
        break;
    case A_DIAG_NEXT:
        diagnostic_next(a, false);
        break;
    case A_DIAG_PREV:
        diagnostic_next(a, true);
        break;
    case A_ODOO_SETTINGS:
        app_odoo_open(a);
        break;
    case A_ODOO_TOGGLE:
        a->odoo_draft_enabled = !a->odoo_draft_enabled;
        break;
    case A_ODOO_APPLY:
        app_odoo_apply(a);
        break;
    case A_ODOO_CLEAR:
        app_set_field(&a->field, "", false);
        a->odoo_error[0] = 0;
        break;
    case A_LINT_IGNORE:
        app_lint_quick_ignore(a, arg);
        break;
    case A_LINT_RULES:
        app_lint_rules_open(a);
        break;
    case A_LINT_RULES_APPLY:
        app_lint_rules_apply(a);
        break;
    case A_LINT_RULES_CLEAR:
        app_set_field(&a->field, "", false);
        a->lint_rules_error[0] = 0;
        break;
    case A_LINT_AUTO:
        a->lint_auto = !a->lint_auto;
        app_toast(a,
                  a->lint_auto ? "Analyse automatique activée."
                               : "Analyse automatique désactivée. F4 reste disponible.",
                  false);
        break;
    case A_SESSION_KEEP_QUIT:
        app_session_keep_quit(a);
        break;
    case A_QUIT:
        close_modal(a);
        request_quit(a);
        break;
    case A_FS_NEW_FILE:
    case A_FS_NEW_FOLDER:
        a->create_directory = id == A_FS_NEW_FOLDER;
        app_set_field(&a->field, a->create_directory ? "nouveau-dossier" : "nouveau.py", true);
        a->fs_error[0] = 0;
        a->modal = M_FS_CREATE;
        break;
    case A_FS_COPY:
        context_copy(a);
        break;
    case A_FS_PASTE:
        context_paste(a);
        break;
    case A_FS_DELETE:
        if (app_context_enabled(a, 4)) {
            a->fs_error[0] = 0;
            a->modal = M_FS_DELETE;
        }
        break;
    case A_FS_CREATE:
        context_create(a);
        break;
    case A_FS_CONFIRM_DELETE:
        context_delete(a);
        break;
    case A_FS_CANCEL:
        if (a->file_job && !a->file_job->deleting)
            atomic_store(&a->file_job->cancel, true);
        break;
    case A_NEW:
        new_file(a);
        break;
    case A_OPEN:
        open_browser(a, 0);
        break;
    case A_FOLDER:
        open_browser(a, 1);
        break;
    case A_SAVE:
        save(a, false);
        break;
    case A_SAVE_AS:
        save(a, true);
        break;
    case A_RUN:
        run_code(a);
        break;
    case A_STOP:
        runner_stop(&a->runner);
        break;
    case A_FULLSCREEN:
        a->window_drag = false;
        a->window_resize = 0;
        ToggleFullscreen();
        app_layout(a);
        a->caret_x = -1;
        break;
    case A_SPLIT:
        if (a->split) {
            int keep = a->active;
            a->split = false;
            a->active_pane = 0;
            a->active = keep;
            a->panes[0].tab = keep;
        } else {
            a->panes[0].tab = a->active;
            if (a->tab_count < 2) {
                int left = a->active;
                new_file(a);
                a->panes[1].tab = a->active;
                a->active = left;
            } else
                a->panes[1].tab = (a->active + 1) % a->tab_count;
            a->split = true;
            a->active_pane = 0;
            if (!a->split_ratio)
                a->split_ratio = 0.5f;
        }
        a->resizing_split = false;
        a->particle_count = 0;
        invalidate(a);
        app_layout(a);
        break;
    case A_PANE_FOCUS:
        app_focus_pane(a, arg);
        break;
    case A_SELECT_NEXT:
        buf_select_next(app_buffer(a));
        app_completion_dismiss(a);
        a->focus = 0;
        app_ensure_visible(a);
        break;
    case A_SELECT_OCCURRENCES:
        buf_select_occurrences(app_buffer(a));
        if (buf_caret_count(app_buffer(a)) == BUF_MAX_CARETS)
            app_toast(a, "Limite de 256 curseurs : d'autres occurrences peuvent rester non sélectionnées.",
                      false);
        app_completion_dismiss(a);
        a->focus = 0;
        app_ensure_visible(a);
        break;
    case A_SINGLE_CARET:
        buf_clear_carets(app_buffer(a));
        app_buffer(a)->anchor = BUF_NO_SELECTION;
        app_completion_dismiss(a);
        a->focus = 0;
        break;
    case A_EFFECTS:
        a->effects = !a->effects;
        if (!a->effects)
            a->particle_count = 0;
        app_toast(a, a->effects ? "Effets visuels activés" : "Mode calme · effets désactivés", false);
        break;
    case A_IMPORTS:
        a->modal = M_IMPORTS;
        app_completion_dismiss(a);
        a->focus = 0;
        break;
    case A_IMPORT_ADD:
        if (a->linter.imports.count < IMPORT_PATHS_MAX) {
            a->import_error[0] = 0;
            open_browser(a, 3);
        } else
            app_toast(a, "Limite : huit dossiers d’import par projet.", true);
        break;
    case A_IMPORT_REMOVE: {
        ImportPaths next = a->linter.imports;
        if (import_paths_remove(&next, arg))
            commit_imports(a, &next);
        break;
    }
    case A_COMPLETION_REFRESH:
        completion_reset(&a->completer);
        app_completion_dismiss(a);
        app_toast(a, "Cache de complétion vidé · actualisation à la prochaine saisie / Ctrl+Espace.", false);
        break;
    case A_SNIPPETS:
        a->modal = M_SNIPPETS;
        a->focus = 0;
        app_completion_dismiss(a);
        break;
    case A_SNIPPET_NEW:
        app_snippet_edit(a, -1);
        break;
    case A_SNIPPET_EDIT:
        app_snippet_edit(a, arg);
        break;
    case A_SNIPPET_DELETE:
        if (arg >= 0 && arg < a->snippets.count) {
            a->snippet_index = arg;
            a->snippet_error[0] = 0;
            a->modal = M_SNIPPET_DELETE;
        }
        break;
    case A_SNIPPET_CONFIRM_DELETE:
        app_snippet_delete(a);
        break;
    case A_SNIPPET_SAVE:
        if (app_snippet_save(a) && a->snippet_quit) {
            a->snippet_quit = false;
            request_quit(a);
        }
        break;
    case A_SNIPPET_DISCARD: {
        bool quit = a->snippet_quit;
        a->snippet_quit = a->snippet_dragging = false;
        a->modal = M_SNIPPETS;
        if (quit)
            request_quit(a);
        break;
    }
    case A_SNIPPET_CONTINUE:
        a->modal = M_SNIPPET_EDIT;
        a->snippet_quit = false;
        break;
    case A_SNIPPET_FOCUS:
        if (arg >= 0 && arg <= 2) {
            a->snippet_focus = arg;
            a->snippet_dragging = false;
            Buffer *field = arg == 0 ? &a->snippet_trigger : arg == 1 ? &a->snippet_title : &a->snippet_body;
            buf_move(field, field->len, false);
        }
        break;
    case A_SETTINGS:
        a->modal = M_SETTINGS;
        app_completion_dismiss(a);
        break;
    case A_HELP:
        a->modal = M_HELP;
        app_completion_dismiss(a);
        break;
    case A_COMMANDS:
        show_palette(a, false);
        break;
    case A_QUICK_OPEN:
        show_palette(a, true);
        break;
    case A_FOLDER_SEARCH:
        app_folder_search_show(a);
        break;
    case A_FOLDER_SEARCH_START:
        app_folder_search_start(a);
        break;
    case A_FOLDER_SEARCH_STOP:
        app_folder_search_stop(a);
        break;
    case A_FOLDER_SEARCH_PICK_ROOT:
        app_folder_search_browse(a);
        break;
    case A_FOLDER_SEARCH_FOCUS:
        if (arg >= 0 && arg < 12) {
            a->folder_search.focus = arg;
            if (arg >= 4 && arg < 8)
                a->folder_search.filters_open = true;
        }
        break;
    case A_FOLDER_SEARCH_FILE:
        app_folder_search_select(a, arg, 0);
        break;
    case A_FOLDER_SEARCH_NEXT:
        app_folder_search_next(a, 1);
        break;
    case A_FOLDER_SEARCH_PREV:
        app_folder_search_next(a, -1);
        break;
    case A_FOLDER_SEARCH_OPTION:
        app_folder_search_option(a, arg);
        break;
    case A_FOLDER_SEARCH_FILTERS:
        a->folder_search.filters_open = !a->folder_search.filters_open;
        if (!a->folder_search.filters_open && a->folder_search.focus >= 4 && a->folder_search.focus < 8)
            a->folder_search.focus = 1;
        break;
    case A_FOLDER_SEARCH_MAXIMIZE:
        app_folder_search_maximize(a);
        break;
    case A_FOLDER_SEARCH_FULLSCREEN:
        app_folder_search_fullscreen(a);
        break;
    case A_FOLDER_SEARCH_OPEN:
        app_folder_search_open_editor(a);
        break;
    case A_FIND:
        show_find(a);
        break;
    case A_FIND_CLOSE:
        a->find_open = false;
        a->focus = 0;
        app_layout(a);
        break;
    case A_FIND_NEXT:
        select_match(a, 1, false);
        break;
    case A_FIND_PREV:
        select_match(a, -1, false);
        break;
    case A_GOTO: {
        char n[32];
        snprintf(n, sizeof(n), "%zu", buf_row(app_buffer(a), app_buffer(a)->cursor) + 1);
        app_set_field(&a->field, n, true);
        a->modal = M_GOTO;
        break;
    }
    case A_GOTO_SUBMIT:
        goto_submit(a);
        break;
    case A_SIDEBAR:
        a->zen_mode = false;
        a->sidebar = !a->sidebar;
        panels_changed(a);
        break;
    case A_TOPBAR:
        a->zen_mode = false;
        a->topbar = !a->topbar;
        panels_changed(a);
        break;
    case A_ZEN:
        if (a->zen_mode) {
            a->sidebar = a->zen_sidebar;
            a->topbar = a->zen_topbar;
            a->terminal = a->zen_terminal;
            a->zen_mode = false;
        } else {
            a->zen_sidebar = a->sidebar;
            a->zen_topbar = a->topbar;
            a->zen_terminal = a->terminal;
            a->sidebar = false;
            a->topbar = false;
            a->terminal = false;
            a->zen_mode = true;
        }
        a->focus = 0;
        panels_changed(a);
        break;
    case A_TERMINAL:
        a->zen_mode = false;
        a->terminal = !a->terminal;
        a->focus = 0;
        panels_changed(a);
        break;
    case A_CONSOLE_CLEAR:
        for (int i = 0; i < a->console_count; i++)
            free(a->console[i].text);
        a->console_count = 0;
        a->output_length = 0;
        a->output_partial[0] = 0;
        a->console_scroll = 0;
        break;
    case A_CONSOLE_EXPAND:
        a->terminal_h = a->terminal_h > 260 ? 211 : (float)(a->h - a->code_y) * 0.64f;
        app_layout(a);
        break;
    case A_CONSOLE_TAB:
        a->console_mode = arg;
        a->focus = arg == 0 ? 2 : 0;
        break;
    case A_TAB:
        if (arg >= 0 && arg < a->tab_count) {
            a->active = arg;
            a->focus = 0;
            invalidate(a);
        }
        break;
    case A_TAB_CLOSE:
        close_tab(a, arg);
        break;
    case A_TREE_ROOT:
        a->tree_root_open = !a->tree_root_open;
        break;
    case A_TREE:
        if (arg >= 0 && arg < a->tree_count) {
            Entry *e = &a->tree[arg];
            if (e->directory) {
                int found = -1;
                for (int i = 0; i < a->expanded_count; i++)
                    if (!strcmp(e->path, a->expanded[i]))
                        found = i;
                if (found >= 0) {
                    memmove(a->expanded[found], a->expanded[found + 1],
                            (size_t)(a->expanded_count - found - 1) * BUF_PATH);
                    a->expanded_count--;
                } else if (a->expanded_count < 48)
                    strset(a->expanded[a->expanded_count++], BUF_PATH, e->path);
                refresh_tree(a);
            } else {
                char path[BUF_PATH];
                strset(path, sizeof(path), e->path);
                open_file(a, path, false);
            }
        }
        break;
    case A_MODAL_CLOSE:
        close_modal(a);
        break;
    case A_BROWSER_UP: {
        char parent[BUF_PATH];
        strset(parent, sizeof(parent),
               native_parent(a->browser_completion.suggesting ? a->browser_completion.suggested_folder
                                                              : a->folder));
        browser_navigate(a, parent);
        break;
    }
    case A_BROWSER_ENTRY:
        if (app_browser_completion_accept(a, arg))
            break;
        if (arg >= 0 && arg < a->browser_count) {
            Entry e = a->browser[arg];
            if (e.directory)
                browser_navigate(a, e.path);
            else if (a->browser_mode == 2)
                app_set_field(&a->field, e.path, false);
            else
                open_file(a, e.path, false);
        }
        break;
    case A_BROWSER_SUBMIT:
        browser_submit(a);
        break;
    case A_UNSAVED_SAVE:
        save(a, false);
        break;
    case A_UNSAVED_DISCARD:
        finish_pending(a);
        break;
    case A_OVERWRITE: {
        char path[BUF_PATH];
        strset(path, sizeof(path), a->save_target);
        save_to(a, path);
        break;
    }
    case A_PALETTE_CHOOSE:
        if (arg >= 0 && arg < a->palette_count) {
            PaletteItem p = a->palette[arg];
            close_modal(a);
            if (p.action == A_PALETTE_FILE)
                open_file(a, p.path, false);
            else
                action(a, p.action, p.arg);
        }
        break;
    case A_MINIMAP: {
        Buffer *b = app_buffer(a);
        buf_index(b);
        float step = fminf(3.6f, fmaxf(0.5f, (float)(a->code_h - 62) / (float)b->line_count));
        float target =
            fmaxf(0, (a->ui.mouse.y - (float)(a->code_y + 44)) / step - (float)a->code_h / a->line_h / 2);
        b->scroll_y = fminf(fmaxf(0, (float)b->line_count * a->line_h - (float)a->code_h + a->line_h * 3),
                            target * a->line_h);
        break;
    }
    case A_MINIMAP_TOGGLE:
        a->minimap = !a->minimap;
        app_layout(a);
        break;
    case A_FONT_UP:
        font_change(a, 1);
        break;
    case A_FONT_DOWN:
        font_change(a, -1);
        break;
    case A_COMPLETION:
        app_completion_apply(a, arg);
        break;
    case A_FIELD_FIND:
        a->focus = 1;
        break;
    case A_FIELD_CONSOLE:
        a->focus = 2;
        break;
    case A_NONE:
    case A_PALETTE_FILE:
    default: /* SSH actions are routed before this local-editor switch. */
        break;
    }
}
void app_action(App *a, Action id, int arg) {
    action(a, id, arg);
}
static bool pressed(App *a, int key) {
    return app_key_pressed(a, key);
}
static char *clipboard_copy(const char *s) {
    size_t n = strlen(s);
    char *p = malloc(n + 1);
    if (p)
        memcpy(p, s, n + 1);
    return p;
}
static void copy_selected(Buffer *b, bool cut) {
    if (b->extra_count) {
        char *text = buf_copy_carets(b, cut);
        if (text) {
            SetClipboardText(text);
            free(text);
        }
        return;
    }
    size_t start, end;
    char *text;
    if (buf_selection(b, &start, &end)) {
        text = buf_selected(b);
        if (text) {
            SetClipboardText(text);
            free(text);
        }
        if (cut)
            buf_insert(b, "");
    } else {
        size_t row = buf_row(b, b->cursor);
        start = b->lines[row];
        end = buf_line_end(b, row);
        size_t n = end - start;
        text = malloc(n + 2);
        if (!text)
            return;
        memcpy(text, b->text + start, n);
        text[n] = '\n';
        text[n + 1] = 0;
        SetClipboardText(text);
        free(text);
        if (cut) {
            if (end < b->len)
                end++;
            else if (start)
                start--;
            buf_replace(b, start, end, "");
        }
    }
}
bool app_history_keys(App *a, Buffer *b, bool ctrl, bool shift) {
    if (!ctrl)
        return false;
    if (pressed(a, KEY_Z)) {
        if (shift)
            buf_redo(b);
        else
            buf_undo(b);
        return true;
    }
    if (pressed(a, KEY_Y)) {
        buf_redo(b);
        return true;
    }
    return false;
}
static void field_keys(App *a, Buffer *b, const unsigned *chars, int count, bool ctrl, bool shift) {
    if (app_history_keys(a, b, ctrl, shift))
        return;
    if (ctrl) {
        if (pressed(a, KEY_A))
            buf_select_all(b);
        else if (pressed(a, KEY_C)) {
            char *s = buf_selected(b);
            if (s) {
                SetClipboardText(s);
                free(s);
            }
        } else if (pressed(a, KEY_X)) {
            char *s = buf_selected(b);
            if (s) {
                SetClipboardText(s);
                free(s);
            }
            buf_insert(b, "");
        } else if (pressed(a, KEY_V)) {
            char *s = clipboard_copy(GetClipboardText() ? GetClipboardText() : "");
            if (s) {
                for (size_t i = 0; s[i]; i++)
                    if (s[i] == '\n' || s[i] == '\r')
                        s[i] = ' ';
                buf_insert(b, s);
                free(s);
            }
        }
    } else
        for (int i = 0; i < count; i++) {
            char s[5];
            if (utf8_encode(chars[i], s))
                buf_insert(b, s);
        }
    if (pressed(a, KEY_BACKSPACE))
        buf_backspace(b, ctrl);
    if (pressed(a, KEY_DELETE))
        buf_delete(b, ctrl);
    if (pressed(a, KEY_LEFT))
        buf_horizontal(b, -1, ctrl, shift);
    if (pressed(a, KEY_RIGHT))
        buf_horizontal(b, 1, ctrl, shift);
    if (pressed(a, KEY_HOME))
        buf_move(b, 0, shift);
    if (pressed(a, KEY_END))
        buf_move(b, b->len, shift);
}
void app_edit_field(App *a, Buffer *field, const unsigned *chars, int count, bool ctrl, bool shift) {
    field_keys(a, field, chars, count, ctrl, shift);
}
static size_t mouse_position(App *a, Vector2 m) {
    Buffer *b = app_buffer(a);
    buf_index(b);
    int row = (int)floorf((m.y - (float)a->code_y - 13 + b->scroll_y) / a->line_h);
    if (row < 0)
        row = 0;
    if ((size_t)row >= b->line_count)
        row = (int)b->line_count - 1;
    float target = fmaxf(0, (m.x - (float)a->code_x + b->scroll_x) / a->char_w);
    size_t p = b->lines[row], end = buf_line_end(b, (size_t)row);
    int col = 0;
    while (p < end) {
        int next = col + (b->text[p] == '\t' ? 4 - col % 4 : 1);
        if (target < ((float)col + (float)next) * 0.5f)
            break;
        col = next;
        p = utf8_next(b->text, b->len, p);
    }
    return p;
}
/* Frameless window gestures never consume Alt+click (multi-cursors). */
static int window_edges(App *a, Vector2 m) {
    if (a->modal || IsWindowFullscreen() || IsWindowMaximized())
        return 0;
    int edge = 0;
    if (m.x >= 0 && m.x < 5)
        edge |= 1;
    if (m.x > a->w - 5 && m.x < a->w)
        edge |= 2;
    if (m.y >= 0 && m.y < 5)
        edge |= 4;
    if (m.y > a->h - 5 && m.y < a->h)
        edge |= 8;
    return edge;
}
static int window_cursor(int edge) {
    if ((edge & 1 && edge & 4) || (edge & 2 && edge & 8))
        return MOUSE_CURSOR_RESIZE_NWSE;
    if ((edge & 2 && edge & 4) || (edge & 1 && edge & 8))
        return MOUSE_CURSOR_RESIZE_NESW;
    return edge & 3 ? MOUSE_CURSOR_RESIZE_EW : MOUSE_CURSOR_RESIZE_NS;
}
static bool window_gestures(App *a, Vector2 m) {
    if (a->window_drag || a->window_resize) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            Vector2 position = GetWindowPosition(), global = {position.x + m.x, position.y + m.y};
            int dx = (int)roundf(global.x - a->window_press.x),
                dy = (int)roundf(global.y - a->window_press.y);
            if (a->window_drag) {
                if (IsWindowMaximized()) {
                    if (abs(dx) <= 3 && abs(dy) <= 3) {
                        if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT))
                            a->window_drag = false;
                        return true;
                    }
                    float fraction = (a->window_press.x - a->window_origin.x) / fmaxf(1, a->window_size.x);
                    float offset_y = fminf(a->window_press.y - a->window_origin.y, 40);
                    RestoreWindow();
                    SetWindowSize(a->windowed_w, a->windowed_h);
                    a->window_origin = (Vector2){global.x - fraction * a->windowed_w, global.y - offset_y};
                    a->window_press = global;
                    dx = dy = 0;
                }
                SetWindowPosition((int)a->window_origin.x + dx, (int)a->window_origin.y + dy);
            } else {
                int x = (int)a->window_origin.x, y = (int)a->window_origin.y, w = (int)a->window_size.x,
                    h = (int)a->window_size.y;
                if (a->window_resize & 1) {
                    int delta = (int)fmaxf(w - 32768, fminf(dx, w - 960));
                    x += delta;
                    w -= delta;
                }
                if (a->window_resize & 2)
                    w = (int)fmaxf(960, fminf(32768, w + dx));
                if (a->window_resize & 4) {
                    int delta = (int)fmaxf(h - 32768, fminf(dy, h - 640));
                    y += delta;
                    h -= delta;
                }
                if (a->window_resize & 8)
                    h = (int)fmaxf(640, fminf(32768, h + dy));
                SetWindowSize(w, h);
                SetWindowPosition(x, y);
                SetMouseCursor(window_cursor(a->window_resize));
            }
        }
        if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            a->window_drag = false;
            a->window_resize = 0;
        }
        return true;
    }
    if (a->modal || IsWindowFullscreen() || !IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        return false;
    int edge = window_edges(a, m);
    bool header = (a->topbar && m.y >= 5 && m.y < a->layout.topbar_h && m.x >= 5 && m.x < 305) ||
                  (m.y >= a->layout.tabs_y && m.y < a->layout.tabs_y + LAYOUT_TABS_H) ||
                  (m.y >= a->layout.status_y && m.y < a->h && m.x >= 502);
    if (!edge && (!header || ui_clicked(&a->ui, false)))
        return false;
    Vector2 position = GetWindowPosition(), global = {position.x + m.x, position.y + m.y};
    if (!edge) {
        bool twice = GetTime() - a->window_click_time < .34 && fabsf(m.x - a->window_click.x) < 6 &&
                     fabsf(m.y - a->window_click.y) < 6;
        a->window_click = m;
        a->window_click_time = GetTime();
        if (twice) {
            if (IsWindowMaximized())
                RestoreWindow();
            else
                MaximizeWindow();
            return true;
        }
    }
    a->window_drag = !edge;
    a->window_resize = edge;
    a->window_press = global;
    a->window_origin = position;
    a->window_size = (Vector2){(float)a->w, (float)a->h};
    a->dragging = false;
    return true;
}

static bool mouse_update(App *a, bool ctrl, bool shift) {
    Vector2 m = GetMousePosition();
    float wheel = GetMouseWheelMove();
    if (a->modal == M_FOLDER_SEARCH && app_folder_search_mouse(a, m, IsMouseButtonPressed(MOUSE_BUTTON_LEFT),
                                                               IsMouseButtonDown(MOUSE_BUTTON_LEFT),
                                                               IsMouseButtonReleased(MOUSE_BUTTON_LEFT)))
        return IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    if (window_gestures(a, m))
        return true;
    if (app_snippet_mouse(a, wheel, shift))
        return true;
    if (wheel && !a->modal && a->completion_count && ui_inside(a->completion_rect, m)) {
        int next = a->completion_index - (int)(wheel * 2);
        a->completion_index = (next % a->completion_count + a->completion_count) % a->completion_count;
        return true;
    }
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        if ((!a->modal || a->modal == M_CONTEXT) && a->sidebar && a->ex > 0 && m.x >= 0 && m.x < a->ex &&
            m.y >= a->layout.topbar_h && m.y < a->layout.status_y) {
            context_open(a, m);
            return true;
        }
        if (a->modal == M_CONTEXT) {
            close_modal(a);
            return true;
        }
    }
    if (!a->modal && a->sidebar && a->ex > 0 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
        fabsf(m.x - (float)a->ex) < 4 && m.y >= a->layout.topbar_h && m.y < a->layout.status_y) {
        a->resizing_sidebar = true;
        a->dragging = false;
    }
    if (a->resizing_sidebar) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            int max_width = a->w - 520;
            if (max_width > LAYOUT_SIDEBAR_MAX)
                max_width = LAYOUT_SIDEBAR_MAX;
            a->sidebar_width = (int)fmaxf(LAYOUT_SIDEBAR_MIN, fminf((float)max_width, m.x));
            app_layout(a);
            a->caret_x = -1;
            SetMouseCursor(MOUSE_CURSOR_RESIZE_EW);
            if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
                a->resizing_sidebar = false;
            return true;
        }
        a->resizing_sidebar = false;
    }
    if (!a->modal && a->split) {
        bool in_editor = m.x > a->ex && m.y >= a->layout.crumb_y && m.y < a->code_bottom;
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && in_editor) {
            if (fabsf(m.x - (float)a->divider_x) < 5) {
                a->resizing_split = true;
                a->dragging = false;
            } else {
                int pane = m.x > (float)a->divider_x ? 1 : 0;
                if (pane != a->active_pane)
                    app_focus_pane(a, pane);
            }
        }
        if (a->resizing_split) {
            /* Apply the final pointer position even when move+release arrive in
               one frame (common over VNC and on low-refresh displays). */
            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
                a->split_ratio = fmaxf(0.2f, fminf(0.8f, (m.x - (float)a->ex) / (float)(a->w - a->ex)));
                app_layout(a);
                SetMouseCursor(MOUSE_CURSOR_RESIZE_EW);
                if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
                    a->resizing_split = false;
                return true;
            }
            a->resizing_split = false;
        }
    }
    Buffer *b = app_buffer(a);
    buf_index(b);
    Buffer *scroll_buffer = b;
    if (a->split && m.x > a->ex && m.y >= a->code_y && m.y < a->code_bottom) {
        int pane = m.x > (float)a->divider_x ? 1 : 0;
        scroll_buffer = a->tabs[a->panes[pane].tab];
        buf_index(scroll_buffer);
    }
    if (wheel) {
        if (a->modal == M_BROWSER) {
            a->browser_scroll -= (int)(wheel * 3);
            if (a->browser_scroll < 0)
                a->browser_scroll = 0;
            if (a->browser_scroll > a->browser_count - 7)
                a->browser_scroll = a->browser_count > 7 ? a->browser_count - 7 : 0;
        } else if (a->modal == M_SSH_PROFILES) {
            a->ssh_profile_scroll -= (int)(wheel * 2);
            int last = a->ssh_profiles.count > 8 ? a->ssh_profiles.count - 8 : 0;
            if (a->ssh_profile_scroll < 0)
                a->ssh_profile_scroll = 0;
            if (a->ssh_profile_scroll > last)
                a->ssh_profile_scroll = last;
        } else if (a->modal == M_SSH_AUTH) {
            a->ssh_prompt_scroll -= (int)(wheel * 3);
            if (a->ssh_prompt_scroll < 0)
                a->ssh_prompt_scroll = 0;
            if (a->ssh_prompt_scroll > 244)
                a->ssh_prompt_scroll = 244;
        } else if (a->modal == M_FOLDER_SEARCH) {
            app_folder_search_scroll(a, m, wheel, shift);
        } else if (a->modal == M_XML_IDS) {
            app_xml_filter(a);
            a->xml.index -= (int)wheel;
            if (a->xml.index < 0)
                a->xml.index = 0;
            if (a->xml.index >= a->xml.count)
                a->xml.index = a->xml.count ? a->xml.count - 1 : 0;
        } else if (a->modal == M_PALETTE) {
            app_filter_palette(a);
            a->palette_index -= (int)wheel;
            if (a->palette_index < 0)
                a->palette_index = 0;
            if (a->palette_index >= a->filtered_count)
                a->palette_index = a->filtered_count ? a->filtered_count - 1 : 0;
        } else if (!a->modal) {
            if (m.x > a->ex && m.y >= a->code_y && m.y < a->code_bottom) {
                if (ctrl)
                    font_change(a, wheel > 0 ? 1 : -1);
                else if (shift)
                    scroll_buffer->scroll_x = fmaxf(0, scroll_buffer->scroll_x - wheel * a->char_w * 5);
                else
                    scroll_buffer->scroll_y =
                        fmaxf(0, fminf(fmaxf(0, (float)scroll_buffer->line_count * a->line_h - a->code_h +
                                                    a->line_h * 4),
                                       scroll_buffer->scroll_y - wheel * a->line_h * 3));
            } else if (m.x > a->ex && m.y >= a->terminal_y && m.y < a->layout.status_y) {
                if (a->console_mode == 1) {
                    LintReport *report = app_diagnostics(a);
                    a->lint_scroll -= (int)(wheel * 3);
                    if (a->lint_scroll < 0)
                        a->lint_scroll = 0;
                    if (report && a->lint_scroll >= report->count)
                        a->lint_scroll = report->count ? report->count - 1 : 0;
                } else
                    a->console_scroll += (int)(wheel * 3);
                if (a->console_scroll < 0)
                    a->console_scroll = 0;
                if (a->console_scroll > a->console_count)
                    a->console_scroll = a->console_count;
            } else if (a->sidebar && m.x >= 0 && m.x < a->ex && m.y >= a->layout.topbar_h) {
                a->tree_scroll =
                    layout_tree_scroll(&a->layout, a->tree_count, a->tree_scroll - (int)(wheel * 3));
            }
        }
    }
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        const Hit *h = ui_clicked(&a->ui, a->modal != M_NONE);
        if (h) {
            Action id = (Action)h->action;
            int arg = h->arg;
            action(a, id, arg);
            a->dragging = false;
            return true;
        }
        if (a->modal) {
            if (a->modal == M_CONTEXT)
                close_modal(a);
            return true;
        }
        if (a->terminal && m.x > a->ex && fabsf(m.y - a->terminal_y) < 5) {
            a->resizing = true;
            return false;
        }
        if (m.x > a->pane_x && m.x < a->code_right && m.y >= a->code_y && m.y < a->code_bottom) {
            a->focus = 0;
            app_completion_dismiss(a);
            size_t p = mouse_position(a, m);
            if (IsKeyDown(KEY_LEFT_ALT)) {
                buf_toggle_caret(b, p);
                a->dragging = false;
                a->caret_time = GetTime();
                return true;
            }
            buf_clear_carets(b);
            bool twice = GetTime() - a->last_click_time < 0.34 && fabsf(m.x - a->last_click.x) < 6 &&
                         fabsf(m.y - a->last_click.y) < 6;
            a->last_click = m;
            a->last_click_time = GetTime();
            if (m.x < a->code_x - 15) {
                size_t row = buf_row(b, p);
                b->anchor = b->lines[row];
                b->cursor = row + 1 < b->line_count ? b->lines[row + 1] : b->len;
            } else if (twice) {
                size_t start = p, end = p;
                while (start) {
                    size_t prev = utf8_prev(b->text, start);
                    unsigned char c = (unsigned char)b->text[prev];
                    if (!(isalnum(c) || c == '_' || c >= 128))
                        break;
                    start = prev;
                }
                while (end < b->len) {
                    unsigned char c = (unsigned char)b->text[end];
                    if (!(isalnum(c) || c == '_' || c >= 128))
                        break;
                    end = utf8_next(b->text, b->len, end);
                }
                b->anchor = start;
                b->cursor = end;
            } else {
                buf_move(b, p, shift);
                if (!shift)
                    b->anchor = p;
                a->dragging = true;
            }
            a->caret_time = GetTime();
        } else if (a->terminal && m.x > a->ex && m.y > a->terminal_y + 38)
            a->focus = 2;
    }
    if (a->resizing) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            a->terminal_h = fmaxf(110, fminf((float)(a->layout.status_y - a->code_y - LAYOUT_MIN_CODE_H),
                                             (float)a->layout.status_y - m.y));
            app_layout(a);
            a->caret_x = -1;
            if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
                a->resizing = false;
        } else
            a->resizing = false;
    }
    if (a->dragging) {
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            if (m.y < a->code_y)
                b->scroll_y = fmaxf(0, b->scroll_y - a->line_h / 4);
            else if (m.y > a->code_bottom)
                b->scroll_y = fminf(fmaxf(0, (float)b->line_count * a->line_h - a->code_h + a->line_h * 2),
                                    b->scroll_y + a->line_h / 4);
            b->cursor = mouse_position(a, m);
        } else
            a->dragging = false;
    }
    int cursor = MOUSE_CURSOR_DEFAULT;
    if (!a->modal) {
        if (a->sidebar && a->ex > 0 && fabsf(m.x - (float)a->ex) < 4 && m.y >= a->layout.topbar_h &&
            m.y < a->layout.status_y)
            cursor = MOUSE_CURSOR_RESIZE_EW;
        else if (a->split && fabsf(m.x - (float)a->divider_x) < 5 && m.y >= a->layout.crumb_y &&
                 m.y < a->code_bottom)
            cursor = MOUSE_CURSOR_RESIZE_EW;
        else if (a->resizing || (a->terminal && m.x > a->ex && fabsf(m.y - a->terminal_y) < 4))
            cursor = MOUSE_CURSOR_RESIZE_NS;
        else if (ui_clicked(&a->ui, false))
            cursor = MOUSE_CURSOR_POINTING_HAND;
        else if (m.x > a->code_x - 10 && m.x < a->code_right && m.y > a->code_y && m.y < a->code_bottom)
            cursor = MOUSE_CURSOR_IBEAM;
    } else if (ui_clicked(&a->ui, true))
        cursor = MOUSE_CURSOR_POINTING_HAND;
    int edges = window_edges(a, m);
    if (edges)
        cursor = window_cursor(edges);
    SetMouseCursor(cursor);
    return false;
}
static void emit(App *a) {
    if (!a->effects)
        return;
    Vector2 pos = app_caret(a, app_buffer(a)->cursor);
    for (int i = 0; i < 4 && a->particle_count < 128; i++)
        a->particles[a->particle_count++] =
            (Particle){pos.x, pos.y + a->line_h * 0.6f, (float)GetRandomValue(-32, 38),
                       (float)GetRandomValue(-65, -12), (float)GetRandomValue(22, 52) / 100};
}
static void modal_keys(App *a, const unsigned *chars, int count, bool ctrl, bool shift) {
    if (pressed(a, KEY_ESCAPE)) {
        close_modal(a);
        return;
    }
    if (a->modal == M_ODOO) {
        app_odoo_keys(a, chars, count, ctrl, shift);
        return;
    }
    if (a->modal == M_LINT_RULES) {
        app_lint_rules_keys(a, chars, count, ctrl, shift);
        return;
    }
    if (a->modal == M_FOLDER_SEARCH) {
        app_folder_search_keys(a, chars, count, ctrl, shift);
        return;
    }
    if (a->modal == M_SSH || a->modal == M_SSH_PROFILES || a->modal == M_SSH_AUTH) {
        app_ssh_keys(a, chars, count, ctrl, shift);
        return;
    }
    if (a->modal == M_XML_IDS) {
        app_xml_keys(a, chars, count, ctrl, shift);
        return;
    }
    if (a->modal == M_SNIPPETS) {
        if (ctrl && pressed(a, KEY_N))
            app_snippet_edit(a, -1);
        else if (pressed(a, KEY_UP) || pressed(a, KEY_DOWN)) {
            a->snippet_index += pressed(a, KEY_DOWN) ? 1 : -1;
            if (a->snippet_index < 0)
                a->snippet_index = 0;
            if (a->snippet_index >= a->snippets.count)
                a->snippet_index = a->snippets.count ? a->snippets.count - 1 : 0;
            if (a->snippet_index < a->snippet_scroll)
                a->snippet_scroll = a->snippet_index;
            if (a->snippet_index > a->snippet_scroll + 7)
                a->snippet_scroll = a->snippet_index - 7;
        } else if (pressed(a, KEY_ENTER) && a->snippets.count)
            app_snippet_edit(a, a->snippet_index);
        else if (pressed(a, KEY_DELETE) && a->snippets.count)
            action(a, A_SNIPPET_DELETE, a->snippet_index);
        return;
    }
    if (a->modal == M_SNIPPET_EDIT) {
        app_snippet_keys(a, chars, count, ctrl, shift);
        return;
    }
    if (a->modal == M_SNIPPET_DELETE) {
        if (pressed(a, KEY_ENTER))
            app_snippet_delete(a);
        return;
    }
    if (a->modal == M_SNIPPET_DIRTY) {
        if (ctrl && pressed(a, KEY_S))
            action(a, A_SNIPPET_SAVE, 0);
        return;
    }
    if (a->modal == M_CONTEXT) {
        int direction = pressed(a, KEY_DOWN) ? 1 : pressed(a, KEY_UP) ? -1 : 0;
        if (direction)
            for (int i = 0; i < 5; i++) {
                a->context_index = (a->context_index + direction + 5) % 5;
                if (app_context_enabled(a, a->context_index))
                    break;
            }
        if (pressed(a, KEY_ENTER) && app_context_enabled(a, a->context_index))
            action(a, app_context_action(a->context_index), 0);
        return;
    }
    if (a->modal == M_FS_BUSY)
        return;
    if (a->modal == M_FS_DELETE) {
        if (pressed(a, KEY_ENTER))
            context_delete(a);
        return;
    }
    if (a->modal == M_PALETTE || a->modal == M_BROWSER || a->modal == M_GOTO || a->modal == M_FS_CREATE) {
        if (a->modal == M_BROWSER && ctrl && pressed(a, KEY_SPACE)) {
            app_browser_completion_begin(a);
            return;
        }
        if (a->modal == M_BROWSER && a->browser_completion.suggesting &&
            (pressed(a, KEY_TAB) || pressed(a, KEY_ENTER) || pressed(a, KEY_KP_ENTER))) {
            app_browser_completion_accept(a, a->browser_index);
            return;
        }
        unsigned long before = a->field.version;
        field_keys(a, &a->field, chars, count, ctrl, shift);
        if (a->modal == M_PALETTE) {
            if (before != a->field.version)
                a->palette_index = 0;
            app_filter_palette(a);
            if (pressed(a, KEY_DOWN) && a->palette_index < a->filtered_count - 1)
                a->palette_index++;
            if (pressed(a, KEY_UP) && a->palette_index > 0)
                a->palette_index--;
        } else if (a->modal == M_BROWSER) {
            if (before != a->field.version) {
                app_browser_completion_cancel(a);
                a->browser_completion.suggesting = false;
                snprintf(a->browser_completion.status, sizeof(a->browser_completion.status),
                         "Ctrl+Espace : actualiser les suggestions");
            }
            if (pressed(a, KEY_DOWN) || pressed(a, KEY_UP)) {
                a->browser_index += pressed(a, KEY_DOWN) ? 1 : -1;
                if (a->browser_index < 0)
                    a->browser_index = 0;
                if (a->browser_index >= a->browser_count)
                    a->browser_index = a->browser_count ? a->browser_count - 1 : 0;
                if (a->browser_count && !a->browser_completion.suggesting)
                    app_set_field(&a->field, a->browser[a->browser_index].path, false);
                if (a->browser_index < a->browser_scroll)
                    a->browser_scroll = a->browser_index;
                if (a->browser_index > a->browser_scroll + 6)
                    a->browser_scroll = a->browser_index - 6;
            }
        }
        if (pressed(a, KEY_ENTER) || pressed(a, KEY_KP_ENTER)) {
            if (a->modal == M_PALETTE) {
                if (a->filtered_count)
                    action(a, A_PALETTE_CHOOSE, a->filtered[a->palette_index]);
            } else if (a->modal == M_BROWSER)
                browser_submit(a);
            else if (a->modal == M_FS_CREATE)
                context_create(a);
            else
                goto_submit(a);
        }
    } else if (a->modal == M_UNSAVED && pressed(a, KEY_ENTER))
        save(a, false);
}
static void editor_keys(App *a, const unsigned *chars, int count, bool ctrl, bool shift) {
    if (app_folder_search_shortcut(a, ctrl, shift))
        return;
    app_buffer(a)->xml_auto_disabled = !a->xml_auto;
    Buffer *b = app_buffer(a);
    if (pressed(a, KEY_F4) && !IsKeyDown(KEY_LEFT_ALT) && !IsKeyDown(KEY_RIGHT_ALT)) {
        action(a, A_LINT_RUN, 0);
        return;
    }
    if (pressed(a, KEY_F2)) {
        action(a, shift ? A_DIAG_PREV : A_DIAG_NEXT, 0);
        return;
    }
    if (ctrl && shift && pressed(a, KEY_M)) {
        action(a, A_PROBLEMS, 0);
        return;
    }
    if (pressed(a, KEY_F1)) {
        action(a, A_HELP, 0);
        return;
    }
    if (pressed(a, KEY_F5)) {
        action(a, shift ? A_STOP : A_RUN, 0);
        return;
    }
    if (pressed(a, KEY_F6)) {
        action(a, A_EFFECTS, 0);
        return;
    }
    if (pressed(a, KEY_F8)) {
        action(a, A_TOPBAR, 0);
        return;
    }
    if (pressed(a, KEY_F9)) {
        action(a, A_ZEN, 0);
        return;
    }
    if (pressed(a, KEY_F7) || (ctrl && pressed(a, KEY_BACKSLASH))) {
        action(a, A_SPLIT, 0);
        return;
    }
    if (ctrl && (pressed(a, KEY_ONE) || pressed(a, KEY_TWO))) {
        int pane = pressed(a, KEY_TWO) ? 1 : 0;
        if (pane && !a->split)
            action(a, A_SPLIT, 0);
        app_focus_pane(a, pane);
        return;
    }
    if (app_multicursor_keys(a, ctrl))
        return;
    if (ctrl) {
        struct {
            int key;
            Action id;
        } shortcuts[] = {{KEY_N, A_NEW},
                         {KEY_O, shift ? A_FOLDER : A_OPEN},
                         {KEY_S, shift ? A_SAVE_AS : A_SAVE},
                         {KEY_W, A_TAB_CLOSE},
                         {KEY_P, shift ? A_COMMANDS : A_QUICK_OPEN},
                         {KEY_K, A_COMMANDS},
                         {KEY_F, A_FIND},
                         {KEY_G, A_GOTO},
                         {KEY_B, A_SIDEBAR},
                         {KEY_J, A_TERMINAL},
                         {KEY_R, A_RUN},
                         {KEY_EQUAL, A_FONT_UP},
                         {KEY_KP_ADD, A_FONT_UP},
                         {KEY_MINUS, A_FONT_DOWN},
                         {KEY_KP_SUBTRACT, A_FONT_DOWN}};
        for (size_t i = 0; i < sizeof(shortcuts) / sizeof(shortcuts[0]); i++)
            if (pressed(a, shortcuts[i].key)) {
                action(a, shortcuts[i].id, a->active);
                return;
            }
        if (pressed(a, KEY_TAB)) {
            a->active = (a->active + (shift ? -1 : 1) + a->tab_count) % a->tab_count;
            invalidate(a);
            return;
        }
        if (pressed(a, KEY_GRAVE)) {
            a->zen_mode = false;
            a->terminal = true;
            a->focus = a->focus == 2 ? 0 : 2;
            app_layout(a);
            return;
        }
    }
    if (a->focus == 1) {
        if (pressed(a, KEY_ESCAPE)) {
            action(a, A_FIND_CLOSE, 0);
            return;
        }
        unsigned long version = a->search.version;
        field_keys(a, &a->search, chars, count, ctrl, shift);
        if (version != a->search.version) {
            app_update_matches(a);
            select_match(a, 1, true);
        }
        if (pressed(a, KEY_ENTER))
            select_match(a, shift ? -1 : 1, false);
        return;
    }
    if (a->focus == 2) {
        if (pressed(a, KEY_ESCAPE)) {
            a->focus = 0;
            return;
        }
        if (ctrl && shift && pressed(a, KEY_C)) {
            size_t n = 1;
            for (int i = 0; i < a->console_count; i++)
                n += strlen(a->console[i].text) + 1;
            char *all = malloc(n);
            if (all) {
                all[0] = 0;
                size_t p = 0;
                for (int i = 0; i < a->console_count; i++) {
                    size_t z = strlen(a->console[i].text);
                    memcpy(all + p, a->console[i].text, z);
                    p += z;
                    all[p++] = '\n';
                }
                all[p] = 0;
                SetClipboardText(all);
                free(all);
                app_toast(a, "Sortie copiée", false);
            }
            return;
        }
        field_keys(a, &a->input, chars, count, ctrl, shift);
        if (pressed(a, KEY_ENTER) && a->runner.running) {
            if (runner_input(&a->runner, a->input.text)) {
                char text[8192];
                snprintf(text, sizeof(text), "%.4000s%.4095s", a->output_partial, a->input.text);
                console_add(a, text, C_BLUE);
                a->output_length = 0;
                a->output_partial[0] = 0;
                app_set_field(&a->input, "", false);
                a->console_scroll = 0;
            } else
                app_toast(a, "La saisie n'a pas pu être envoyée (limite : 4 Ko).", true);
        }
        return;
    }
    const int completion_keys[] = {KEY_ESCAPE, KEY_DOWN, KEY_UP, KEY_ENTER, KEY_KP_ENTER, KEY_TAB};
    for (size_t i = 0; i < sizeof(completion_keys) / sizeof(completion_keys[0]); i++)
        if (pressed(a, completion_keys[i]) && app_completion_key(a, completion_keys[i], shift))
            return;
    unsigned long version = b->version;
    size_t cursor = b->cursor;
    if (ctrl) {
        if (pressed(a, KEY_A))
            buf_select_all(b);
        else if (pressed(a, KEY_C))
            copy_selected(b, false);
        else if (pressed(a, KEY_X))
            copy_selected(b, true);
        else if (pressed(a, KEY_V)) {
            const char *clip = GetClipboardText();
            if (clip) {
                char *norm = clipboard_copy(clip);
                if (norm) {
                    size_t p = 0;
                    for (size_t i = 0; norm[i]; i++) {
                        if (norm[i] == '\r') {
                            if (norm[i + 1] == '\n')
                                continue;
                            norm[p++] = '\n';
                        } else
                            norm[p++] = norm[i];
                    }
                    norm[p] = 0;
                    if (!buf_paste(b, norm))
                        app_toast(a, "Collage impossible : UTF-8 requis ou limite de 2 Mo.", true);
                    free(norm);
                }
            }
        } else if (app_history_keys(a, b, ctrl, shift)) {
            /* Visibility and completion invalidation run after every edit. */
        } else if (pressed(a, KEY_SLASH))
            buf_comment(b);
        else if (pressed(a, KEY_D)) {
            if (shift)
                buf_duplicate(b);
            else
                action(a, A_SELECT_NEXT, 0);
        } else if (shift && pressed(a, KEY_L))
            action(a, A_SELECT_OCCURRENCES, 0);
        else if (pressed(a, KEY_SPACE)) {
            if (!app_xml_begin(a, false))
                app_completion_show(a, true);
            return;
        }
    } else {
        for (int i = 0; i < count; i++) {
            if (!buf_type(b, chars[i]))
                app_toast(a, "Modification impossible : mémoire ou taille maximale.", true);
            emit(a);
        }
        if (pressed(a, KEY_ENTER) || pressed(a, KEY_KP_ENTER))
            buf_newline(b);
        if (pressed(a, KEY_TAB))
            buf_indent(b, shift);
        if (pressed(a, KEY_ESCAPE)) {
            if (b->extra_count) {
                buf_clear_carets(b);
                b->anchor = BUF_NO_SELECTION;
            } else if (a->find_open) {
                a->find_open = false;
                b->anchor = BUF_NO_SELECTION;
            } else if (IsWindowFullscreen())
                action(a, A_FULLSCREEN, 0);
            else
                b->anchor = BUF_NO_SELECTION;
            app_layout(a);
        }
    }
    if (pressed(a, KEY_BACKSPACE))
        buf_backspace(b, ctrl);
    if (pressed(a, KEY_DELETE))
        buf_delete(b, ctrl);
    if (pressed(a, KEY_LEFT))
        buf_horizontal(b, -1, ctrl, shift);
    if (pressed(a, KEY_RIGHT))
        buf_horizontal(b, 1, ctrl, shift);
    if (pressed(a, KEY_UP))
        buf_vertical(b, -1, shift);
    if (pressed(a, KEY_DOWN))
        buf_vertical(b, 1, shift);
    if (pressed(a, KEY_HOME))
        buf_home_end(b, false, ctrl, shift);
    if (pressed(a, KEY_END))
        buf_home_end(b, true, ctrl, shift);
    if (pressed(a, KEY_PAGE_UP) || pressed(a, KEY_PAGE_DOWN))
        buf_vertical(b, (pressed(a, KEY_PAGE_UP) ? -1 : 1) * (int)((float)a->code_h / a->line_h - 2), shift);
    if (version != b->version || cursor != b->cursor) {
        app_ensure_visible(a);
        if (version != b->version && (!ctrl || pressed(a, KEY_BACKSPACE)))
            app_completion_show(a, false);
        else
            app_completion_dismiss(a);
    }
}
/* Window shortcuts are consumed before ordinary mouse/modal/editor input.
   Search owns its F11 (and Alt+Enter alias); no native-window action leaks out. */
static bool window_shortcuts(App *a) {
    if (!pressed(a, KEY_F11) && !(IsKeyDown(KEY_LEFT_ALT) && pressed(a, KEY_ENTER)))
        return false;
    if (a->modal == M_FOLDER_SEARCH)
        app_folder_search_maximize(a);
    else
        app_action(a, A_FULLSCREEN, 0);
    return true;
}
static void update(App *a, float dt) {
    a->frame_tick++;
    app_ssh_poll(a);
    app_xml_poll(a);
    app_browser_completion_poll(a);
    app_folder_search_poll(a);
    poll_files(a);
    app_layout(a);
    Buffer *b = app_buffer(a);
    buf_index(b);
    if (WindowShouldClose() && !a->modal)
        request_quit(a);
    poll_runner(a);
    if (a->demo && !a->demo_started && GetTime() - a->start_time > 0.8) {
        a->demo_started = true;
        run_code(a);
    }
    if (!a->file_job && GetTime() - a->tree_time > 3)
        refresh_tree(a);
    memset(a->keys, 0, sizeof(a->keys));
    int key;
    while ((key = GetKeyPressed()))
        if (key >= 0 && key < 512)
            a->keys[key] = true;
    unsigned chars[128];
    int count = 0, c;
    while ((c = GetCharPressed()))
        if (c >= 32 && c != 127 && count < 128)
            chars[count++] = (unsigned)c;
    if (a->ssh_quit_pending || !a->running)
        return;
    bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL) || IsKeyDown(KEY_LEFT_SUPER) ||
                IsKeyDown(KEY_RIGHT_SUPER) || a->keys[KEY_LEFT_CONTROL] || a->keys[KEY_RIGHT_CONTROL] ||
                a->keys[KEY_LEFT_SUPER] || a->keys[KEY_RIGHT_SUPER];
    bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT) || a->keys[KEY_LEFT_SHIFT] ||
                 a->keys[KEY_RIGHT_SHIFT];
    if (IsKeyDown(KEY_RIGHT_ALT))
        ctrl = false; /* AltGr des claviers AZERTY */
    bool physical[KEYBOARD_KEYS], activity = false;
    memcpy(physical, a->keys, sizeof(physical));
    for (unsigned i = 1; i < KEYBOARD_KEYS; i++)
        if (physical[i]) {
            activity = true;
            break;
        }
    keyboard_poll(&a->keyboard, ctrl, activity, GetTime(), keyboard_native_read, NULL);
    memset(a->keys, 0, sizeof(a->keys));
    for (unsigned i = 1; i < KEYBOARD_KEYS; i++)
        if (physical[i]) {
            int logical = keyboard_logical(&a->keyboard, (int)i);
            if (logical)
                a->keys[logical] = true;
        }
    if (IsKeyDown(KEY_LEFT_ALT) && pressed(a, KEY_F4))
        action(a, A_QUIT, 0);
    bool toggled = window_shortcuts(a);
    bool clicked = mouse_update(a, ctrl, shift);
    if (!clicked && !toggled) {
        if (a->modal)
            modal_keys(a, chars, count, ctrl, shift);
        else
            editor_keys(a, chars, count, ctrl, shift);
    }
    app_layout(a);
    b = app_buffer(a);
    buf_index(b);
    app_prepare_syntax(a, a->active_pane);
    if (a->find_open)
        app_update_matches(a);
    for (int i = 0; i < a->particle_count;) {
        Particle *p = &a->particles[i];
        p->x += p->vx * dt;
        p->y += p->vy * dt;
        p->vy += 95 * dt;
        p->life -= dt;
        if (p->life <= 0)
            a->particles[i] = a->particles[--a->particle_count];
        else
            i++;
    }
    b = app_buffer(a); /* a tab or an entire workspace can have changed during input */
    if (!a->ssh_quit_pending)
        app_session_checkpoint(a, false);
    char title[512];
    snprintf(title, sizeof(title), "%s%s — Python Studio / C", buf_dirty(b) ? "● " : "", b->name);
    if (strcmp(title, a->title)) {
        strset(a->title, sizeof(a->title), title);
        SetWindowTitle(title);
    }
    if (a->running) {
        analysis_update(a, runner_clock());
        app_completion_poll(a);
    }
}
static void load_settings(App *a) {
    const char *base = getenv("PRISME_CONFIG_DIR");
    if (base && *base) {
        snprintf(a->config, sizeof(a->config), "%s/settings-c.cfg", base);
    } else {
#ifdef _WIN32
        base = getenv("APPDATA");
        if (!base)
            base = GetWorkingDirectory();
        char dir[BUF_PATH];
        path_join(dir, sizeof(dir), base, "prisme-c");
        MakeDirectory(dir);
#else
        base = getenv("HOME");
        if (!base)
            base = GetWorkingDirectory();
        char parent[BUF_PATH], dir[BUF_PATH];
        snprintf(parent, sizeof(parent), "%s/.config", base);
        MakeDirectory(parent);
        path_join(dir, sizeof(dir), parent, "prisme-c");
        MakeDirectory(dir);
#endif
        path_join(a->config, sizeof(a->config), dir, "settings.cfg");
    }
    FILE *f = fopen(a->config, "r");
    if (f) {
        int effects, minimap, sidebar, font, topbar = 1, terminal = 1, width = 226, height = 211,
                                             revision = 0, automatic = 1, xml_auto = 1;
        int fields = fscanf(f, "%d %d %d %d %d %d %d %d %d %d %d", &effects, &minimap, &sidebar, &font,
                            &topbar, &terminal, &width, &height, &revision, &automatic, &xml_auto);
        if (fields >= 4) {
            if (fields >= 11)
                a->xml_auto = xml_auto != 0;
            if (fields >= 10)
                a->lint_auto = automatic != 0;
            a->effects = effects != 0;
            a->minimap = minimap != 0;
            a->sidebar = sidebar != 0;
            a->font_size = ui_code_font_size(font, !(fields >= 9 && revision >= 2));
            if (fields >= 5)
                a->topbar = topbar != 0;
            if (fields >= 6)
                a->terminal = terminal != 0;
            if (fields >= 7 && width >= LAYOUT_SIDEBAR_MIN && width <= LAYOUT_SIDEBAR_MAX)
                a->sidebar_width = width;
            if (fields >= 8 && height >= 110 && height <= 4096)
                a->terminal_h = (float)height;
        }
        fclose(f);
    }
}
bool app_path_usable(App *a, const char *path) {
    char error[512] = "";
    if (ssh_guard(&a->ssh, path, false, error, sizeof(error)))
        return true;
    app_toast(a, error, true);
    return false;
}
void app_ssh_reset_analysis(App *a) {
    analysis_reset(a);
    ImportPaths roots = a->linter.imports;
    LintRules disabled = a->linter.disabled;
    OdooConfig odoo = a->linter.odoo;
    lint_close(&a->linter);
    lint_init(&a->linter);
    a->linter.imports = roots;
    a->linter.disabled = disabled;
    a->linter.odoo = odoo;
    completion_reset(&a->completer);
    app_completion_dismiss(a);
}
void app_ssh_change_project(App *a, const char *path) {
    if (!app_path_usable(a, path))
        return;
#ifndef _WIN32
    char actual[BUF_PATH];
    if (realpath(path, actual))
        path = actual;
    if (!app_path_usable(a, path))
        return;
#endif
    app_folder_search_cancel(a);
    strset(a->project, sizeof(a->project), path);
    if (!ssh_owns(&a->ssh, path))
        strset(a->last_local_project, sizeof(a->last_local_project), path);
    if (ssh_owns(&a->ssh, path)) {
        char caption[256];
        ssh_profile_label(&a->ssh.profile, caption, sizeof(caption));
        snprintf(a->project_name, sizeof(a->project_name), "SSH · %.246s", caption);
    } else
        strset(a->project_name, sizeof(a->project_name), fs_basename(path));
    a->expanded_count = 0;
    a->tree_scroll = 0;
    load_project_imports(a);
    refresh_tree(a);
    invalidate(a);
}
void app_ssh_drop_remote_tabs(App *a) {
    for (int i = a->tab_count - 1; i >= 0; i--)
        if (ssh_owns(&a->ssh, a->tabs[i]->path))
            remove_tab(a, i);
    if (ssh_owns(&a->ssh, a->project))
        app_ssh_change_project(a, a->last_local_project);
}
static bool resource_root(char *assets, size_t cap, const AppOptions *o) {
    if (o->assets) {
        absolute_path(assets, cap, o->assets, GetWorkingDirectory());
        return DirectoryExists(assets);
    }
    char candidate[BUF_PATH];
    snprintf(candidate, sizeof(candidate), "%sassets", GetApplicationDirectory());
    if (DirectoryExists(candidate)) {
        strset(assets, cap, candidate);
        return true;
    }
    snprintf(candidate, sizeof(candidate), "%s../assets", GetApplicationDirectory());
    if (DirectoryExists(candidate)) {
        absolute_path(assets, cap, candidate, GetWorkingDirectory());
        return true;
    }
    absolute_path(assets, cap, "assets", GetWorkingDirectory());
    return DirectoryExists(assets);
}
int app_run(const AppOptions *o) {
    App *a = calloc(1, sizeof(*a));
    if (!a) {
        fprintf(stderr, "Mémoire insuffisante.\n");
        return 1;
    }
    if (!resource_root(a->assets, sizeof(a->assets), o)) {
        fprintf(stderr, "Dossier assets introuvable. Utilisez --assets chemin/vers/assets.\n");
        free(a);
        return 1;
    }
    studio_store_init(&a->session, NULL, !o->no_session);
    char previous_project[BUF_PATH] = "";
    bool have_previous = !o->project && !o->file &&
                         studio_last_project(&a->session, previous_project, sizeof(previous_project));
    char root[BUF_PATH];
    strset(root, sizeof(root), native_parent(a->assets));
    if (o->project)
        absolute_path(a->project, sizeof(a->project), o->project, GetWorkingDirectory());
    else if (o->file) {
        char path[BUF_PATH];
        absolute_path(path, sizeof(path), o->file, GetWorkingDirectory());
        strset(a->project, sizeof(a->project), native_parent(path));
    } else if (have_previous)
        strset(a->project, sizeof(a->project), previous_project);
    else
        path_join(a->project, sizeof(a->project), root, "examples");
    if (!have_previous && !DirectoryExists(a->project)) {
        fprintf(stderr, "Dossier introuvable : %s\n", a->project);
        free(a);
        return 1;
    }
    strset(a->project_name, sizeof(a->project_name),
           !strcmp(fs_basename(a->project), "examples") ? "mon-atelier" : fs_basename(a->project));
    const char *python = o->python ? o->python : getenv("PRISME_PYTHON");
#ifdef _WIN32
    strset(a->python, sizeof(a->python), python && *python ? python : "python");
#else
    strset(a->python, sizeof(a->python), python && *python ? python : "python3");
#endif
    /* A relative interpreter path is relative to the launch directory, not the
       analyzed file/project directory used by the child process. */
    if (strchr(a->python, '/')
#ifdef _WIN32
        || strchr(a->python, '\\')
#endif
    ) {
        char interpreter[BUF_PATH];
        strset(interpreter, sizeof(interpreter), a->python);
        absolute_path(a->python, sizeof(a->python), interpreter, GetWorkingDirectory());
    }
    a->effects = true;
    a->lint_auto = true;
    a->xml_auto = true;
    a->minimap = true;
    a->sidebar = true;
    a->topbar = true;
    a->sidebar_width = LAYOUT_SIDEBAR_DEFAULT;
    a->font_size = UI_CODE_DEFAULT;
    a->terminal = true;
    a->terminal_h = 211;
    a->tree_root_open = true;
    a->caret_x = -1;
    a->split_ratio = 0.5f;
    load_settings(a);
    runner_init(&a->runner);
    lint_init(&a->linter);
    app_lint_rules_load(a);
    completion_init(&a->completer);
    load_project_imports(a);
    if (o->import_count < 0 || o->import_count > IMPORT_PATHS_MAX) {
        free(a);
        return 2;
    }
    for (int i = 0; i < o->import_count; i++) {
        char path[BUF_PATH], error[512] = {0};
        if (!o->import_dirs[i] || strlen(o->import_dirs[i]) >= BUF_PATH) {
            fprintf(stderr, "Chemin d’import vide ou trop long.\n");
            free(a);
            return 2;
        }
        absolute_path(path, sizeof(path), o->import_dirs[i], GetWorkingDirectory());
        if (!DirectoryExists(path)) {
            fprintf(stderr, "Dossier d’import introuvable : %s\n", path);
            free(a);
            return 2;
        }
        bool duplicate = false;
        for (int j = 0; j < a->linter.imports.count; j++)
            if (!strcmp(path, a->linter.imports.paths[j]))
                duplicate = true;
        if (!duplicate && !import_paths_add(&a->linter.imports, path, error, sizeof(error))) {
            fprintf(stderr, "%s\n", error);
            free(a);
            return 2;
        }
    }
    if (o->no_lint || o->safe_mode)
        a->lint_auto = false;
    if (o->no_effects)
        a->effects = false;
    if (o->safe_mode) {
        a->effects = false;
        a->minimap = false;
    }
    /* Only the editor is frameless; the standalone graphics demo is unchanged. */
    SetConfigFlags(FLAG_WINDOW_UNDECORATED);
    if (!graphics_open(o->width, o->height, "Python Studio — édition C", o->msaa && !o->safe_mode,
                       o->verbose_gl)) {
        free(a);
        return PRISME_GRAPHICS_ERROR;
    }
    a->windowed_w = GetScreenWidth();
    a->windowed_h = GetScreenHeight();
    SetWindowMinSize(960, 640);
    if (!o->windowed && !o->fullscreen && !o->frames)
        MaximizeWindow();
    SetExitKey(KEY_NULL);
    SetTargetFPS(o->safe_mode ? 30 : 60);
    if (!ui_init(&a->ui, a->assets)) {
        fprintf(stderr, "Chargement des polices impossible.\n");
        CloseWindow();
        free(a);
        return 1;
    }
    char ssh_helper[BUF_PATH];
    snprintf(ssh_helper, sizeof(ssh_helper), "%spython-studio-ssh-helper", GetApplicationDirectory());
    app_ssh_init(a, ssh_helper);
    buf_init(&a->field, "", "champ");
    buf_init(&a->search, "", "recherche");
    buf_init(&a->input, "", "console");
    buf_init(&a->snippet_trigger, "", "déclencheur");
    buf_init(&a->snippet_title, "", "titre");
    buf_init(&a->snippet_body, "", "extrait");
    app_snippets_load(a);
    a->running = true;
    if (*a->import_error)
        app_toast(a, a->import_error, true);
    if (*a->snippet_error)
        app_toast(a, a->snippet_error, true);
    a->demo = o->demo;
    a->start_time = GetTime();
    bool session_restored = app_session_restore(a);
    char initial[BUF_PATH];
    if (o->file)
        absolute_path(initial, sizeof(initial), o->file, GetWorkingDirectory());
    else
        path_join(initial, sizeof(initial), a->project, "main.py");
    if (o->file || !session_restored)
        open_file(a, initial, true);
    if (!a->tab_count)
        new_file(a);
    if (!session_restored && !o->file) {
        char other[BUF_PATH];
        path_join(other, sizeof(other), a->project, "couleurs.py");
        if (FileExists(other)) {
            open_file(a, other, true);
            a->active = 0;
        }
        if (!strcmp(app_buffer(a)->name, "main.py"))
            buf_move(app_buffer(a), buf_position(app_buffer(a), 5, 4), false);
    }
    char engine[160];
    snprintf(engine, sizeof(engine),
             "PYTHON STUDIO C " PRISME_VERSION " / C11 + raylib 5.5 / OpenGL %s / UTF-8", graphics_version());
    console_add(a, engine, C_DIM);
    console_add(a, "Votre atelier est prêt. Exécutez un fichier Python avec F5.", C_MUTED);
    if (!session_restored) {
        a->panes[0].tab = a->active;
        a->panes[1].tab = a->active;
    }
    if (o->safe_mode) {
        a->minimap = false;
        a->effects = false;
        a->lint_auto = false;
    }
    a->session_ready = true;
    if (o->split && !a->split)
        action(a, A_SPLIT, 0);
    if (o->fullscreen)
        action(a, A_FULLSCREEN, 0);
    if (o->zen)
        action(a, A_ZEN, 0);
    refresh_tree(a);
    app_layout(a);
    int frames = 0;
    while (a->running) {
        float dt = fminf(0.05f, GetFrameTime());
        update(a, dt);
        app_draw(a, dt);
        frames++;
        if (o->frames && frames >= o->frames) {
            if (o->screenshot) {
                Image image = LoadImageFromScreen();
                ExportImage(image, o->screenshot);
                UnloadImage(image);
            }
            break;
        }
    }
    file_job_free(a->file_job);
    a->file_job = NULL;
    if (!a->ssh_quit_pending)
        app_session_checkpoint(a, true);
    studio_store_close(&a->session);
    app_path_completion_shutdown(a);
    app_browser_completion_shutdown(a);
    app_xml_shutdown(a);
    app_folder_search_shutdown(a);
    app_ssh_free(a);
    runner_close(&a->runner);
    lint_close(&a->linter);
    completion_close(&a->completer);
    for (int i = 0; i < a->tab_count; i++)
        analysis_release(a, i);
    FILE *f = o->safe_mode ? NULL : fopen(a->config, "w");
    if (f) {
        fprintf(f, "%d %d %d %d %d %d %d %d %d %d %d\n", a->effects, a->minimap,
                a->zen_mode ? a->zen_sidebar : a->sidebar, a->font_size,
                a->zen_mode ? a->zen_topbar : a->topbar, a->zen_mode ? a->zen_terminal : a->terminal,
                a->sidebar_width, (int)a->terminal_h, 3, a->lint_auto, a->xml_auto);
        fclose(f);
    }
    for (int i = 0; i < a->tab_count; i++) {
        buf_free(a->tabs[i]);
        free(a->tabs[i]);
    }
    for (int i = 0; i < a->console_count; i++)
        free(a->console[i].text);
    buf_free(&a->field);
    buf_free(&a->search);
    buf_free(&a->input);
    buf_free(&a->snippet_trigger);
    buf_free(&a->snippet_title);
    buf_free(&a->snippet_body);
    snippets_free(&a->snippets);
    free(a->panes[0].colors);
    free(a->panes[1].colors);
    ui_free(&a->ui);
    CloseWindow();
    free(a);
    return 0;
}
