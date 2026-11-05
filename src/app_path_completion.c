#include "app_internal.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Same immutable, credential-free lease as explicit XML reading. Only atomic
   generation/state/process indicators are read from the owning SSH session. */
static bool lease_guard(void *user, const char *path, bool leaf, char *error, size_t cap) {
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
static bool roots_valid(App *a, const PathCompletionState *x) {
    if (a->xml_paths.count != x->xml_paths.count || a->linter.imports.count != x->imports.count)
        return false;
    for (int i = 0; i < a->xml_paths.count; i++)
        if (strcmp(a->xml_paths.paths[i], x->xml_paths.paths[i]))
            return false;
    for (int i = 0; i < a->linter.imports.count; i++)
        if (strcmp(a->linter.imports.paths[i], x->imports.paths[i]))
            return false;
    return true;
}
static bool valid(App *a) {
    PathCompletionState *x = &a->path_completion;
    Buffer *b = x->document;
    return x->active && roots_valid(a, x) && a->completion_active &&
           x->generation == a->completion_generation && !a->modal && !a->focus && a->active >= 0 &&
           a->active < a->tab_count && a->tabs[a->active] == b && a->active_pane == a->completion_pane && b &&
           b->version == a->completion_version && b->cursor == a->completion_cursor && !b->extra_count &&
           (b->anchor == BUF_NO_SELECTION || b->anchor == b->cursor) && !strcmp(a->project, x->project) &&
           !strcmp(b->path, x->document_path) && !strcmp(b->name, x->name) &&
           atomic_load(&a->ssh.generation) == x->lease.generation &&
           atomic_load(&a->ssh.state) == x->ssh_state && !strcmp(a->ssh.mountpoint, x->lease.mountpoint) &&
           (x->ssh_state != SSH_MOUNTED || ssh_mounted(&a->ssh));
}
void app_path_completion_cancel(App *a) {
    PathCompletionState *x = &a->path_completion;
    path_job_cancel(x->job);
    free(x->result);
    x->result = NULL;
    x->active = false;
    x->document = NULL;
}
void app_path_completion_shutdown(App *a) {
    app_path_completion_cancel(a);
    path_job_free(a->path_completion.job);
    a->path_completion.job = NULL;
}
static bool add(char paths[][FS_PATH], int *count, const char *root, const char *directory) {
    char path[FS_PATH];
    int n = root ? snprintf(path, sizeof(path), "%s/%s", root, directory)
                 : snprintf(path, sizeof(path), "%s", directory);
    if (n < 1 || n >= FS_PATH)
        return false;
    for (int i = 0; i < *count; i++)
        if (!strcmp(paths[i], path))
            return true;
    if (*count == PATH_ROOTS_MAX)
        return false;
    strcpy(paths[(*count)++], path);
    return true;
}
bool app_path_completion_begin(App *a) {
    Buffer *b = app_buffer(a);
    PathContext ctx;
    if (!b || a->modal || a->focus || b->extra_count ||
        (b->anchor != BUF_NO_SELECTION && b->anchor != b->cursor) ||
        !path_completion_context(b->text, b->len, b->cursor, &ctx))
        return false;
    PathCompletionState *x = &a->path_completion;
    if (x->job) {
        if (!atomic_load(&x->job->done)) {
            app_completion_dismiss(a);
            app_toast(a, "La lecture du dossier précédent termine son annulation. Réessayez dans un instant.",
                      false);
            return true;
        }
        path_job_free(x->job);
        x->job = NULL;
    }
    app_completion_dismiss(a);
    char paths[PATH_ROOTS_MAX][FS_PATH], parent[FS_PATH], home_path[FS_PATH];
    int count = 0;
    bool ok = true;
    if (ctx.directory[0] == '~') {
        const char *home = getenv("HOME");
#ifdef _WIN32
        if (!home)
            home = getenv("USERPROFILE");
#endif
        int n = home ? snprintf(home_path, sizeof(home_path), "%s/%s", home, ctx.directory + 2) : -1;
        ok = ctx.directory[1] == '/' && n > 0 && n < FS_PATH && add(paths, &count, NULL, home_path);
    } else if (ctx.directory[0] == '/'
#ifdef _WIN32
               || ctx.directory[0] == '\\' ||
               (isalpha((unsigned char)ctx.directory[0]) && ctx.directory[1] == ':')
#endif
    )
        ok = add(paths, &count, NULL, ctx.directory);
    else {
        for (int i = 0; ok && i < a->xml_paths.count; i++)
            ok = add(paths, &count, a->xml_paths.paths[i], ctx.directory);
        if (ok)
            ok = add(paths, &count, a->project, ctx.directory);
        if (ok && *b->path && fs_parent(b->path, parent))
            ok = add(paths, &count, parent, ctx.directory);
        for (int i = 0; ok && i < a->linter.imports.count; i++)
            ok = add(paths, &count, a->linter.imports.paths[i], ctx.directory);
    }
    if (!ok || !count) {
        app_toast(a, "Chemin de dossier invalide ou trop long (4095 octets maximum).", true);
        return true;
    }
    x->context = ctx;
    x->xml_paths = a->xml_paths;
    x->imports = a->linter.imports;
    x->document = b;
    x->active = true;
    strcpy(x->project, a->project);
    strcpy(x->document_path, b->path);
    strcpy(x->name, b->name);
    x->lease.owner = &a->ssh;
    x->lease.initialized = a->ssh.initialized;
    x->lease.generation = atomic_load(&a->ssh.generation);
    x->ssh_state = atomic_load(&a->ssh.state);
    strcpy(x->lease.cache, a->ssh.cache);
    strcpy(x->lease.mountpoint, a->ssh.mountpoint);
    a->completion_generation++;
    x->generation = a->completion_generation;
    a->completion_document = b;
    a->completion_version = b->version;
    a->completion_cursor = b->cursor;
    a->completion_pane = a->active_pane;
    a->completion_start = ctx.component;
    a->completion_end = ctx.component_end;
    a->completion_active = a->completion_pending = a->completion_manual = true;
    char error[512];
    x->job = path_job_start((const char (*)[FS_PATH])paths, count, &ctx, lease_guard, &x->lease, error,
                            sizeof(error));
    if (!x->job) {
        app_completion_dismiss(a);
        app_toast(a, error, true);
    }
    return true;
}
static void utf8_label(char *out, size_t cap, const char *text, bool slash) {
    size_t n = strlen(text), limit = cap - 1 - (slash ? 1u : 0u);
    bool cut = n > limit;
    if (cut)
        limit -= 3;
    if (n > limit) {
        n = limit;
        while (n && ((unsigned char)text[n] & 0xc0) == 0x80)
            n--;
    }
    memcpy(out, text, n);
    if (cut) {
        memcpy(out + n, "...", 3);
        n += 3;
    }
    if (slash)
        out[n++] = '/';
    out[n] = 0;
}
void app_path_completion_poll(App *a) {
    PathCompletionState *x = &a->path_completion;
    if (x->active && !valid(a))
        app_completion_dismiss(a);
    if (!x->job || !atomic_load(&x->job->done))
        return;
    PathLoadJob *job = x->job;
    if (x->active && !atomic_load(&job->cancel)) {
        if (job->success) {
            x->result = malloc(sizeof(*x->result));
            if (x->result)
                *x->result = job->report;
            else {
                app_completion_dismiss(a);
                app_toast(a, "Mémoire insuffisante.", true);
            }
        } else {
            app_completion_dismiss(a);
            app_toast(a, *job->error ? job->error : "Lecture du dossier annulée.", true);
        }
    }
    path_job_free(job);
    x->job = NULL;
    if (!x->active || !x->result)
        return;
    PathReport *report = x->result;
    a->completion_pending = false;
    a->completion_count = report->count;
    a->completion_index = 0;
    for (int i = 0; i < report->count; i++) {
        PathChoice *choice = &report->items[i];
        CompletionItem *item = &a->completions[i];
        memset(item, 0, sizeof(*item));
        utf8_label(item->name, sizeof(item->name), choice->name, choice->directory);
        item->kind = choice->directory ? 'D' : 'P';
        item->snippet = -1;
        item->start = x->context.component;
        item->end = x->context.component_end;
        char parent[220];
        utf8_label(parent, sizeof(parent), choice->parent, false);
        snprintf(item->detail, sizeof(item->detail), "%s · %s", choice->directory ? "Dossier" : "Fichier",
                 parent);
    }
    if (report->limited)
        snprintf(a->completion_status, sizeof(a->completion_status),
                 "Liste limitée : précisez le préfixe, puis Ctrl+Espace.");
    else if (report->skipped)
        snprintf(a->completion_status, sizeof(a->completion_status),
                 "Des entrées inaccessibles ou non insérables ont été ignorées.");
    if (*a->completion_status)
        app_toast(a, a->completion_status, false);
    if (!report->count) {
        app_toast(
            a, *a->completion_status ? a->completion_status : "Aucun élément correspondant dans ce dossier.",
            false);
        app_completion_dismiss(a);
    }
}
bool app_path_completion_apply(App *a, int index) {
    PathCompletionState *x = &a->path_completion;
    if (!valid(a) || !x->result || index < 0 || index >= x->result->count) {
        app_completion_dismiss(a);
        return false;
    }
    PathChoice *choice = &x->result->items[index];
    bool directory = choice->directory;
    Buffer *b = x->document;
    XmlEdit plan = {0};
    char error[512] = "";
    bool ok = path_choice_edit(b->text, b->len, &x->context, choice, &plan, error, sizeof(error)) &&
              xml_edit_apply(b, &plan);
    free(plan.text);
    if (!ok) {
        app_toast(a, *error ? error : "Insertion du chemin impossible : mémoire ou taille maximale.", true);
        return false;
    }
    app_completion_dismiss(a);
    app_ensure_visible(a);
    if (directory)
        app_path_completion_begin(a);
    return true;
}
