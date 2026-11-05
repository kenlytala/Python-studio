#include "app_internal.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define RECT(x, y, w, h) ((Rectangle){(float)(x), (float)(y), (float)(w), (float)(h)})
static void text(App *a, const char *s, float x, float y, float w, float size, Tone tone) {
    ui_text_fit(&a->ui, s, x, y, w, size, tone, false);
}
static void button(App *a, Rectangle r, Action action, int arg, const char *label, int style) {
    ui_button(&a->ui, r, action, arg, label, -1, style, false, NULL);
}
static void form(App *a) {
    Rectangle r = app_ssh_shell(a, "Ouvrir un dossier distant — SSH",
                                "SSHFS / SFTP · Python, PyLint et dépendances restent locaux", 760, 580);
    bool busy = a->ssh.state != SSH_IDLE && a->ssh.state != SSH_ERROR;
    if (!busy) {
        char label[64];
        snprintf(label, sizeof(label), "Profils (%d)", a->ssh_profiles.count);
        button(a, RECT(r.x + 27, r.y + 81, 144, 30), A_SSH_PROFILES, 0, label, 1);
        button(a, RECT(r.x + 182, r.y + 81, 112, 30), A_SSH_NEW, 0, "Nouvelle", 1);
        button(a, RECT(r.x + 306, r.y + 81, 178, 30), A_SSH_SAVE_PROFILE, 0, "Enregistrer le profil", 1);
    } else
        text(a, "Profil conservé · déconnectez avant de changer de connexion", r.x + 27, r.y + 88,
             r.width - 54, 12, C_MUTED);
    const char *labels[] = {"Hôte / alias SSH", "Utilisateur (vide = config)", "Port (vide = config)",
                            "Dossier distant absolu, ou .", "Clé privée (vide = agent/config)"};
    for (int i = 0; i < 5; i++) {
        float y = r.y + 127 + (float)i * 54;
        text(a, labels[i], r.x + 27, y, r.width - 54, 12, C_MUTED);
        app_field_draw(a, &a->ssh_fields[i], RECT(r.x + 27, y + 20, r.width - 54, 32),
                       !busy && a->ssh_field == i, "", A_NONE, false);
        if (!busy)
            ui_hit(&a->ui, RECT(r.x + 27, y + 20, r.width - 54, 32), A_SSH_FIELD, i, NULL);
    }
    text(a, ssh_state_label(a->ssh.state), r.x + 27, r.y + 422, r.width - 54, 12,
         a->ssh.state == SSH_MOUNTED ? C_ACCENT : C_MUTED);
    /* Long errors remain accessible in the tooltip; no unbounded drawn text. */
    text(a, a->ssh.error, r.x + 27, r.y + 449, r.width - 54, 12, C_RED);
    if (*a->ssh.error)
        ui_hit(&a->ui, RECT(r.x + 27, r.y + 444, r.width - 54, 40), A_NONE, 0, a->ssh.error);
    text(a, "Clé du serveur vérifiée · aucun mot de passe ni passphrase enregistré", r.x + 27, r.y + 510,
         r.width - 54, 12, C_MUTED);
    if (a->ssh.state == SSH_MOUNTED || a->ssh.state == SSH_LOST) {
        button(a, RECT(r.x + 27, r.y + 532, 140, 32), A_SSH_DISCONNECT, 0, "Déconnecter", 1);
        button(a, RECT(r.x + 182, r.y + 532, 145, 32), A_SSH_RECONNECT, 0, "Reconnecter", 1);
    } else if (a->ssh.state != SSH_DISCONNECTING) {
        button(a, RECT(r.x + r.width - 178, r.y + 532, 150, 32),
               a->ssh.state == SSH_CONNECTING ? A_SSH_CANCEL : A_SSH_CONNECT, 0,
               a->ssh.state == SSH_CONNECTING ? "Annuler" : "Se connecter", 2);
    }
}
static void profiles(App *a) {
    Rectangle r = app_ssh_shell(a, "Connexions SSH mémorisées",
                                "Hôte, utilisateur, port, dossier et chemin de clé · aucun secret", 760, 580);
    for (int row = 0; row < 8 && row + a->ssh_profile_scroll < a->ssh_profiles.count; row++) {
        int index = row + a->ssh_profile_scroll;
        SshProfile *p = &a->ssh_profiles.items[index];
        float y = r.y + 86 + (float)row * 50;
        char label[SSH_PATH + 500];
        ssh_profile_label(p, label, sizeof(label));
        ui_box(r.x + 27, y, r.width - 54, 44, index == a->ssh_profile_index ? C_ACCENT : C_RAISED, 5,
               index == a->ssh_profile_index ? 18 : 255);
        text(a, label, r.x + 39, y + 5, r.width - 78, 13, C_TEXT);
        text(a, *p->identity ? p->identity : "Agent / clé de ~/.ssh/config", r.x + 39, y + 25, r.width - 78,
             11, C_MUTED);
        ui_hit(&a->ui, RECT(r.x + 27, y, r.width - 54, 44), A_SSH_PROFILE_PICK, index, p->folder);
    }
    if (!a->ssh_profiles.count)
        text(a, "Aucun profil · ajoutez une nouvelle connexion.", r.x + 27, r.y + 112, r.width - 54, 13,
             C_MUTED);
    text(a, "Entrée : choisir · Suppr : retirer le profil (pas les fichiers)", r.x + 27, r.y + 493,
         r.width - 54, 12, C_MUTED);
    button(a, RECT(r.x + 27, r.y + 529, 120, 32), A_SSH, 0, "Retour", 1);
    button(a, RECT(r.x + 161, r.y + 529, 120, 32), A_SSH_NEW, 0, "Nouvelle", 1);
    if (a->ssh_profiles.count)
        button(a, RECT(r.x + r.width - 202, r.y + 529, 175, 32), A_SSH_PROFILE_REMOVE, 0,
               "Retirer la sélection", 1);
}
static int wrap(char lines[256][256], const char *s, int count) {
    size_t len = strlen(s), col = 0, bytes = 0;
    if (count >= 256)
        return count;
    lines[count][0] = 0;
    for (size_t pos = 0; pos < len && count < 256;) {
        if (s[pos] == '\r') {
            pos++;
            continue;
        }
        if (s[pos] == '\n' || col >= 90 || bytes >= 248) {
            lines[count][bytes] = 0;
            count++;
            bytes = col = 0;
            if (count >= 256)
                break;
            lines[count][0] = 0;
            if (s[pos] == '\n') {
                pos++;
                continue;
            }
        }
        size_t next = utf8_next(s, len, pos), n = next - pos;
        if (!n || bytes + n >= 256)
            break;
        memcpy(lines[count] + bytes, s + pos, n);
        bytes += n;
        col++;
        pos = next;
        lines[count][bytes] = 0;
    }
    return count < 256 ? count + 1 : 256;
}
static size_t glyph_count(const Buffer *b, size_t end) {
    size_t count = 0;
    for (size_t p = 0; p < end && p < b->len; p = utf8_next(b->text, b->len, p))
        count++;
    return count;
}
static void masked_field(App *a, Rectangle r) {
    Buffer *b = &a->ssh_auth;
    char stars[SSH_AUTH_MAX + 1];
    size_t count = glyph_count(b, b->len);
    if (count > SSH_AUTH_MAX)
        count = SSH_AUTH_MAX;
    memset(stars, '*', count);
    stars[count] = 0;
    Buffer masked;
    if (!buf_init(&masked, stars, "réponse masquée"))
        return;
    masked.cursor = glyph_count(b, b->cursor);
    masked.anchor = b->anchor == BUF_NO_SELECTION ? BUF_NO_SELECTION : glyph_count(b, b->anchor);
    app_field_draw(a, &masked, r, true, "", A_NONE, false);
    buf_free(&masked);
}
static void auth(App *a) {
    bool confirm = a->ssh.auth_confirm;
    Rectangle r = app_ssh_shell(a, confirm ? "Vérifier la clé du serveur" : "Authentification SSH",
                                "OpenSSH · réponse transmise uniquement au processus de connexion", 760, 500);
    char lines[256][256];
    int count = 0;
    if (confirm && *a->ssh.log) {
        const char *start = a->ssh.log_len > 4096 ? a->ssh.log + a->ssh.log_len - 4096 : a->ssh.log;
        while (((unsigned char)*start & 0xc0) == 0x80)
            start++;
        count = wrap(lines, start, count);
    }
    count = wrap(lines, a->ssh.prompt, count);
    int last = count > 12 ? count - 12 : 0;
    if (a->ssh_prompt_scroll > last)
        a->ssh_prompt_scroll = last;
    for (int i = 0; i < 12 && i + a->ssh_prompt_scroll < count; i++)
        text(a, lines[i + a->ssh_prompt_scroll], r.x + 27, r.y + 91 + (float)i * 19, r.width - 54, 12,
             C_TEXT);
    if (count > 12)
        text(a, "Molette : lire la suite de la demande", r.x + 27, r.y + 325, r.width - 54, 11, C_MUTED);
    if (confirm) {
        text(a, "Comparez l’empreinte par un canal de confiance avant d’accepter.", r.x + 27, r.y + 392,
             r.width - 54, 12, C_NUMBER);
        button(a, RECT(r.x + 27, r.y + 444, 130, 34), A_SSH_AUTH_NO, 0, "Refuser", 1);
        button(a, RECT(r.x + r.width - 248, r.y + 444, 220, 34), A_SSH_AUTH_YES, 0, "Accepter · Ctrl Entrée",
               2);
    } else {
        masked_field(a, RECT(r.x + 27, r.y + 365, r.width - 54, 35));
        text(a, "Mot de passe / passphrase masqué · Entrée pour répondre", r.x + 27, r.y + 411, r.width - 54,
             12, C_MUTED);
        button(a, RECT(r.x + 27, r.y + 447, 130, 32), A_MODAL_CLOSE, 0, "Annuler", 1);
        button(a, RECT(r.x + r.width - 178, r.y + 447, 150, 32), A_SSH_AUTH_SEND, 0, "Continuer", 2);
    }
}
void app_ssh_draw(App *a) {
    if (a->modal == M_SSH_AUTH)
        auth(a);
    else if (a->modal == M_SSH_PROFILES)
        profiles(a);
    else
        form(a);
}
