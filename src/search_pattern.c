#define PCRE2_CODE_UNIT_WIDTH 8
#include "search_pattern.h"
#include "buffer.h"
#include "runner.h"
#include <pcre2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MATCH_SECONDS 2.0
#define MATCH_LIMIT 200000u
#define DEPTH_LIMIT 512u
#define HEAP_KIB 8192u

typedef struct {
    pcre2_code *code;
    bool bare;
} Glob;
struct SearchMatcher {
    pcre2_code *content;
    Glob globs[4][SEARCH_PATTERNS_MAX];
    unsigned counts[4], ticks;
    const atomic_bool *cancel;
    pcre2_match_context *context;
    pcre2_match_data *data;
    double deadline;
    bool timed_out;
};
static bool fail(char *error, size_t cap, const char *text) {
    if (error && cap)
        snprintf(error, cap, "%s", text);
    return false;
}
SearchOptions folder_search_options(void) {
    SearchOptions options = {.case_sensitive = true, .recursive = true};
    return options;
}
bool folder_search_options_equal(const SearchOptions *a, const SearchOptions *b) {
    return a && b && a->case_sensitive == b->case_sensitive && a->whole_word == b->whole_word &&
           a->regex == b->regex && a->recursive == b->recursive &&
           !strcmp(a->include_files, b->include_files) && !strcmp(a->exclude_files, b->exclude_files) &&
           !strcmp(a->include_dirs, b->include_dirs) && !strcmp(a->exclude_dirs, b->exclude_dirs);
}
static bool tokens(const char *value, char output[SEARCH_PATTERNS_MAX][SEARCH_PATTERN_MAX + 1],
                   unsigned *count, char *error, size_t cap) {
    *count = 0;
    const char *nul = memchr(value, 0, SEARCH_FILTER_MAX + 1);
    if (!nul || !utf8_valid(value, (size_t)(nul - value)))
        return fail(error, cap, "Filtres : 512 octets UTF-8 maximum, sur une ligne.");
    for (const char *p = value; p < nul; p++)
        if ((unsigned char)*p < 32 || (unsigned char)*p == 127)
            return fail(error, cap, "Filtres : 512 octets UTF-8 maximum, sur une ligne.");
    while (*value) {
        const char *end = strchr(value, ';');
        if (!end)
            end = value + strlen(value);
        const char *next = *end ? end + 1 : end;
        while (value < end && *value == ' ')
            value++;
        while (end > value && end[-1] == ' ')
            end--;
        if (value < end) {
            char token[SEARCH_FILTER_MAX + 1];
            size_t n = (size_t)(end - value);
            memcpy(token, value, n);
            token[n] = 0;
            for (size_t i = 0; i < n; i++)
                if (token[i] == '\\')
                    token[i] = '/';
            char *start = token;
            if (start[0] == '.' && start[1] == '/')
                start += 2;
            n = strlen(start);
            while (n && start[n - 1] == '/')
                start[--n] = 0;
            bool parent = false;
            for (const char *p = start; *p;) {
                const char *q = strchr(p, '/');
                if (!q)
                    q = p + strlen(p);
                if (q - p == 2 && p[0] == '.' && p[1] == '.')
                    parent = true;
                p = *q ? q + 1 : q;
            }
            if (!n || n > SEARCH_PATTERN_MAX || start[0] == '/' || strchr(start, ':') || parent)
                return fail(error, cap,
                            "Filtre relatif invalide : pas de chemin absolu ou '..', 256 octets par motif.");
            if (*count == SEARCH_PATTERNS_MAX)
                return fail(error, cap, "Filtres : 16 motifs maximum par champ, séparés par ';'.");
            strcpy(output[(*count)++], start);
        }
        value = next;
    }
    return true;
}
bool folder_search_options_valid(const SearchOptions *options, char *error, size_t cap) {
    if (!options)
        return fail(error, cap, "Options de recherche invalides.");
    const char *values[] = {options->include_files, options->exclude_files, options->include_dirs,
                            options->exclude_dirs};
    char parsed[SEARCH_PATTERNS_MAX][SEARCH_PATTERN_MAX + 1];
    unsigned count;
    for (unsigned i = 0; i < 4; i++)
        if (!tokens(values[i], parsed, &count, error, cap))
            return false;
    return true;
}
static int callout(pcre2_callout_block *block, void *user) {
    (void)block;
    SearchMatcher *m = user;
    if (!(++m->ticks % 64)) {
        if (m->cancel && atomic_load(m->cancel))
            return PCRE2_ERROR_CALLOUT;
        if (runner_clock() >= m->deadline) {
            m->timed_out = true;
            return PCRE2_ERROR_CALLOUT;
        }
    }
    return 0;
}
static pcre2_code *compile(const char *pattern, bool sensitive, char *error, size_t cap) {
    uint32_t flags = PCRE2_UTF | PCRE2_UCP | PCRE2_MULTILINE | PCRE2_AUTO_CALLOUT | PCRE2_NEVER_BACKSLASH_C;
    if (!sensitive)
        flags |= PCRE2_CASELESS;
    int code;
    PCRE2_SIZE offset;
    pcre2_code *result = pcre2_compile((PCRE2_SPTR)pattern, strlen(pattern), flags, &code, &offset, NULL);
    if (!result) {
        PCRE2_UCHAR message[256] = {0};
        pcre2_get_error_message(code, message, sizeof(message));
        if (error && cap)
            snprintf(error, cap, "Expression invalide (octet %zu du motif compilé) : %s", (size_t)offset,
                     (char *)message);
    }
    return result;
}
static char *literal(const char *value) {
    size_t n = strlen(value);
    char *out = malloc(n * 5 + 5);
    if (!out)
        return NULL;
    strcpy(out, "\\Q");
    size_t used = 2;
    for (size_t i = 0; i < n; i++) {
        if (value[i] == '\\' && i + 1 < n && value[i + 1] == 'E') {
            const char *escape = "\\E\\\\E\\Q";
            size_t bytes = strlen(escape);
            memcpy(out + used, escape, bytes);
            used += bytes;
            i++;
        } else
            out[used++] = value[i];
    }
    memcpy(out + used, "\\E", 3);
    return out;
}
static void glob_regex(const char *value, char out[4096]) {
    size_t used = 0;
    out[used++] = '\\';
    out[used++] = 'A';
    for (size_t i = 0; value[i]; i++) {
        const char *part = NULL;
        if (value[i] == '*') {
            if (value[i + 1] == '*') {
                while (value[i + 1] == '*')
                    i++;
                if (value[i + 1] == '/') {
                    part = "(?:.*/)?";
                    i++;
                } else
                    part = ".*";
            } else
                part = "[^/]*";
        } else if (value[i] == '?')
            part = "[^/]";
        if (part) {
            size_t n = strlen(part);
            memcpy(out + used, part, n);
            used += n;
        } else {
            if (strchr("\\.^$|+()[]{}", value[i]))
                out[used++] = '\\';
            out[used++] = value[i];
        }
    }
    memcpy(out + used, "\\z", 3);
}
SearchMatcher *search_matcher_new(const char *query, const SearchOptions *o, const atomic_bool *cancel,
                                  char *error, size_t cap) {
    if (!folder_search_query_valid(query) || !folder_search_options_valid(o, error, cap))
        return NULL;
    SearchMatcher *m = calloc(1, sizeof(*m));
    if (!m) {
        fail(error, cap, "Mémoire insuffisante pour les motifs.");
        return NULL;
    }
    m->cancel = cancel;
    if (o->regex || !o->case_sensitive || o->whole_word) {
        char *raw = o->regex ? NULL : literal(query);
        const char *pattern = o->regex ? query : raw;
        if (!pattern) {
            fail(error, cap, "Mémoire insuffisante pour le motif.");
            goto bad;
        }
        char *wrapped = NULL;
        if (o->whole_word) {
            size_t n = strlen(pattern) + 128;
            wrapped = malloc(n);
            if (!wrapped) {
                free(raw);
                fail(error, cap, "Mémoire insuffisante pour le motif.");
                goto bad;
            }
            snprintf(wrapped, n, "(?<![\\p{L}\\p{N}\\p{M}_])(?:%s)(?![\\p{L}\\p{N}\\p{M}_])", pattern);
            pattern = wrapped;
        }
        m->content = compile(pattern, o->case_sensitive, error, cap);
        free(raw);
        free(wrapped);
        if (!m->content)
            goto bad;
    }
    const char *values[] = {o->include_files, o->exclude_files, o->include_dirs, o->exclude_dirs};
    for (unsigned kind = 0; kind < 4; kind++) {
        char parsed[SEARCH_PATTERNS_MAX][SEARCH_PATTERN_MAX + 1];
        unsigned count;
        if (!tokens(values[kind], parsed, &count, error, cap))
            goto bad;
        for (unsigned i = 0; i < count; i++) {
            char pattern[4096];
            glob_regex(parsed[i], pattern);
            pcre2_code *code = compile(pattern, true, error, cap);
            if (!code)
                goto bad;
            m->globs[kind][m->counts[kind]++] = (Glob){code, strchr(parsed[i], '/') == NULL};
        }
    }
    m->context = pcre2_match_context_create(NULL);
    m->data = pcre2_match_data_create(513, NULL);
    if (!m->context || !m->data) {
        fail(error, cap, "Mémoire insuffisante pour PCRE2.");
        goto bad;
    }
    pcre2_set_match_limit(m->context, MATCH_LIMIT);
    pcre2_set_depth_limit(m->context, DEPTH_LIMIT);
    pcre2_set_heap_limit(m->context, HEAP_KIB);
    pcre2_set_callout(m->context, callout, m);
    return m;
bad:
    search_matcher_free(m);
    return NULL;
}
void search_matcher_free(SearchMatcher *m) {
    if (!m)
        return;
    if (m->content)
        pcre2_code_free(m->content);
    for (unsigned k = 0; k < 4; k++)
        for (unsigned i = 0; i < m->counts[k]; i++)
            pcre2_code_free(m->globs[k][i].code);
    if (m->context)
        pcre2_match_context_free(m->context);
    if (m->data)
        pcre2_match_data_free(m->data);
    free(m);
}
bool search_matcher_regex(const SearchMatcher *m) {
    return m && m->content;
}
void search_matcher_begin(SearchMatcher *m) {
    m->ticks = 0;
    m->timed_out = false;
    m->deadline = runner_clock() + MATCH_SECONDS;
}
static int match(SearchMatcher *m, pcre2_code *code, const char *text, size_t len, size_t start,
                 size_t *begin, size_t *end, char *error, size_t cap) {
    if (m->cancel && atomic_load(m->cancel))
        return 0;
    if (runner_clock() >= m->deadline) {
        fail(error, cap,
             "Expression interrompue : budget de temps atteint (2 s/fichier). Simplifiez le motif.");
        return -1;
    }
    /* Subjects are validated once by matched_hits/usable_name. All offsets
       come from UTF-8 PCRE2 or utf8_next; raw-byte \C is forbidden. */
    if (start > len || (start < len && ((unsigned char)text[start] & 0xc0) == 0x80)) {
        fail(error, cap, "Offset UTF-8 invalide.");
        return -1;
    }
    int result = pcre2_match(code, (PCRE2_SPTR)text, len, start, PCRE2_NO_UTF_CHECK, m->data, m->context);
    if (m->cancel && atomic_load(m->cancel))
        return 0;
    if (result == PCRE2_ERROR_NOMATCH)
        return 0;
    if (result < 0) {
        if (m->timed_out || result == PCRE2_ERROR_CALLOUT || result == PCRE2_ERROR_MATCHLIMIT ||
            result == PCRE2_ERROR_DEPTHLIMIT || result == PCRE2_ERROR_HEAPLIMIT)
            fail(error, cap,
                 "Expression interrompue : limite de travail, temps ou mémoire atteinte. Simplifiez le "
                 "motif.");
        else if (error && cap)
            snprintf(error, cap, "Expression interrompue (PCRE2 %d).", result);
        return -1;
    }
    PCRE2_SIZE *offsets = pcre2_get_ovector_pointer(m->data);
    if (offsets[0] < start || offsets[0] > offsets[1] || offsets[1] > len) {
        fail(error, cap, "Intervalle d’expression invalide.");
        return -1;
    }
    *begin = offsets[0];
    *end = offsets[1];
    return 1;
}
int search_matcher_next(SearchMatcher *m, const char *text, size_t len, size_t start, size_t *begin,
                        size_t *end, char *error, size_t cap) {
    return match(m, m->content, text, len, start, begin, end, error, cap);
}
int search_matcher_filter(SearchMatcher *m, unsigned kind, const char *relative, bool directory, char *error,
                          size_t cap) {
    if (kind > 3)
        return -1;
    if (!m->counts[kind])
        return kind == 0 || kind == 2 ? 1 : 0;
    const char *name = strrchr(relative, '/');
    name = name ? name + 1 : relative;
    for (unsigned i = 0; i < m->counts[kind]; i++) {
        Glob *g = &m->globs[kind][i];
        const char *value = g->bare ? name : relative;
        size_t begin, end;
        search_matcher_begin(m);
        int result = match(m, g->code, value, strlen(value), 0, &begin, &end, error, cap);
        if (result)
            return result;
        if (directory && !g->bare) {
            char slash[FS_PATH + 1];
            snprintf(slash, sizeof(slash), "%s/", value);
            result = match(m, g->code, slash, strlen(slash), 0, &begin, &end, error, cap);
            if (result)
                return result;
        }
    }
    return 0;
}
