#define _POSIX_C_SOURCE 200809L
#include "path_complete.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dirent.h>
#include <pthread.h>
#include <sys/stat.h>
#endif

static bool fail(char *error, size_t cap, const char *text) {
    if (error && cap)
        snprintf(error, cap, "%s", text);
    return false;
}
static bool separator(unsigned char c) {
    return c == '/'
#ifdef _WIN32
           || c == '\\'
#endif
        ;
}
static bool delimiter(unsigned char c) {
    return c < 128 && (isspace(c) || strchr("\"'`<>=()[]{};,|", c));
}
static bool usable_name(const char *name, char quote) {
    size_t n = 0;
    while (n < FS_PATH && name[n]) {
        unsigned char c = (unsigned char)name[n++];
        if (c < 32 || c == 127 || separator(c) || (quote && c == (unsigned char)quote))
            return false;
    }
    return n && n < FS_PATH - 1 && strcmp(name, ".") && strcmp(name, "..") && utf8_valid(name, n) &&
           !(strchr(name, '\'') && strchr(name, '"'));
}
bool path_completion_context(const char *text, size_t len, size_t cursor, PathContext *out) {
    if (!text || !out || cursor > len || !utf8_valid(text, len) ||
        (cursor < len && ((unsigned char)text[cursor] & 0xc0) == 0x80))
        return false;
    size_t left = cursor, right = cursor;
    while (left && text[left - 1] != '\n')
        left--;
    while (right < len && text[right] != '\n')
        right++;
    size_t start = cursor, end = cursor, point = cursor;
    char quote = 0;
    for (size_t i = left; i < right; i++) {
        if (text[i] != '\'' && text[i] != '"')
            continue;
        size_t stop = i + 1;
        while (stop < right && text[stop] != text[i])
            stop++;
        bool closed = stop < right;
        if (i < cursor && cursor <= stop + (closed ? 1u : 0u)) {
            start = i + 1;
            end = stop;
            point = cursor < stop ? cursor : stop;
            quote = text[i];
            break;
        }
        i = closed ? stop : right;
    }
    if (!quote) {
        while (start > left && !delimiter((unsigned char)text[start - 1]))
            start--;
        while (end < right && !delimiter((unsigned char)text[end]))
            end++;
        /* A slash in markup (<tag / or </tag) is never a directory request. */
        bool markup = false;
        for (size_t i = left; i < cursor; i++) {
            if (text[i] == '<')
                markup = true;
            else if (text[i] == '>')
                markup = false;
        }
        if (markup)
            return false;
    }
    if (end <= start || end - start >= FS_PATH)
        return false;
    for (size_t i = start; i < end; i++)
        if ((unsigned char)text[i] < 32 || text[i] == 127)
            return false;
    for (size_t i = start; i + 2 < end; i++)
        if (!memcmp(text + i, "://", 3))
            return false;
    size_t component = start;
    bool slash = false;
    for (size_t i = start; i < point; i++)
        if (separator((unsigned char)text[i])) {
            component = i + 1;
            slash = true;
        }
    if (!slash)
        return false;
    size_t stop = point;
    while (stop < end && !separator((unsigned char)text[stop]))
        stop++;
    memset(out, 0, sizeof(*out));
    out->start = start;
    out->end = end;
    out->component = component;
    out->component_end = stop;
    out->quote = quote;
    memcpy(out->directory, text + start, component - start);
    out->directory[component - start] = 0;
    memcpy(out->prefix, text + component, point - component);
    out->prefix[point - component] = 0;
    return true;
}
static bool needs_quote(const char *text) {
    for (size_t i = 0; text[i]; i++) {
        unsigned char c = (unsigned char)text[i];
        if (delimiter(c))
            return true;
        /* Unicode whitespace that is also accepted inside quoted paths. */
        if ((c == 0xc2 && ((unsigned char)text[i + 1] == 0x85 || (unsigned char)text[i + 1] == 0xa0)) ||
            (c == 0xe1 && (unsigned char)text[i + 1] == 0x9a && (unsigned char)text[i + 2] == 0x80) ||
            (c == 0xe2 && (unsigned char)text[i + 1] == 0x80 &&
             (((unsigned char)text[i + 2] >= 0x80 && (unsigned char)text[i + 2] <= 0x8a) ||
              (unsigned char)text[i + 2] == 0xa8 || (unsigned char)text[i + 2] == 0xa9 ||
              (unsigned char)text[i + 2] == 0xaf)) ||
            (c == 0xe2 && (unsigned char)text[i + 1] == 0x81 && (unsigned char)text[i + 2] == 0x9f) ||
            (c == 0xe3 && (unsigned char)text[i + 1] == 0x80 && (unsigned char)text[i + 2] == 0x80))
            return true;
    }
    return false;
}
bool path_choice_edit(const char *text, size_t len, const PathContext *ctx, const PathChoice *choice,
                      XmlEdit *out, char *error, size_t cap) {
    if (error && cap)
        *error = 0;
    if (!text || !ctx || !choice || !out || ctx->start > ctx->component ||
        ctx->component > ctx->component_end || ctx->component_end > ctx->end || ctx->end > len ||
        (ctx->quote && ctx->quote != '\'' && ctx->quote != '"') ||
        !memchr(choice->name, 0, sizeof(choice->name)) || !usable_name(choice->name, ctx->quote) ||
        !utf8_valid(text, len) || (ctx->start < len && ((unsigned char)text[ctx->start] & 0xc0) == 0x80) ||
        (ctx->component < len && ((unsigned char)text[ctx->component] & 0xc0) == 0x80) ||
        (ctx->component_end < len && ((unsigned char)text[ctx->component_end] & 0xc0) == 0x80) ||
        (ctx->end < len && ((unsigned char)text[ctx->end] & 0xc0) == 0x80))
        return fail(error, cap, "Proposition de chemin invalide.");
    if (ctx->component_end < ctx->end && !choice->directory)
        return fail(error, cap, "Un composant parent doit rester un dossier.");
    size_t end = ctx->component_end, n = strlen(choice->name);
    bool slash = choice->directory;
    if (slash && end < ctx->end && separator((unsigned char)text[end]))
        end++;
    size_t prefix = ctx->component - ctx->start, suffix = ctx->end - end;
    if (prefix + n + (slash ? 1u : 0u) + suffix >= FS_PATH)
        return fail(error, cap, "Chemin complété trop long (4095 octets maximum).");
    bool wrap = !ctx->quote && needs_quote(choice->name);
    size_t size = wrap ? prefix + n + (slash ? 1u : 0u) + suffix + 2 : n + (slash ? 1u : 0u);
    char *replacement = malloc(size + 1);
    if (!replacement)
        return fail(error, cap, "Mémoire insuffisante.");
    size_t used = 0;
    if (wrap) {
        char quote = '"';
        if (memchr(text + ctx->start, '"', prefix) || strchr(choice->name, '"') ||
            memchr(text + end, '"', suffix))
            quote = '\'';
        if (quote == '\'' && (memchr(text + ctx->start, '\'', prefix) || strchr(choice->name, '\'') ||
                              memchr(text + end, '\'', suffix))) {
            free(replacement);
            return fail(error, cap, "Le chemin contient les deux guillemets ; saisissez-le explicitement.");
        }
        replacement[used++] = quote;
        memcpy(replacement + used, text + ctx->start, prefix);
        used += prefix;
        memcpy(replacement + used, choice->name, n);
        used += n;
        if (slash)
            replacement[used++] = '/';
        memcpy(replacement + used, text + end, suffix);
        used += suffix;
        replacement[used++] = quote;
    } else {
        memcpy(replacement, choice->name, n);
        used = n;
        if (slash)
            replacement[used++] = '/';
    }
    replacement[used] = 0;
    *out = (XmlEdit){.start = wrap ? ctx->start : ctx->component,
                     .end = wrap ? ctx->end : end,
                     .text = replacement,
                     .cursor = ctx->component + n + (slash ? 1u : 0u) + (wrap ? 1u : 0u)};
    return true;
}
static int compare(const PathChoice *a, const PathChoice *b) {
    if (a->directory != b->directory)
        return a->directory ? -1 : 1;
    return strcmp(a->name, b->name);
}
static void offer(PathLoadJob *job, const char *parent, const char *name, bool directory) {
    PathReport *r = &job->report;
    for (int i = 0; i < r->count; i++)
        if (r->items[i].directory == directory && !strcmp(r->items[i].name, name))
            return;
    PathChoice item = {.directory = directory};
    strcpy(item.name, name);
    strcpy(item.parent, parent);
    int at = 0;
    while (at < r->count && compare(&r->items[at], &item) < 0)
        at++;
    if (r->count == PATH_CHOICES_MAX)
        r->limited = true;
    if (at == PATH_CHOICES_MAX)
        return;
    if (r->count < PATH_CHOICES_MAX)
        r->count++;
    for (int i = r->count - 1; i > at; i--)
        r->items[i] = r->items[i - 1];
    r->items[at] = item;
}
static void entry(PathLoadJob *job, const char *parent, const char *name) {
    PathReport *r = &job->report;
    if (strncmp(name, job->context.prefix, strlen(job->context.prefix)))
        return;
    if (!usable_name(name, job->context.quote)) {
        r->skipped++;
        return;
    }
    char path[FS_PATH], ignored[512];
    int n = snprintf(path, sizeof(path), "%s/%s", parent, name);
    if (n < 1 || n >= FS_PATH) {
        r->skipped++;
        return;
    }
    if (job->guard && !job->guard(job->guard_context, path, false, ignored, sizeof(ignored))) {
        r->skipped++;
        return;
    }
    if (atomic_load(&job->cancel))
        return;
#ifdef _WIN32
    wchar_t wide[FS_PATH];
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wide, FS_PATH)) {
        r->skipped++;
        return;
    }
    DWORD attributes = GetFileAttributesW(wide);
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DEVICE)) {
        r->skipped++;
        return;
    }
    bool directory = (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
    struct stat info;
    if (stat(path, &info) || (!S_ISREG(info.st_mode) && !S_ISDIR(info.st_mode))) {
        r->skipped++;
        return;
    }
    bool directory = S_ISDIR(info.st_mode);
#endif
    if (job->context.component_end < job->context.end && !directory)
        return;
    offer(job, parent, name, directory);
}
static void load(PathLoadJob *job) {
    for (int i = 0; i < job->path_count && !atomic_load(&job->cancel); i++) {
        const char *parent = job->paths[i];
        if (job->guard && !job->guard(job->guard_context, parent, false, job->error, sizeof(job->error)))
            return;
        if (atomic_load(&job->cancel))
            return;
#ifdef _WIN32
        char pattern[FS_PATH];
        wchar_t wide[FS_PATH];
        WIN32_FIND_DATAW data;
        int n = snprintf(pattern, sizeof(pattern), "%s/*", parent);
        if (n < 1 || n >= FS_PATH ||
            !MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, pattern, -1, wide, FS_PATH)) {
            fail(job->error, sizeof(job->error), "Chemin de dossier trop long ou invalide.");
            return;
        }
        HANDLE handle = FindFirstFileW(wide, &data);
        if (handle == INVALID_HANDLE_VALUE) {
            DWORD e = GetLastError();
            if (e == ERROR_FILE_NOT_FOUND) {
                if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, parent, -1, wide, FS_PATH))
                    return;
                DWORD attributes = GetFileAttributesW(wide);
                if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    job->report.directories++;
                    if (job->guard &&
                        !job->guard(job->guard_context, parent, false, job->error, sizeof(job->error)))
                        return;
                    continue;
                }
                e = GetLastError();
            }
            if (e == ERROR_PATH_NOT_FOUND || e == ERROR_DIRECTORY || e == ERROR_FILE_NOT_FOUND)
                continue;
            fail(job->error, sizeof(job->error), "Lecture du dossier impossible.");
            return;
        }
        job->report.directories++;
        bool budget_stop = false;
        do {
            if (atomic_load(&job->cancel))
                break;
            char name[FS_PATH];
            if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, data.cFileName, -1, name, FS_PATH, NULL,
                                     NULL)) {
                job->report.skipped++;
                continue;
            }
            if (!strcmp(name, ".") || !strcmp(name, ".."))
                continue;
            if (job->report.scanned == PATH_SCAN_MAX) {
                job->report.limited = true;
                budget_stop = true;
                break;
            }
            job->report.scanned++;
            entry(job, parent, name);
        } while (FindNextFileW(handle, &data));
        DWORD terminal = GetLastError();
        FindClose(handle);
        if (!atomic_load(&job->cancel) && !budget_stop && terminal != ERROR_NO_MORE_FILES) {
            fail(job->error, sizeof(job->error), "Lecture du dossier interrompue.");
            return;
        }
