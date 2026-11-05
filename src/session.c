#define _POSIX_C_SOURCE 200809L
#include "session.h"
#include "files.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <math.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#include <io.h>
#include <process.h>
#define close _close
#define unlink _unlink
#define getpid _getpid
#else
#include <dirent.h>
#include <unistd.h>
#include <sys/file.h>
#endif
#ifndef O_NOFOLLOW
#define O_NOFOLLOW 0
#endif
#ifndef O_NONBLOCK
#define O_NONBLOCK 0
#endif
#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif
static atomic_ulong journal_counter;
static const char magic[] = "PYTHON-STUDIO-C-SESSION\n";
static bool fail(char *e, size_t cap, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    if (e && cap)
        vsnprintf(e, cap, fmt, ap);
    va_end(ap);
    return false;
}
static void set(char *out, size_t cap, const char *text) {
    snprintf(out, cap, "%s", text ? text : "");
}
static uint64_t hash_bytes(uint64_t hash, const void *data, size_t len) {
    const unsigned char *p = data;
    for (size_t i = 0; i < len; i++) {
        hash ^= p[i];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}
static uint64_t project_key(const char *p) {
    return hash_bytes(UINT64_C(14695981039346656037), p, strlen(p));
}
static bool join(char *out, size_t cap, const char *a, const char *b) {
    size_t n = strlen(a), m = strlen(b);
    if (n + m + 2 > cap) {
        if (cap)
            *out = 0;
        return false;
    }
    memcpy(out, a, n);
    out[n] = '/';
    memcpy(out + n + 1, b, m + 1);
    return true;
}
static bool absolute(const char *p) {
    size_t n = strlen(p);
    if (strstr(p, "/../") || (n >= 3 && !strcmp(p + n - 3, "/..")))
        return false;
#ifdef _WIN32
    if (strstr(p, "\\..\\") || (n >= 3 && !strcmp(p + n - 3, "\\..")))
        return false;
    return !*p || (p[0] && p[1] == ':') || p[0] == '/' || p[0] == '\\';
#else
    return !*p || *p == '/';
#endif
}
static bool parents(const char *path) {
    if (!path || !*path)
        return false;
    char copy[BUF_PATH];
    set(copy, sizeof(copy), path);
    for (char *p = copy + 1;; p++)
        if (*p == '/' || !*p) {
            char c = *p;
            *p = 0;
#ifdef _WIN32
            int r = _mkdir(copy);
#else
            int r = mkdir(copy, 0700);
#endif
            if (r && errno != EEXIST)
                return false;
            *p = c;
            if (!c)
                break;
        }
#ifndef _WIN32
    struct stat st;
    if (lstat(path, &st) || !S_ISDIR(st.st_mode) || st.st_uid != getuid())
        return false;
    if (chmod(path, 0700))
        return false;
#endif
    return true;
}
void studio_default_directory(char *out, size_t cap) {
    const char *base = getenv("PRISME_CONFIG_DIR");
    if (base && *base) {
        snprintf(out, cap, "%s/sessions-c", base);
        return;
    }
#ifdef _WIN32
    base = getenv("APPDATA");
    snprintf(out, cap, "%s/prisme-c/sessions-c", base ? base : ".");
#else
    base = getenv("HOME");
    snprintf(out, cap, "%s/.config/prisme-c/sessions-c", base ? base : ".");
#endif
}
void studio_store_init(StudioStore *s, const char *directory, bool enabled) {
    memset(s, 0, sizeof(*s));
    s->lock_fd = -1;
    s->enabled = enabled;
    s->serial = (unsigned long)time(NULL) * 1000000ul + atomic_fetch_add(&journal_counter, 1000ul);
    if (directory)
        set(s->directory, sizeof(s->directory), directory);
    else
        studio_default_directory(s->directory, sizeof(s->directory));
}
void studio_store_close(StudioStore *s) {
    if (s->lock_fd >= 0)
        close(s->lock_fd);
    s->lock_fd = -1;
    s->project[0] = 0;
}
static bool bind(StudioStore *s, const char *project, char *e, size_t cap) {
    if (!s->enabled)
        return true;
    if (!strcmp(s->project, project))
        return true;
    studio_store_close(s);
    if (!*project || !absolute(project) || strlen(project) >= BUF_PATH || !parents(s->directory))
        return fail(e, cap, "Dossier de sessions inaccessible.");
    char name[128], lock[BUF_PATH];
    uint64_t key = project_key(project);
    snprintf(name, sizeof(name), "%016llx.state", (unsigned long long)key);
    if (!join(s->path, sizeof(s->path), s->directory, name))
        return fail(e, cap, "Chemin de session trop long.");
#ifndef _WIN32
    snprintf(name, sizeof(name), "%016llx.lock", (unsigned long long)key);
    if (!join(lock, sizeof(lock), s->directory, name))
        return fail(e, cap, "Chemin de verrou trop long.");
    int fd = open(lock, O_RDWR | O_CREAT | O_NOFOLLOW | O_CLOEXEC, 0600);
    struct stat st;
    if (fd < 0)
        return fail(e, cap, "Verrou de session : %s", strerror(errno));
    if (fstat(fd, &st) || !S_ISREG(st.st_mode) || st.st_uid != getuid() || st.st_nlink != 1 ||
        fchmod(fd, 0600)) {
        close(fd);
        return fail(e, cap, "Verrou de session non privé.");
    }
    if (flock(fd, LOCK_EX | LOCK_NB)) {
        int saved = errno;
        close(fd);
        if (saved != EWOULDBLOCK && saved != EAGAIN)
            return fail(e, cap, "Verrouillage de session impossible.");
        snprintf(name, sizeof(name), "%016llx-%ld-%lu.state", (unsigned long long)key, (long)getpid(),
                 ++s->serial);
        if (!join(s->path, sizeof(s->path), s->directory, name))
            return fail(e, cap, "Chemin de session trop long.");
    } else
        s->lock_fd = fd;
#else
    (void)lock;
    snprintf(name, sizeof(name), "%016llx-%ld-%lu.state", (unsigned long long)key, (long)getpid(),
             ++s->serial);
    if (!join(s->path, sizeof(s->path), s->directory, name))
        return fail(e, cap, "Chemin de session trop long.");
#endif
    set(s->project, sizeof(s->project), project);
    return true;
}
typedef struct {
    FILE *f;
    uint64_t hash;
    size_t total;
    bool ok;
} Codec;
static void write_raw(Codec *c, const void *p, size_t len) {
    if (!c->ok || len > STUDIO_SESSION_LIMIT - c->total) {
        c->ok = false;
        return;
    }
    if (fwrite(p, 1, len, c->f) != len)
        c->ok = false;
    c->hash = hash_bytes(c->hash, p, len);
    c->total += len;
}
static void read_raw(Codec *c, void *p, size_t len) {
    if (!c->ok || len > STUDIO_SESSION_LIMIT - c->total) {
        c->ok = false;
        return;
    }
    if (fread(p, 1, len, c->f) != len) {
        c->ok = false;
        return;
    }
    c->hash = hash_bytes(c->hash, p, len);
    c->total += len;
}
static void put64(Codec *c, uint64_t n) {
    unsigned char raw[8];
    for (int i = 0; i < 8; i++)
        raw[i] = (unsigned char)(n >> (i * 8));
    write_raw(c, raw, 8);
}
static uint64_t get64(Codec *c) {
    unsigned char raw[8] = {0};
    read_raw(c, raw, 8);
    uint64_t n = 0;
    for (int i = 0; i < 8; i++)
        n |= (uint64_t)raw[i] << (i * 8);
    return n;
}
static void put_text(Codec *c, const char *text) {
    size_t n = strlen(text);
    put64(c, n);
    write_raw(c, text, n);
}
static char *get_text(Codec *c, size_t max) {
    uint64_t n = get64(c);
    if (!c->ok || n > max) {
        c->ok = false;
        return NULL;
    }
    char *text = malloc((size_t)n + 1);
    if (!text) {
        c->ok = false;
        return NULL;
    }
    read_raw(c, text, (size_t)n);
    text[n] = 0;
    if (!c->ok || memchr(text, 0, (size_t)n) || !utf8_valid(text, (size_t)n)) {
        free(text);
        c->ok = false;
        return NULL;
    }
    return text;
}
static void get_fixed(Codec *c, char *out, size_t cap) {
    char *p = get_text(c, cap - 1);
    if (p) {
        set(out, cap, p);
        free(p);
    }
}
static void put_real(Codec *c, double value) {
    uint64_t raw = 0;
    if (!isfinite(value))
        c->ok = false;
    memcpy(&raw, &value, sizeof(raw));
    put64(c, raw);
}
static double get_real(Codec *c, double low, double high) {
    uint64_t raw = get64(c);
    double n = 0;
    memcpy(&n, &raw, sizeof(n));
    if (!isfinite(n)) {
        c->ok = false;
        return low;
    }
    return n < low ? low : n > high ? high : n;
}
static FILE *private_read(const char *path) {
    int fd = open(path, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0)
        return NULL;
    struct stat st;
    if (fstat(fd, &st) || !S_ISREG(st.st_mode) || st.st_size < 0 ||
        (uint64_t)st.st_size > STUDIO_SESSION_LIMIT
#ifndef _WIN32
        || st.st_uid != getuid() || st.st_nlink != 1
#endif
    ) {
        close(fd);
        errno = EINVAL;
        return NULL;
    }
    FILE *f = fdopen(fd, "rb");
    if (!f)
        close(fd);
    return f;
}
static bool checksum(Codec *c) {
    uint64_t expected = c->hash;
    uint64_t stored = get64(c);
    return c->ok && stored == expected && fgetc(c->f) == EOF && !ferror(c->f);
}
static void finish_write(Codec *c) {
    uint64_t expected = c->hash;
    put64(c, expected);
}
static bool atomic_start(StudioStore *s, const char *path, char *tmp, size_t cap, FILE **out, char *error,
                         size_t ecap) {
    (void)path;
    if (!parents(s->directory) || snprintf(tmp, cap, "%s/.studio-XXXXXX", s->directory) >= (int)cap)
        return fail(error, ecap, "Dossier de sessions inaccessible.");
#ifndef _WIN32
    struct stat st;
    if (!lstat(path, &st) && (!S_ISREG(st.st_mode) || st.st_uid != getuid() || st.st_nlink != 1))
        return fail(error, ecap, "Fichier de session non privé.");
    int fd = mkstemp(tmp);
#else
    if (_mktemp_s(tmp, cap))
        return fail(error, ecap, "Fichier temporaire impossible.");
    int fd = _open(tmp, _O_CREAT | _O_EXCL | _O_RDWR | _O_BINARY, _S_IREAD | _S_IWRITE);
#endif
    if (fd < 0)
        return fail(error, ecap, "Écriture de session : %s", strerror(errno));
    *out = fdopen(fd, "wb");
    if (!*out) {
        close(fd);
        unlink(tmp);
        return fail(error, ecap, "Écriture de session impossible.");
    }
    return true;
}
static bool atomic_finish(FILE *f, bool ok, const char *tmp, const char *path, StudioStore *s, char *e,
                          size_t cap) {
    if (fflush(f))
        ok = false;
#ifndef _WIN32
    if (ok && fsync(fileno(f)))
        ok = false;
#else
    if (ok && _commit(_fileno(f)))
        ok = false;
#endif
    if (fclose(f))
        ok = false;
#ifndef _WIN32
    if (ok && rename(tmp, path))
        ok = false;
    if (ok) {
        int fd = open(s->directory, O_RDONLY | O_CLOEXEC);
        if (fd < 0 || fsync(fd))
            ok = false;
        if (fd >= 0)
            close(fd);
    }
#else
    (void)s;
    if (ok && !MoveFileExA(tmp, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        ok = false;
#endif
    unlink(tmp);
    return ok ? true
              : fail(e, cap, "État/brouillons non enregistrés ou durabilité incertaine : %s",
                     strerror(errno));
}
static bool write_last(StudioStore *s, char *e, size_t cap) {
    char path[BUF_PATH], tmp[BUF_PATH];
    FILE *f = NULL;
    if (!join(path, sizeof(path), s->directory, "last.state") ||
        !atomic_start(s, path, tmp, sizeof(tmp), &f, e, cap))
        return false;
    Codec c = {f, UINT64_C(14695981039346656037), 0, true};
    put_text(&c, "PYTHON-STUDIO-LAST-1");
    put_text(&c, s->project);
    finish_write(&c);
    return atomic_finish(f, c.ok, tmp, path, s, e, cap);
}
bool studio_last_project(StudioStore *s, char *out, size_t cap) {
    if (!s->enabled)
        return false;
    char path[BUF_PATH];
    if (!join(path, sizeof(path), s->directory, "last.state"))
        return false;
    FILE *f = private_read(path);
    if (!f)
        return false;
    Codec c = {f, UINT64_C(14695981039346656037), 0, true};
    char *header = get_text(&c, 40), *project = get_text(&c, BUF_PATH - 1);
    bool ok = header && project && !strcmp(header, "PYTHON-STUDIO-LAST-1") && *project && absolute(project) &&
              strlen(project) < cap && checksum(&c);
    if (ok)
        set(out, cap, project);
    free(header);
    free(project);
    fclose(f);
    return ok;
}
void studio_image_free(StudioImage *i) {
    for (int n = 0; n < STUDIO_TABS; n++)
        if (i->tabs[n]) {
            buf_free(i->tabs[n]);
            free(i->tabs[n]);
        }
    memset(i, 0, sizeof(*i));
}
static bool valid_image(const StudioImage *i) {
    if (i->count < 1 || i->count > STUDIO_TABS || i->active < 0 || i->active >= i->count ||
        i->active_pane < 0 || i->active_pane > 1 || i->expanded_count < 0 ||
        i->expanded_count > STUDIO_EXPANDED)
        return false;
    for (int p = 0; p < 2; p++)
        if (i->panes[p] < 0 || i->panes[p] >= i->count)
            return false;
    if (i->panes[i->active_pane] != i->active || !absolute(i->project) || !*i->project ||
        !absolute(i->remote_root))
        return false;
    for (int n = 0; n < i->count; n++)
        if (!i->tabs[n] || !i->tabs[n]->text || !i->tabs[n]->saved || i->tabs[n]->len > BUF_LIMIT ||
            strlen(i->tabs[n]->saved) > BUF_LIMIT || !absolute(i->tabs[n]->path) ||
            !absolute(i->tabs[n]->recovery_origin))
            return false;
    return true;
}
bool studio_save(StudioStore *s, const StudioImage *i, char *e, size_t cap) {
    if (!s->enabled)
        return true;
    if (!valid_image(i))
        return fail(e, cap, "État de projet invalide ; ancien journal préservé.");
    if (!bind(s, i->project, e, cap))
        return false;
    char tmp[BUF_PATH];
    FILE *f = NULL;
    if (!atomic_start(s, s->path, tmp, sizeof(tmp), &f, e, cap))
        return false;
    Codec c = {f, UINT64_C(14695981039346656037), 0, true};
    write_raw(&c, magic, sizeof(magic));
    put64(&c, 1);
    put_text(&c, i->project);
    put_text(&c, i->remote_root);
    const char *profile[] = {i->profile.id,   i->profile.host,   i->profile.user,
                             i->profile.port, i->profile.folder, i->profile.identity};
    for (int n = 0; n < 6; n++)
        put_text(&c, profile[n]);
    put64(&c, (uint64_t)i->count);
    put64(&c, (uint64_t)i->active);
    put64(&c, (uint64_t)i->active_pane);
    put64(&c, (uint64_t)i->panes[0]);
    put64(&c, (uint64_t)i->panes[1]);
    put64(&c, i->flags);
    put_real(&c, i->ratio);
    put_real(&c, i->terminal_height);
    put64(&c, (uint64_t)i->font_size);
    put64(&c, (uint64_t)i->sidebar_width);
    put64(&c, (uint64_t)i->tree_scroll);
    put64(&c, (uint64_t)i->console_mode);
    put64(&c, (uint64_t)i->expanded_count);
    for (int n = 0; n < i->expanded_count; n++)
        put_text(&c, i->expanded[n]);
    for (int n = 0; n < i->count; n++) {
        Buffer *b = i->tabs[n];
        put_text(&c, b->path);
        put_text(&c, b->name);
        put_text(&c, b->recovery_origin);
        put_text(&c, i->discard[n] ? b->saved : b->text);
        put_text(&c, b->saved);
        put64(&c, (uint64_t)i->remote[n]);
        put64(&c, (uint64_t)b->crlf);
        put64(&c, (uint64_t)b->bom);
        put_real(&c, b->scroll_x);
        put_real(&c, b->scroll_y);
        int count = buf_caret_count(b);
        put64(&c, (uint64_t)count);
        for (int k = 0; k < count; k++) {
            BufCaret caret = buf_caret(b, k);
            put64(&c, (uint64_t)caret.cursor);
            put64(&c, caret.anchor == BUF_NO_SELECTION ? UINT64_MAX : (uint64_t)caret.anchor);
        }
    }
    finish_write(&c);
    if (!atomic_finish(f, c.ok, tmp, s->path, s, e, cap))
        return false;
    return write_last(s, e, cap);
}
static int integer(Codec *c, int max) {
    uint64_t n = get64(c);
    if (n > (uint64_t)max) {
        c->ok = false;
        return 0;
    }
    return (int)n;
}
static size_t position(Buffer *b, uint64_t p) {
    size_t n = p > b->len ? b->len : (size_t)p;
    while (n && n < b->len && ((unsigned char)b->text[n] & 0xc0) == 0x80)
        n--;
    return n;
}
static bool newest(StudioStore *s, const char *project, char *path, size_t cap) {
    set(path, cap, s->path);
#ifndef _WIN32
    DIR *dir = opendir(s->directory);
    if (!dir)
        return false;
    char prefix[32];
    snprintf(prefix, sizeof(prefix), "%016llx", (unsigned long long)project_key(project));
    struct timespec latest = {0};
    bool found = false;
    struct dirent *entry;
    while ((entry = readdir(dir))) {
        size_t len = strlen(entry->d_name);
        if (strncmp(entry->d_name, prefix, 16) || len < 22 || strcmp(entry->d_name + len - 6, ".state"))
            continue;
        char candidate[BUF_PATH];
        struct stat st;
        if (!join(candidate, sizeof(candidate), s->directory, entry->d_name) || lstat(candidate, &st))
            continue;
#ifdef __APPLE__
        struct timespec stamp = st.st_mtimespec;
#else
        struct timespec stamp = st.st_mtim;
#endif
        if (!found || stamp.tv_sec > latest.tv_sec ||
            (stamp.tv_sec == latest.tv_sec && stamp.tv_nsec > latest.tv_nsec)) {
            set(path, cap, candidate);
            latest = stamp;
            found = true;
        }
    }
    closedir(dir);
    return found;
#else
    char pattern[BUF_PATH], prefix[32];
    snprintf(prefix, sizeof(prefix), "%016llx", (unsigned long long)project_key(project));
    if (!join(pattern, sizeof(pattern), s->directory, "*.state"))
        return false;
    WIN32_FIND_DATAA data;
    HANDLE handle = FindFirstFileA(pattern, &data);
    if (handle == INVALID_HANDLE_VALUE)
        return false;
    FILETIME latest = {0};
    bool found = false;
    do {
        size_t len = strlen(data.cFileName);
        if (strncmp(data.cFileName, prefix, 16) || len < 22 || strcmp(data.cFileName + len - 6, ".state") ||
            (data.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)))
            continue;
        if (!found || CompareFileTime(&data.ftLastWriteTime, &latest) > 0) {
            if (join(path, cap, s->directory, data.cFileName)) {
                latest = data.ftLastWriteTime;
                found = true;
            }
        }
    } while (FindNextFileA(handle, &data));
    FindClose(handle);
    return found;
#endif
}
int studio_load(StudioStore *s, const char *project, StudioImage *i, char *e, size_t cap) {
    if (!s->enabled)
        return 0;
    if (!bind(s, project, e, cap))
        return -1;
    char source[BUF_PATH];
    if (!newest(s, project, source, sizeof(source)))
        return 0;
    FILE *f = private_read(source);
    if (!f) {
        if (errno == ENOENT)
            return 0;
        if (!strcmp(source, s->path) || !access(s->path, F_OK)) {
            char name[128];
            snprintf(name, sizeof(name), "%016llx-recovery-%ld-%lu.state",
                     (unsigned long long)project_key(project), (long)getpid(), ++s->serial);
            join(s->path, sizeof(s->path), s->directory, name);
        }
        fail(e, cap, "Session illisible, fichier préservé.");
        return -1;
    }
    memset(i, 0, sizeof(*i));
    Codec c = {f, UINT64_C(14695981039346656037), 0, true};
    char header[sizeof(magic)] = {0};
    read_raw(&c, header, sizeof(header));
    if (memcmp(header, magic, sizeof(header)) || get64(&c) != 1)
        c.ok = false;
    get_fixed(&c, i->project, sizeof(i->project));
    get_fixed(&c, i->remote_root, sizeof(i->remote_root));
    char *profile[] = {i->profile.id,   i->profile.host,   i->profile.user,
                       i->profile.port, i->profile.folder, i->profile.identity};
    size_t sizes[] = {sizeof(i->profile.id),   sizeof(i->profile.host),   sizeof(i->profile.user),
                      sizeof(i->profile.port), sizeof(i->profile.folder), sizeof(i->profile.identity)};
    for (int n = 0; n < 6; n++)
        get_fixed(&c, profile[n], sizes[n]);
    i->count = integer(&c, STUDIO_TABS);
    i->active = integer(&c, STUDIO_TABS - 1);
    i->active_pane = integer(&c, 1);
    i->panes[0] = integer(&c, STUDIO_TABS - 1);
    i->panes[1] = integer(&c, STUDIO_TABS - 1);
    i->flags = (uint32_t)integer(&c, 63);
    i->ratio = get_real(&c, .25, .75);
    i->terminal_height = get_real(&c, 110, 1000);
    i->font_size = integer(&c, 32);
    i->sidebar_width = integer(&c, 420);
    i->tree_scroll = integer(&c, 100000);
    i->console_mode = integer(&c, 3);
    if (i->font_size < 14)
        i->font_size = 20;
    if (i->sidebar_width < 190)
        i->sidebar_width = 246;
    i->expanded_count = integer(&c, STUDIO_EXPANDED);
    for (int n = 0; n < i->expanded_count; n++)
        get_fixed(&c, i->expanded[n], BUF_PATH);
    for (int n = 0; c.ok && n < i->count; n++) {
        Buffer *b = calloc(1, sizeof(*b));
        if (!b) {
            c.ok = false;
            break;
        }
        i->tabs[n] = b;
        char path[BUF_PATH] = {0}, name[256] = {0}, origin[BUF_PATH] = {0};
        get_fixed(&c, path, sizeof(path));
        get_fixed(&c, name, sizeof(name));
        get_fixed(&c, origin, sizeof(origin));
        char *text = get_text(&c, BUF_LIMIT), *saved = get_text(&c, BUF_LIMIT);
        if (!text || !saved || !*name || strchr(name, '/') || !buf_init(b, text, name)) {
            free(text);
            free(saved);
            c.ok = false;
            break;
        }
        free(text);
        free(b->saved);
        b->saved = saved;
        set(b->path, sizeof(b->path), path);
        set(b->recovery_origin, sizeof(b->recovery_origin), origin);
        i->remote[n] = integer(&c, 1) != 0;
        b->crlf = integer(&c, 1) != 0;
        b->bom = integer(&c, 1) != 0;
        b->scroll_x = (float)get_real(&c, 0, 1e8);
        b->scroll_y = (float)get_real(&c, 0, 1e8);
        int count = integer(&c, BUF_MAX_CARETS);
        if (count < 1)
            c.ok = false;
        BufCaret carets[BUF_MAX_CARETS];
        for (int k = 0; k < count; k++) {
            uint64_t cur = get64(&c), anchor = get64(&c);
            carets[k] =
                (BufCaret){position(b, cur), anchor == UINT64_MAX ? BUF_NO_SELECTION : position(b, anchor)};
        }
        if (c.ok)
            buf_set_carets(b, carets, count);
    }
    bool ok = c.ok && valid_image(i) && !strcmp(i->project, project) && checksum(&c);
    fclose(f);
    if (!ok) {
        studio_image_free(i);
        if (!strcmp(source, s->path) || !access(s->path, F_OK)) {
            char name[128];
            snprintf(name, sizeof(name), "%016llx-recovery-%ld-%lu.state",
                     (unsigned long long)project_key(project), (long)getpid(), ++s->serial);
            join(s->path, sizeof(s->path), s->directory, name);
        }
        fail(e, cap, "Session invalide/incomplète : fichier préservé, aucune restauration partielle.");
        return -1;
    }
    if (strcmp(source, s->path) && !access(s->path, F_OK)) {
        char name[128];
        snprintf(name, sizeof(name), "%016llx-recovery-%ld-%lu.state",
                 (unsigned long long)project_key(project), (long)getpid(), ++s->serial);
        if (!join(s->path, sizeof(s->path), s->directory, name)) {
            studio_image_free(i);
            fail(e, cap, "Chemin du journal de récupération trop long.");
            return -1;
        }
    }
    return 1;
}
static bool document(const char *path, Buffer *b, SshSession *ssh) {
    char error[512];
    if (!ssh_guard(ssh, path, false, error, sizeof(error)))
        return false;
    int fd = open(path, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0)
        return false;
    struct stat st;
    if (fstat(fd, &st) || !S_ISREG(st.st_mode) || st.st_size < 0 || st.st_size > BUF_LIMIT) {
        close(fd);
        return false;
    }
    size_t size = (size_t)st.st_size;
    char *raw = malloc(size + 1);
    if (!raw) {
        close(fd);
        return false;
    }
    FILE *f = fdopen(fd, "rb");
    if (!f) {
        close(fd);
        free(raw);
        return false;
    }
    bool ok = fread(raw, 1, size, f) == size && !ferror(f);
    fclose(f);
    raw[size] = 0;
    bool bom = size >= 3 && !memcmp(raw, "\xef\xbb\xbf", 3), crlf = false;
    size_t n = 0;
    for (size_t k = bom ? 3 : 0; k < size; k++) {
        if (raw[k] == '\r') {
            if (k + 1 < size && raw[k + 1] == '\n') {
                crlf = true;
                k++;
            }
            raw[n++] = '\n';
        } else
            raw[n++] = raw[k];
    }
    raw[n] = 0;
    ok = ok && !memchr(raw, 0, n) && utf8_valid(raw, n) &&
         ssh_guard(ssh, path, false, error, sizeof(error)) && buf_init(b, raw, fs_basename(path));
    free(raw);
    if (ok) {
        set(b->path, sizeof(b->path), path);
        b->bom = bom;
        b->crlf = crlf;
    }
    return ok;
}
int studio_reconcile(StudioImage *i, SshSession *ssh, bool remote_only) {
    bool remote_valid = false;
    if (!remote_only && *i->remote_root && *i->profile.host) {
        char expected[BUF_PATH], error[512];
        remote_valid = join(expected, sizeof(expected), ssh->cache, i->profile.id) &&
                       !strcmp(expected, i->remote_root) &&
                       ssh_profile_valid(&i->profile, false, error, sizeof(error));
        if (remote_valid) {
            ssh->profile = i->profile;
            set(ssh->mountpoint, sizeof(ssh->mountpoint), i->remote_root);
            ssh->state = SSH_LOST;
            set(ssh->error, sizeof(ssh->error),
                "Session SSH restaurée hors ligne. Reconnectez explicitement ; aucun secret enregistré.");
        }
    }
    int recovered = 0;
    for (int n = 0; n < i->count; n++) {
        Buffer *b = i->tabs[n];
        if (!*b->path)
            continue;
        bool remote = i->remote[n] || (*ssh->cache && fs_under(ssh->cache, b->path));
        if (remote_only && !b->session_pending_remote)
            continue;
        if (!remote_only && remote && remote_valid && fs_under(i->remote_root, b->path)) {
            b->session_pending_remote = true;
            continue;
        }
        Buffer *disk = calloc(1, sizeof(*disk));
        bool ok = disk && (!remote || remote_only) && document(b->path, disk, ssh);
        if (ok && buf_dirty(b) &&
            (strcmp(disk->text, b->saved) || disk->bom != b->bom || disk->crlf != b->crlf))
            ok = false;
        if (ok && !buf_dirty(b)) {
            BufCaret carets[BUF_MAX_CARETS];
            int count = buf_caret_count(b);
            for (int k = 0; k < count; k++)
                carets[k] = buf_caret(b, k);
            float x = b->scroll_x, y = b->scroll_y;
            unsigned long version = b->version + 1;
            buf_free(b);
            *b = *disk;
            b->version = version;
            b->lines_version = 0;
            memset(disk, 0, sizeof(*disk));
            buf_set_carets(b, carets, count);
            b->scroll_x = x;
            b->scroll_y = y;
        } else if (!ok) {
            set(b->recovery_origin, sizeof(b->recovery_origin), b->path);
            *b->path = 0;
            b->saved[0] = 0;
            recovered++;
        }
        b->session_pending_remote = false;
        if (disk) {
            buf_free(disk);
            free(disk);
        }
    }
    return recovered;
}

uint64_t studio_signature(const StudioImage *i) {
    uint64_t h = hash_bytes(UINT64_C(14695981039346656037), i, sizeof(*i));
    for (int n = 0; n < i->count && n < STUDIO_TABS; n++) {
        const Buffer *b = i->tabs[n];
        if (!b || !b->text || !b->saved)
            continue;
        h = hash_bytes(h, b->text, b->len);
        h = hash_bytes(h, b->saved, strlen(b->saved));
        h = hash_bytes(h, b->path, strlen(b->path));
        h = hash_bytes(h, b->name, strlen(b->name));
        h = hash_bytes(h, b->recovery_origin, strlen(b->recovery_origin));
        h = hash_bytes(h, &b->scroll_x, sizeof(b->scroll_x));
        h = hash_bytes(h, &b->scroll_y, sizeof(b->scroll_y));
        h = hash_bytes(h, &b->crlf, sizeof(b->crlf));
        h = hash_bytes(h, &b->bom, sizeof(b->bom));
        for (int k = 0; k < buf_caret_count(b); k++) {
            BufCaret c = buf_caret(b, k);
            h = hash_bytes(h, &c, sizeof(c));
        }
    }
    return h;
}
