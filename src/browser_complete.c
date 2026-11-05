#define _POSIX_C_SOURCE 200809L
#include "browser_complete.h"
#include "buffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dirent.h>
#include <pthread.h>
#include <sys/stat.h>
#endif
static bool fail(char *error, size_t cap, const char *text) {
    if (cap)
        snprintf(error, cap, "%s", text);
    return false;
}
static bool slash(char c) {
    return c == '/'
#ifdef _WIN32
           || c == '\\'
#endif
        ;
}
static bool absolute(const char *s) {
    return slash(*s)
#ifdef _WIN32
           || (s[0] && s[1] == ':' && slash(s[2]))
#endif
        ;
}
static bool valid(const char *s) {
    if (!s)
        return false;
    size_t n = 0;
    while (n < FS_PATH && s[n])
        n++;
    if (n == FS_PATH || !utf8_valid(s, n))
        return false;
    for (size_t i = 0; i < n; i++)
        if ((unsigned char)s[i] < 32 || (unsigned char)s[i] == 127)
            return false;
    return true;
}
static bool expanded(BrowserLoadJob *job, const char *text, char out[FS_PATH]) {
    const char *base = job->base, *leaf = text;
    if (text[0] == '~' && (!text[1] || slash(text[1]))) {
        if (!*job->home)
            return false;
        base = job->home;
        leaf = text[1] ? text + 2 : "";
    } else if (absolute(text)) {
        if (strlen(text) >= FS_PATH)
            return false;
        strcpy(out, text);
        return true;
    }
    int n = snprintf(out, FS_PATH, "%s%s%s", base,
                     *base && !slash(base[strlen(base) - 1]) && *leaf ? "/" : "", leaf);
    return n >= 0 && n < FS_PATH;
}
static bool guard(BrowserLoadJob *job, const char *path) {
    return !job->guard || job->guard(job->guard_context, path, false, job->error, sizeof(job->error));
}
static int directory_kind(const char *path) {
#ifdef _WIN32
    wchar_t w[FS_PATH];
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, w, FS_PATH))
        return -1;
    DWORD attr = GetFileAttributesW(w);
    if (attr == INVALID_FILE_ATTRIBUTES)
        return -1;
    return (attr & FILE_ATTRIBUTE_DIRECTORY) ? 1 : (attr & FILE_ATTRIBUTE_DEVICE) ? -1 : 0;
#else
    struct stat info;
    if (stat(path, &info))
        return -1;
    return S_ISDIR(info.st_mode) ? 1 : S_ISREG(info.st_mode) ? 0 : -1;
#endif
}
static int compare(const BrowserChoice *a, const BrowserChoice *b) {
    if (a->directory != b->directory)
        return a->directory ? -1 : 1;
    return strcmp(fs_basename(a->path), fs_basename(b->path));
}
static void offer(BrowserLoadJob *job, const char *name, const char *prefix) {
    if (strncmp(name, prefix, strlen(prefix)) || !strcmp(name, ".") || !strcmp(name, ".."))
        return;
    if (!valid(name)) {
        job->skipped++;
        return;
    }
    BrowserChoice next = {0};
    int n = snprintf(next.path, sizeof(next.path), "%s%s%s", job->directory,
                     slash(job->directory[strlen(job->directory) - 1]) ? "" : "/", name);
    if (n < 0 || n >= FS_PATH) {
        job->skipped++;
        return;
    }
    char entry_error[512] = {0};
    if (job->guard && !job->guard(job->guard_context, next.path, false, entry_error, sizeof(entry_error))) {
        job->skipped++;
        return;
    }
    int kind = directory_kind(next.path);
    if (kind < 0 || (job->directories_only && !kind))
        return;
    next.directory = kind == 1;
    unsigned at = 0;
    while (at < job->count && compare(&job->items[at], &next) < 0)
        at++;
    if (job->count == BROWSER_CHOICES_MAX) {
        job->limited = true;
        if (at == BROWSER_CHOICES_MAX)
            return;
    } else
        job->count++;
    for (unsigned i = job->count - 1; i > at; i--)
        job->items[i] = job->items[i - 1];
    job->items[at] = next;
}
static void load(BrowserLoadJob *job) {
    char candidate[FS_PATH], prefix[FS_PATH] = "", left[FS_PATH];
    if (!expanded(job, job->text, candidate) || !guard(job, candidate))
        return;
    if (job->cursor == strlen(job->text) && directory_kind(candidate) == 1)
        strcpy(job->directory, candidate);
    else {
        size_t split = job->cursor;
        while (split && !slash(job->text[split - 1]))
            split--;
        memcpy(prefix, job->text + split, job->cursor - split);
        prefix[job->cursor - split] = 0;
        memcpy(left, job->text, split);
        left[split] = 0;
        if (!expanded(job, left, job->directory))
            return;
    }
    if (!guard(job, job->directory) || atomic_load(&job->cancel))
        return;
#ifdef _WIN32
    char pattern[FS_PATH];
    wchar_t wide[FS_PATH];
    WIN32_FIND_DATAW data;
    int n = snprintf(pattern, sizeof(pattern), "%s/*", job->directory);
    if (n < 0 || n >= FS_PATH ||
        !MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, pattern, -1, wide, FS_PATH)) {
        fail(job->error, sizeof(job->error), "Chemin invalide.");
        return;
    }
    HANDLE handle = FindFirstFileW(wide, &data);
    if (handle == INVALID_HANDLE_VALUE) {
        if (GetLastError() == ERROR_FILE_NOT_FOUND && directory_kind(job->directory) == 1) {
            job->success = true;
            return;
        }
        fail(job->error, sizeof(job->error), "Lecture du dossier impossible.");
        return;
    }
    do {
        if (atomic_load(&job->cancel))
            break;
        char name[FS_PATH];
        if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, data.cFileName, -1, name, FS_PATH, NULL,
                                 NULL)) {
            job->skipped++;
            continue;
        }
        if (!strcmp(name, ".") || !strcmp(name, ".."))
            continue;
        if (job->scanned == BROWSER_SCAN_MAX) {
            job->limited = true;
            break;
        }
        job->scanned++;
        offer(job, name, prefix);
    } while (FindNextFileW(handle, &data));
    DWORD terminal = GetLastError();
    FindClose(handle);
    if (!job->limited && !atomic_load(&job->cancel) && terminal != ERROR_NO_MORE_FILES) {
        fail(job->error, sizeof(job->error), "Lecture du dossier interrompue.");
        return;
    }
