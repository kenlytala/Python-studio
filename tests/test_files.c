#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#include "files.h"
#include "file_job.h"
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
static int checks;
#define CHECK(c)                                                                                             \
    do {                                                                                                     \
        checks++;                                                                                            \
        if (!(c)) {                                                                                          \
            fprintf(stderr, "FAIL %d: %s\n", __LINE__, #c);                                                  \
            exit(1);                                                                                         \
        }                                                                                                    \
    } while (0)
static void path(char *out, const char *base, const char *leaf) {
    CHECK(snprintf(out, FS_PATH, "%s/%s", base, leaf) < FS_PATH);
}
static void put(const char *path, const char *text) {
    FILE *f = fopen(path, "wb");
    CHECK(f);
    CHECK(fwrite(text, 1, strlen(text), f) == strlen(text));
    CHECK(!fclose(f));
}
static void content(const char *path, const char *text) {
    char data[256];
    FILE *f = fopen(path, "rb");
    CHECK(f);
    size_t n = fread(data, 1, sizeof(data) - 1, f);
    data[n] = 0;
    fclose(f);
    CHECK(!strcmp(data, text));
}
static unsigned long long bytes_seen;
static int entries_seen;
static bool progress(void *user, unsigned long long bytes, bool entry) {
    (void)user;
    bytes_seen += bytes;
    entries_seen += entry;
    return true;
}
static bool cancel_copy(void *user, unsigned long long bytes, bool entry) {
    (void)entry;
    unsigned long long *count = user;
    *count += bytes;
    return *count < 100000;
}
typedef struct {
    char path[FS_PATH];
    bool fired;
} Race;
static bool race(void *user, unsigned long long bytes, bool entry) {
    (void)bytes;
    (void)entry;
    Race *r = user;
    if (!r->fired) {
        put(r->path, "not yours");
        r->fired = true;
    }
    return true;
}
static void wait_job(FileJob *job) {
    CHECK(job);
    double elapsed = 0;
    while (!atomic_load(&job->done) && elapsed < 5) {
        struct timespec t = {0, 1000000};
        nanosleep(&t, NULL);
        elapsed += .001;
    }
    CHECK(atomic_load(&job->done));
}
int main(void) {
    char workspace[] = "/tmp/prisme-files-XXXXXX";
    CHECK(mkdtemp(workspace));
    char root[FS_PATH], outside[FS_PATH], a[FS_PATH], b[FS_PATH], c[FS_PATH], d[FS_PATH], created[FS_PATH],
        error[512];
    path(root, workspace, "project");
    path(outside, workspace, "external");
    CHECK(!mkdir(root, 0700));
    CHECK(!mkdir(outside, 0700));
    CHECK(fs_under(root, root));
    CHECK(!fs_under("/tmp/name\\", "/tmp/name/file"));
    CHECK(fs_under("/tmp/name\\", "/tmp/name\\/file"));
    CHECK(!strcmp(fs_basename("/tmp/nom\\"), "nom\\"));
    CHECK(fs_parent("/tmp/nom\\", a) && !strcmp(a, "/tmp"));
    CHECK(fs_parent("/tmp/nom\\/file", a) && !strcmp(a, "/tmp/nom\\"));
    CHECK(fs_parent("/", a) && !strcmp(a, "/"));
    path(a, root, "same");
    put(a, "do not delete");
    path(b, root, "same\\");
    put(b, "delete only this");
    CHECK(fs_remove(root, b, NULL, NULL, error, sizeof(error)));
    content(a, "do not delete");
    CHECK(fs_kind(b) == FS_MISSING);
    path(a, root, "a");
    CHECK(fs_under(root, a));
    path(a, workspace, "project-other");
    CHECK(!fs_under(root, a));
    const char *bad[] = {"", ".", "..", "../escape", "a/b", "a\\b", "a:b", "a\nb", "trailing.", "trailing "};
    for (unsigned i = 0; i < sizeof(bad) / sizeof(*bad); i++)
        CHECK(!fs_create(root, root, bad[i], false, created, error, sizeof(error)));
    CHECK(fs_create(root, root, "étoile.py", false, created, error, sizeof(error)));
    content(created, "");
    put(created, "original");
    CHECK(!fs_create(root, root, "étoile.py", false, a, error, sizeof(error)));
    content(created, "original");
    CHECK(fs_create(root, root, "dossier", true, a, error, sizeof(error)));
    CHECK(fs_kind(a) == FS_DIRECTORY);
    CHECK(!fs_create(root, root, "dossier", true, b, error, sizeof(error)));
    CHECK(!fs_create(root, outside, "escape", false, b, error, sizeof(error)));
    path(b, root, "outside-link");
    CHECK(!symlink(outside, b));
    CHECK(!fs_create(root, b, "escape", false, c, error, sizeof(error)));
    path(c, outside, "sentinel");
    put(c, "keep me");
    CHECK(fs_remove(root, b, NULL, NULL, error, sizeof(error)));
    content(c, "keep me");
    CHECK(!fs_remove(root, root, NULL, NULL, error, sizeof(error)));
    CHECK(fs_kind(root) == FS_DIRECTORY);
    CHECK(!fs_remove(root, c, NULL, NULL, error, sizeof(error)));
    content(c, "keep me");

    FsPaths *paths = calloc(1, sizeof(*paths));
    CHECK(paths);
    char *uri = malloc(FS_CLIP_LIMIT + 1);
    CHECK(uri);
    char query[FS_PATH * 2];
    snprintf(query, sizeof(query),
             "# "
             "comment\r\ncopy\nfile://localhost%s/étoile.py\r\nfile://remote/denied\nfile:///tmp/"
             "a%%20b%%23c.py\nfile:///bad%%00name\nfile:///bad%%zzname\nrelative.txt\n",
             root);
    CHECK(fs_paths_parse(paths, query) == 2);
    CHECK(strstr(paths->paths[0], "étoile.py"));
    CHECK(!strcmp(paths->paths[1], "/tmp/a b#c.py"));
    CHECK(fs_paths_text(paths, uri, FS_CLIP_LIMIT + 1));
    CHECK(strstr(uri, "%C3%A9toile.py"));
    CHECK(strstr(uri, "a%20b%23c.py"));
    CHECK(fs_paths_parse(paths, uri) == 2);
    CHECK(!strcmp(paths->paths[1], "/tmp/a b#c.py"));
    CHECK(fs_paths_parse(paths, "cut\nfile:///tmp/a\nfile:///tmp/a\n/tmp/b\n") == 2);
    char many[4096] = {0};
    size_t used = 0;
    for (int i = 0; i < 65; i++)
        used += (size_t)snprintf(many + used, sizeof(many) - used, "file:///tmp/f-%d\n", i);
    CHECK(fs_paths_parse(paths, many) == -1 && paths->count == 0);
    CHECK(fs_paths_parse(paths, "file:///tmp/a\n") == 1);
    char tiny[4];
    CHECK(!fs_paths_text(paths, tiny, sizeof(tiny)));

    paths->count = 1;
    path(paths->paths[0], outside, "binary.dat");
    FILE *file = fopen(paths->paths[0], "wb");
    CHECK(file);
    unsigned char data[65536];
    for (unsigned i = 0; i < sizeof(data); i++)
        data[i] = (unsigned char)(i * 37);
    for (int i = 0; i < 48; i++)
        CHECK(fwrite(data, 1, sizeof(data), file) == sizeof(data));
    CHECK(!fclose(file));
    bytes_seen = 0;
    entries_seen = 0;
    CHECK(fs_paste(root, root, paths, progress, NULL, created, error, sizeof(error)));
    CHECK(bytes_seen == 48 * sizeof(data) && entries_seen == 1);
    file = fopen(created, "rb");
    CHECK(file);
    unsigned char received[65536];
    for (int i = 0; i < 48; i++) {
        CHECK(fread(received, 1, sizeof(received), file) == sizeof(received));
        CHECK(!memcmp(data, received, sizeof(data)));
    }
    CHECK(fgetc(file) == EOF);
    fclose(file);
    CHECK(fs_paste(root, root, paths, NULL, NULL, b, error, sizeof(error)));
    CHECK(strstr(b, "(copie)"));
    CHECK(fs_paste(root, root, paths, NULL, NULL, c, error, sizeof(error)));
    CHECK(strstr(c, "(copie 2)"));
    unsigned long long cancellation = 0;
    CHECK(!fs_paste(root, root, paths, cancel_copy, &cancellation, d, error, sizeof(error)));
    path(d, root, "binary (copie 3).dat");
    CHECK(fs_kind(d) == FS_MISSING);
    CHECK(fs_kind(paths->paths[0]) == FS_FILE);
    CHECK(fs_remove(root, created, NULL, NULL, error, sizeof(error)));
    CHECK(fs_remove(root, b, NULL, NULL, error, sizeof(error)));
    CHECK(fs_remove(root, c, NULL, NULL, error, sizeof(error)));

    path(a, outside, "tree");
    CHECK(!mkdir(a, 0700));
    path(b, a, "nested");
    CHECK(!mkdir(b, 0700));
    path(c, b, "code.py");
    put(c, "print('hello')");
    path(d, a, "link");
    CHECK(!symlink("nested/code.py", d));
    path(d, a, "cycle");
    CHECK(!symlink(".", d));
    paths->count = 1;
    strcpy(paths->paths[0], a);
    CHECK(fs_paste(root, root, paths, NULL, NULL, created, error, sizeof(error)));
    path(d, created, "nested/code.py");
    content(d, "print('hello')");
    path(d, created, "link");
    CHECK(fs_kind(d) == FS_LINK);
    path(d, created, "cycle");
    CHECK(fs_kind(d) == FS_LINK);
    CHECK(fs_remove(root, created, progress, NULL, error, sizeof(error)));
    content(c, "print('hello')");
    path(b, root, "self");
    CHECK(!mkdir(b, 0700));
    paths->count = 1;
    strcpy(paths->paths[0], b);
    CHECK(!fs_paste(root, b, paths, NULL, NULL, created, error, sizeof(error)));
    path(c, b, "sub");
    CHECK(!mkdir(c, 0700));
    CHECK(!fs_paste(root, c, paths, NULL, NULL, created, error, sizeof(error)));
    CHECK(fs_paste(root, root, paths, NULL, NULL, created, error, sizeof(error)));
    CHECK(strstr(created, "(copie)"));
    CHECK(fs_remove(root, created, NULL, NULL, error, sizeof(error)));

    path(a, outside, "fifo-tree");
    CHECK(!mkdir(a, 0700));
    path(b, a, "pipe");
    CHECK(!mkfifo(b, 0600));
    path(b, a, "normal");
    put(b, "rollback");
    strcpy(paths->paths[0], a);
    CHECK(!fs_paste(root, root, paths, NULL, NULL, created, error, sizeof(error)));
    path(b, root, "fifo-tree");
    CHECK(fs_kind(b) == FS_MISSING);
    path(a, outside, "race.py");
    put(a, "source");
    strcpy(paths->paths[0], a);
    Race r = {0};
    path(r.path, root, "race.py");
    CHECK(!fs_paste(root, root, paths, race, &r, created, error, sizeof(error)));
    content(r.path, "not yours");
    content(a, "source");

    path(a, outside, "strange\\");
    CHECK(!mkdir(a, 0700));
    path(b, a, "child.py");
    put(b, "backslash folder");
    paths->count = 1;
    strcpy(paths->paths[0], a);
    CHECK(fs_paste(root, root, paths, NULL, NULL, created, error, sizeof(error)));
    path(b, created, "child.py");
    content(b, "backslash folder");
    CHECK(fs_create(root, created, "new.py", false, c, error, sizeof(error)));
    path(d, created, "new.py");
    CHECK(!strcmp(c, d));
    content(c, "");
    CHECK(fs_remove(root, created, NULL, NULL, error, sizeof(error)));
    CHECK(fs_kind(a) == FS_DIRECTORY);

    path(a, outside, "job.py");
    put(a, "background");
    paths->count = 1;
    strcpy(paths->paths[0], a);
    FileJob *job = file_job_start(root, root, NULL, paths, error, sizeof(error));
    wait_job(job);
    CHECK(job->success);
    strcpy(created, job->created);
    CHECK(atomic_load(&job->entries) == 1);
    file_job_free(job);
    content(created, "background");
    job = file_job_start(root, NULL, created, NULL, error, sizeof(error));
    wait_job(job);
    CHECK(job->success);
    file_job_free(job);
    CHECK(fs_kind(created) == FS_MISSING);
    CHECK(fs_remove(workspace, root, NULL, NULL, error, sizeof(error)));
    CHECK(fs_remove(workspace, outside, NULL, NULL, error, sizeof(error)));
    CHECK(!rmdir(workspace));
    free(paths);
    free(uri);
    printf("OK %d filesystem assertions: exclusive create, recursive copy/delete, clipboard URIs, links, "
           "rollback, cancellation, race and worker\n",
           checks);
    return 0;
}
