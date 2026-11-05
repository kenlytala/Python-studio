#define _POSIX_C_SOURCE 200809L
#include "completion.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#else
#include <sys/stat.h>
#include <signal.h>
#include <time.h>
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
static void pause_ms(void) {
#ifdef _WIN32
    Sleep(5);
#else
    struct timespec delay = {0, 5000000};
    nanosleep(&delay, NULL);
#endif
}
static void folder(const char *path) {
#ifdef _WIN32
    CHECK(!_mkdir(path));
#else
    CHECK(!mkdir(path, 0700));
#endif
}
static void write_file(const char *path, const char *text) {
    FILE *f = fopen(path, "wb");
    CHECK(f && fwrite(text, 1, strlen(text), f) == strlen(text) && !fclose(f));
}
static bool has(Completer *c, const char *word) {
    for (int i = 0; i < c->report.count; i++)
        if (!strcmp(c->report.items[i].name, word))
            return true;
    return false;
}
static void send(Completer *c, const char *python, const char *project, const ImportPaths *paths,
                 const char *filename, const char *source, unsigned long version, size_t cursor) {
    char error[512];
    double until = runner_clock() + 10;
    bool sent = false;
    while (!sent && runner_clock() < until) {
        completion_poll(c, runner_clock());
        sent = completion_submit(c, python, project, paths, filename, source, version, cursor, error,
                                 sizeof(error));
        if (!sent && *error)
            fprintf(stderr, "Submit: %s\n", error);
        CHECK(sent || !*error);
        if (!sent)
            pause_ms();
    }
    CHECK(sent);
    until = runner_clock() + COMPLETION_TIMEOUT + 2;
    while (!c->ready && runner_clock() < until) {
        completion_poll(c, runner_clock());
        pause_ms();
    }
    CHECK(c->ready && !c->awaiting && c->version == version && c->cursor == cursor);
}
int main(int argc, char **argv) {
    CHECK(argc == 2);
    const char *python = argv[1];
    CompletionReport report = {0};
    const char *valid = "PRISME-COMPLETE\t1\t7\t4\t6\t0\t\nC\tC\t43686172\t436c61737365\nEND\t7\t1\n";
    CHECK(completion_parse(&report, valid) && report.sequence == 7 && report.count == 1);
    CHECK(!strcmp(report.items[0].name, "Char") && report.items[0].start == 4 && report.items[0].end == 6);
    const char *bad[] = {"",
                         "PRISME-COMPLETE\t2\t7\t4\t6\t0\t\nEND\t7\t0\n",
                         "PRISME-COMPLETE\t1\t0\t4\t6\t0\t\nEND\t0\t0\n",
                         "PRISME-COMPLETE\t1\t7\t8\t6\t0\t\nEND\t7\t0\n",
                         "PRISME-COMPLETE\t1\t7\t0\t999999999\t0\t\nEND\t7\t0\n",
                         "PRISME-COMPLETE\t1\t7\t0\t0\t2\t\nEND\t7\t0\n",
                         "PRISME-COMPLETE\t1\t7\t0\t0\t0\t00\nEND\t7\t0\n",
                         "PRISME-COMPLETE\t1\t7\t0\t0\t0\t\nC\tX\t61\t\nEND\t7\t1\n",
                         "PRISME-COMPLETE\t1\t7\t0\t0\t0\t\nC\tV\tff\t\nEND\t7\t1\n",
                         "PRISME-COMPLETE\t1\t7\t0\t0\t0\t\nEND\t6\t0\n",
                         "PRISME-COMPLETE\t1\t7\t0\t0\t0\t\nEND\t7\t1\n",
                         "PRISME-COMPLETE\t1\t7\t0\t0\t0\t\nEND\t7\t0\nextra\n"};
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        CHECK(!completion_parse(&report, bad[i]));
        CHECK(report.sequence == 7 && report.count == 1 && !strcmp(report.items[0].name, "Char"));
    }
    for (size_t i = 0; i < strlen(valid); i++) {
        char truncated[512];
        memcpy(truncated, valid, i);
        truncated[i] = 0;
        CHECK(!completion_parse(&report, truncated));
    }
    Buffer b;
    CHECK(buf_init(&b, "# été\nfields.Char_suffix", "test.py"));
    size_t a, z;
    buf_move(&b, strlen("# été\nfields.Ch"), false);
    completion_span(&b, &a, &z);
    CHECK(a == strlen("# été\nfields.") && z == b.len);
    buf_free(&b);
    char base[BUF_PATH], project[BUF_PATH], sdk[BUF_PATH], package[BUF_PATH], filename[BUF_PATH],
        path[BUF_PATH], sentinel[BUF_PATH];
