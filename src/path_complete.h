#ifndef PYTHON_STUDIO_PATH_COMPLETE_H
#define PYTHON_STUDIO_PATH_COMPLETE_H
#include "files.h"
#include "xml_edit.h"
#include <stdatomic.h>

#define PATH_CHOICES_MAX 32
#define PATH_SCAN_MAX 8192
#define PATH_ROOTS_MAX 18

typedef struct {
    size_t start, end, component, component_end; /* UTF-8 byte boundaries. */
    char directory[FS_PATH], prefix[FS_PATH], quote;
} PathContext;
typedef struct {
    char name[FS_PATH], parent[FS_PATH];
    bool directory;
} PathChoice;
typedef struct {
    PathChoice items[PATH_CHOICES_MAX];
    int count, directories, skipped;
    unsigned scanned;
    bool limited;
} PathReport;
/* Pure lexical parsing/planning: never probes the filesystem. */
bool path_completion_context(const char *text, size_t len, size_t cursor, PathContext *out);
bool path_choice_edit(const char *text, size_t len, const PathContext *context, const PathChoice *choice,
                      XmlEdit *out, char *error, size_t cap);

typedef struct {
    atomic_bool done, cancel;
    bool success;
    int path_count;
    char paths[PATH_ROOTS_MAX][FS_PATH], error[512];
    PathContext context;
    PathReport report;
    FsGuard guard;
    void *guard_context, *thread;
} PathLoadJob;
/* Directories only, already expanded to absolute candidate paths by the caller.
   A single nonrecursive, read-only worker. Guard context outlives job_free. */
PathLoadJob *path_job_start(const char paths[][FS_PATH], int count, const PathContext *context, FsGuard guard,
                            void *guard_context, char *error, size_t cap);
void path_job_cancel(PathLoadJob *job);
void path_job_free(PathLoadJob *job); /* Joins: completed jobs or shutdown only. */
#endif
