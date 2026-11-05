#define _POSIX_C_SOURCE 200809L
#include "xml_refs.h"
#include "xml_fixture.h"
#include <libxml/parser.h>
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
static void pause_ms(void) {
#ifdef _WIN32
    Sleep(1);
#else
    struct timespec time = {0, 1000000};
    nanosleep(&time, NULL);
#endif
}
static void wait_job(XmlLoadJob *job) {
    for (int i = 0; i < 4000 && !atomic_load(&job->done); i++)
        pause_ms();
    CHECK(atomic_load(&job->done));
}
static void json(const char *text) {
    putchar('"');
    for (const unsigned char *p = (const unsigned char *)text; *p; p++) {
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
static int probe(const char *path, int index) {
    FILE *f = fopen(path, "rb");
    if (!f)
        return 2;
    char *raw = malloc(XML_REF_SOURCE_MAX + 1);
    if (!raw) {
        fclose(f);
        return 2;
    }
    size_t len = fread(raw, 1, XML_REF_SOURCE_MAX + 1, f);
    bool read_ok = !ferror(f);
    fclose(f);
    char error[512] = {0};
    XmlReferences refs = {0};
    bool ok = read_ok && xml_refs_parse(&refs, raw, len, NULL, error, sizeof(error));
    free(raw);
    if (!ok) {
        fprintf(stderr, "%s\n", error);
        return 1;
    }
    char *snippet = refs.count ? xml_refs_snippet(&refs, index, error, sizeof(error)) : NULL;
    if (refs.count && !snippet) {
        fprintf(stderr, "%s\n", error);
        xml_refs_free(&refs);
        return 1;
    }
    printf("{\"count\":%d,\"items\":[", refs.count);
    for (int i = 0; i < refs.count; i++) {
        XmlIdentifier *item = &refs.items[i];
        if (i)
            putchar(',');
        printf("{\"id\":");
        json(item->id);
        printf(",\"tag\":");
        json(item->tag);
        printf(",\"model\":");
        json(item->model);
        printf(",\"line\":%ld}", item->line);
    }
    printf("],\"snippet\":");
    if (snippet)
        json(snippet);
    else
        printf("null");
    printf("}\n");
    free(snippet);
    xml_refs_free(&refs);
    return 0;
}
static void expect_invalid(const char *raw, size_t size) {
    XmlReferences refs = {0};
    char error[512] = {0};
    CHECK(!xml_refs_parse(&refs, raw, size, NULL, error, sizeof(error)));
    CHECK(*error && !refs.raw && !refs.items);
}
static void paths(void) {
    XmlPathSpan span;
    const char *path = "odoo/addons/sale/views/sale_views.xml";
    CHECK(xml_path_span(path, strlen(path), 12, BUF_NO_SELECTION, &span));
    CHECK(span.start == 0 && span.end == strlen(path) && !strcmp(span.path, path));
    CHECK(xml_path_span(path, strlen(path), strlen(path), BUF_NO_SELECTION, &span));
    const char *quoted = "é\n    \"dossier été/sale views.XML\" tail";
    CHECK(xml_path_span(quoted, strlen(quoted), strlen("é\n    \"dossier"), BUF_NO_SELECTION, &span));
    CHECK(!strcmp(span.path, "dossier été/sale views.XML") && quoted[span.start] == '"' &&
          quoted[span.end - 1] == '"');
    CHECK(xml_path_span(quoted, strlen(quoted), span.end, span.start, &span));
    const char *selected = "  path with spaces.xml  ";
    CHECK(xml_path_span(selected, strlen(selected), 0, strlen(selected), &span));
    CHECK(!strcmp(span.path, "path with spaces.xml"));
    const char *bad[] = {"file.py", "https://example.test/a.xml", "name.xml\nx.xml", "a\0b.xml"};
    for (int i = 0; i < 3; i++)
        CHECK(!xml_path_span(bad[i], strlen(bad[i]), strlen(bad[i]), 0, &span));
    CHECK(!xml_path_span(bad[3], 7, 7, 0, &span));
    CHECK(!xml_path_span(path, strlen(path), strlen(path) + 1, BUF_NO_SELECTION, &span));
}
static void parse_and_serialize(void) {
    XmlReferences refs = {0};
    char error[512] = {0};
    CHECK(xml_refs_parse(&refs, xml_fixture, strlen(xml_fixture), NULL, error, sizeof(error)));
    CHECK(refs.count == 6 && !strcmp(refs.items[0].id, "view_order_form") && refs.items[0].line == 4);
    CHECK(!strcmp(refs.items[1].id, "inside_arch") && !strcmp(refs.items[3].tag, "template"));
    CHECK(!strcmp(refs.items[0].model, "ir.ui.view"));
    char *snippet = xml_refs_snippet(&refs, 0, error, sizeof(error));
    CHECK(snippet);
    CHECK(strstr(snippet, "<record") && strstr(snippet, "id=\"view_order_form\"") &&
          strstr(snippet, "priority=\"16\""));
    CHECK(strstr(snippet, "<field name=\"name\">sale.order.form</field>") &&
          strstr(snippet, "<field name=\"model\">sale.order</field>"));
    CHECK(strstr(snippet, "<field name=\"inherit_id\" ref=\"sale.base\"/>") &&
          strstr(snippet, "<field name=\"arch\" type=\"xml\"></field>"));
    CHECK(!strstr(snippet, "partner_id") && !strstr(snippet, "forbidden") && !strstr(snippet, "<?xml"));
    xmlDocPtr doc = xmlReadMemory(snippet, (int)strlen(snippet), NULL, "UTF-8", XML_PARSE_NONET);
    CHECK(doc);
    xmlFreeDoc(doc);
    free(snippet);
    snippet = xml_refs_snippet(&refs, 3, error, sizeof(error));
    CHECK(snippet);
    CHECK(strstr(snippet, "xmlns:q=\"urn:q\"") && strstr(snippet, "Hello &lt;world&gt; <b>bold</b> tail"));
    free(snippet);
    snippet = xml_refs_snippet(&refs, 5, error, sizeof(error));
    CHECK(snippet && strstr(snippet, "model=\"two\"") && strstr(snippet, "second") &&
          !strstr(snippet, "first"));
    free(snippet);
    CHECK(!xml_refs_snippet(&refs, -1, error, sizeof(error)) &&
          !xml_refs_snippet(&refs, 6, error, sizeof(error)));
    xml_refs_free(&refs);
    const char *mixed = "<odoo xml:space=\"preserve\"><r id=\"x\">\n<!-- literal <!DOCTYPE fake> --><?note "
                        "ok?><f>A\nB&#13;C<![CDATA[<z>]]></f>\n</r></odoo>";
    CHECK(xml_refs_parse(&refs, mixed, strlen(mixed), NULL, error, sizeof(error)));
    snippet = xml_refs_snippet(&refs, 0, error, sizeof(error));
    CHECK(snippet);
    CHECK(strstr(snippet, "xml:space=\"preserve\"") && strstr(snippet, "&#10;") && strstr(snippet, "&#13;") &&
          strstr(snippet, "<?note ok?>"));
    doc = xmlReadMemory(snippet, (int)strlen(snippet), NULL, "UTF-8", XML_PARSE_NONET);
    CHECK(doc);
    xmlFreeDoc(doc);
    free(snippet);
    xml_refs_free(&refs);
    const char *nested =
        "<r id=\"outer\"><x><field name=\"arch\" type=\"xml\"/></x><record id=\"nested\"><field "
        "name=\"arch\"><x/></field></record><field name=\"name\">keep</field></r>";
    CHECK(xml_refs_parse(&refs, nested, strlen(nested), NULL, error, sizeof(error)));
    snippet = xml_refs_snippet(&refs, 0, error, sizeof(error));
    CHECK(snippet && strstr(snippet, "<field name=\"arch\" type=\"xml\"></field>") &&
          strstr(snippet, "<field name=\"arch\"></field>") && strstr(snippet, "keep"));
    free(snippet);
    xml_refs_free(&refs);
    const char *bom = "\xef\xbb\xbf<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?><r id=\"été\"/>";
    CHECK(xml_refs_parse(&refs, bom + 3, strlen(bom) - 3, NULL, error, sizeof(error)) &&
          !strcmp(refs.items[0].id, "été"));
    snippet = xml_refs_snippet(&refs, 0, error, sizeof(error));
    CHECK(snippet && strstr(snippet, "été"));
    free(snippet);
    xml_refs_free(&refs);
    CHECK(xml_refs_parse(&refs, bom, strlen(bom), NULL, error, sizeof(error)) &&
          !strcmp(refs.items[0].id, "été"));
    snippet = xml_refs_snippet(&refs, 0, error, sizeof(error));
    CHECK(snippet && strstr(snippet, "été"));
    free(snippet);
    xml_refs_free(&refs);
    CHECK(xml_refs_parse(&refs, "<r/>", 4, NULL, error, sizeof(error)) && !refs.count);
    xml_refs_free(&refs);
}
static void limits_and_security(void) {
    const char *bad[] = {"",
                         "<r>",
                         "<r id=\"x\" id=\"y\"/>",
                         "<q:r id=\"x\"/>",
                         "<r>&unknown;</r>",
                         "<!DOCTYPE r [<!ENTITY e \"secret\">]><r>&e;</r>",
                         "<!DOCTYPE r SYSTEM \"file:///does-not-exist\"><r/>",
                         "<!DOCTYPE r [<!ENTITY % e SYSTEM \"https://invalid.test/a\">%e;]><r/>",
                         "<r>\xff</r>"};
    for (unsigned i = 0; i < sizeof(bad) / sizeof(*bad); i++)
        expect_invalid(bad[i], strlen(bad[i]));
    expect_invalid("<r>\0</r>", 8);
    char *raw = malloc(XML_REF_SOURCE_MAX + 2);
    CHECK(raw);
    memset(raw, 'x', XML_REF_SOURCE_MAX + 1);
    expect_invalid(raw, XML_REF_SOURCE_MAX + 1);
    size_t used = 0;
    used += (size_t)sprintf(raw, "<r>");
    for (int i = 0; i < XML_REF_IDS_MAX; i++)
        used += (size_t)sprintf(raw + used, "<x id=\"%d\"/>", i);
    used += (size_t)sprintf(raw + used, "</r>");
    XmlReferences refs = {0};
    char error[512] = {0};
    CHECK(xml_refs_parse(&refs, raw, used, NULL, error, sizeof(error)) && refs.count == XML_REF_IDS_MAX);
    xml_refs_free(&refs);
    used -= 4;
    used += (size_t)sprintf(raw + used, "<x id=\"overflow\"/></r>");
    expect_invalid(raw, used);
    used = 0;
    for (int i = 0; i < XML_REF_DEPTH_MAX; i++) {
        memcpy(raw + used, "<r>", 3);
        used += 3;
    }
    for (int i = 0; i < XML_REF_DEPTH_MAX; i++) {
        memcpy(raw + used, "</r>", 4);
        used += 4;
    }
    CHECK(xml_refs_parse(&refs, raw, used, NULL, error, sizeof(error)));
    xml_refs_free(&refs);
    memmove(raw + 3, raw, used);
    memcpy(raw, "<r>", 3);
    used += 3;
    memcpy(raw + used, "</r>", 4);
    used += 4;
    expect_invalid(raw, used);
    used = (size_t)sprintf(raw, "<r>");
    for (int i = 0; i < XML_REF_NODES_MAX; i++) {
        memcpy(raw + used, "<!---->", 7);
        used += 7;
    }
    used += (size_t)sprintf(raw + used, "</r>");
    expect_invalid(raw, used);
    used = (size_t)sprintf(raw, "<r id=\"");
    memset(raw + used, 'a', 1024);
    used += 1024;
    used += (size_t)sprintf(raw + used, "\"/>");
    expect_invalid(raw, used);
    used = (size_t)sprintf(raw, "<r id=\"x\"><field>");
    memset(raw + used, 'a', XML_REF_OUTPUT_MAX);
    used += XML_REF_OUTPUT_MAX;
    used += (size_t)sprintf(raw + used, "</field></r>");
    CHECK(xml_refs_parse(&refs, raw, used, NULL, error, sizeof(error)));
    CHECK(!xml_refs_snippet(&refs, 0, error, sizeof(error)));
    xml_refs_free(&refs);
    free(raw);
    atomic_bool cancel;
    atomic_init(&cancel, true);
    CHECK(!xml_refs_parse(&refs, xml_fixture, strlen(xml_fixture), &cancel, error, sizeof(error)) &&
          !refs.raw);
}
typedef struct {
    atomic_bool entered, release;
    bool block, deny_after;
    int calls;
} Guard;
static bool guard(void *context, const char *path, bool leaf, char *error, size_t cap) {
    Guard *g = context;
    (void)path;
    (void)leaf;
    g->calls++;
    atomic_store(&g->entered, true);
    while (g->block && !atomic_load(&g->release))
        pause_ms();
    if (g->deny_after && g->calls > 1) {
        snprintf(error, cap, "generation changed");
        return false;
    }
    return true;
}
static void workers(void) {
    char directory[BUF_PATH];
#ifdef _WIN32
    char temp[MAX_PATH];
    CHECK(GetTempPathA(MAX_PATH, temp) && GetTempFileNameA(temp, "xml", 0, directory));
    CHECK(!remove(directory) && !_mkdir(directory));
#else
    strcpy(directory, "/tmp/python-studio-xml-XXXXXX");
    CHECK(mkdtemp(directory));
#endif
    char paths[2][BUF_PATH], error[512] = {0};
    CHECK(snprintf(paths[0], BUF_PATH, "%s/missing.xml", directory) < BUF_PATH);
    CHECK(snprintf(paths[1], BUF_PATH, "%s/source été.xml", directory) < BUF_PATH);
    FILE *file = fopen(paths[1], "wb");
    CHECK(file);
    CHECK(fwrite(xml_fixture, 1, strlen(xml_fixture), file) == strlen(xml_fixture));
    CHECK(!fclose(file));
    XmlLoadJob *job = xml_job_start((const char (*)[BUF_PATH])paths, 2, NULL, NULL, error, sizeof(error));
    CHECK(job);
    wait_job(job);
    CHECK(job->success && job->references.count == 6 && !strcmp(job->references.path, paths[1]));
    CHECK(!remove(paths[1]));
    char *snippet = xml_refs_snippet(&job->references, 0, error, sizeof(error));
    CHECK(snippet && strstr(snippet, "sale.order.form"));
    free(snippet);
    xml_job_free(job);
    file = fopen(paths[1], "wb");
    CHECK(file);
    CHECK(fwrite(xml_fixture, 1, strlen(xml_fixture), file) == strlen(xml_fixture));
    CHECK(!fclose(file));
    Guard g = {0};
    atomic_init(&g.entered, false);
    atomic_init(&g.release, false);
    g.deny_after = true;
    job = xml_job_start((const char (*)[BUF_PATH]) & paths[1], 1, guard, &g, error, sizeof(error));
    CHECK(job);
    wait_job(job);
    CHECK(!job->success && strstr(job->error, "generation changed") && g.calls == 2);
    xml_job_free(job);
    g.deny_after = false;
    g.block = true;
    g.calls = 0;
    atomic_store(&g.entered, false);
    job = xml_job_start((const char (*)[BUF_PATH]) & paths[1], 1, guard, &g, error, sizeof(error));
    CHECK(job);
    for (int i = 0; i < 2000 && !atomic_load(&g.entered); i++)
        pause_ms();
    CHECK(atomic_load(&g.entered));
    xml_job_cancel(job);
    CHECK(!atomic_load(&job->done));
    atomic_store(&g.release, true);
    wait_job(job);
    CHECK(!job->success);
    xml_job_free(job);
#ifndef _WIN32
    char fifo[1][BUF_PATH];
    CHECK(snprintf(fifo[0], BUF_PATH, "%s/fifo.xml", directory) < BUF_PATH);
    CHECK(!mkfifo(fifo[0], 0600));
    job = xml_job_start((const char (*)[BUF_PATH])fifo, 1, NULL, NULL, error, sizeof(error));
    CHECK(job);
    wait_job(job);
    CHECK(!job->success && strstr(job->error, "régulier"));
    xml_job_free(job);
    CHECK(!unlink(fifo[0]));
#endif
    CHECK(!remove(paths[1]));
#ifdef _WIN32
    CHECK(!_rmdir(directory));
#else
    CHECK(!rmdir(directory));
#endif
}
int main(int argc, char **argv) {
    if (argc >= 3 && !strcmp(argv[1], "--probe"))
        return probe(argv[2], argc > 3 ? atoi(argv[3]) : 0);
    paths();
    parse_and_serialize();
    limits_and_security();
    workers();
    printf(
        "OK %d XML core/worker assertions: UTF-8 paths, every ID, duplicates, fields/empty arch, namespaces, "
        "mixed content, DTD/network rejection, limits, cancellation, immutable read snapshot; no GUI\n",
        checks);
    return 0;
}
