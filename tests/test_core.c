#define _POSIX_C_SOURCE 200809L
#include "buffer.h"
#include "syntax.h"
#include "runner.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#include <process.h>
#define getpid _getpid
#else
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#endif

#define CHECK(expr)                                                                                          \
    do {                                                                                                     \
        if (!(expr)) {                                                                                       \
            fprintf(stderr, "FAIL %s:%d : %s\n", __FILE__, __LINE__, #expr);                                 \
            return false;                                                                                    \
        }                                                                                                    \
    } while (0)
static char directory[4096];
static void delay(void) {
#ifdef _WIN32
    Sleep(5);
#else
    struct timespec t = {0, 5000000};
    nanosleep(&t, NULL);
#endif
}
static void path(char *out, size_t cap, const char *name) {
    size_t n = strlen(directory), m = strlen(name);
    if (n + m + 2 > cap) {
        fprintf(stderr, "Test path too long\n");
        exit(2);
    }
    memcpy(out, directory, n);
    out[n] = '/';
    memcpy(out + n + 1, name, m + 1);
}
static bool unicode(void) {
    char s[5];
    CHECK(utf8_encode(0x00e9, s) == 2);
    CHECK(!strcmp(s, "é"));
    CHECK(utf8_valid("étoile λ", strlen("étoile λ")));
    CHECK(!utf8_valid("\xc0\xaf", 2));
    CHECK(!utf8_valid("\xed\xa0\x80", 3));
    CHECK(!utf8_valid("\xf4\x90\x80\x80", 4));
    CHECK(utf8_encode(0xd800, s) == 0);
    CHECK(utf8_prev("aé", 3) == 1);
    CHECK(utf8_next("aé", 3, 1) == 3);
    return true;
}
static bool replace_undo(void) {
    Buffer b;
    CHECK(buf_init(&b, "abc\ndef\nghi", "test.py"));
    CHECK(buf_replace(&b, 1, 10, "X\nY"));
    CHECK(!strcmp(b.text, "aX\nYi"));
    CHECK(b.cursor == 4);
    CHECK(buf_undo(&b));
    CHECK(!strcmp(b.text, "abc\ndef\nghi"));
    CHECK(buf_redo(&b));
    CHECK(!strcmp(b.text, "aX\nYi"));
    buf_free(&b);
    return true;
}
static bool pairs(void) {
    Buffer b;
    CHECK(buf_init(&b, "", "test.py"));
    CHECK(buf_type(&b, '('));
    CHECK(!strcmp(b.text, "()"));
    CHECK(b.cursor == 1);
    CHECK(buf_type(&b, ')'));
    CHECK(b.cursor == 2 && b.len == 2);
    buf_move(&b, 1, false);
    buf_backspace(&b, false);
    CHECK(!*b.text);
    CHECK(buf_insert(&b, "hello"));
    buf_select_all(&b);
    CHECK(buf_type(&b, '"'));
    CHECK(!strcmp(b.text, "\"hello\""));
    char *selected = buf_selected(&b);
    CHECK(!strcmp(selected, "hello"));
    free(selected);
    buf_free(&b);
    return true;
}
static bool indentation(void) {
    Buffer b;
    CHECK(buf_init(&b, "def foo():", "test.py"));
    buf_move(&b, b.len, false);
    buf_newline(&b);
    CHECK(!strcmp(b.text, "def foo():\n    "));
    CHECK(buf_insert(&b, "pass"));
    CHECK(buf_undo(&b));
    CHECK(!strcmp(b.text, "def foo():\n    "));
    buf_free(&b);
    CHECK(buf_init(&b, "one\ntwo\nthree", "test.py"));
    b.anchor = 0;
    b.cursor = 8;
    buf_indent(&b, false);
    CHECK(!strcmp(b.text, "    one\n    two\nthree"));
    buf_indent(&b, true);
    CHECK(!strcmp(b.text, "one\ntwo\nthree"));
    buf_free(&b);
    return true;
}
static bool pair_newline(void) {
    Buffer b;
    CHECK(buf_init(&b, "{}", "t.py"));
    buf_move(&b, 1, false);
    buf_newline(&b);
    CHECK(!strcmp(b.text, "{\n    \n}"));
    CHECK(b.cursor == 6);
    buf_free(&b);
    return true;
}
static bool comments(void) {
    Buffer b;
    CHECK(buf_init(&b, "    a = 1\n    b = 2", "t.py"));
    buf_select_all(&b);
    buf_comment(&b);
    CHECK(!strcmp(b.text, "    # a = 1\n    # b = 2"));
    buf_comment(&b);
    CHECK(!strcmp(b.text, "    a = 1\n    b = 2"));
    buf_free(&b);
    return true;
}
static bool delete_boundaries(void) {
    Buffer b;
    CHECK(buf_init(&b, "x", "t.py"));
    buf_backspace(&b, false);
    CHECK(!strcmp(b.text, "x"));
    buf_move(&b, 1, false);
    buf_delete(&b, false);
    CHECK(!strcmp(b.text, "x"));
    CHECK(buf_insert(&b, "\né"));
    buf_move(&b, 2, false);
    buf_backspace(&b, false);
    CHECK(!strcmp(b.text, "xé"));
    buf_delete(&b, false);
    CHECK(!strcmp(b.text, "x"));
    buf_free(&b);
    return true;
}
static bool line_positions(void) {
    Buffer b;
    CHECK(buf_init(&b, "étoile\n\tλ=3\n", "t.py"));
    buf_index(&b);
    CHECK(b.line_count == 3);
    CHECK(buf_column(&b, 3) == 2);
    CHECK(buf_position(&b, 1, 4) == 9);
    CHECK(buf_row(&b, b.len) == 2);
    buf_free(&b);
    return true;
}
static bool duplicate_find(void) {
    Buffer b;
    CHECK(buf_init(&b, "hello\nworld", "t.py"));
    buf_move(&b, 2, false);
    buf_duplicate(&b);
    CHECK(!strcmp(b.text, "hello\nhello\nworld"));
    CHECK(b.cursor == 8);
    CHECK(buf_find(&b, "HELLO", 0, false) == 0);
    CHECK(buf_find(&b, "hello", 1, false) == 6);
    CHECK(buf_find(&b, "hello", b.len, true) == 6);
    CHECK(buf_find(&b, "missing", 0, false) == SIZE_MAX);
    buf_free(&b);
    return true;
}
static bool random_edits(void) {
    Buffer b;
    CHECK(buf_init(&b, "", "t.py"));
    char reference[8192] = "";
    srand(42);
    const char *values[] = {"", "a", "xy\nz", " \n", "étoile"};
    for (int i = 0; i < 500; i++) {
        size_t len = strlen(reference), a = (size_t)rand() % (len + 1), z = (size_t)rand() % (len + 1);
        while (a && a < len && ((unsigned char)reference[a] & 0xc0) == 0x80)
            a--;
        while (z && z < len && ((unsigned char)reference[z] & 0xc0) == 0x80)
            z--;
        if (a > z) {
            size_t t = a;
            a = z;
            z = t;
        }
        const char *v = values[rand() % 5];
        size_t n = strlen(v);
        memmove(reference + a + n, reference + z, len - z + 1);
        memcpy(reference + a, v, n);
        CHECK(buf_replace(&b, a, z, v));
        CHECK(!strcmp(reference, b.text));
        CHECK(utf8_valid(b.text, b.len));
    }
    buf_free(&b);
    return true;
}
static bool file_roundtrip(void) {
    char filename[4096], err[512] = "";
    path(filename, sizeof(filename), "crlf.py");
    FILE *f = fopen(filename, "wb");
    CHECK(f);
    const char *value = "\xef\xbb\xbf"
                        "a = 1\r\nb = 'é'\r\n";
    fwrite(value, 1, strlen(value), f);
    fclose(f);
    Buffer b;
    CHECK(buf_load(&b, filename, err, sizeof(err)));
    CHECK(b.bom && b.crlf);
    CHECK(!buf_dirty(&b));
    buf_move(&b, 5, false);
    CHECK(buf_insert(&b, "0"));
    CHECK(buf_dirty(&b));
    CHECK(buf_save(&b, filename, err, sizeof(err)));
    CHECK(!buf_dirty(&b));
    CHECK(buf_save(&b, b.path, err, sizeof(err)));
    char bytes[128] = {0};
    f = fopen(filename, "rb");
    CHECK(f);
    fread(bytes, 1, sizeof(bytes) - 1, f);
    fclose(f);
    CHECK(!strcmp(bytes, "\xef\xbb\xbf"
                         "a = 10\r\nb = 'é'\r\n"));
    buf_free(&b);
    remove(filename);
    return true;
}
static bool reject_binary(void) {
    char filename[4096], err[512];
    path(filename, sizeof(filename), "binary.dat");
    FILE *f = fopen(filename, "wb");
    CHECK(f);
    fwrite("a\0b", 1, 3, f);
    fclose(f);
    Buffer b;
    CHECK(!buf_load(&b, filename, err, sizeof(err)));
    remove(filename);
    return true;
}
static bool lexing(void) {
    const char *s = "def hello(x=3):\n    print(\"salut\") # commentaire\n";
    size_t n = strlen(s);
    uint8_t *colors = malloc(n);
    CHECK(colors);
    syntax_colorize(s, n, colors);
    CHECK(colors[0] == SYN_KEYWORD && colors[4] == SYN_DEFINITION);
    CHECK(colors[strstr(s, "print") - s] == SYN_BUILTIN);
    CHECK(colors[strstr(s, "#") - s] == SYN_COMMENT);
    CHECK(colors[strstr(s, "3") - s] == SYN_NUMBER);
    free(colors);
    return true;
}
static bool multiline_lexing(void) {
    const char *s = "s = '''hello\nworld'''\nx = 1";
    uint8_t colors[128];
    syntax_colorize(s, strlen(s), colors);
    CHECK(colors[13] == SYN_STRING);
    CHECK(colors[strstr(s, "x =") - s] == SYN_TEXT);
    return true;
}
static bool completion(void) {
    char words[32][96];
    int count = syntax_complete("private_name = 1", "pri", words, 32);
    bool print = false, custom = false;
    for (int i = 0; i < count; i++) {
        if (!strcmp(words[i], "print"))
            print = true;
        if (!strcmp(words[i], "private_name"))
            custom = true;
    }
    CHECK(print && custom);
    return true;
}
static bool collect(Runner *r, char *out, size_t cap) {
    double deadline = runner_clock() + 5;
    size_t used = 0;
    out[0] = 0;
    while (r->running && runner_clock() < deadline) {
        char chunk[4096];
        size_t n = runner_poll(r, chunk, sizeof(chunk));
        if (n && used + n < cap) {
            memcpy(out + used, chunk, n);
            used += n;
            out[used] = 0;
        }
        delay();
    }
    return !r->running;
}
static const char *interpreter(void) {
    const char *p = getenv("PRISME_PYTHON");
    if (p && *p)
        return p;
#ifdef _WIN32
    return "python";
#else
    return "python3";
#endif
}
static bool execution(void) {
    Runner r;
    runner_init(&r);
    char error[512], out[16384], filename[4096];
    path(filename, sizeof(filename), "main.py");
    CHECK(runner_start(&r, interpreter(), "print('Bonjour, étoile !')", filename, directory, error,
                       sizeof(error)));
    CHECK(collect(&r, out, sizeof(out)));
    CHECK(r.exit_code == 0);
    CHECK(strstr(out, "Bonjour, étoile !"));
    CHECK(!r.temporary[0]);
    runner_close(&r);
    return true;
}
static bool local_import(void) {
    char module[4096], filename[4096], out[16384], err[512];
    path(module, sizeof(module), "local_module.py");
    path(filename, sizeof(filename), "main.py");
    FILE *f = fopen(module, "w");
    CHECK(f);
    fputs("VALUE = 42\n", f);
    fclose(f);
    Runner r;
    runner_init(&r);
    CHECK(runner_start(&r, interpreter(), "from local_module import VALUE\nprint(VALUE)", filename, directory,
                       err, sizeof(err)));
    CHECK(collect(&r, out, sizeof(out)));
    CHECK(r.exit_code == 0 && strstr(out, "42"));
    runner_close(&r);
    remove(module);
    return true;
}
static bool traceback(void) {
    Runner r;
    runner_init(&r);
    char filename[4096], err[512], out[16384];
    path(filename, sizeof(filename), "original.py");
    CHECK(runner_start(&r, interpreter(), "raise ValueError('test')", filename, directory, err, sizeof(err)));
    CHECK(collect(&r, out, sizeof(out)));
    CHECK(r.exit_code == 1 && strstr(out, "original.py") && strstr(out, "ValueError: test"));
    runner_close(&r);
    return true;
}
static bool standard_input(void) {
    Runner r;
    runner_init(&r);
    char filename[4096], err[512], out[16384];
    path(filename, sizeof(filename), "stdin.py");
    CHECK(runner_start(&r, interpreter(), "s=input('Nom : '); print('Bonjour',s)", filename, directory, err,
                       sizeof(err)));
    CHECK(runner_input(&r, "Ada"));
    CHECK(collect(&r, out, sizeof(out)));
    CHECK(strstr(out, "Bonjour Ada"));
    runner_close(&r);
    return true;
}
static bool stop_process(void) {
    Runner r;
    runner_init(&r);
    char filename[4096], err[512], out[16384];
    path(filename, sizeof(filename), "loop.py");
    CHECK(runner_start(&r, interpreter(), "import time\nwhile True: time.sleep(0.1)", filename, directory,
                       err, sizeof(err)));
    runner_stop(&r);
    CHECK(collect(&r, out, sizeof(out)));
    CHECK(r.stopped && r.exit_code != 0);
    runner_close(&r);
    return true;
}
int main(void) {
    const char *tmp = getenv("TMPDIR");
#ifdef _WIN32
    if (!tmp)
        tmp = getenv("TEMP");
#endif
    if (!tmp)
        tmp = "/tmp";
    snprintf(directory, sizeof(directory), "%s/prisme-c-tests-%ld", tmp, (long)getpid());
#ifdef _WIN32
    _mkdir(directory);
#else
    mkdir(directory, 0700);
#endif
    struct {
        const char *name;
        bool (*test)(void);
    } tests[] = {{"UTF-8", unicode},
                 {"replace / undo / redo", replace_undo},
                 {"pairs", pairs},
                 {"indentation", indentation},
                 {"new line in pair", pair_newline},
                 {"comments", comments},
                 {"deletion boundaries", delete_boundaries},
                 {"line index", line_positions},
                 {"duplicate / search", duplicate_find},
                 {"500 random edits", random_edits},
                 {"atomic save / BOM / CRLF", file_roundtrip},
                 {"binary rejection", reject_binary},
                 {"Python lexer", lexing},
                 {"multiline strings", multiline_lexing},
                 {"completion", completion},
                 {"Python execution / UTF-8", execution},
                 {"local imports", local_import},
                 {"traceback filename", traceback},
                 {"stdin", standard_input},
                 {"stop process", stop_process}};
    int passed = 0, count = (int)(sizeof(tests) / sizeof(tests[0]));
    for (int i = 0; i < count; i++) {
        if (tests[i].test()) {
            printf("OK %s\n", tests[i].name);
            passed++;
        } else
            printf("FAILED %s\n", tests[i].name);
    }
    printf("%d/%d tests passed\n", passed, count);
    return passed == count ? 0 : 1;
}
