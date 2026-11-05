#include "app_internal.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static bool busy(const App *a) {
    return a->ssh.state != SSH_IDLE && a->ssh.state != SSH_ERROR;
}
static void report(App *a, const char *error) {
    snprintf(a->ssh.error, sizeof(a->ssh.error), "%s", error);
    app_toast(a, error, true);
}
static bool refresh_profiles(App *a) {
    char error[512] = "";
    if (!*a->ssh_profiles_path ||
        !ssh_profiles_load(a->ssh_profiles_path, &a->ssh_profiles, error, sizeof(error))) {
        report(a, *error ? error : "Chemin des profils SSH invalide.");
        return false;
    }
    return true;
}
static void set_fields(App *a, const SshProfile *p) {
    const char *fields[] = {p->host, p->user, p->port, p->folder, p->identity};
    for (int i = 0; i < 5; i++)
        app_set_field(&a->ssh_fields[i], fields[i], false);
    snprintf(a->ssh_selected, sizeof(a->ssh_selected), "%s", p->id);
    a->ssh_field = 0;
    a->modal = M_SSH;
    a->focus = 0;
    app_completion_dismiss(a);
    a->window_drag = a->dragging = a->resizing_split = a->resizing_sidebar = false;
    a->window_resize = 0;
}
static void show(App *a) {
    (void)refresh_profiles(a);
    SshProfile profile;
    ssh_profile_init(&profile);
    if (busy(a))
        profile = a->ssh.profile;
    else
        for (int i = 0; i < a->ssh_profiles.count; i++)
            if (!strcmp(a->ssh_profiles.last, a->ssh_profiles.items[i].id))
                profile = a->ssh_profiles.items[i];
    set_fields(a, &profile);
}
static bool field_value(char *out, size_t cap, const Buffer *b, bool trim) {
    size_t begin = 0, end = b->len;
    if (trim) {
        while (begin < end && isspace((unsigned char)b->text[begin]))
            begin++;
        while (end > begin && isspace((unsigned char)b->text[end - 1]))
            end--;
    }
    if (end - begin >= cap)
        return false;
    memcpy(out, b->text + begin, end - begin);
    out[end - begin] = 0;
    return true;
}
static bool remember(App *a, SshProfile *p) {
    ssh_profile_init(p);
    snprintf(p->id, sizeof(p->id), "%s", a->ssh_selected);
    char error[512] = "";
    bool ok = field_value(p->host, sizeof(p->host), &a->ssh_fields[0], true) &&
              field_value(p->user, sizeof(p->user), &a->ssh_fields[1], true) &&
              field_value(p->port, sizeof(p->port), &a->ssh_fields[2], true) &&
              field_value(p->folder, sizeof(p->folder), &a->ssh_fields[3], false) &&
              field_value(p->identity, sizeof(p->identity), &a->ssh_fields[4], true);
    if (!ok || !ssh_profiles_put(a->ssh_profiles_path, p, error, sizeof(error))) {
        report(a, *error ? error : "Paramètre SSH trop long.");
        return false;
    }
    snprintf(a->ssh_selected, sizeof(a->ssh_selected), "%s", p->id);
    (void)refresh_profiles(a);
    return true;
}
static void clear_secret(App *a) {
    Buffer *b = &a->ssh_auth;
    if (b->text)
        ssh_erase(b->text, b->capacity);
    if (b->saved)
        ssh_erase(b->saved, strlen(b->saved));
    History *h[] = {&b->undo, &b->redo};
    for (int i = 0; i < 2; i++)
        for (int j = 0; j < h[i]->count; j++) {
            Snapshot *s = &h[i]->items[j];
            if (s->text)
                ssh_erase(s->text, s->len);
        }
    buf_free(b);
    buf_init(b, "", "authentification SSH");
}
void app_ssh_init(App *a, const char *helper) {
    ssh_init(&a->ssh, NULL, helper);
    snprintf(a->last_local_project, sizeof(a->last_local_project), "%s", a->project);
    (void)ssh_profiles_default_path(a->ssh_profiles_path, sizeof(a->ssh_profiles_path));
    for (int i = 0; i < 5; i++)
        buf_init(&a->ssh_fields[i], "", "paramètre SSH");
    buf_init(&a->ssh_auth, "", "authentification SSH");
    fs_set_guard(ssh_guard, &a->ssh);
}
void app_ssh_free(App *a) {
    clear_secret(a);
    buf_free(&a->ssh_auth);
    for (int i = 0; i < 5; i++)
        buf_free(&a->ssh_fields[i]);
    char error[1024] = "";
    if (!ssh_close(&a->ssh, error, sizeof(error)))
        fprintf(stderr, "SSH : %s\n", error);
    fs_set_guard(NULL, NULL);
}
static void connect_profile(App *a) {
    if (busy(a)) {
        report(a, "Déconnectez la session actuelle avant de changer de profil.");
        return;
    }
    if (a->file_job || a->runner.running) {
        report(a, "Terminez les opérations de fichiers et arrêtez F5 avant la connexion SSH.");
        return;
    }
    SshProfile p;
    /* Persist before network/preflight failures, never after authentication. */
    if (!remember(a, &p))
        return;
    if (*a->ssh.profile.host && !ssh_profile_equal(&p, &a->ssh.profile)) {
        for (int i = 0; i < a->tab_count; i++)
            if (*a->tabs[i]->path && ssh_owns(&a->ssh, a->tabs[i]->path)) {
                report(a,
                       "Fermez les onglets de la précédente connexion avant de changer de serveur/dossier.");
                return;
            }
    }
    char error[1024] = "";
    if (!ssh_connect(&a->ssh, &p, error, sizeof(error))) {
        a->ssh.state = SSH_ERROR;
        report(a, error);
    }
    a->modal = M_SSH;
}
static void abort_session_quit(App *a) {
    a->ssh_quit_pending = false;
    a->session_committing_quit = false;
    a->session_keep = false;
    memset(a->session_discard, 0, sizeof(a->session_discard));
    app_session_checkpoint(a, true);
}
static void disconnect_profile(App *a, bool reconnect, bool quitting) {
    if (a->ssh.state == SSH_CONNECTING) {
        a->ssh_quit_pending = quitting;
        ssh_cancel(&a->ssh);
        a->modal = M_SSH;
        return;
    }
    if (a->file_job || a->runner.running) {
        if (quitting)
            abort_session_quit(a);
        report(a, "Terminez les opérations de fichiers et arrêtez F5 avant de déconnecter SSH.");
        return;
    }
    for (int i = 0; i < a->tab_count; i++)
        if (!reconnect && !(quitting && a->session_committing_quit) && ssh_owns(&a->ssh, a->tabs[i]->path) &&
            buf_dirty(a->tabs[i])) {
            report(
                a,
                "Documents SSH modifiés : enregistrez ou utilisez Enregistrer sous vers un dossier local.");
            return;
        }
    char error[1024] = "";
    app_ssh_reset_analysis(a);
    if (!ssh_disconnect(&a->ssh, error, sizeof(error))) {
        if (quitting)
            abort_session_quit(a);
        report(a, error);
        return;
    }
    a->ssh_reconnect = reconnect;
    a->ssh_quit_pending = quitting;
    a->modal = M_SSH;
}
static void answer(App *a, const char *text) {
    char error[512] = "";
    bool ok = ssh_answer(&a->ssh, text, error, sizeof(error));
    clear_secret(a);
    a->modal = a->ssh_previous_modal;
    if (!ok)
        report(a, error);
}
bool app_ssh_close_modal(App *a) {
    if (a->modal == M_SSH_AUTH) {
        answer(a, NULL);
        ssh_cancel(&a->ssh);
        return true;
    }
    if (a->modal == M_SSH) {
        if (a->ssh.state == SSH_DISCONNECTING || a->ssh_quit_pending)
            return true;
        ssh_cancel(&a->ssh);
    }
    return false;
}
bool app_ssh_request_quit(App *a) {
    if (!a->ssh.initialized)
        return false;
    if (a->ssh.state == SSH_DISCONNECTING) {
        a->ssh_quit_pending = true;
        a->modal = M_SSH;
        return true;
    }
    if (busy(a) || a->ssh.process > 0 || ssh_mounted(&a->ssh)) {
        disconnect_profile(a, false, true);
        return true;
    }
    return false;
}
void app_ssh_poll(App *a) {
    SshEvent event = ssh_poll(&a->ssh);
    if (event == SSH_EVENT_READY) {
        if (a->ssh_quit_pending)
            disconnect_profile(a, false, true);
        else if (ssh_is_dir(&a->ssh, a->ssh.mountpoint)) {
            app_session_remote_ready(a);
            app_ssh_change_project(a, a->ssh.mountpoint);
            a->modal = M_NONE;
            app_toast(a, "Dossier SSH ouvert · Python et PyLint restent locaux", false);
        }
    } else if (event == SSH_EVENT_DISCONNECTED) {
        clear_secret(a);
        if (a->ssh_reconnect) {
            a->ssh_reconnect = false;
            char error[1024] = "";
            if (!ssh_connect(&a->ssh, &a->ssh.profile, error, sizeof(error))) {
                a->ssh.state = SSH_ERROR;
                report(a, error);
            }
        } else if (a->ssh_quit_pending)
            a->running = false;
        else {
            app_ssh_drop_remote_tabs(a);
            a->modal = M_NONE;
            app_toast(a, "Dossier SSH déconnecté.", false);
        }
    } else if (event == SSH_EVENT_ERROR || event == SSH_EVENT_LOST) {
        clear_secret(a);
        a->ssh_reconnect = false;
        if (a->ssh_quit_pending && !ssh_mounted(&a->ssh) && !a->ssh.process && !a->ssh.probe) {
            a->running = false;
            return;
        }
        if (a->ssh_quit_pending)
            abort_session_quit(a);
        if (a->modal == M_SSH_AUTH)
            a->modal = a->ssh_previous_modal;
        app_ssh_reset_analysis(a);
        app_toast(a, a->ssh.error, true);
    }
    if (a->modal == M_SSH_AUTH && !a->ssh.auth_pending) {
        clear_secret(a);
        a->modal = a->ssh_previous_modal;
    }
    if (a->ssh.auth_pending && a->modal != M_SSH_AUTH) {
        a->ssh_previous_modal = a->modal;
        a->modal = M_SSH_AUTH;
        a->ssh_prompt_scroll = 0;
        a->ssh_auth_frame = a->frame_tick;
        clear_secret(a);
        app_completion_dismiss(a);
    }
}
void app_ssh_action(App *a, Action id, int arg) {
    char error[1024] = "";
    if (id == A_SSH) {
        show(a);
        return;
    }
    if (id == A_SSH_CANCEL) {
        ssh_cancel(&a->ssh);
        return;
    }
    if (id == A_SSH_CONNECT) {
        connect_profile(a);
        return;
    }
    if (id == A_SSH_DISCONNECT || id == A_SSH_RECONNECT) {
        disconnect_profile(a, id == A_SSH_RECONNECT, false);
        return;
    }
    if (id == A_SSH_AUTH_YES || id == A_SSH_AUTH_NO || id == A_SSH_AUTH_SEND) {
        if (a->modal != M_SSH_AUTH)
            return;
        if (id == A_SSH_AUTH_SEND && !a->ssh.auth_confirm)
            answer(a, a->ssh_auth.text);
        else if (a->ssh.auth_confirm && id != A_SSH_AUTH_SEND)
            answer(a, id == A_SSH_AUTH_YES ? "yes" : "no");
        return;
    }
    if (busy(a))
        return;
    if (id == A_SSH_FIELD && arg >= 0 && arg < 5)
        a->ssh_field = arg;
    else if (id == A_SSH_NEW) {
        SshProfile p;
        ssh_profile_init(&p);
        set_fields(a, &p);
    } else if (id == A_SSH_SAVE_PROFILE) {
        SshProfile p;
        if (remember(a, &p))
            app_toast(a, "Profil SSH enregistré · aucun secret", false);
    } else if (id == A_SSH_PROFILES) {
        if (!refresh_profiles(a))
            return;
        a->modal = M_SSH_PROFILES;
        a->ssh_profile_index = 0;
        for (int i = 0; i < a->ssh_profiles.count; i++)
            if (!strcmp(a->ssh_profiles.items[i].id, a->ssh_selected))
                a->ssh_profile_index = i;
        a->ssh_profile_scroll = a->ssh_profile_index > 7 ? a->ssh_profile_index - 7 : 0;
    } else if (id == A_SSH_PROFILE_PICK && arg >= 0 && arg < a->ssh_profiles.count) {
        char key[33];
        snprintf(key, sizeof(key), "%s", a->ssh_profiles.items[arg].id);
        if (!ssh_profiles_select(a->ssh_profiles_path, key, error, sizeof(error))) {
            report(a, error);
            return;
        }
        if (refresh_profiles(a))
            for (int i = 0; i < a->ssh_profiles.count; i++)
                if (!strcmp(key, a->ssh_profiles.items[i].id)) {
                    set_fields(a, &a->ssh_profiles.items[i]);
                    break;
                }
    } else if (id == A_SSH_PROFILE_REMOVE && a->ssh_profile_index >= 0 &&
               a->ssh_profile_index < a->ssh_profiles.count) {
        if (!ssh_profiles_delete(a->ssh_profiles_path, a->ssh_profiles.items[a->ssh_profile_index].id, error,
                                 sizeof(error))) {
            report(a, error);
            return;
        }
        if (refresh_profiles(a)) {
            snprintf(a->ssh_selected, sizeof(a->ssh_selected), "%s", a->ssh_profiles.last);
            app_ssh_action(a, A_SSH_PROFILES, 0);
        }
        app_toast(a, "Profil retiré · aucun fichier distant supprimé", false);
    }
}
static bool pressed(App *a, int key) {
    return app_key_pressed(a, key);
}
void app_ssh_keys(App *a, const unsigned *chars, int count, bool ctrl, bool shift) {
    if (a->modal == M_SSH_AUTH) {
        if (a->ssh.auth_confirm) {
            bool fresh = (a->keys[KEY_ENTER] || IsKeyPressed(KEY_ENTER)) && !IsKeyPressedRepeat(KEY_ENTER);
            if (ctrl && fresh && a->frame_tick > a->ssh_auth_frame)
                app_ssh_action(a, A_SSH_AUTH_YES, 0);
        } else {
            if (!(ctrl && (pressed(a, KEY_C) || pressed(a, KEY_X))))
                app_edit_field(a, &a->ssh_auth, chars, count, ctrl, shift);
            if (a->ssh_auth.len > SSH_AUTH_MAX)
                clear_secret(a);
            if (pressed(a, KEY_ENTER))
                app_ssh_action(a, A_SSH_AUTH_SEND, 0);
        }
        return;
    }
    if (busy(a))
        return;
    if (a->modal == M_SSH_PROFILES) {
        int total = a->ssh_profiles.count;
        if (pressed(a, KEY_DOWN) && a->ssh_profile_index + 1 < total)
            a->ssh_profile_index++;
        if (pressed(a, KEY_UP) && a->ssh_profile_index > 0)
            a->ssh_profile_index--;
        if (a->ssh_profile_index < a->ssh_profile_scroll)
            a->ssh_profile_scroll = a->ssh_profile_index;
        if (a->ssh_profile_index > a->ssh_profile_scroll + 7)
            a->ssh_profile_scroll = a->ssh_profile_index - 7;
        if (pressed(a, KEY_ENTER) && total)
            app_ssh_action(a, A_SSH_PROFILE_PICK, a->ssh_profile_index);
        if (pressed(a, KEY_DELETE) && total)
            app_ssh_action(a, A_SSH_PROFILE_REMOVE, 0);
        return;
    }
    if (ctrl && pressed(a, KEY_S)) {
        app_ssh_action(a, A_SSH_SAVE_PROFILE, 0);
        return;
    }
    int index = a->ssh_field;
    if (index < 0 || index > 4)
        return;
    Buffer *b = &a->ssh_fields[index];
    char before[SSH_PATH];
    size_t cursor = b->cursor, anchor = b->anchor;
    snprintf(before, sizeof(before), "%s", b->text);
    app_edit_field(a, b, chars, count, ctrl, shift);
    const size_t limits[] = {253, 128, 5, SSH_PATH - 1, SSH_PATH - 1};
    if (b->len > limits[index]) {
        app_set_field(b, before, false);
        b->cursor = cursor;
        b->anchor = anchor;
        app_toast(a, "Champ SSH trop long : valeur précédente conservée.", true);
    }
    if (pressed(a, KEY_TAB))
        a->ssh_field = (index + (shift ? 4 : 1)) % 5;
    if (pressed(a, KEY_ENTER)) {
        if (index < 4)
            a->ssh_field++;
        else
            connect_profile(a);
    }
}
