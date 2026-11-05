#ifndef PRISME_FILE_JOB_H
#define PRISME_FILE_JOB_H
#include "files.h"
#include <stdatomic.h>
typedef struct {
    atomic_bool done, cancel;
    atomic_ullong bytes, entries;
    bool deleting, success;
    char root[FS_PATH], parent[FS_PATH], target[FS_PATH], created[FS_PATH], error[512];
    FsPaths sources;
    void *thread;
    FsGuard guard;
    void *guard_context;
} FileJob;
FileJob *file_job_start_guarded(const char *root, const char *parent, const char *target,
                                const FsPaths *sources, FsGuard guard, void *context, char *error,
                                size_t cap);
FileJob *file_job_start(const char *root, const char *parent, const char *target, const FsPaths *sources,
                        char *error, size_t cap);
void file_job_free(FileJob *job);
#endif
