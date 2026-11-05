#define _GNU_SOURCE
/* Production C SSH controller + transport. Raylib/window effects are replaced;
   authentication, SSHFS, SFTP and all filesystem operations remain real. */
#include "app_internal.h"
#include "ssh_json.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

bool ui_inside(Rectangle r, Vector2 p) {
    return p.x >= r.x && p.x < r.x + r.width && p.y >= r.y && p.y < r.y + r.height;
}
static bool fake_fullscreen;
void ToggleFullscreen(void) {
    fake_fullscreen = !fake_fullscreen;
}
bool IsWindowFullscreen(void) {
    return fake_fullscreen;
}
void app_layout(App *a) {
    (void)a;
}
bool IsKeyPressed(int key) {
    (void)key;
    return false;
}
bool IsKeyPressedRepeat(int key) {
    (void)key;
    return false;
}
Buffer *app_buffer(App *a) {
    return a->tabs[a->active];
}
void app_ensure_visible(App *a) {
    (void)a;
}
void app_completion_dismiss(App *a) {
    app_path_completion_cancel(a);
    a->completion_active = a->completion_pending = false;
    a->completion_count = a->completion_index = 0;
    a->completion_document = NULL;
    a->completion_generation++;
}
void app_toast(App *a, const char *s, bool error) {
    snprintf(a->toast, sizeof(a->toast), "%s", s);
    a->toast_error = error;
}
void app_set_field(Buffer *b, const char *s, bool select) {
    unsigned long version = b->version;
    buf_free(b);
    if (!buf_init(b, s, "field"))
        abort();
    b->version = version + 1;
    b->cursor = b->len;
    if (select)
        b->anchor = 0;
}
void app_edit_field(App *a, Buffer *b, const unsigned *s, int n, bool c, bool shift) {
    (void)a;
    (void)b;
    (void)s;
    (void)n;
    (void)c;
    (void)shift;
    abort();
}
void app_ssh_change_project(App *a, const char *path) {
    snprintf(a->project, sizeof(a->project), "%s", path);
}
void app_ssh_drop_remote_tabs(App *a) {
    snprintf(a->project, sizeof(a->project), "%s", a->last_local_project);
}
void app_ssh_reset_analysis(App *a) {
    (void)a;
}
void app_multicursor_reset(App *a) {
    (void)a;
}
void app_action(App *a, Action id, int arg) {
    (void)a;
    (void)id;
    (void)arg;
}
static double now(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec / 1e9;
}
static void event(const char *type, const char *value, bool confirm) {
    char raw[65537];
    SjOut out;
    sj_out(&out, raw, sizeof(raw));
    sj_add(&out, "{\"type\":");
    sj_quote(&out, type);
    sj_add(&out, ",\"value\":");
    sj_quote(&out, value);
    sj_add(&out, confirm ? ",\"confirm\":true}\n" : ",\"confirm\":false}\n");
    if (!out.ok || fwrite(raw, 1, out.len, stdout) != out.len)
        abort();
    fflush(stdout);
}
static bool join(char out[SSH_PATH], const char *parent, const char *name) {
    int n = snprintf(out, SSH_PATH, "%s/%s", parent, name);
    return n > 0 && n < SSH_PATH;
}
static bool exercise(App *a, Buffer *remote, const char *root, char error[1024]) {
    SshSession *s = &a->ssh;
    char path[SSH_PATH], created[SSH_PATH], folder[SSH_PATH];
    if (!join(path, s->mountpoint, "main.py") || !ssh_guard(s, path, false, error, 1024) ||
        !buf_load(remote, path, error, 1024))
        return false;
    if (!buf_insert(remote, "# saved by native C\n") ||
        !buf_save_guarded(remote, remote->path, ssh_save_guard, s, error, 1024))
        return false;
    if (!fs_create(s->mountpoint, s->mountpoint, "created-c", true, folder, error, 1024))
        return false;
    if (!fs_create(s->mountpoint, folder, "entry.txt", false, created, error, 1024))
        return false;
    if (!fs_remove(s->mountpoint, created, NULL, NULL, error, 1024) ||
        !fs_remove(s->mountpoint, folder, NULL, NULL, error, 1024))
        return false;
    char upload[SSH_PATH];
    if (!join(upload, root, "upload-c.txt"))
        return false;
    FILE *file = fopen(upload, "w");
    if (!file)
        return false;
    fputs("uploaded through native guarded worker\n", file);
    fclose(file);
    FsPaths *paths = calloc(1, sizeof(*paths));
    if (!paths)
        return false;
    paths->count = 1;
    strcpy(paths->paths[0], upload);
    FileJob *job =
        file_job_start_guarded(s->mountpoint, s->mountpoint, NULL, paths, ssh_guard, s, error, 1024);
    if (!job) {
        free(paths);
        return false;
    }
    double deadline = now() + 10;
    while (!atomic_load(&job->done) && now() < deadline) {
        struct timespec delay = {0, 5000000};
        nanosleep(&delay, NULL);
    }
    bool ok = atomic_load(&job->done) && job->success;
    if (!ok)
        snprintf(error, 1024, "copy: %s", job->error);
    file_job_free(job);
    if (ok) {
        strcpy(paths->paths[0], remote->path);
        ok = fs_paste(root, root, paths, NULL, NULL, created, error, 1024);
    }
    /* Remote symlinks are not traversed/copied as sources; unlink is safe. */
    char link[SSH_PATH];
    if (ok && join(link, s->mountpoint, "outside-link") && fs_kind(link) == FS_LINK) {
        if (ssh_guard(s, link, false, error, 1024))
            ok = false;
        else {
            strcpy(paths->paths[0], link);
            if (fs_paste(root, root, paths, NULL, NULL, created, error, 1024))
                ok = false;
            else
                ok = fs_remove(s->mountpoint, link, NULL, NULL, error, 1024);
        }
    }
    free(paths);
    return ok;
}
static bool path_roundtrip_wait(App *a) {
    if (!a->path_completion.job)
        return false;
    double deadline = now() + 8;
    while (!atomic_load(&a->path_completion.job->done) && now() < deadline) {
        struct timespec delay = {0, 1000000};
        nanosleep(&delay, NULL);
    }
    if (!atomic_load(&a->path_completion.job->done))
        return false;
    app_path_completion_poll(a);
    return a->path_completion.active && a->completion_count == 1;
}
static bool folder_roundtrip(App *a, Buffer *remote, const char *local, char error[1024]) {
    if (!join(a->config, local, "settings-search.cfg"))
        return false;
    char *original = malloc(remote->len + 1);
    if (!original)
        return false;
    memcpy(original, remote->text, remote->len + 1);
    unsigned long version = remote->version;
    size_t cursor = remote->cursor, anchor = remote->anchor;
    int undo = remote->undo.count;
    app_folder_search_show(a);
    FolderSearchState *s = &a->folder_search;
    app_set_field(&s->folder, a->ssh.mountpoint, false);
    app_set_field(&s->query, "needle", false);
    app_folder_search_start(a);
    double deadline = now() + 6;
    while (s->job && !atomic_load(&s->job->done) && now() < deadline) {
        struct timespec delay = {0, 5000000};
        nanosleep(&delay, NULL);
    }
    bool ok = s->job && atomic_load(&s->job->done);
    if (ok)
        app_folder_search_poll(a);
    ok = ok && !s->job && s->report && s->report->count == 2 && s->report->hit_count == 3 && !*s->error;
    const int positions[][2] = {{0, 1}, {1, 0}, {0, 0}};
    for (unsigned i = 0; ok && i < 3; i++) {
        a->keys[KEY_F3] = true;
        app_folder_search_keys(a, NULL, 0, false, false);
        a->keys[KEY_F3] = false;
        ok = s->file == positions[i][0] && s->hit == positions[i][1];
    }
    if (ok) {
        a->keys[KEY_F3] = true;
        app_folder_search_keys(a, NULL, 0, false, true);
        a->keys[KEY_F3] = false;
        ok = s->file == 1 && s->hit == 0 && !strcmp(s->view.text, "α\tneedle\n");
    }
    if (ok) {
        app_folder_search_option(a, 0);
        app_folder_search_option(a, 1);
        app_folder_search_option(a, 2);
        app_set_field(&s->query, "NEEDLE(?: \\+ NEEDLE)?", false);
        app_set_field(&s->filters[0], "*.txt", false);
        app_set_field(&s->filters[3], "sub-search", false);
        app_folder_search_start(a);
        deadline = now() + 6;
        while (s->job && !atomic_load(&s->job->done) && now() < deadline) {
            struct timespec delay = {0, 5000000};
            nanosleep(&delay, NULL);
        }
        ok = s->job && atomic_load(&s->job->done);
        if (ok)
            app_folder_search_poll(a);
        ok =
            ok && s->report && s->report->count == 1 && s->report->hit_count == 1 && s->report->options.regex;
        app_folder_search_fullscreen(a);
        ok = ok && !fake_fullscreen && s->maximized;
        app_folder_search_next(a, 1);
        ok = ok && s->file == 0 && s->hit == 0;
        app_folder_search_restore_window(a);
        ok = ok && !fake_fullscreen;
    }
    ok = ok && remote->version == version && remote->cursor == cursor && remote->anchor == anchor &&
         remote->undo.count == undo && !strcmp(remote->text, original);
    free(original);
    if (!ok)
        snprintf(error, 1024, "Folder search SSH: %.480s", s->error);
    else
        event("folder_search_completed",
              "2 mounted files, 3 literal hits then 1 insensitive/whole-word/regex + file/dir filters; "
              "global F3/Maj+F3, mock fullscreen restore, editor unchanged",
              false);
    app_folder_search_shutdown(a);
    a->modal = M_NONE;
    return ok;
}

