#define _POSIX_C_SOURCE 200809L
#include "completion.h"
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif
#include "prisme_completion_bridge.inc"

static bool fail(char *error, size_t cap, const char *text) {
    if (cap)
        snprintf(error, cap, "%s", text);
    return false;
}
static bool numeric(const char *text, size_t *out) {
    if (!text || !*text)
        return false;
    size_t n = 0;
    for (const char *p = text; *p; p++) {
        if (*p < '0' || *p > '9' || n > 100000000u)
            return false;
        n = n * 10u + (size_t)(*p - '0');
    }
    if (n > 1000000000u)
        return false;
    *out = n;
    return true;
}
static int digit(unsigned char c) {
    return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
}
static bool decode(char *out, size_t cap, const char *text) {
    size_t n = strlen(text);
    if (n % 2 || n / 2 >= cap)
        return false;
    for (size_t i = 0; i < n; i += 2) {
        int a = digit((unsigned char)text[i]), b = digit((unsigned char)text[i + 1]);
        if (a < 0 || b < 0 || a * 16 + b < 32 || a * 16 + b == 127)
            return false;
        out[i / 2] = (char)(a * 16 + b);
    }
    out[n / 2] = 0;
    return utf8_valid(out, n / 2);
}
bool completion_parse(CompletionReport *out, const char *protocol) {
    if (!out || !protocol || strlen(protocol) > COMPLETION_OUTPUT_MAX)
        return false;
    CompletionReport next = {0};
    bool header = false, ended = false;
    const char *p = protocol;
    while (*p) {
        const char *end = strchr(p, '\n');
        if (!end || ended || end == p || end - p >= 2048)
            return false;
        char line[2048];
        size_t n = (size_t)(end - p);
        memcpy(line, p, n);
        line[n] = 0;
        char *fields[9] = {line};
        int count = 1;
        for (size_t i = 0; i < n; i++)
            if (line[i] == '\t') {
                if (count == 9)
                    return false;
                line[i] = 0;
                fields[count++] = line + i + 1;
            }
        if (!header) {
            size_t sequence, failed;
            if (count != 7 || strcmp(fields[0], "PRISME-COMPLETE") || strcmp(fields[1], "1") ||
                !numeric(fields[2], &sequence) || !sequence || !numeric(fields[3], &next.start) ||
                !numeric(fields[4], &next.end) || next.start > next.end || next.end > BUF_LIMIT ||
                !numeric(fields[5], &failed) || failed > 1 ||
                !decode(next.status, sizeof(next.status), fields[6]))
                return false;
            next.sequence = (unsigned long)sequence;
            next.failed = failed != 0;
            header = true;
        } else if (!strcmp(fields[0], "C")) {
            if (count != 4 || next.count == COMPLETION_MAX || strlen(fields[1]) != 1 ||
                !strchr("CFMOV", fields[1][0]))
                return false;
            CompletionItem *item = &next.items[next.count];
            item->kind = fields[1][0];
            item->snippet = -1;
            item->start = next.start;
            item->end = next.end;
            if (!decode(item->name, sizeof(item->name), fields[2]) || !*item->name ||
                !decode(item->detail, sizeof(item->detail), fields[3]))
                return false;
            for (int i = 0; i < next.count; i++)
                if (!strcmp(next.items[i].name, item->name))
                    return false;
            next.count++;
        } else if (!strcmp(fields[0], "END")) {
            size_t sequence, total;
            if (count != 3 || !numeric(fields[1], &sequence) || sequence != next.sequence ||
                !numeric(fields[2], &total) || total != (size_t)next.count || (next.failed && next.count))
                return false;
            ended = true;
        } else
            return false;
        p = end + 1;
    }
    if (!header || !ended)
        return false;
    *out = next;
    return true;
}
static void cleanup_request(Completer *service) {
    if (service->request)
        fclose(service->request);
    service->request = NULL;
    if (*service->request_path)
        remove(service->request_path);
    service->request_path[0] = 0;
}
void completion_init(Completer *service) {
    memset(service, 0, sizeof(*service));
    runner_init(&service->process);
}
void completion_reset(Completer *service) {
    runner_stop(&service->process);
    service->awaiting = service->ready = false;
    service->used = 0;
    service->output[0] = 0;
    memset(&service->report, 0, sizeof(service->report));
    service->retry_after = 0;
    if (!service->process.running)
        cleanup_request(service);
}
static void failed_report(Completer *service, const char *message) {
    memset(&service->report, 0, sizeof(service->report));
    service->report.sequence = service->sequence;
    service->report.failed = true;
    snprintf(service->report.status, sizeof(service->report.status), "%s", message);
    service->ready = true;
    service->awaiting = false;
    service->retry_after = runner_clock() + 5;
}
void completion_poll(Completer *service, double now) {
    if (!service->process.running)
        return;
    if (service->awaiting && now - service->sent_at > COMPLETION_TIMEOUT) {
        failed_report(
            service, "Complétion interrompue après 15 s. Réduisez les dossiers et actualisez la complétion.");
        runner_stop(&service->process);
    }
    char chunk[4096];
    for (int i = 0; i < 8 && service->process.running; i++) {
        size_t n = runner_poll(&service->process, chunk, sizeof(chunk));
        if (!n)
            break;
        if (service->process.stopped)
            continue;
        if (n > COMPLETION_OUTPUT_MAX - service->used) {
            failed_report(service, "Réponse de complétion trop volumineuse ; outil arrêté.");
            runner_stop(&service->process);
            continue;
        }
        memcpy(service->output + service->used, chunk, n);
        service->used += n;
        service->output[service->used] = 0;
        if (strstr(service->output, "\nEND\t") && service->used &&
            service->output[service->used - 1] == '\n') {
            if (!service->awaiting || !completion_parse(&service->report, service->output) ||
                service->report.sequence != service->sequence) {
                failed_report(service, "Protocole de complétion invalide ; vérifiez l’interpréteur Python.");
                runner_stop(&service->process);
            } else {
                service->awaiting = false;
                service->ready = true;
            }
            service->used = 0;
            service->output[0] = 0;
        }
    }
    if (!service->process.running) {
        cleanup_request(service);
        if (service->awaiting)
            failed_report(service,
                          "Outil de complétion indisponible. Vérifiez --python et les sources du framework.");
        service->used = 0;
    }
}
static bool request_file(Completer *service, char *error, size_t cap) {
#ifdef _WIN32
    char directory[MAX_PATH];
    if (!GetTempPathA(MAX_PATH, directory) || !GetTempFileNameA(directory, "psc", 0, service->request_path))
        return fail(error, cap, "Dossier temporaire inaccessible pour la complétion.");
    service->request = fopen(service->request_path, "w+b");
#else
    const char *tmp = getenv("TMPDIR");
    int n = snprintf(service->request_path, sizeof(service->request_path), "%s/prisme-completion-XXXXXX",
                     tmp && *tmp ? tmp : "/tmp");
    if (n < 0 || (size_t)n >= sizeof(service->request_path)) {
        service->request_path[0] = 0;
        return fail(error, cap, "Chemin temporaire de complétion trop long.");
    }
    int fd = mkstemp(service->request_path);
    service->request = fd >= 0 ? fdopen(fd, "w+b") : NULL;
    if (fd >= 0 && !service->request)
        close(fd);
#endif
    if (!service->request) {
        cleanup_request(service);
        return fail(error, cap, "Création de la requête privée de complétion impossible.");
    }
    return true;
}
static char *encode(char *out, const char *text) {
    const char *hex = "0123456789abcdef";
    for (const unsigned char *p = (const unsigned char *)text; *p; p++) {
        *out++ = hex[*p >> 4];
        *out++ = hex[*p & 15];
    }
    *out = 0;
    return out;
}
static char *program(const Completer *service, const char *project, const ImportPaths *paths) {
    size_t cap = sizeof(completion_bridge) + 2 * (strlen(service->request_path) + strlen(project)) + 256;
    for (int i = 0; i < paths->count; i++)
        cap += 2 * strlen(paths->paths[i]) + 64;
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
    APPEND((const char *)completion_bridge);
    APPEND("\n_prisme_completion_serve(bytes.fromhex('");
    p = encode(p, service->request_path);
    APPEND("').decode('utf-8'),bytes.fromhex('");
    p = encode(p, project);
    APPEND("').decode('utf-8'),[");
    for (int i = 0; i < paths->count; i++) {
        APPEND("bytes.fromhex('");
        p = encode(p, paths->paths[i]);
        APPEND("').decode('utf-8'),");
    }
    APPEND("])\n");
    *p = 0;
#undef APPEND
    return text;
}
static bool same_paths(const ImportPaths *left, const ImportPaths *right) {
    if (left->count != right->count)
        return false;
    for (int i = 0; i < left->count; i++)
        if (strcmp(left->paths[i], right->paths[i]))
            return false;
    return true;
}
bool completion_submit(Completer *service, const char *python, const char *project, const ImportPaths *paths,
                       const char *filename, const char *source, unsigned long version, size_t cursor,
                       char *error, size_t cap) {
    if (cap)
        error[0] = 0;
    if (!service || !python || !*python || !project || !filename || !source || !import_paths_valid(paths) ||
        strlen(project) >= BUF_PATH || strlen(filename) >= BUF_PATH || strlen(python) >= BUF_PATH)
        return fail(error, cap, "Configuration de complétion invalide.");
    size_t n = strlen(source);
    if (n > BUF_LIMIT || cursor > n || !utf8_valid(source, n) ||
        (cursor < n && ((unsigned char)source[cursor] & 0xc0) == 0x80))
        return fail(error, cap, "Position ou document UTF-8 invalide pour la complétion.");
    if (service->process.running && (strcmp(service->project, project) || strcmp(service->python, python) ||
                                     !same_paths(&service->paths, paths)))
        completion_reset(service);
    if (service->awaiting || (service->process.running && service->process.stopped) ||
        runner_clock() < service->retry_after)
        return false;
    if (!service->process.running) {
        cleanup_request(service);
        if (!request_file(service, error, cap))
            return false;
        char *code = program(service, project, paths);
        bool allocated = code != NULL;
        bool ok =
            allocated && runner_start_tool(&service->process, python, code, filename, project, error, cap);
        free(code);
        if (!ok) {
            cleanup_request(service);
            service->retry_after = runner_clock() + 5;
            return allocated ? false : fail(error, cap, "Mémoire insuffisante pour la complétion.");
        }
        snprintf(service->project, sizeof(service->project), "%s", project);
        snprintf(service->python, sizeof(service->python), "%s", python);
        service->paths = *paths;
    }
    service->sequence = service->sequence >= 1000000000ul ? 1 : service->sequence + 1;
    service->version = version;
    service->cursor = cursor;
    snprintf(service->filename, sizeof(service->filename), "%s", filename);
    FILE *f = service->request;
    clearerr(f);
    rewind(f);
#ifdef _WIN32
    bool ok = _chsize(_fileno(f), 0) == 0;
#else
    bool ok = ftruncate(fileno(f), 0) == 0;
#endif
    char hex[2 * BUF_PATH];
    encode(hex, filename);
    ok = ok && fprintf(f, "PRISME-COMPLETE-REQUEST\t1\t%lu\t%zu\t%s\n", service->sequence, cursor, hex) > 0;
    ok = ok && fwrite(source, 1, n, f) == n;
    ok = fflush(f) == 0 && ok;
    char command[48];
    snprintf(command, sizeof(command), "Q\t%lu", service->sequence);
    if (!ok || !runner_input(&service->process, command)) {
        runner_stop(&service->process);
        return fail(error, cap, "Impossible d’envoyer la requête de complétion.");
    }
    service->awaiting = true;
    service->ready = false;
    service->used = 0;
    service->output[0] = 0;
    service->sent_at = runner_clock();
    return true;
}
void completion_close(Completer *service) {
    runner_close(&service->process);
    cleanup_request(service);
    service->awaiting = service->ready = false;
}
static bool name_char(unsigned char c) {
    return isalnum(c) || c == '_' || c >= 128;
}
void completion_span(const Buffer *document, size_t *start, size_t *end) {
    size_t a = document->cursor, b = a;
    while (a) {
        size_t prev = utf8_prev(document->text, a);
        if (!name_char((unsigned char)document->text[prev]))
            break;
        a = prev;
    }
    while (b < document->len && name_char((unsigned char)document->text[b]))
        b = utf8_next(document->text, document->len, b);
    *start = a;
    *end = b;
}