#ifdef _WIN32
    char temp[MAX_PATH];
    CHECK(GetTempPathA(MAX_PATH, temp) && GetTempFileNameA(temp, "cmp", 0, base));
    CHECK(!remove(base));
    folder(base);
#else
    strcpy(base, "/tmp/prisme-completion-XXXXXX");
    CHECK(mkdtemp(base));
#endif
    CHECK(snprintf(project, sizeof(project), "%s/project", base) < BUF_PATH);
    CHECK(snprintf(sdk, sizeof(sdk), "%s/SDK été, ; ' [racine]", base) < BUF_PATH);
    CHECK(snprintf(package, sizeof(package), "%s/odoo", sdk) < BUF_PATH);
    CHECK(snprintf(filename, sizeof(filename), "%s/model.py", project) < BUF_PATH);
    CHECK(snprintf(sentinel, sizeof(sentinel), "%s/MUST-NOT-EXECUTE", base) < BUF_PATH);
    folder(project);
    folder(sdk);
    folder(package);
    write_file(filename, "# Original on disk, not edited by completion\n");
    CHECK(snprintf(path, sizeof(path), "%s/__init__.py", package) < BUF_PATH);
    write_file(path, "from . import fields, models, api\nraise RuntimeError('must not import Odoo')\n");
    CHECK(snprintf(path, sizeof(path), "%s/fields.py", package) < BUF_PATH);
    char code[2 * BUF_PATH];
#ifdef _WIN32
    snprintf(code, sizeof(code),
             "raise RuntimeError('not executed')\nclass Char: pass\nclass Integer: pass\n");
#else
    snprintf(code, sizeof(code), "open(%c%s%c,'w').write('BAD')\nclass Char: pass\nclass Integer: pass\n",
             '"', sentinel, '"');
#endif
    write_file(path, code);
    CHECK(snprintf(path, sizeof(path), "%s/models.py", package) < BUF_PATH);
    write_file(path, "class BaseModel:\n    def ensure_one(self): pass\n    def search(self, domain): "
                     "pass\nAbstractModel = BaseModel\nclass Model(AbstractModel): pass\n");
    CHECK(snprintf(path, sizeof(path), "%s/api.py", package) < BUF_PATH);
    write_file(path, "def depends(*args): pass\nclass Environment:\n    def ref(self, xmlid): pass\n");
    CHECK(snprintf(path, sizeof(path), "%s/ast.py", sdk) < BUF_PATH);
    write_file(path, "raise RuntimeError('poisoned project ast.py')\n");
    Completer *service = calloc(1, sizeof(*service));
    CHECK(service);
    completion_init(service);
    ImportPaths paths = {0};
    char error[512];
    CHECK(import_paths_add(&paths, sdk, error, sizeof(error)));
    const char *source = "# été\nfrom odoo import fields, models, api\nfields.Ch";
    send(service, python, project, &paths, filename, source, 11, strlen(source));
    CHECK(!service->report.failed && has(service, "Char") &&
          service->report.start == strlen("# été\nfrom odoo import fields, models, api\nfields."));
    char request[BUF_PATH], adapter[BUF_PATH];
    strcpy(request, service->request_path);
    strcpy(adapter, service->process.temporary);
#ifndef _WIN32
    struct stat st;
    CHECK(!stat(request, &st) && (st.st_mode & 0777) == 0600);
#endif
    int pid = service->process.pid;
    source = "from odoo import models\nclass Demo(models.Model):\n    _name = 'demo.model'\n    def "
             "test(self):\n        self.ens";
    send(service, python, project, &paths, filename, source, 12, strlen(source));
    CHECK(has(service, "ensure_one") && service->process.pid == pid &&
          !strcmp(request, service->request_path));
    /* Snapshot-file transport, not Runner's 4095-byte stdin limit. */
    char *large_source = calloc(1, 90000);
    CHECK(large_source);
    for (int i = 0; i < 2000; i++)
        strcat(large_source, "# Unsaved été UTF-8 padding, not executed\n");
    strcat(large_source, "from odoo import fields, models\nclass Draft(models.Model):\n    title = "
                         "fields.Char()\n    def action(self):\n        self.ti");
    CHECK(strlen(large_source) > 4095);
    send(service, python, project, &paths, filename, large_source, 121, strlen(large_source));
    CHECK(!service->report.failed && has(service, "title") && service->process.pid == pid);
    free(large_source);
    /* Cancellation drains the stopped process; the same service can restart. */
    service->ready = false;
    CHECK(completion_submit(service, python, project, &paths, filename, source, 122, strlen(source), error,
                            sizeof(error)));
    completion_reset(service);
    CHECK(!service->awaiting && !service->ready);
    send(service, python, project, &paths, filename, source, 123, strlen(source));
    CHECK(!service->report.failed && has(service, "ensure_one"));
