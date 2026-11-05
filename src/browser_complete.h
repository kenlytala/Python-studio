#ifndef PYTHON_STUDIO_BROWSER_COMPLETE_H
#define PYTHON_STUDIO_BROWSER_COMPLETE_H
#include "files.h"
#include <stdatomic.h>
#define BROWSER_SCAN_MAX 8192
#define BROWSER_CHOICES_MAX 256
typedef struct {
    char path[FS_PATH];
    bool directory;
} BrowserChoice;
typedef struct {
    atomic_bool done, cancel;
    char text[FS_PATH], base[FS_PATH], home[FS_PATH], directory[FS_PATH], error[512];
    size_t cursor;
    bool directories_only, limited, success;
    unsigned scanned, skipped, count;
    BrowserChoice items[BROWSER_CHOICES_MAX];
    FsGuard guard;
    void *guard_context, *thread;
} BrowserLoadJob;
BrowserLoadJob *browser_job_start(const char *text, size_t cursor, const char *base, const char *home,
                                  bool directories_only, FsGuard guard, void *user, char *error, size_t cap);
void browser_job_cancel(BrowserLoadJob *job);
void browser_job_free(BrowserLoadJob *job);
#endif