#else
        DIR *dir = opendir(parent);
        if (!dir) {
            if (errno == ENOENT || errno == ENOTDIR)
                continue;
            snprintf(job->error, sizeof(job->error), "Lecture du dossier impossible : %s", strerror(errno));
            return;
        }
        job->report.directories++;
        bool ok = true;
        for (;;) {
            if (atomic_load(&job->cancel))
                break;
            errno = 0;
            struct dirent *item = readdir(dir);
            if (!item) {
                if (errno == EINTR)
                    continue;
                if (errno) {
                    fail(job->error, sizeof(job->error), "Lecture du dossier interrompue.");
                    ok = false;
                }
                break;
            }
            if (!strcmp(item->d_name, ".") || !strcmp(item->d_name, ".."))
                continue;
            if (job->report.scanned == PATH_SCAN_MAX) {
                job->report.limited = true;
                break;
            }
            job->report.scanned++;
            entry(job, parent, item->d_name);
        }
        closedir(dir);
        if (!ok)
            return;
#endif
        if (atomic_load(&job->cancel))
            return;
        if (job->guard && !job->guard(job->guard_context, parent, false, job->error, sizeof(job->error)))
            return;
        if (job->report.scanned == PATH_SCAN_MAX) {
            job->report.limited = true;
            break;
        }
    }
    if (atomic_load(&job->cancel))
        return;
    if (!job->report.directories) {
        fail(job->error, sizeof(job->error),
             "Dossier introuvable dans les bases XML, le projet, le dossier du document ou les imports.");
        return;
    }
    job->success = true;
}
#ifdef _WIN32
static DWORD WINAPI work(void *arg)
#else
static void *work(void *arg)
#endif
{
    PathLoadJob *job = arg;
    load(job);
    atomic_store(&job->done, true);
    return 0;
}
PathLoadJob *path_job_start(const char paths[][FS_PATH], int count, const PathContext *context, FsGuard guard,
                            void *user, char *error, size_t cap) {
    if (!paths || !context || count < 1 || count > PATH_ROOTS_MAX ||
        (context->quote && context->quote != '\'' && context->quote != '"') ||
        context->start > context->component || context->component > context->component_end ||
        context->component_end > context->end || context->end > BUF_LIMIT ||
        !memchr(context->directory, 0, sizeof(context->directory)) ||
        !memchr(context->prefix, 0, sizeof(context->prefix)) ||
        !utf8_valid(context->directory, strlen(context->directory)) ||
        !utf8_valid(context->prefix, strlen(context->prefix))) {
        fail(error, cap, "Bases de chemins invalides (18 maximum).");
        return NULL;
    }
    PathLoadJob *job = calloc(1, sizeof(*job));
    if (!job) {
        fail(error, cap, "Mémoire insuffisante.");
        return NULL;
    }
    atomic_init(&job->cancel, false);
    atomic_init(&job->done, false);
    job->context = *context;
    job->guard = guard;
    job->guard_context = user;
    for (int i = 0; i < count; i++) {
        size_t n = 0;
        while (n < FS_PATH && paths[i][n])
            n++;
        if (!n || n == FS_PATH || !utf8_valid(paths[i], n)) {
            free(job);
            fail(error, cap, "Chemin de dossier invalide.");
            return NULL;
        }
        bool duplicate = false;
        for (int j = 0; j < job->path_count; j++)
            if (!strcmp(job->paths[j], paths[i]))
                duplicate = true;
        if (!duplicate)
            strcpy(job->paths[job->path_count++], paths[i]);
    }
#ifdef _WIN32
    job->thread = CreateThread(NULL, 0, work, job, 0, NULL);
    bool ok = job->thread != NULL;
#else
    pthread_t *thread = malloc(sizeof(*thread));
    job->thread = thread;
    bool ok = thread && !pthread_create(thread, NULL, work, job);
#endif
    if (!ok) {
#ifndef _WIN32
        free(job->thread);
#endif
        free(job);
        fail(error, cap, "Impossible de démarrer la lecture du dossier.");
        return NULL;
    }
    return job;
}
void path_job_cancel(PathLoadJob *job) {
    if (job)
        atomic_store(&job->cancel, true);
}
void path_job_free(PathLoadJob *job) {
    if (!job)
        return;
    path_job_cancel(job);
#ifdef _WIN32
    WaitForSingleObject(job->thread, INFINITE);
    CloseHandle(job->thread);
#else
    pthread_join(*(pthread_t *)job->thread, NULL);
    free(job->thread);
#endif
    free(job);
}