#ifndef _WIN32
    /* An unexpected OS kill is a visible error, with retry backoff and recovery. */
    CHECK(!kill(service->process.pid, SIGSTOP));
    service->ready = false;
    CHECK(completion_submit(service, python, project, &paths, filename, source, 124, strlen(source), error,
                            sizeof(error)));
    CHECK(!kill(service->process.pid, SIGKILL));
    double deadline = runner_clock() + 5;
    while (!service->ready && runner_clock() < deadline) {
        completion_poll(service, runner_clock());
        pause_ms();
    }
    CHECK(service->ready && service->report.failed && !service->process.running);
    CHECK(!completion_submit(service, python, project, &paths, filename, source, 125, strlen(source), error,
                             sizeof(error)) &&
          !*error);
    completion_reset(service);
    send(service, python, project, &paths, filename, source, 126, strlen(source));
    CHECK(!service->report.failed && has(service, "ensure_one"));
#endif
    CHECK(snprintf(path, sizeof(path), "%s/fields.py", package) < BUF_PATH);
    write_file(path, "class ChangedField: pass\n");
    source = "from odoo import fields\nfields.Changed";
    send(service, python, project, &paths, filename, source, 13, strlen(source));
    CHECK(has(service, "ChangedField"));
    ImportPaths missing = {0};
    CHECK(import_paths_add(&missing, "/no-such-prisme-completion-sdk", error, sizeof(error)));
    send(service, python, project, &missing, filename, source, 14, strlen(source));
    CHECK(service->report.failed && strstr(service->report.status, "introuvable") && !service->report.count);
    completion_reset(service);
    send(service, python, project, &paths, filename, source, 15, strlen(source));
    CHECK(!service->report.failed && has(service, "ChangedField"));
    service->ready = false;
    CHECK(completion_submit(service, python, project, &paths, filename, source, 16, strlen(source), error,
                            sizeof(error)));
    service->sent_at = runner_clock() - COMPLETION_TIMEOUT - 1;
    completion_poll(service, runner_clock());
    CHECK(service->ready && service->report.failed && strstr(service->report.status, "15 s"));
    char last_request[BUF_PATH], last_adapter[BUF_PATH];
    strcpy(last_request, service->request_path);
    strcpy(last_adapter, service->process.temporary);
    completion_close(service);
    CHECK(!service->process.running && !service->request && !*service->request_path);
    FILE *f = fopen(last_request, "rb");
    CHECK(!f);
    f = fopen(last_adapter, "rb");
    CHECK(!f);
    f = fopen(request, "rb");
    CHECK(!f);
    f = fopen(adapter, "rb");
    CHECK(!f);
    f = fopen(sentinel, "rb");
    CHECK(!f);
    f = fopen(filename, "rb");
    CHECK(f);
    char original[128] = "";
    CHECK(fread(original, 1, sizeof(original) - 1, f) > 0 && !fclose(f));
    CHECK(!strcmp(original, "# Original on disk, not edited by completion\n"));
    free(service);
    CHECK(snprintf(path, sizeof(path), "%s/ast.py", sdk) < BUF_PATH && !remove(path));
    for (const char **p = (const char *[]){"__init__.py", "fields.py", "models.py", "api.py", NULL}; *p; p++)
        CHECK(snprintf(path, sizeof(path), "%s/%s", package, *p) < BUF_PATH && !remove(path));
    CHECK(!remove(filename));
#ifdef _WIN32
    CHECK(!_rmdir(package) && !_rmdir(sdk) && !_rmdir(project) && !_rmdir(base));
#else
    CHECK(!rmdir(package) && !rmdir(sdk) && !rmdir(project) && !rmdir(base));
#endif
    printf("OK %d completion checks: strict protocol, real isolated Python worker, Odoo aliases/core "
           "methods, UTF-8 byte ranges, cache/refresh/root removal/errors/timeouts, private-file cleanup, no "
           "execution/writes to documents; no GUI\n",
           checks);
    return 0;
}
