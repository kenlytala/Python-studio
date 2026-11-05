#define _XOPEN_SOURCE 700
#define _POSIX_C_SOURCE 200809L
#include "ssh.h"
#include "ssh_json.h"
#include "buffer.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

static bool fail(char *e, size_t n, const char *message) {
    if (n)
        snprintf(e, n, "%s", message);
    return false;
}
static bool id_valid(const char *id) {
    if (strlen(id) != 32)
        return false;
    for (int i = 0; i < 32; i++)
        if (!strchr("0123456789abcdef", id[i]))
            return false;
    return true;
}
static bool safe_text(const char *s, size_t max, bool comma) {
    size_t n = strlen(s);
    if (n > max || !utf8_valid(s, n))
        return false;
    for (size_t i = 0; i < n; i++)
        if ((unsigned char)s[i] < 32 || s[i] == 127 || (comma && s[i] == ','))
            return false;
    return true;
}
void ssh_profile_init(SshProfile *p) {
    memset(p, 0, sizeof(*p));
    strcpy(p->folder, ".");
}
bool ssh_profile_equal(const SshProfile *a, const SshProfile *b) {
    return !strcmp(a->host, b->host) && !strcmp(a->user, b->user) && !strcmp(a->port, b->port) &&
           !strcmp(a->folder, b->folder) && !strcmp(a->identity, b->identity);
}
void ssh_profile_label(const SshProfile *p, char *out, size_t cap) {
    snprintf(out, cap, "%s%s%s%s%s · %s", p->user, *p->user ? "@" : "", p->host, *p->port ? ":" : "", p->port,
             p->folder);
}
bool ssh_profile_valid(const SshProfile *p, bool check_key, char *e, size_t cap) {
    size_t n = strnlen(p->host, sizeof(p->host));
    if (!n || n >= sizeof(p->host) || (!strchr(p->host, ':') && !isalnum((unsigned char)p->host[0])))
        return fail(e, cap, "Hôte/alias SSH invalide.");
    for (size_t i = 0; i < n; i++)
        if ((unsigned char)p->host[i] >= 128 ||
            !(isalnum((unsigned char)p->host[i]) || strchr("_.:-", p->host[i])))
            return fail(e, cap, "Hôte/alias SSH invalide.");
#ifndef _WIN32
    if (strchr(p->host, ':')) {
        struct in6_addr address;
        if (inet_pton(AF_INET6, p->host, &address) != 1)
            return fail(e, cap, "Adresse IPv6 invalide.");
    }
#endif
    n = strnlen(p->user, sizeof(p->user));
    if (n >= sizeof(p->user) || (n && !(isalnum((unsigned char)p->user[0]) || p->user[0] == '_')))
        return fail(e, cap, "Utilisateur SSH invalide.");
    for (size_t i = 0; i < n; i++)
        if ((unsigned char)p->user[i] >= 128 ||
            !(isalnum((unsigned char)p->user[i]) || strchr("_.@-", p->user[i])))
            return fail(e, cap, "Utilisateur SSH invalide.");
    n = strnlen(p->port, sizeof(p->port));
    if (n >= sizeof(p->port))
        return fail(e, cap, "Port SSH invalide (1–65535, ou vide).");
    unsigned port = 0;
    for (size_t i = 0; i < n; i++) {
        if (p->port[i] < '0' || p->port[i] > '9')
            return fail(e, cap, "Port SSH invalide.");
        port = port * 10 + (unsigned)(p->port[i] - '0');
    }
    if (n && (!port || port > 65535))
        return fail(e, cap, "Port SSH invalide (1–65535, ou vide).");
    if (strnlen(p->folder, sizeof(p->folder)) >= sizeof(p->folder) || !*p->folder ||
        !safe_text(p->folder, SSH_PATH - 1, false) || (strcmp(p->folder, ".") && *p->folder != '/'))
        return fail(e, cap, "Dossier distant : chemin absolu, ou . pour le dossier personnel.");
    if (strnlen(p->identity, sizeof(p->identity)) >= sizeof(p->identity) ||
        !safe_text(p->identity, SSH_PATH - 1, true))
        return fail(e, cap, "Chemin de clé privée invalide (sans virgule ni contrôle).");
#ifndef _WIN32
    if (check_key && *p->identity) {
        struct stat st;
        char path[SSH_PATH];
        const char *name = p->identity;
        if (name[0] == '~' && name[1] == '/') {
            const char *home = getenv("HOME");
            if (!home || snprintf(path, sizeof(path), "%s/%s", home, name + 2) >= (int)sizeof(path))
                return fail(e, cap, "Chemin de clé privée trop long.");
            name = path;
        }
        if (stat(name, &st) || !S_ISREG(st.st_mode))
            return fail(e, cap, "Clé privée introuvable.");
    }
#else
    (void)check_key;
#endif
    return true;
}
bool ssh_profiles_default_path(char *out, size_t cap) {
    const char *base = getenv("PRISME_CONFIG_DIR");
    int n;
    if (base && *base)
        n = snprintf(out, cap, "%s/ssh-profiles.json", base);
    else {
        base = getenv("HOME");
        if (!base || !*base)
            return false;
        n = snprintf(out, cap, "%s/.config/prisme/ssh-profiles.json", base);
    }
    return n > 0 && (size_t)n < cap;
}
#ifndef _WIN32
static bool parents(const char *path) {
    char parent[SSH_PATH];
    size_t n = strlen(path);
    if (n >= sizeof(parent))
        return false;
    memcpy(parent, path, n + 1);
    for (char *p = parent + 1; *p; p++)
        if (*p == '/') {
            *p = 0;
            if (mkdir(parent, 0700) && errno != EEXIST)
                return false;
            *p = '/';
        }
    return true;
}
static int lock_profiles(const char *path, char *e, size_t cap) {
    char name[SSH_PATH];
    if (snprintf(name, sizeof(name), "%s.lock", path) >= (int)sizeof(name) || !parents(path)) {
        fail(e, cap, "Dossier de profils inaccessible.");
        return -1;
    }
    int fd = open(name, O_CREAT | O_RDWR | O_CLOEXEC | O_NOFOLLOW, 0600);
    struct stat st;
    if (fd < 0 || fstat(fd, &st) || !S_ISREG(st.st_mode) || st.st_uid != getuid() || fchmod(fd, 0600) ||
        flock(fd, LOCK_EX | LOCK_NB)) {
        if (fd >= 0)
            close(fd);
        fail(e, cap, "Profils SSH verrouillés ou non privés. Réessayez après l’autre éditeur.");
        return -1;
    }
    return fd;
}
static bool random_id(char out[33]) {
    unsigned char bytes[16];
    int fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return false;
    size_t n = 0;
    while (n < sizeof(bytes)) {
        ssize_t got = read(fd, bytes + n, sizeof(bytes) - n);
        if (got <= 0) {
            close(fd);
            return false;
        }
        n += (size_t)got;
    }
    close(fd);
    for (int i = 0; i < 16; i++)
        snprintf(out + i * 2, 3, "%02x", bytes[i]);
    return true;
}
static bool read_file(const char *path, char **data, size_t *len, bool *missing, char *e, size_t cap) {
    *data = NULL;
    *missing = false;
    int fd = open(path, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) {
        if (errno == ENOENT) {
            *missing = true;
            return true;
        }
        return fail(e, cap, "Lecture des profils SSH impossible.");
    }
    struct stat st;
    if (fstat(fd, &st) || !S_ISREG(st.st_mode) || st.st_uid != getuid() || st.st_size > SSH_JSON_MAX) {
        close(fd);
        return fail(e, cap, "Fichier de profils SSH invalide ou trop volumineux.");
    }
    char *raw = malloc(SSH_JSON_MAX + 1);
    size_t n = 0;
    bool ok = raw != NULL;
    while (ok && n < SSH_JSON_MAX) {
        ssize_t got = read(fd, raw + n, SSH_JSON_MAX - n);
        if (got < 0 && errno == EINTR)
            continue;
        if (got < 0) {
            ok = false;
            break;
        }
        if (!got)
            break;
        n += (size_t)got;
    }
    char extra;
    if (n == SSH_JSON_MAX && read(fd, &extra, 1) != 0)
        ok = false;
    close(fd);
    if (!ok) {
        free(raw);
        return fail(e, cap, "Lecture des profils SSH interrompue.");
    }
    raw[n] = 0;
    *data = raw;
    *len = n;
    return true;
}
static bool field(const SjDoc *d, int object, const char *key, char *out, size_t cap) {
    int token = sj_get(d, object, key);
    return token == -1 || sj_string(d, token, out, cap);
}
static bool profile_decode(const SjDoc *d, int object, SshProfile *p, bool with_id, char *e, size_t cap) {
    ssh_profile_init(p);
    return (sj_is(d, object, SJ_OBJECT) &&
            (!with_id || (field(d, object, "id", p->id, sizeof(p->id)) && id_valid(p->id))) &&
            field(d, object, "host", p->host, sizeof(p->host)) &&
            field(d, object, "user", p->user, sizeof(p->user)) &&
            field(d, object, "port", p->port, sizeof(p->port)) &&
            field(d, object, "folder", p->folder, sizeof(p->folder)) &&
            field(d, object, "identity", p->identity, sizeof(p->identity)) &&
            ssh_profile_valid(p, false, e, cap)) ||
           fail(e, cap, "Paramètre/identifiant de profil SSH invalide.");
}
static bool write_profiles(const char *path, const SshProfiles *s, char *e, size_t cap) {
    char *raw = malloc(SSH_JSON_MAX + 1);
    if (!raw)
        return fail(e, cap, "Mémoire insuffisante.");
    SjOut b;
    sj_out(&b, raw, SSH_JSON_MAX + 1);
    sj_add(&b, "{\"schema\":1,\"last\":");
    sj_quote(&b, s->last);
    sj_add(&b, ",\"profiles\":[");
    for (int i = 0; i < s->count; i++) {
        const SshProfile *p = &s->items[i];
        if (i)
            sj_add(&b, ",");
        sj_add(&b, "{\"id\":");
        sj_quote(&b, p->id);
        sj_add(&b, ",\"host\":");
        sj_quote(&b, p->host);
        sj_add(&b, ",\"user\":");
        sj_quote(&b, p->user);
        sj_add(&b, ",\"port\":");
        sj_quote(&b, p->port);
        sj_add(&b, ",\"folder\":");
        sj_quote(&b, p->folder);
        sj_add(&b, ",\"identity\":");
        sj_quote(&b, p->identity);
        sj_add(&b, "}");
    }
    sj_add(&b, "]}\n");
    char temporary[SSH_PATH];
    struct stat st;
    if (!b.ok || (lstat(path, &st) == 0 && (!S_ISREG(st.st_mode) || st.st_uid != getuid())) ||
        snprintf(temporary, sizeof(temporary), "%s.tmp-XXXXXX", path) >= (int)sizeof(temporary)) {
        free(raw);
        return fail(e, cap, "Destination des profils SSH invalide.");
    }
    int fd = mkstemp(temporary);
    bool ok = fd >= 0;
    size_t sent = 0;
    while (ok && sent < b.len) {
        ssize_t n = write(fd, raw + sent, b.len - sent);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0)
            ok = false;
        else
            sent += (size_t)n;
    }
    if (ok && (fchmod(fd, 0600) || fsync(fd)))
        ok = false;
    if (fd >= 0 && close(fd))
        ok = false;
    if (ok && rename(temporary, path))
        ok = false;
    if (!ok && fd >= 0)
        unlink(temporary);
    if (ok) {
        char parent[SSH_PATH];
        snprintf(parent, sizeof(parent), "%s", path);
        char *slash = strrchr(parent, '/');
        if (slash) {
            if (slash == parent)
                slash[1] = 0;
            else
                *slash = 0;
        } else
            strcpy(parent, ".");
        int dir = open(parent, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (dir < 0 || fsync(dir))
            ok = false;
        if (dir >= 0)
            close(dir);
    }
    free(raw);
    return ok || fail(e, cap, "Enregistrement des profils SSH impossible.");
}
static bool read_profiles(const char *path, SshProfiles *out, char *e, size_t cap) {
    memset(out, 0, sizeof(*out));
    char *raw = NULL;
    size_t len;
    bool missing;
    if (!read_file(path, &raw, &len, &missing, e, cap))
        return false;
    if (missing) {
        char old[SSH_PATH];
        snprintf(old, sizeof(old), "%s", path);
        char *slash = strrchr(old, '/');
        if (!slash || (size_t)(slash - old) + 10 >= sizeof(old))
            return true;
        strcpy(slash + 1, "ssh.json");
        if (!read_file(old, &raw, &len, &missing, e, cap))
            return false;
        if (missing)
            return true;
        SjToken tokens[64];
        SjDoc d;
        bool ok = len <= 16384 && sj_parse(&d, raw, len, tokens, 64) &&
                  profile_decode(&d, 0, &out->items[0], false, e, cap);
        free(raw);
        if (!ok) {
            memset(out, 0, sizeof(*out));
            return true;
        }
        if (!random_id(out->items[0].id))
            return fail(e, cap, "Aléa système indisponible.");
        out->count = 1;
        strcpy(out->last, out->items[0].id);
        return write_profiles(path, out, e, cap);
    }
    SjToken tokens[2048];
    SjDoc d;
    bool ok = sj_parse(&d, raw, len, tokens, 2048) && sj_is(&d, 0, SJ_OBJECT);
    int schema = ok ? sj_get(&d, 0, "schema") : -1, list = ok ? sj_get(&d, 0, "profiles") : -1;
    ok = ok && sj_is(&d, schema, SJ_NUMBER) && d.tokens[schema].len == 1 &&
         raw[d.tokens[schema].start] == '1' && sj_is(&d, list, SJ_ARRAY) &&
         d.tokens[list].count <= SSH_PROFILES_MAX && field(&d, 0, "last", out->last, sizeof(out->last));
    if (ok)
        for (int p = list + 1; p < d.tokens[list].next; p = d.tokens[p].next) {
            SshProfile *item = &out->items[out->count];
            if (!profile_decode(&d, p, item, true, e, cap)) {
                ok = false;
                break;
            }
            for (int j = 0; j < out->count; j++)
                if (!strcmp(item->id, out->items[j].id))
                    ok = false;
            if (!ok)
                break;
            out->count++;
        }
    if (ok && *out->last) {
        bool found = false;
        for (int i = 0; i < out->count; i++)
            if (!strcmp(out->last, out->items[i].id))
                found = true;
        ok = found;
    }
    free(raw);
    return ok || fail(e, cap, "Format/liste de profils SSH invalide. Fichier conservé sans écrasement.");
}
static bool normalize_identity(SshProfile *p, char *e, size_t cap) {
    if (!*p->identity)
        return true;
    char path[SSH_PATH];
    const char *key = p->identity;
    if (key[0] == '~' && key[1] == '/') {
        const char *home = getenv("HOME");
        if (!home || snprintf(path, sizeof(path), "%s/%s", home, key + 2) >= (int)sizeof(path))
            return fail(e, cap, "Chemin de clé trop long.");
    } else if (key[0] != '/') {
        char cwd[SSH_PATH];
        if (!getcwd(cwd, sizeof(cwd)) || snprintf(path, sizeof(path), "%s/%s", cwd, key) >= (int)sizeof(path))
            return fail(e, cap, "Chemin de clé trop long.");
    } else
        return true;
    strcpy(p->identity, path);
    return true;
}
bool ssh_profiles_load(const char *path, SshProfiles *out, char *e, size_t cap) {
    int lock = lock_profiles(path, e, cap);
    if (lock < 0)
        return false;
    SshProfiles *temporary = calloc(1, sizeof(*temporary));
    bool ok = temporary && read_profiles(path, temporary, e, cap);
    if (ok)
        *out = *temporary;
    free(temporary);
    close(lock);
    return ok;
}
static bool change_profiles(const char *path, SshProfile *profile, const char *id, int mode, char *e,
                            size_t cap) {
    if (profile && (!ssh_profile_valid(profile, false, e, cap) || !normalize_identity(profile, e, cap) ||
                    !ssh_profile_valid(profile, false, e, cap)))
        return false;
    int lock = lock_profiles(path, e, cap);
    if (lock < 0)
        return false;
    SshProfiles *s = calloc(1, sizeof(*s));
    bool ok = s && read_profiles(path, s, e, cap);
    int index = -1;
    if (ok) {
        for (int i = 0; i < s->count; i++)
            if (!strcmp(s->items[i].id, id))
                index = i;
        if (mode == 0) {
            if (index < 0)
                for (int i = 0; i < s->count; i++)
                    if (ssh_profile_equal(&s->items[i], profile)) {
                        index = i;
                        break;
                    }
            if (index < 0) {
                if (s->count >= SSH_PROFILES_MAX)
                    ok = fail(e, cap, "Limite : 32 profils SSH. Retirez un profil inutilisé.");
                else {
                    index = s->count++;
                    ok = random_id(s->items[index].id);
                }
            }
            if (ok) {
                char key[33];
                strcpy(key, s->items[index].id);
                s->items[index] = *profile;
                strcpy(s->items[index].id, key);
                strcpy(s->last, key);
                *profile = s->items[index];
            }
        } else if (index < 0)
            ok = fail(e, cap, "Ce profil a été retiré. Rechargez la liste.");
        else if (mode == 1)
            strcpy(s->last, id);
        else {
            bool selected = !strcmp(s->last, id);
            memmove(&s->items[index], &s->items[index + 1],
                    (size_t)(s->count - index - 1) * sizeof(s->items[0]));
            s->count--;
            if (selected)
                strcpy(s->last, s->count ? s->items[s->count - 1].id : "");
        }
        if (ok)
            ok = write_profiles(path, s, e, cap);
    }
    free(s);
    close(lock);
    return ok;
}
bool ssh_profiles_put(const char *path, SshProfile *p, char *e, size_t cap) {
    return change_profiles(path, p, p->id, 0, e, cap);
}
bool ssh_profiles_select(const char *path, const char *id, char *e, size_t cap) {
    return change_profiles(path, NULL, id, 1, e, cap);
}
bool ssh_profiles_delete(const char *path, const char *id, char *e, size_t cap) {
    return change_profiles(path, NULL, id, 2, e, cap);
}
#else
bool ssh_profiles_load(const char *p, SshProfiles *s, char *e, size_t n) {
    (void)p;
    (void)s;
    return fail(e, n, "Profils SSH : Linux requis.");
}
bool ssh_profiles_put(const char *p, SshProfile *s, char *e, size_t n) {
    (void)p;
    (void)s;
    return fail(e, n, "SSHFS : Linux requis.");
}
bool ssh_profiles_select(const char *p, const char *id, char *e, size_t n) {
    (void)p;
    (void)id;
    return fail(e, n, "SSHFS : Linux requis.");
}
bool ssh_profiles_delete(const char *p, const char *id, char *e, size_t n) {
    return ssh_profiles_select(p, id, e, n);
}
#endif
