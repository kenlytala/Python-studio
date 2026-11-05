#define _POSIX_C_SOURCE 200809L
#include "import_paths.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif
static int checks;
#define CHECK(c)                                                                                             \
    do {                                                                                                     \
        checks++;                                                                                            \
        if (!(c)) {                                                                                          \
            fprintf(stderr, "FAIL %d: %s\n", __LINE__, #c);                                                  \
            exit(1);                                                                                         \
        }                                                                                                    \
    } while (0)
static void write_file(const char *file, const char *text, size_t n) {
    FILE *f = fopen(file, "wb");
    CHECK(f);
    CHECK(fwrite(text, 1, n, f) == n);
    CHECK(!fclose(f));
}
int main(void) {
    char error[512], directory[BUF_PATH], settings[BUF_PATH], first[BUF_PATH], second[BUF_PATH];
#ifdef _WIN32
    char temp[MAX_PATH];
    CHECK(GetTempPathA(MAX_PATH, temp));
    CHECK(GetTempFileNameA(temp, "imp", 0, directory));
    CHECK(!remove(directory));
    CHECK(!_mkdir(directory));
#else
    strcpy(directory, "/tmp/prisme-imports-XXXXXX");
    CHECK(mkdtemp(directory));
#endif
    CHECK(snprintf(settings, sizeof(settings), "%s/settings.cfg", directory) < BUF_PATH);
    ImportPaths *paths = calloc(1, sizeof(*paths)), *loaded = calloc(1, sizeof(*loaded)),
                *before = calloc(1, sizeof(*before));
    CHECK(paths && loaded && before && import_paths_valid(paths));
    CHECK(!import_paths_add(paths, "relative/path", error, sizeof(error)) && *error);
    CHECK(!import_paths_add(paths, "", error, sizeof(error)));
    CHECK(!import_paths_add(paths, "/bad\xff", error, sizeof(error)));
    char large[BUF_PATH + 1];
    memset(large, 'x', BUF_PATH);
    large[0] = '/';
    large[BUF_PATH] = 0;
    CHECK(!import_paths_add(paths, large, error, sizeof(error)) && paths->count == 0);
    const char *special = "/sdk/été, ;'\"[brackets]\\and\nnewline";
    CHECK(import_paths_add(paths, special, error, sizeof(error)) && paths->count == 1);
    CHECK(!import_paths_add(paths, special, error, sizeof(error)) && paths->count == 1);
    for (int i = 1; i < IMPORT_PATHS_MAX; i++) {
        char name[80];
        snprintf(name, sizeof(name), "/sdk/engine-%d", i);
        CHECK(import_paths_add(paths, name, error, sizeof(error)));
    }
    CHECK(import_paths_valid(paths) && paths->count == IMPORT_PATHS_MAX);
    CHECK(!import_paths_add(paths, "/one-too-many", error, sizeof(error)));
    CHECK(!import_paths_remove(paths, -1) && !import_paths_remove(paths, paths->count));
    CHECK(import_paths_file(first, sizeof(first), settings, "/projects/atelier A"));
    CHECK(import_paths_file(second, sizeof(second), settings, "/projects/atelier B"));
    CHECK(strcmp(first, second));
    char same[BUF_PATH];
    CHECK(import_paths_file(same, sizeof(same), settings, "/projects/atelier A") && !strcmp(same, first));
    CHECK(!import_paths_file(same, 8, settings, "/projects/atelier A"));
    CHECK(!import_paths_file(same, sizeof(same), settings, "relative"));
    CHECK(!import_paths_load(loaded, first, "/projects/atelier A", error, sizeof(error)) && !*error);
    CHECK(import_paths_save(paths, first, "/projects/atelier A", error, sizeof(error)));
    CHECK(import_paths_load(loaded, first, "/projects/atelier A", error, sizeof(error)));
    CHECK(loaded->count == paths->count);
    for (int i = 0; i < paths->count; i++)
        CHECK(!strcmp(loaded->paths[i], paths->paths[i]));
#ifndef _WIN32
    struct stat mode;
    CHECK(!stat(first, &mode) && (mode.st_mode & 0777) == 0600);
#endif
    *before = *loaded;
    CHECK(!import_paths_load(loaded, first, "/projects/atelier B", error, sizeof(error)) && *error);
    CHECK(!memcmp(before, loaded, sizeof(*loaded)));
    CHECK(import_paths_remove(paths, 0) && !strcmp(paths->paths[0], "/sdk/engine-1"));
    CHECK(import_paths_remove(paths, paths->count - 1));
    CHECK(import_paths_save(paths, second, "/projects/atelier B", error, sizeof(error)));
    memset(loaded, 0, sizeof(*loaded));
    CHECK(import_paths_load(loaded, second, "/projects/atelier B", error, sizeof(error)));
    CHECK(loaded->count == 6 && !strcmp(loaded->paths[5], "/sdk/engine-6"));
    paths->count = -1;
    CHECK(!import_paths_valid(paths) && !import_paths_remove(paths, 0));
    paths->count = IMPORT_PATHS_MAX + 1;
    CHECK(!import_paths_valid(paths));
    memset(paths, 0, sizeof(*paths));
    large[BUF_PATH - 1] = 0;
    CHECK(import_paths_add(paths, large, error, sizeof(error)));
    CHECK(import_paths_save(paths, first, "/projects/atelier A", error, sizeof(error)));
    CHECK(import_paths_load(loaded, first, "/projects/atelier A", error, sizeof(error)) &&
          strlen(loaded->paths[0]) == BUF_PATH - 1);
    memset(paths, 0, sizeof(*paths));
    CHECK(import_paths_save(paths, first, "/projects/atelier A", error, sizeof(error)));
    CHECK(import_paths_load(loaded, first, "/projects/atelier A", error, sizeof(error)) &&
          loaded->count == 0);
    CHECK(import_paths_add(loaded, "/do-not-lose", error, sizeof(error)));
    *before = *loaded;
    const char *corrupt[] = {
        "",
        "PRISME-IMPORTS\t2\n",
        "PRISME-IMPORTS\t1\nPROJECT\tzz\n",
        "PRISME-IMPORTS\t1\nPROJECT\t2f70726f6a656374732f6174656c6965722041\nPATH\t00\n",
        "PRISME-IMPORTS\t1\nPROJECT\t2f70726f6a656374732f6174656c6965722041\nPATH\t2f61\nPATH\t2f61\n",
        "PRISME-IMPORTS\t1\nPROJECT\t2f70726f6a656374732f6174656c6965722041\nPATH\t2fff\n",
        "PRISME-IMPORTS\t1\nPROJECT\t2f70726f6a656374732f6174656c6965722041\nUNKNOWN\t2f61\n"};
    for (size_t i = 0; i < sizeof(corrupt) / sizeof(*corrupt); i++) {
        write_file(first, corrupt[i], strlen(corrupt[i]));
        CHECK(!import_paths_load(loaded, first, "/projects/atelier A", error, sizeof(error)) && *error);
        CHECK(!memcmp(before, loaded, sizeof(*loaded)));
    }
    const char binary[] = "PRISME-IMPORTS\t1\n\0PROJECT\t2f61\n";
    write_file(first, binary, sizeof(binary) - 1);
    CHECK(!import_paths_load(loaded, first, "/projects/atelier A", error, sizeof(error)));
    CHECK(!memcmp(before, loaded, sizeof(*loaded)));
    char *huge = malloc(100000);
    CHECK(huge);
    memset(huge, 'A', 100000);
    write_file(first, huge, 100000);
    free(huge);
    CHECK(!import_paths_load(loaded, first, "/projects/atelier A", error, sizeof(error)) &&
          !memcmp(before, loaded, sizeof(*loaded)));
    CHECK(import_paths_save(before, first, "/projects/atelier A", error, sizeof(error)));
    char bad[BUF_PATH];
    CHECK(snprintf(bad, sizeof(bad), "%s/missing/profile.cfg", directory) < BUF_PATH);
    CHECK(!import_paths_save(before, bad, "/projects/atelier A", error, sizeof(error)) && *error);
    CHECK(import_paths_load(loaded, first, "/projects/atelier A", error, sizeof(error)) &&
          loaded->count == 1);
    CHECK(!remove(first) && !remove(second));
#ifdef _WIN32
    CHECK(!_rmdir(directory));
#else
    CHECK(!rmdir(directory));
#endif
    free(paths);
    free(loaded);
    free(before);
    printf("OK %d import-path checks: bounds, UTF-8, eight roots, project scope, atomic profiles, "
           "corrupt/truncated/binary data, no window\n",
           checks);
    return 0;
}
