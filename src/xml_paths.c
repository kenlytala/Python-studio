#include "xml_paths.h"
#include "files.h"
#include <stdio.h>
#include <string.h>

static bool fail(char *error, size_t cap, const char *text) {
    if (cap)
        snprintf(error, cap, "%s", text);
    return false;
}
bool xml_paths_file(char *out, size_t cap, const char *settings, const char *project) {
    char imports[BUF_PATH], parent[BUF_PATH];
    if (!out || !cap || !import_paths_file(imports, sizeof(imports), settings, project) ||
        !fs_parent(imports, parent))
        return false;
    const char *name = fs_basename(imports);
    if (strncmp(name, "imports-", 8))
        return false;
    int n = snprintf(out, cap, "%s/xml-roots-%s", parent, name + 8);
    if (n < 0 || (size_t)n >= cap) {
        out[0] = 0;
        return false;
    }
    return true;
}
bool xml_paths_load(XmlPaths *out, const char *file, const char *project, char *error, size_t cap) {
    bool ok = import_paths_load(out, file, project, error, cap);
    if (!ok && cap && *error)
        fail(error, cap,
             "Dossiers XML non chargés : profil illisible, invalide ou associé à un autre projet.");
    return ok;
}
bool xml_paths_save(const XmlPaths *paths, const char *file, const char *project, char *error, size_t cap) {
    bool ok = import_paths_save(paths, file, project, error, cap);
    if (!ok)
        fail(error, cap, "Enregistrement des dossiers XML impossible ; liste précédente conservée.");
    return ok;
}
bool xml_paths_add(XmlPaths *paths, const char *path, char *error, size_t cap) {
    if (cap)
        *error = 0;
    if (!import_paths_valid(paths) || !path)
        return fail(error, cap, "Dossier XML invalide.");
    for (int i = 0; i < paths->count; i++)
        if (!strcmp(paths->paths[i], path))
            return fail(error, cap, "Ce dossier XML est déjà présent.");
    if (paths->count == XML_PATHS_MAX)
        return fail(error, cap, "Limite : huit dossiers XML par projet.");
    if (!import_paths_add(paths, path, error, cap))
        return fail(error, cap, "Chemin XML absolu UTF-8 requis (4095 octets maximum).");
    return true;
}
bool xml_paths_remove(XmlPaths *paths, int index) {
    return import_paths_remove(paths, index);
}
bool xml_paths_move(XmlPaths *paths, int index, int direction) {
    if (!import_paths_valid(paths) || (direction != -1 && direction != 1) || index < 0 ||
        index >= paths->count || index + direction < 0 || index + direction >= paths->count)
        return false;
    char temp[BUF_PATH];
    memcpy(temp, paths->paths[index], BUF_PATH);
    memcpy(paths->paths[index], paths->paths[index + direction], BUF_PATH);
    memcpy(paths->paths[index + direction], temp, BUF_PATH);
    return true;
}
