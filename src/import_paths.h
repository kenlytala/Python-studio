#ifndef PRISME_IMPORT_PATHS_H
#define PRISME_IMPORT_PATHS_H
#include "buffer.h"

#define IMPORT_PATHS_MAX 8

typedef struct {
    int count;
    char paths[IMPORT_PATHS_MAX][BUF_PATH];
} ImportPaths;

/* Paths are absolute UTF-8. No environment-variable or separator expansion. */
bool import_paths_valid(const ImportPaths *paths);
bool import_paths_add(ImportPaths *paths, const char *path, char *error, size_t cap);
bool import_paths_remove(ImportPaths *paths, int index);
/* A separate profile in the preferences directory, bound to the exact project. */
bool import_paths_file(char *out, size_t cap, const char *settings, const char *project);
/* Transactional load; a missing file returns false with an empty error. */
bool import_paths_load(ImportPaths *out, const char *file, const char *project, char *error, size_t cap);
bool import_paths_save(const ImportPaths *paths, const char *file, const char *project, char *error,
                       size_t cap);
#endif
