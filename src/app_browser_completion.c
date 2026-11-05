#include "app_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
void app_browser_completion_cancel(App *a) {
    browser_job_cancel(a->browser_completion.job);
    a->browser_completion.active = false;
}
void app_browser_completion_shutdown(App *a) {
    app_browser_completion_cancel(a);
    browser_job_free(a->browser_completion.job);
    a->browser_completion.job = NULL;
}
void app_browser_completion_begin(App *a) {
    BrowserCompletionState *s = &a->browser_completion;
    if (a->modal != M_BROWSER)
        return;
    if (s->job && !atomic_load(&s->job->done)) {
        browser_job_cancel(s->job);
        snprintf(s->status, sizeof(s->status), "La lecture précédente termine son annulation. Réessayez.");
        return;
    }
    browser_job_free(s->job);
    s->job = NULL;
    s->suggesting = false;
    s->version = a->field.version;
    s->cursor = a->field.cursor;
    s->anchor = a->field.anchor;
    s->mode = a->browser_mode;
    snprintf(s->base, sizeof(s->base), "%s", a->folder);
    snprintf(s->project, sizeof(s->project), "%s", a->project);
    s->lease.owner = &a->ssh;
    s->lease.initialized = a->ssh.initialized;
    s->lease.generation = atomic_load(&a->ssh.generation);
    s->ssh_state = atomic_load(&a->ssh.state);
    strcpy(s->lease.cache, a->ssh.cache);
    strcpy(s->lease.mountpoint, a->ssh.mountpoint);
    const char *home = getenv("HOME");
#ifdef _WIN32
    if (!home)
        home = getenv("USERPROFILE");
#endif
    s->job = browser_job_start(a->field.text, a->field.cursor, a->folder, home ? home : "",
                               a->browser_mode == 1 || a->browser_mode == 3 || a->browser_mode == 4 ||
                                   a->browser_mode == 5,
                               lease_guard, &s->lease, s->status, sizeof(s->status));
    s->active = s->job != NULL;
    if (s->job)
        snprintf(s->status, sizeof(s->status), "Lecture du chemin…");
    else
        app_toast(a, s->status, true);
}
void app_browser_completion_poll(App *a) {
    BrowserCompletionState *s = &a->browser_completion;
    BrowserLoadJob *job = s->job;
    if (!job)
        return;
    bool valid = s->active && a->modal == M_BROWSER && a->field.version == s->version &&
                 a->field.cursor == s->cursor && a->field.anchor == s->anchor && a->browser_mode == s->mode &&
                 !strcmp(a->folder, s->base) && !strcmp(a->project, s->project) &&
                 atomic_load(&a->ssh.generation) == s->lease.generation &&
                 atomic_load(&a->ssh.state) == s->ssh_state;
    if (!valid) {
        s->active = false;
        browser_job_cancel(job);
    }
    if (!atomic_load(&job->done))
        return;
    if (valid && !atomic_load(&job->cancel)) {
        if (!job->success) {
            snprintf(s->status, sizeof(s->status), "%s", job->error);
            app_toast(a, s->status, true);
        } else {
            a->browser_count = (int)job->count;
            a->browser_index = a->browser_scroll = 0;
            snprintf(s->suggested_folder, sizeof(s->suggested_folder), "%s", job->directory);
            for (unsigned i = 0; i < job->count; i++) {
                Entry *entry = &a->browser[i];
                memset(entry, 0, sizeof(*entry));
                snprintf(entry->path, sizeof(entry->path), "%s", job->items[i].path);
                snprintf(entry->name, sizeof(entry->name), "%s", fs_basename(job->items[i].path));
                entry->directory = job->items[i].directory;
            }
            s->suggesting = true;
            snprintf(s->status, sizeof(s->status), "%s",
                     job->limited  ? "Suggestions limitées : précisez le chemin puis Ctrl+Espace."
                     : !job->count ? "Aucun élément correspondant."
                                   : "Tab/Entrée : compléter · Entrée ensuite : ouvrir/choisir");
        }
    }
    browser_job_free(job);
    s->job = NULL;
    s->active = false;
}
bool app_browser_completion_accept(App *a, int index) {
    BrowserCompletionState *s = &a->browser_completion;
    if (a->modal != M_BROWSER || !s->suggesting)
        return false;
    if (index < 0 || index >= a->browser_count)
        return true;
    Entry *entry = &a->browser[index];
    char value[BUF_PATH];
    int n = snprintf(value, sizeof(value), "%s%s", entry->path, entry->directory ? "/" : "");
    if (n < 0 || n >= BUF_PATH) {
        app_toast(a, "Chemin complété trop long.", true);
        return true;
    }
    if (!app_path_usable(a, entry->path))
        return true;
    if (!strcmp(a->field.text, value) || buf_replace(&a->field, 0, a->field.len, value)) {
        s->suggesting = false;
        snprintf(s->status, sizeof(s->status),
                 "Chemin complété · Ctrl+Espace : contenu · Entrée : ouvrir/choisir");
    }
    return true;
}
