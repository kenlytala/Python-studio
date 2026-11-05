#include "lint_rules.h"
#include "buffer.h"
#include "files.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static bool fail(char *error, size_t cap, const char *message) {
    if (cap)
        snprintf(error, cap, "%s", message);
    return false;
}
static bool separator(unsigned char c) {
    return c == ',' || c == ';' || c == ' ' || c == '\t' || c == '\r' || c == '\n';
}
static bool alpha(unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}
static bool digit(unsigned char c) {
    return c >= '0' && c <= '9';
}
static unsigned char lower(unsigned char c) {
    return c >= 'A' && c <= 'Z' ? (unsigned char)(c + 32) : c;
}
bool lint_rules_parse(LintRules *out, const char *value, char *error, size_t cap) {
    if (cap)
        error[0] = 0;
    if (!out || !value)
        return fail(error, cap, "La liste des règles PyLint doit être du texte.");
    size_t n = 0;
    while (n < LINT_RULES_BYTES && value[n])
        n++;
    if (n == LINT_RULES_BYTES)
        return fail(error, cap, "Liste PyLint limitée à 4 095 octets.");
    LintRules next = {0};
    char seen[LINT_RULES_MAX][LINT_RULE_NAME_MAX + 1];
    size_t used = 0;
    for (size_t p = 0; p < n;) {
        if (separator((unsigned char)value[p])) {
            p++;
            continue;
        }
        size_t start = p;
        while (p < n && !separator((unsigned char)value[p]))
            p++;
        size_t size = p - start;
        bool id = size == 5 && strchr("efwcri", lower((unsigned char)value[start]));
        for (size_t i = 1; i < size && id; i++)
            if (!digit((unsigned char)value[start + i]))
                id = false;
        bool symbol = size > 1 && size <= LINT_RULE_NAME_MAX && alpha((unsigned char)value[start]);
        for (size_t i = 1; i < size && symbol; i++) {
            unsigned char c = (unsigned char)value[start + i];
            if (!alpha(c) && !digit(c) && !(c == '-' && i + 1 < size && value[start + i - 1] != '-'))
                symbol = false;
        }
        if (!id && !symbol)
            return fail(error, cap,
                        "Règle PyLint invalide : utilisez un ID (W0611) ou un nom (unused-import).");
        char token[LINT_RULE_NAME_MAX + 1];
        for (size_t i = 0; i < size; i++)
            token[i] = (char)(id && !i ? lower((unsigned char)value[start + i]) - 32
                                       : lower((unsigned char)value[start + i]));
        token[size] = 0;
        if (!strcmp(token, "all"))
            return fail(error, cap,
                        "Règle PyLint invalide : utilisez un ID (W0611) ou un nom (unused-import).");
        bool duplicate = false;
        for (unsigned i = 0; i < next.count; i++)
            if (!strcmp(seen[i], token)) {
                duplicate = true;
                break;
            }
        if (duplicate)
            continue;
        if (next.count == LINT_RULES_MAX)
            return fail(error, cap, "Limite : 64 règles PyLint distinctes.");
        if (used + (next.count ? 1 : 0) + size >= sizeof(next.text))
            return fail(error, cap, "Liste PyLint limitée à 4 095 octets.");
        if (next.count)
            next.text[used++] = ',';
        memcpy(next.text + used, token, size + 1);
        used += size;
        strcpy(seen[next.count++], token);
    }
    *out = next;
    return true;
}
bool lint_rules_valid(const LintRules *rules) {
    if (!rules || !memchr(rules->text, 0, sizeof(rules->text)))
        return false;
    LintRules canonical;
    return lint_rules_parse(&canonical, rules->text, NULL, 0) && canonical.count == rules->count &&
           !strcmp(canonical.text, rules->text);
}
bool lint_rules_file(char *out, size_t cap, const char *settings) {
    char parent[BUF_PATH];
    if (!out || !cap || !settings || !fs_parent(settings, parent))
        return false;
    int n = snprintf(out, cap, "%s/pylint-rules.cfg", parent);
    return n >= 0 && (size_t)n < cap;
}
static const char header[] = "PYTHON-STUDIO-PYLINT\t1\nDISABLE\t";
bool lint_rules_load(LintRules *out, const char *file, char *error, size_t cap) {
    if (cap)
        error[0] = 0;
    if (!out || !file)
        return fail(error, cap, "Profil PyLint invalide.");
    FILE *f = fopen(file, "rb");
    if (!f)
        return errno == ENOENT ? false : fail(error, cap, strerror(errno));
    char data[LINT_RULES_BYTES + sizeof(header) + 2];
    size_t n = fread(data, 1, sizeof(data), f);
    bool ok = !ferror(f) && n < sizeof(data);
    fclose(f);
    if (ok) {
        data[n] = 0;
        ok = n >= sizeof(header) && !memchr(data, 0, n) && !memcmp(data, header, sizeof(header) - 1) &&
             data[n - 1] == '\n';
    }
    LintRules next;
    if (ok) {
        data[n - 1] = 0;
        ok = lint_rules_parse(&next, data + sizeof(header) - 1, error, cap);
    }
    if (!ok)
        return fail(error, cap, "Profil des règles PyLint invalide ou trop volumineux.");
    *out = next;
    return true;
}
bool lint_rules_save(const LintRules *rules, const char *file, char *error, size_t cap) {
    char local_error[512] = {0};
    if (!error || !cap) {
        error = local_error;
        cap = sizeof(local_error);
    }
    if (cap)
        error[0] = 0;
    if (!lint_rules_valid(rules) || !file)
        return fail(error, cap, "Profil PyLint invalide.");
    char data[LINT_RULES_BYTES + sizeof(header) + 1];
    snprintf(data, sizeof(data), "%s%s\n", header, rules->text);
    Buffer *b = calloc(1, sizeof(*b));
    if (!b)
        return fail(error, cap, "Mémoire insuffisante pour les règles PyLint.");
    bool ok = buf_init(b, data, "pylint-rules.cfg");
    if (ok)
        ok = buf_save(b, file, error, cap);
    else
        fail(error, cap, "Mémoire insuffisante pour les règles PyLint.");
    buf_free(b);
    free(b);
    return ok;
}

