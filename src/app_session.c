#include "app_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void app_session_capture(App *a, StudioImage *i) {
    memset(i, 0, sizeof(*i));
    snprintf(i->project, sizeof(i->project), "%s",
             *a->last_local_project ? a->last_local_project : a->project);
    i->count = a->tab_count;
    i->active = a->active;
    i->active_pane = a->active_pane;
    i->panes[0] = a->panes[0].tab;
    i->panes[1] = a->panes[1].tab;
    i->panes[i->active_pane] = i->active;
    bool remote = false;
    for (int n = 0; n < i->count; n++) {
        i->tabs[n] = a->tabs[n];
        i->discard[n] = a->session_committing_quit && a->session_discard[n];
        i->remote[n] = *a->tabs[n]->path && ssh_owns(&a->ssh, a->tabs[n]->path);
        remote = remote || i->remote[n];
    }
    if (remote) {
        snprintf(i->remote_root, sizeof(i->remote_root), "%s", a->ssh.mountpoint);
        i->profile = a->ssh.profile;
    }
    i->flags =
        (a->split ? ST_SPLIT : 0) | (a->minimap ? ST_MINIMAP : 0) | (a->tree_root_open ? ST_TREE_OPEN : 0);
    bool sidebar = a->zen_mode ? a->zen_sidebar : a->sidebar, top = a->zen_mode ? a->zen_topbar : a->topbar,
         terminal = a->zen_mode ? a->zen_terminal : a->terminal;
    i->flags |= (sidebar ? ST_SIDEBAR : 0) | (top ? ST_TOPBAR : 0) | (terminal ? ST_TERMINAL : 0);
    i->ratio = a->split_ratio;
    i->terminal_height = a->terminal_h;
    i->font_size = a->font_size;
    i->sidebar_width = a->sidebar_width;
    i->tree_scroll = a->tree_scroll;
    i->console_mode = a->console_mode;
    i->expanded_count = a->expanded_count;
    for (int n = 0; n < i->expanded_count; n++)
        snprintf(i->expanded[n], BUF_PATH, "%s", a->expanded[n]);
}
bool app_session_checkpoint(App *a, bool force) {
    if (!a->session_ready || !a->session.enabled)
        return true;
    double now = runner_clock();
    if (!force && now < a->session_next)
        return true;
    a->session_next = now + 2;
    StudioImage *i = calloc(1, sizeof(*i));
    if (!i) {
        app_toast(a, "Mémoire insuffisante pour préserver les brouillons.", true);
        return false;
    }
    app_session_capture(a, i);
    uint64_t signature = studio_signature(i);
    if (!force && a->session_signature_valid && signature == a->session_signature) {
        free(i);
        return true;
    }
    char error[512] = "";
    bool ok = studio_save(&a->session, i, error, sizeof(error));
    free(i);
    if (!ok) {
        if (strcmp(a->session_error, error)) {
            app_toast(a, *error ? error : "État/brouillons non enregistrés.", true);
            snprintf(a->session_error, sizeof(a->session_error), "%s", error);
        }
    } else {
        a->session_error[0] = 0;
        a->session_signature = signature;
        a->session_signature_valid = true;
    }
    return ok;
}
bool app_session_restore(App *a) {
    if (!a->session.enabled)
        return false;
    StudioImage *i = calloc(1, sizeof(*i));
    if (!i)
        return false;
    char error[512] = "";
    int result = studio_load(&a->session, a->last_local_project, i, error, sizeof(error));
    if (result != 1) {
        if (result < 0)
            app_toast(a, error, true);
        free(i);
        return false;
    }
    int recovered = studio_reconcile(i, &a->ssh, false);
    if (a->tab_count)
        app_ssh_reset_analysis(a);
    for (int n = 0; n < a->tab_count; n++) {
        buf_free(a->tabs[n]);
        free(a->tabs[n]);
        a->tabs[n] = NULL;
    }
    a->tab_count = i->count;
    for (int n = 0; n < i->count; n++) {
        a->tabs[n] = i->tabs[n];
        i->tabs[n] = NULL;
    }
    a->active = i->active;
    a->active_pane = i->active_pane;
    for (int n = 0; n < 2; n++) {
        a->panes[n].tab = i->panes[n];
        a->panes[n].lex_buffer = NULL;
    }
    a->split = (i->flags & ST_SPLIT) != 0;
    a->sidebar = (i->flags & ST_SIDEBAR) != 0;
    a->topbar = (i->flags & ST_TOPBAR) != 0;
    a->terminal = (i->flags & ST_TERMINAL) != 0;
    a->minimap = (i->flags & ST_MINIMAP) != 0;
    a->tree_root_open = (i->flags & ST_TREE_OPEN) != 0;
    a->split_ratio = (float)i->ratio;
    a->terminal_h = (float)i->terminal_height;
    a->font_size = i->font_size;
    a->sidebar_width = i->sidebar_width;
    a->tree_scroll = i->tree_scroll;
    a->console_mode = i->console_mode;
    a->expanded_count = i->expanded_count;
    for (int n = 0; n < i->expanded_count; n++)
        snprintf(a->expanded[n], BUF_PATH, "%s", i->expanded[n]);
    a->lex_buffer = a->search_buffer = NULL;
    a->caret_x = -1;
    a->session_signature_valid = false;
    memset(a->session_discard, 0, sizeof(a->session_discard));
    app_completion_dismiss(a);
    app_multicursor_reset(a);
    if (recovered)
        app_toast(
            a, "Fichiers absents/changés : brouillons récupérés à part. Enregistrer sous pour les préserver.",
            true);
    else if (a->ssh.state == SSH_LOST)
        app_toast(a, a->ssh.error, false);
    studio_image_free(i);
    free(i);
    return true;
}
void app_session_remote_ready(App *a) {
    StudioImage *i = calloc(1, sizeof(*i));
    if (!i)
        return;
    app_session_capture(a, i);
    int recovered = studio_reconcile(i, &a->ssh, true);
    free(i);
    if (recovered)
        app_toast(a, "Fichiers SSH changés/illisibles : brouillons récupérés à part, aucun écrasement.",
                  true);
}
void app_session_keep_quit(App *a) {
    if (!a->session.enabled) {
        app_toast(a, "Les sessions sont désactivées pour ce lancement.", true);
        return;
    }
    a->session_keep = true;
    memset(a->session_discard, 0, sizeof(a->session_discard));
    a->modal = M_NONE;
    a->pending = 0;
    app_action(a, A_QUIT, 0);
}
