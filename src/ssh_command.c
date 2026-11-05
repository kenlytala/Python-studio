#define _POSIX_C_SOURCE 200809L
#include "ssh.h"
#include "ssh_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void ssh_erase(void *data, size_t n) {
    volatile unsigned char *p = data;
    while (n--)
        *p++ = 0;
}
/* SSHFS splits ssh_command on spaces; it does NOT interpret shell quotes.
   Only transport words which need no quoting may go there. IdentityFile is a
   separate OpenSSH -o value: its embedded quotes are parsed by OpenSSH itself. */
static bool client_word(SjOut *out, const char *word) {
    if (strpbrk(word, " \t\r\n,\\\"'"))
        return out->ok = false;
    return sj_add(out, word);
}
bool ssh_command(SshCommand *c, const SshProfile *p, const char *point, const char *sshfs, const char *ssh,
                 const char *config, char *error, size_t cap) {
    if (!ssh_profile_valid(p, true, error, cap))
        return false;
    memset(c, 0, sizeof(*c));
    int n = strchr(p->host, ':') ? snprintf(c->source, sizeof(c->source), "[%s]:%s", p->host, p->folder)
                                 : snprintf(c->source, sizeof(c->source), "%s:%s", p->host, p->folder);
    if (n < 0 || (size_t)n >= sizeof(c->source))
        return false;
    char client[16384];
    SjOut words;
    sj_out(&words, client, sizeof(client));
    client_word(&words, ssh);
    if (config && *config) {
        sj_add(&words, " -F ");
        client_word(&words, config);
    }
    sj_add(&words, " -a -x -o ClearAllForwardings=yes -o PermitLocalCommand=no -o RequestTTY=no -o "
                   "ControlMaster=no -o ControlPath=none");
    if (*p->user) {
        sj_add(&words, " -l ");
        client_word(&words, p->user);
    }
    if (*p->port) {
        sj_add(&words, " -p ");
        client_word(&words, p->port);
    }
    /* All client flags belong inside ssh_command; -F must not strip security. */
    sj_add(
        &words,
        " -o ServerAliveInterval=10 -o ServerAliveCountMax=2 -o ConnectTimeout=15 -o "
        "StrictHostKeyChecking=ask -o FingerprintHash=sha256 -o BatchMode=no -o NumberOfPasswordPrompts=3");
    if (!words.ok || strchr(client, ',')) {
        snprintf(error, cap,
                 "Commande SSH trop longue ou chemin du client/config non pris en charge (sans espace ni "
                 "virgule).");
        return false;
    }
    snprintf(c->options, sizeof(c->options), "ssh_command=%s", client);
    char **v = c->argv;
    v[c->count++] = (char *)sshfs;
    v[c->count++] = "-f";
    v[c->count++] = c->source;
    v[c->count++] = (char *)point;
    const char *options[] = {"contain_symlinks", "default_permissions", "idmap=user", "attr_timeout=3",
                             c->options};
    for (size_t i = 0; i < sizeof(options) / sizeof(options[0]); i++) {
        v[c->count++] = "-o";
        v[c->count++] = (char *)options[i];
    }
    if (*p->identity) {
        char key[SSH_PATH];
        const char *identity = p->identity;
        if (identity[0] == '~' && identity[1] == '/') {
            const char *home = getenv("HOME");
            if (!home || snprintf(key, sizeof(key), "%s/%s", home, identity + 2) >= (int)sizeof(key))
                return false;
            identity = key;
        }
        SjOut option;
        sj_out(&option, c->identity_option, sizeof(c->identity_option));
        sj_add(&option, "IdentityFile=\"");
        for (const char *p = identity; *p; p++) {
            if (*p == '\\' || *p == '\"')
                sj_add(&option, "\\");
            char c[2] = {*p, 0};
            sj_add(&option, c);
        }
        sj_add(&option, "\"");
        if (!option.ok)
            return false;
        v[c->count++] = "-o";
        v[c->count++] = c->identity_option;
    }
    v[c->count] = NULL;
    return true;
}
const char *ssh_state_label(SshState state) {
    switch (state) {
    case SSH_IDLE:
        return "Prêt · clés/agent, mot de passe ou passphrase";
    case SSH_CONNECTING:
        return "Connexion… authentification dans l’éditeur";
    case SSH_MOUNTED:
        return "Connecté · fichiers distants, Python et PyLint locaux";
    case SSH_LOST:
        return "Connexion perdue · brouillons conservés";
    case SSH_DISCONNECTING:
        return "Déconnexion… aucun démontage forcé";
    default:
        return "Connexion impossible · configuration mémorisée";
    }
}
