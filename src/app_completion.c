#include "app_internal.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void app_completion_dismiss(App *a) {
    app_path_completion_cancel(a);
    if (a->completion_active || a->completion_pending || a->completion_count)
        a->completion_generation++;
    a->completion_status[0] = 0;
    a->completion_count = a->completion_index = 0;
    a->completion_active = a->completion_pending = false;
}
static void copy_name(char out[96], const char *text) {
    size_t n = 0;
    while (n < 95 && text[n])
        n++;
    memmove(out, text, n);
    out[n] = 0;
}
static bool python_document(const Buffer *b) {
    const char *ext = strrchr(b->name, '.');
    return ext && (!strcmp(ext, ".py") || !strcmp(ext, ".pyw") || !strcmp(ext, ".pyi"));
}
static bool context_valid(const App *a) {
    return a->completion_active && a->completion_document && !a->modal && !a->focus && a->active >= 0 &&
           a->active < a->tab_count && a->tabs[a->active] == a->completion_document &&
           a->active_pane == a->completion_pane && a->completion_document->version == a->completion_version &&
           a->completion_document->cursor == a->completion_cursor && !a->completion_document->extra_count &&
           (a->completion_document->anchor == BUF_NO_SELECTION ||
            a->completion_document->anchor == a->completion_document->cursor);
}
static bool duplicate(App *a, const char *name) {
    for (int i = 0; i < a->completion_count; i++)
        if (!strcmp(a->completions[i].name, name))
            return true;
    return false;
}
static void build_items(App *a, const CompletionReport *framework) {
    Buffer *b = app_buffer(a);
    char selected[96] = "";
    char selected_kind = 0;
    if (a->completion_index >= 0 && a->completion_index < a->completion_count) {
        snprintf(selected, sizeof(selected), "%s", a->completions[a->completion_index].name);
        selected_kind = a->completions[a->completion_index].kind;
    }
    a->completion_count = 0;
    size_t n = b->cursor - a->completion_start;
    char prefix[96] = "";
    if (n >= sizeof(prefix))
        return;
    memcpy(prefix, b->text + a->completion_start, n);
    prefix[n] = 0;
    bool qualified = a->completion_start && b->text[a->completion_start - 1] == '.';
    if (!qualified && (n || a->completion_manual)) {
        int indices[COMPLETION_MAX];
        int count = snippets_match(&a->snippets, prefix, indices, COMPLETION_MAX);
        for (int i = 0; i < count; i++) {
            const Snippet *s = &a->snippets.items[indices[i]];
            CompletionItem *item = &a->completions[a->completion_count++];
            memset(item, 0, sizeof(*item));
            char title[SNIPPET_TITLE];
            memcpy(title, s->title, sizeof(title));
            copy_name(item->name, s->trigger);
            char detail[256];
            snprintf(detail, sizeof(detail), "Macro · %s", *title ? title : item->name);
            memcpy(item->detail, detail, strlen(detail) + 1);
            item->kind = 'S';
            item->snippet = indices[i];
            item->start = a->completion_start;
            item->end = n ? a->completion_end : b->cursor;
        }
    }
    if (framework)
        for (int i = 0; i < framework->count && a->completion_count < COMPLETION_MAX; i++)
            if (!duplicate(a, framework->items[i].name))
                a->completions[a->completion_count++] = framework->items[i];
    if (!qualified && n >= 2 && !xml_document_name(b->name)) {
        char words[COMPLETION_MAX][96];
        int count = syntax_complete(b->text, prefix, words, COMPLETION_MAX);
        for (int i = 0; i < count && a->completion_count < COMPLETION_MAX; i++)
            if (!duplicate(a, words[i])) {
                CompletionItem *item = &a->completions[a->completion_count++];
                memset(item, 0, sizeof(*item));
                copy_name(item->name, words[i]);
                snprintf(item->detail, sizeof(item->detail), "%s", "Mot du document / Python");
                item->kind = 'W';
                item->snippet = -1;
                item->start = a->completion_start;
                item->end = a->completion_end;
            }
    }
    a->completion_index = 0;
    for (int i = 0; i < a->completion_count; i++)
        if (a->completions[i].kind == selected_kind && !strcmp(a->completions[i].name, selected)) {
            a->completion_index = i;
            break;
        }
}
void app_completion_show(App *a, bool manual) {
    Buffer *b = app_buffer(a);
    if (a->modal || a->focus || b->extra_count || (b->anchor != BUF_NO_SELECTION && b->anchor != b->cursor)) {
        app_completion_dismiss(a);
        if (manual && b->extra_count)
            app_toast(a, "Complétion : revenez à un seul curseur avec Échap.", false);
        return;
    }
    if (manual && app_path_completion_begin(a))
        return;
    if (a->path_completion.active)
        app_completion_dismiss(a);
    int xml_count = xml_document_name(b->name)
                        ? xml_tag_completions(b->text, b->len, b->cursor, a->completions, COMPLETION_MAX)
                        : -1;
    if (xml_count >= 0 && !manual && !a->xml_auto) {
        app_completion_dismiss(a);
        return;
    }
    size_t start, end;
    if (xml_count >= 0) {
        start = xml_count ? a->completions[0].start : b->cursor;
        end = xml_count ? a->completions[0].end : b->cursor;
    } else
        completion_span(b, &start, &end);
    size_t n = b->cursor - start;
    bool qualified = start && b->text[start - 1] == '.';
    bool import_space = b->cursor >= 7 && !memcmp(b->text + b->cursor - 7, "import ", 7);
    if (xml_count < 0 && (n >= 96 || (!n && !qualified && !manual && !import_space))) {
        app_completion_dismiss(a);
        return;
    }
    a->completion_generation++;
    a->completion_status[0] = 0;
    a->completion_document = b;
    a->completion_version = b->version;
    a->completion_cursor = b->cursor;
    a->completion_pane = a->active_pane;
    a->completion_start = start;
    a->completion_end = end;
    a->completion_active = true;
    a->completion_manual = manual;
    a->completion_pending =
        xml_count < 0 && python_document(b) && (n >= 2 || qualified || manual || import_space);
    a->completion_changed = runner_clock() - (manual ? COMPLETION_DEBOUNCE : 0);
    a->completion_index = a->completion_count = 0;
    if (xml_count >= 0) {
        a->completion_count = xml_count;
        return;
    }
    build_items(a, NULL);
}
void app_completion_poll(App *a) {
    app_path_completion_poll(a);
    if (a->path_completion.active)
        return;
    double now = runner_clock();
    completion_poll(&a->completer, now);
    if (!context_valid(a)) {
        app_completion_dismiss(a);
        a->completer.ready = false;
        return;
    }
    Buffer *b = app_buffer(a);
    if (a->completer.ready) {
        bool same = a->completion_request_generation == a->completion_generation &&
                    a->completer.version == b->version && a->completer.cursor == b->cursor &&
                    ((!*b->path && !strcmp(fs_basename(a->completer.filename), b->name)) ||
                     (*b->path && !strcmp(b->path, a->completer.filename)));
        a->completer.ready = false;
        if (same) {
            snprintf(a->completion_status, sizeof(a->completion_status), "%s", a->completer.report.status);
            build_items(a, &a->completer.report);
            a->completion_pending = false;
            if (a->completer.report.failed)
                app_toast(a, a->completer.report.status, true);
            else if (!a->completion_count && (a->completion_manual || *a->completer.report.status))
                app_toast(a,
                          *a->completer.report.status
                              ? a->completer.report.status
                              : "Aucune proposition statique ; vérifiez les dossiers d’import du framework.",
                          false);
        }
    }
    char access_error[512];
    if (!ssh_guard(&a->ssh, *b->path ? b->path : a->project, false, access_error, sizeof(access_error))) {
        app_completion_dismiss(a);
        return;
    }
    if (a->completion_pending && !a->completer.awaiting &&
        now - a->completion_changed >= COMPLETION_DEBOUNCE) {
        char filename[BUF_PATH], error[512];
        int n = *b->path ? snprintf(filename, sizeof(filename), "%s", b->path)
                         : snprintf(filename, sizeof(filename), "%s/%s", a->project, b->name);
        if (n < 0 || (size_t)n >= sizeof(filename)) {
            a->completion_pending = false;
            return;
        }
        if (completion_submit(&a->completer, a->python, a->project, &a->linter.imports, filename, b->text,
                              b->version, b->cursor, error, sizeof(error)))
            a->completion_request_generation = a->completion_generation;
        else if (*error) {
            a->completion_pending = false;
            app_toast(a, error, true);
        }
    }
}
bool app_completion_apply(App *a, int index) {
    if (!context_valid(a) || index < 0 || index >= a->completion_count) {
        app_completion_dismiss(a);
        return false;
    }
    Buffer *b = app_buffer(a);
    CompletionItem *item = &a->completions[index];
    if (item->start > item->end || item->end > b->len ||
        (item->start < b->len && ((unsigned char)b->text[item->start] & 0xc0) == 0x80) ||
        (item->end < b->len && ((unsigned char)b->text[item->end] & 0xc0) == 0x80)) {
        app_completion_dismiss(a);
        return false;
    }
    if (item->kind == 'D' || item->kind == 'P')
        return app_path_completion_apply(a, index);
    bool ok;
    char error[512] = "";
    if (item->kind == 'X' || item->kind == 'T') {
        XmlEdit edit = {0};
        int status = xml_edit_completion(b->text, b->len, b->cursor, item->name, &edit);
        ok = status > 0 && xml_edit_apply(b, &edit);
        free(edit.text);
    } else if (item->kind == 'S') {
        ok = item->snippet >= 0 && item->snippet < a->snippets.count &&
             !strcmp(item->name, a->snippets.items[item->snippet].trigger) &&
             snippet_expand(b, &a->snippets.items[item->snippet], item->start, item->end, error,
                            sizeof(error));
    } else
        ok = buf_replace(b, item->start, item->end, item->name);
    app_completion_dismiss(a);
    if (ok)
        app_ensure_visible(a);
    else
        app_toast(a, *error ? error : "Insertion de la proposition impossible.", true);
    return ok;
}

bool app_completion_key(App *a, int key, bool shift) {
    if (!a->completion_count && !a->completion_pending)
        return false;
    if (key == KEY_ESCAPE) {
        app_completion_dismiss(a);
        return true;
    }
    if (!a->completion_count)
        return false;
    if (key == KEY_DOWN || key == KEY_UP) {
        a->completion_index =
            (a->completion_index + a->completion_count + (key == KEY_DOWN ? 1 : -1)) % a->completion_count;
        return true;
    }
    if (key == KEY_ENTER || key == KEY_KP_ENTER || (key == KEY_TAB && !shift)) {
        app_completion_apply(a, a->completion_index);
        return true;
    }
    return false;
}
