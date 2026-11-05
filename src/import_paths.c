#include "import_paths.h"
#include "files.h"
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PROFILE_LIMIT (2u * BUF_PATH * (IMPORT_PATHS_MAX + 1u) + 256u)

static bool fail(char *error, size_t cap, const char *text) {
    if (cap)
        snprintf(error, cap, "%s", text);
    return false;
}
static bool valid_path(const char *path) {
    if (!path)
        return false;
    size_t n = 0;
    while (n < BUF_PATH && path[n])
        n++;
    if (!n || n == BUF_PATH || !utf8_valid(path, n))
        return false;
    return path[0] == '/' || (n >= 3 && isalpha((unsigned char)path[0]) && path[1] == ':' &&
                              (path[2] == '/' || path[2] == '\\'));
}
bool import_paths_valid(const ImportPaths *paths) {
    if (!paths || paths->count < 0 || paths->count > IMPORT_PATHS_MAX)
        return false;
    for (int i = 0; i < paths->count; i++) {
        if (!valid_path(paths->paths[i]))
            return false;
        for (int j = 0; j < i; j++)
            if (!strcmp(paths->paths[i], paths->paths[j]))
                return false;
    }
    return true;
}
bool import_paths_add(ImportPaths *paths, const char *path, char *error, size_t cap) {
    if (cap)
        error[0] = 0;
    if (!import_paths_valid(paths) || !valid_path(path))
        return fail(error, cap, "Chemin d’import absolu UTF-8 requis (moins de 4096 octets).");
    for (int i = 0; i < paths->count; i++)
        if (!strcmp(paths->paths[i], path))
            return fail(error, cap, "Ce dossier d’import est déjà présent.");
    if (paths->count == IMPORT_PATHS_MAX)
        return fail(error, cap, "Limite : huit dossiers d’import par projet.");
    memcpy(paths->paths[paths->count++], path, strlen(path) + 1);
    return true;
}
bool import_paths_remove(ImportPaths *paths, int index) {
    if (!import_paths_valid(paths) || index < 0 || index >= paths->count)
        return false;
    if (index + 1 < paths->count)
        memmove(paths->paths[index], paths->paths[index + 1], (size_t)(paths->count - index - 1) * BUF_PATH);
    paths->count--;
    memset(paths->paths[paths->count], 0, BUF_PATH);
    return true;
}
bool import_paths_file(char *out, size_t cap, const char *settings, const char *project) {
    if (!out || !cap || !settings || !valid_path(project))
        return false;
    char parent[BUF_PATH];
    if (!fs_parent(settings, parent))
        return false;
    uint64_t hash = UINT64_C(14695981039346656037);
    for (const unsigned char *p = (const unsigned char *)project; *p; p++) {
        hash ^= *p;
        hash *= UINT64_C(1099511628211);
    }
    int n = snprintf(out, cap, "%s/imports-%016llx.cfg", parent, (unsigned long long)hash);
    if (n < 0 || (size_t)n >= cap) {
        out[0] = 0;
        return false;
    }
    return true;
}
static char *encode(char *out, const char *text) {
    static const char digits[] = "0123456789abcdef";
    for (const unsigned char *p = (const unsigned char *)text; *p; p++) {
        *out++ = digits[*p >> 4];
        *out++ = digits[*p & 15];
    }
    *out = 0;
    return out;
}
static int nibble(unsigned char c) {
    return c >= '0' && c <= '9'   ? c - '0'
           : c >= 'a' && c <= 'f' ? c - 'a' + 10
           : c >= 'A' && c <= 'F' ? c - 'A' + 10
                                  : -1;
}
static bool decode(char *out, const char *hex) {
    size_t n = strlen(hex);
    if (n % 2 || n / 2 >= BUF_PATH)
        return false;
    for (size_t i = 0; i < n; i += 2) {
        int a = nibble((unsigned char)hex[i]), b = nibble((unsigned char)hex[i + 1]);
        if (a < 0 || b < 0 || !(a * 16 + b))
            return false;
        out[i / 2] = (char)(a * 16 + b);
    }
    out[n / 2] = 0;
    return valid_path(out);
}
bool import_paths_load(ImportPaths *out, const char *file, const char *project, char *error, size_t cap) {
    if (cap)
        error[0] = 0;
    if (!out || !file || !valid_path(project))
        return fail(error, cap, "Profil d’import invalide.");
    FILE *f = fopen(file, "rb");
    if (!f)
        return errno == ENOENT ? false : fail(error, cap, strerror(errno));
    char *text = malloc(PROFILE_LIMIT + 1u);
    ImportPaths *next = calloc(1, sizeof(*next));
    if (!text || !next) {
        free(text);
        free(next);
        fclose(f);
        return fail(error, cap, "Mémoire insuffisante pour les dossiers d’import.");
    }
    size_t n = fread(text, 1, PROFILE_LIMIT + 1u, f);
    bool ok = !ferror(f) && n <= PROFILE_LIMIT;
    fclose(f);
    if (ok) {
        ok = !memchr(text, 0, n);
        text[n] = 0;
    }
    const char header[] = "PRISME-IMPORTS\t1\nPROJECT\t";
    if (ok)
        ok = n >= sizeof(header) && !strncmp(text, header, sizeof(header) - 1);
    char decoded[BUF_PATH], *p = text + sizeof(header) - 1;
    if (ok) {
        char *end = strchr(p, '\n');
        ok = end != NULL;
        if (ok) {
            *end = 0;
            ok = decode(decoded, p) && !strcmp(decoded, project);
            p = end + 1;
        }
    }
    while (ok && *p) {
        char *end = strchr(p, '\n');
        if (!end) {
            ok = false;
            break;
        }
        *end = 0;
        ok =
            !strncmp(p, "PATH\t", 5) && decode(decoded, p + 5) && import_paths_add(next, decoded, error, cap);
        p = end + 1;
    }
    if (ok)
        *out = *next;
    free(next);
    free(text);
    return ok ? true
              : fail(error, cap,
                     "Profil d’import invalide ou associé à un autre projet ; aucun chemin chargé.");
}
bool import_paths_save(const ImportPaths *paths, const char *file, const char *project, char *error,
                       size_t cap) {
    if (cap)
        error[0] = 0;
    if (!import_paths_valid(paths) || !valid_path(project) || !file)
        return fail(error, cap, "Profil d’import invalide.");
    char *text = malloc(PROFILE_LIMIT + 1u);
    Buffer *buffer = calloc(1, sizeof(*buffer));
    if (!text || !buffer) {
        free(text);
        free(buffer);
        return fail(error, cap, "Mémoire insuffisante pour enregistrer les dossiers d’import.");
    }
    char *p = text;
    const char *header = "PRISME-IMPORTS\t1\nPROJECT\t";
    memcpy(p, header, strlen(header));
    p += strlen(header);
    p = encode(p, project);
    *p++ = '\n';
    for (int i = 0; i < paths->count; i++) {
        memcpy(p, "PATH\t", 5);
        p += 5;
        p = encode(p, paths->paths[i]);
        *p++ = '\n';
    }
    *p = 0;
    bool ok = buf_init(buffer, text, "imports.cfg");
    if (ok)
        ok = buf_save(buffer, file, error, cap);
    else
        fail(error, cap, "Mémoire insuffisante pour le profil d’import.");
    buf_free(buffer);
    free(buffer);
    free(text);
    return ok;
}
