#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#include "linter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
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
static void pause_test(void) {
#ifdef _WIN32
    Sleep(5);
#else
    struct timespec t = {0, 5000000};
    nanosleep(&t, NULL);
#endif
}
static void wait_analysis(Linter *engine) {
    double deadline = runner_clock() + 20;
    while (engine->process.running && runner_clock() < deadline) {
        lint_poll(engine, runner_clock());
        pause_test();
    }
    CHECK(!engine->process.running);
}
static void put(const char *path, const char *content) {
    FILE *f = fopen(path, "wb");
    CHECK(f);
    CHECK(fwrite(content, 1, strlen(content), f) == strlen(content));
    CHECK(!fclose(f));
}
static void join(char *out, const char *root, const char *leaf) {
    CHECK(snprintf(out, BUF_PATH, "%s/%s", root, leaf) < BUF_PATH);
}
static bool exists(const char *path) {
    FILE *f = fopen(path, "rb");
    if (f)
        fclose(f);
    return f != NULL;
}
static void make_directory(const char *path) {
#ifdef _WIN32
    CHECK(!_mkdir(path));
#else
    CHECK(!mkdir(path, 0700));
#endif
}
static void remove_directory(const char *path) {
#ifdef _WIN32
    CHECK(!_rmdir(path));
#else
    CHECK(!rmdir(path));
#endif
}
static bool has_code(const LintReport *report, const char *code) {
    for (int i = 0; i < report->count; i++)
        if (!strcmp(report->items[i].code, code))
            return true;
    return false;
}
static void protocol_and_positions(void) {
    LintReport *report = calloc(1, sizeof(*report));
    CHECK(report);
    const char *good =
        "PRISME-LINT\t1\tpylint\t342e31\t\n"
        "D\tW\t2\t1\t2\t3\tW0611\t756e757365642d696d706f7274\t556e75736564\n"
        "D\tE\t1\t4\t1\t9\tE0602\t756e646566696e65642d7661726961626c65\t556e646566696e6564206e616d65\n"
        "D\tI\t1\t0\t1\t0\tC0114\t646f63737472696e67\t4d697373696e67\nEND\t3\t8\n";
    CHECK(lint_parse(report, good));
    CHECK(report->count == 3 && report->total == 8);
    CHECK(report->errors == 1 && report->warnings == 1 && report->infos == 1);
    CHECK(!strcmp(report->version, "4.1") && report->backend == LINT_BACKEND_PYLINT);
    CHECK(report->items[0].severity == LINT_INFO && report->items[1].severity == LINT_ERROR &&
          report->items[2].row == 1);
    CHECK(!strcmp(report->items[1].symbol, "undefined-variable"));
    const char *bad[] = {"",
                         "PRISME-LINT\t2\tpylint\t\t\nEND\t0\t0\n",
                         "PRISME-LINT\t1\tunknown\t\t\nEND\t0\t0\n",
                         "PRISME-LINT\t1\tpylint\t\t\n",
                         "PRISME-LINT\t1\tpylint\t\t\nEND\t1\t1\n",
                         "PRISME-LINT\t1\tpylint\t\t\nEND\t0\t0\nextra\n",
                         "PRISME-LINT\t1\tpylint\t00\t\nEND\t0\t0\n",
                         "PRISME-LINT\t1\tpylint\tc0af\t\nEND\t0\t0\n",
                         "PRISME-LINT\t1\tpylint\t0\t\nEND\t0\t0\n",
                         "PRISME-LINT\t1\tpylint\tzz\t\nEND\t0\t0\n",
                         "PRISME-LINT\t1\tpylint\t\t\nD\tE\t-1\t0\t1\t0\tE0602\t\t\nEND\t1\t1\n",
                         "PRISME-LINT\t1\tpylint\t\t\nD\tE\t1\t0\t1\t0\tbad\t\t\nEND\t1\t1\n"};
    for (size_t i = 0; i < sizeof(bad) / sizeof(*bad); i++) {
        CHECK(!lint_parse(report, bad[i]));
        CHECK(report->count == 3 && !strcmp(report->items[1].code, "E0602"));
    }
    char *many = calloc(1, 100000);
    CHECK(many);
    strcpy(many, "PRISME-LINT\t1\tpylint\t\t\n");
    for (int i = 0; i < 257; i++)
        strcat(many, "D\tE\t1\t0\t1\t0\tE0602\t\t78\n");
    strcat(many, "END\t257\t257\n");
    CHECK(!lint_parse(report, many));
    free(many);
    Buffer b;
    CHECK(buf_init(&b, "\tprénom = inconnue\nsecond\n", "u.py"));
    Diagnostic d = {0};
    d.column = (size_t)(strstr(b.text, "inconnue") - b.text);
    d.end_column = d.column + 8;
    size_t start, end;
    lint_range(&b, &d, &start, &end);
    CHECK(!strncmp(b.text + start, "inconnue", end - start) && end - start == 8);
    d.column = (size_t)(strstr(b.text, "é") - b.text) + 1;
    d.end_column = d.column;
    lint_range(&b, &d, &start, &end);
    CHECK(!strncmp(b.text + start, "é", 2));
    CHECK(utf8_valid(b.text + start, end - start));
    d.row = SIZE_MAX;
    d.column = SIZE_MAX;
    d.end_row = SIZE_MAX;
    d.end_column = SIZE_MAX;
    lint_range(&b, &d, &start, &end);
    CHECK(start <= end && end <= b.len);
    memset(report, 0, sizeof(*report));
    report->count = 1;
    report->items[0].column = 1;
    report->items[0].end_column = 8;
    lint_resolve(report, &b);
    CHECK(report->resolved && report->items[0].visual_column == 4 &&
          report->items[0].end_visual_column == 10);
    buf_free(&b);
    free(report);
    LintDocument *doc = calloc(1, sizeof(*doc));
    CHECK(doc);
    lint_observe(doc, 7, "/tmp/a.py", "/tmp", 100);
    CHECK(!doc->valid && doc->waiting && !lint_ready(doc, 100.9));
    CHECK(lint_ready(doc, 101));
    lint_observe(doc, 7, "/tmp/a.py", "/tmp", 110);
    CHECK(doc->changed_at == 100);
    doc->valid = true;
    lint_observe(doc, 8, "/tmp/a.py", "/tmp", 111);
    CHECK(!doc->valid && doc->waiting && doc->changed_at == 111);
    lint_observe(doc, 8, "/tmp/new.py", "/tmp", 112);
    CHECK(doc->changed_at == 112);
    lint_observe(doc, 8, "/tmp/new.py", "/another", 113);
    CHECK(doc->changed_at == 113);
    free(doc);
}
static void fallback(const char *python, const char *root) {
    Linter e;
    lint_init(&e);
    LintDocument *doc = calloc(1, sizeof(*doc));
    CHECK(doc);
    char file[BUF_PATH], error[512];
    join(file, root, "syntax.py");
    lint_observe(doc, 1, file, root, runner_clock());
    CHECK(lint_start(&e, doc, python, "def f(:\n    pass\n", error, sizeof(error)));
    wait_analysis(&e);
    CHECK(doc->valid && doc->report.backend == LINT_BACKEND_SYNTAX && doc->report.errors == 1);
    CHECK(!strcmp(doc->report.items[0].code, "E0001") && doc->report.items[0].row == 0 &&
          doc->report.items[0].column == 6);
    lint_observe(doc, 2, file, root, runner_clock());
    CHECK(lint_start(&e, doc, python, "return 4\n", error, sizeof(error)));
    wait_analysis(&e);
    CHECK(doc->report.errors == 1 && strstr(doc->report.items[0].message, "outside function"));
    lint_observe(doc, 3, file, root, runner_clock());
    CHECK(lint_start(&e, doc, python, "print('ok')\n", error, sizeof(error)));
    wait_analysis(&e);
    if (doc->report.backend == LINT_BACKEND_SYNTAX) {
        CHECK(!doc->report.errors && strstr(doc->report.status, "PyLint non installé"));
        puts("OK real missing-Pylint fallback: syntax remains available, incomplete analysis explicitly "
             "reported");
    } else {
        CHECK(doc->report.backend == LINT_BACKEND_PYLINT);
        puts("Pylint present in fallback interpreter; missing-package case not applicable");
    }
    lint_close(&e);
    free(doc);
}
static void integration(const char *python, const char *root, bool require) {
    Linter e;
    lint_init(&e);
    LintDocument *doc = calloc(1, sizeof(*doc));
    CHECK(doc);
    char file[BUF_PATH], sentinel[BUF_PATH], shadow[BUF_PATH], config[BUF_PATH], error[512];
    join(file, root, "unsaved.py");
    join(sentinel, root, "EXECUTED");
    put(file, "print('disk remains unchanged')\n");
    join(shadow, root, "linecache.py");
    put(shadow, "raise RuntimeError('project linecache executed')\n");
    join(shadow, root, "pylint.py");
    put(shadow, "raise RuntimeError('project pylint executed')\n");
    join(config, root, ".pylintrc");
    put(config, "[MAIN]\ninit-hook=raise RuntimeError('project hook executed')\nload-plugins=evil_plugin\n");
    lint_observe(doc, 1, file, root, runner_clock());
    const char *source = "\"\"\"Native editor analysis.\"\"\"\nimport os\nprénom = 1; "
                         "print(inconnu)\nopen('EXECUTED', 'w').write('never execute')\n";
    CHECK(lint_start(&e, doc, python, source, error, sizeof(error)));
    wait_analysis(&e);
    CHECK(doc->valid && doc->analyzed_version == 1 && !exists(sentinel));
    if (doc->report.backend != LINT_BACKEND_PYLINT) {
        if (require) {
            fprintf(stderr, "Expected actual Pylint: %s\n", doc->report.status);
            CHECK(false);
        }
        CHECK(doc->report.backend == LINT_BACKEND_SYNTAX);
        puts("SKIP full Pylint unavailable; configure PRISME_TEST_PYTHON to an environment with Pylint");
    } else {
        bool undefined = false, unused = false, style = false;
        for (int i = 0; i < doc->report.count; i++) {
            Diagnostic *d = &doc->report.items[i];
            if (!strcmp(d->code, "E0602")) {
                undefined = true;
                /* PyLint 2.7 reports a point; newer versions report the whole token.
                   Both must resolve to the same UTF-8 underline in the editor. */
                CHECK(d->row == 2 && d->column == 19 && (d->end_column == 19 || d->end_column == 26) &&
                      d->severity == LINT_ERROR);
                CHECK(!strcmp(d->symbol, "undefined-variable") && strstr(d->message, "inconnu"));
                Buffer current;
                size_t start, end;
                CHECK(buf_init(&current, source, file));
                lint_range(&current, d, &start, &end);
                CHECK(end - start == 7 && !strncmp(current.text + start, "inconnu", 7));
                buf_free(&current);
            }
            if (!strcmp(d->code, "W0611"))
                unused = true;
            if (d->severity == LINT_INFO)
                style = true;
        }
        CHECK(undefined && unused && style);
        CHECK(doc->report.errors > 0 && doc->report.warnings > 0 && doc->report.infos > 0);
        printf("OK actual Pylint %s: errors, warnings, conventions, Unicode byte columns, unsaved contents, "
               "isolated imports/config\n",
               doc->report.version);
    }
    FILE *f = fopen(file, "rb");
    CHECK(f);
    char content[128] = {0};
    CHECK(fread(content, 1, 127, f) > 0);
    fclose(f);
    CHECK(!strcmp(content, "print('disk remains unchanged')\n"));
    if (doc->report.backend == LINT_BACKEND_PYLINT) {
        char *large = calloc(1, 30000);
        CHECK(large);
        strcpy(large, "\"\"\"Lots of style messages.\"\"\"\n");
        size_t used = strlen(large);
        for (int i = 0; i < 400; i++)
            used += (size_t)snprintf(large + used, 30000 - used, "badName%d = 0\n", i);
        snprintf(large + used, 30000 - used, "print(missing_last)\n");
        lint_observe(doc, 10, file, root, runner_clock());
        CHECK(lint_start(&e, doc, python, large, error, sizeof(error)));
        wait_analysis(&e);
        CHECK(doc->report.backend == LINT_BACKEND_PYLINT && doc->report.count == LINT_MAX_ITEMS &&
              doc->report.total > LINT_MAX_ITEMS);
        bool kept = false;
        for (int i = 0; i < doc->report.count; i++)
            if (!strcmp(doc->report.items[i].code, "E0602") &&
                strstr(doc->report.items[i].message, "missing_last"))
                kept = true;
        CHECK(kept && doc->report.errors > 0);
        free(large);
    }
    if (doc->report.backend == LINT_BACKEND_PYLINT) {
        char helper[BUF_PATH];
        join(helper, root, "helper_local.py");
        put(helper, "open('EXECUTED', 'w').write('imports must not run')\nVALUE = 42\n");
        lint_observe(doc, 11, file, root, runner_clock());
        CHECK(lint_start(&e, doc, python, "from helper_local import VALUE\nprint(VALUE)\n", error,
                         sizeof(error)));
        wait_analysis(&e);
        CHECK(doc->report.backend == LINT_BACKEND_PYLINT && !exists(sentinel));
        for (int i = 0; i < doc->report.count; i++)
            CHECK(strcmp(doc->report.items[i].code, "E0401"));
        CHECK(!remove(helper));
    }
    if (doc->report.backend == LINT_BACKEND_PYLINT) {
        char sdk[BUF_PATH], package[BUF_PATH], init[BUF_PATH], poison[BUF_PATH];
        CHECK(snprintf(sdk, sizeof(sdk), "%s-SDK été, ;'[]", root) < BUF_PATH);
        make_directory(sdk);
        join(package, sdk, "prisme_external_framework");
        make_directory(package);
        join(init, package, "__init__.py");
        put(init, "\"\"\"External framework.\"\"\"\nopen('EXECUTED', 'w').write('never import "
                  "dynamically')\nclass Node:\n    \"\"\"A framework node.\"\"\"\n");
        join(poison, sdk, "pylint.py");
        put(poison, "raise RuntimeError('SDK pylint executed')\n");
        join(poison, sdk, "linecache.py");
        put(poison, "raise RuntimeError('SDK linecache executed')\n");
        const char *framework =
            "from prisme_external_framework import Node\nprint(Node)\nprint(real_problem)\n";
        lint_observe(doc, 20, file, root, runner_clock());
        CHECK(lint_start(&e, doc, python, framework, error, sizeof(error)));
        wait_analysis(&e);
        CHECK(doc->report.backend == LINT_BACKEND_PYLINT && has_code(&doc->report, "E0401") &&
              has_code(&doc->report, "E0602"));
        CHECK(import_paths_add(&e.imports, sdk, error, sizeof(error)));
        lint_observe(doc, 21, file, root, runner_clock());
        CHECK(lint_start(&e, doc, python, framework, error, sizeof(error)));
        wait_analysis(&e);
        CHECK(doc->report.backend == LINT_BACKEND_PYLINT && !has_code(&doc->report, "E0401") &&
              !has_code(&doc->report, "E0611"));
        CHECK(has_code(&doc->report, "E0602") && !exists(sentinel));
        CHECK(import_paths_remove(&e.imports, 0));
        lint_observe(doc, 22, file, root, runner_clock());
        CHECK(lint_start(&e, doc, python, framework, error, sizeof(error)));
        wait_analysis(&e);
        CHECK(doc->report.backend == LINT_BACKEND_PYLINT && has_code(&doc->report, "E0401") &&
              has_code(&doc->report, "E0602"));
        CHECK(import_paths_add(&e.imports, sdk, error, sizeof(error)));
        CHECK(!remove(init));
        remove_directory(package);
        join(poison, sdk, "pylint.py");
        CHECK(!remove(poison));
        join(poison, sdk, "linecache.py");
        CHECK(!remove(poison));
        remove_directory(sdk);
        lint_observe(doc, 23, file, root, runner_clock());
        CHECK(lint_start(&e, doc, python, framework, error, sizeof(error)));
        wait_analysis(&e);
        CHECK(doc->report.backend == LINT_BACKEND_ERROR &&
              strstr(doc->report.status, "Dossier d’import introuvable"));
        CHECK(import_paths_remove(&e.imports, 0));
        puts("OK real external framework: E0401 before, resolved by configured UTF-8/space/quote path, "
             "genuine E0602 retained, no module execution, E0401 returns after removal, missing directory "
             "explicit");
    }
    /* Late results must never replace a newer document version. */
    lint_observe(doc, 2, file, root, runner_clock());
    CHECK(lint_start(&e, doc, python, source, error, sizeof(error)));
    lint_observe(doc, 3, file, root, runner_clock());
    lint_poll(&e, runner_clock());
    wait_analysis(&e);
    CHECK(!doc->valid && doc->waiting && e.owner == NULL);
    CHECK(lint_start(&e, doc, python, "def f(:\n", error, sizeof(error)));
    wait_analysis(&e);
    CHECK(doc->valid && doc->analyzed_version == 3 && doc->report.errors == 1);
    /* Timeout is a reported failure, not a misleading 'no problems'. */
    lint_observe(doc, 4, file, root, runner_clock());
    CHECK(lint_start(&e, doc, python, source, error, sizeof(error)));
    lint_poll(&e, e.process.started + LINT_TIMEOUT + 1);
    wait_analysis(&e);
    CHECK(doc->valid && doc->report.backend == LINT_BACKEND_ERROR && strstr(doc->report.status, "15 s"));
    /* Closing/reusing a tab is safe while its subprocess is still alive. */
    lint_observe(doc, 5, file, root, runner_clock());
    CHECK(lint_start(&e, doc, python, source, error, sizeof(error)));
    lint_forget(&e, doc);
    free(doc);
    doc = NULL;
    wait_analysis(&e);
    CHECK(e.owner == NULL);
    doc = calloc(1, sizeof(*doc));
    CHECK(doc);
    lint_observe(doc, 6, file, root, runner_clock());
    bool started = lint_start(&e, doc, "/no-such-prisme-python", source, error, sizeof(error));
    if (started)
        wait_analysis(&e);
    CHECK(doc->valid && doc->report.backend == LINT_BACKEND_ERROR);
    lint_close(&e);
    free(doc);
    CHECK(!remove(file));
    CHECK(!remove(config));
    join(shadow, root, "linecache.py");
    CHECK(!remove(shadow));
    join(shadow, root, "pylint.py");
    CHECK(!remove(shadow));
}
static void disabled_integration(const char *python, const char *root) {
    int start = checks;
    Linter e;
    lint_init(&e);
    LintDocument *doc = calloc(1, sizeof(*doc));
    CHECK(doc);
    char file[BUF_PATH], error[512];
    join(file, root, "rules.py");
    const char *source = "import os\nprint(inconnu)\n";
    const char *rules[] = {"", "w0611; C0114", "unused-import,missing-module-docstring", ""};
    unsigned long last_revision = 0;
    for (int i = 0; i < 4; i++) {
        CHECK(lint_set_disabled(&e, rules[i], error, sizeof(error)));
        lint_observe(doc, (unsigned long)i + 1, file, root, runner_clock());
        CHECK(lint_start(&e, doc, python, source, error, sizeof(error)));
        wait_analysis(&e);
        CHECK(doc->valid && doc->report.backend == LINT_BACKEND_PYLINT && has_code(&doc->report, "E0602"));
        CHECK(doc->report.revision > last_revision);
        last_revision = doc->report.revision;
        CHECK(has_code(&doc->report, "W0611") == (!i || i == 3));
        CHECK(has_code(&doc->report, "C0114") == (!i || i == 3));
        if (i == 1 || i == 2)
            CHECK(doc->report.total == 1);
    }
    CHECK(lint_set_disabled(&e, "W9999", error, sizeof(error)));
    lint_observe(doc, 5, file, root, runner_clock());
    CHECK(lint_start(&e, doc, python, source, error, sizeof(error)));
    wait_analysis(&e);
    CHECK(doc->report.backend == LINT_BACKEND_ERROR && strstr(doc->report.status, "W9999"));
    CHECK(lint_set_disabled(&e, "E0001,syntax-error", error, sizeof(error)));
    lint_observe(doc, 6, file, root, runner_clock());
    CHECK(lint_start(&e, doc, python, "def f(:\n", error, sizeof(error)));
    wait_analysis(&e);
    CHECK(doc->report.backend == LINT_BACKEND_SYNTAX && has_code(&doc->report, "E0001"));
    LintRules before = e.disabled;
    CHECK(!lint_set_disabled(&e, "--init-hook=bad", error, sizeof(error)) &&
          !memcmp(&before, &e.disabled, sizeof(before)));
    CHECK(lint_set_disabled(&e, "", error, sizeof(error)));
    lint_observe(doc, 7, file, root, runner_clock());
    CHECK(lint_start(&e, doc, python, source, error, sizeof(error)) && e.process.running);
    CHECK(lint_set_disabled(&e, "W0611,C0114", error, sizeof(error)) && !e.owner && e.discard && !doc->valid);
    wait_analysis(&e);
    CHECK(!doc->valid);
    CHECK(lint_start(&e, doc, python, source, error, sizeof(error)));
    wait_analysis(&e);
    CHECK(doc->valid && doc->report.backend == LINT_BACKEND_PYLINT && doc->report.total == 1 &&
          has_code(&doc->report, "E0602"));
    char *large = calloc(1, 20000);
    CHECK(large);
    for (int i = 0; i < 300; i++) {
        char line[64];
        snprintf(line, sizeof(line), "import os as unused_%d\n", i);
        strcat(large, line);
    }
    strcat(large, "print(inconnu)\n");
    CHECK(lint_set_disabled(&e, "", error, sizeof(error)));
    lint_observe(doc, 8, file, root, runner_clock());
    CHECK(lint_start(&e, doc, python, large, error, sizeof(error)));
    wait_analysis(&e);
    CHECK(doc->report.backend == LINT_BACKEND_PYLINT && doc->report.total > 256);
    CHECK(lint_set_disabled(&e, "W0611,W0404,C0114", error, sizeof(error)));
    lint_observe(doc, 9, file, root, runner_clock());
    CHECK(lint_start(&e, doc, python, large, error, sizeof(error)));
    wait_analysis(&e);
    CHECK(doc->report.backend == LINT_BACKEND_PYLINT && doc->report.total == 1 &&
          has_code(&doc->report, "E0602"));
    free(large);
    lint_close(&e);
    free(doc);
    printf("OK %d REAL PyLint disabled-rule checks: IDs/names, remove/re-enable, syntax retained, unknown "
           "rule error, old process discarded, >256 warnings disabled BEFORE protocol truncation\n",
           checks - start);
}
static void odoo_integration(const char *python, const char *root) {
    int first = checks;
    Linter e;
    lint_init(&e);
    LintDocument *doc = calloc(1, sizeof(*doc));
    CHECK(doc);
    OdooConfig config;
    odoo_config_init(&config);
    char file[BUF_PATH], error[512], poison[BUF_PATH], sentinel[BUF_PATH];
    join(file, root, "odoo_model.py");
    join(sentinel, root, "NO_PLUGIN_SHADOW");
    join(poison, root, "pylint_odoo.py");
    put(poison, "raise RuntimeError('project plugin must not import')\n");
    CHECK(lint_set_odoo(&e, &config, error, sizeof(error)));
    const char *source = "from odoo import models\nclass Example(models.Model):\n    _name='x.example'\n    "
                         "def check(self):\n        self._cr.execute('SELECT 1')\n        print(inconnu)\n";
    lint_observe(doc, 1, file, root, runner_clock());
    CHECK(lint_start(&e, doc, python, source, error, sizeof(error)));
    wait_analysis(&e);
    CHECK(doc->report.backend == LINT_BACKEND_PYLINT && has_code(&doc->report, "W8165") &&
          has_code(&doc->report, "E0602") && strstr(doc->report.status, "Odoo19"));
    CHECK(lint_rules_parse(&config.ignored, "W8165,W9001", error, sizeof(error)) &&
          lint_set_odoo(&e, &config, error, sizeof(error)));
    lint_observe(doc, 2, file, root, runner_clock());
    CHECK(lint_start(&e, doc, python, source, error, sizeof(error)));
    wait_analysis(&e);
    CHECK(doc->report.backend == LINT_BACKEND_PYLINT && !has_code(&doc->report, "W8165") &&
          has_code(&doc->report, "E0602"));
    config.enabled = false;
    CHECK(lint_set_odoo(&e, &config, error, sizeof(error)));
    lint_observe(doc, 3, file, root, runner_clock());
    CHECK(lint_start(&e, doc, python, source, error, sizeof(error)));
    wait_analysis(&e);
    CHECK(doc->report.backend == LINT_BACKEND_PYLINT && !strstr(doc->report.status, "Odoo19"));
    config.enabled = true;
    CHECK(lint_set_odoo(&e, &config, error, sizeof(error)));
    join(file, root, "view.xml");
    const char *xml = "<odoo><record id='a'/><record id='a' model='x'><field name='arch' type='xml'><tree "
                      "attrs='{}'/></field></record></odoo>";
    lint_observe(doc, 4, file, root, runner_clock());
    CHECK(lint_start(&e, doc, python, xml, error, sizeof(error)));
    wait_analysis(&e);
    CHECK(doc->report.backend == LINT_BACKEND_ODOO_XML && has_code(&doc->report, "W9002") &&
          has_code(&doc->report, "W9005") && !has_code(&doc->report, "W9001"));
    const char *empty =
        "<odoo><record id='a' model='ir.ui.view'><field name='arch' type='xml'></field></record></odoo>";
    lint_observe(doc, 5, file, root, runner_clock());
    CHECK(lint_start(&e, doc, python, empty, error, sizeof(error)));
    wait_analysis(&e);
    CHECK(doc->report.backend == LINT_BACKEND_ODOO_XML && !doc->report.count);
    lint_observe(doc, 6, file, root, runner_clock());
    CHECK(lint_start(&e, doc, python,
                     "<!DOCTYPE odoo [<!ENTITY a SYSTEM 'file:///not-opened'>]><odoo>&a;</odoo>", error,
                     sizeof(error)));
    wait_analysis(&e);
    CHECK(doc->report.errors == 1 && has_code(&doc->report, "E0001"));
    config.enabled = false;
    CHECK(lint_set_odoo(&e, &config, error, sizeof(error)));
    lint_observe(doc, 7, file, root, runner_clock());
    CHECK(lint_start(&e, doc, python, "<odoo><broken></odoo>", error, sizeof(error)));
    wait_analysis(&e);
    CHECK(doc->report.backend == LINT_BACKEND_XML && has_code(&doc->report, "E0001"));
    lint_observe(doc, 8, file, root, runner_clock());
    CHECK(
        lint_start(&e, doc, python, "<?xml version='1.0' encoding='UTF-16'?><odoo/>", error, sizeof(error)));
    wait_analysis(&e);
    CHECK(doc->report.backend == LINT_BACKEND_XML && !doc->report.errors);
    CHECK(!exists(sentinel));
    CHECK(!remove(poison));
    lint_close(&e);
    free(doc);
    printf(
        "OK %d REAL native Odoo19 assertions: trusted plugin W8165, retained E0602, scoped exclusions/off "
        "mode, Odoo XML/local IDs/empty arch/DTD rejection and generic XML syntax; no Odoo server/database\n",
        checks - first);
}
int main(int argc, char **argv) {
    protocol_and_positions();
    char directory[BUF_PATH];
#ifdef _WIN32
    char temp[MAX_PATH];
    CHECK(GetTempPathA(MAX_PATH, temp));
    CHECK(GetTempFileNameA(temp, "plt", 0, directory));
    CHECK(!remove(directory));
    CHECK(!_mkdir(directory));
#else
    strcpy(directory, "/tmp/prisme-linter-XXXXXX");
    CHECK(mkdtemp(directory));
#endif
    if (argc > 1 && !strcmp(argv[1], "--fallback-only"))
        fallback(argc > 2 ? argv[2] : "python3", directory);
    else if (argc > 1 && !strcmp(argv[1], "--odoo-only")) {
        CHECK(argc > 2);
        odoo_integration(argv[2], directory);
    } else if (argc > 1) {
        integration(argv[1], directory, argc > 2 && !strcmp(argv[2], "--require-pylint"));
        if (argc > 2 && !strcmp(argv[2], "--require-pylint"))
            disabled_integration(argv[1], directory);
    }
#ifdef _WIN32
    CHECK(!_rmdir(directory));
#else
    CHECK(!rmdir(directory));
#endif
    printf("OK %d unconditional diagnostic assertions; no display or preview\n", checks);
    return 0;
}
