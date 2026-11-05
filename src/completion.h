#ifndef PRISME_COMPLETION_H
#define PRISME_COMPLETION_H
#include "buffer.h"
#include "import_paths.h"
#include "runner.h"
#include <stdio.h>

#define COMPLETION_MAX 32
#define COMPLETION_OUTPUT_MAX 65536u
#define COMPLETION_DEBOUNCE 0.18
#define COMPLETION_TIMEOUT 15.0

typedef struct {
    char name[96], detail[256];
    char kind; /* W word, S snippet, C class, F callable, M module, O Odoo model, V value. */
    int snippet;
    size_t start, end;
} CompletionItem;
typedef struct {
    unsigned long sequence;
    int count;
    bool failed;
    char status[384];
    size_t start, end;
    CompletionItem items[COMPLETION_MAX];
} CompletionReport;
typedef struct {
    Runner process;
    FILE *request;
    char request_path[BUF_PATH], project[BUF_PATH], python[BUF_PATH], filename[BUF_PATH];
    ImportPaths paths;
    unsigned long sequence, version;
    size_t cursor, used;
    bool awaiting, ready;
    double sent_at, retry_after;
    char output[COMPLETION_OUTPUT_MAX + 1u];
    CompletionReport report;
} Completer;

void completion_init(Completer *service);
/* Nonblocking cancellation; call poll until the old worker has exited. */
void completion_reset(Completer *service);
void completion_poll(Completer *service, double now);
void completion_close(Completer *service);
bool completion_parse(CompletionReport *out, const char *protocol);
bool completion_submit(Completer *service, const char *python, const char *project, const ImportPaths *paths,
                       const char *filename, const char *source, unsigned long version, size_t cursor,
                       char *error, size_t cap);
/* Word span at cursor, including the suffix to avoid duplicate text mid-word. */
void completion_span(const Buffer *document, size_t *start, size_t *end);
#endif
