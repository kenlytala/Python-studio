#define _GNU_SOURCE
#include "ssh.h"
#include "ssh_json.h"
#include "buffer.h"
#include "files.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(__linux__)
#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/file.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
extern char **environ;
#endif
static bool fail(char *e, size_t cap, const char *message) {
    if (cap)
        snprintf(e, cap, "%s", message);
    return false;
}

#if defined(__linux__)
static double clock_now(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec / 1e9;
}
static bool copy(char *out, size_t cap, const char *s) {
    size_t n = strlen(s);
    if (n >= cap)
        return false;
    memmove(out, s, n + 1);
    return true;
}
/* Lexical normalisation, no stat of a possibly disconnected mount. */
static bool normalized(const char *path, char out[SSH_PATH]) {
    if (!path || !*path)
        return false;
    char raw[SSH_PATH];
    if (*path == '/') {
        if (!copy(raw, sizeof(raw), path))
            return false;
    } else {
        char cwd[SSH_PATH];
        if (!getcwd(cwd, sizeof(cwd)) || snprintf(raw, sizeof(raw), "%s/%s", cwd, path) >= (int)sizeof(raw))
            return false;
    }
    size_t positions[SSH_PATH / 2], used = 1;
    int count = 0;
    out[0] = '/';
    for (const char *p = raw; *p;) {
        while (*p == '/')
            p++;
        const char *start = p;
        while (*p && *p != '/')
            p++;
        size_t n = (size_t)(p - start);
        if (!n)
            break;
        if (n == 1 && *start == '.')
            continue;
        if (n == 2 && start[0] == '.' && start[1] == '.') {
            if (count)
                used = positions[--count];
            continue;
        }
        if (count >= SSH_PATH / 2 || used + n + 2 >= SSH_PATH)
            return false;
        positions[count++] = used;
        if (used > 1)
            out[used++] = '/';
        memcpy(out + used, start, n);
        used += n;
    }
    out[used] = 0;
    return true;
}
long ssh_mount_id(const char *path, bool sshfs_only) {
    char target[SSH_PATH];
    if (!normalized(path, target))
        return -1;
    FILE *f = fopen("/proc/self/mountinfo", "r");
    if (!f)
        return -1;
    char *line = NULL;
    size_t capacity = 0;
    long result = 0;
    int matches = 0;
    while (getline(&line, &capacity, f) >= 0) {
        char *p = line, *end;
        errno = 0;
        long id = strtol(p, &end, 10);
        if (end == p || errno || id <= 0)
            continue;
        p = end;
        for (int column = 1; column < 4; column++) {
            while (*p == ' ')
                p++;
            while (*p && *p != ' ')
                p++;
        }
        while (*p == ' ')
            p++;
        char mount[SSH_PATH];
        size_t n = 0;
        while (*p && *p != ' ' && n + 1 < sizeof(mount)) {
            if (*p == '\\' && p[1] >= '0' && p[1] <= '7' && p[2] >= '0' && p[2] <= '7' && p[3] >= '0' &&
                p[3] <= '7') {
                mount[n++] = (char)((p[1] - '0') * 64 + (p[2] - '0') * 8 + p[3] - '0');
                p += 4;
            } else
                mount[n++] = *p++;
        }
        mount[n] = 0;
        if (!strcmp(mount, target)) {
            matches++;
            bool type = strstr(p, " - fuse.sshfs ") != NULL;
            result = !sshfs_only || type ? id : -2;
        }
    }
    bool ok = !ferror(f);
    free(line);
    fclose(f);
    return !ok ? -1 : matches > 1 ? -2 : result;
}
bool ssh_owns(const SshSession *s, const char *path) {
    if (!s || !s->initialized || !*s->cache)
        return false;
    char full[SSH_PATH];
    return normalized(path, full) && fs_under(s->cache, full);
}
bool ssh_mounted(SshSession *s) {
    long generation = atomic_load(&s->generation);
    return generation > 0 && *s->mountpoint && ssh_mount_id(s->mountpoint, true) == generation;
}
/* Resolve only LOCAL symlinks until reaching a reserved root. Once inside a
   managed root, never resolve a server-supplied link or stat an offline path. */
