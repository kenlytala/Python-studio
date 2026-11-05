#include "app_internal.h"
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Only immutable ownership paths and a pinned generation are copied, never the
   SshSession's profile, token, prompt, log or authentication state. The owner
   outlives the job; the worker reads its three atomic lease indicators only. */
static bool lease_guard(void *context, const char *path, bool leaf, char *error, size_t cap) {
    XmlReadLease *lease = context;
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
static bool contains_ci(const char *text, const char *query) {
    if (!*query)
        return true;
    for (size_t i = 0; text[i]; i++) {
        size_t j = 0;
        while (query[j] && text[i + j] &&
               tolower((unsigned char)query[j]) == tolower((unsigned char)text[i + j]))
            j++;
        if (!query[j])
            return true;
    }
    return false;
}
void app_xml_cancel(App *a) {
    XmlInlineState *x = &a->xml;
    xml_job_cancel(x->job);
    xml_refs_free(&x->refs);
    free(x->original);
    x->original = NULL;
    x->document = NULL;
    x->visible = false;
    x->count = x->index = 0;
    if (!x->job) {
        free(x->lease);
        x->lease = NULL;
    }
    if (a->modal == M_XML_IDS)
        a->modal = M_NONE;
    a->focus = 0;
    a->dragging = false;
}
void app_xml_shutdown(App *a) {
    app_xml_cancel(a);
    xml_job_free(a->xml.job);
    a->xml.job = NULL;
    free(a->xml.lease);
    a->xml.lease = NULL;
}
void app_xml_poll(App *a) {
    XmlInlineState *x = &a->xml;
    if (!x->job || !atomic_load(&x->job->done))
        return;
    if (x->visible && !atomic_load(&x->job->cancel)) {
        if (x->job->success) {
            x->refs = x->job->references;
            memset(&x->job->references, 0, sizeof(x->job->references));
        }
        snprintf(x->error, sizeof(x->error), "%s", x->job->error);
        x->filter_version = ULONG_MAX;
    }
    xml_job_free(x->job);
    x->job = NULL;
    free(x->lease);
    x->lease = NULL;
    if (x->visible)
        app_xml_filter(a);
}
static bool add_path(char paths[][BUF_PATH], int *count, const char *root, const char *value) {
    char path[BUF_PATH];
    int size =
        root ? snprintf(path, sizeof(path), "%s/%s", root, value) : snprintf(path, sizeof(path), "%s", value);
    if (size < 1 || size >= BUF_PATH)
        return false;
    for (int i = 0; i < *count; i++)
        if (!strcmp(paths[i], path))
            return true;
    if (*count == XML_REF_ROOTS_MAX)
        return false;
    strcpy(paths[(*count)++], path);
    return true;
}
bool app_xml_begin(App *a, bool required) {
    Buffer *b = app_buffer(a);
    XmlPathSpan span;
    if (!b || b->extra_count || !xml_path_span(b->text, b->len, b->cursor, b->anchor, &span)) {
        if (required)
            app_toast(a,
                      "Placez le curseur sur un chemin .xml (ou sélectionnez-le), avec un seul curseur, puis "
                      "Ctrl+Espace.",
                      true);
        return required;
    }
    app_xml_poll(a);
    XmlInlineState *x = &a->xml;
    if (x->job) {
        app_toast(a, "La lecture XML précédente termine son annulation. Réessayez dans un instant.", false);
        return true;
    }
    app_xml_cancel(a);
    x->error[0] = 0;
    x->filter_version = ULONG_MAX;
    x->start = span.start;
    x->end = span.end;
    x->cursor = b->cursor;
    x->anchor = b->anchor;
    x->version = b->version;
    x->pane = a->active_pane;
    x->original = malloc(span.end - span.start + 1);
    x->lease = calloc(1, sizeof(*x->lease));
    if (!x->original || !x->lease) {
        app_xml_cancel(a);
        app_toast(a, "Mémoire insuffisante.", true);
        return true;
    }
    memcpy(x->original, b->text + span.start, span.end - span.start);
    x->original[span.end - span.start] = 0;
    strcpy(x->project, a->project);
    x->document = b;
    x->visible = true;
    XmlReadLease *lease = x->lease;
    lease->owner = &a->ssh;
    lease->initialized = a->ssh.initialized;
    lease->generation = atomic_load(&a->ssh.generation);
    strcpy(lease->cache, a->ssh.cache);
    strcpy(lease->mountpoint, a->ssh.mountpoint);
    char paths[XML_REF_ROOTS_MAX][BUF_PATH], parent[BUF_PATH], value[BUF_PATH];
    int count = 0;
    bool ok = true;
    if (span.path[0] == '~') {
        const char *home = getenv("HOME");
#ifdef _WIN32
        if (!home)
            home = getenv("USERPROFILE");
#endif
        if (!home || (span.path[1] != '/' && span.path[1] != '\\'))
            ok = false;
        else {
            int size = snprintf(value, sizeof(value), "%s/%s", home, span.path + 2);
            ok = size > 0 && size < BUF_PATH && add_path(paths, &count, NULL, value);
        }
    } else if (span.path[0] == '/'
#ifdef _WIN32
               || span.path[0] == '\\' || (isalpha((unsigned char)span.path[0]) && span.path[1] == ':')
#endif
    )
        ok = add_path(paths, &count, NULL, span.path);
    else {
        for (int i = 0; ok && i < a->xml_paths.count; i++)
            ok = add_path(paths, &count, a->xml_paths.paths[i], span.path);
        if (ok)
            ok = add_path(paths, &count, a->project, span.path);
        if (ok && *b->path && fs_parent(b->path, parent))
            ok = add_path(paths, &count, parent, span.path);
        for (int i = 0; ok && i < a->linter.imports.count; i++)
            ok = add_path(paths, &count, a->linter.imports.paths[i], span.path);
    }
    if (!ok)
        snprintf(x->error, sizeof(x->error),
                 "Chemin XML invalide ou trop long ; utilisez un chemin absolu, relatif ou ~/…");
    else
        x->job = xml_job_start((const char (*)[BUF_PATH])paths, count, lease_guard, lease, x->error,
                               sizeof(x->error));
    if (!x->job) {
        free(x->lease);
        x->lease = NULL;
    }
    app_set_field(&a->field, "", false);
    app_completion_dismiss(a);
    app_multicursor_reset(a);
    a->modal = M_XML_IDS;
    a->focus = 0;
    return true;
}
int app_xml_filter(App *a) {
    XmlInlineState *x = &a->xml;
    if (x->filter_version == a->field.version)
        return x->count;
    x->filter_version = a->field.version;
    x->count = 0;
    for (int i = 0; i < x->refs.count; i++) {
        XmlIdentifier *item = &x->refs.items[i];
        if (contains_ci(item->id, a->field.text) || contains_ci(item->tag, a->field.text) ||
            contains_ci(item->model, a->field.text))
            x->filtered[x->count++] = i;
    }
    if (x->index >= x->count)
        x->index = x->count ? x->count - 1 : 0;
    return x->count;
}
void app_xml_choose(App *a, int index) {
    XmlInlineState *x = &a->xml;
    app_xml_filter(a);
    if (!x->visible || a->modal != M_XML_IDS || index < 0 || index >= x->count)
        return;
    bool member = false;
    for (int i = 0; i < a->tab_count; i++)
        if (a->tabs[i] == x->document)
            member = true;
    Buffer *b = x->document;
    if (!member || b != app_buffer(a) || b->version != x->version || b->extra_count ||
        b->cursor != x->cursor || b->anchor != x->anchor || a->active_pane != x->pane ||
        strcmp(x->project, a->project) || x->end > b->len ||
        memcmp(b->text + x->start, x->original, x->end - x->start)) {
        app_xml_cancel(a);
        app_toast(a, "Le document a changé : aucune insertion. Relancez Ctrl+Espace sur le chemin XML.",
                  true);
        return;
    }
    char *snippet = xml_refs_snippet(&x->refs, x->filtered[index], x->error, sizeof(x->error));
    if (!snippet) {
        app_toast(a, x->error, true);
        return;
    }
    size_t line = x->start;
    while (line && b->text[line - 1] != '\n')
        line--;
    size_t indentation = 0;
    while (line + indentation < x->start &&
           (b->text[line + indentation] == ' ' || b->text[line + indentation] == '\t'))
        indentation++;
    size_t n = strlen(snippet), lines = 0;
    for (size_t i = 0; i < n; i++)
        if (snippet[i] == '\n')
            lines++;
    if (lines && indentation > (XML_REF_OUTPUT_MAX - n) / lines) {
        free(snippet);
        app_toast(a, "Bloc XML indenté trop volumineux (256 Kio maximum).", true);
        return;
    }
    size_t size = n + lines * indentation;
    char *indented = malloc(size + 1);
    if (!indented) {
        free(snippet);
        app_toast(a, "Mémoire insuffisante.", true);
        return;
    }
    size_t used = 0;
    for (size_t i = 0; i < n; i++) {
        indented[used++] = snippet[i];
        if (snippet[i] == '\n') {
            memcpy(indented + used, b->text + line, indentation);
            used += indentation;
        }
    }
    indented[used] = 0;
    free(snippet);
    bool ok = buf_replace(b, x->start, x->end, indented);
    free(indented);
    if (!ok) {
        app_toast(
            a,
            "Insertion impossible : limite de 2 Mio par document ou mémoire insuffisante. Chemin conservé.",
            true);
        return;
    }
    app_xml_cancel(a);
    app_ensure_visible(a);
    app_toast(a, "Bloc XML inséré · champ arch vide · Ctrl+Z pour annuler.", false);
}
static bool pressed(App *a, int key) {
    return app_key_pressed(a, key);
}
void app_xml_keys(App *a, const unsigned *chars, int count, bool ctrl, bool shift) {
    if (pressed(a, KEY_ESCAPE)) {
        app_xml_cancel(a);
        return;
    }
    unsigned long version = a->field.version;
    app_edit_field(a, &a->field, chars, count, ctrl, shift);
    if (version != a->field.version) {
        a->xml.index = 0;
        a->xml.filter_version = ULONG_MAX;
    }
    app_xml_filter(a);
    if (pressed(a, KEY_DOWN) && a->xml.index < a->xml.count - 1)
        a->xml.index++;
    if (pressed(a, KEY_UP) && a->xml.index > 0)
        a->xml.index--;
    if (pressed(a, KEY_ENTER) || pressed(a, KEY_KP_ENTER))
        app_xml_choose(a, a->xml.index);
}

void app_xml_paths_load(App *a) {
    memset(&a->xml_paths, 0, sizeof(a->xml_paths));
    a->xml_paths_error[0] = 0;
    char file[BUF_PATH];
    if (!xml_paths_file(file, sizeof(file), a->config, a->project))
        snprintf(a->xml_paths_error, sizeof(a->xml_paths_error), "Chemin de préférences XML invalide.");
    else
        xml_paths_load(&a->xml_paths, file, a->project, a->xml_paths_error, sizeof(a->xml_paths_error));
    if (*a->xml_paths_error) {
        if (a->running)
            app_toast(a, a->xml_paths_error, true);
        else
            fprintf(stderr, "%s\n", a->xml_paths_error);
    }
}
