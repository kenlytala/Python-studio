#define _POSIX_C_SOURCE 200809L
#include "folder_search.h"
#include "import_paths.h"
#include "search_pattern.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>
typedef struct _stat64 SearchStat;
#define read_fd _read
#define close_fd _close
#define fstat_fd _fstat64
#else
#include <dirent.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>
typedef struct stat SearchStat;
#define read_fd read
#define close_fd close
#define fstat_fd fstat
#endif

typedef struct {
#ifdef _WIN32
    CRITICAL_SECTION mutex;
#else
    pthread_mutex_t mutex;
#endif
} SearchPublication;
typedef struct {
    atomic_uint refs;
    char *text;
    SearchHit *hits;
} SearchStorage;
static void *publication_new(void) {
    SearchPublication *p = calloc(1, sizeof(*p));
    if (!p)
        return NULL;
#ifdef _WIN32
    if (!InitializeCriticalSectionEx(&p->mutex, 0, 0)) {
        free(p);
        return NULL;
    }
#else
    if (pthread_mutex_init(&p->mutex, NULL)) {
        free(p);
        return NULL;
    }
#endif
    return p;
}
static void publication_lock(FolderSearchJob *job) {
    SearchPublication *p = job->publication;
#ifdef _WIN32
    EnterCriticalSection(&p->mutex);
#else
    pthread_mutex_lock(&p->mutex);
#endif
}
static bool publication_try(FolderSearchJob *job) {
    SearchPublication *p = job->publication;
#ifdef _WIN32
    return TryEnterCriticalSection(&p->mutex) != 0;
#else
    return pthread_mutex_trylock(&p->mutex) == 0;
#endif
}
static void publication_unlock(FolderSearchJob *job) {
    SearchPublication *p = job->publication;
#ifdef _WIN32
    LeaveCriticalSection(&p->mutex);
#else
    pthread_mutex_unlock(&p->mutex);
#endif
}
static void publication_free(void *data) {
    SearchPublication *p = data;
    if (!p)
        return;
#ifdef _WIN32
    DeleteCriticalSection(&p->mutex);
#else
    pthread_mutex_destroy(&p->mutex);
#endif
    free(p);
}
static void release_file(SearchFile *file) {
    SearchStorage *storage = file->storage;
    if (!storage) {
        free(file->text);
        free(file->hits);
        return;
    }
    if (atomic_fetch_sub_explicit(&storage->refs, 1, memory_order_acq_rel) == 1) {
        free(storage->text);
        free(storage->hits);
        free(storage);
    }
}
unsigned folder_search_columns(const char *text, size_t len) {
    if (!text)
        return 0;
    unsigned column = 0, widest = 0;
    for (size_t p = 0; p < len; p = utf8_next(text, len, p)) {
        if (text[p] == '\n') {
            if (column > widest)
                widest = column;
            column = 0;
        } else
            column += text[p] == '\t' ? 4 - column % 4 : 1;
    }
    return column > widest ? column : widest;
}
static bool fail(char *error, size_t cap, const char *text) {
    if (error && cap)
        snprintf(error, cap, "%s", text);
    return false;
}
static bool valid_path(const char *path) {
    if (!path)
        return false;
    size_t n = 0;
    while (n < FS_PATH && path[n]) {
        unsigned char c = (unsigned char)path[n++];
        if (c < 32 || c == 127)
            return false;
    }
    if (!n || n >= FS_PATH || !utf8_valid(path, n))
        return false;
    return path[0] == '/'
#ifdef _WIN32
           || path[0] == '\\' ||
           (n >= 3 && isalpha((unsigned char)path[0]) && path[1] == ':' &&
            (path[2] == '/' || path[2] == '\\'))
#endif
        ;
}
bool folder_search_root(char out[FS_PATH], const char *value, const char *project, char *error, size_t cap) {
    if (!out || !value || !*value || strlen(value) >= FS_PATH || !valid_path(project))
        return fail(error, cap, "Dossier de recherche invalide (4 095 octets maximum).");
    char raw[FS_PATH];
    int n;
    if (value[0] == '~' && (!value[1] || value[1] == '/')) {
        const char *home = getenv("HOME");
#ifdef _WIN32
        if (!home || !*home)
            home = getenv("USERPROFILE");
#endif
        if (!valid_path(home))
            return fail(error, cap, "Dossier personnel introuvable.");
        n = snprintf(raw, sizeof(raw), "%s%s", home, value + 1);
    } else if (valid_path(value))
        n = snprintf(raw, sizeof(raw), "%s", value);
    else
        n = snprintf(raw, sizeof(raw), "%s/%s", project, value);
    if (n < 1 || n >= FS_PATH || !valid_path(raw))
        return fail(error, cap, "Dossier absolu UTF-8 requis (4 095 octets maximum).");
#ifdef _WIN32
    for (char *p = raw; *p; p++)
        if (*p == '\\')
            *p = '/';
#endif
    size_t positions[FS_PATH / 2 + 1], used = 0, depth = 0;
    char normalized[FS_PATH];
    const char *p = raw;
#ifdef _WIN32
    if (isalpha((unsigned char)p[0]) && p[1] == ':') {
        normalized[used++] = p[0];
        normalized[used++] = ':';
        p += 2;
    }
    /* Preserve a UNC prefix. */
    if (p[0] == '/' && p[1] == '/')
        normalized[used++] = '/';
#endif
    if (*p == '/')
        normalized[used++] = '/';
    while (*p == '/')
        p++;
    while (*p) {
        const char *start = p;
        while (*p && *p != '/')
            p++;
        size_t len = (size_t)(p - start);
        while (*p == '/')
            p++;
        if (len == 1 && start[0] == '.')
            continue;
        if (len == 2 && start[0] == '.' && start[1] == '.') {
            if (depth)
                used = positions[--depth];
            continue;
        }
        positions[depth++] = used;
        if (used && normalized[used - 1] != '/')
            normalized[used++] = '/';
        if (len >= FS_PATH - used)
            return fail(error, cap, "Dossier trop long.");
        memcpy(normalized + used, start, len);
        used += len;
    }
    normalized[used] = 0;
    memcpy(out, normalized, used + 1);
    return true;
}
SearchLimits folder_search_limits(void) {
    return (SearchLimits){SEARCH_FILE_MAX, SEARCH_READ_MAX,    SEARCH_SNAPSHOT_MAX, SEARCH_FILES_MAX,
                          SEARCH_HITS_MAX, SEARCH_ENTRIES_MAX, SEARCH_DEPTH_MAX,    SEARCH_LINES_MAX};
}
static bool limits_valid(const SearchLimits *p) {
    return p && p->file_bytes && p->file_bytes <= SEARCH_FILE_MAX && p->read_bytes &&
           p->read_bytes <= SEARCH_READ_MAX && p->snapshot_bytes &&
           p->snapshot_bytes <= SEARCH_SNAPSHOT_MAX && p->files && p->files <= SEARCH_FILES_MAX && p->hits &&
           p->hits <= SEARCH_HITS_MAX && p->entries && p->entries <= SEARCH_ENTRIES_MAX && p->depth &&
           p->depth <= SEARCH_DEPTH_MAX && p->lines && p->lines <= SEARCH_LINES_MAX;
}
bool folder_search_query_valid(const char *query) {
    if (!query)
        return false;
    size_t n = 0;
    while (n <= SEARCH_QUERY_MAX && query[n]) {
        unsigned char c = (unsigned char)query[n++];
        if ((c < 32 && c != '\t') || c == 127)
            return false;
    }
    return n && n <= SEARCH_QUERY_MAX && utf8_valid(query, n);
}
static bool find_hits(const char *text, size_t len, const char *query, unsigned limit,
                      const atomic_bool *cancel, SearchHit **out, unsigned *count, char *error, size_t cap) {
    if (out)
        *out = NULL;
    if (count)
        *count = 0;
    if (!out || !count || !text || len > SEARCH_FILE_MAX || !utf8_valid(text, len) || memchr(text, 0, len) ||
        !folder_search_query_valid(query) || !limit || limit > SEARCH_HITS_MAX)
        return fail(error, cap, "Texte ou limite de recherche invalide.");
    size_t qlen = strlen(query), prefix[SEARCH_QUERY_MAX] = {0}, k = 0;
    for (size_t i = 1; i < qlen; i++) {
        while (k && query[k] != query[i])
            k = prefix[k - 1];
        if (query[k] == query[i])
            k++;
        prefix[i] = k;
    }
    SearchHit *hits = NULL;
    unsigned used = 0, capacity = 0;
    k = 0;
    for (size_t i = 0; i < len && used < limit; i++) {
        if (cancel && !(i % 4096) && atomic_load(cancel)) {
            free(hits);
            return true;
        }
        while (k && query[k] != text[i])
            k = prefix[k - 1];
        if (query[k] == text[i])
            k++;
        if (k == qlen) {
            if (used == capacity) {
                unsigned next = capacity ? capacity * 2 : 16;
                if (next > limit)
                    next = limit;
                SearchHit *p = realloc(hits, (size_t)next * sizeof(*p));
                if (!p) {
                    free(hits);
                    return fail(error, cap, "Mémoire insuffisante pour les occurrences.");
                }
                hits = p;
                capacity = next;
            }
            hits[used++] = (SearchHit){.start = i + 1 - qlen, .end = i + 1};
            k = 0; /* Explicitly non-overlapping literal matches. */
        }
    }
    size_t pos = 0;
    unsigned row = 0, column = 0;
    for (unsigned h = 0; h < used; h++) {
        while (pos < hits[h].end) {
            if (cancel && !(pos % 4096) && atomic_load(cancel)) {
                free(hits);
                return true;
            }
            if (pos == hits[h].start) {
                hits[h].line = row;
                hits[h].column = column;
            }
            if (text[pos] == '\n') {
                row++;
                column = 0;
            } else if (text[pos] == '\t')
                column += 4 - column % 4;
            else
                column++;
            pos = utf8_next(text, len, pos);
        }
        hits[h].width = column - hits[h].column;
    }
    *out = hits;
    *count = used;
    return true;
}
bool folder_search_find(const char *text, size_t len, const char *query, unsigned limit, SearchHit **hits,
                        unsigned *count, char *error, size_t cap) {
    if (error && cap)
        *error = 0;
    return find_hits(text, len, query, limit, NULL, hits, count, error, cap);
}
static bool matched_hits(SearchMatcher *matcher, const char *text, size_t len, const char *query,
                         unsigned limit, const atomic_bool *cancel, SearchHit **out, unsigned *count,
                         char *error, size_t cap) {
    if (!search_matcher_regex(matcher))
        return find_hits(text, len, query, limit, cancel, out, count, error, cap);
    *out = NULL;
    *count = 0;
    if (!text || len > SEARCH_FILE_MAX || !utf8_valid(text, len) || memchr(text, 0, len) || !limit ||
        limit > SEARCH_HITS_MAX)
        return fail(error, cap, "Texte ou limite de recherche invalide.");
    SearchHit *hits = NULL;
    unsigned used = 0, capacity = 0;
    size_t offset = 0;
    search_matcher_begin(matcher);
    while (used < limit) {
        size_t begin, end;
        int result = search_matcher_next(matcher, text, len, offset, &begin, &end, error, cap);
        if (result < 0) {
            free(hits);
            return false;
        }
        if (!result)
            break;
        if (used == capacity) {
            unsigned next = capacity ? capacity * 2 : 16;
            if (next > limit)
                next = limit;
            SearchHit *p = realloc(hits, (size_t)next * sizeof(*p));
            if (!p) {
                free(hits);
                return fail(error, cap, "Mémoire insuffisante pour les occurrences.");
            }
            hits = p;
            capacity = next;
        }
        hits[used++] = (SearchHit){.start = begin, .end = end};
        if (end > begin)
            offset = end;
        else if (end == len)
            break;
        else
            offset = utf8_next(text, len, end);
    }
    size_t pos = 0;
    unsigned row = 0, column = 0;
    for (unsigned i = 0; i < used; i++) {
        while (pos < hits[i].start) {
            if (text[pos] == '\n') {
                row++;
                column = 0;
            } else if (text[pos] == '\t')
                column += 4 - column % 4;
            else
                column++;
            pos = utf8_next(text, len, pos);
        }
        hits[i].line = row;
        hits[i].column = column;
        bool crossed = false;
        while (pos < hits[i].end) {
            if (text[pos] == '\n') {
                if (!crossed) {
                    hits[i].width = column - hits[i].column;
                    crossed = true;
                }
                row++;
                column = 0;
            } else if (text[pos] == '\t')
                column += 4 - column % 4;
            else
                column++;
            pos = utf8_next(text, len, pos);
        }
        if (!crossed)
            hits[i].width = column - hits[i].column;
    }
    if (cancel && atomic_load(cancel)) {
        free(hits);
        return true;
    }
    *out = hits;
    *count = used;
    return true;
}
bool folder_search_find_options(const char *text, size_t len, const char *query, unsigned limit,
                                const SearchOptions *options, SearchHit **hits, unsigned *count, char *error,
                                size_t cap) {
    if (error && cap)
        *error = 0;
    if (hits)
        *hits = NULL;
    if (count)
        *count = 0;
    if (!hits || !count || !folder_search_query_valid(query))
        return fail(error, cap, "Texte ou limite de recherche invalide.");
    SearchOptions defaults = folder_search_options();
    SearchMatcher *m = search_matcher_new(query, options ? options : &defaults, NULL, error, cap);
    if (!m)
        return false;
    bool ok = matched_hits(m, text, len, query, limit, NULL, hits, count, error, cap);
    search_matcher_free(m);
    return ok;
}
void folder_search_report_free(SearchReport *report) {
    if (!report)
        return;
    for (unsigned i = 0; i < report->count; i++) {
        release_file(&report->files[i]);
    }
    free(report->files);
    free(report);
}
bool folder_search_next(const SearchReport *report, int *file, int *hit, int direction) {
    if (!report || !report->count || !file || !hit || (direction != 1 && direction != -1))
        return false;
    if (*file < 0 || (unsigned)*file >= report->count || *hit < 0 ||
        (unsigned)*hit >= report->files[*file].count) {
        *file = direction == 1 ? 0 : (int)report->count - 1;
        *hit = direction == 1 ? 0 : (int)report->files[*file].count - 1;
        return true;
    }
    *hit += direction;
    if ((unsigned)*hit >= report->files[*file].count && *hit >= 0) {
        *file = (*file + 1) % (int)report->count;
        *hit = 0;
    } else if (*hit < 0) {
        *file = (*file + (int)report->count - 1) % (int)report->count;
        *hit = (int)report->files[*file].count - 1;
    }
    return true;
}
bool folder_search_profile_file(char *out, size_t cap, const char *settings, const char *project) {
    char previous[FS_PATH], parent[FS_PATH];
    if (!out || !cap || !import_paths_file(previous, sizeof(previous), settings, project) ||
        !fs_parent(previous, parent))
        return false;
    const char *name = fs_basename(previous);
    if (strncmp(name, "imports-", 8))
        return false;
    int n = snprintf(out, cap, "%s/folder-search-%s", parent, name + 8);
    return n > 0 && (size_t)n < cap;
}
bool folder_search_profile_load(char out[FS_PATH], const char *file, const char *project, char *error,
                                size_t cap) {
    if (!out)
        return fail(error, cap, "Profil de recherche invalide.");
    ImportPaths *p = calloc(1, sizeof(*p));
    if (!p)
        return fail(error, cap, "Mémoire insuffisante.");
    bool ok = import_paths_load(p, file, project, error, cap);
    if (ok && (p->count != 1 || !valid_path(p->paths[0])))
        ok = fail(error, cap, "Un dossier de recherche par projet est requis.");
    if (ok)
        strcpy(out, p->paths[0]);
    free(p);
    return ok;
}
bool folder_search_profile_save(const char *root, const char *file, const char *project, char *error,
                                size_t cap) {
    if (!valid_path(root))
        return fail(error, cap, "Dossier absolu UTF-8 requis (4 095 octets maximum).");
    ImportPaths *p = calloc(1, sizeof(*p));
    if (!p)
        return fail(error, cap, "Mémoire insuffisante.");
    p->count = 1;
    strcpy(p->paths[0], root);
    bool ok = import_paths_save(p, file, project, error, cap);
    free(p);
    return ok;
}