static int managed_alias(SshSession *s, const char *path, bool unlink_leaf, char out[SSH_PATH]) {
    char current[SSH_PATH];
    if (!normalized(path, current))
        return -1;
    for (int links = 0; links < 40; links++) {
        if (ssh_owns(s, current)) {
            copy(out, SSH_PATH, current);
            return 1;
        }
        bool restart = false;
        size_t len = strlen(current);
        for (size_t i = 1; i <= len; i++)
            if (!current[i] || current[i] == '/') {
                if (unlink_leaf && i == len)
                    return 0;
                char saved = current[i];
                current[i] = 0;
                struct stat st;
                int result = lstat(current, &st);
                if (!result && S_ISLNK(st.st_mode)) {
                    char target[SSH_PATH], combined[SSH_PATH];
                    ssize_t n = readlink(current, target, sizeof(target) - 1);
                    if (n <= 0 || (size_t)n >= sizeof(target) - 1)
                        return -1;
                    target[n] = 0;
                    current[i] = saved;
                    int used;
                    if (*target == '/')
                        used = snprintf(combined, sizeof(combined), "%s%s", target, current + i);
                    else {
                        size_t parent = i;
                        while (parent > 0 && current[parent - 1] != '/')
                            parent--;
                        used = snprintf(combined, sizeof(combined), "%.*s%s%s", (int)parent, current, target,
                                        current + i);
                    }
                    if (used < 0 || (size_t)used >= sizeof(combined) || !normalized(combined, current))
                        return -1;
                    restart = true;
                    break;
                }
                current[i] = saved;
                if (result)
                    return 0;
            }
        if (!restart)
            return 0;
    }
    return -1;
}
bool ssh_guard(void *context, const char *path, bool allow_leaf_link, char *error, size_t cap) {
    SshSession *s = context;
    if (!s || !s->initialized)
        return true;
    char full[SSH_PATH];
    if (!normalized(path, full))
        return fail(error, cap, "Chemin invalide ou trop long.");
    if (!ssh_owns(s, full)) {
        int alias = managed_alias(s, full, allow_leaf_link, full);
        if (alias < 0)
            return fail(error, cap, "Lien local circulaire ou chemin trop long.");
        if (!alias)
            return true;
    }
    if (!*s->mountpoint || !fs_under(s->mountpoint, full) || atomic_load(&s->state) != SSH_MOUNTED ||
        !atomic_load(&s->process_alive) || !ssh_mounted(s))
        return fail(error, cap,
                    "Montage SSH absent ou remplacé : opération bloquée. Brouillon conservé ; reconnectez ou "
                    "enregistrez sous un nom local.");
    size_t root = strlen(s->mountpoint), len = strlen(full);
    for (size_t i = root + 1; i <= len; i++)
        if (!full[i] || full[i] == '/') {
            char saved = full[i];
            full[i] = 0;
            struct stat st;
            bool link = !lstat(full, &st) && S_ISLNK(st.st_mode);
            full[i] = saved;
            if (link && !(allow_leaf_link && i == len))
                return fail(error, cap,
                            "Lien symbolique SSH non parcouru. Ouvrez le chemin réel sur le serveur ; seule "
                            "la suppression du lien est autorisée.");
        }
    return true;
}
bool ssh_save_guard(void *s, const char *path, char *error, size_t cap) {
    return ssh_guard(s, path, false, error, cap);
}
bool ssh_is_dir(SshSession *s, const char *path) {
    char error[512];
    struct stat st;
    return ssh_guard(s, path, false, error, sizeof(error)) && !stat(path, &st) && S_ISDIR(st.st_mode) &&
           ssh_guard(s, path, false, error, sizeof(error));
}

