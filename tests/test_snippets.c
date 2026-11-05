#define _POSIX_C_SOURCE 200809L
#include "snippets.h"
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
static void write_file(const char *path, const void *text, size_t n) {
    FILE *f = fopen(path, "wb");
    CHECK(f && fwrite(text, 1, n, f) == n && !fclose(f));
}
int main(void) {
    Snippets library = {0}, loaded = {0}, clone = {0};
    char error[512], path[BUF_PATH], settings[BUF_PATH], directory[BUF_PATH];
    CHECK(snippets_valid(&library));
    CHECK(snippets_default(&library) && library.count == 1);
    CHECK(!strcmp(library.items[0].trigger, "xml_form") && strstr(library.items[0].body, "ir.ui.view"));
    CHECK(!snippets_put(&library, -1, "xml_form", "", "duplicate", error, sizeof(error)) &&
          strstr(error, "existe"));
    for (const char **p = (const char *[]){"", "0bad", "two words", "a.b", "déclencheur", "a\n", NULL}; *p;
         p++)
        CHECK(!snippets_put(&library, -1, *p, "", "code", error, sizeof(error)));
    CHECK(!snippets_put(&library, -2, "bad", "", "code", error, sizeof(error)));
    CHECK(!snippets_put(&library, 1, "bad", "", "code", error, sizeof(error)));
    CHECK(!snippets_put(&library, -1, "empty", "", "", error, sizeof(error)));
    CHECK(!snippets_put(&library, -1, "binary", "", "\xff", error, sizeof(error)));
    CHECK(!snippets_put(&library, -1, "title", "bad\nlabel", "code", error, sizeof(error)));
    CHECK(!snippets_put(&library, -1, "control", "", "a\x01", error, sizeof(error)));
    CHECK(snippets_put(&library, -1, "xml_field", "Un champ accentué",
                       "<field name=\"été\"/>\nEND\nSNIP\tx\n$0 ${literal}", error, sizeof(error)));
    CHECK(snippets_put(&library, -1, "xml_f", "Court", "abc", error, sizeof(error)));
    int indices[64];
    CHECK(snippets_match(&library, "xml_f", indices, 64) == 3 && indices[0] == 2 && indices[1] == 0);
    CHECK(snippets_match(&library, "xml_form", indices, 64) == 1 && indices[0] == 0);
    CHECK(snippets_match(&library, "XML", indices, 64) == 0);
    CHECK(snippets_match(&library, "", indices, 1) == 1 && indices[0] == 2);
    CHECK(snippets_clone(&clone, &library));
    CHECK(clone.items[0].body != library.items[0].body &&
          !strcmp(clone.items[1].body, library.items[1].body));
    CHECK(snippets_put(&clone, 0, clone.items[0].trigger, clone.items[0].title, clone.items[0].body, error,
                       sizeof(error)));
    CHECK(snippets_remove(&clone, 0) && clone.count == 2 && !strcmp(clone.items[0].trigger, "xml_field"));
    CHECK(!snippets_remove(&clone, -1) && !snippets_remove(&clone, 2));
    snippets_free(&clone);
#ifdef _WIN32
    char temp[MAX_PATH];
    CHECK(GetTempPathA(MAX_PATH, temp));
    CHECK(GetTempFileNameA(temp, "snp", 0, directory));
    CHECK(!remove(directory) && !_mkdir(directory));
#else
    strcpy(directory, "/tmp/prisme-snippets-XXXXXX");
    CHECK(mkdtemp(directory));
#endif
    CHECK(snprintf(settings, sizeof(settings), "%s/settings.cfg", directory) < BUF_PATH);
    CHECK(snippets_file(path, sizeof(path), settings));
    CHECK(!snippets_load(&loaded, path, error, sizeof(error)) && !*error && !loaded.count);
    CHECK(snippets_save(&library, path, error, sizeof(error)));
#ifndef _WIN32
    struct stat st;
    CHECK(!stat(path, &st) && (st.st_mode & 0777) == 0600);
#endif
    CHECK(snippets_load(&loaded, path, error, sizeof(error)) && loaded.count == library.count);
    CHECK(!strcmp(loaded.items[1].body, library.items[1].body));
    FILE *f = fopen(path, "rb");
    CHECK(f);
    CHECK(!fseek(f, 0, SEEK_END));
    long length = ftell(f);
    CHECK(length > 0 && !fseek(f, 0, SEEK_SET));
    char *serialized = malloc((size_t)length + 1);
    CHECK(serialized && fread(serialized, 1, (size_t)length, f) == (size_t)length && !fclose(f));
    serialized[length] = 0;
    char *original = loaded.items[0].body;
    for (size_t i = 0; i < (size_t)length; i++) {
        write_file(path, serialized, i);
        CHECK(!snippets_load(&loaded, path, error, sizeof(error)) && *error);
        CHECK(loaded.count == 3 && loaded.items[0].body == original);
    }
    const char *malformed = "PRISME-SNIPPETS\t1\nSNIP\tx\t99999999999999999999999\t1\nx\nEND\n";
    write_file(path, malformed, strlen(malformed));
    CHECK(!snippets_load(&loaded, path, error, sizeof(error)) && loaded.items[0].body == original);
    serialized[5] = 0;
    write_file(path, serialized, (size_t)length);
    CHECK(!snippets_load(&loaded, path, error, sizeof(error)) && loaded.items[0].body == original);
    free(serialized);
    CHECK(snippets_save(&library, path, error, sizeof(error)));
    char bad[BUF_PATH];
    CHECK(snprintf(bad, sizeof(bad), "%s/missing/deeper/snippets.cfg", directory) < BUF_PATH);
    CHECK(!snippets_save(&library, bad, error, sizeof(error)) && *error);
    CHECK(snippets_load(&loaded, path, error, sizeof(error)) && loaded.count == 3);
    Snippets empty = {0};
    CHECK(snippets_save(&empty, path, error, sizeof(error)));
    CHECK(snippets_load(&loaded, path, error, sizeof(error)) && !loaded.count);
    /* Complete maximum-size libraries fit the atomic Buffer save limit. */
    char *large = malloc(SNIPPET_BODY_MAX + 2);
    CHECK(large);
    memset(large, 'x', SNIPPET_BODY_MAX + 1);
    large[SNIPPET_BODY_MAX + 1] = 0;
    CHECK(!snippets_put(&empty, -1, "too_long", "", large, error, sizeof(error)));
    large[SNIPPET_BODY_MAX] = 0;
    for (int i = 0; i < SNIPPETS_MAX; i++) {
        char name[32];
        snprintf(name, sizeof(name), "macro_%02d", i);
        CHECK(snippets_put(&empty, -1, name, "", large, error, sizeof(error)));
    }
    CHECK(!snippets_put(&empty, -1, "overflow", "", "x", error, sizeof(error)) && empty.count == 64);
    CHECK(snippets_save(&empty, path, error, sizeof(error)));
    CHECK(snippets_load(&loaded, path, error, sizeof(error)) && loaded.count == 64 &&
          strlen(loaded.items[63].body) == SNIPPET_BODY_MAX);
    CHECK(snippets_match(&loaded, "macro_", indices, 3) == 3 && indices[0] == 0 && indices[2] == 2);
    free(large);
    snippets_free(&empty);
    /* Literal, UTF-8-aware multiline expansion, exact indentation and atomic undo. */
    Buffer b;
    CHECK(buf_init(&b, "\t  xml_form_tail rest", "vue.xml"));
    b.bom = b.crlf = true;
    Snippet code = {.body = "<form>\r\n    <field name=\"été\"/>\r</form>"};
    unsigned long version = b.version;
    CHECK(snippet_expand(&b, &code, 3, 16, error, sizeof(error)));
    CHECK(!strcmp(b.text, "\t  <form>\n\t      <field name=\"été\"/>\n\t  </form> rest"));
    CHECK(b.version == version + 1 && b.bom && b.crlf && b.undo.count == 1);
    CHECK(buf_undo(&b) && !strcmp(b.text, "\t  xml_form_tail rest"));
    CHECK(buf_redo(&b) && strstr(b.text, "été"));
    buf_free(&b);
    CHECK(buf_init(&b, "é xml_form !", "test.xml"));
    code.body = "a\n  b\n$0 ${literal}";
    version = b.version;
    CHECK(!snippet_expand(&b, &code, 1, 2, error, sizeof(error)) && b.version == version);
    CHECK(snippet_expand(&b, &code, 3, 11, error, sizeof(error)));
    CHECK(!strcmp(b.text, "é a\n  b\n$0 ${literal} !"));
    buf_free(&b);
    CHECK(buf_init(&b, "xml_form", "test.py"));
    CHECK(buf_add_caret(&b, 2, BUF_NO_SELECTION));
    version = b.version;
    CHECK(!snippet_expand(&b, &code, 0, b.len, error, sizeof(error)) && b.version == version &&
          b.extra_count);
    buf_clear_carets(&b);
    CHECK(!snippet_expand(&b, &code, 9, 0, error, sizeof(error)));
    buf_free(&b);
    char *full = malloc(BUF_LIMIT + 1);
    CHECK(full);
    memset(full, 'x', BUF_LIMIT);
    full[BUF_LIMIT] = 0;
    CHECK(buf_init(&b, full, "full.py"));
    version = b.version;
    CHECK(!snippet_expand(&b, &code, 0, 0, error, sizeof(error)) && b.version == version &&
          b.len == BUF_LIMIT);
    free(full);
    buf_free(&b);
    CHECK(!remove(path));
#ifdef _WIN32
    CHECK(!_rmdir(directory));
#else
    CHECK(!rmdir(directory));
#endif
    snippets_free(&clone);
    snippets_free(&library);
    snippets_free(&loaded);
    printf("OK %d snippet checks: global persistence, 64 entries, bounded/corrupt/truncated profiles, UTF-8, "
           "literal XML, indentation, atomic undo, limits; no GUI\n",
           checks);
    return 0;
}
