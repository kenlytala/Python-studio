#include "app_internal.h"
#include "files.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void app_snippets_load(App *a) {
    char path[BUF_PATH];
    a->snippet_error[0] = 0;
    if (!snippets_file(path, sizeof(path), a->config)) {
        snprintf(a->snippet_error, sizeof(a->snippet_error), "Chemin de bibliothèque d’extraits invalide.");
        return;
    }
    if (!snippets_load(&a->snippets, path, a->snippet_error, sizeof(a->snippet_error)) &&
        !*a->snippet_error && !snippets_default(&a->snippets))
        snprintf(a->snippet_error, sizeof(a->snippet_error), "Mémoire insuffisante pour les extraits.");
}
bool app_snippet_dirty(const App *a) {
    if (a->snippet_index >= 0 && a->snippet_index < a->snippets.count) {
        const Snippet *s = &a->snippets.items[a->snippet_index];
        return !a->snippet_trigger.text || !a->snippet_title.text || !a->snippet_body.text ||
               strcmp(s->trigger, a->snippet_trigger.text) || strcmp(s->title, a->snippet_title.text) ||
               strcmp(s->body, a->snippet_body.text);
    }
    return a->snippet_trigger.len || a->snippet_title.len || a->snippet_body.len;
}
void app_snippet_edit(App *a, int index) {
    if (index < -1 || index >= a->snippets.count)
        return;
    if (index == -1 && a->snippets.count == SNIPPETS_MAX) {
        app_toast(a, "La bibliothèque contient déjà 64 extraits.", true);
        return;
    }
    const Snippet *s = index >= 0 ? &a->snippets.items[index] : NULL;
    app_set_field(&a->snippet_trigger, s ? s->trigger : "", false);
    app_set_field(&a->snippet_title, s ? s->title : "", false);
    app_set_field(&a->snippet_body, s ? s->body : "", false);
    buf_move(&a->snippet_body, 0, false);
    a->snippet_index = index;
    a->snippet_focus = 0;
    a->snippet_quit = a->snippet_dragging = false;
    a->snippet_error[0] = 0;
    a->modal = M_SNIPPET_EDIT;
    a->focus = 0;
    app_completion_dismiss(a);
}
void app_snippet_leave(App *a) {
    a->snippet_dragging = false;
    a->snippet_quit = false;
    a->modal = app_snippet_dirty(a) ? M_SNIPPET_DIRTY : M_SNIPPETS;
}
static bool store(App *a, Snippets *next) {
    char path[BUF_PATH], parent[BUF_PATH];
    if (!snippets_file(path, sizeof(path), a->config) || !fs_parent(path, parent)) {
        snprintf(a->snippet_error, sizeof(a->snippet_error), "Chemin de bibliothèque trop long.");
        return false;
    }
    MakeDirectory(parent);
    if (!snippets_save(next, path, a->snippet_error, sizeof(a->snippet_error))) {
        app_toast(a, a->snippet_error, true);
        return false;
    }
    snippets_free(&a->snippets);
    a->snippets = *next;
    memset(next, 0, sizeof(*next));
    a->snippet_error[0] = 0;
    a->modal = M_SNIPPETS;
    a->snippet_dragging = false;
    app_completion_dismiss(a);
    app_toast(a, "Bibliothèque commune enregistrée · disponible dans tous les projets.", false);
    return true;
}
bool app_snippet_save(App *a) {
    Snippets next = {0};
    if (!snippets_clone(&next, &a->snippets)) {
        snprintf(a->snippet_error, sizeof(a->snippet_error), "Mémoire insuffisante pour la bibliothèque.");
        return false;
    }
    bool ok = snippets_put(&next, a->snippet_index, a->snippet_trigger.text, a->snippet_title.text,
                           a->snippet_body.text, a->snippet_error, sizeof(a->snippet_error));
    if (ok)
        ok = store(a, &next);
    snippets_free(&next);
    return ok;
}
bool app_snippet_delete(App *a) {
    Snippets next = {0};
    if (!snippets_clone(&next, &a->snippets)) {
        snprintf(a->snippet_error, sizeof(a->snippet_error), "Mémoire insuffisante pour la bibliothèque.");
        return false;
    }
    bool ok = snippets_remove(&next, a->snippet_index) && store(a, &next);
    snippets_free(&next);
    return ok;
}