#else
    DIR *dir = opendir(job->directory);
    if (!dir) {
        snprintf(job->error, sizeof(job->error), "Lecture du dossier impossible : %s", strerror(errno));
        return;
    }
    bool ok = true;
    for (;;) {
        if (atomic_load(&job->cancel))
            break;
        errno = 0;
        struct dirent *entry = readdir(dir);
        if (!entry) {
            if (errno == EINTR)
                continue;
            if (errno) {
                ok = false;
                fail(job->error, sizeof(job->error), "Lecture du dossier interrompue.");
            }
            break;
        }
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
            continue;
        if (job->scanned == BROWSER_SCAN_MAX) {
            job->limited = true;
            break;
        }
        job->scanned++;
        offer(job, entry->d_name, prefix);
    }
    closedir(dir);
    if (!ok)
        return;
#endif
    if (!atomic_load(&job->cancel) && guard(job, job->directory))
        job->success = true;
}
#ifdef _WIN32
static DWORD WINAPI work(void *user)
#else
static void *work(void *user)
#endif
{
    BrowserLoadJob *job = user;
    load(job);
    if (!job->success && !*job->error && !atomic_load(&job->cancel))
        fail(job->error, sizeof(job->error), "Lecture des suggestions impossible.");
    atomic_store(&job->done, true);
    return 0;
}
BrowserLoadJob *browser_job_start(const char *text, size_t cursor, const char *base, const char *home,
                                  bool only, FsGuard check, void *user, char *error, size_t cap) {
    if (!valid(text) || !valid(base) || !absolute(base) || !valid(home ? home : "") ||
        cursor > strlen(text) || (cursor < strlen(text) && ((unsigned char)text[cursor] & 192) == 128)) {
        fail(error, cap, "Chemin invalide ou trop long (4 095 octets).");
        return NULL;
    }
    BrowserLoadJob *job = calloc(1, sizeof(*job));
    if (!job) {
        fail(error, cap, "Mémoire insuffisante.");
        return NULL;
    }
    atomic_init(&job->done, false);
    atomic_init(&job->cancel, false);
    strcpy(job->text, text);
    strcpy(job->base, base);
    if (home)
        strcpy(job->home, home);
    job->cursor = cursor;
    job->directories_only = only;
    job->guard = check;
    job->guard_context = user;
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
        fail(error, cap, "Impossible de démarrer les suggestions.");
        return NULL;
    }
    return job;
}
void browser_job_cancel(BrowserLoadJob *job) {
    if (job)
        atomic_store(&job->cancel, true);
}
void browser_job_free(BrowserLoadJob *job) {
    if (!job)
        return;
    browser_job_cancel(job);
#ifdef _WIN32
    WaitForSingleObject(job->thread, INFINITE);
    CloseHandle(job->thread);
#else
    pthread_join(*(pthread_t *)job->thread, NULL);
    free(job->thread);
#endif
    free(job);
}
