#define _POSIX_C_SOURCE 200809L
#include "app_internal.h"
#include "xml_fixture.h"
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
static int checks, visible;
#define CHECK(c)                                                                                             \
    do {                                                                                                     \
        checks++;                                                                                            \
        if (!(c)) {                                                                                          \
            fprintf(stderr, "FAIL %d: %s\n", __LINE__, #c);                                                  \
            exit(1);                                                                                         \
        }                                                                                                    \
    } while (0)
bool IsKeyPressed(int key) {
    (void)key;
    return false;
}
bool IsKeyPressedRepeat(int key) {
    (void)key;
    return false;
}
Buffer *app_buffer(App *a) {
    return a->tabs[a->active];
}
void app_toast(App *a, const char *text, bool error) {
    snprintf(a->toast, sizeof(a->toast), "%s", text);
    a->toast_error = error;
}
void app_ensure_visible(App *a) {
    (void)a;
    visible++;
}
void app_completion_dismiss(App *a) {
    a->completion_active = false;
    a->completion_count = 0;
}
void app_multicursor_reset(App *a) {
    (void)a;
}
void app_set_field(Buffer *b, const char *text, bool select) {
    unsigned long version = b->version;
    buf_free(b);
    CHECK(buf_init(b, text, "field"));
    b->version = version + 1;
    b->cursor = b->len;
    if (select)
        b->anchor = 0;
}
void app_edit_field(App *a, Buffer *b, const unsigned *chars, int count, bool ctrl, bool shift) {
    (void)a;
    (void)shift;
    if (!ctrl)
        for (int i = 0; i < count; i++)
            CHECK(buf_type(b, chars[i]));
}
static void pause_ms(void) {
#ifdef _WIN32
    Sleep(1);
#else
    struct timespec time = {0, 1000000};
    nanosleep(&time, NULL);
#endif
}
static void load(App *a) {
    if (a->xml.job) {
        for (int i = 0; i < 4000 && !atomic_load(&a->xml.job->done); i++)
            pause_ms();
        CHECK(atomic_load(&a->xml.job->done));
    }
    app_xml_poll(a);
}
static void document(App *a, Buffer *b, const char *text) {
    app_xml_cancel(a);
    load(a);
    buf_free(b);
    CHECK(buf_init(b, text, "new.xml"));
    a->tabs[0] = b;
    a->active = a->active_pane = 0;
    a->tab_count = 1;
    a->focus = 0;
    a->modal = M_NONE;
    b->cursor = b->len;
    const char *extension = strstr(text, ".xml");
    if (extension)
        b->cursor = (size_t)(extension - text) + 4;
}
static void choose(App *a, const char *query) {
    app_set_field(&a->field, query, false);
    a->xml.filter_version = ~0ul;
    CHECK(app_xml_filter(a) == 1);
    app_xml_choose(a, 0);
}
static void key(App *a, int code) {
    memset(a->keys, 0, sizeof(a->keys));
    a->keys[code] = true;
    app_xml_keys(a, NULL, 0, false, false);
    memset(a->keys, 0, sizeof(a->keys));
}
static void write_source(const char *path) {
    FILE *f = fopen(path, "wb");
    CHECK(f);
    CHECK(fwrite(xml_fixture, 1, strlen(xml_fixture), f) == strlen(xml_fixture));
    CHECK(!fclose(f));
}
int main(void) {
    App *a = calloc(1, sizeof(*a));
    CHECK(a);
    atomic_init(&a->ssh.state, SSH_IDLE);
    atomic_init(&a->ssh.generation, 0);
    atomic_init(&a->ssh.process_alive, false);
    CHECK(buf_init(&a->field, "", "field"));
    char directory[BUF_PATH], source[BUF_PATH], other[BUF_PATH], parent[BUF_PATH];
#ifdef _WIN32
    char temp[MAX_PATH];
    CHECK(GetTempPathA(MAX_PATH, temp) && GetTempFileNameA(temp, "xui", 0, directory));
    CHECK(!remove(directory) && !_mkdir(directory));
#else
    strcpy(directory, "/tmp/python-studio-xml-ui-XXXXXX");
    CHECK(mkdtemp(directory));
#endif
    strcpy(a->project, directory);
    CHECK(snprintf(source, sizeof(source), "%s/sale_views.xml", directory) < BUF_PATH);
    write_source(source);
    Buffer b = {0};
    const char *original = "before\n    sale_views.xml\nafter";
    document(a, &b, original);
    size_t caret = b.cursor;
    unsigned long version = b.version;
    CHECK(app_xml_begin(a, false) && a->modal == M_XML_IDS && a->xml.document == &b && !a->completion_active);
    load(a);
    CHECK(a->xml.count == 6 && !*a->xml.error);
    app_set_field(&a->field, "view_order_form", false);
    CHECK(app_xml_filter(a) == 1);
    key(a, KEY_ENTER);
    CHECK(a->modal == M_NONE && !a->xml.refs.raw && visible && b.version == version + 1 && b.undo.count == 1);
    CHECK(strstr(b.text, "before\n    <record") &&
          strstr(b.text, "\n        <field name=\"name\">sale.order.form</field>") &&
          strstr(b.text, "<field name=\"arch\" type=\"xml\"></field>") &&
          strstr(b.text, "\n    </record>\nafter"));
    CHECK(buf_undo(&b) && !strcmp(b.text, original) && b.cursor == caret);
    CHECK(buf_redo(&b) && strstr(b.text, "sale.order.form"));
    document(a, &b, original);
    CHECK(app_xml_begin(a, false));
    load(a);
    key(a, KEY_ESCAPE);
    CHECK(a->modal == M_NONE && !strcmp(b.text, original) && !b.undo.count);
    /* Keyboard search, arrows, Enter and filtered-row mouse dispatch share one path. */
    CHECK(app_xml_begin(a, false));
    load(a);
    unsigned query[] = {'s', 'a', 'l', 'e', '.', 'm', 'e', 'n', 'u'};
    app_xml_keys(a, query, 9, false, false);
    CHECK(a->xml.count == 1 && !strcmp(a->xml.refs.items[a->xml.filtered[0]].id, "sale.menu"));
    app_xml_choose(a, 0);
    CHECK(strstr(b.text, "<menuitem") && strstr(b.text, "id=\"sale.menu\""));
    for (int kind = 0; kind < 7; kind++) {
        document(a, &b, original);
        CHECK(app_xml_begin(a, false));
        load(a);
        if (kind == 0)
            CHECK(buf_insert(&b, "x"));
        else if (kind == 1)
            b.cursor = 0;
        else if (kind == 2)
            a->active_pane = 1;
        else if (kind == 3)
            strcpy(a->project, "/changed-project");
        else if (kind == 4)
            CHECK(buf_add_caret(&b, 0, BUF_NO_SELECTION));
        else if (kind == 5) {
            a->tabs[0] = NULL;
            a->tab_count = 0;
        } else
            b.anchor = 0;
        char *before = malloc(b.len + 1);
        CHECK(before);
        strcpy(before, b.text);
        app_xml_choose(a, 0);
        CHECK(a->modal == M_NONE && !strcmp(before, b.text) && a->toast_error);
        free(before);
        a->tabs[0] = &b;
        a->tab_count = 1;
        strcpy(a->project, directory);
    }
    document(a, &b, "é\n  \"sale views.xml\" tail");
    CHECK(snprintf(other, sizeof(other), "%s/sale views.xml", directory) < BUF_PATH);
    write_source(other);
    b.anchor = strlen("é\n  ");
    b.cursor = strlen("é\n  \"sale views.xml\"");
    CHECK(app_xml_begin(a, false));
    load(a);
    choose(a, "sale.menu");
    CHECK(!strncmp(b.text, "é\n  <menuitem", strlen("é\n  <menuitem")) && strstr(b.text, "/> tail"));
    CHECK(!remove(other));
    /* Relative lookup also works beside the current document and in import roots. */
    CHECK(!remove(source));
    CHECK(snprintf(parent, sizeof(parent), "%s/document", directory) < BUF_PATH);
#ifdef _WIN32
    CHECK(!_mkdir(parent));
#else
    CHECK(!mkdir(parent, 0700));
#endif
    CHECK(snprintf(other, sizeof(other), "%s/sale_views.xml", parent) < BUF_PATH);
    write_source(other);
    document(a, &b, original);
    CHECK(snprintf(b.path, sizeof(b.path), "%s/new.xml", parent) < BUF_PATH);
    CHECK(app_xml_begin(a, false));
    load(a);
    CHECK(a->xml.refs.raw && !strcmp(a->xml.refs.path, other));
    app_xml_cancel(a);
    document(a, &b, original);
    a->linter.imports.count = 1;
    strcpy(a->linter.imports.paths[0], parent);
    CHECK(app_xml_begin(a, false));
    load(a);
    CHECK(a->xml.refs.count == 6);
    app_xml_cancel(a);
    a->linter.imports.count = 0;
    /* Dedicated roots take priority over project/document/Python import fallbacks. */
    document(a, &b, original);
    a->xml_paths.count = 1;
    strcpy(a->xml_paths.paths[0], parent);
    write_source(source);
    CHECK(app_xml_begin(a, false));
    load(a);
    CHECK(a->xml.refs.raw && !strcmp(a->xml.refs.path, other));
    app_xml_cancel(a);
    CHECK(!remove(source));
    document(a, &b, "last.xml");
    CHECK(snprintf(b.path, sizeof(b.path), "%s/other-document/current.xml", directory) < BUF_PATH);
    a->xml_paths.count = XML_PATHS_MAX;
    a->linter.imports.count = IMPORT_PATHS_MAX;
    for (int i = 0; i < XML_PATHS_MAX; i++)
        CHECK(snprintf(a->xml_paths.paths[i], BUF_PATH, "%s/xml-root-%d", directory, i) < BUF_PATH);
    for (int i = 0; i < IMPORT_PATHS_MAX; i++)
        CHECK(snprintf(a->linter.imports.paths[i], BUF_PATH, "%s/python-root-%d", directory, i) < BUF_PATH);
    strcpy(a->linter.imports.paths[IMPORT_PATHS_MAX - 1], parent);
    char last[BUF_PATH];
    CHECK(snprintf(last, sizeof(last), "%s/last.xml", parent) < BUF_PATH);
    write_source(last);
    CHECK(app_xml_begin(a, false));
    CHECK(a->xml.job && a->xml.job->path_count == XML_REF_ROOTS_MAX);
    load(a);
    CHECK(a->xml.refs.raw && !strcmp(a->xml.refs.path, last));
    choose(a, "view_order_form");
    CHECK(strstr(b.text, "sale.order.form") && b.undo.count == 1);
    CHECK(buf_undo(&b) && !strcmp(b.text, "last.xml") && buf_redo(&b));
    a->xml_paths.count = a->linter.imports.count = 0;
    CHECK(!remove(last));
    /* Extreme indentation cannot amplify an already bounded fragment in memory. */
    char *big = malloc(XML_REF_OUTPUT_MAX + 32);
    CHECK(big);
    memset(big, ' ', XML_REF_OUTPUT_MAX);
    strcpy(big + XML_REF_OUTPUT_MAX, "sale_views.xml");
    write_source(source);
    document(a, &b, big);
    CHECK(app_xml_begin(a, false));
    load(a);
    choose(a, "view_order_form");
    CHECK(!strcmp(b.text, big) && !b.undo.count && a->toast_error);
    free(big);
    /* Offline mount metadata and aliases must not read local empty mount stubs. */
#ifndef _WIN32
    document(a, &b, original);
    a->ssh.initialized = true;
    strcpy(a->ssh.cache, parent);
    strcpy(a->ssh.mountpoint, parent);
    a->linter.imports.count = 1;
    strcpy(a->linter.imports.paths[0], parent);
    CHECK(!remove(source));
    CHECK(app_xml_begin(a, false));
    load(a);
    CHECK(!a->xml.refs.raw && *a->xml.error && !strcmp(b.text, original));
    app_xml_cancel(a);
    char alias[BUF_PATH];
    CHECK(snprintf(alias, sizeof(alias), "%s/alias", directory) < BUF_PATH);
    CHECK(!symlink(parent, alias));
    document(a, &b, original);
    strcpy(a->linter.imports.paths[0], alias);
    CHECK(app_xml_begin(a, false));
    load(a);
    CHECK(!a->xml.refs.raw && *a->xml.error);
    app_xml_cancel(a);
    CHECK(!unlink(alias));
    document(a, &b, original);
    a->linter.imports.count = 0;
    a->xml_paths.count = 1;
    strcpy(a->xml_paths.paths[0], parent);
    CHECK(app_xml_begin(a, false));
    load(a);
    CHECK(!a->xml.refs.raw && *a->xml.error && !strcmp(b.text, original));
    app_xml_cancel(a);
    a->xml_paths.count = 0;
    a->ssh.initialized = false;
    a->linter.imports.count = 0;
#endif
    /* A cancelled/finished request cannot resurrect its picker on the next frame. */
    write_source(source);
    document(a, &b, original);
    CHECK(app_xml_begin(a, false));
    app_xml_cancel(a);
    load(a);
    CHECK(a->modal == M_NONE && !a->xml.refs.raw && !a->xml.job && !a->xml.lease &&
          !strcmp(b.text, original));
    document(a, &b, "xml_form");
    CHECK(!app_xml_begin(a, false) && a->modal == M_NONE);
    CHECK(app_xml_begin(a, true) && a->toast_error);
    app_xml_shutdown(a);
    buf_free(&b);
    buf_free(&a->field);
    free(a);
    CHECK(!remove(source) && !remove(other));
#ifdef _WIN32
    CHECK(!_rmdir(parent) && !_rmdir(directory));
#else
    CHECK(!rmdir(parent) && !rmdir(directory));
#endif
    printf("OK %d actual XML controller assertions: inline path replacement, filter/keys/row dispatch, one "
           "undo, stale context, parent/import roots, limits, offline SSH/alias, cancellation; no GUI\n",
           checks);
    return 0;
}
