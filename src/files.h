#ifndef PRISME_FILES_H
#define PRISME_FILES_H
#include <stdbool.h>
#include <stddef.h>

#define FS_PATH 4096
#define FS_MAX_PATHS 64
#define FS_CLIP_LIMIT (512 * 1024)
typedef struct {
    int count;
    char paths[FS_MAX_PATHS][FS_PATH];
} FsPaths;
typedef enum { FS_MISSING, FS_FILE, FS_DIRECTORY, FS_LINK, FS_OTHER } FsKind;
/* Return false from progress to cancel a copy. Deletion is not cancellable. */
typedef bool (*FsProgress)(void *user, unsigned long long bytes, bool entry);
typedef bool (*FsGuard)(void *context, const char *path, bool allow_leaf_link, char *error, size_t cap);
/* Per-thread policy, set before an operation; workers carry their own policy. */
void fs_set_guard(FsGuard guard, void *context);
FsKind fs_kind(const char *path);
/* Native separators: on POSIX a backslash is a filename character, not '/'. */
const char *fs_basename(const char *path);
bool fs_parent(const char *path, char *out); /* out: FS_PATH bytes; absolute paths */
bool fs_under(const char *parent, const char *path);
bool fs_name_valid(const char *name, char *error, size_t cap);
bool fs_create(const char *root, const char *parent, const char *name, bool directory, char *created,
               char *error, size_t cap);
bool fs_remove(const char *root, const char *path, FsProgress progress, void *user, char *error, size_t cap);
bool fs_paste(const char *root, const char *parent, const FsPaths *sources, FsProgress progress, void *user,
              char *first_created, char *error, size_t cap);
/* Clipboard formats: local file:// URIs, GNOME copied-files, absolute paths.
   Parser returns -1 for a clipboard exceeding the item/size limit (no partial paste). */
int fs_paths_parse(FsPaths *paths, const char *text);
bool fs_paths_text(const FsPaths *paths, char *out, size_t cap);
#endif
