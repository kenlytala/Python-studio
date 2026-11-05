#define _POSIX_C_SOURCE 200809L
#include "file_job.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <pthread.h>
#endif
static bool progress(void *user, unsigned long long bytes, bool entry) {
    FileJob *job = user;
    atomic_fetch_add(&job->bytes, bytes);
    if (entry)
        atomic_fetch_add(&job->entries, 1);
    return job->deleting || !atomic_load(&job->cancel);
}
#ifdef _WIN32
static DWORD WINAPI work(void *user)
#else
static void *work(void *user)
#endif
{
    FileJob *job = user;
    fs_set_guard(job->guard, job->guard_context);
    if (job->deleting)
        job->success = fs_remove(job->root, job->target, progress, job, job->error, sizeof(job->error));
    else
        job->success = fs_paste(job->root, job->parent, &job->sources, progress, job, job->created,
                                job->error, sizeof(job->error));
    atomic_store(&job->done, true);
    return 0;
}
FileJob *file_job_start_guarded(const char *root, const char *parent, const char *target,
                                const FsPaths *sources, FsGuard guard, void *context, char *error,
                                size_t cap) {
    if (!root || strlen(root) >= FS_PATH || (parent && strlen(parent) >= FS_PATH) ||
        (target && strlen(target) >= FS_PATH))
        return NULL;
    FileJob *job = calloc(1, sizeof(*job));
    if (!job) {
        snprintf(error, cap, "Mémoire insuffisante.");
        return NULL;
    }
    atomic_init(&job->done, false);
    atomic_init(&job->cancel, false);
    atomic_init(&job->bytes, 0);
    atomic_init(&job->entries, 0);
    strcpy(job->root, root);
    if (parent)
        strcpy(job->parent, parent);
    if (target)
        strcpy(job->target, target);
    if (sources)
        job->sources = *sources;
    job->deleting = target && *target;
    job->guard = guard;
    job->guard_context = context;
#ifdef _WIN32
    job->thread = CreateThread(NULL, 16 * 1024 * 1024, work, job, 0, NULL);
    bool ok = job->thread != NULL;
#else
    pthread_t *thread = malloc(sizeof(*thread));
    job->thread = thread;
    /* macOS defaults to a small worker stack: bound recursion must not overflow it. */
    pthread_attr_t attributes;
    bool ok = false;
    if (thread && pthread_attr_init(&attributes) == 0) {
        if (pthread_attr_setstacksize(&attributes, 16 * 1024 * 1024) == 0)
            ok = pthread_create(thread, &attributes, work, job) == 0;
        pthread_attr_destroy(&attributes);
    }
#endif
    if (!ok) {
        snprintf(error, cap, "Impossible de démarrer l’opération de fichiers.");
#ifndef _WIN32
        free(job->thread);
#endif
        free(job);
        return NULL;
    }
    return job;
}
void file_job_free(FileJob *job) {
    if (!job)
        return;
    atomic_store(&job->cancel, true);
#ifdef _WIN32
    WaitForSingleObject(job->thread, INFINITE);
    CloseHandle(job->thread);
#else
    pthread_join(*(pthread_t *)job->thread, NULL);
    free(job->thread);
#endif
    free(job);
}

FileJob *file_job_start(const char *root, const char *parent, const char *target, const FsPaths *sources,
                        char *error, size_t cap) {
    return file_job_start_guarded(root, parent, target, sources, NULL, NULL, error, cap);
}
