#define _POSIX_C_SOURCE 200809L
#include "lint_rules.h"
#include "buffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#else
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
static int nibble(char c) {
    return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
}
static int parse_lines(void) {
    char line[20000], text[10000];
    while (fgets(line, sizeof(line), stdin)) {
        size_t n = strcspn(line, "\r\n");
        bool ok = n % 2 == 0 && n / 2 < sizeof(text);
        for (size_t i = 0; i < n && ok; i += 2) {
            int a = nibble(line[i]), b = nibble(line[i + 1]);
            if (a < 0 || b < 0 || !((a << 4) | b))
                ok = false;
            else
                text[i / 2] = (char)((a << 4) | b);
        }
        if (ok)
            text[n / 2] = 0;
        LintRules rules;
        ok = ok && lint_rules_parse(&rules, text, NULL, 0);
        if (ok)
            printf("{\"ok\":true,\"value\":\"%s\"}\n", rules.text);
        else
            puts("{\"ok\":false}");
    }
    return 0;
}
static void quick_rules_model(void) {
    int first = checks;
    char code[6], decoded[6], error[512];
    const char *categories = "EWCRIF";
    for (int category = 0; category < 6; category++)
        for (int i = 0; i < 10000; i++) {
            snprintf(code, sizeof(code), "%c%04d", categories[category], i);
            int action = lint_rule_action(code, "example-message");
            if (!strcmp(code, "E0001")) {
                CHECK(action < 0);
                continue;
            }
            CHECK(action == category * 10000 + i && lint_rule_decode(action, decoded) &&
                  !strcmp(code, decoded));
        }
    CHECK(lint_rule_action("W0611", "syntax-error") < 0 && lint_rule_action("w0611", "unused-import") < 0);
    CHECK(lint_rule_action("--bad", "x") < 0 && lint_rule_action(NULL, "x") < 0 &&
          lint_rule_action("W0611", NULL) < 0);
    CHECK(!lint_rule_decode(-1, decoded) && !lint_rule_decode(60000, decoded));
    LintRules current = {0}, out = {0};
    CHECK(lint_rules_add(&out, &current, "W0611", "unused-import", error, sizeof(error)) &&
          !strcmp(out.text, "W0611"));
    current = out;
    CHECK(lint_rules_add(&out, &current, "W0611", "unused-import", error, sizeof(error)) &&
          !strcmp(out.text, "W0611"));
    CHECK(lint_rules_parse(&current, "unused-import", error, sizeof(error)));
    CHECK(lint_rules_add(&out, &current, "W0611", "unused-import", error, sizeof(error)) &&
          !strcmp(out.text, "unused-import"));
    CHECK(lint_rules_parse(&current, "C0114", error, sizeof(error)));
    CHECK(lint_rules_add(&out, &current, "W0611", "unused-import", error, sizeof(error)) &&
          !strcmp(out.text, "C0114,W0611"));
    LintRules before = out;
    CHECK(!lint_rules_add(&out, &current, "E0001", "syntax-error", error, sizeof(error)) &&
          !memcmp(&out, &before, sizeof(out)));
    char many[512] = "";
    for (int i = 0; i < 64; i++) {
        char token[16];
        snprintf(token, sizeof(token), "%sW%04d", i ? "," : "", i);
        strcat(many, token);
    }
    CHECK(lint_rules_parse(&current, many, error, sizeof(error)));
    CHECK(!lint_rules_add(&out, &current, "C0114", "missing-module-docstring", error, sizeof(error)) &&
          !memcmp(&out, &before, sizeof(out)));
    char large[4096] = "";
    for (int i = 0; i < 43; i++) {
        char token[100];
        snprintf(token, sizeof(token), "a%04d", i);
        memset(token + 5, 'b', 89);
        token[94] = 0;
        if (i)
            strcat(large, ",");
        strcat(large, token);
    }
    strcat(large, ",last-rule");
    CHECK(strlen(large) == 4094 && lint_rules_parse(&current, large, error, sizeof(error)));
    CHECK(!lint_rules_add(&out, &current, "W0611", "unused-import", error, sizeof(error)) &&
          !memcmp(&out, &before, sizeof(out)));
    CHECK(lint_rules_scope("a", "bc", "") != lint_rules_scope("ab", "c", "") &&
          lint_rules_scope("a", "bc", "") == lint_rules_scope("a", "bc", ""));
    printf("OK %d quick-rule identity/append checks: all 60000 concrete IDs, syntax excluded, code/name "
           "duplicate protection, atomic limits, scope boundaries\n",
           checks - first);
}
int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "--parse-lines"))
        return parse_lines();
    quick_rules_model();
    int base = checks;
    LintRules r = {0};
    char error[512];
    CHECK(lint_rules_valid(&r));
    CHECK(lint_rules_parse(&r, " w0611; C0114\nUNUSED-IMPORT, w0611\tunused-import ", error, sizeof(error)));
    CHECK(r.count == 3 && !strcmp(r.text, "W0611,C0114,unused-import") && lint_rules_valid(&r));
    LintRules before = r;
    const char *bad[] = {"--init-hook=bad", "a_b",     "a.b", "a--b", "a-", "-x", "all", "ALL", "W", "é",
                         "a\vbb",           "x('bad')"};
    for (size_t i = 0; i < sizeof(bad) / sizeof(*bad); i++) {
        CHECK(!lint_rules_parse(&r, bad[i], error, sizeof(error)) && *error);
        CHECK(!memcmp(&r, &before, sizeof(r)));
    }
    CHECK(!lint_rules_parse(&r, NULL, error, sizeof(error)) &&
          !lint_rules_parse(NULL, "", error, sizeof(error)));
    char value[8192];
    memset(value, 'a', 95);
    value[95] = 0;
    CHECK(lint_rules_parse(&r, value, error, sizeof(error)));
    value[95] = 'a';
    value[96] = 0;
    CHECK(!lint_rules_parse(&r, value, error, sizeof(error)));
    memset(value, ' ', 4090);
    strcpy(value + 4090, "W0611");
    CHECK(lint_rules_parse(&r, value, error, sizeof(error)) && !strcmp(r.text, "W0611"));
    memset(value, ' ', 4091);
    strcpy(value + 4091, "W0611");
    CHECK(!lint_rules_parse(&r, value, error, sizeof(error)));
    value[0] = 0;
    for (int i = 0; i < 64; i++) {
        char token[16];
        snprintf(token, sizeof(token), "%sW%04d", i ? "," : "", i);
        strcat(value, token);
    }
    CHECK(lint_rules_parse(&r, value, error, sizeof(error)) && r.count == 64);
    before = r;
    strcat(value, ",W0064");
    CHECK(!lint_rules_parse(&r, value, error, sizeof(error)) && !memcmp(&r, &before, sizeof(r)));
    value[0] = 0;
    for (int i = 0; i < 100; i++)
        strcat(value, "W0611,");
    CHECK(lint_rules_parse(&r, value, error, sizeof(error)) && r.count == 1);
    CHECK(lint_rules_parse(&r, ",;\t\r\n", error, sizeof(error)) && !r.count && !*r.text);
    CHECK(lint_rules_parse(&r, "E0602,F0001,W0611,C0114,R0903,I0010,too-few-public-methods", error,
                           sizeof(error)) &&
          r.count == 7);
    char dir[BUF_PATH], settings[BUF_PATH], file[BUF_PATH];