static bool xml_roundtrip_once(App *a, Buffer *remote, char error[1024]) {
    Buffer document = {0};
    const char *path = "    sale/views/sale_views.xml";
    if (!buf_init(&document, "    sale/", "draft.xml"))
        return false;
    document.cursor = document.len;
    a->tabs[1] = &document;
    a->tab_count = 2;
    a->active = 1;
    a->active_pane = 0;
    a->modal = M_NONE;
    a->focus = 0;
    bool ok = app_path_completion_begin(a) && path_roundtrip_wait(a) && a->completions[0].kind == 'D' &&
              app_path_completion_apply(a, 0) && !strcmp(document.text, "    sale/views/") &&
              path_roundtrip_wait(a) && a->completions[0].kind == 'P' && app_path_completion_apply(a, 0) &&
              !strcmp(document.text, path) && document.undo.count == 2;
    if (ok)
        event("directory_completed", "sale/ -> views/ -> sale_views.xml; no remote execution", false);
    ok = ok && app_xml_begin(a, false);
    double deadline = now() + 8;
    while (ok && a->xml.job && !atomic_load(&a->xml.job->done) && now() < deadline) {
        struct timespec delay = {0, 1000000};
        nanosleep(&delay, NULL);
    }
    if (a->xml.job && !atomic_load(&a->xml.job->done))
        ok = false;
    if (ok)
        app_xml_poll(a);
    if (ok)
        ok = a->xml.refs.count >= 1 && !*a->xml.error;
    if (ok) {
        app_set_field(&a->field, "view_order_form", false);
        ok = app_xml_filter(a) == 1;
        if (ok) {
            app_xml_choose(a, 0);
            ok = a->modal == M_NONE && document.undo.count == 3 &&
                 strstr(document.text, "<field name=\"name\">sale.order.form</field>") &&
                 strstr(document.text, "<field name=\"model\">sale.order</field>") &&
                 strstr(document.text, "<field name=\"arch\" type=\"xml\"></field>") &&
                 !strstr(document.text, "partner_id");
        }
        if (ok)
            ok = buf_undo(&document) && !strcmp(document.text, path) && buf_redo(&document) &&
                 strstr(document.text, "sale.order.form") && buf_undo(&document);
    }
    if (!ok)
        snprintf(error, 1024, "XML inline SSH: %.450s / %.450s", a->xml.error, a->toast);
    app_path_completion_shutdown(a);
    app_xml_shutdown(a);
    buf_free(&document);
    a->tabs[1] = NULL;
    a->tabs[0] = remote;
    a->tab_count = 1;
    a->active = 0;
    if (ok)
        event("xml_inserted", "Every ID, fields, empty arch, atomic undo; remote XML read only", false);
    return ok;
}
static bool xml_roundtrip(App *a, Buffer *remote, char error[1024]) {
    if (!xml_roundtrip_once(a, remote, error))
        return false;
    char project[BUF_PATH];
    strcpy(project, a->project);
    strcpy(a->project, a->last_local_project);
    a->xml_paths.count = 1;
    strcpy(a->xml_paths.paths[0], a->ssh.mountpoint);
    bool ok = xml_roundtrip_once(a, remote, error);
    strcpy(a->project, project);
    a->xml_paths.count = 0;
    return ok;
}
static bool session_roundtrip(App *a, Buffer *remote, const char *root, char error[1024]) {
    char directory[SSH_PATH];
    if (!join(directory, root, "sessions"))
        return false;
    StudioStore writer, reader;
    studio_store_init(&writer, directory, true);
    studio_store_init(&reader, directory, true);
    StudioImage *capture = calloc(1, sizeof(*capture)), *loaded = calloc(1, sizeof(*loaded));
    SshSession *fresh = calloc(1, sizeof(*fresh));
    Buffer external = {0};
    bool ok = false;
    if (!capture || !loaded || !fresh)
        goto done;
    if (!buf_insert(remote, "# draft restored from session\n"))
        goto done;
    a->font_size = 20;
    a->sidebar_width = 246;
    a->terminal_h = 230;
    a->split_ratio = .5f;
    app_session_capture(a, capture);
    if (!studio_save(&writer, capture, error, 1024) ||
        studio_load(&reader, a->last_local_project, loaded, error, 1024) != 1)
        goto done;
    ssh_init(fresh, a->ssh.cache, a->ssh.helper);
    if (studio_reconcile(loaded, fresh, false) || fresh->state != SSH_LOST || fresh->process ||
        fresh->generation || !loaded->tabs[0]->session_pending_remote ||
        strcmp(loaded->tabs[0]->text, remote->text))
        goto done;
    if (buf_save_guarded(loaded->tabs[0], loaded->tabs[0]->path, ssh_save_guard, fresh, error, 1024))
        goto done;
    if (studio_reconcile(loaded, &a->ssh, true) || !buf_dirty(loaded->tabs[0]) ||
        loaded->tabs[0]->session_pending_remote || strcmp(loaded->tabs[0]->path, remote->path))
        goto done;
    if (!buf_load(&external, remote->path, error, 1024) || !buf_insert(&external, "# external change\n") ||
        !buf_save_guarded(&external, external.path, ssh_save_guard, &a->ssh, error, 1024))
        goto done;
    loaded->tabs[0]->session_pending_remote = true;
    if (studio_reconcile(loaded, &a->ssh, true) != 1 || *loaded->tabs[0]->path ||
        !buf_dirty(loaded->tabs[0]) || strcmp(loaded->tabs[0]->text, remote->text))
        goto done;
    if (!buf_undo(remote) || buf_dirty(remote))
        goto done;
    event("session_recovered", "offline snapshot, explicit SSH validation and external conflict preserved",
          false);
    ok = true;
done:
    if (!ok)
        snprintf(error, 1024, "native remote session roundtrip failed");
    free(capture);
    if (loaded) {
        studio_image_free(loaded);
        free(loaded);
    }
    free(fresh);
    buf_free(&external);
    studio_store_close(&writer);
    studio_store_close(&reader);
    return ok;
}
int main(int argc, char **argv) {
    if (argc != 8)
        return 2;
    /* HELPER CONFIG SOURCE CACHE KEY MODE LOCAL_ROOT; all are nonsecret. */
    App *a = calloc(1, sizeof(*a));
    Buffer *remote = calloc(1, sizeof(*remote));
    if (!a || !remote)
        return 2;
    snprintf(a->project, sizeof(a->project), "%s", argv[7]);
    app_ssh_init(a, argv[1]);
    snprintf(a->ssh.cache, sizeof(a->ssh.cache), "%s", argv[4]);
    snprintf(a->ssh.config_override, sizeof(a->ssh.config_override), "%s", argv[2]);
    a->running = true;
    app_ssh_action(a, A_SSH, 0);
    const char *fields[] = {"prisme-test", "odoo", "2222", argv[3], argv[5]};
    for (int i = 0; i < 5; i++)
        app_set_field(&a->ssh_fields[i], fields[i], false);
    app_ssh_action(a, A_SSH_CONNECT, 0);
    const char *mode = argv[6];
    char error[1024] = "";
    double deadline = now() + 35;
    bool sent = false, ready = false, finished = false, failed = false;
    int phase = 0, held = -1;
    while (now() < deadline) {
        a->frame_tick++;
        app_ssh_poll(a);
        if (a->ssh.auth_pending && !sent) {
            char prompt[24000];
            snprintf(prompt, sizeof(prompt), "%.6000s\n%s", a->ssh.auth_confirm ? a->ssh.log : "",
                     a->ssh.prompt);
            event("auth", prompt, a->ssh.auth_confirm);
            sent = true;
        }
        if (!a->ssh.auth_pending)
            sent = false;
        struct pollfd input = {STDIN_FILENO, POLLIN, 0};
        if (poll(&input, 1, 0) > 0 && (input.revents & POLLIN)) {
            char line[65537];
            if (!fgets(line, sizeof(line), stdin)) {
                failed = true;
                break;
            }
            SjDoc d;
            SjToken tokens[16];
            char answer[SSH_AUTH_MAX + 1];
            if (!sj_parse(&d, line, strlen(line), tokens, 16)) {
                failed = true;
                break;
            }
            int token = sj_get(&d, 0, "answer");
            if (a->ssh.auth_pending) {
                if (sj_is(&d, token, SJ_NULL))
                    app_ssh_close_modal(a);
                else if (sj_string(&d, token, answer, sizeof(answer))) {
                    if (a->ssh.auth_confirm)
                        app_ssh_action(a, !strcmp(answer, "yes") ? A_SSH_AUTH_YES : A_SSH_AUTH_NO, 0);
                    else {
                        app_set_field(&a->ssh_auth, answer, false);
                        app_ssh_action(a, A_SSH_AUTH_SEND, 0);
                    }
                } else
                    failed = true;
                sent = false; /* A second askpass can arrive in the very next frame. */
                ssh_erase(answer, sizeof(answer));
                ssh_erase(line, sizeof(line));
            }
        }
        if (a->ssh.state == SSH_ERROR && !a->ssh.process && !a->ssh.probe) {
            event("error", *a->ssh.error ? a->ssh.error : a->toast, false);
            failed = true;
            break;
        }
        if (a->ssh.state == SSH_MOUNTED && !ready) {
            ready = true;
            event("ready", a->ssh.mountpoint, false);
            /* Another manager with this profile cannot steal/clean this mount. */
            SshSession *other = calloc(1, sizeof(*other));
            if (!other) {
                failed = true;
                break;
            }
            ssh_init(other, argv[4], argv[1]);
            snprintf(other->config_override, sizeof(other->config_override), "%s", argv[2]);
            long generation = ssh_mount_id(a->ssh.mountpoint, true);
            if (ssh_connect(other, &a->ssh.profile, error, sizeof(error)) ||
                !ssh_close(other, error, sizeof(error)) ||
                ssh_mount_id(a->ssh.mountpoint, true) != generation) {
                free(other);
                failed = true;
                break;
            }
            free(other);
            if (!exercise(a, remote, argv[7], error)) {
                event("failure", error, false);
                failed = true;
                break;
            }
            a->tabs[0] = remote;
            a->tab_count = 1;
            if (!strcmp(mode, "search") && !folder_roundtrip(a, remote, argv[7], error)) {
                event("failure", error, false);
                failed = true;
                break;
            }
            if (!strcmp(mode, "xml") && !xml_roundtrip(a, remote, error)) {
                event("failure", error, false);
                failed = true;
                break;
            }
            if (!strcmp(mode, "session") && !session_roundtrip(a, remote, argv[7], error)) {
                event("failure", error, false);
                failed = true;
                break;
            }
            if (!strcmp(mode, "reconnect")) {
                if (!buf_insert(remote, "# kept draft\n")) {
                    failed = true;
                    break;
                }
                app_ssh_action(a, A_SSH_DISCONNECT, 0);
                if (a->ssh.state != SSH_MOUNTED || !buf_dirty(remote)) {
                    event("failure", "dirty disconnect guard", false);
                    failed = true;
                    break;
                }
                phase = 1;
                event("drop_ready", "dirty buffer preserved", false);
            } else {
                if (!strcmp(mode, "busy"))
                    held = open(a->ssh.mountpoint, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
                app_ssh_action(a, A_SSH_DISCONNECT, 0);
                phase = 1;
            }
        }
        if (!strcmp(mode, "reconnect") && phase == 1 && a->ssh.state == SSH_LOST) {
            if (buf_save_guarded(remote, remote->path, ssh_save_guard, &a->ssh, error, sizeof(error)) ||
                !buf_dirty(remote)) {
                event("failure", "offline save guard", false);
                failed = true;
                break;
            }
            Buffer salvage;
            if (!buf_init(&salvage, remote->text, "salvage.py")) {
                failed = true;
                break;
            }
            char path[SSH_PATH];
            join(path, argv[7], "salvage.py");
            bool saved = buf_save_guarded(&salvage, path, ssh_save_guard, &a->ssh, error, sizeof(error));
            buf_free(&salvage);
            if (!saved) {
                event("failure", error, false);
                failed = true;
                break;
            }
            app_ssh_action(a, A_SSH_RECONNECT, 0);
            phase = 2;
        }
        if (!strcmp(mode, "reconnect") && phase == 2 && a->ssh.state == SSH_MOUNTED) {
            if (!buf_dirty(remote) || !strstr(remote->text, "kept draft") ||
                !buf_save_guarded(remote, remote->path, ssh_save_guard, &a->ssh, error, sizeof(error))) {
                event("failure", error, false);
                failed = true;
                break;
            }
            event("reconnected", "draft saved", false);
            app_ssh_action(a, A_SSH_DISCONNECT, 0);
            phase = 3;
        }
        if (!strcmp(mode, "busy") && phase == 1 && a->ssh.state == SSH_MOUNTED &&
            strstr(a->ssh.error, "occupé")) {
            event("busy_protected", "normal unmount refused", false);
            if (held >= 0) {
                close(held);
                held = -1;
            }
            app_ssh_action(a, A_SSH_DISCONNECT, 0);
            phase = 2;
        }
        if (ready && phase && a->ssh.state == SSH_IDLE) {
            if (!buf_insert(remote, "# offline draft\n") ||
                buf_save_guarded(remote, remote->path, ssh_save_guard, &a->ssh, error, sizeof(error)) ||
                !buf_dirty(remote)) {
                event("failure", "stale mount save", false);
                failed = true;
                break;
            }
            event("done", "native CRUD, guards and clean unmount", false);
            finished = true;
            break;
        }
        struct timespec delay = {0, 5000000};
        nanosleep(&delay, NULL);
    }
    if (held >= 0)
        close(held);
    if (!finished && !failed)
        event("failure", "timeout", false);
    app_path_completion_shutdown(a);
    app_xml_shutdown(a);
    app_folder_search_shutdown(a);
    if (a->ssh.process || ssh_mounted(&a->ssh)) {
        char close_error[1024] = "";
        if (!ssh_close(&a->ssh, close_error, sizeof(close_error)))
            event("cleanup_error", close_error, false);
    }
    if (*a->ssh.mountpoint && ssh_mount_id(a->ssh.mountpoint, false) != 0)
        failed = true;
    app_ssh_free(a);
    buf_free(&a->field);
    buf_free(remote);
    free(remote);
    free(a);
    return finished && !failed ? 0 : 1;
}
