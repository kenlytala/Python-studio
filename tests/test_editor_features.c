#define _POSIX_C_SOURCE 200809L
#include "syntax.h"
#include "browser_complete.h"
#include "odoo_config.h"
#include "buffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#else
#include <unistd.h>
#include <sys/stat.h>
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
    Sleep(1);
#else
    struct timespec t = {0, 1000000};
    nanosleep(&t, NULL);
#endif
}
static void mkdir_test(const char *path) {
#ifdef _WIN32
    CHECK(!_mkdir(path));
#else
    CHECK(!mkdir(path, 0700));
#endif
}
static void rmdir_test(const char *path) {
#ifdef _WIN32
    CHECK(!_rmdir(path));
#else
    CHECK(!rmdir(path));
#endif
}
static void put(const char *path, const char *text) {
    FILE *f = fopen(path, "wb");
    CHECK(f);
    CHECK(fwrite(text, 1, strlen(text), f) == strlen(text));
    CHECK(!fclose(f));
}
static void join(char *out, const char *base, const char *name) {
    CHECK(snprintf(out, FS_PATH, "%s/%s", base, name) < FS_PATH);
}
static BrowserLoadJob *load_browser(const char *text, const char *base, bool only) {
    char error[512] = {0};
    BrowserLoadJob *job =
        browser_job_start(text, strlen(text), base, base, only, NULL, NULL, error, sizeof(error));
    CHECK(job);
    unsigned limit = 5000;
    while (!atomic_load(&job->done) && limit--)
        pause_test();
    CHECK(atomic_load(&job->done) && job->success);
    return job;
}
static int unhex(char c) {
    return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
}
static int oracle(void) {
    char *line = malloc(262144), *source = malloc(131072);
    uint8_t *colors = malloc(131072);
    if (!line || !source || !colors)
        return 1;
    while (fgets(line, 262144, stdin)) {
        char *tab = strchr(line, '\t');
        if (!tab)
            return 2;
        *tab++ = 0;
        SyntaxLanguage language = !strcmp(line, "js") ? SYNTAX_JAVASCRIPT : SYNTAX_XML;
        size_t n = strcspn(tab, "\r\n");
        if (n % 2 || n / 2 >= 131072)
            return 2;
        for (size_t i = 0; i < n; i += 2) {
            int a = unhex(tab[i]), b = unhex(tab[i + 1]);
            if (a < 0 || b < 0)
                return 2;
            source[i / 2] = (char)((a << 4) | b);
        }
        syntax_colorize_language(source, n / 2, colors, language);
        for (size_t i = 0; i < n / 2; i++)
            printf("%x", colors[i]);
        putchar('\n');
    }
    free(line);
    free(source);
    free(colors);
    return 0;
}
int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "--colors-lines"))
        return oracle();
    CHECK(syntax_language("test.JS") == SYNTAX_JAVASCRIPT && syntax_language("test.XML") == SYNTAX_XML &&
          syntax_language("a.txt") == SYNTAX_PLAIN);
    const char *js = "export function example(x) { const value = `hi ${x+1}`; return /[a-z]+/gi.test(value) "
                     "? x/2 : true; // comment\n}";
    size_t n = strlen(js);
    uint8_t *colors = malloc(n);
    CHECK(colors);
    syntax_colorize_language(js, n, colors, SYNTAX_JAVASCRIPT);
    CHECK(colors[0] == SYN_KEYWORD && colors[strstr(js, "example") - js] == SYN_DEFINITION &&
          colors[strstr(js, "hi ") - js] == SYN_STRING && colors[strstr(js, "${") - js] == SYN_OPERATOR &&
          colors[strstr(js, "/[a-z") - js] == SYN_STRING && colors[strstr(js, "/2") - js] == SYN_OPERATOR &&
          colors[strstr(js, "// comment") - js] == SYN_COMMENT);
    free(colors);
    const char *xml = "<odoo><!-- comment --><record id=\"é&amp;\"><![CDATA[<raw>]]></record></odoo>";
    n = strlen(xml);
    colors = malloc(n);
    CHECK(colors);
    syntax_colorize_language(xml, n, colors, SYNTAX_XML);
    CHECK(colors[1] == SYN_DEFINITION && colors[strstr(xml, "id=") - xml] == SYN_BUILTIN &&
          colors[strstr(xml, "&amp;") - xml] == SYN_CONSTANT &&
          colors[strstr(xml, "<!--") - xml] == SYN_COMMENT &&
          colors[strstr(xml, "<![CDATA[") - xml] == SYN_STRING);
    free(colors);
    char dir[FS_PATH], path[FS_PATH], alpha[FS_PATH], file[FS_PATH], settings[FS_PATH], profile[FS_PATH],
        error[512];