static bool guard(FolderSearchJob *job, const char *path, char *error, size_t cap) {
    return !job->guard || job->guard(job->guard_context, path, false, error, cap);
}
static bool ignored(const char *name) {
    const char *names[] = {".git",        ".hg",          ".svn",   ".venv",        "venv",
                           "__pycache__", "node_modules", ".cache", ".pytest_cache"};
    for (size_t i = 0; i < sizeof(names) / sizeof(*names); i++)
        if (!strcmp(name, names[i]))
            return true;
    return false;
}
static bool usable_name(const char *name) {
    size_t n = strlen(name);
    if (!n || n >= FS_PATH || !utf8_valid(name, n))
        return false;
    for (size_t i = 0; i < n; i++)
        if ((unsigned char)name[i] < 32 || name[i] == 127)
            return false;
    return true;
}
static bool same_stat(const SearchStat *a, const SearchStat *b) {
    if (a->st_dev != b->st_dev || a->st_ino != b->st_ino || a->st_size != b->st_size ||
        a->st_mtime != b->st_mtime || a->st_ctime != b->st_ctime)
        return false;
#ifdef __APPLE__
    return a->st_mtimespec.tv_nsec == b->st_mtimespec.tv_nsec &&
           a->st_ctimespec.tv_nsec == b->st_ctimespec.tv_nsec;
#elif !defined(_WIN32)
    return a->st_mtim.tv_nsec == b->st_mtim.tv_nsec && a->st_ctim.tv_nsec == b->st_ctim.tv_nsec;
#else
    return true;
#endif
}
static bool regular(const SearchStat *p) {
#ifdef _WIN32
    return (p->st_mode & _S_IFMT) == _S_IFREG;
#else
    return S_ISREG(p->st_mode);
#endif
}
static bool directory(const SearchStat *p) {
#ifdef _WIN32
    return (p->st_mode & _S_IFMT) == _S_IFDIR;
#else
    return S_ISDIR(p->st_mode);
#endif
}
static bool child_stat(const char *path, SearchStat *out, bool *link) {
    *link = false;
#ifdef _WIN32
    wchar_t wide[FS_PATH];
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wide, FS_PATH))
        return false;
    DWORD a = GetFileAttributesW(wide);
    if (a == INVALID_FILE_ATTRIBUTES)
        return false;
    if (a & FILE_ATTRIBUTE_REPARSE_POINT) {
        *link = true;
        return true;
    }
    return !_wstat64(wide, out);
