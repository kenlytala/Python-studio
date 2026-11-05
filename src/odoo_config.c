#include "odoo_config.h"
#include "buffer.h"
#include "files.h"
#include "ssh_json.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static bool fail(char *error, size_t cap, const char *text) {
    if (cap)
        snprintf(error, cap, "%s", text);
    return false;
}
void odoo_config_init(OdooConfig *config) {
    memset(config, 0, sizeof(*config));
    config->enabled = true;
    strcpy(config->version, "19.0");
}
bool odoo_config_valid(const OdooConfig *config) {
    return config && memchr(config->version, 0, sizeof(config->version)) &&
           !strcmp(config->version, "19.0") && lint_rules_valid(&config->ignored);
}
bool odoo_diagnostic_code(const char *code) {
    if (!code || strlen(code) != 5 || !strchr("ECWRIF", code[0]))
        return false;
    for (int i = 1; i < 5; i++)
        if (code[i] < '0' || code[i] > '9')
            return false;
    return (code[1] == '8' && code[2] == '1') ||
           (code[0] == 'W' && code[1] == '9' && code[2] == '0' &&
            ((code[3] == '0' && code[4] >= '1') || (code[3] == '1' && code[4] == '0')));
}
bool odoo_config_file(char *out, size_t cap, const char *settings, const char *project) {
    char parent[BUF_PATH];
    if (!out || !cap || !settings || !project || !fs_parent(settings, parent))
        return false;
    uint64_t hash = UINT64_C(14695981039346656037);
    for (const unsigned char *p = (const unsigned char *)project; *p; p++) {
        hash ^= *p;
        hash *= UINT64_C(1099511628211);
    }
    int n = snprintf(out, cap, "%s/odoo-%016llx.json", parent, (unsigned long long)hash);
    return n >= 0 && (size_t)n < cap;
}
bool odoo_config_load(OdooConfig *out, const char *file, const char *project, char *error, size_t cap) {
    if (cap)
        error[0] = 0;
    if (!out || !file || !project)
        return fail(error, cap, "Profil Odoo invalide.");
    FILE *f = fopen(file, "rb");
    if (!f)
        return errno == ENOENT ? false : fail(error, cap, strerror(errno));
    char *raw = malloc(16385);
    if (!raw) {
        fclose(f);
        return fail(error, cap, "Mémoire insuffisante.");
    }
    size_t n = fread(raw, 1, 16385, f);
    bool ok = !ferror(f) && n <= 16384 && !memchr(raw, 0, n);
    fclose(f);
    SjToken tokens[40];
    SjDoc doc;
    OdooConfig next;
    odoo_config_init(&next);
    char actual[BUF_PATH], ignored[LINT_RULES_BYTES];
    if (ok)
        ok = sj_parse(&doc, raw, n, tokens, 40) && sj_is(&doc, 0, SJ_OBJECT);
    int schema = -1, version = -1, path = -1, enabled = -1, rules = -1;
    if (ok) {
        schema = sj_get(&doc, 0, "schema");
        version = sj_get(&doc, 0, "version");
        path = sj_get(&doc, 0, "project");
        enabled = sj_get(&doc, 0, "enabled");
        rules = sj_get(&doc, 0, "ignored");
        ok = schema >= 0 && sj_is(&doc, schema, SJ_NUMBER) && tokens[schema].len == 1 &&
             raw[tokens[schema].start] == '1' &&
             sj_string(&doc, version, next.version, sizeof(next.version)) && !strcmp(next.version, "19.0") &&
             sj_string(&doc, path, actual, sizeof(actual)) && !strcmp(actual, project) &&
             (sj_is(&doc, enabled, SJ_TRUE) || sj_is(&doc, enabled, SJ_FALSE)) &&
             sj_string(&doc, rules, ignored, sizeof(ignored)) &&
             lint_rules_parse(&next.ignored, ignored, error, cap);
        if (ok)
            next.enabled = sj_is(&doc, enabled, SJ_TRUE);
    }
    free(raw);
    if (!ok)
        return fail(error, cap, "Configuration Odoo19 invalide, fichier conservé.");
    *out = next;
    return true;
}
bool odoo_config_save(const OdooConfig *config, const char *file, const char *project, char *error,
                      size_t cap) {
    char local[512] = {0};
    if (!error || !cap) {
        error = local;
        cap = sizeof(local);
    }
    error[0] = 0;
    if (!odoo_config_valid(config) || !file || !project)
        return fail(error, cap, "Configuration Odoo19 invalide.");
    char raw[16385];
    SjOut out;
    sj_out(&out, raw, sizeof(raw));
    sj_add(&out, "{\"schema\":1,\"project\":");
    sj_quote(&out, project);
    sj_add(&out, ",\"version\":\"19.0\",\"enabled\":");
    sj_add(&out, config->enabled ? "true" : "false");
    sj_add(&out, ",\"ignored\":");
    sj_quote(&out, config->ignored.text);
    sj_add(&out, "}\n");
    if (!out.ok)
        return fail(error, cap, "Configuration Odoo trop volumineuse.");
    Buffer *buffer = calloc(1, sizeof(*buffer));
    if (!buffer)
        return fail(error, cap, "Mémoire insuffisante.");
    bool ok = buf_init(buffer, raw, "odoo.json");
    if (ok)
        ok = buf_save(buffer, file, error, cap);
    else
        fail(error, cap, "Mémoire insuffisante.");
    buf_free(buffer);
    free(buffer);
    return ok;
}
