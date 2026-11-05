#ifndef PYTHON_STUDIO_XML_REFS_H
#define PYTHON_STUDIO_XML_REFS_H
#include "buffer.h"
#include "files.h"
#include <stdatomic.h>

#define XML_REF_SOURCE_MAX (8u * 1024u * 1024u)
#define XML_REF_OUTPUT_MAX (256u * 1024u)
#define XML_REF_IDS_MAX 4096
#define XML_REF_ROOTS_MAX 18 /* 8 XML + project + document + 8 Python imports. */
#define XML_REF_DEPTH_MAX 128
#define XML_REF_NODES_MAX 100000

typedef struct {
    size_t start, end; /* UTF-8 bytes; quotes included, outside whitespace excluded. */
    char path[BUF_PATH];
} XmlPathSpan;
typedef struct {
    char id[1024], tag[128], model[1024];
    unsigned ordinal;
    long line;
} XmlIdentifier;
typedef struct {
    char *raw;
    size_t len;
    XmlIdentifier *items;
    int count;
    char path[BUF_PATH];
} XmlReferences;

bool xml_path_span(const char *text, size_t len, size_t cursor, size_t anchor, XmlPathSpan *out);
/* Input is copied. No DTD, external resource, network, framework import or XML evaluation. */
bool xml_refs_parse(XmlReferences *out, const char *raw, size_t len, const atomic_bool *cancel, char *error,
                    size_t cap);
void xml_refs_free(XmlReferences *refs);
/* The caller owns the returned standalone XML fragment (no XML declaration). */
char *xml_refs_snippet(const XmlReferences *refs, int index, char *error, size_t cap);

typedef struct {
    atomic_bool done, cancel;
    bool success;
    int path_count;
    char paths[XML_REF_ROOTS_MAX][BUF_PATH], error[512];
    XmlReferences references;
    FsGuard guard;
    void *guard_context;
    void *thread;
} XmlLoadJob;
/* Candidate paths and guard context must remain valid until xml_job_free joins. */
XmlLoadJob *xml_job_start(const char paths[][BUF_PATH], int count, FsGuard guard, void *context, char *error,
                          size_t cap);
void xml_job_cancel(XmlLoadJob *job); /* Nonblocking, even if a filesystem read is stuck. */
void xml_job_free(XmlLoadJob *job);   /* Joins: only call on done jobs or during shutdown. */
#endif
