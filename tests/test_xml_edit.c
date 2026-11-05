#define _POSIX_C_SOURCE 200809L
#include "xml_edit.h"
#include "xml_paths.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
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
static char *copy(const char *s) {
    char *v = malloc(strlen(s) + 1);
    CHECK(v);
    strcpy(v, s);
    return v;
}
static Buffer *document(const char *marked) {
    Buffer *b = calloc(1, sizeof(*b));
    CHECK(b);
    char *text = copy(marked), *cursor = strchr(text, '|');
    CHECK(cursor);
    size_t p = (size_t)(cursor - text);
    memmove(cursor, cursor + 1, strlen(cursor));
    CHECK(buf_init(b, text, "view.xml"));
    b->cursor = p;
    free(text);
    return b;
}
static void equals(const Buffer *b, const char *marked) {
    char *s = copy(marked), *cursor = strchr(s, '|');
    CHECK(cursor);
    size_t p = (size_t)(cursor - s);
    memmove(cursor, cursor + 1, strlen(cursor));
    CHECK(!strcmp(b->text, s));
    CHECK(b->cursor == p);
    CHECK(b->anchor == BUF_NO_SELECTION);
    free(s);
}
static void run(const char *before, unsigned action, const char *after) {
    Buffer *b = document(before);
    if (action == '\n')
        buf_newline(b);
    else
        CHECK(buf_type(b, action));
    equals(b, after);
    CHECK(b->undo.count <= 1);
    if (b->undo.count) {
        CHECK(buf_dirty(b));
        CHECK(buf_undo(b));
        equals(b, before);
        CHECK(!buf_dirty(b));
        CHECK(buf_redo(b));
        equals(b, after);
        CHECK(buf_dirty(b));
    }
    buf_free(b);
    free(b);
}
static void json_string(const char *s) {
    putchar('"');
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
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
static int probe(int argc, char **argv) {
    if (argc < 5)
        return 2;
    FILE *f = fopen(argv[2], "rb");
    if (!f)
        return 2;
    char *raw = malloc(BUF_LIMIT + 2);
    if (!raw) {
        fclose(f);
        return 2;
    }
    size_t n = fread(raw, 1, BUF_LIMIT + 1, f);
    bool ok = !ferror(f) && n <= BUF_LIMIT;
    fclose(f);
    if (!ok || memchr(raw, 0, n)) {
        free(raw);
        return 2;
    }
    raw[n] = 0;
    Buffer *b = calloc(1, sizeof(*b));
    if (!b) {
        free(raw);
        return 2;
    }
    if (!buf_init(b, raw, "view.xml")) {
        free(raw);
        free(b);
        return 2;
    }
    free(raw);
    char *end;
    errno = 0;
    unsigned long long value = strtoull(argv[3], &end, 10);
    if (errno || *end || value > b->len) {
        buf_free(b);
        free(b);
        return 2;
    }
    b->cursor = (size_t)value;
    if (!strcmp(argv[4], "list")) {
        CompletionItem items[COMPLETION_MAX];
        int count = xml_tag_completions(b->text, b->len, b->cursor, items, COMPLETION_MAX);
        printf("{\"items\":");
        if (count < 0)
            printf("null");
        else {
            putchar('[');
            for (int i = 0; i < count; i++) {
                if (i)
                    putchar(',');
                printf("{\"name\":");
                json_string(items[i].name);
                printf(",\"kind\":\"%c\",\"start\":%zu,\"end\":%zu}", items[i].kind, items[i].start,
                       items[i].end);
            }
            putchar(']');
        }
        puts("}");
        buf_free(b);
        free(b);
        return 0;
    }
    char *original = copy(b->text);
    size_t before = b->cursor;
    if (!strcmp(argv[4], "type") && argc == 6 && strlen(argv[5]) == 1)
        ok = buf_type(b, (unsigned char)argv[5][0]);
    else if (!strcmp(argv[4], "newline") && argc == 5) {
        buf_newline(b);
        ok = true;
    } else if (!strcmp(argv[4], "complete") && argc == 6) {
        XmlEdit edit = {0};
        int status = xml_edit_completion(b->text, b->len, b->cursor, argv[5], &edit);
        ok = status > 0 && xml_edit_apply(b, &edit);
        free(edit.text);
    } else {
        free(original);
        buf_free(b);
        free(b);
        return 2;
    }
    char *after = copy(b->text);
    size_t caret = b->cursor;
    int undo = b->undo.count;
    bool undo_ok = true, redo_ok = true;
    if (undo) {
        undo_ok = buf_undo(b) && !strcmp(original, b->text) && b->cursor == before;
        redo_ok = buf_redo(b) && !strcmp(after, b->text) && b->cursor == caret;
    }
    printf("{\"ok\":%s,\"text\":", ok ? "true" : "false");
    json_string(after);
    printf(",\"cursor\":%zu,\"undo\":%d,\"undo_restores\":%s,\"redo_restores\":%s}\n", caret, undo,
           undo_ok ? "true" : "false", redo_ok ? "true" : "false");
    free(original);
    free(after);
    buf_free(b);
    free(b);
    return 0;
}
static void paths(void) {
    char directory[BUF_PATH];
#ifdef _WIN32
    char temp[MAX_PATH];
    CHECK(GetTempPathA(MAX_PATH, temp) && GetTempFileNameA(temp, "xpd", 0, directory));
    CHECK(!remove(directory) && !_mkdir(directory));
#else
    strcpy(directory, "/tmp/python-studio-xml-paths-XXXXXX");
    CHECK(mkdtemp(directory));
#endif
    char settings[BUF_PATH], file[BUF_PATH], imports[BUF_PATH], other[BUF_PATH], error[512];
    CHECK(snprintf(settings, sizeof(settings), "%s/settings.cfg", directory) < BUF_PATH);
    CHECK(xml_paths_file(file, sizeof(file), settings, directory));
    CHECK(import_paths_file(imports, sizeof(imports), settings, directory));
    CHECK(strcmp(file, imports));
    CHECK(xml_paths_file(other, sizeof(other), settings, "/other-project"));
    CHECK(strcmp(file, other));
    XmlPaths list = {0}, loaded = {0};
    CHECK(!xml_paths_load(&loaded, file, directory, error, sizeof(error)) && !*error);
    CHECK(xml_paths_add(&list, "/sdk été", error, sizeof(error)));
    CHECK(xml_paths_add(&list, "/offline/server", error, sizeof(error)));
    CHECK(!xml_paths_add(&list, "/sdk été", error, sizeof(error)) && list.count == 2);
    CHECK(!xml_paths_add(&list, "relative", error, sizeof(error)));
    CHECK(xml_paths_move(&list, 1, -1) && !strcmp(list.paths[0], "/offline/server"));
    CHECK(!xml_paths_move(&list, 0, -1) && !xml_paths_move(&list, 0, 2));
    CHECK(xml_paths_save(&list, file, directory, error, sizeof(error)));
    CHECK(xml_paths_load(&loaded, file, directory, error, sizeof(error)) && loaded.count == 2);
    CHECK(!memcmp(&list, &loaded, sizeof(list)));
    CHECK(!xml_paths_load(&loaded, file, "/other-project", error, sizeof(error)) && *error);
    CHECK(loaded.count == 2 && !strcmp(loaded.paths[0], "/offline/server"));
    CHECK(xml_paths_remove(&list, 0) && list.count == 1 && !strcmp(list.paths[0], "/sdk été"));
    for (int i = 1; i < XML_PATHS_MAX; i++) {
        char path[64];
        snprintf(path, sizeof(path), "/sdk/%d", i);
        CHECK(xml_paths_add(&list, path, error, sizeof(error)));
    }
    CHECK(!xml_paths_add(&list, "/ninth", error, sizeof(error)) && list.count == XML_PATHS_MAX);
    char bad[BUF_PATH];
    CHECK(snprintf(bad, sizeof(bad), "%s/missing/profile.cfg", directory) < BUF_PATH);
    CHECK(!xml_paths_save(&list, bad, directory, error, sizeof(error)) && *error);
    CHECK(xml_paths_load(&loaded, file, directory, error, sizeof(error)) && loaded.count == 2);
    FILE *f = fopen(file, "wb");
    CHECK(f);
    CHECK(fputs("broken\n", f) >= 0);
    CHECK(!fclose(f));
    CHECK(!xml_paths_load(&loaded, file, directory, error, sizeof(error)) && loaded.count == 2);
    CHECK(!remove(file));
#ifdef _WIN32
    CHECK(!_rmdir(directory));
#else
    CHECK(!rmdir(directory));
#endif
}
int main(int argc, char **argv) {
    if (argc > 1 && !strcmp(argv[1], "--probe"))
        return probe(argc, argv);
    run("<record id=\"x\"|", '>', "<record id=\"x\">|</record>");
    run("<r xmlns:q=\"urn:q\"><q:été|</r>", '>', "<r xmlns:q=\"urn:q\"><q:été>|</q:été></r>");
    run("<猫|", '>', "<猫>|</猫>");
    run("<field name=\"x\"|</field>", '>', "<field name=\"x\">|</field>");
    run("<a| \n</a>", '>', "<a>| \n</a>");
    run("<a|></a>", '>', "<a>|</a>");
    run("<a /|", '>', "<a />|");
    run("<a></a|", '>', "<a></a>|");
    run("<r><form><|</r>", '/', "<r><form></form>|</r>");
    run("<r><a/><|", '/', "<r><a/></r>|");
    run("<r><|</r>", '/', "<r></r>|");
    run("<r><| \n</r>", '/', "<r> \n</r>|");
    run("<|", '/', "</|");
    run("<r url=\"https:|\"/>", '/', "<r url=\"https:/|\"/>");
    run("<r><!-- <x| --></r>", '>', "<r><!-- <x>| --></r>");
    run("<r><![CDATA[<x|]]></r>", '>', "<r><![CDATA[<x>|]]></r>");
    run("<r><?note <x|?></r>", '>', "<r><?note <x>|?></r>");
    run("<!DOCTYPE r [<!ENTITY x \"<a|\">]><r/>", '>', "<!DOCTYPE r [<!ENTITY x \"<a>|\">]><r/>");
    run("<r a=\"x|\"/>", '>', "<r a=\"x>|\"/>");
    run("<a></wrong><x|", '>', "<a></wrong><x>|");
    run("<a <b|", '>', "<a <b>|");
    run("<3bad|", '>', "<3bad>|");
    run("    <form>|</form>", '\n', "    <form>\n        |\n    </form>");
    run("\t<form>|</form>", '\n', "\t<form>\n\t\t|\n\t</form>");
    run("  <r>texte:|</r>", '\n', "  <r>texte:\n  |</r>");
    run("  <a/>|", '\n', "  <a/>\n  |");
    Buffer *b = document("<a|");
    b->xml_auto_disabled = true;
    CHECK(buf_type(b, '>'));
    equals(b, "<a>|");
    buf_free(b);
    free(b);
    b = document("<a|");
    snprintf(b->name, sizeof(b->name), "main.py");
    CHECK(buf_type(b, '>'));
    equals(b, "<a>|");
    buf_free(b);
    free(b);
    b = document("|");
    CHECK(buf_paste(b, "<r><x/></r>"));
    equals(b, "<r><x/></r>|");
    CHECK(b->undo.count == 1);
    CHECK(buf_undo(b));
    equals(b, "|");
    CHECK(buf_redo(b));
    equals(b, "<r><x/></r>|");
    buf_free(b);
    free(b);
    b = document("<r><a>|</a><b></b></r>");
    CHECK(buf_add_caret(b, 13, BUF_NO_SELECTION));
    BufCaret primary = buf_caret(b, 0), secondary = buf_caret(b, 1);
    buf_newline(b);
    CHECK(!strcmp(b->text, "<r><a>\n    \n</a><b>\n    \n</b></r>") && buf_caret_count(b) == 2 &&
          b->undo.count == 1);
    CHECK(buf_undo(b) && !strcmp(b->text, "<r><a></a><b></b></r>"));
    CHECK(b->cursor == primary.cursor && b->extra[0].cursor == secondary.cursor && buf_redo(b));
    buf_free(b);
    free(b);
    b = document("<fi|");
    CompletionItem items[COMPLETION_MAX];
    CHECK(xml_tag_completions(b->text, b->len, b->cursor, items, COMPLETION_MAX) >= 1);
    CHECK(!strcmp(items[0].name, "field") && items[0].kind == 'X');
    XmlEdit edit = {0};
    CHECK(xml_edit_completion(b->text, b->len, b->cursor, "field", &edit) > 0 && xml_edit_apply(b, &edit));
    free(edit.text);
    equals(b, "<field>|</field>");
    CHECK(buf_undo(b));
    equals(b, "<fi|");
    CHECK(buf_redo(b));
    equals(b, "<field>|</field>");
    CHECK(buf_undo(b) && b->redo.count == 1 && buf_insert(b, "x") && !b->redo.count);
    buf_free(b);
    free(b);
    b = document("<r><foo></f|");
    CHECK(xml_tag_completions(b->text, b->len, b->cursor, items, COMPLETION_MAX) == 1);
    CHECK(!strcmp(items[0].name, "foo") && items[0].kind == 'T');
    buf_free(b);
    free(b);
    b = document("<rec| id=\"x>y\"></record>");
    CHECK(xml_edit_completion(b->text, b->len, b->cursor, "record", &edit) > 0 && xml_edit_apply(b, &edit));
    free(edit.text);
    equals(b, "<record id=\"x>y\">|</record>");
    buf_free(b);
    free(b);
    b = document("<fi| name=\"x\"/>");
    CHECK(xml_edit_completion(b->text, b->len, b->cursor, "field", &edit) > 0 && xml_edit_apply(b, &edit));
    free(edit.text);
    equals(b, "<field name=\"x\"/>|");
    buf_free(b);
    free(b);
    b = document("<r a=\"field|");
    CHECK(xml_tag_completions(b->text, b->len, b->cursor, items, COMPLETION_MAX) < 0);
    buf_free(b);
    free(b);
    b = document("猫|");
    XmlEdit invalid = {0, 0, 2, "<"};
    CHECK(!xml_edit_apply(b, &invalid) && !strcmp(b->text, "猫") && !b->undo.count);
    buf_free(b);
    free(b);
    b = document("<a|");
    char *limit = malloc(BUF_LIMIT + 1);
    CHECK(limit);
    memset(limit, ' ', BUF_LIMIT);
    limit[0] = '<';
    limit[1] = 'a';
    limit[BUF_LIMIT] = 0;
    buf_free(b);
    CHECK(buf_init(b, limit, "view.xml"));
    b->cursor = 2;
    CHECK(!buf_type(b, '>') && b->len == BUF_LIMIT && !b->undo.count);
    free(limit);
    buf_free(b);
    free(b);
    size_t budget_length = 4 * (XML_EDIT_EVENTS + 1u);
    char *budget = malloc(budget_length + 4);
    CHECK(budget);
    for (size_t i = 0; i < budget_length; i += 4)
        memcpy(budget + i, "<a/>", 4);
    memcpy(budget + budget_length, "<fi", 4);
    b = calloc(1, sizeof(*b));
    CHECK(b && buf_init(b, budget, "view.xml"));
    b->cursor = b->len;
    CHECK(xml_tag_completions(b->text, b->len, b->cursor, items, COMPLETION_MAX) < 0);
    CHECK(buf_type(b, '>') && b->undo.count == 1 && b->len == budget_length + 4 &&
          b->text[b->len - 1] == '>');
    CHECK(buf_undo(b) && !strcmp(b->text, budget));
    buf_free(b);
    free(b);
    free(budget);
    paths();
    printf("%d XML editing/history/root-profile assertions passed\n", checks);
    return 0;
}
