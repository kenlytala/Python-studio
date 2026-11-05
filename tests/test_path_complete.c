#define _POSIX_C_SOURCE 200809L
#include "path_complete.h"
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
static void delay(void) {
#ifdef _WIN32
    Sleep(1);
#else
    struct timespec t = {0, 1000000};
    nanosleep(&t, NULL);
#endif
}
static PathLoadJob *load(const char paths[][FS_PATH], int count, PathContext *ctx, FsGuard guard,
                         void *user) {
    char error[512];
    PathLoadJob *job = path_job_start(paths, count, ctx, guard, user, error, sizeof(error));
    CHECK(job);
    for (int i = 0; !atomic_load(&job->done) && i < 5000; i++)
        delay();
    CHECK(atomic_load(&job->done));
    return job;
}
static char *copy(const char *text) {
    char *p = malloc(strlen(text) + 1);
    CHECK(p);
    strcpy(p, text);
    return p;
}
static char *unmark(const char *text, size_t *cursor) {
    char *s = copy(text), *mark = strchr(s, '|');
    CHECK(mark);
    *cursor = (size_t)(mark - s);
    memmove(mark, mark + 1, strlen(mark));
    return s;
}
static void json_string(const char *text) {
    putchar('"');
    for (const unsigned char *p = (const unsigned char *)text; *p; p++) {
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
static void run(const char *before, const char *name, bool dir, const char *expected) {
    size_t cursor, target;
    char *text = unmark(before, &cursor), *after = unmark(expected, &target);
    PathContext ctx;
    CHECK(path_completion_context(text, strlen(text), cursor, &ctx));
    PathChoice choice = {.directory = dir};
    strcpy(choice.name, name);
    strcpy(choice.parent, "/base");
    XmlEdit edit = {0};
    char error[512];
    CHECK(path_choice_edit(text, strlen(text), &ctx, &choice, &edit, error, sizeof(error)));
    Buffer *b = calloc(1, sizeof(*b));
    CHECK(b && buf_init(b, text, "view.xml"));
    b->cursor = cursor;
    CHECK(xml_edit_apply(b, &edit));
    CHECK(!strcmp(b->text, after) && b->cursor == target);
    if (b->undo.count) {
        CHECK(b->undo.count == 1 && buf_undo(b) && !strcmp(b->text, text) && b->cursor == cursor);
        CHECK(buf_redo(b) && !strcmp(b->text, after) && b->cursor == target);
    }
    free(edit.text);
    buf_free(b);
    free(b);
    free(text);
    free(after);
}
static int probe(int argc, char **argv) {
    if (argc < 5)
        return 2;
    FILE *file = fopen(argv[2], "rb");
    if (!file)
        return 2;
    char *text = malloc(BUF_LIMIT + 2);
    if (!text) {
        fclose(file);
        return 2;
    }
    size_t n = fread(text, 1, BUF_LIMIT + 1, file);
    bool ok = !ferror(file) && n <= BUF_LIMIT;
    fclose(file);
    if (!ok || memchr(text, 0, n)) {
        free(text);
        return 2;
    }
    text[n] = 0;
    char *end;
    errno = 0;
    unsigned long long cursor = strtoull(argv[3], &end, 10);
    if (errno || *end || cursor > n) {
        free(text);
        return 2;
    }
    PathContext ctx;
    if (!path_completion_context(text, n, (size_t)cursor, &ctx)) {
        puts("{\"context\":null}");
        free(text);
        return 0;
    }
    printf("{\"context\":{\"start\":%zu,\"end\":%zu,\"component\":%zu,\"component_end\":%zu,\"directory\":",
           ctx.start, ctx.end, ctx.component, ctx.component_end);
    json_string(ctx.directory);
    printf(",\"prefix\":");
    json_string(ctx.prefix);
    printf(",\"quote\":");
    char quote[2] = {ctx.quote, 0};
    json_string(quote);
    putchar('}');
    if (!strcmp(argv[4], "edit") && argc == 7) {
        PathChoice item = {.directory = !strcmp(argv[6], "1")};
        if (strlen(argv[5]) >= sizeof(item.name)) {
            free(text);
            return 2;
        }
        strcpy(item.name, argv[5]);
        XmlEdit plan = {0};
        char error[512];
        Buffer *b = calloc(1, sizeof(*b));
        CHECK(b && buf_init(b, text, "view.xml"));
        b->cursor = (size_t)cursor;
        bool edited =
            path_choice_edit(text, n, &ctx, &item, &plan, error, sizeof(error)) && xml_edit_apply(b, &plan);
        free(plan.text);
        char *after = copy(b->text);
        size_t caret = b->cursor;
        int undo = b->undo.count;
        bool history = true;
        if (undo)
            history = buf_undo(b) && !strcmp(b->text, text) && b->cursor == cursor && buf_redo(b) &&
                      !strcmp(b->text, after) && b->cursor == caret;
        printf(",\"ok\":%s,\"text\":", edited ? "true" : "false");
        json_string(after);
        printf(",\"cursor\":%zu,\"undo\":%d,\"history\":%s", caret, undo, history ? "true" : "false");
        free(after);
        buf_free(b);
        free(b);
    } else if (strcmp(argv[4], "context")) {
        free(text);
        return 2;
    }
    puts("}");
    free(text);
    return 0;
}
static int list(int argc, char **argv) {
    if (argc < 4 || argc - 3 > PATH_ROOTS_MAX || strlen(argv[2]) >= FS_PATH)
        return 2;
    char paths[PATH_ROOTS_MAX][FS_PATH];
    PathContext ctx = {0};
    if (strlen(argv[3]) >= FS_PATH)
        return 2;
    strcpy(ctx.prefix, argv[3]);
    strcpy(paths[0], argv[2]);
    int count = 1;
    for (int i = 4; i < argc; i++) {
        if (strlen(argv[i]) >= FS_PATH)
            return 2;
        strcpy(paths[count++], argv[i]);
    }
    PathLoadJob *job = load((const char (*)[FS_PATH])paths, count, &ctx, NULL, NULL);
    printf("{\"ok\":%s,\"limited\":%s,\"scanned\":%u,\"items\":[", job->success ? "true" : "false",
           job->report.limited ? "true" : "false", job->report.scanned);
    for (int i = 0; i < job->report.count; i++) {
        if (i)
            putchar(',');
        PathChoice *item = &job->report.items[i];
        printf("{\"name\":");
        json_string(item->name);
        printf(",\"directory\":%s,\"parent\":", item->directory ? "true" : "false");
        json_string(item->parent);
        putchar('}');
    }
    printf("],\"error\":");
    json_string(job->error);
    puts("}");
    path_job_free(job);
    return 0;
}
static bool deny(void *user, const char *path, bool leaf, char *error, size_t cap) {
    (void)path;
    (void)leaf;
    (*(int *)user)++;
    snprintf(error, cap, "offline test guard");
    return false;
}
static void write_file(const char *root, const char *name) {
    char path[FS_PATH];
    CHECK(snprintf(path, sizeof(path), "%s/%s", root, name) < FS_PATH);
    FILE *f = fopen(path, "wb");
    CHECK(f && fputs("untouched source", f) >= 0 && !fclose(f));
}
typedef struct {
    atomic_bool entered, release;
} Gate;
static bool gate_guard(void *user, const char *path, bool leaf, char *error, size_t cap) {
    (void)path;
    (void)leaf;
    (void)error;
    (void)cap;
    Gate *gate = user;
    atomic_store(&gate->entered, true);
    for (int i = 0; i < 5000 && !atomic_load(&gate->release); i++)
        delay();
    return true;
}
static void workers(void) {
    char directory[FS_PATH];
#ifdef _WIN32
    char base[MAX_PATH];
    CHECK(GetTempPathA(MAX_PATH, base) && GetTempFileNameA(base, "pct", 0, directory));
    CHECK(!remove(directory) && !mkdir(directory, 0700));
#else
    strcpy(directory, "/tmp/studio-paths-XXXXXX");
    CHECK(mkdtemp(directory));
#endif
    char paths[PATH_ROOTS_MAX][FS_PATH], view[FS_PATH], other[FS_PATH];
    strcpy(paths[0], directory);
    CHECK(snprintf(view, sizeof(view), "%s/views", directory) < FS_PATH && !mkdir(view, 0700));
    CHECK(snprintf(other, sizeof(other), "%s/other", directory) < FS_PATH && !mkdir(other, 0700));
    write_file(directory, "z.py");
    write_file(directory, "a.xml");
    write_file(directory, "été.xml");
    write_file(directory, ".hidden");
    write_file(other, "a.xml");
    write_file(other, "b.xml");
    strcpy(paths[1], other);
    PathContext ctx = {0};
    PathLoadJob *job = load((const char (*)[FS_PATH])paths, 2, &ctx, NULL, NULL);
    CHECK(job->success && job->report.count == 7 && job->report.directories == 2);
    CHECK(!strcmp(job->report.items[0].name, "other") && job->report.items[0].directory);
    CHECK(!strcmp(job->report.items[1].name, "views") && job->report.items[1].directory);
    CHECK(!strcmp(job->report.items[3].name, "a.xml") && !strcmp(job->report.items[3].parent, directory));
    path_job_free(job);
    strcpy(ctx.prefix, "a");
    job = load((const char (*)[FS_PATH])paths, 2, &ctx, NULL, NULL);
    CHECK(job->success && job->report.count == 1 && !strcmp(job->report.items[0].name, "a.xml"));
    path_job_free(job);
    int guards = 0;
    job = load((const char (*)[FS_PATH])paths, 2, &ctx, deny, &guards);
    CHECK(!job->success && guards == 1 && strstr(job->error, "offline"));
    path_job_free(job);
    ctx.prefix[0] = 0;
    for (int i = 0; i < 60; i++) {
        char name[32];
        snprintf(name, sizeof(name), "f%03d.xml", i);
        write_file(directory, name);
    }
    job = load((const char (*)[FS_PATH])paths, 2, &ctx, NULL, NULL);
    CHECK(job->success && job->report.count == PATH_CHOICES_MAX && job->report.limited);
    path_job_free(job);
    Gate gate;
    atomic_init(&gate.entered, false);
    atomic_init(&gate.release, false);
    char cancel_error[512];
    job = path_job_start((const char (*)[FS_PATH])paths, 2, &ctx, gate_guard, &gate, cancel_error,
                         sizeof(cancel_error));
    CHECK(job);
    for (int i = 0; i < 5000 && !atomic_load(&gate.entered); i++)
        delay();
    CHECK(atomic_load(&gate.entered));
    path_job_cancel(job);
    CHECK(atomic_load(&job->cancel) && !atomic_load(&job->done));
    atomic_store(&gate.release, true);
    for (int i = 0; i < 5000 && !atomic_load(&job->done); i++)
        delay();
    CHECK(atomic_load(&job->done) && !job->success && !job->report.scanned);
    path_job_free(job);
    for (int i = 0; i < 60; i++) {
        char path[FS_PATH];
        CHECK(snprintf(path, sizeof(path), "%s/f%03d.xml", directory, i) < FS_PATH && !remove(path));
    }
    const char *files[] = {"z.py", "a.xml", "été.xml", ".hidden", "other/a.xml", "other/b.xml"};
    for (size_t i = 0; i < sizeof(files) / sizeof(*files); i++) {
        char path[FS_PATH];
        CHECK(snprintf(path, sizeof(path), "%s/%s", directory, files[i]) < FS_PATH && !remove(path));
    }
    CHECK(!rmdir(view) && !rmdir(other) && !rmdir(directory));
}
int main(int argc, char **argv) {
    if (argc > 1 && !strcmp(argv[1], "--probe"))
        return probe(argc, argv);
    if (argc > 1 && !strcmp(argv[1], "--list"))
        return list(argc, argv);
    run("sale/|", "views", true, "sale/views/|");
    run("sale/vi|ews", "views", true, "sale/views/|");
    run("sale/vi|ews/file.xml", "views", true, "sale/views/|file.xml");
    run("sale/|", "mes vues", true, "\"sale/mes vues/|\"");
    run("<field file=\"sale/|\"/>", "views", true, "<field file=\"sale/views/|\"/>");
    run("open('sale/vi|')", "views.xml", false, "open('sale/views.xml|')");
    run("open(\"sale/vi|", "views", true, "open(\"sale/views/|");
    run("\"sale/\"|", "views", true, "\"sale/views/|\"");
    run("sale/a.xml|", "a.xml", false, "sale/a.xml|");
    run("# été\n猫/sale/vi|ews", "vues_été", true, "# été\n猫/sale/vues_été/|");
    run("sale/|", "vues\xe2\x80\x83été.xml", false, "\"sale/vues\xe2\x80\x83été.xml|\"");
    PathContext ctx;
    const char *invalid[] = {"sale",         "xml_form",          "<fi",     "</",
                             "<field /",     "<field name='x' /", "sale/ ",  "http://sale/",
                             "file:///tmp/", "`sale/`",           "x = 2 / "};
    for (size_t i = 0; i < sizeof(invalid) / sizeof(*invalid); i++)
        CHECK(!path_completion_context(invalid[i], strlen(invalid[i]), strlen(invalid[i]), &ctx));
    CHECK(!path_completion_context("sale/", 5, 99, &ctx));
    CHECK(!path_completion_context("猫/sale/", strlen("猫/sale/"), 1, &ctx));
    CHECK(path_completion_context("sale/", 5, 5, &ctx));
    PathChoice invalid_name = {0};
    strcpy(invalid_name.name, "../bad");
    XmlEdit plan = {0};
    char error[512];
    CHECK(!path_choice_edit("sale/", 5, &ctx, &invalid_name, &plan, error, sizeof(error)) && !plan.text);
    strcpy(invalid_name.name, "valid.xml");
    CHECK(path_completion_context("猫/sale/", strlen("猫/sale/"), strlen("猫/sale/"), &ctx));
    ctx.start = 1;
    CHECK(
        !path_choice_edit("猫/sale/", strlen("猫/sale/"), &ctx, &invalid_name, &plan, error, sizeof(error)));
    CHECK(path_completion_context("sale/", 5, 5, &ctx));
    memset(invalid_name.name, 'a', sizeof(invalid_name.name));
    CHECK(!path_choice_edit("sale/", 5, &ctx, &invalid_name, &plan, error, sizeof(error)));
    char candidates[PATH_ROOTS_MAX][FS_PATH] = {{0}};
    CHECK(!path_job_start((const char (*)[FS_PATH])candidates, 0, &ctx, NULL, NULL, error, sizeof(error)));
    CHECK(!path_job_start((const char (*)[FS_PATH])candidates, PATH_ROOTS_MAX + 1, &ctx, NULL, NULL, error,
                          sizeof(error)));
    memset(ctx.prefix, 'x', sizeof(ctx.prefix));
    CHECK(!path_job_start((const char (*)[FS_PATH])candidates, 1, &ctx, NULL, NULL, error, sizeof(error)));
    workers();
    printf("%d native path-completion assertions passed\n", checks);
    return 0;
}