#else
    if (lstat(path, out))
        return false;
    *link = S_ISLNK(out->st_mode);
    return true;
#endif
}
static int open_read(const char *path) {
#ifdef _WIN32
    wchar_t wide[FS_PATH];
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wide, FS_PATH))
        return -1;
    return _wopen(wide, _O_RDONLY | _O_BINARY | _O_NOINHERIT);
#else
    return open(path, O_RDONLY | O_NONBLOCK
#ifdef O_NOFOLLOW
                          | O_NOFOLLOW
#endif
    );
#endif
}
static void progress(FolderSearchJob *job) {
    SearchReport *r = job->report;
    atomic_store(&job->progress_entries, r->entries);
    atomic_store(&job->progress_examined, r->examined);
    atomic_store(&job->progress_found, r->count);
    atomic_store(&job->progress_hits, r->hit_count);
    atomic_store(&job->progress_bytes, r->read_bytes);
    atomic_store(&job->progress_skipped, r->skipped);
    atomic_store(&job->progress_ignored, r->ignored);
    atomic_store(&job->progress_limited, r->limited);
}
static bool read_file(FolderSearchJob *job, const char *path, const char *relative, const SearchStat *info) {
    SearchReport *r = job->report;
    if (info->st_size < 0 || (uint64_t)info->st_size > job->limits.file_bytes) {
        r->skipped++;
        return true;
    }
    if ((uint64_t)info->st_size > job->limits.read_bytes - r->read_bytes) {
        r->limited = job->stop = true;
        return true;
    }
    char error[512];
    if (!guard(job, path, error, sizeof(error))) {
        r->skipped++;
        return true;
    }
    if (atomic_load(&job->cancel))
        return true;
    int fd = open_read(path);
    if (fd < 0) {
        r->skipped++;
        return true;
    }
    SearchStat before, after;
    if (fstat_fd(fd, &before) || !regular(&before) || !same_stat(info, &before)) {
        close_fd(fd);
        r->skipped++;
        return true;
    }
    size_t expected = (size_t)before.st_size, used = 0;
    char *raw = malloc(expected + 1);
    if (!raw) {
        close_fd(fd);
        return fail(job->error, sizeof(job->error), "Mémoire insuffisante pour le contenu du fichier.");
    }
    bool ok = true;
    while (used < expected && !atomic_load(&job->cancel)) {
        size_t left = expected - used;
        if (left > 65536)
            left = 65536;
        if (left > job->limits.read_bytes - r->read_bytes)
            left = job->limits.read_bytes - r->read_bytes;
        if (!left) {
            r->limited = job->stop = true;
            ok = false;
            break;
        }
#ifdef _WIN32
        int n = read_fd(fd, raw + used, (unsigned)left);
#else
        ssize_t n = read_fd(fd, raw + used, left);
#endif
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0) {
            ok = false;
            break;
        }
        size_t previous = used;
        used += (size_t)n;
        r->read_bytes += (size_t)n;
        atomic_store(&job->progress_bytes, r->read_bytes);
        for (size_t i = previous; i < used; i++) {
            unsigned char c = (unsigned char)raw[i];
            if ((c < 32 && c != '\t' && c != '\n' && c != '\r') || c == 127) {
                close_fd(fd);
                free(raw);
                r->skipped++;
                return true;
            }
        }
    }
    if (fstat_fd(fd, &after))
        ok = false;
    close_fd(fd);
    if (!guard(job, path, error, sizeof(error)))
        ok = false;
    if (atomic_load(&job->cancel)) {
        free(raw);
        return true;
    }
    if (!ok || used != expected || !same_stat(&before, &after) || !utf8_valid(raw, used)) {
        free(raw);
        r->skipped++;
        return true;
    }
    size_t n = 0;
    unsigned lines = 1;
    for (size_t i = 0; i < used; i++) {
        unsigned char c = (unsigned char)raw[i];
        if ((c < 32 && c != '\t' && c != '\n' && c != '\r') || c == 127) {
            free(raw);
            r->skipped++;
            return true;
        }
        if (c == '\r') {
            c = '\n';
            if (i + 1 < used && raw[i + 1] == '\n')
                i++;
        }
        raw[n++] = (char)c;
        if (c == '\n')
            lines++;
    }
    raw[n] = 0;
    if (lines > job->limits.lines) {
        free(raw);
        r->skipped++;
        return true;
    }
    SearchHit *hits = NULL;
    unsigned count = 0;
    if (!matched_hits(job->matcher, raw, n, job->query, job->limits.hits - r->hit_count, &job->cancel, &hits,
                      &count, job->error, sizeof(job->error))) {
        free(raw);
        return false;
    }
    if (atomic_load(&job->cancel) || !count) {
        free(raw);
        free(hits);
        return true;
    }
    if (n > job->limits.snapshot_bytes - r->snapshot_bytes) {
        free(raw);
        free(hits);
        r->limited = job->stop = true;
        return true;
    }
    SearchStorage *storage = malloc(sizeof(*storage));
    if (!storage) {
        free(raw);
        free(hits);
        return fail(job->error, sizeof(job->error), "Mémoire insuffisante pour publier le fichier.");
    }
    atomic_init(&storage->refs, 1);
    storage->text = raw;
    storage->hits = hits;
    /* Only descriptor writes are protected. File I/O/matching are never under
       the lock; a UI snapshot uses trylock rather than waiting. */
    publication_lock(job);
    SearchFile *file = &r->files[r->count++];
    file->storage = storage;
    strcpy(file->path, path);
    strcpy(file->relative, relative);
    file->text = raw;
    file->len = n;
    file->hits = hits;
    file->count = count;
    r->snapshot_bytes += n;
    r->hit_count += count;
    if (r->count == job->limits.files || r->hit_count == job->limits.hits)
        r->limited = job->stop = true;
    progress(job);
    publication_unlock(job);
    return true;
}
typedef struct {
    char **items;
    unsigned count, cap;
    size_t held;
} Names;
static void names_free(FolderSearchJob *job, Names *names) {
    for (unsigned i = 0; i < names->count; i++)
        free(names->items[i]);
    free(names->items);
    job->names_live -= names->held;
}
static bool name_add(FolderSearchJob *job, Names *names, const char *name, bool *stop) {
    *stop = false;
    if (!strcmp(name, ".") || !strcmp(name, ".."))
        return true;
    if (job->report->entries == job->limits.entries) {
        job->report->limited = *stop = true;
        return true;
    }
    job->report->entries++;
    atomic_store(&job->progress_entries, job->report->entries);
    if (!usable_name(name)) {
        job->report->skipped++;
        return true;
    }
    size_t bytes = strlen(name) + 1;
    if (bytes > SEARCH_NAMES_MAX - job->names_live) {
        job->report->limited = *stop = true;
        return true;
    }
    if (names->count == names->cap) {
        unsigned next = names->cap ? names->cap * 2 : 32;
        if (next > SEARCH_ENTRIES_MAX)
            next = SEARCH_ENTRIES_MAX;
        char **p = realloc(names->items, (size_t)next * sizeof(*p));
        if (!p)
            return fail(job->error, sizeof(job->error), "Mémoire insuffisante pour la liste des fichiers.");
        names->items = p;
        names->cap = next;
    }
    char *copy = malloc(bytes);
    if (!copy)
        return fail(job->error, sizeof(job->error), "Mémoire insuffisante pour un nom de fichier.");
    memcpy(copy, name, bytes);
    names->items[names->count++] = copy;
    names->held += bytes;
    job->names_live += bytes;
    return true;
}
static int name_compare(const void *a, const void *b) {
    return strcmp(*(char *const *)a, *(char *const *)b);
}
static bool collect(FolderSearchJob *job, const char *path, Names *names, unsigned depth) {
#ifdef _WIN32
    char pattern[FS_PATH];
    wchar_t wide[FS_PATH];
    WIN32_FIND_DATAW data;
    int n = snprintf(pattern, sizeof(pattern), "%s/*", path);
    if (n < 1 || n >= FS_PATH ||
        !MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, pattern, -1, wide, FS_PATH))
        return fail(job->error, sizeof(job->error), "Chemin de recherche trop long.");
    HANDLE handle = FindFirstFileW(wide, &data);
    if (handle == INVALID_HANDLE_VALUE) {
        DWORD e = GetLastError();
        if (e == ERROR_FILE_NOT_FOUND &&
            MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wide, FS_PATH)) {
            DWORD attrs = GetFileAttributesW(wide);
            if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY))
                return true;
        }
        if (!depth)
            return fail(job->error, sizeof(job->error), "Dossier de recherche introuvable ou inaccessible.");
        job->report->skipped++;
        return true;
    }
    bool ok = true, stopped = false;
    do {
        if (atomic_load(&job->cancel)) {
            stopped = true;
            break;
        }
        char name[FS_PATH];
        if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, data.cFileName, -1, name, FS_PATH, NULL,
                                 NULL)) {
            if (job->report->entries == job->limits.entries) {
                job->report->limited = stopped = true;
                break;
            }
            job->report->entries++;
            job->report->skipped++;
            atomic_store(&job->progress_entries, job->report->entries);
            continue;
        }
        ok = name_add(job, names, name, &stopped);
        if (!ok || stopped)
            break;
    } while (FindNextFileW(handle, &data));
    DWORD terminal = GetLastError();
    FindClose(handle);
    if (ok && !stopped && terminal != ERROR_NO_MORE_FILES)
        ok = fail(job->error, sizeof(job->error), "Lecture du dossier interrompue.");
    return ok;
