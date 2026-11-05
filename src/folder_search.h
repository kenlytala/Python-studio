#ifndef PYTHON_STUDIO_FOLDER_SEARCH_H
#define PYTHON_STUDIO_FOLDER_SEARCH_H
#include "files.h"
#include <stdatomic.h>

#define SEARCH_QUERY_MAX 1024u
#define SEARCH_FILE_MAX (2u * 1024u * 1024u)
#define SEARCH_READ_MAX (64u * 1024u * 1024u)
#define SEARCH_SNAPSHOT_MAX (16u * 1024u * 1024u)
#define SEARCH_FILES_MAX 256u
#define SEARCH_HITS_MAX 16384u
#define SEARCH_ENTRIES_MAX 20000u
#define SEARCH_DEPTH_MAX 64u
#define SEARCH_LINES_MAX 65536u
#define SEARCH_FILTER_MAX 512u
#define SEARCH_PATTERN_MAX 256u
#define SEARCH_PATTERNS_MAX 16u
#define SEARCH_NAMES_MAX (4u * 1024u * 1024u)

typedef struct {
    bool case_sensitive, whole_word, regex, recursive;
    char include_files[SEARCH_FILTER_MAX + 1], exclude_files[SEARCH_FILTER_MAX + 1];
    char include_dirs[SEARCH_FILTER_MAX + 1], exclude_dirs[SEARCH_FILTER_MAX + 1];
} SearchOptions;
SearchOptions folder_search_options(void);
bool folder_search_options_equal(const SearchOptions *a, const SearchOptions *b);
typedef struct {
    size_t file_bytes, read_bytes, snapshot_bytes;
    unsigned files, hits, entries, depth, lines;
} SearchLimits;
typedef struct {
    size_t start, end;            /* UTF-8 bytes. */
    unsigned line, column, width; /* 0-based line; visual cells, tabs=4. */
} SearchHit;
typedef struct {
    char path[FS_PATH], relative[FS_PATH];
    char *text;
    size_t len;
    SearchHit *hits;
    unsigned count;
    void *storage; /* Internal shared immutable text/hits ownership, or NULL. */
} SearchFile;
typedef struct {
    char root[FS_PATH], query[SEARCH_QUERY_MAX + 1];
    SearchFile *files;
    unsigned count, hit_count, entries, examined, skipped, ignored;
    size_t read_bytes, snapshot_bytes;
    bool limited, cancelled, complete;
    SearchOptions options;
} SearchReport;
/* Lexical only: expand ~/ and relative paths against an absolute project. */
bool folder_search_root(char out[FS_PATH], const char *value, const char *project, char *error, size_t cap);
SearchLimits folder_search_limits(void);
bool folder_search_query_valid(const char *query);
bool folder_search_find(const char *text, size_t len, const char *query, unsigned limit, SearchHit **hits,
                        unsigned *count, char *error, size_t cap);
bool folder_search_find_options(const char *text, size_t len, const char *query, unsigned limit,
                                const SearchOptions *options, SearchHit **hits, unsigned *count, char *error,
                                size_t cap);
void folder_search_report_free(SearchReport *report);
bool folder_search_next(const SearchReport *report, int *file, int *hit, int direction);

/* A single remembered directory per project, separate from XML/import paths.
   These local preference APIs never stat/traverse the chosen directory. */
bool folder_search_profile_file(char *out, size_t cap, const char *settings, const char *project);
bool folder_search_profile_load(char out[FS_PATH], const char *file, const char *project, char *error,
                                size_t cap);
bool folder_search_profile_save(const char *root, const char *file, const char *project, char *error,
                                size_t cap);

typedef struct {
    atomic_bool done, cancel;
    atomic_uint progress_entries, progress_examined, progress_found, progress_hits, progress_skipped,
        progress_ignored;
    atomic_bool progress_limited;
    atomic_size_t progress_bytes;
    SearchLimits limits;
    SearchOptions options;
    void *matcher;
    SearchReport *report, *partial_report;
    void *publication;
    char root[FS_PATH], query[SEARCH_QUERY_MAX + 1], error[512];
    FsGuard guard;
    void *guard_context, *thread;
    size_t names_live;
    bool stop;
} FolderSearchJob;
/* Nonrecursive work is NOT performed by the caller. Guard lifetime covers free.
   Finished read-only snapshots remain in memory; navigation never opens files. */
FolderSearchJob *folder_search_start(const char *root, const char *query, const SearchLimits *limits,
                                     FsGuard guard, void *user, char *error, size_t cap);
FolderSearchJob *folder_search_start_options(const char *root, const char *query, const SearchLimits *limits,
                                             const SearchOptions *options, FsGuard guard, void *user,
                                             char *error, size_t cap);
/* New sorted immutable view only when count > after; never waits for the
   worker/I/O. NULL also means busy or allocation failure (retry next frame).
   Caller frees this report. File text/hits are shared, not copied or modified. */
SearchReport *folder_search_snapshot(FolderSearchJob *job, unsigned after);
/* Longest UTF-8 scalar-cell line; tabs4, already validated snapshot input. */
unsigned folder_search_columns(const char *text, size_t len);
void folder_search_cancel(FolderSearchJob *job); /* Never joins. */
void folder_search_free(FolderSearchJob *job);   /* Join on completion or shutdown. */
#endif