int lint_rule_action(const char *code, const char *symbol) {
    if (!code || !symbol)
        return -1;
    size_t n = 0;
    while (n < 6 && code[n])
        n++;
    if (n != 5 || !strcmp(code, "E0001") || !strcmp(symbol, "syntax-error"))
        return -1;
    static const char categories[] = "EWCRIF";
    const char *category = strchr(categories, code[0]);
    if (!category)
        return -1;
    int value = 0;
    for (int i = 1; i < 5; i++) {
        if (!digit((unsigned char)code[i]))
            return -1;
        value = value * 10 + code[i] - '0';
    }
    return (int)(category - categories) * 10000 + value;
}
bool lint_rule_decode(int action, char out[6]) {
    if (!out || action < 0 || action >= 60000)
        return false;
    snprintf(out, 6, "%c%04d", "EWCRIF"[action / 10000], action % 10000);
    return true;
}
static bool has_token(const char *list, const char *token) {
    if (!token || !*token)
        return false;
    size_t n = strlen(token);
    for (const char *p = list; *p;) {
        const char *end = strchr(p, ',');
        size_t length = end ? (size_t)(end - p) : strlen(p);
        if (length == n && !memcmp(p, token, n))
            return true;
        if (!end)
            break;
        p = end + 1;
    }
    return false;
}
bool lint_rules_add(LintRules *out, const LintRules *current, const char *code, const char *symbol,
                    char *error, size_t cap) {
    if (cap)
        error[0] = 0;
    if (!out || !lint_rules_valid(current) || lint_rule_action(code, symbol) < 0)
        return fail(error, cap, "Ce diagnostic ne peut pas être désactivé ici.");
    if (has_token(current->text, code) || has_token(current->text, symbol)) {
        *out = *current;
        return true;
    }
    char joined[LINT_RULES_BYTES + 8];
    snprintf(joined, sizeof(joined), "%s%s%s", current->text, current->count ? "," : "", code);
    return lint_rules_parse(out, joined, error, cap);
}
uint64_t lint_rules_scope(const char *project, const char *path, const char *name) {
    uint64_t hash = UINT64_C(14695981039346656037);
    const char *values[] = {project, path, name};
    for (unsigned i = 0; i < 3; i++) {
        const unsigned char *p = (const unsigned char *)(values[i] ? values[i] : "");
        for (; *p; p++) {
            hash ^= *p;
            hash *= UINT64_C(1099511628211);
        }
        hash ^= 0;
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}
