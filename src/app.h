#ifndef PRISME_APP_H
#define PRISME_APP_H
#include <stdbool.h>
#include "import_paths.h"
#ifndef PRISME_VERSION
#define PRISME_VERSION "1.9.0"
#endif
typedef struct {
    const char *file, *project, *assets, *python, *screenshot;
    const char *import_dirs[IMPORT_PATHS_MAX];
    int import_count;
    int width, height, frames;
    bool no_session, no_effects, no_lint, demo, safe_mode, msaa, verbose_gl, software, split, fullscreen, zen,
        windowed;
} AppOptions;
int app_run(const AppOptions *options);
#endif
