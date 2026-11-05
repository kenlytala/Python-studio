#define _POSIX_C_SOURCE 200809L
#include "linter.h"
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "prisme_lint_bridge.inc"

static void copy(char *out, size_t cap, const char *s) {
    if (cap)
        snprintf(out, cap, "%s", s ? s : "");
}
static bool failure(char *error, size_t cap, const char *message) {
    copy(error, cap, message);
    return false;
}
static int hex(unsigned char c) {
    return c >= '0' && c <= '9'   ? c - '0'
           : c >= 'a' && c <= 'f' ? c - 'a' + 10
           : c >= 'A' && c <= 'F' ? c - 'A' + 10
                                  : -1;
}
static bool decode(char *out, size_t cap, const char *s) {
    size_t n = strlen(s);
    if (n % 2 || n / 2 >= cap)
        return false;
    for (size_t i = 0; i < n; i += 2) {
        int a = hex((unsigned char)s[i]), b = hex((unsigned char)s[i + 1]);
        if (a < 0 || b < 0)
            return false;
        unsigned char c = (unsigned char)(a * 16 + b);
        if (c < 32 || c == 127)
            return false;
        out[i / 2] = (char)c;
    }
    out[n / 2] = 0;
    return utf8_valid(out, n / 2);
}
static bool number(const char *s, size_t *value) {
    if (!s || !*s || !isdigit((unsigned char)*s))
        return false;
    errno = 0;
    char *end;
    unsigned long n = strtoul(s, &end, 10);
    if (errno || *end || n > 100000000ul)
        return false;
    *value = (size_t)n;
    return true;
}
static int compare(const void *left, const void *right) {
    const Diagnostic *a = left, *b = right;
    if (a->row != b->row)
        return a->row < b->row ? -1 : 1;
    if (a->column != b->column)
        return a->column < b->column ? -1 : 1;
    if (a->severity != b->severity)
        return a->severity < b->severity ? -1 : 1;
    return strcmp(a->code, b->code);
}
bool lint_parse(LintReport *out, const char *protocol) {
    if (!out || !protocol || strlen(protocol) > LINT_OUTPUT_LIMIT)
        return false;
    LintReport *report = calloc(1, sizeof(*report));
    if (!report)
        return false;
    const char *p = protocol;
    bool header = false, ended = false, ok = true;
    while (*p && ok) {
        const char *next = strchr(p, '\n');
        size_t n = next ? (size_t)(next - p) : strlen(p);
        char line[4096];
        if (!n || n >= sizeof(line) || ended) {
            ok = false;
            break;
        }
        memcpy(line, p, n);
        line[n] = 0;
        char *fields[12] = {line};
        int count = 1;
        for (size_t i = 0; i < n; i++)
            if (line[i] == '\t') {
                line[i] = 0;
                if (count == 12) {
                    ok = false;
                    break;
                }
                fields[count++] = line + i + 1;
            }
        if (!ok)
            break;
        if (!header) {
            ok = count == 5 && !strcmp(fields[0], "PRISME-LINT") && !strcmp(fields[1], "1");
            if (ok) {
                report->backend = !strcmp(fields[2], "pylint")     ? LINT_BACKEND_PYLINT
                                  : !strcmp(fields[2], "syntax")   ? LINT_BACKEND_SYNTAX
                                  : !strcmp(fields[2], "error")    ? LINT_BACKEND_ERROR
                                  : !strcmp(fields[2], "xml")      ? LINT_BACKEND_XML
                                  : !strcmp(fields[2], "odoo-xml") ? LINT_BACKEND_ODOO_XML
                                                                   : LINT_BACKEND_NONE;
                ok = report->backend != LINT_BACKEND_NONE &&
                     decode(report->version, sizeof(report->version), fields[3]) &&
                     decode(report->status, sizeof(report->status), fields[4]);
            }
            header = ok;
        } else if (!strcmp(fields[0], "D")) {
            size_t row, col, end_row, end_col;
            ok = count == 9 && report->count < LINT_MAX_ITEMS && strlen(fields[1]) == 1 &&
                 strchr("EWI", fields[1][0]) && number(fields[2], &row) && row > 0 &&
                 number(fields[3], &col) && number(fields[4], &end_row) && end_row > 0 &&
                 number(fields[5], &end_col) && strlen(fields[6]) == 5 &&
                 isupper((unsigned char)fields[6][0]);
            if (ok)
                for (int i = 1; i < 5; i++)
                    if (!isdigit((unsigned char)fields[6][i]))
                        ok = false;
            if (ok) {
                Diagnostic *d = &report->items[report->count];
                d->severity = fields[1][0] == 'E'   ? LINT_ERROR
                              : fields[1][0] == 'W' ? LINT_WARNING
                                                    : LINT_INFO;
                d->row = row - 1;
                d->column = col;
                d->end_row = end_row - 1;
                d->end_column = end_col;
                copy(d->code, sizeof(d->code), fields[6]);
                ok = decode(d->symbol, sizeof(d->symbol), fields[7]) &&
                     decode(d->message, sizeof(d->message), fields[8]);
                if (ok) {
                    report->count++;
                    if (d->severity == LINT_ERROR)
                        report->errors++;
                    else if (d->severity == LINT_WARNING)
                        report->warnings++;
                    else
                        report->infos++;
                }
            }
        } else if (!strcmp(fields[0], "END")) {
            size_t stored, total;
            ok = count == 3 && number(fields[1], &stored) && number(fields[2], &total) &&
                 stored == (size_t)report->count && total >= stored && total <= INT_MAX;
            if (ok) {
                report->total = (int)total;
                ended = true;
            }
        } else
            ok = false;
        p = next ? next + 1 : p + n;
    }
    ok = ok && header && ended;
    if (ok) {
        qsort(report->items, (size_t)report->count, sizeof(*report->items), compare);
        *out = *report;
    }
    free(report);
    return ok;
}
static size_t encoded_size(size_t n) {
    return ((n + 2) / 3) * 4;
}
static char *encoded(char *out, const char *s) {
    const unsigned char *p = (const unsigned char *)s;
    size_t n = strlen(s);
    const char *digits = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    for (size_t i = 0; i < n; i += 3) {
        unsigned value = (unsigned)p[i] << 16;
        if (i + 1 < n)
            value |= (unsigned)p[i + 1] << 8;
        if (i + 2 < n)
            value |= p[i + 2];
        *out++ = digits[value >> 18];
        *out++ = digits[(value >> 12) & 63];
        *out++ = i + 1 < n ? digits[(value >> 6) & 63] : '=';
        *out++ = i + 2 < n ? digits[value & 63] : '=';
    }
    *out = 0;
    return out;
}
static char *program(const char *source, const LintDocument *doc, const ImportPaths *imports,
                     const LintRules *rules, const OdooConfig *odoo) {
    const char *a = "\n_prisme_analyze(base64.b64decode('";
    const char *b = "').decode('utf-8'),base64.b64decode('";
    const char *c = "').decode('utf-8','surrogateescape'),base64.b64decode('";
    const char *d = "').decode('utf-8','surrogateescape'),[";
    const char *prefix = "base64.b64decode('", *ending = "').decode('utf-8','surrogateescape'),";
    size_t cap = sizeof(lint_bridge) + encoded_size(strlen(odoo->ignored.text)) + 80 +
                 encoded_size(strlen(rules->text)) + strlen("],base64.b64decode('').decode('ascii'))\n") +
                 encoded_size(strlen(source)) + encoded_size(strlen(doc->filename)) +
                 encoded_size(strlen(doc->project)) + strlen(a) + strlen(b) + strlen(c) + strlen(d) + 5;
    for (int i = 0; i < imports->count; i++)
        cap += encoded_size(strlen(imports->paths[i])) + strlen(prefix) + strlen(ending);
    char *text = malloc(cap);
    if (!text)
        return NULL;
    char *p = text;
#define APPEND(s)                                                                                            \
    do {                                                                                                     \
        size_t n = strlen(s);                                                                                \
        memcpy(p, s, n);                                                                                     \
        p += n;                                                                                              \
    } while (0)
    APPEND((const char *)lint_bridge);
    APPEND(a);
    p = encoded(p, source);
    APPEND(b);
    p = encoded(p, doc->filename);
    APPEND(c);
    p = encoded(p, doc->project);
    APPEND(d);
    for (int i = 0; i < imports->count; i++) {
        APPEND(prefix);
        p = encoded(p, imports->paths[i]);
        APPEND(ending);
    }
    APPEND("],base64.b64decode('");
    p = encoded(p, rules->text);
    APPEND("').decode('ascii'),");
    APPEND(odoo->enabled ? "True" : "False");
    APPEND(",base64.b64decode('");
    p = encoded(p, odoo->ignored.text);
    APPEND("').decode('ascii'))\n");
    *p = 0;
#undef APPEND
    return text;
}
void lint_init(Linter *engine) {
    memset(engine, 0, sizeof(*engine));
    runner_init(&engine->process);
    odoo_config_init(&engine->odoo);
    engine->odoo.enabled = false;
}
bool lint_set_odoo(Linter *engine, const OdooConfig *config, char *error, size_t cap) {
    if (!engine || !odoo_config_valid(config))
        return failure(error, cap, "Configuration Odoo19 invalide.");
    if (engine->odoo.enabled == config->enabled && !strcmp(engine->odoo.ignored.text, config->ignored.text))
        return true;
    if (engine->owner) {
        engine->owner->valid = false;
        engine->owner->waiting = engine->owner->observed;
        engine->owner->changed_at = runner_clock();
    }
    if (engine->process.running) {
        engine->owner = NULL;
        engine->discard = true;
        runner_stop(&engine->process);
    }
    engine->odoo = *config;
    return true;
}
bool lint_set_disabled(Linter *engine, const char *value, char *error, size_t cap) {
    LintRules next;
    if (!engine || !lint_rules_parse(&next, value, error, cap))
        return false;
    if (lint_rules_valid(&engine->disabled) && !strcmp(next.text, engine->disabled.text))
        return true;
    if (engine->owner) {
        engine->owner->valid = false;
        engine->owner->waiting = engine->owner->observed;
        engine->owner->changed_at = runner_clock();
    }
    if (engine->process.running) {
        engine->owner = NULL;
        engine->discard = true;
        runner_stop(&engine->process);
    }
    engine->disabled = next;
    return true;
}
void lint_observe(LintDocument *doc, unsigned long version, const char *filename, const char *project,
                  double now) {
    if (!doc || !filename || !project)
        return;
    if (!doc->observed || doc->observed_version != version || strcmp(doc->filename, filename) ||
        strcmp(doc->project, project)) {
        doc->observed = true;
        doc->observed_version = version;
        copy(doc->filename, sizeof(doc->filename), filename);
        copy(doc->project, sizeof(doc->project), project);
        doc->changed_at = now;
        doc->valid = false;
        doc->waiting = true;
    }
}
bool lint_ready(const LintDocument *doc, double now) {
    return doc && doc->observed && doc->waiting && now - doc->changed_at >= LINT_DEBOUNCE;
}
static void report_failure(LintDocument *doc, unsigned long version, const char *message) {
    memset(&doc->report, 0, sizeof(doc->report));
    doc->report.backend = LINT_BACKEND_ERROR;
    copy(doc->report.status, sizeof(doc->report.status), message);
    doc->analyzed_version = version;
    doc->valid = true;
    doc->waiting = false;
}
bool lint_start(Linter *engine, LintDocument *doc, const char *python, const char *source, char *error,
                size_t cap) {
    if (!engine || !doc || !doc->observed || !python || !*python || !source)
        return failure(error, cap, "Configuration d’analyse invalide.");
    if (engine->process.running)
        return failure(error, cap, "Une analyse est déjà en cours.");
    if (!odoo_config_valid(&engine->odoo))
        return failure(error, cap, "Configuration Odoo19 invalide.");
    if (!lint_rules_valid(&engine->disabled))
        return failure(error, cap, "Règles PyLint invalides.");
    if (!import_paths_valid(&engine->imports))
        return failure(error, cap, "Dossiers d’import invalides.");
    if (strlen(source) > BUF_LIMIT)
        return failure(error, cap, "Fichier trop volumineux pour l’analyse.");
    char *code = program(source, doc, &engine->imports, &engine->disabled, &engine->odoo);
    char *output = malloc(LINT_OUTPUT_LIMIT + 1u);
    bool ok = code && output;
    if (!ok)
        failure(error, cap, "Mémoire insuffisante pour l’analyse.");
    if (ok)
        ok = runner_start_tool(&engine->process, python, code, doc->filename, doc->project, error, cap);
    free(code);
    if (!ok) {
        free(output);
        report_failure(doc, doc->observed_version, error && cap ? error : "Analyse indisponible.");
        return false;
    }
    free(engine->output);
    engine->output = output;
    engine->used = 0;
    output[0] = 0;
    engine->owner = doc;
    engine->version = doc->observed_version;
    copy(engine->filename, sizeof(engine->filename), doc->filename);
    copy(engine->project, sizeof(engine->project), doc->project);
    engine->discard = engine->failed = false;
    engine->error[0] = 0;
    doc->waiting = false;
    doc->valid = false;
    return true;
}
void lint_poll(Linter *engine, double now) {
    if (!engine || !engine->process.running)
        return;
    LintDocument *doc = engine->owner;
    if (doc && (doc->observed_version != engine->version || strcmp(doc->filename, engine->filename) ||
                strcmp(doc->project, engine->project))) {
        engine->discard = true;
        runner_stop(&engine->process);
    }
    if (!engine->discard && now - engine->process.started > LINT_TIMEOUT && !engine->failed) {
        engine->failed = true;
        copy(engine->error, sizeof(engine->error), "Analyse interrompue après 15 s. Réessayez avec F4.");
        runner_stop(&engine->process);
    }
    char chunk[8192];
    for (int i = 0; i < 4 && engine->process.running; i++) {
        size_t n = runner_poll(&engine->process, chunk, sizeof(chunk));
        if (n) {
            if (n > LINT_OUTPUT_LIMIT - engine->used) {
                engine->failed = true;
                copy(engine->error, sizeof(engine->error), "Sortie d’analyse trop volumineuse.");
                runner_stop(&engine->process);
            } else {
                memcpy(engine->output + engine->used, chunk, n);
                engine->used += n;
                engine->output[engine->used] = 0;
            }
        } else
            break;
    }
    if (!engine->process.running) {
        if (doc && !engine->discard) {
            if (!engine->failed && engine->process.exit_code) {
                engine->failed = true;
                snprintf(engine->error, sizeof(engine->error),
                         "Python/PyLint n’a pas pu terminer l’analyse (code %d). Vérifiez l’interpréteur "
                         "configuré.",
                         engine->process.exit_code);
            }
            if (!engine->failed && !lint_parse(&doc->report, engine->output)) {
                engine->failed = true;
                copy(engine->error, sizeof(engine->error), "Réponse d’analyse invalide. Réessayez avec F4.");
            }
            if (engine->failed)
                report_failure(doc, engine->version, engine->error);
            else {
                doc->report.revision = ++engine->report_sequence;
                doc->analyzed_version = engine->version;
                doc->valid = true;
                doc->waiting = false;
            }
        }
        free(engine->output);
        engine->output = NULL;
        engine->used = 0;
        engine->owner = NULL;
    }
}
void lint_forget(Linter *engine, LintDocument *doc) {
    if (engine && engine->owner == doc) {
        engine->owner = NULL;
        engine->discard = true;
        runner_stop(&engine->process);
    }
}
void lint_close(Linter *engine) {
    if (!engine)
        return;
    engine->owner = NULL;
    engine->discard = true;
    runner_close(&engine->process);
    free(engine->output);
    engine->output = NULL;
    engine->used = 0;
}
static size_t position(Buffer *b, size_t row, size_t column) {
    if (row >= b->line_count)
        row = b->line_count - 1;
    size_t start = b->lines[row], end = buf_line_end(b, row);
    size_t p = column > end - start ? end : start + column;
    while (p > start && p < b->len && ((unsigned char)b->text[p] & 0xc0) == 0x80)
        p--;
    return p;
}
void lint_range(Buffer *b, const Diagnostic *d, size_t *start, size_t *end) {
    buf_index(b);
    *start = position(b, d->row, d->column);
    *end = position(b, d->end_row, d->end_column);
    if (*end <= *start) {
        *end = *start;
        size_t limit = buf_line_end(b, buf_row(b, *start));
        if (*end < limit) {
            bool word = isalnum((unsigned char)b->text[*end]) || b->text[*end] == '_' ||
                        (unsigned char)b->text[*end] >= 128;
            do {
                *end = utf8_next(b->text, b->len, *end);
                if (!word || *end - *start >= 512)
                    break;
            } while (*end < limit && (isalnum((unsigned char)b->text[*end]) || b->text[*end] == '_' ||
                                      (unsigned char)b->text[*end] >= 128));
        }
    }
}
void lint_resolve(LintReport *report, Buffer *b) {
    if (!report || report->resolved)
        return;
    for (int i = 0; i < report->count; i++) {
        Diagnostic *d = &report->items[i];
        lint_range(b, d, &d->start, &d->end);
        d->row = buf_row(b, d->start);
        d->end_row = buf_row(b, d->end);
        d->visual_column = buf_column(b, d->start);
        d->end_visual_column = buf_column(b, d->end);
    }
    report->resolved = true;
}