#else
    DIR *stream = opendir(path);
    if (!stream) {
        if (!depth)
            return fail(job->error, sizeof(job->error), strerror(errno));
        job->report->skipped++;
        return true;
    }
    bool ok = true;
    while (!atomic_load(&job->cancel)) {
        errno = 0;
        struct dirent *entry = readdir(stream);
        if (!entry) {
            if (errno == EINTR)
                continue;
            if (errno)
                ok = fail(job->error, sizeof(job->error), "Lecture du dossier interrompue.");
            break;
        }
        bool stop;
        if (!name_add(job, names, entry->d_name, &stop)) {
            ok = false;
            break;
        }
        if (stop)
            break;
    }
    closedir(stream);
    return ok;
#endif
}
static bool walk(FolderSearchJob *job, const char *path, const char *relative, unsigned depth,
                 bool included) {
    if (job->stop || atomic_load(&job->cancel))
        return true;
    if (!guard(job, path, job->error, sizeof(job->error)))
        return false;
    if (atomic_load(&job->cancel))
        return true;
    Names names = {0};
    bool ok = collect(job, path, &names, depth);
    if (ok)
        ok = guard(job, path, job->error, sizeof(job->error));
    if (ok && names.count > 1)
        qsort(names.items, names.count, sizeof(*names.items), name_compare);
    for (unsigned i = 0; ok && i < names.count && !job->stop && !atomic_load(&job->cancel); i++) {
        char child[FS_PATH], rel[FS_PATH], ignored_error[512];
        int n = snprintf(child, sizeof(child), "%s/%s", path, names.items[i]);
        int m = *relative ? snprintf(rel, sizeof(rel), "%s/%s", relative, names.items[i])
                          : snprintf(rel, sizeof(rel), "%s", names.items[i]);
        if (n < 1 || n >= FS_PATH || m < 1 || m >= FS_PATH ||
            !guard(job, child, ignored_error, sizeof(ignored_error))) {
            job->report->skipped++;
            continue;
        }
        if (atomic_load(&job->cancel))
            break;
        SearchStat info;
        bool link;
        if (!child_stat(child, &info, &link)) {
            job->report->skipped++;
            continue;
        }
        if (link)
            job->report->skipped++;
        else if (directory(&info)) {
            int excluded = search_matcher_filter(job->matcher, 3, rel, true, job->error, sizeof(job->error));
            if (excluded < 0) {
                ok = false;
                break;
            }
            if (ignored(names.items[i]) || !job->options.recursive || excluded)
                job->report->ignored++;
            else if (depth >= job->limits.depth || job->report->entries >= job->limits.entries)
                job->report->limited = true;
            else {
                int selected = included ? 1
                                        : search_matcher_filter(job->matcher, 2, rel, true, job->error,
                                                                sizeof(job->error));
                if (selected < 0)
                    ok = false;
                else
                    ok = walk(job, child, rel, depth + 1, selected != 0);
            }
        } else if (regular(&info)) {
            if (!included) {
                job->report->ignored++;
                continue;
            }
            int selected = search_matcher_filter(job->matcher, 0, rel, false, job->error, sizeof(job->error));
            int excluded = selected > 0 ? search_matcher_filter(job->matcher, 1, rel, false, job->error,
                                                                sizeof(job->error))
                                        : 0;
            if (selected < 0 || excluded < 0) {
                ok = false;
                break;
            }
            if (!included || !selected || excluded) {
                job->report->ignored++;
                continue;
            }
            job->report->examined++;
            progress(job);
            ok = read_file(job, child, rel, &info);
        } else
            job->report->skipped++;
    }
    names_free(job, &names);
    return ok;
}
static int file_compare(const void *a, const void *b) {
    return strcmp(((const SearchFile *)a)->relative, ((const SearchFile *)b)->relative);
}
SearchReport *folder_search_snapshot(FolderSearchJob *job, unsigned after) {
    if (!job || after >= SEARCH_FILES_MAX)
        return NULL;
    if (atomic_load(&job->progress_found) <= after && !atomic_load(&job->done))
        return NULL;
    if (!publication_try(job))
        return NULL;
    SearchReport *source = job->report ? job->report : job->partial_report, *view = NULL;
    if (!source || source->count <= after)
        goto done;
    view = calloc(1, sizeof(*view));
    if (!view)
        goto done;
    view->files = calloc(source->count, sizeof(*view->files));
    if (!view->files) {
        free(view);
        view = NULL;
        goto done;
    }
    strcpy(view->root, job->root);
    strcpy(view->query, job->query);
    view->options = job->options;
    view->count = source->count;
    for (unsigned i = 0; i < view->count; i++) {
        view->files[i] = source->files[i];
        SearchStorage *storage = view->files[i].storage;
        atomic_fetch_add_explicit(&storage->refs, 1, memory_order_relaxed);
        view->hit_count += view->files[i].count;
        view->snapshot_bytes += view->files[i].len;
    }
    view->entries = atomic_load(&job->progress_entries);
    view->examined = atomic_load(&job->progress_examined);
    view->read_bytes = atomic_load(&job->progress_bytes);
    view->skipped = atomic_load(&job->progress_skipped);
    view->ignored = atomic_load(&job->progress_ignored);
    view->limited = atomic_load(&job->progress_limited);
    view->cancelled = atomic_load(&job->cancel);
    view->complete = source->complete;
done:
    publication_unlock(job);
    if (view && view->count > 1)
        qsort(view->files, view->count, sizeof(*view->files), file_compare);
    return view;
}
#ifdef _WIN32
static DWORD WINAPI work(void *arg)
#else
static void *work(void *arg)
#endif
{
    FolderSearchJob *job = arg;
    job->matcher =
        search_matcher_new(job->query, &job->options, &job->cancel, job->error, sizeof(job->error));
    int included =
        job->matcher ? search_matcher_filter(job->matcher, 2, ".", true, job->error, sizeof(job->error)) : -1;
    bool ok = included >= 0 && walk(job, job->root, "", 0, included != 0);
    search_matcher_free(job->matcher);
    job->matcher = NULL;
    if (ok)
        ok = guard(job, job->root, job->error, sizeof(job->error));
    publication_lock(job);
    SearchReport *r = job->report;
    if (r->count > 1)
        qsort(r->files, r->count, sizeof(*r->files), file_compare);
    r->cancelled = atomic_load(&job->cancel);
    r->complete = ok;
    progress(job);
    if (!ok) {
        job->partial_report = job->report;
        job->report = NULL;
    }
    publication_unlock(job);
    atomic_store(&job->done, true);
    return 0;
}
FolderSearchJob *folder_search_start_options(const char *root, const char *query, const SearchLimits *limits,
                                             const SearchOptions *options, FsGuard guard_fn, void *user,
                                             char *error, size_t cap) {
    if (error && cap)
        *error = 0;
    SearchLimits chosen = limits ? *limits : folder_search_limits();
    SearchOptions selected = options ? *options : folder_search_options();
    if (!folder_search_options_valid(&selected, error, cap))
        return NULL;
    if (!valid_path(root) || !folder_search_query_valid(query) || !limits_valid(&chosen)) {
        fail(error, cap, "Dossier absolu, texte ou limites de recherche invalides.");
        return NULL;
    }
    FolderSearchJob *job = calloc(1, sizeof(*job));
    if (!job) {
        fail(error, cap, "Mémoire insuffisante pour la recherche.");
        return NULL;
    }
    atomic_init(&job->done, false);
    atomic_init(&job->cancel, false);
    atomic_init(&job->progress_entries, 0);
    atomic_init(&job->progress_examined, 0);
    atomic_init(&job->progress_found, 0);
    atomic_init(&job->progress_hits, 0);
    atomic_init(&job->progress_bytes, 0);
    atomic_init(&job->progress_skipped, 0);
    atomic_init(&job->progress_ignored, 0);
    atomic_init(&job->progress_limited, false);
    job->publication = publication_new();
    if (!job->publication) {
        free(job);
        fail(error, cap, "Impossible d’initialiser la publication des résultats.");
        return NULL;
    }
    job->limits = chosen;
    job->options = selected;
    job->guard = guard_fn;
    job->guard_context = user;
    strcpy(job->root, root);
    strcpy(job->query, query);
    job->report = calloc(1, sizeof(*job->report));
    if (job->report)
        job->report->files = calloc(chosen.files, sizeof(*job->report->files));
    if (!job->report || !job->report->files) {
        folder_search_report_free(job->report);
        publication_free(job->publication);
        free(job);
        fail(error, cap, "Mémoire insuffisante pour les résultats.");
        return NULL;
    }
    strcpy(job->report->root, root);
    strcpy(job->report->query, query);
    job->report->options = selected;
#ifdef _WIN32
    job->thread = CreateThread(NULL, 0, work, job, 0, NULL);
    bool started = job->thread != NULL;
#else
    pthread_t *thread = malloc(sizeof(*thread));
    bool started = thread && !pthread_create(thread, NULL, work, job);
    if (started)
        job->thread = thread;
    else
        free(thread);
#endif
    if (!started) {
        folder_search_report_free(job->report);
        publication_free(job->publication);
        free(job);
        fail(error, cap, "Impossible de créer le worker de recherche.");
        return NULL;
    }
    return job;
}
FolderSearchJob *folder_search_start(const char *root, const char *query, const SearchLimits *limits,
                                     FsGuard guard_fn, void *user, char *error, size_t cap) {
    return folder_search_start_options(root, query, limits, NULL, guard_fn, user, error, cap);
}
void folder_search_cancel(FolderSearchJob *job) {
    if (job)
        atomic_store(&job->cancel, true);
}
void folder_search_free(FolderSearchJob *job) {
    if (!job)
        return;
    folder_search_cancel(job);
#ifdef _WIN32
    if (job->thread) {
        WaitForSingleObject(job->thread, INFINITE);
        CloseHandle(job->thread);
    }
#else
    if (job->thread) {
        pthread_join(*(pthread_t *)job->thread, NULL);
        free(job->thread);
    }
#endif
    folder_search_report_free(job->report);
    folder_search_report_free(job->partial_report);
    publication_free(job->publication);
    free(job);
}