static bool executable(const char *name, char out[SSH_PATH]) {
    if (strchr(name, '/'))
        return normalized(name, out) && access(out, X_OK) == 0;
    const char *path = getenv("PATH");
    if (!path)
        path = "/usr/bin:/bin";
    for (const char *p = path; *p;) {
        const char *end = strchr(p, ':');
        if (!end)
            end = p + strlen(p);
        if (end > p && (size_t)(end - p) + strlen(name) + 2 < SSH_PATH) {
            char dir[SSH_PATH], candidate[SSH_PATH];
            size_t n = (size_t)(end - p);
            memcpy(dir, p, n);
            dir[n] = 0;
            size_t size = strlen(name);
            memcpy(candidate, dir, n);
            candidate[n] = '/';
            memcpy(candidate + n + 1, name, size + 1);
            if (normalized(candidate, out) && !access(out, X_OK)) {
                struct stat st;
                if (!stat(out, &st) && S_ISREG(st.st_mode))
                    return true;
            }
        }
        p = *end ? end + 1 : end;
    }
    return false;
}
static void close_fd(int *fd) {
    if (*fd >= 0)
        close(*fd);
    *fd = -1;
}
static void reap(long *pid, int *status) {
    if (*pid <= 0)
        return;
    int code;
    pid_t got = waitpid((pid_t)*pid, &code, WNOHANG);
    if (got == *pid || (got < 0 && errno == ECHILD)) {
        *pid = 0;
        if (status)
            *status = got > 0 && WIFEXITED(code) ? WEXITSTATUS(code) : 255;
    }
}
static void child_close_fds(unsigned max) {
#ifdef SYS_close_range
    if (!syscall(SYS_close_range, 3U, ~0U, 0U))
        return;
#endif
    for (unsigned fd = 3; fd < max; fd++)
        close((int)fd);
}
/* All memory/env preparation is done by the parent, before fork. No shell. */
static long spawn(char *const argv[], char *const env[], int log_fd) {
    struct rlimit limit;
    unsigned max = 65536;
    if (!getrlimit(RLIMIT_NOFILE, &limit) && limit.rlim_cur < 1048576)
        max = (unsigned)limit.rlim_cur;
    int nullfd = open("/dev/null", O_RDWR | O_CLOEXEC);
    if (nullfd < 0)
        return -1;
    pid_t pid = fork();
    if (pid == 0) {
        if (setsid() < 0 || chdir("/") || dup2(nullfd, STDIN_FILENO) < 0 || dup2(nullfd, STDOUT_FILENO) < 0 ||
            dup2(log_fd >= 0 ? log_fd : nullfd, STDERR_FILENO) < 0)
            _exit(126);
        child_close_fds(max);
        execve(argv[0], argv, env ? env : environ);
        _exit(127);
    }
    close(nullfd);
    return (long)pid;
}
static bool private_cache(SshSession *s, char *e, size_t cap) {
    char path[SSH_PATH];
    if (!copy(path, sizeof(path), s->cache))
        return false;
    for (char *p = path + 1; *p; p++)
        if (*p == '/') {
            *p = 0;
            if (mkdir(path, 0700) && errno != EEXIST)
                return fail(e, cap, "Cache SSH inaccessible.");
            *p = '/';
        }
    if (mkdir(path, 0700) && errno != EEXIST)
        return fail(e, cap, "Cache SSH inaccessible.");
    struct stat st;
    if (lstat(path, &st) || !S_ISDIR(st.st_mode) || st.st_uid != getuid() || chmod(path, 0700))
        return fail(e, cap, "Cache SSH non privé ou symbolique.");
    char actual[SSH_PATH];
    if (!realpath(path, actual))
        return fail(e, cap, "Cache SSH inaccessible.");
    return copy(s->cache, sizeof(s->cache), actual);
}
static bool empty_mountpoint(SshSession *s, const SshProfile *p, char *e, size_t cap) {
    if (strlen(p->id) != 32 || strspn(p->id, "0123456789abcdef") != 32)
        return fail(e, cap, "Enregistrez le profil avant la connexion.");
    char lock[SSH_PATH];
    if (snprintf(lock, sizeof(lock), "%s/%s.lock", s->cache, p->id) >= (int)sizeof(lock) ||
        snprintf(s->mountpoint, sizeof(s->mountpoint), "%s/%s", s->cache, p->id) >=
            (int)sizeof(s->mountpoint))
        return fail(e, cap, "Point de montage trop long.");
    s->lock_fd = open(lock, O_RDWR | O_CREAT | O_NOFOLLOW | O_CLOEXEC, 0600);
    struct stat st;
    if (s->lock_fd < 0 || fstat(s->lock_fd, &st) || !S_ISREG(st.st_mode) || st.st_uid != getuid() ||
        fchmod(s->lock_fd, 0600) || flock(s->lock_fd, LOCK_EX | LOCK_NB))
        return fail(e, cap, "Connexion déjà utilisée par une autre instance.");
    if (ssh_mount_id(s->mountpoint, false) != 0)
        return fail(e, cap, "Montage déjà actif/ambigu : démontage explicite requis avant de réessayer.");
    if (mkdir(s->mountpoint, 0700) && errno != EEXIST)
        return fail(e, cap, "Création du point de montage impossible.");
    if (lstat(s->mountpoint, &st) || !S_ISDIR(st.st_mode) || st.st_uid != getuid() ||
        chmod(s->mountpoint, 0700))
        return fail(e, cap, "Point de montage symbolique ou non privé refusé.");
    DIR *dir = opendir(s->mountpoint);
    if (!dir)
        return fail(e, cap, "Point de montage inaccessible.");
    bool empty = true;
    errno = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)))
        if (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")) {
            empty = false;
            break;
        }
    if (errno)
        empty = false;
    closedir(dir);
    return empty || fail(e, cap, "Point de montage non vide : fichiers locaux protégés, connexion refusée.");
}
static bool broker(SshSession *s, char *e, size_t cap) {
    strcpy(s->runtime, "/tmp/prisme-c-ssh-XXXXXX");
    if (!mkdtemp(s->runtime)) {
        *s->runtime = 0;
        return fail(e, cap, "Création du canal d’authentification impossible.");
    }
    snprintf(s->socket_path, sizeof(s->socket_path), "%s/askpass.sock", s->runtime);
    unsigned char token[32];
    int fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
    size_t n = 0;
    while (fd >= 0 && n < sizeof(token)) {
        ssize_t got = read(fd, token + n, sizeof(token) - n);
        if (got <= 0)
            break;
        n += (size_t)got;
    }
    if (fd >= 0)
        close(fd);
    if (n != sizeof(token))
        return fail(e, cap, "Aléa système indisponible.");
    for (int i = 0; i < 32; i++)
        snprintf(s->token + i * 2, 3, "%02x", token[i]);
    ssh_erase(token, sizeof(token));
    s->listener = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (s->listener < 0)
        return fail(e, cap, "Canal d’authentification indisponible.");
    struct sockaddr_un address = {0};
    address.sun_family = AF_UNIX;
    if (strlen(s->socket_path) >= sizeof(address.sun_path))
        return false;
    strcpy(address.sun_path, s->socket_path);
    if (bind(s->listener, (struct sockaddr *)&address, sizeof(address)) || chmod(s->socket_path, 0600) ||
        listen(s->listener, 2))
        return fail(e, cap, "Canal d’authentification inaccessible.");
    return true;
}
static char **environment(SshSession *s) {
    size_t n = 0;
    while (environ[n])
        n++;
    char **env = calloc(n + 7, sizeof(*env));
    if (!env)
        return NULL;
    size_t count = 0;
    const char *names[] = {"SSH_ASKPASS=", "SSH_ASKPASS_REQUIRE=", "PRISME_ASKPASS_SOCKET=",
                           "PRISME_ASKPASS_TOKEN=", "DISPLAY="};
    for (size_t i = 0; i < n; i++) {
        bool skip = false;
        for (int j = 0; j < 5; j++)
            if (!strncmp(environ[i], names[j], strlen(names[j])))
                skip = true;
        if (!skip)
            env[count++] = environ[i];
    }
    const char *values[] = {s->helper, "force", s->socket_path, s->token,
                            getenv("DISPLAY") ? getenv("DISPLAY") : ":0"};
    for (int i = 0; i < 5; i++) {
        size_t size = strlen(names[i]) + strlen(values[i]) + 1;
        env[count] = malloc(size);
        if (!env[count]) {
            for (int j = 0; j < i; j++)
                free(env[count - (size_t)j - 1]);
            free(env);
            return NULL;
        }
        snprintf(env[count++], size, "%s%s", names[i], values[i]);
    }
    return env;
}
static void free_environment(char **env) {
    if (!env)
        return;
    size_t n = 0;
    while (env[n])
        n++;
    for (size_t i = n - 5; i < n; i++)
        free(env[i]);
    free(env);
}
static void read_log(SshSession *s) {
    if (s->log_fd < 0)
        return;
    for (int i = 0; i < 8; i++) {
        char data[4096];
        ssize_t n = read(s->log_fd, data, sizeof(data));
        if (n < 0 && errno == EINTR)
            continue;
        if (n < 0)
            break;
        if (!n) {
            close_fd(&s->log_fd);
            break;
        }
        for (ssize_t j = 0; j < n; j++)
            if ((unsigned char)data[j] < 32 && data[j] != '\n' && data[j] != '\t')
                data[j] = ' ';
        size_t add = (size_t)n;
        if (s->log_len + add > 32768) {
            size_t remove = s->log_len + add - 32768;
            memmove(s->log, s->log + remove, s->log_len - remove);
            s->log_len -= remove;
        }
        memcpy(s->log + s->log_len, data, add);
        s->log_len += add;
        s->log[s->log_len] = 0;
    }
}
bool ssh_answer(SshSession *s, const char *answer, char *e, size_t cap) {
    if (!s->auth_pending || s->client < 0)
        return fail(e, cap, "Demande d’authentification expirée.");
    if (answer &&
        (strlen(answer) > SSH_AUTH_MAX || !utf8_valid(answer, strlen(answer)) || strpbrk(answer, "\r\n") ||
         (s->auth_confirm && strcmp(answer, "yes") && strcmp(answer, "no"))))
        return fail(e, cap, "Réponse d’authentification invalide.");
    char message[SSH_AUTH_MAX * 6 + 64];
    SjOut out;
    sj_out(&out, message, sizeof(message));
    sj_add(&out, "{\"answer\":");
    if (answer)
        sj_quote(&out, answer);
    else
        sj_add(&out, "null");
    sj_add(&out, "}\n");
    size_t sent = 0;
    bool ok = out.ok;
    while (ok && sent < out.len) {
        ssize_t n = send(s->client, out.text + sent, out.len - sent, MSG_NOSIGNAL);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0)
            ok = false;
        else
            sent += (size_t)n;
    }
    ssh_erase(message, sizeof(message));
    close_fd(&s->client);
    s->auth_pending = false;
    s->incoming_len = 0;
    ssh_erase(s->incoming, sizeof(s->incoming));
    s->prompt[0] = 0;
    return ok || fail(e, cap, "Canal d’authentification fermé.");
}
static void auth_poll(SshSession *s) {
    if (s->listener < 0 || s->state == SSH_DISCONNECTING)
        return;
    if (s->client < 0) {
        int fd = accept4(s->listener, NULL, NULL, SOCK_NONBLOCK | SOCK_CLOEXEC);
        if (fd < 0)
            return;
        struct ucred peer;
        socklen_t size = sizeof(peer);
        if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &peer, &size) || peer.uid != getuid()) {
            close(fd);
            return;
        }
        s->client = fd;
        s->incoming_len = 0;
        s->client_deadline = clock_now() + 2;
    }
    if (s->auth_pending) {
        if (clock_now() > s->client_deadline) {
            char e[128];
            (void)ssh_answer(s, NULL, e, sizeof(e));
        }
        return;
    }
    bool bad = false, complete = false;
    for (int i = 0; i < 8 && s->incoming_len < sizeof(s->incoming) - 1; i++) {
        ssize_t got =
            read(s->client, s->incoming + s->incoming_len, sizeof(s->incoming) - 1 - s->incoming_len);
        if (got < 0 && errno == EINTR)
            continue;
        if (got < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            break;
        if (got <= 0) {
            bad = true;
            break;
        }
        s->incoming_len += (size_t)got;
        char *end = memchr(s->incoming, '\n', s->incoming_len);
        if (end) {
            s->incoming_len = (size_t)(end - s->incoming);
            complete = true;
            break;
        }
    }
    if (complete) {
        SjDoc d;
        SjToken tokens[32];
        char token[65], prompt[SSH_PROMPT_MAX + 1];
        bool ok = sj_parse(&d, s->incoming, s->incoming_len, tokens, 32) &&
                  sj_string(&d, sj_get(&d, 0, "token"), token, sizeof(token)) && strlen(token) == 64 &&
                  sj_string(&d, sj_get(&d, 0, "prompt"), prompt, sizeof(prompt));
        unsigned diff = 0;
        if (ok)
            for (int i = 0; i < 64; i++)
                diff |= (unsigned char)token[i] ^ (unsigned char)s->token[i];
        int confirm = ok ? sj_get(&d, 0, "confirm") : -1;
        ok = ok && !diff && (sj_is(&d, confirm, SJ_TRUE) || sj_is(&d, confirm, SJ_FALSE));
        if (ok) {
            size_t n = 0;
            for (size_t i = 0; prompt[i]; i++)
                if ((unsigned char)prompt[i] >= 32 || prompt[i] == '\n' || prompt[i] == '\t')
                    s->prompt[n++] = prompt[i];
            s->prompt[n] = 0;
            s->auth_confirm = sj_is(&d, confirm, SJ_TRUE);
            s->auth_pending = true;
            s->client_deadline = clock_now() + 295;
            if (s->auth_confirm && !strstr(s->prompt, "SHA256:") && !strstr(s->log, "SHA256:")) {
                char e[128];
                (void)ssh_answer(s, "no", e, sizeof(e));
                s->cancelled = true;
                copy(s->error, sizeof(s->error), "Empreinte du serveur indisponible : connexion refusée.");
            }
        } else
            bad = true;
        ssh_erase(token, sizeof(token));
    }
    if (bad || (!s->auth_pending && clock_now() > s->client_deadline) ||
        s->incoming_len == sizeof(s->incoming) - 1) {
        close_fd(&s->client);
        s->incoming_len = 0;
    }
}
static void resources(SshSession *s) {
    if (s->auth_pending) {
        char e[64];
        (void)ssh_answer(s, NULL, e, sizeof(e));
    }
    close_fd(&s->client);
    close_fd(&s->listener);
    close_fd(&s->log_fd);
    if (*s->socket_path)
        unlink(s->socket_path);
    if (*s->runtime)
        rmdir(s->runtime);
    *s->runtime = *s->socket_path = 0;
    ssh_erase(s->token, sizeof(s->token));
    close_fd(&s->lock_fd);
    s->generation = 0;
}
static bool stop_safe(SshSession *s) {
    long id = *s->mountpoint ? ssh_mount_id(s->mountpoint, false) : 0;
    if (id != 0)
        return false;
    if (s->process > 0) {
        kill((pid_t)-s->process, SIGTERM);
        kill((pid_t)s->process, SIGTERM);
    }
    if (s->probe > 0) {
        kill((pid_t)-s->probe, SIGTERM);
        kill((pid_t)s->probe, SIGTERM);
    }
    s->stopping = true;
    s->stop_deadline = clock_now() + 2;
    return true;
}
static bool begin_disconnect(SshSession *s, char *e, size_t cap) {
    long id = *s->mountpoint ? ssh_mount_id(s->mountpoint, false) : 0;
    long generation = s->generation;
    if (id > 0 && s->lock_fd < 0)
        return fail(e, cap, "Montage non détenu par cette instance : démontage refusé.");
    if (id < 0 || (id > 0 && generation > 0 && id != generation))
        return fail(e, cap,
                    "Montage remplacé/empilé : aucun démontage ni arrêt du transport. Intervention manuelle "
                    "requise.");
    if (s->auth_pending) {
        char ignored[64];
        (void)ssh_answer(s, NULL, ignored, sizeof(ignored));
    }
    s->state = SSH_DISCONNECTING;
    if (!id)
        return stop_safe(s);
    if (!generation)
        s->generation = id;
    /* The asynchronous read-only probe can itself keep a FUSE directory busy.
       Stop/reap only OUR probe before attempting a normal unmount. Never kill
       SSHFS while any mount is still present. */
    if (s->probe > 0) {
        kill((pid_t)-s->probe, SIGTERM);
        kill((pid_t)s->probe, SIGTERM);
        s->probe_stopping = true;
        s->stop_deadline = clock_now() + 2;
        return true;
    }
    if (s->unmount_process > 0)
        return true;
    char *argv[] = {s->unmount_program, "-u", s->mountpoint, NULL};
    s->unmount_process = spawn(argv, NULL, -1);
    if (s->unmount_process <= 0) {
        s->unmount_process = 0;
        s->state = SSH_LOST;
        return fail(e, cap, "Impossible de démarrer le démontage normal.");
    }
    s->stop_deadline = clock_now() + 6;
    s->stopping = false;
    return true;
}
void ssh_init(SshSession *s, const char *cache, const char *helper) {
    memset(s, 0, sizeof(*s));
    s->listener = s->client = s->log_fd = s->lock_fd = -1;
    atomic_init(&s->state, SSH_IDLE);
    atomic_init(&s->generation, 0);
    atomic_init(&s->process_alive, false);
    s->initialized = true;
    ssh_profile_init(&s->profile);
    char directory[SSH_PATH];
    if (!cache) {
        const char *base = getenv("XDG_CACHE_HOME");
        if (base && *base)
            snprintf(directory, sizeof(directory), "%s/prisme/sshfs-c", base);
        else {
            base = getenv("HOME");
            snprintf(directory, sizeof(directory), "%s/.cache/prisme/sshfs-c", base ? base : ".");
        }
        cache = directory;
    }
    if (!normalized(cache, s->cache) || !helper || !normalized(helper, s->helper)) {
        s->state = SSH_ERROR;
        copy(s->error, sizeof(s->error), "Chemin du service SSH invalide.");
    }
}
bool ssh_connect(SshSession *s, const SshProfile *p, char *e, size_t cap) {
    if (!s->initialized || (s->state != SSH_IDLE && s->state != SSH_ERROR) || s->process > 0 ||
        s->probe > 0 || s->unmount_process > 0 || s->lock_fd >= 0)
        return fail(e, cap, "Déconnectez la session SSH actuelle avant une nouvelle connexion.");
    if (!ssh_profile_valid(p, true, e, cap))
        return false;
    char sshfs[SSH_PATH], ssh[SSH_PATH];
    if (!executable("sshfs", sshfs) || !executable("ssh", ssh) ||
        (!executable("fusermount3", s->unmount_program) && !executable("fusermount", s->unmount_program)))
        return fail(e, cap, "Installer SSHFS/OpenSSH : sudo apt install sshfs openssh-client");
    struct stat helper;
    if (!*s->helper || stat(s->helper, &helper) || !S_ISREG(helper.st_mode) || access(s->helper, X_OK))
        return fail(
            e, cap,
            "Service python-studio-ssh-helper introuvable à côté de l’exécutable. Reconstruisez l’édition C "
            "complète.");
    if (!private_cache(s, e, cap))
        return false;
    if (!empty_mountpoint(s, p, e, cap)) {
        close_fd(&s->lock_fd);
        return false;
    }
    s->profile = *p;
    s->log_len = 0;
    *s->log = *s->error = 0;
    s->cancelled = s->failing = s->stopping = s->probe_stopping = false;
    s->generation = 0;
    s->state = SSH_CONNECTING;
    s->deadline = clock_now() + 600;
    SshCommand command;
    int pipefd[2] = {-1, -1};
    char **env = NULL;
    bool ok = broker(s, e, cap) &&
              ssh_command(&command, p, s->mountpoint, sshfs, ssh, s->config_override, e, cap) &&
              pipe2(pipefd, O_CLOEXEC) == 0;
    if (ok) {
        env = environment(s);
        ok = env != NULL;
    }
    if (ok) {
        s->process = spawn(command.argv, env, pipefd[1]);
        ok = s->process > 0;
    }
    free_environment(env);
    if (pipefd[1] >= 0)
        close(pipefd[1]);
    if (ok) {
        s->log_fd = pipefd[0];
        fcntl(s->log_fd, F_SETFL, fcntl(s->log_fd, F_GETFL) | O_NONBLOCK);
        s->process_alive = true;
        return true;
    }
    if (pipefd[0] >= 0)
        close(pipefd[0]);
    s->process = 0;
    s->process_alive = false;
    s->state = SSH_ERROR;
    if (!*e)
        fail(e, cap, "Démarrage SSH impossible.");
    copy(s->error, sizeof(s->error), e);
    resources(s);
    return false;
}
void ssh_cancel(SshSession *s) {
    if (s->initialized && s->state == SSH_CONNECTING)
        s->cancelled = true;
}
bool ssh_disconnect(SshSession *s, char *e, size_t cap) {
    if (!s->initialized || !*s->mountpoint)
        return fail(e, cap, "Aucun dossier SSH à déconnecter.");
    if (s->state == SSH_DISCONNECTING || s->unmount_process > 0)
        return fail(e, cap, "Une déconnexion est déjà en cours.");
    s->failing = false;
    return begin_disconnect(s, e, cap);
}
SshEvent ssh_poll(SshSession *s) {
    if (!s->initialized)
        return SSH_EVENT_NONE;
    read_log(s);
    int process_status = -1, probe_status = -1, unmount_status = -1;
    reap(&s->process, &process_status);
    reap(&s->probe, &probe_status);
    reap(&s->unmount_process, &unmount_status);
    s->process_alive = s->process > 0;
    auth_poll(s);
    if (s->state == SSH_CONNECTING) {
        long id = ssh_mount_id(s->mountpoint, true);
        if (id > 0 && !s->generation)
            s->generation = id;
        bool replaced = id < 0 || (id > 0 && s->generation > 0 && id != s->generation);
        if (replaced) {
            s->state = SSH_LOST;
            copy(s->error, sizeof(s->error), "Montage SSH inconnu, remplacé ou empilé : opération bloquée.");
            return SSH_EVENT_ERROR;
        }
        if (s->cancelled || !s->process_alive || clock_now() > s->deadline ||
            (probe_status >= 0 && probe_status != 0)) {
            if (!*s->error) {
                if (s->cancelled)
                    copy(s->error, sizeof(s->error), "Connexion annulée.");
                else if (clock_now() > s->deadline)
                    copy(s->error, sizeof(s->error), "Connexion SSH expirée après dix minutes.");
                else if (strstr(s->log, "HOST IDENTIFICATION HAS CHANGED"))
                    copy(s->error, sizeof(s->error),
                         "HOST IDENTIFICATION HAS CHANGED : clé du serveur modifiée. Connexion refusée ; "
                         "aucune clé écrasée. Vérifiez l’empreinte par un canal de confiance.");
                else
                    snprintf(s->error, sizeof(s->error), "Connexion SSH impossible. %.850s",
                             s->log_len > 850 ? s->log + s->log_len - 850 : s->log);
            }
            s->failing = true;
            char e[512] = "";
            if (!begin_disconnect(s, e, sizeof(e))) {
                s->state = SSH_LOST;
                copy(s->error, sizeof(s->error), e);
                return SSH_EVENT_ERROR;
            }
        } else if (probe_status == 0 && id == s->generation && s->process_alive) {
            s->state = SSH_MOUNTED;
            return SSH_EVENT_READY;
        } else if (id > 0 && !s->probe && probe_status < 0) {
            char *argv[] = {s->helper, "--probe", s->mountpoint, NULL};
            s->probe = spawn(argv, NULL, -1);
            if (s->probe <= 0) {
                s->probe = 0;
                s->cancelled = true;
                copy(s->error, sizeof(s->error), "Vérification du dossier SSH impossible.");
            }
        }
    }
    if (s->state == SSH_MOUNTED && (!s->process_alive || !ssh_mounted(s))) {
        s->state = SSH_LOST;
        copy(s->error, sizeof(s->error),
             "Connexion SSH perdue. Brouillons conservés ; reconnectez ou enregistrez sous un nom local.");
        return SSH_EVENT_LOST;
    }
    if (s->state == SSH_DISCONNECTING) {
        long id = ssh_mount_id(s->mountpoint, false);
        if (id < 0 || (id > 0 && s->generation > 0 && id != s->generation)) {
            s->state = SSH_LOST;
            copy(s->error, sizeof(s->error), "Montage remplacé/empilé : aucun arrêt ni démontage forcé.");
            return SSH_EVENT_ERROR;
        }
        if (s->probe_stopping) {
            if (s->probe > 0) {
                if (clock_now() > s->stop_deadline) {
                    kill((pid_t)-s->probe, SIGKILL);
                    kill((pid_t)s->probe, SIGKILL);
                }
                return SSH_EVENT_NONE;
            }
            s->probe_stopping = false;
            char e[512] = "";
            if (!begin_disconnect(s, e, sizeof(e))) {
                s->state = SSH_LOST;
                copy(s->error, sizeof(s->error), e);
                return SSH_EVENT_ERROR;
            }
        }
        if (s->unmount_process > 0 && clock_now() > s->stop_deadline) {
            kill((pid_t)s->unmount_process, SIGTERM);
            s->state = SSH_LOST;
            copy(
                s->error, sizeof(s->error),
                "Démontage non terminé. Aucun démontage forcé ; fermez les accès au dossier puis réessayez.");
            return SSH_EVENT_ERROR;
        }
        if (unmount_status >= 0 && id > 0) {
            s->state = s->process_alive && !s->failing && !s->cancelled ? SSH_MOUNTED : SSH_LOST;
            copy(s->error, sizeof(s->error),
                 "Démontage normal refusé (dossier occupé). Fermez les accès au dossier puis réessayez.");
            return SSH_EVENT_ERROR;
        }
        if (!id && !s->stopping)
            (void)stop_safe(s);
        if (s->stopping && clock_now() > s->stop_deadline && !id) {
            if (s->process > 0) {
                kill((pid_t)-s->process, SIGKILL);
                kill((pid_t)s->process, SIGKILL);
            }
            if (s->probe > 0) {
                kill((pid_t)-s->probe, SIGKILL);
                kill((pid_t)s->probe, SIGKILL);
            }
        }
        if (!id && !s->process && !s->probe && !s->unmount_process) {
            resources(s);
            if (!s->failing)
                *s->error = 0;
            s->state = s->failing ? SSH_ERROR : SSH_IDLE;
            s->process_alive = false;
            return s->failing ? SSH_EVENT_ERROR : SSH_EVENT_DISCONNECTED;
        }
    }
    return SSH_EVENT_NONE;
}
bool ssh_close(SshSession *s, char *e, size_t cap) {
    if (!s->initialized)
        return true;
    if (s->lock_fd < 0 && s->process <= 0 && s->probe <= 0 && s->unmount_process <= 0) {
        resources(s);
        s->initialized = false;
        return true;
    }
    if (s->state == SSH_CONNECTING)
        s->cancelled = true;
    else if (s->state != SSH_DISCONNECTING && (s->process > 0 || ssh_mount_id(s->mountpoint, false) > 0)) {
        if (!begin_disconnect(s, e, cap))
            return false;
    }
    double end = clock_now() + 7;
    while (clock_now() < end && (s->process > 0 || s->probe > 0 || s->unmount_process > 0 ||
                                 s->state == SSH_DISCONNECTING || s->state == SSH_CONNECTING)) {
        SshEvent event = ssh_poll(s);
        if (event == SSH_EVENT_ERROR && s->state == SSH_LOST)
            return fail(e, cap, s->error);
        struct timespec delay = {0, 20000000};
        nanosleep(&delay, NULL);
    }
    if (s->process > 0 || s->probe > 0 || s->unmount_process > 0 ||
        (*s->mountpoint && ssh_mount_id(s->mountpoint, false) != 0))
        return fail(e, cap, "Session SSH encore occupée : aucun démontage forcé.");
    resources(s);
    s->initialized = false;
    return true;
}
#else
long ssh_mount_id(const char *p, bool only) {
    (void)p;
    (void)only;
    return -1;
}
void ssh_init(SshSession *s, const char *cache, const char *helper) {
    (void)cache;
    (void)helper;
    memset(s, 0, sizeof(*s));
}
bool ssh_owns(const SshSession *s, const char *p) {
    (void)s;
    (void)p;
    return false;
}
bool ssh_mounted(SshSession *s) {
    (void)s;
    return false;
}
bool ssh_guard(void *s, const char *p, bool link, char *e, size_t n) {
    (void)s;
    (void)p;
    (void)link;
    (void)e;
    (void)n;
    return true;
}
bool ssh_save_guard(void *s, const char *p, char *e, size_t n) {
    return ssh_guard(s, p, false, e, n);
}
bool ssh_is_dir(SshSession *s, const char *p) {
    (void)s;
    return fs_kind(p) == FS_DIRECTORY;
}
bool ssh_connect(SshSession *s, const SshProfile *p, char *e, size_t n) {
    (void)s;
    (void)p;
    return fail(e, n, "SSHFS/OpenSSH : cette intégration cible Linux.");
}
SshEvent ssh_poll(SshSession *s) {
    (void)s;
    return SSH_EVENT_NONE;
}
void ssh_cancel(SshSession *s) {
    (void)s;
}
bool ssh_disconnect(SshSession *s, char *e, size_t n) {
    (void)s;
    return fail(e, n, "SSHFS : Linux requis.");
}
bool ssh_answer(SshSession *s, const char *a, char *e, size_t n) {
    (void)s;
    (void)a;
    return fail(e, n, "SSHFS : Linux requis.");
}
bool ssh_close(SshSession *s, char *e, size_t n) {
    (void)s;
    (void)e;
    (void)n;
    return true;
}
#endif