#ifdef _WIN32
    char temp[MAX_PATH];
    CHECK(GetTempPathA(MAX_PATH, temp) && GetTempFileNameA(temp, "edf", 0, dir));
    CHECK(!remove(dir));
    mkdir_test(dir);
#else
    strcpy(dir, "/tmp/python-studio-features-XXXXXX");
    CHECK(mkdtemp(dir));
#endif
    join(alpha, dir, "alpha");
    mkdir_test(alpha);
    join(path, dir, "a dir");
    mkdir_test(path);
    join(file, dir, "apple.js");
    put(file, "const x=1");
    BrowserLoadJob *job = load_browser("a", dir, false);
    CHECK(job->count == 3 && job->items[0].directory && job->items[1].directory && !job->items[2].directory);
    CHECK(!strcmp(fs_basename(job->items[0].path), "a dir"));
    browser_job_free(job);
    job = load_browser("a", dir, true);
    CHECK(job->count == 2 && job->items[1].directory);
    browser_job_free(job);
    join(path, alpha, "inside.xml");
    put(path, "<odoo/>");
    job = load_browser(alpha, dir, false);
    CHECK(job->count == 1 && !strcmp(fs_basename(job->items[0].path), "inside.xml"));
    browser_job_free(job);
    CHECK(!browser_job_start("bad\n", 4, dir, dir, false, NULL, NULL, error, sizeof(error)));
    job = load_browser("", dir, false);
    CHECK(job->count == 3);
    browser_job_free(job);
    OdooConfig config, out;
    odoo_config_init(&config);
    CHECK(config.enabled && odoo_config_valid(&config));
    CHECK(odoo_diagnostic_code("W8165") && odoo_diagnostic_code("W9001") && !odoo_diagnostic_code("W0611"));
    CHECK(lint_rules_parse(&config.ignored, "w8165; W9001", error, sizeof(error)));
    join(settings, dir, "settings.cfg");
    CHECK(odoo_config_file(profile, sizeof(profile), settings, dir));
    CHECK(!odoo_config_load(&out, profile, dir, error, sizeof(error)) && !*error);
    CHECK(odoo_config_save(&config, profile, dir, error, sizeof(error)));
    CHECK(odoo_config_load(&out, profile, dir, error, sizeof(error)) && out.enabled &&
          !strcmp(out.ignored.text, "W8165,W9001"));
    config.enabled = false;
    CHECK(odoo_config_save(&config, profile, dir, error, sizeof(error)));
    CHECK(odoo_config_load(&out, profile, dir, error, sizeof(error)) && !out.enabled);
    CHECK(!odoo_config_load(&out, profile, "/wrong-project", error, sizeof(error)) && *error);
    CHECK(!remove(profile));
    CHECK(!remove(path));
    rmdir_test(alpha);
    join(path, dir, "a dir");
    rmdir_test(path);
    CHECK(!remove(file));
    rmdir_test(dir);
    printf("OK %d new-feature assertions: JS/XML lexers, bounded directory suggestions, mode filtering and "
           "project Odoo19 profile persistence; no window\n",
           checks);
    return 0;
}
