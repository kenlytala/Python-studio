#ifndef PRISME_LINTER_H
#define PRISME_LINTER_H
#include "buffer.h"
#include "import_paths.h"
#include "runner.h"
#include "lint_rules.h"
#include "odoo_config.h"
#include <stdbool.h>
#include <stddef.h>

#define LINT_MAX_ITEMS 256
#define LINT_OUTPUT_LIMIT (512u * 1024u)
#define LINT_DEBOUNCE 1.0
#define LINT_TIMEOUT 15.0

typedef enum { LINT_ERROR, LINT_WARNING, LINT_INFO } LintSeverity;
typedef enum {
    LINT_BACKEND_NONE,
    LINT_BACKEND_SYNTAX,
    LINT_BACKEND_PYLINT,
    LINT_BACKEND_ERROR,
    LINT_BACKEND_XML,
    LINT_BACKEND_ODOO_XML
} LintBackend;
typedef struct {
    LintSeverity severity;
    size_t row, column, end_row, end_column;             /* rows 0-based; columns UTF-8 bytes */
    size_t start, end, visual_column, end_visual_column; /* resolved once per report */
    char code[12], symbol[96], message[768];
} Diagnostic;
typedef struct {
    Diagnostic items[LINT_MAX_ITEMS];
    int count, total, errors, warnings, infos;
    LintBackend backend;
    bool resolved;
    unsigned long revision; /* accepted result identity, not the document version */
    char version[64], status[512];
} LintReport;
typedef struct {
    unsigned long observed_version, analyzed_version;
    double changed_at;
    bool observed, valid, waiting;
    char filename[BUF_PATH], project[BUF_PATH];
    LintReport report;
} LintDocument;
typedef struct {
    Runner process;
    LintRules disabled;
    OdooConfig odoo;
    ImportPaths imports; /* Immutable snapshot at lint_start; caller invalidates caches on change. */
    LintDocument *owner;
    unsigned long version, report_sequence;
    char filename[BUF_PATH], project[BUF_PATH];
    char *output;
    size_t used;
    bool discard, failed;
    char error[512];
} Linter;

void lint_init(Linter *engine);
bool lint_set_odoo(Linter *engine, const OdooConfig *config, char *error, size_t cap);
/* Changes cancel/discard an old in-flight report; invalidate other document caches in the caller. */
bool lint_set_disabled(Linter *engine, const char *value, char *error, size_t cap);
void lint_observe(LintDocument *doc, unsigned long version, const char *filename, const char *project,
                  double now);
bool lint_ready(const LintDocument *doc, double now);
bool lint_start(Linter *engine, LintDocument *doc, const char *python, const char *source, char *error,
                size_t cap);
void lint_poll(Linter *engine, double now);
/* Invalidate/cancel before freeing a closed tab's document. No blocking wait. */
void lint_forget(Linter *engine, LintDocument *doc);
void lint_close(Linter *engine);
bool lint_parse(LintReport *out, const char *protocol);
/* Validated byte offsets, aligned to UTF-8; end is exclusive and never < start. */
void lint_range(Buffer *b, const Diagnostic *diagnostic, size_t *start, size_t *end);
void lint_resolve(LintReport *report, Buffer *b);
#endif