#ifdef _WIN32
    char temp[MAX_PATH];
    CHECK(GetTempPathA(MAX_PATH, temp) && GetTempFileNameA(temp, "lrs", 0, dir));
    CHECK(!remove(dir) && !_mkdir(dir));
#else
    strcpy(dir, "/tmp/python-studio-lint-rules-XXXXXX");
    CHECK(mkdtemp(dir));
#endif
    CHECK(snprintf(settings, sizeof(settings), "%s/settings.cfg", dir) < BUF_PATH);
    CHECK(lint_rules_file(file, sizeof(file), settings));
    before = r;
    CHECK(!lint_rules_load(&r, file, error, sizeof(error)) && !*error && !memcmp(&r, &before, sizeof(r)));
    CHECK(lint_rules_save(&r, file, error, sizeof(error)));
    memset(&r, 0, sizeof(r));
    CHECK(lint_rules_load(&r, file, error, sizeof(error)) && !strcmp(r.text, before.text));
    FILE *f = fopen(file, "wb");
    CHECK(f);
    CHECK(fputs("PYTHON-STUDIO-PYLINT\t99\nDISABLE\tW0611\n", f) >= 0 && !fclose(f));
    before = r;
    CHECK(!lint_rules_load(&r, file, error, sizeof(error)) && *error && !memcmp(&r, &before, sizeof(r)));
    CHECK(lint_rules_parse(&r, "", error, sizeof(error)) && lint_rules_save(&r, file, error, sizeof(error)));
    CHECK(lint_rules_load(&r, file, error, sizeof(error)) && !r.count && !*r.text);
    char missing[BUF_PATH];
    CHECK(snprintf(missing, sizeof(missing), "%s/missing/rules.cfg", dir) < BUF_PATH);
    CHECK(!lint_rules_save(&r, missing, error, sizeof(error)) && *error);
    r.count = 1;
    CHECK(!lint_rules_valid(&r) && !lint_rules_save(&r, file, error, sizeof(error)));
    r.count = 0;
    CHECK(!remove(file));
#ifdef _WIN32
    CHECK(!_rmdir(dir));
#else
    CHECK(!rmdir(dir));
#endif
    printf("OK %d lint-rule grammar/profile checks: limits, injection rejection, canonical IDs/names, atomic "
           "rejection, persistence, empty re-enable; no project files/window\n",
           checks - base);
    return 0;
}
