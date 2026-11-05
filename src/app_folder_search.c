#include "app_internal.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Credential-free immutable lease; only these three owner indicators are read
   by the worker. The owner and this lease outlive the outstanding job. */
static bool search_guard(void *user, const char *path, bool leaf, char *error, size_t cap) {
    XmlReadLease *lease = user;
    SshSession view = {0};
    view.initialized = lease->initialized;
    strcpy(view.cache, lease->cache);
    strcpy(view.mountpoint, lease->mountpoint);
    long generation = atomic_load(&lease->owner->generation);
    SshState state = atomic_load(&lease->owner->state);
    bool alive = atomic_load(&lease->owner->process_alive);
    atomic_init(&view.generation, lease->generation);
    atomic_init(&view.state, generation == lease->generation ? state : SSH_LOST);
    atomic_init(&view.process_alive, alive && generation == lease->generation);
    return ssh_guard(&view, path, leaf, error, cap);
}
static bool pressed(App *a, int key) {
    return app_key_pressed(a, key);
}
void app_folder_search_cancel(App *a) {
    folder_search_cancel(a->folder_search.job);
    a->folder_search.active = false;
    a->folder_search.scroll_drag = 0;
    if (a->folder_search.report && !a->folder_search.report->complete)
        a->folder_search.report->cancelled = true;
}
static void clear_results(FolderSearchState *s) {
    folder_search_report_free(s->report);
    s->report = NULL;
    buf_free(&s->view);
    s->file = s->hit = -1;
    s->scroll = s->x = s->y = s->extent_columns = 0;
    s->scroll_drag = 0;
    s->htrack = s->hthumb = s->vtrack = s->vthumb = (Rectangle){0};
}
static bool ensure(App *a) {
    FolderSearchState *s = &a->folder_search;
    if (s->initialized && !strcmp(s->project, a->project))
        return true;
    app_folder_search_restore_window(a);
    app_folder_search_cancel(a);
    clear_results(s);
    for (unsigned i = 0; i < 4; i++)
        buf_free(&s->filters[i]);
    buf_free(&s->folder);
    buf_free(&s->query);
    strcpy(s->project, a->project);
    s->profile[0] = s->error[0] = s->note[0] = 0;
    char root[FS_PATH];
    strcpy(root, a->project);
    if (folder_search_profile_file(s->profile, sizeof(s->profile), a->config, a->project))
        folder_search_profile_load(root, s->profile, a->project, s->error, sizeof(s->error));
    else
        snprintf(s->error, sizeof(s->error), "Chemin des préférences de recherche invalide.");
    s->initialized = buf_init(&s->folder, root, "dossier") && buf_init(&s->query, "", "recherche");
    for (unsigned i = 0; i < 4 && s->initialized; i++)
        s->initialized = buf_init(&s->filters[i], "", "filtre");
    if (!s->initialized) {
        for (unsigned i = 0; i < 4; i++)
            buf_free(&s->filters[i]);
        buf_free(&s->folder);
        buf_free(&s->query);
        app_toast(a, "Mémoire insuffisante pour la recherche.", true);
        return false;
    }
    s->options = folder_search_options();
    s->maximized = s->filters_open = false;
    s->folder.cursor = s->folder.len;
    s->focus = 1;
    s->rows = s->list_rows = 8;
    s->columns = 50;
    return true;
}
SearchOptions app_folder_search_options(const App *a) {
    const FolderSearchState *s = &a->folder_search;
    SearchOptions options = s->options;
    char *values[] = {options.include_files, options.exclude_files, options.include_dirs,
                      options.exclude_dirs};
    for (unsigned i = 0; i < 4; i++)
        snprintf(values[i], SEARCH_FILTER_MAX + 1, "%s", s->filters[i].text ? s->filters[i].text : "");
    return options;
}
void app_folder_search_option(App *a, int index) {
    if (!ensure(a) || index < 0 || index > 3)
        return;
    FolderSearchState *s = &a->folder_search;
    bool *flags[] = {&s->options.case_sensitive, &s->options.whole_word, &s->options.regex,
                     &s->options.recursive};
    *flags[index] = !*flags[index];
    s->focus = 8 + index;
    app_folder_search_cancel(a);
    snprintf(s->note, sizeof(s->note), "Options modifiées : relancez la recherche.");
}
void app_folder_search_restore_window(App *a) {
    /* Lifecycle hook only: search never changes the editor's native window. */
    (void)a;
}
void app_folder_search_fullscreen(App *a) {
    /* F11 is contextual. The global editor shortcut is unchanged. */
    app_folder_search_maximize(a);
}
void app_folder_search_maximize(App *a) {
    if (ensure(a)) {
        a->folder_search.scroll_drag = 0;
        a->folder_search.maximized = !a->folder_search.maximized;
    }
}
void app_folder_search_show(App *a) {
    if (!ensure(a))
        return;
    app_completion_dismiss(a);
    app_multicursor_reset(a);
    a->modal = M_FOLDER_SEARCH;
    a->focus = 0;
    a->dragging = false;
    a->folder_search.focus = 1;
    buf_select_all(&a->folder_search.query);
}
bool app_folder_search_shortcut(App *a, bool ctrl, bool shift) {
    if (!ctrl || !shift || !pressed(a, KEY_F))
        return false;
    app_folder_search_show(a);
    return true;
}
static bool valid(App *a) {
    FolderSearchState *s = &a->folder_search;
    SearchOptions options = app_folder_search_options(a);
    return s->active && s->job && s->initialized && a->modal == M_FOLDER_SEARCH &&
           !strcmp(a->project, s->project) && !strcmp(s->folder.text, s->job->root) &&
           !strcmp(s->query.text, s->job->query) && folder_search_options_equal(&options, &s->job->options) &&
           atomic_load(&a->ssh.generation) == s->lease.generation &&
           atomic_load(&a->ssh.state) == s->ssh_state && !strcmp(a->ssh.cache, s->lease.cache) &&
           !strcmp(a->ssh.mountpoint, s->lease.mountpoint) &&
           (s->ssh_state != SSH_MOUNTED || ssh_mounted(&a->ssh));
}
void app_folder_search_select(App *a, int file, int hit) {
    FolderSearchState *s = &a->folder_search;
    if (!s->report || file < 0 || (unsigned)file >= s->report->count || hit < 0 ||
        (unsigned)hit >= s->report->files[file].count)
        return;
    SearchFile *f = &s->report->files[file];
    s->scroll_drag = 0;
    if (s->file != file) {
        buf_free(&s->view);
        if (!buf_init(&s->view, f->text, fs_basename(f->path))) {
            snprintf(s->error, sizeof(s->error), "Mémoire insuffisante pour afficher le fichier.");
            s->file = s->hit = -1;
            return;
        }
        buf_index(&s->view);
        s->extent_columns = folder_search_columns(s->view.text, s->view.len) + 1;
        s->x = s->y = 0;
    }
    s->file = file;
    s->hit = hit;
    s->focus = 2;
    if ((unsigned)file < s->scroll)
        s->scroll = (unsigned)file;
    if ((unsigned)file >= s->scroll + s->list_rows)
        s->scroll = (unsigned)file - s->list_rows + 1;
    SearchHit *match = &f->hits[hit];
    if (match->line < s->y || match->line >= s->y + s->rows)
        s->y = match->line > s->rows / 2 ? match->line - s->rows / 2 : 0;
    if (match->column < s->x || match->column + (match->width ? match->width : 1) > s->x + s->columns) {
        unsigned margin = s->columns / 4 > 4 ? s->columns / 4 : 4;
        s->x = match->column > margin ? match->column - margin : 0;
    }
}
void app_folder_search_next(App *a, int direction) {
    FolderSearchState *s = &a->folder_search;
    int file = s->file, hit = s->hit;
    if (folder_search_next(s->report, &file, &hit, direction))
        app_folder_search_select(a, file, hit);
}
static void publish_report(App *a, SearchReport *report) {
    FolderSearchState *s = &a->folder_search;
    SearchReport *old = s->report;
    int selected = -1, top = -1;
    if (old)
        for (unsigned i = 0; i < report->count; i++) {
            if (s->file >= 0 && (unsigned)s->file < old->count &&
                !strcmp(report->files[i].path, old->files[s->file].path))
                selected = (int)i;
            if (s->scroll < old->count && !strcmp(report->files[i].path, old->files[s->scroll].path))
                top = (int)i;
        }
    s->report = report;
    if (selected >= 0) {
        /* The same immutable file: keep the hit, view buffer, typing focus,
           horizontal/vertical viewport, and top-visible filename anchor. */
        s->file = selected;
        if (top >= 0)
            s->scroll = (unsigned)top;
    } else if (report->count) {
        int focus = s->focus;
        s->file = -1;
        app_folder_search_select(a, 0, 0);
        s->focus = focus;
    }
    folder_search_report_free(old);
}
void app_folder_search_poll(App *a) {
    FolderSearchState *s = &a->folder_search;
    if (s->initialized && strcmp(s->project, a->project)) {
        app_folder_search_restore_window(a);
        app_folder_search_cancel(a);
        clear_results(s);
        for (unsigned i = 0; i < 4; i++)
            buf_free(&s->filters[i]);
        buf_free(&s->folder);
        buf_free(&s->query);
        s->initialized = false;
        if (a->modal == M_FOLDER_SEARCH) {
            a->modal = M_NONE;
            a->focus = 0;
        }
    }
    if (s->active && !valid(a)) {
        app_folder_search_cancel(a);
        snprintf(s->error, sizeof(s->error),
                 "Recherche annulée : le dossier, le texte, les options ou la connexion a changé.");
    }
    if (!s->job)
        return;
    if (!atomic_load(&s->job->done)) {
        if (valid(a)) {
            SearchReport *view = folder_search_snapshot(s->job, s->report ? s->report->count : 0);
            if (view)
                publish_report(a, view);
        }
        return;
    }
    if (valid(a)) {
        SearchReport *report = s->job->report ? s->job->report : s->job->partial_report;
        if (report && (report->count || !*s->job->error)) {
            s->job->report = s->job->partial_report = NULL;
            publish_report(a, report);
        }
        if (*s->job->error) {
            snprintf(s->error, sizeof(s->error), "%s", s->job->error);
            if (s->report && s->report->count)
                snprintf(s->note, sizeof(s->note), "Résultats partiels : la recherche a été interrompue.");
        } else if (s->report && !s->report->count && !s->report->cancelled)
            snprintf(s->note, sizeof(s->note), "Aucun fichier contenant cette chaîne.");
    }
    s->active = false;
    folder_search_free(s->job);
    s->job = NULL;
}
void app_folder_search_start(App *a) {
    if (!ensure(a))
        return;
    FolderSearchState *s = &a->folder_search;
    if (s->job) {
        if (!atomic_load(&s->job->done)) {
            snprintf(s->error, sizeof(s->error),
                     "La recherche précédente termine son annulation. Réessayez dans un instant.");
            return;
        }
        app_folder_search_poll(a);
    }
    char root[FS_PATH];
    if (!folder_search_root(root, s->folder.text, a->project, s->error, sizeof(s->error)))
        return;
    if (!folder_search_query_valid(s->query.text)) {
        snprintf(s->error, sizeof(s->error),
                 "Texte non vide requis : 1 024 octets UTF-8 maximum, sans retour à la ligne.");
        return;
    }
    SearchOptions options = app_folder_search_options(a);
    s->error[0] = s->note[0] = 0;
    char preferences_error[512] = "";
    folder_search_profile_save(root, s->profile, a->project, preferences_error, sizeof(preferences_error));
    app_set_field(&s->folder, root, false);
    clear_results(s);
    s->lease.owner = &a->ssh;
    s->lease.initialized = a->ssh.initialized;
    s->lease.generation = atomic_load(&a->ssh.generation);
    s->ssh_state = atomic_load(&a->ssh.state);
    strcpy(s->lease.cache, a->ssh.cache);
    strcpy(s->lease.mountpoint, a->ssh.mountpoint);
    s->job = folder_search_start_options(root, s->query.text, NULL, &options, search_guard, &s->lease,
                                         s->error, sizeof(s->error));
    s->active = s->job != NULL;
    if (*preferences_error && !*s->error)
        snprintf(s->error, sizeof(s->error), "Dossier non mémorisé : %.450s", preferences_error);
    s->focus = 1;
}
void app_folder_search_stop(App *a) {
    folder_search_cancel(a->folder_search.job);
    snprintf(a->folder_search.note, sizeof(a->folder_search.note),
             "Arrêt demandé : les résultats déjà trouvés seront conservés.");
}
void app_folder_search_picked(App *a, const char *root) {
    if (!ensure(a))
        return;
    FolderSearchState *s = &a->folder_search;
    char normalized[FS_PATH];
    if (!folder_search_root(normalized, root, a->project, s->error, sizeof(s->error)))
        return;
    app_folder_search_cancel(a);
    app_set_field(&s->folder, normalized, false);
    s->error[0] = 0;
    folder_search_profile_save(normalized, s->profile, a->project, s->error, sizeof(s->error));
    app_folder_search_show(a);
}
static unsigned horizontal_limit(FolderSearchState *s) {
    if (!s->view.text)
        return 0;
    if (!s->extent_columns)
        s->extent_columns = folder_search_columns(s->view.text, s->view.len) + 1;
    return s->extent_columns > s->columns ? s->extent_columns - s->columns : 0;
}
bool app_folder_search_mouse(App *a, Vector2 m, bool pressed, bool down, bool released) {
    FolderSearchState *s = &a->folder_search;
    if (!s->initialized || a->modal != M_FOLDER_SEARCH) {
        s->scroll_drag = 0;
        return false;
    }
    if (s->scroll_drag) {
        bool horizontal = s->scroll_drag == 1;
        if (down || released) {
            unsigned position = layout_scroll_position(horizontal ? s->horizontal : s->vertical,
                                                       horizontal ? m.x : m.y, s->scroll_grab);
            if (horizontal)
                s->x = position;
            else
                s->y = position;
        }
        if (!down)
            s->scroll_drag = 0;
        return true;
    }
    if (!pressed)
        return false;
    for (int axis = 1; axis <= 2; axis++) {
        bool horizontal = axis == 1;
        Rectangle track = horizontal ? s->htrack : s->vtrack, thumb = horizontal ? s->hthumb : s->vthumb;
        if (!ui_inside(track, m))
            continue;
        s->focus = 3;
        ScrollAxis model = horizontal ? s->horizontal : s->vertical;
        if (model.limit && model.span > model.thumb_span) {
            float pointer = horizontal ? m.x : m.y, start = horizontal ? thumb.x : thumb.y;
            s->scroll_grab = ui_inside(thumb, m) ? pointer - start : model.thumb_span / 2;
            s->scroll_drag = down ? axis : 0;
            unsigned position = layout_scroll_position(model, pointer, s->scroll_grab);
            if (horizontal)
                s->x = position;
            else
                s->y = position;
        }
        return true;
    }
    return false;
}
/* Clamp BEFORE converting a wheel delta to an integer, including very large
   finite deltas. Keep fractional wheel semantics (truncate the delta). */
