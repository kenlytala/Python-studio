#ifndef PYTHON_STUDIO_XML_PATHS_H
#define PYTHON_STUDIO_XML_PATHS_H
#include "import_paths.h"

/* Reuse the bounded, project-bound ordered-path storage format, in a SEPARATE
   namespace. These paths are never passed to Python, PyLint or F5. */
#define XML_PATHS_MAX IMPORT_PATHS_MAX
typedef ImportPaths XmlPaths;
bool xml_paths_file(char *out, size_t cap, const char *settings, const char *project);
bool xml_paths_load(XmlPaths *out, const char *file, const char *project, char *error, size_t cap);
bool xml_paths_save(const XmlPaths *paths, const char *file, const char *project, char *error, size_t cap);
bool xml_paths_add(XmlPaths *paths, const char *path, char *error, size_t cap);
bool xml_paths_remove(XmlPaths *paths, int index);
bool xml_paths_move(XmlPaths *paths, int index, int direction);
#endif
