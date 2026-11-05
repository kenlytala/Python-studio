#define _POSIX_C_SOURCE 200809L
#include "folder_search.h"
#include "buffer.h"
#include "import_paths.h"
#include "xml_paths.h"
#include "runner.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define mkdir(p, m) _mkdir(p)
#define rmdir _rmdir
#else
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
static unsigned checks;
#define CHECK(x)                                                                                             \
    do {                                                                                                     \
        checks++;                                                                                            \
        if (!(x)) {                                                                                          \
            fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x);                                                  \
            exit(1);                                                                                         \
        }                                                                                                    \
    } while (0)
static void pause_ms(void) {
#ifdef _WIN32
    Sleep(2);
#else
    struct timespec t = {0, 2000000};
    nanosleep(&t, NULL);
#endif
}
static void complete(FolderSearchJob *job) {
    CHECK(job);
    double until = runner_clock() + 4;
    while (!atomic_load(&job->done) && runner_clock() < until)
        pause_ms();
    CHECK(atomic_load(&job->done));
}
static FolderSearchJob *scan(const char *root, const SearchLimits *limits) {
    char error[512];
    FolderSearchJob *job = folder_search_start(root, "needle", limits, NULL, NULL, error, sizeof(error));
    complete(job);
    CHECK(!*job->error && job->report);
    return job;
}
static void path(char out[FS_PATH], const char *root, const char *leaf) {
    int n = snprintf(out, FS_PATH, "%s/%s", root, leaf);
    CHECK(n > 0 && n < FS_PATH);
}
static void write_at(const char *root, const char *name, const void *bytes, size_t n) {
    char full[FS_PATH];
    path(full, root, name);
    FILE *f = fopen(full, "wb");
    CHECK(f);
    CHECK(fwrite(bytes, 1, n, f) == n);
    CHECK(!fclose(f));
}
static void remove_at(const char *root, const char *name) {
    char full[FS_PATH];
    path(full, root, name);
    CHECK(!remove(full));
}
static void json(const char *value) {
    putchar('"');
    for (const unsigned char *p = (const unsigned char *)value; *p; p++) {
        if (*p == '"' || *p == '\\') {
            putchar('\\');
            putchar(*p);
        } else if (*p < 32)
            printf("\\u%04x", *p);
        else
            putchar(*p);
    }
    putchar('"');
}
static void json_hits(const SearchHit *hits, unsigned count) {
    putchar('[');
    for (unsigned i = 0; i < count; i++) {
        if (i)
            putchar(',');
        printf("[%zu,%zu,%u,%u,%u]", hits[i].start, hits[i].end, hits[i].line, hits[i].column, hits[i].width);
    }
    putchar(']');
}
static void json_report(const SearchReport *r) {
    printf("{\"root\":");
    json(r->root);
    printf(",\"query\":");
    json(r->query);
    printf(",\"files\":[");
    for (unsigned i = 0; i < r->count; i++) {
        if (i)
            putchar(',');
        printf("{\"relative\":");
        json(r->files[i].relative);
        printf(",\"text\":");
        json(r->files[i].text);
        printf(",\"hits\":");
        json_hits(r->files[i].hits, r->files[i].count);
        putchar('}');
    }
    printf("],\"hit_count\":%u,\"entries\":%u,\"examined\":%u,\"skipped\":%u,\"ignored\":%u,\"read_bytes\":%"
           "zu,\"snapshot_bytes\":%zu,\"limited\":%s,\"cancelled\":%s",
           r->hit_count, r->entries, r->examined, r->skipped, r->ignored, r->read_bytes, r->snapshot_bytes,
           r->limited ? "true" : "false", r->cancelled ? "true" : "false");
    for (int direction = 1; direction >= -1; direction -= 2) {
        printf(",\"%s\":[", direction == 1 ? "forward" : "backward");
        int file = -1, hit = -1;
        for (unsigned i = 0; r->hit_count && i <= r->hit_count; i++) {
            if (i)
                putchar(',');
            CHECK(folder_search_next(r, &file, &hit, direction));
            printf("[%d,%d]", file, hit);
        }
        putchar(']');
    }
    puts("}");
}
static bool option_arg(SearchOptions *o, const char *argument) {
    const char *value = strchr(argument, '=');
    if (!value)
        return false;
    size_t n = (size_t)(value - argument);
    value++;
    const char *names[] = {"case_sensitive", "whole_word",    "regex",        "recursive",
                           "include_files",  "exclude_files", "include_dirs", "exclude_dirs"};
    bool *flags[] = {&o->case_sensitive, &o->whole_word, &o->regex, &o->recursive};
    char *filters[] = {o->include_files, o->exclude_files, o->include_dirs, o->exclude_dirs};
    for (unsigned i = 0; i < 8; i++)
        if (n == strlen(names[i]) && !strncmp(argument, names[i], n)) {
            if (i < 4) {
                if (strcmp(value, "0") && strcmp(value, "1"))
                    return false;
                *flags[i] = !strcmp(value, "1");
            } else {
                if (strlen(value) > SEARCH_FILTER_MAX)
                    return false;
                strcpy(filters[i - 4], value);
            }
            return true;
        }
    return false;
}
static int command(int argc, char **argv) {
    if (argc >= 4 && !strcmp(argv[1], "--scan")) {
        SearchLimits l = folder_search_limits();
        SearchOptions options = folder_search_options();
        for (int i = 4; i < argc; i++) {
            if (option_arg(&options, argv[i]))
                continue;
            char name[40];
            unsigned long long value;
            if (sscanf(argv[i], "%39[^=]=%llu", name, &value) != 2)
                return 2;
#define LIMIT(n)                                                                                             \
    if (!strcmp(name, #n))                                                                                   \
        l.n = value;                                                                                         \
    else
            LIMIT(file_bytes)
            LIMIT(read_bytes)
            LIMIT(snapshot_bytes) LIMIT(files) LIMIT(hits) LIMIT(entries) LIMIT(depth) LIMIT(lines) {
                return 2;
            }
#undef LIMIT
        }
        char error[512];
        FolderSearchJob *j =
            folder_search_start_options(argv[2], argv[3], &l, &options, NULL, NULL, error, sizeof(error));
        if (j) {
            complete(j);
            snprintf(error, sizeof(error), "%s", j->error);
        }
        if (!j || *error) {
            printf("{\"error\":");
            json(error);
            puts("}");
        } else
            json_report(j->report);
        folder_search_free(j);
        return 0;
    }
    if (argc >= 4 && !strcmp(argv[1], "--model")) {
        FILE *f = fopen(argv[3], "rb");
        if (!f)
            return 2;
        char *text = malloc(SEARCH_FILE_MAX + 1u);
        if (!text) {
            fclose(f);
            return 2;
        }
        size_t n = fread(text, 1, SEARCH_FILE_MAX, f);
        bool ok = !ferror(f);
        fclose(f);
        text[n] = 0;
        unsigned limit = argc > 4 ? (unsigned)strtoul(argv[4], NULL, 10) : SEARCH_HITS_MAX;
        SearchHit *hits = NULL;
        unsigned count = 0;
        SearchOptions options = folder_search_options();
        for (int i = 5; i < argc; i++)
            if (!option_arg(&options, argv[i])) {
                free(text);
                return 2;
            }
        char error[512] = "Lecture impossible.";
        if (ok)
            ok = folder_search_find_options(text, n, argv[2], limit, &options, &hits, &count, error,
                                            sizeof(error));
        if (ok) {
            printf("{\"hits\":");
            json_hits(hits, count);
            puts("}");
        } else {
            printf("{\"error\":");
            json(error);
            puts("}");
        }
        free(hits);
        free(text);
        return 0;
    }
    return 2;
}
static void advanced_models(void) {
    SearchOptions o = folder_search_options();
    SearchHit *hits = NULL;
    unsigned count = 0;
    char error[512];
    CHECK(o.case_sensitive && o.recursive && !o.regex && !o.whole_word);
    const char *text = "Été été ÉTÉ";
    o.case_sensitive = false;
    CHECK(folder_search_find_options(text, strlen(text), "été", SEARCH_HITS_MAX, &o, &hits, &count, error,
                                     sizeof(error)));
    CHECK(count == 3 && hits[0].start == 0 && hits[1].start == 6 && hits[2].column == 8);
    free(hits);
    text = "cat écat 猫cat cat_ cat2 cat!";
    o = folder_search_options();
    o.whole_word = true;
    CHECK(folder_search_find_options(text, strlen(text), "cat", SEARCH_HITS_MAX, &o, &hits, &count, error,
                                     sizeof(error)));
    CHECK(count == 2 && hits[0].start == 0 && hits[1].end == strlen(text) - 1);
    free(hits);
    text = "x\nα\ty\nz";
    o = folder_search_options();
    o.regex = true;
    CHECK(folder_search_find_options(text, strlen(text), "(?s)x.*y", SEARCH_HITS_MAX, &o, &hits, &count,
                                     error, sizeof(error)));
    CHECK(count == 1 && hits[0].end == 6 && hits[0].line == 0 && hits[0].width == 1);
    free(hits);
    text = "猫😀";
    CHECK(folder_search_find_options(text, strlen(text), "(?=.)|$", SEARCH_HITS_MAX, &o, &hits, &count, error,
                                     sizeof(error)));
    CHECK(count == 3 && hits[0].start == 0 && hits[1].start == 3 && hits[2].start == 7 && !hits[2].width);
    free(hits);
    CHECK(folder_search_find_options("", 0, "\\z", SEARCH_HITS_MAX, &o, &hits, &count, error, sizeof(error)));
    CHECK(count == 1 && !hits[0].start && !hits[0].end);
    free(hits);
    const char *bad[] = {"(", "[", "\\C", "a{4,2}"};
    for (unsigned i = 0; i < 4; i++)
        CHECK(!folder_search_find_options(text, strlen(text), bad[i], SEARCH_HITS_MAX, &o, &hits, &count,
                                          error, sizeof(error)) &&
              *error);
    char *large = malloc(10002);
    CHECK(large);
    memset(large, 'a', 10000);
    large[10000] = '!';
    large[10001] = 0;
    double before = runner_clock();
    CHECK(!folder_search_find_options(large, 10001, "(*NO_START_OPT)(*NO_AUTO_POSSESS)(a|aa)+$",
                                      SEARCH_HITS_MAX, &o, &hits, &count, error, sizeof(error)));
    CHECK(strstr(error, "limite") && runner_clock() - before < 2.5);
    free(large);
    o = folder_search_options();
    strcpy(o.include_files, "../bad");
    CHECK(!folder_search_find_options("cat", 3, "cat", 10, &o, &hits, &count, error, sizeof(error)) &&
          *error);
    puts("OK advanced native options: Unicode insensitive/whole-word, PCRE2 groups/UTF-8/zero/multiline, "
         "explicit invalid/budget errors");
}
static void model(void) {
    SearchHit *hits = NULL;
    unsigned count = 0;
    char error[512];
    CHECK(folder_search_find("[a]* [a]* aaa", 13, "[a]*", SEARCH_HITS_MAX, &hits, &count, error,
                             sizeof(error)));
    CHECK(count == 2 && hits[1].start == 5);
    free(hits);
    CHECK(folder_search_find("aaaaa", 5, "aa", SEARCH_HITS_MAX, &hits, &count, error, sizeof(error)));
    CHECK(count == 2 && hits[0].end == 2 && hits[1].start == 2);
    free(hits);
    const char *unicode = "α\tété\nXété été";
    CHECK(folder_search_find(unicode, strlen(unicode), "été", SEARCH_HITS_MAX, &hits, &count, error,
                             sizeof(error)));
    CHECK(count == 3);
    CHECK(hits[0].start == 3 && hits[0].end == 8 && hits[0].line == 0 && hits[0].column == 4 &&
          hits[0].width == 3);
    CHECK(hits[1].line == 1 && hits[1].column == 1 && hits[2].column == 5);
    free(hits);
    CHECK(
        folder_search_find("x\tété\tété", 13, "\tété", SEARCH_HITS_MAX, &hits, &count, error, sizeof(error)));
    CHECK(count == 2 && hits[0].width == 6 && hits[1].width == 4);
    free(hits);
    CHECK(folder_search_find("Needle needle NEEDLE", 20, "needle", SEARCH_HITS_MAX, &hits, &count, error,
                             sizeof(error)));
    CHECK(count == 1 && hits[0].start == 7);
    free(hits);
    CHECK(folder_search_find("", 0, "needle", SEARCH_HITS_MAX, &hits, &count, error, sizeof(error)));
    CHECK(!count && !hits);
    const char *invalid[] = {"", "a\nb", "a\rb", "\001", "\177", "\xff"};
    for (unsigned i = 0; i < sizeof(invalid) / sizeof(*invalid); i++)
        CHECK(!folder_search_query_valid(invalid[i]));
    CHECK(folder_search_query_valid("\t猫😀"));
    char large[1026];
    memset(large, 'x', 1024);
    large[1024] = 0;
    CHECK(folder_search_query_valid(large));
    large[1024] = 'x';
    large[1025] = 0;
    CHECK(!folder_search_query_valid(large));
    CHECK(!folder_search_find("x", 1, "x", 0, &hits, &count, error, sizeof(error)));
    CHECK(!folder_search_find("\xff", 1, "x", 1, &hits, &count, error, sizeof(error)));
    CHECK(!folder_search_find("x", 1, "", 1, &hits, &count, error, sizeof(error)));
    CHECK(!folder_search_next(NULL, NULL, NULL, 1));
    char full[FS_PATH];
    CHECK(folder_search_root(full, "a/../b/", "/not-mounted/project", error, sizeof(error)) &&
          !strcmp(full, "/not-mounted/project/b"));
    CHECK(folder_search_root(full, "../../../../x", "/not-mounted/project", error, sizeof(error)) &&
          !strcmp(full, "/x"));
    CHECK(folder_search_root(full, "/a/../b/", "/not-mounted/project", error, sizeof(error)) &&
          !strcmp(full, "/b"));
    CHECK(!folder_search_root(full, "", "/project", error, sizeof(error)));
    CHECK(!folder_search_root(full, "a\nb", "/project", error, sizeof(error)));
    char deep[FS_PATH];
    size_t n = 0;
    for (unsigned i = 0; i < 800; i++) {
        deep[n++] = 'a';
        deep[n++] = '/';
    }
    deep[n++] = 'b';
    deep[n] = 0;
    CHECK(folder_search_root(full, deep, "/project", error, sizeof(error)));
    CHECK(strlen(full) == strlen(deep) + 9);
}
#ifndef _WIN32
static void temp_root(char out[FS_PATH]) {
    strcpy(out, "/tmp/studio-folder-search-XXXXXX");
    CHECK(mkdtemp(out));
}
typedef struct {
    const char *path;
    unsigned count;
    bool change;
} Changed;
static bool changed_guard(void *user, const char *file, bool leaf, char *error, size_t cap) {
    (void)leaf;
    (void)error;
    (void)cap;
    Changed *g = user;
    if (!strcmp(file, g->path) && ++g->count == 2) {
        FILE *f = fopen(file, "wb");
        CHECK(f);
        CHECK(fputs("needle changed", f) >= 0);
        CHECK(!fclose(f));
        g->change = true;
    }
    return true;
}
static bool denied_guard(void *user, const char *file, bool leaf, char *error, size_t cap) {
    (void)leaf;
    const char *denied = user;
    if (!strcmp(file, denied)) {
        snprintf(error, cap, "DENIED");
        return false;
    }
    return true;
}
typedef struct {
    pthread_t owner;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    bool entered, release, wrong_thread;
} Blocked;
static bool blocked_guard(void *user, const char *file, bool leaf, char *error, size_t cap) {
    (void)file;
    (void)leaf;
    (void)error;
    (void)cap;
    Blocked *g = user;
    pthread_mutex_lock(&g->mutex);
    g->wrong_thread |= pthread_equal(g->owner, pthread_self());
    g->entered = true;
    pthread_cond_broadcast(&g->cond);
    while (!g->release)
        pthread_cond_wait(&g->cond, &g->mutex);
    pthread_mutex_unlock(&g->mutex);
    return true;
}

typedef struct {
    atomic_bool entered, release, last_entered, last_release;
    char first[FS_PATH], last[FS_PATH];
    bool denied;
} ProgressGate;
static void progress_gate_init(ProgressGate *g, const char *root, bool denied) {
    atomic_init(&g->entered, false);
    atomic_init(&g->release, false);
    atomic_init(&g->last_entered, false);
    atomic_init(&g->last_release, false);
    path(g->first, root, "a.txt");
    path(g->last, root, "zz.txt");
    g->denied = denied;
}
static bool progress_guard(void *user, const char *file, bool leaf, char *error, size_t cap) {
    (void)leaf;
    ProgressGate *g = user;
    /* A revoked lease rejects later leaves AND the final root recheck. */
    if (g->denied && atomic_load(&g->entered) && strcmp(file, g->first)) {
        snprintf(error, cap, "Search lease invalidated");
        return false;
    }
    if (!strcmp(file, g->first)) {
        atomic_store(&g->entered, true);
        while (!atomic_load(&g->release))
            pause_ms();
        if (g->denied) {
            snprintf(error, cap, "Access invalidated after first validated file");
            return false;
        }
    }
    if (!strcmp(file, g->last)) {
        atomic_store(&g->last_entered, true);
        while (!atomic_load(&g->last_release))
            pause_ms();
    }
    return true;
}
static void progress_wait(atomic_bool *flag) {
    double until = runner_clock() + 3;
    while (!atomic_load(flag) && runner_clock() < until)
        pause_ms();
    CHECK(atomic_load(flag));
}
static void progressive_worker(void) {
    char root[FS_PATH], directory[FS_PATH], error[512];
    temp_root(root);
    path(directory, root, "a");
    CHECK(!mkdir(directory, 0700));
    write_at(root, "a/n.txt", "needle + needle\n", 16);
    write_at(root, "a.txt", "needle\n", 7);
    write_at(root, "zz.txt", "needle\n", 7);
    for (unsigned mode = 0; mode < 3; mode++) {
        ProgressGate gate;
        progress_gate_init(&gate, root, mode == 2);
        FolderSearchJob *job =
            folder_search_start(root, "needle", NULL, progress_guard, &gate, error, sizeof(error));
        CHECK(job);
        progress_wait(&gate.entered);
        CHECK(!atomic_load(&job->done));
        SearchReport *one = folder_search_snapshot(job, 0);
        CHECK(one && one->count == 1 && one->hit_count == 2 && !one->complete);
        CHECK(!strcmp(one->files[0].relative, "a/n.txt") && !strcmp(one->files[0].text, "needle + needle\n"));
        for (unsigned i = 0; i < 32; i++)
            CHECK(!folder_search_snapshot(job, 1));
        CHECK(!folder_search_snapshot(job, SEARCH_FILES_MAX));
        /* POSIX white-box contention: the publication's first member is the
           pthread mutex. A locked publisher must never block the UI caller. */
        pthread_mutex_t *publication = job->publication;
        CHECK(!pthread_mutex_lock(publication));
        double start = runner_clock();
        CHECK(!folder_search_snapshot(job, 0) && runner_clock() - start < .1);
        CHECK(!pthread_mutex_unlock(publication));
        SearchReport *two = NULL;
        if (mode == 0) {
            atomic_store(&gate.release, true);
            progress_wait(&gate.last_entered);
            two = folder_search_snapshot(job, 1);
            CHECK(two && two->count == 2 && two->hit_count == 3 && !two->complete);
            CHECK(!strcmp(two->files[0].relative, "a.txt") && !strcmp(two->files[1].relative, "a/n.txt"));
            CHECK(two->files[1].text == one->files[0].text && two->files[1].hits == one->files[0].hits);
            CHECK(!atomic_load(&job->done));
            int file = 0, hit = 0;
            CHECK(folder_search_next(two, &file, &hit, 1) && file == 1 && hit == 0);
        } else if (mode == 1)
            folder_search_cancel(job);
        atomic_store(&gate.release, true);
        atomic_store(&gate.last_release, true);
        complete(job);
        if (mode == 0) {
            CHECK(job->report && job->report->complete && job->report->count == 3);
            CHECK(job->report->files[1].text == one->files[0].text);
        } else if (mode == 1)
            CHECK(job->report && job->report->cancelled && job->report->count == 1);
        else {
            CHECK(*job->error && !job->report && job->partial_report && job->partial_report->count == 1);
            two = folder_search_snapshot(job, 0);
            CHECK(two && two->count == 1 && !two->complete);
        }
        folder_search_free(job);
        /* Previously published descriptors keep text/hits alive after final
           sort, cancellation, terminal error and destruction of the job. */
        CHECK(one->files[0].hits[1].start == 9 && !strcmp(one->files[0].text, "needle + needle\n"));
        folder_search_report_free(one);
        if (two) {
            unsigned at = mode == 0 ? 1 : 0;
            CHECK(two->files[at].count == 2 && !strcmp(two->files[at].text, "needle + needle\n"));
            folder_search_report_free(two);
        }
    }
    remove_at(root, "a/n.txt");
    remove_at(root, "a.txt");
    remove_at(root, "zz.txt");
    CHECK(!rmdir(directory) && !rmdir(root));
    puts("OK native progressive publication: blocked later file, sorted arrivals, trylock, shared immutable "
         "payload, F3, cancel/error/job-free lifetime");
}
static void worker(void) {
    char root[FS_PATH], full[FS_PATH], error[512];
    temp_root(root);
    write_at(root, "a.txt", "needle + needle\r\n", 17);
    write_at(root, "z.txt", "no", 2);
    path(full, root, "sub");
    CHECK(!mkdir(full, 0700));
    write_at(root, "sub/b.xml", "α\tneedle\n", 10);
    write_at(root, "binary", "needle\0", 7);
    write_at(root, "invalid", "\xffneedle", 7);
    write_at(root, "control", "needle\001", 7);
    path(full, root, ".git");
    CHECK(!mkdir(full, 0700));
    write_at(root, ".git/x", "needle", 6);
    char target[FS_PATH];
    path(full, root, "link");
    path(target, root, "a.txt");
    CHECK(!symlink(target, full));
    path(full, root, "linkdir");
    path(target, root, "sub");
    CHECK(!symlink(target, full));
    path(full, root, "pipe");
    CHECK(!mkfifo(full, 0600));
    FolderSearchJob *j = scan(root, NULL);
    SearchReport *r = j->report;
    CHECK(r->count == 2 && r->hit_count == 3);
    CHECK(r->skipped == 6 && r->ignored == 1);
    CHECK(!strcmp(r->files[0].relative, "a.txt") && !strcmp(r->files[1].relative, "sub/b.xml"));
    CHECK(!strcmp(r->files[0].text, "needle + needle\n"));
    CHECK(r->files[1].hits[0].column == 4 && r->files[1].hits[0].start == 3);
    CHECK(!r->cancelled && !r->limited);
    int file = -1, hit = -1;
    const int wanted[][2] = {{0, 0}, {0, 1}, {1, 0}, {0, 0}};
    for (unsigned i = 0; i < 4; i++) {
        CHECK(folder_search_next(r, &file, &hit, 1));
        CHECK(file == wanted[i][0] && hit == wanted[i][1]);
    }
    CHECK(folder_search_next(r, &file, &hit, -1) && file == 1 && hit == 0);
    CHECK(!folder_search_next(r, &file, &hit, 0));
    size_t read_bytes = r->read_bytes;
    write_at(root, "a.txt", "changed", 7);
    CHECK(!strcmp(r->files[0].text, "needle + needle\n"));
    write_at(root, "a.txt", "needle + needle\r\n", 17);
    folder_search_free(j);
    SearchLimits l = folder_search_limits();
    l.files = 1;
    j = scan(root, &l);
    CHECK(j->report->count == 1 && j->report->hit_count == 2 && j->report->limited);
    folder_search_free(j);
    l = folder_search_limits();
    l.hits = 1;
    j = scan(root, &l);
    CHECK(j->report->count == 1 && j->report->hit_count == 1 && j->report->limited);
    folder_search_free(j);
    l = folder_search_limits();
    l.read_bytes = 17;
    j = scan(root, &l);
    CHECK(j->report->count == 1 && j->report->read_bytes == 17 && j->report->limited);
    folder_search_free(j);
    l = folder_search_limits();
    l.snapshot_bytes = 17;
    j = scan(root, &l);
    CHECK(j->report->count == 1 && j->report->snapshot_bytes == 16 && j->report->limited);
    folder_search_free(j);
    l = folder_search_limits();
    l.file_bytes = 8;
    j = scan(root, &l);
    CHECK(!j->report->count && j->report->skipped == 8);
    folder_search_free(j);
    l = folder_search_limits();
    l.lines = 1;
    j = scan(root, &l);
    CHECK(!j->report->count && j->report->skipped == 8);
    folder_search_free(j);
    l = folder_search_limits();
    l.entries = 2;
    j = scan(root, &l);
    CHECK(j->report->entries == 2 && j->report->limited && j->report->count <= 2);
    folder_search_free(j);
    path(full, root, "sub/deep");
    CHECK(!mkdir(full, 0700));
    write_at(root, "sub/deep/x", "needle", 6);
    l = folder_search_limits();
    l.depth = 1;
    j = scan(root, &l);
    CHECK(j->report->count == 2 && j->report->limited);
    folder_search_free(j);
    remove_at(root, "sub/deep/x");
    path(full, root, "sub/deep");
    CHECK(!rmdir(full));
    char *binary = calloc(100001, 1);
    CHECK(binary);
    memset(binary + 1, 'x', 100000);
    write_at(root, "big", binary, 100001);
    free(binary);
    j = scan(root, NULL);
    CHECK(j->report->read_bytes == read_bytes + 65536 && j->report->skipped == 7);
    folder_search_free(j);
    remove_at(root, "big");
    path(full, root, "a.txt");
    Changed g = {full, 0, false};
    j = folder_search_start(root, "needle", NULL, changed_guard, &g, error, sizeof(error));
    complete(j);
    CHECK(g.change && j->report && j->report->count == 1 && j->report->skipped == 7);
    folder_search_free(j);
    write_at(root, "a.txt", "needle + needle\r\n", 17);
    path(full, root, "sub");
    j = folder_search_start(root, "needle", NULL, denied_guard, full, error, sizeof(error));
    complete(j);
    CHECK(j->report && j->report->count == 1);
    folder_search_free(j);
    j = folder_search_start(root, "needle", NULL, denied_guard, root, error, sizeof(error));
    complete(j);
    CHECK(!j->report && !strcmp(j->error, "DENIED"));
    folder_search_free(j);
    Blocked b = {0};
    b.owner = pthread_self();
    CHECK(!pthread_mutex_init(&b.mutex, NULL));
    CHECK(!pthread_cond_init(&b.cond, NULL));
    j = folder_search_start(root, "needle", NULL, blocked_guard, &b, error, sizeof(error));
    CHECK(j);
    pthread_mutex_lock(&b.mutex);
    while (!b.entered)
        pthread_cond_wait(&b.cond, &b.mutex);
    CHECK(!b.wrong_thread);
    double started = runner_clock();
    folder_search_cancel(j);
    CHECK(runner_clock() - started < .1 && !atomic_load(&j->done));
    b.release = true;
    pthread_cond_broadcast(&b.cond);
    pthread_mutex_unlock(&b.mutex);
    complete(j);
    CHECK(j->report && j->report->cancelled && !j->report->count);
    folder_search_free(j);
    CHECK(!pthread_cond_destroy(&b.cond));
    CHECK(!pthread_mutex_destroy(&b.mutex));
    l = folder_search_limits();
    l.files = 0;
    CHECK(!folder_search_start(root, "needle", &l, NULL, NULL, error, sizeof(error)));
    CHECK(!folder_search_start("relative", "needle", NULL, NULL, NULL, error, sizeof(error)));
    CHECK(!folder_search_start(root, "", NULL, NULL, NULL, error, sizeof(error)));
    path(full, root, "missing");
    j = folder_search_start(full, "needle", NULL, NULL, NULL, error, sizeof(error));
    complete(j);
    CHECK(*j->error && !j->report);
    folder_search_free(j);
    path(full, root, "a.txt");
    j = folder_search_start(full, "needle", NULL, NULL, NULL, error, sizeof(error));
    complete(j);
    CHECK(*j->error && !j->report);
    folder_search_free(j);
    const char *names[] = {"a.txt", "z.txt",   "binary", "invalid",   "control",
                           "link",  "linkdir", "pipe",   "sub/b.xml", ".git/x"};
    for (unsigned i = 0; i < sizeof(names) / sizeof(*names); i++)
        remove_at(root, names[i]);
    path(full, root, "sub");
    CHECK(!rmdir(full));
    path(full, root, ".git");
    CHECK(!rmdir(full));
    CHECK(!rmdir(root));
}
static void profiles(void) {
    char root[FS_PATH], config[FS_PATH], file[FS_PATH], other[FS_PATH], out[FS_PATH], error[512];
    temp_root(root);
    path(config, root, "settings.cfg");
    CHECK(folder_search_profile_file(file, sizeof(file), config, "/project"));
    CHECK(strstr(fs_basename(file), "folder-search-") == fs_basename(file));
    CHECK(import_paths_file(other, sizeof(other), config, "/project") && strcmp(file, other));
    CHECK(xml_paths_file(other, sizeof(other), config, "/project") && strcmp(file, other));
    strcpy(out, "/project");
    CHECK(!folder_search_profile_load(out, file, "/project", error, sizeof(error)) && !*error &&
          !strcmp(out, "/project"));
    CHECK(folder_search_profile_save("/not-mounted/été", file, "/project", error, sizeof(error)));
    CHECK(folder_search_profile_load(out, file, "/project", error, sizeof(error)) &&
          !strcmp(out, "/not-mounted/été"));
    struct stat info;
    CHECK(!stat(file, &info) && (info.st_mode & 0777) == 0600);
    CHECK(!folder_search_profile_load(out, file, "/other", error, sizeof(error)) &&
          !strcmp(out, "/not-mounted/été"));
    CHECK(!folder_search_profile_save("relative", file, "/project", error, sizeof(error)));
    CHECK(folder_search_profile_load(out, file, "/project", error, sizeof(error)) &&
          !strcmp(out, "/not-mounted/été"));
    CHECK(!remove(file));
    CHECK(!rmdir(root));
}
#endif
static void viewport_columns(void) {
    CHECK(folder_search_columns(NULL, 0) == 0);
    CHECK(folder_search_columns("", 0) == 0);
    const char *unicode = "α\t猫😀";
    CHECK(folder_search_columns(unicode, strlen(unicode)) == 6);
    const char *combining = "é";
    CHECK(folder_search_columns(combining, strlen(combining)) == 2);
    const char *lines = "short\na\tb\tend\n";
    CHECK(folder_search_columns(lines, strlen(lines)) == 11);
    char *tabs = malloc(SEARCH_FILE_MAX);
    CHECK(tabs);
    memset(tabs, '\t', SEARCH_FILE_MAX);
    CHECK(folder_search_columns(tabs, SEARCH_FILE_MAX) == 4 * SEARCH_FILE_MAX);
    free(tabs);
}
int main(int argc, char **argv) {
    if (argc > 1)
        return command(argc, argv);
    model();
    viewport_columns();
    advanced_models();
#ifndef _WIN32
    worker();
    progressive_worker();
    profiles();
#endif
    printf("OK %u folder-search checks: literal/UTF-8/tabs, recursive snapshots, global F3 model, budgets, "
           "async cancellation, guarded changes, isolated private preferences\n",
           checks);
    return 0;
}