static unsigned scroll_wheel(unsigned position, unsigned limit, float wheel, unsigned step) {
    if (position > limit)
        position = limit;
    double delta = (double)wheel * (double)step;
    if (delta > 0)
        return delta >= position ? 0 : position - (unsigned)delta;
    return -delta >= limit - position ? limit : position + (unsigned)(-delta);
}
void app_folder_search_scroll(App *a, Vector2 position, float wheel, bool shift) {
    FolderSearchState *s = &a->folder_search;
    if (!s->initialized || !isfinite(wheel))
        return;
    if (ui_inside(s->list_rect, position)) {
        unsigned count = s->report ? s->report->count : 0;
        unsigned last = count > s->list_rows ? count - s->list_rows : 0;
        s->scroll = scroll_wheel(s->scroll, last, wheel, 3);
    } else if (ui_inside(s->content_rect, position)) {
        unsigned last = shift                          ? horizontal_limit(s)
                        : s->view.line_count > s->rows ? (unsigned)s->view.line_count - s->rows
                                                       : 0;
        if (shift)
            s->x = scroll_wheel(s->x, last, wheel, 5);
        else
            s->y = scroll_wheel(s->y, last, wheel, 3);
    }
}
void app_folder_search_keys(App *a, const unsigned *chars, int count, bool ctrl, bool shift) {
    FolderSearchState *s = &a->folder_search;
    if (!s->initialized)
        return;
    if (pressed(a, KEY_F11)) {
        app_folder_search_fullscreen(a);
        return;
    }
    if (pressed(a, KEY_F3)) {
        app_folder_search_next(a, shift ? -1 : 1);
        return;
    }
    if (!ctrl && pressed(a, KEY_TAB)) {
        const int order[] = {0, 1, 8, 9, 10, 11, 4, 5, 6, 7, 2, 3};
        unsigned i = 0;
        while (i < 12 && order[i] != s->focus)
            i++;
        s->focus = order[(i + (shift ? 11 : 1)) % 12];
        if (s->focus >= 4 && s->focus < 8)
            s->filters_open = true;
        return;
    }
    if (ctrl && pressed(a, KEY_ENTER)) {
        app_folder_search_start(a);
        return;
    }
    if (s->focus >= 8 && s->focus < 12) {
        if (!ctrl && pressed(a, KEY_SPACE))
            app_folder_search_option(a, s->focus - 8);
        return;
    }
    if (s->focus < 2 || (s->focus >= 4 && s->focus < 8)) {
        Buffer *field = s->focus == 0 ? &s->folder : s->focus == 1 ? &s->query : &s->filters[s->focus - 4];
        char before[FS_PATH];
        snprintf(before, sizeof(before), "%s", field->text);
        size_t cursor = field->cursor, anchor = field->anchor;
        app_edit_field(a, field, chars, count, ctrl, shift);
        size_t limit = s->focus == 0 ? FS_PATH - 1u : s->focus == 1 ? SEARCH_QUERY_MAX : SEARCH_FILTER_MAX;
        if (field->len > limit) {
            app_set_field(field, before, false);
            field->cursor = cursor;
            field->anchor = anchor;
            snprintf(s->error, sizeof(s->error), "Limite du champ : %zu octets UTF-8.", limit);
        } else if (strcmp(before, field->text) && s->active) {
            app_folder_search_cancel(a);
            snprintf(s->note, sizeof(s->note), "Paramètres modifiés : relancez la recherche.");
        }
        if (pressed(a, KEY_ENTER) || pressed(a, KEY_KP_ENTER))
            app_folder_search_start(a);
    } else if (s->focus == 2 && s->report && s->report->count) {
        if (pressed(a, KEY_UP) || pressed(a, KEY_DOWN)) {
            int index = s->file + (pressed(a, KEY_DOWN) ? 1 : -1);
            if (index < 0)
                index = 0;
            if ((unsigned)index >= s->report->count)
                index = (int)s->report->count - 1;
            app_folder_search_select(a, index, 0);
        }
    } else if (s->focus == 3) {
        int delta = (pressed(a, KEY_DOWN) ? 1 : 0) - (pressed(a, KEY_UP) ? 1 : 0) +
                    ((pressed(a, KEY_PAGE_DOWN) ? 1 : 0) - (pressed(a, KEY_PAGE_UP) ? 1 : 0)) * (int)s->rows;
        int next = (int)s->y + delta;
        unsigned last = s->view.line_count > s->rows ? (unsigned)(s->view.line_count - s->rows) : 0;
        s->y = next < 0 ? 0 : (unsigned)next > last ? last : (unsigned)next;
        int x = (int)s->x + ((pressed(a, KEY_RIGHT) ? 1 : 0) - (pressed(a, KEY_LEFT) ? 1 : 0)) * 4;
        unsigned right = horizontal_limit(s);
        s->x = x < 0 ? 0 : (unsigned)x > right ? right : (unsigned)x;
    }
}
void app_folder_search_shutdown(App *a) {
    FolderSearchState *s = &a->folder_search;
    app_folder_search_cancel(a);
    folder_search_free(s->job);
    s->job = NULL;
    clear_results(s);
    for (unsigned i = 0; i < 4; i++)
        buf_free(&s->filters[i]);
    buf_free(&s->folder);
    buf_free(&s->query);
    s->initialized = false;
}
