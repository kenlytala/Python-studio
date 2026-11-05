#define _POSIX_C_SOURCE 200809L
#include "buffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <process.h>
#define getpid _getpid
#else
#include <unistd.h>
#endif
#define CHECK(x)                                                                                             \
    do {                                                                                                     \
        if (!(x)) {                                                                                          \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x);                                     \
            return false;                                                                                    \
        }                                                                                                    \
    } while (0)
static bool insert_history(void) {
    Buffer b;
    CHECK(buf_init(&b, "abc\ndef", "test.py"));
    CHECK(buf_add_caret(&b, 4, BUF_NO_SELECTION));
    CHECK(buf_insert(&b, "é"));
    CHECK(!strcmp(b.text, "éabc\nédef"));
    CHECK(buf_caret_count(&b) == 2 && b.undo.count == 1);
    CHECK(buf_undo(&b));
    CHECK(!strcmp(b.text, "abc\ndef") && buf_caret_count(&b) == 2);
    CHECK(buf_redo(&b));
    CHECK(!strcmp(b.text, "éabc\nédef") && buf_caret_count(&b) == 2);
    buf_free(&b);
    return true;
}
static bool toggle(void) {
    Buffer b;
    CHECK(buf_init(&b, "abcd", "x"));
    buf_toggle_caret(&b, 2);
    CHECK(buf_caret_count(&b) == 2 && b.cursor == 2);
    buf_toggle_caret(&b, 2);
    CHECK(buf_caret_count(&b) == 1 && b.cursor == 0);
    buf_toggle_caret(&b, 0);
    CHECK(buf_caret_count(&b) == 1);
    buf_free(&b);
    return true;
}
static bool merge_ranges(void) {
    Buffer b;
    CHECK(buf_init(&b, "abcdef", "x"));
    BufCaret cs[] = {{4, 1}, {5, 2}};
    buf_set_carets(&b, cs, 2);
    CHECK(buf_caret_count(&b) == 1);
    size_t a, z;
    CHECK(buf_selection(&b, &a, &z) && a == 1 && z == 5);
    BufCaret adjacent[] = {{2, 0}, {4, 2}};
    buf_set_carets(&b, adjacent, 2);
    CHECK(buf_caret_count(&b) == 2);
    CHECK(buf_insert(&b, "X"));
    CHECK(!strcmp(b.text, "XXef"));
    CHECK(b.undo.count == 1);
    CHECK(buf_undo(&b) && buf_caret_count(&b) == 2);
    buf_free(&b);
    return true;
}
static bool occurrences(void) {
    Buffer b;
    CHECK(buf_init(&b, "café café CAFÉ", "x"));
    CHECK(buf_select_next(&b) && buf_caret_count(&b) == 1);
    CHECK(buf_select_next(&b) && buf_caret_count(&b) == 2);
    CHECK(!buf_select_next(&b));
    CHECK(buf_insert(&b, "étoile"));
    CHECK(!strcmp(b.text, "étoile étoile CAFÉ"));
    CHECK(buf_undo(&b) && buf_caret_count(&b) == 2);
    char *s = buf_selected(&b);
    CHECK(s && !strcmp(s, "café\ncafé"));
    free(s);
    buf_free(&b);
    return true;
}
static bool caret_limit(void) {
    char text[601];
    for (int i = 0; i < 300; i++) {
        text[2 * i] = 'a';
        text[2 * i + 1] = ' ';
    }
    text[600] = 0;
    Buffer b;
    CHECK(buf_init(&b, text, "x"));
    buf_select_occurrences(&b);
    CHECK(buf_caret_count(&b) == BUF_MAX_CARETS);
    CHECK(!buf_select_next(&b));
    CHECK(buf_insert(&b, "z"));
    CHECK(b.undo.count == 1);
    CHECK(buf_undo(&b));
    CHECK(!strcmp(b.text, text) && buf_caret_count(&b) == BUF_MAX_CARETS);
    buf_free(&b);
    return true;
}
static bool overlapping_delete(void) {
    Buffer b;
    CHECK(buf_init(&b, "abc", "x"));
    BufCaret cs[] = {{1, BUF_NO_SELECTION}, {2, BUF_NO_SELECTION}};
    buf_set_carets(&b, cs, 2);
    buf_backspace(&b, false);
    CHECK(!strcmp(b.text, "c") && buf_caret_count(&b) == 1);
    CHECK(buf_undo(&b) && !strcmp(b.text, "abc") && buf_caret_count(&b) == 2);
    buf_delete(&b, false);
    CHECK(!strcmp(b.text, "a"));
    CHECK(buf_undo(&b));
    BufCaret overlapping[] = {{1, BUF_NO_SELECTION}, {3, BUF_NO_SELECTION}};
    buf_set_carets(&b, overlapping, 2);
    buf_backspace(&b, true);
    CHECK(!strcmp(b.text, ""));
    CHECK(buf_caret_count(&b) == 1);
    buf_free(&b);
    return true;
}
static bool utf8_boundaries(void) {
    Buffer b;
    CHECK(buf_init(&b, "aé", "x"));
    buf_move(&b, 1, false);
    CHECK(buf_add_caret(&b, 3, BUF_NO_SELECTION));
    CHECK(buf_type(&b, 0x03bb));
    CHECK(!strcmp(b.text, "aλéλ"));
    buf_backspace(&b, false);
    CHECK(!strcmp(b.text, "aé"));
    buf_clear_carets(&b);
    buf_move(&b, 0, false);
    CHECK(buf_add_caret(&b, 2, BUF_NO_SELECTION));
    CHECK(b.cursor == 1);
    CHECK(utf8_valid(b.text, b.len));
    buf_free(&b);
    return true;
}
static bool pairing(void) {
    Buffer b;
    CHECK(buf_init(&b, "\n", "x"));
    CHECK(buf_add_caret(&b, 1, BUF_NO_SELECTION));
    CHECK(buf_type(&b, '('));
    CHECK(!strcmp(b.text, "()\n()"));
    buf_backspace(&b, false);
    CHECK(!strcmp(b.text, "\n"));
    CHECK(buf_undo(&b));
    int undo = b.undo.count;
    CHECK(buf_type(&b, ')'));
    CHECK(!strcmp(b.text, "()\n()") && b.undo.count == undo);
    for (int i = 0; i < 2; i++)
        CHECK(b.text[buf_caret(&b, i).cursor - 1] == ')');
    buf_free(&b);
    return true;
}
static bool wrap_selection(void) {
    Buffer b;
    CHECK(buf_init(&b, "ab cd", "x"));
    BufCaret cs[] = {{2, 0}, {5, 3}};
    buf_set_carets(&b, cs, 2);
    CHECK(buf_type(&b, '"'));
    CHECK(!strcmp(b.text, "\"ab\" \"cd\""));
    char *s = buf_selected(&b);
    CHECK(s && !strcmp(s, "ab\ncd"));
    free(s);
    CHECK(buf_undo(&b));
    CHECK(buf_redo(&b));
    CHECK(buf_caret_count(&b) == 2);
    buf_free(&b);
    return true;
}
static bool newline_indents(void) {
    Buffer b;
    CHECK(buf_init(&b, "if a:\n  x", "x"));
    BufCaret cs[] = {{5, BUF_NO_SELECTION}, {9, BUF_NO_SELECTION}};
    buf_set_carets(&b, cs, 2);
    buf_newline(&b);
    CHECK(!strcmp(b.text, "if a:\n    \n  x\n  "));
    CHECK(b.undo.count == 1);
    CHECK(buf_undo(&b) && !strcmp(b.text, "if a:\n  x"));
    buf_free(&b);
    return true;
}
static bool newline_pairs(void) {
    Buffer b;
    CHECK(buf_init(&b, "()\n()", "x"));
    BufCaret cs[] = {{1, BUF_NO_SELECTION}, {4, BUF_NO_SELECTION}};
    buf_set_carets(&b, cs, 2);
    buf_newline(&b);
    CHECK(!strcmp(b.text, "(\n    \n)\n(\n    \n)"));
    for (int i = 0; i < 2; i++)
        CHECK(b.text[buf_caret(&b, i).cursor - 1] == ' ' && b.text[buf_caret(&b, i).cursor] == '\n');
    buf_free(&b);
    return true;
}
static bool navigation(void) {
    Buffer b;
    CHECK(buf_init(&b, "abc\nxyz", "x"));
    BufCaret cs[] = {{1, BUF_NO_SELECTION}, {5, BUF_NO_SELECTION}};
    buf_set_carets(&b, cs, 2);
    buf_horizontal(&b, 1, false, false);
    CHECK(b.cursor == 2 && b.extra[0].cursor == 6);
    buf_home_end(&b, false, false, false);
    CHECK(b.cursor == 0 && b.extra[0].cursor == 4);
    buf_home_end(&b, true, false, true);
    char *s = buf_selected(&b);
    CHECK(s && !strcmp(s, "abc\nxyz"));
    free(s);
    CHECK(buf_insert(&b, "z") && !strcmp(b.text, "z\nz"));
    CHECK(buf_undo(&b));
    buf_vertical(&b, 1, false);
    CHECK(buf_caret_count(&b) == 1);
    buf_free(&b);
    return true;
}
static bool tabs(void) {
    Buffer b;
    CHECK(buf_init(&b, "a\nbc", "x"));
    CHECK(buf_add_caret(&b, 3, BUF_NO_SELECTION));
    buf_indent(&b, false);
    CHECK(!strcmp(b.text, "    a\nb   c"));
    CHECK(b.undo.count == 1);
    CHECK(buf_undo(&b));
    buf_free(&b);
    return true;
}
static bool transform(void) {
    Buffer b;
    CHECK(buf_init(&b, "a\nb\nc", "x"));
    BufCaret cs[] = {{1, 0}, {5, 4}};
    buf_set_carets(&b, cs, 2);
    buf_indent(&b, false);
    CHECK(!strcmp(b.text, "    a\nb\n    c"));
    char *text = buf_selected(&b);
    CHECK(text && !strcmp(text, "a\nc"));
    free(text);
    buf_comment(&b);
    CHECK(!strcmp(b.text, "    # a\nb\n    # c"));
    buf_comment(&b);
    buf_indent(&b, true);
    CHECK(!strcmp(b.text, "a\nb\nc"));
    CHECK(buf_caret_count(&b) == 2);
    buf_free(&b);
    return true;
}
static bool duplicate_lines(void) {
    Buffer b;
    CHECK(buf_init(&b, "word\nother", "x"));
    CHECK(buf_add_caret(&b, 2, BUF_NO_SELECTION));
    CHECK(buf_add_caret(&b, 7, BUF_NO_SELECTION));
    buf_comment(&b);
    CHECK(!strcmp(b.text, "# word\n# other"));
    CHECK(buf_undo(&b));
    buf_duplicate(&b);
    CHECK(!strcmp(b.text, "word\nother\nword\nother"));
    CHECK(b.undo.count == 1);
    CHECK(buf_undo(&b) && buf_caret_count(&b) == 3);
    buf_free(&b);
    return true;
}
static bool cut_copy(void) {
    Buffer b;
    CHECK(buf_init(&b, "a\nb\nc", "x"));
    CHECK(buf_add_caret(&b, 4, BUF_NO_SELECTION));
    char *s = buf_copy_carets(&b, true);
    CHECK(s && !strcmp(s, "a\nc\n"));
    free(s);
    CHECK(!strcmp(b.text, "b"));
    CHECK(buf_undo(&b) && !strcmp(b.text, "a\nb\nc"));
    BufCaret cs[] = {{1, 0}, {5, 4}};
    buf_set_carets(&b, cs, 2);
    s = buf_copy_carets(&b, true);
    CHECK(s && !strcmp(s, "a\nc"));
    free(s);
    CHECK(!strcmp(b.text, "\nb\n"));
    buf_free(&b);
    return true;
}
static bool paste(void) {
    Buffer b;
    CHECK(buf_init(&b, "X\nY", "x"));
    CHECK(buf_add_caret(&b, 2, BUF_NO_SELECTION));
    CHECK(buf_paste(&b, "one\ntwo"));
    CHECK(!strcmp(b.text, "oneX\ntwoY"));
    CHECK(buf_paste(&b, "Q"));
    CHECK(!strcmp(b.text, "oneQX\ntwoQY"));
    CHECK(buf_undo(&b) && buf_undo(&b));
    CHECK(!strcmp(b.text, "X\nY") && buf_caret_count(&b) == 2);
    buf_free(&b);
    return true;
}
static bool atomic_rejection(void) {
    char *full = malloc(BUF_LIMIT);
    CHECK(full);
    memset(full, 'a', BUF_LIMIT - 1);
    full[BUF_LIMIT - 1] = 0;
    Buffer b;
    CHECK(buf_init(&b, full, "x"));
    CHECK(buf_add_caret(&b, b.len, BUF_NO_SELECTION));
    CHECK(!buf_insert(&b, "xx"));
    CHECK(!strcmp(b.text, full) && b.undo.count == 0 && !buf_dirty(&b));
    CHECK(!buf_insert(&b, "\xc0\xaf"));
    CHECK(b.undo.count == 0);
    free(full);
    buf_free(&b);
    CHECK(buf_init(&b, "aébc", "x"));
    BufEdit invalid[] = {{2, 3, "x"}};
    CHECK(!buf_apply_edits(&b, invalid, 1));
    BufEdit overlap[] = {{0, 3, "x"}, {1, 4, "y"}};
    CHECK(!buf_apply_edits(&b, overlap, 2));
    CHECK(!strcmp(b.text, "aébc") && b.undo.count == 0);
    buf_free(&b);
    return true;
}
static bool explicit_replace(void) {
    Buffer b;
    CHECK(buf_init(&b, "abc", "x"));
    CHECK(buf_add_caret(&b, 3, BUF_NO_SELECTION));
    CHECK(buf_replace(&b, 1, 2, ""));
    CHECK(!strcmp(b.text, "ac") && buf_caret_count(&b) == 1);
    CHECK(buf_undo(&b) && buf_caret_count(&b) == 2);
    buf_free(&b);
    return true;
}
static bool history_eviction(void) {
    Buffer b;
    CHECK(buf_init(&b, "\n", "x"));
    CHECK(buf_add_caret(&b, 1, BUF_NO_SELECTION));
    for (int i = 0; i < 200; i++)
        CHECK(buf_insert(&b, "x"));
    CHECK(b.undo.count == BUF_HISTORY);
    for (int i = 0; i < BUF_HISTORY; i++)
        CHECK(buf_undo(&b));
    CHECK(!buf_undo(&b) && buf_caret_count(&b) == 2);
    for (int i = 0; i < BUF_HISTORY; i++)
        CHECK(buf_redo(&b));
    CHECK(b.len == 401 && buf_caret_count(&b) == 2);
    buf_free(&b);
    return true;
}
static unsigned random_state = 0x197a33u;
static unsigned rng(void) {
    random_state = random_state * 1664525u + 1013904223u;
    return random_state;
}
static bool random_edits(void) {
    const char *base = "abcdefghij";
    for (int iteration = 0; iteration < 500; iteration++) {
        Buffer b;
        CHECK(buf_init(&b, base, "x"));
        int positions[11] = {1};
        int count = 2 + (int)(rng() % 5);
        while (buf_caret_count(&b) < count) {
            size_t p = rng() % 11;
            if (!positions[p]) {
                positions[p] = 1;
                CHECK(buf_add_caret(&b, p, BUF_NO_SELECTION));
            }
        }
        char expected[100];
        size_t used = 0;
        for (int p = 0; p <= 10; p++) {
            if (positions[p]) {
                memcpy(expected + used, "é", 2);
                used += 2;
            }
            if (p < 10)
                expected[used++] = base[p];
        }
        expected[used] = 0;
        CHECK(buf_insert(&b, "é"));
        CHECK(!strcmp(b.text, expected) && buf_caret_count(&b) == count);
        CHECK(buf_undo(&b) && !strcmp(b.text, base) && buf_caret_count(&b) == count);
        CHECK(buf_redo(&b) && !strcmp(b.text, expected));
        buf_backspace(&b, false);
        CHECK(!strcmp(b.text, base));
        CHECK(buf_undo(&b) && !strcmp(b.text, expected));
        buf_free(&b);
    }
    return true;
}
static bool save_multi(void) {
    Buffer b;
    CHECK(buf_init(&b, "café café\n", "x.py"));
    buf_select_occurrences(&b);
    CHECK(buf_insert(&b, "étoile"));
    b.crlf = true;
    b.bom = true;
    const char *tmp = getenv("TMPDIR");
#ifdef _WIN32
    if (!tmp)
        tmp = getenv("TEMP");
    if (!tmp)
        tmp = ".";
#else
    if (!tmp)
        tmp = "/tmp";
#endif
    char filename[4096], error[1024];
    snprintf(filename, sizeof(filename), "%s/prisme-multi-%ld.py", tmp, (long)getpid());
    CHECK(buf_save(&b, filename, error, sizeof(error)) && !buf_dirty(&b));
    Buffer loaded;
    CHECK(buf_load(&loaded, filename, error, sizeof(error)));
    CHECK(!strcmp(loaded.text, "étoile étoile\n") && loaded.crlf && loaded.bom);
    CHECK(buf_undo(&b) && buf_dirty(&b));
    CHECK(buf_redo(&b) && !buf_dirty(&b));
    buf_free(&loaded);
    buf_free(&b);
    remove(filename);
    return true;
}
static bool vertical_series(void) {
    Buffer b;
    const char *original = "abcd\nabcd\nabcd\nabcd\nabcd";
    CHECK(buf_init(&b, original, "vertical.py"));
    b.cursor = buf_position(&b, 2, 2);
    unsigned long version = b.version;
    CHECK(buf_add_vertical_caret(&b, 1) && buf_row(&b, b.cursor) == 3);
    CHECK(buf_add_vertical_caret(&b, -1) && buf_row(&b, b.cursor) == 1);
    CHECK(buf_add_vertical_caret(&b, -1) && buf_row(&b, b.cursor) == 0);
    CHECK(buf_add_vertical_caret(&b, 1) && buf_row(&b, b.cursor) == 4);
    CHECK(buf_caret_count(&b) == 5 && b.version == version && !b.undo.count && !strcmp(b.text, original));
    CHECK(!buf_add_vertical_caret(&b, 1) && !buf_add_vertical_caret(&b, -1));
    CHECK(buf_type(&b, '!') && !strcmp(b.text, "ab!cd\nab!cd\nab!cd\nab!cd\nab!cd"));
    CHECK(b.undo.count == 1 && buf_caret_count(&b) == 5);
    CHECK(buf_undo(&b) && !strcmp(b.text, original) && buf_caret_count(&b) == 5);
    CHECK(buf_redo(&b) && strstr(b.text, "ab!cd"));
    buf_free(&b);
    return true;
}
static bool vertical_visual_column(void) {
    Buffer b;
    const char *original = "abcdef\nx\n\tétoile\néclair\nabcdef";
    CHECK(buf_init(&b, original, "vertical.py"));
    b.cursor = buf_position(&b, 0, 5);
    b.bom = b.crlf = true;
    for (size_t row = 1; row < 5; row++) {
        CHECK(buf_add_vertical_caret_at_column(&b, 1, 5));
        CHECK(b.cursor == buf_position(&b, row, 5));
    }
    CHECK(buf_type(&b, 'Z') && !strcmp(b.text, "abcdeZf\nxZ\n\téZtoile\néclaiZr\nabcdeZf"));
    CHECK(utf8_valid(b.text, b.len) && b.bom && b.crlf && b.undo.count == 1);
    CHECK(buf_undo(&b) && !strcmp(b.text, original) && buf_caret_count(&b) == 5);
    buf_free(&b);
    return true;
}
static bool vertical_selection(void) {
    Buffer b;
    CHECK(buf_init(&b, "abc\nabc\nabc\nabc", "selection.py"));
    b.cursor = buf_position(&b, 1, 1);
    b.anchor = 0;
    CHECK(!buf_add_vertical_caret(&b, -1)); /* never merge into an existing selection */
    CHECK(b.anchor == 0 && b.cursor == 5 && buf_caret_count(&b) == 1);
    CHECK(buf_add_vertical_caret(&b, 1) && buf_caret_count(&b) == 2);
    CHECK(buf_insert(&b, "Z") && !strcmp(b.text, "Zbc\naZbc\nabc"));
    CHECK(buf_undo(&b) && !strcmp(b.text, "abc\nabc\nabc\nabc") && buf_caret_count(&b) == 2);
    buf_free(&b);
    return true;
}
static bool vertical_bounds(void) {
    Buffer b;
    CHECK(!buf_add_vertical_caret(NULL, 1) && !buf_add_vertical_caret_at_column(NULL, 1, 0));
    CHECK(buf_init(&b, "", "empty.py"));
    CHECK(!buf_add_vertical_caret(&b, -1) && !buf_add_vertical_caret(&b, 1));
    CHECK(!buf_add_vertical_caret(&b, 0) && !buf_add_vertical_caret_at_column(&b, 2, SIZE_MAX));
    CHECK(buf_caret_count(&b) == 1 && !b.undo.count);
    buf_free(&b);
    char original[1201] = "";
    for (int i = 0; i < 300; i++)
        strcat(original, "abc\n");
    CHECK(buf_init(&b, original, "limit.py"));
    b.cursor = 2;
    unsigned long version = b.version;
    for (int i = 1; i < BUF_MAX_CARETS; i++)
        CHECK(buf_add_vertical_caret(&b, 1));
    size_t cursor = b.cursor;
    CHECK(buf_caret_count(&b) == BUF_MAX_CARETS);
    CHECK(!buf_add_vertical_caret(&b, 1) && !buf_add_vertical_caret(&b, -1));
    CHECK(b.cursor == cursor && b.version == version && !b.undo.count && !strcmp(b.text, original));
    buf_free(&b);
    return true;
}
static bool vertical_matrix(void) {
    const char *source = "alpha\nx\n\tété\n\nabcd\n\tab\nlast";
    int cases = 0;
    for (size_t row = 0; row < 7; row++)
        for (size_t col = 0; col < 14; col++)
            for (int direction = -1; direction <= 1; direction += 2) {
                Buffer b;
                CHECK(buf_init(&b, source, "matrix.py"));
                b.cursor = buf_position(&b, row, col);
                unsigned long version = b.version;
                for (int i = 0; i < 7; i++)
                    buf_add_vertical_caret_at_column(&b, direction, col);
                for (int i = 0; i < 7; i++)
                    buf_add_vertical_caret_at_column(&b, -direction, col);
                CHECK(buf_caret_count(&b) == 7 && b.version == version && !b.undo.count &&
                      !strcmp(b.text, source));
                bool found[7] = {0};
                for (int i = 0; i < 7; i++) {
                    BufCaret c = buf_caret(&b, i);
                    size_t r = buf_row(&b, c.cursor);
                    CHECK(r < 7 && !found[r] && c.cursor == buf_position(&b, r, col));
                    CHECK(c.anchor == BUF_NO_SELECTION);
                    found[r] = true;
                }
                CHECK(buf_type(&b, '!') && b.undo.count == 1 && utf8_valid(b.text, b.len));
                CHECK(buf_undo(&b) && !strcmp(b.text, source) && buf_caret_count(&b) == 7);
                buf_free(&b);
                cases++;
            }
    printf("OK %d vertical cursor placement matrices: direction reversal, visual columns, "
           "short/empty/tab/UTF-8 lines, one-undo edits\n",
           cases);
    return true;
}
int main(void) {
    struct {
        const char *name;
        bool (*test)(void);
    } tests[] = {{"vertical cursor series / reverse / simultaneous input", vertical_series},
                 {"vertical visual column / tabs / UTF-8 / undo", vertical_visual_column},
                 {"vertical carets preserve nonoverlapping selections", vertical_selection},
                 {"vertical edges / invalid direction / 256-cursor limit", vertical_bounds},
                 {"vertical placement matrix", vertical_matrix},
                 {"atomic UTF-8 insert / undo / redo", insert_history},
                 {"toggle caret", toggle},
                 {"overlap / adjacent selections", merge_ranges},
                 {"exact UTF-8 occurrences", occurrences},
                 {"256 cursor limit", caret_limit},
                 {"overlapping deletion", overlapping_delete},
                 {"UTF-8 boundaries", utf8_boundaries},
                 {"paired characters / skipping", pairing},
                 {"wrap selections", wrap_selection},
                 {"per-cursor indentation", newline_indents},
                 {"newlines inside pairs", newline_pairs},
                 {"movement / shift selections", navigation},
                 {"per-cursor tab stops", tabs},
                 {"indent / comments preserve carets", transform},
                 {"duplicate rows once", duplicate_lines},
                 {"copy / cut in document order", cut_copy},
                 {"paste one line per cursor", paste},
                 {"atomic size / UTF-8 rejection", atomic_rejection},
                 {"single replace collapses extras", explicit_replace},
                 {"bounded multi-caret history", history_eviction},
                 {"500 random multicursor edits", random_edits},
                 {"save / BOM / undo dirty state", save_multi}};
    int count = (int)(sizeof(tests) / sizeof(*tests)), passed = 0;
    for (int i = 0; i < count; i++) {
        bool ok = tests[i].test();
        printf("%s %s\n", ok ? "OK" : "FAILED", tests[i].name);
        if (ok)
            passed++;
    }
    printf("%d/%d multicursor tests passed\n", passed, count);
    return passed == count ? 0 : 1;
}
