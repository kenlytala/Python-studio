#define _POSIX_C_SOURCE 200809L
#include "session.h"
#include "files.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
static int checks;
#define CHECK(x)                                                                                             \
    do {                                                                                                     \
        checks++;                                                                                            \
        if (!(x)) {                                                                                          \
            fprintf(stderr, "session FAIL %d: %s\n", __LINE__, #x);                                          \
            exit(1);                                                                                         \
        }                                                                                                    \
    } while (0)
static char base[BUF_PATH], project[BUF_PATH], directory[BUF_PATH], file[BUF_PATH], error[1024];
static void join(char *out, const char *a, const char *b) {
    CHECK(strlen(a) + strlen(b) + 2 < BUF_PATH);
    strcpy(out, a);
    strcat(out, "/");
    strcat(out, b);
}
static void write_file(const char *path, const char *text) {
    FILE *f = fopen(path, "wb");
    CHECK(f);
    CHECK(fwrite(text, 1, strlen(text), f) == strlen(text));
    CHECK(!fclose(f));
}
static char *bytes(const char *path, size_t *len) {
    FILE *f = fopen(path, "rb");
    CHECK(f);
    CHECK(!fseek(f, 0, SEEK_END));
    long n = ftell(f);
    CHECK(n >= 0);
    rewind(f);
    char *p = malloc((size_t)n + 1);
    CHECK(p);
    CHECK(fread(p, 1, (size_t)n, f) == (size_t)n);
    p[n] = 0;
    fclose(f);
    *len = (size_t)n;
    return p;
}
static StudioImage *sample(void) {
    StudioImage *i = calloc(1, sizeof(*i));
    CHECK(i);
    strcpy(i->project, project);
    i->count = 2;
    i->active = 1;
    i->active_pane = 1;
    i->panes[0] = 0;
    i->panes[1] = 1;
    i->flags = ST_SPLIT | ST_TERMINAL | ST_TOPBAR;
    i->ratio = .63;
    i->terminal_height = 320;
    i->font_size = 22;
    i->sidebar_width = 280;
    i->tree_scroll = 5;
    i->console_mode = 1;
    i->expanded_count = 1;
    strcpy(i->expanded[0], project);
    for (int n = 0; n < 2; n++) {
        i->tabs[n] = calloc(1, sizeof(Buffer));
        CHECK(i->tabs[n]);
        CHECK(buf_init(i->tabs[n], n ? "étoile\nsecond\n" : "alpha\n", "main.py"));
    }
    strcpy(i->tabs[0]->path, file);
    CHECK(buf_insert(i->tabs[0], "# draft\n"));
    strcpy(i->tabs[1]->name, "sans_titre.py");
    CHECK(buf_insert(i->tabs[1], "# non enregistré\n"));
    BufCaret carets[] = {{4, 0}, {12, BUF_NO_SELECTION}};
    buf_set_carets(i->tabs[1], carets, 2);
    i->tabs[1]->scroll_x = 35.5f;
    i->tabs[1]->scroll_y = 145.25f;
    i->tabs[1]->crlf = i->tabs[1]->bom = true;
    return i;
}
static void reset_file(void) {
    unlink(file);
    write_file(file, "alpha\n");
}
static void roundtrip(void) {
    StudioStore s;
    studio_store_init(&s, directory, true);
    StudioImage *i = sample(), *r = calloc(1, sizeof(*r));
    CHECK(r);
    reset_file();
    CHECK(studio_save(&s, i, error, sizeof(error)));
    struct stat st;
    CHECK(!stat(s.path, &st));
    CHECK((st.st_mode & 0777) == 0600);
    char last[BUF_PATH];
    CHECK(studio_last_project(&s, last, sizeof(last)));
    CHECK(!strcmp(last, project));
    CHECK(studio_load(&s, project, r, error, sizeof(error)) == 1);
    CHECK(r->count == 2 && r->active == 1 && r->panes[1] == 1 && r->active_pane == 1);
    CHECK(r->flags == i->flags && r->ratio == .63);
    CHECK(!strcmp(r->tabs[0]->text, i->tabs[0]->text) && buf_dirty(r->tabs[0]));
    CHECK(!*r->tabs[1]->path && buf_dirty(r->tabs[1]));
    CHECK(r->tabs[1]->scroll_x == 35.5f && r->tabs[1]->scroll_y == 145.25f);
    CHECK(r->tabs[1]->crlf && r->tabs[1]->bom);
    CHECK(buf_caret_count(r->tabs[1]) == 2);
    SshSession ssh;
    ssh_init(&ssh, NULL, "/tmp/unused-helper");
    CHECK(studio_reconcile(r, &ssh, false) == 0);
    CHECK(!strcmp(r->tabs[0]->path, file) && buf_dirty(r->tabs[0]));
    studio_image_free(r);
    free(r);
    studio_image_free(i);
    free(i);
    studio_store_close(&s);
}
static void conflicts_and_missing(void) {
    StudioStore s;
    char dir[BUF_PATH];
    join(dir, base, "conflicts");
    studio_store_init(&s, dir, true);
    StudioImage *i = sample(), *r = calloc(1, sizeof(*r));
    CHECK(r);
    reset_file();
    CHECK(studio_save(&s, i, error, sizeof(error)));
    write_file(file, "external = 42\n");
    CHECK(studio_load(&s, project, r, error, sizeof(error)) == 1);
    SshSession ssh;
    ssh_init(&ssh, NULL, "/tmp/unused-helper");
    CHECK(studio_reconcile(r, &ssh, false) == 1);
    CHECK(!*r->tabs[0]->path && !strcmp(r->tabs[0]->recovery_origin, file));
    CHECK(!strcmp(r->tabs[0]->text, i->tabs[0]->text));
    size_t n;
    char *raw = bytes(file, &n);
    CHECK(!strcmp(raw, "external = 42\n"));
    free(raw);
    studio_image_free(r);
    unlink(file);
    CHECK(studio_load(&s, project, r, error, sizeof(error)) == 1);
    CHECK(studio_reconcile(r, &ssh, false) == 1);
    CHECK(!*r->tabs[0]->path && buf_dirty(r->tabs[0]));
    studio_image_free(r);
    CHECK(!mkfifo(file, 0600));
    CHECK(studio_load(&s, project, r, error, sizeof(error)) == 1);
    CHECK(studio_reconcile(r, &ssh, false) == 1);
    CHECK(!*r->tabs[0]->path);
    unlink(file);
    studio_image_free(r);
    free(r);
    studio_image_free(i);
    free(i);
    studio_store_close(&s);
}
static void clean_refresh_and_discard(void) {
    StudioStore s;
    char dir[BUF_PATH];
    join(dir, base, "clean");
    studio_store_init(&s, dir, true);
    StudioImage *i = sample(), *r = calloc(1, sizeof(*r));
    CHECK(r);
    i->discard[0] = true;
    reset_file();
    CHECK(studio_save(&s, i, error, sizeof(error)));
    write_file(file, "é\n");
    CHECK(studio_load(&s, project, r, error, sizeof(error)) == 1);
    SshSession ssh;
    ssh_init(&ssh, NULL, "/tmp/unused-helper");
    unsigned long previous_version = r->tabs[0]->version;
    CHECK(studio_reconcile(r, &ssh, false) == 0);
    CHECK(r->tabs[0]->version > previous_version);
    CHECK(!strcmp(r->tabs[0]->text, "é\n") && !buf_dirty(r->tabs[0]));
    CHECK(!strcmp(r->tabs[0]->path, file));
    studio_image_free(r);
    free(r);
    studio_image_free(i);
    free(i);
    studio_store_close(&s);
}
static void corruption_and_bounds(void) {
    StudioStore s;
    char dir[BUF_PATH];
    join(dir, base, "bad");
    studio_store_init(&s, dir, true);
    StudioImage *i = sample(), *r = calloc(1, sizeof(*r));
    CHECK(r);
    CHECK(studio_save(&s, i, error, sizeof(error)));
    char old[BUF_PATH];
    strcpy(old, s.path);
    FILE *f = fopen(old, "r+b");
    CHECK(f);
    CHECK(!fseek(f, -1, SEEK_END));
    int c = fgetc(f);
    CHECK(c != EOF);
    CHECK(!fseek(f, -1, SEEK_END));
    CHECK(fputc(c ^ 1, f) != EOF);
    fclose(f);
    size_t n;
    char *before = bytes(old, &n);
    CHECK(studio_load(&s, project, r, error, sizeof(error)) == -1);
    CHECK(r->count == 0 && !r->tabs[0]);
    CHECK(strcmp(s.path, old));
    CHECK(studio_save(&s, i, error, sizeof(error)));
    size_t m;
    char *after = bytes(old, &m);
    CHECK(m == n && !memcmp(before, after, n));
    free(before);
    free(after);
    i->active = 24;
    CHECK(!studio_save(&s, i, error, sizeof(error)));
    i->active = 1;
    studio_image_free(r);
    free(r);
    studio_image_free(i);
    free(i);
    studio_store_close(&s);
}
static void symlink_and_concurrent(void) {
    StudioStore a, b;
    char dir[BUF_PATH];
    join(dir, base, "parallel");
    studio_store_init(&a, dir, true);
    studio_store_init(&b, dir, true);
    StudioImage *i = sample(), *r = calloc(1, sizeof(*r));
    CHECK(r);
    CHECK(studio_save(&a, i, error, sizeof(error)));
    CHECK(studio_load(&b, project, r, error, sizeof(error)) == 1);
    CHECK(strcmp(a.path, b.path));
    CHECK(studio_save(&b, r, error, sizeof(error)));
    CHECK(!access(a.path, F_OK) && !access(b.path, F_OK));
    size_t before_size;
    char *before = bytes(a.path, &before_size);
    write_file(b.path, "broken other window");
    struct timespec old_time[2] = {{1, 0}, {1, 0}}, new_time[2] = {{2, 0}, {2, 0}};
    CHECK(!utimensat(AT_FDCWD, a.path, old_time, 0));
    CHECK(!utimensat(AT_FDCWD, b.path, new_time, 0));
    studio_store_close(&a);
    studio_store_close(&b);
    StudioStore c;
    studio_store_init(&c, dir, true);
    StudioImage *bad = calloc(1, sizeof(*bad));
    CHECK(bad);
    CHECK(studio_load(&c, project, bad, error, sizeof(error)) == -1);
    CHECK(strcmp(a.path, c.path));
    CHECK(studio_save(&c, i, error, sizeof(error)));
    size_t after_size;
    char *after = bytes(a.path, &after_size);
    CHECK(before_size == after_size && !memcmp(before, after, before_size));
    free(before);
    free(after);
    studio_image_free(bad);
    free(bad);
    studio_store_close(&c);
    char target[BUF_PATH];
    join(target, base, "sentinel");
    write_file(target, "KEEP");
    CHECK(!unlink(a.path));
    CHECK(!symlink(target, a.path));
    CHECK(!studio_save(&a, i, error, sizeof(error)));
    size_t n;
    char *raw = bytes(target, &n);
    CHECK(!strcmp(raw, "KEEP"));
    free(raw);
    studio_image_free(r);
    free(r);
    studio_image_free(i);
    free(i);
    studio_store_close(&a);
    studio_store_close(&b);
}
static void offline_remote_and_no_session(void) {
    StudioStore s;
    char dir[BUF_PATH], cache[BUF_PATH];
    join(dir, base, "remote");
    join(cache, base, "cache");
    studio_store_init(&s, dir, true);
    StudioImage *i = sample(), *r = calloc(1, sizeof(*r));
    CHECK(r);
    SshSession ssh;
    ssh_init(&ssh, cache, "/tmp/unused-helper");
    strcpy(i->profile.id, "0123456789abcdef0123456789abcdef");
    strcpy(i->profile.host, "host.invalid");
    strcpy(i->profile.user, "odoo");
    strcpy(i->profile.port, "2222");
    strcpy(i->profile.folder, "/remote");
    join(i->remote_root, cache, i->profile.id);
    join(i->tabs[0]->path, i->remote_root, "main.py");
    i->remote[0] = true;
    CHECK(studio_save(&s, i, error, sizeof(error)));
    CHECK(studio_load(&s, project, r, error, sizeof(error)) == 1);
    CHECK(studio_reconcile(r, &ssh, false) == 0);
    CHECK(ssh.state == SSH_LOST && ssh.process == 0 && ssh.generation == 0);
    CHECK(r->tabs[0]->session_pending_remote);
    CHECK(!strcmp(r->tabs[0]->text, i->tabs[0]->text));
    CHECK(!buf_save_guarded(r->tabs[0], r->tabs[0]->path, ssh_save_guard, &ssh, error, sizeof(error)));
    CHECK(studio_reconcile(r, &ssh, true) == 1);
    CHECK(!*r->tabs[0]->path && buf_dirty(r->tabs[0]));
    CHECK(!r->tabs[0]->session_pending_remote);
    studio_image_free(r);
    free(r);
    studio_image_free(i);
    free(i);
    studio_store_close(&s);
    join(dir, base, "disabled");
    studio_store_init(&s, dir, false);
    i = sample();
    CHECK(studio_save(&s, i, error, sizeof(error)));
    CHECK(access(dir, F_OK));
    CHECK(!studio_last_project(&s, project, sizeof(project)));
    studio_image_free(i);
    free(i);
    studio_store_close(&s);
}
int main(void) {
    strcpy(base, "/tmp/python-studio-session-XXXXXX");
    CHECK(mkdtemp(base));
    join(project, base, "project");
    CHECK(!mkdir(project, 0700));
    join(file, project, "main.py");
    join(directory, base, "sessions");
    roundtrip();
    conflicts_and_missing();
    clean_refresh_and_discard();
    corruption_and_bounds();
    symlink_and_concurrent();
    offline_remote_and_no_session();
    char command[BUF_PATH + 32];
    snprintf(command, sizeof(command), "rm -rf -- '%s'", base);
    CHECK(!system(command));
    printf("Native sessions: %d assertions, 6 groups OK\n", checks);
    return 0;
}
