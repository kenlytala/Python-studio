#define _POSIX_C_SOURCE 200809L
#include "graphics.h"
#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>

#ifndef PRISME_OPENGL_VERSION
#define PRISME_OPENGL_VERSION "2.1"
#endif

const char *graphics_version(void) {
    return PRISME_OPENGL_VERSION;
}

bool graphics_request_software(void) {
#ifdef __linux__
    /* Mesa/llvmpipe fallback. This does not fake a supported OpenGL version. */
    if (setenv("LIBGL_ALWAYS_SOFTWARE", "1", 1) != 0) {
        perror("Impossible d'activer le rendu logiciel Mesa");
        return false;
    }
    return true;
#else
    fprintf(stderr, "--software est disponible sous Linux avec Mesa/llvmpipe.\n");
    return false;
#endif
}

bool graphics_open(int width, int height, const char *title, bool msaa, bool verbose) {
    SetTraceLogLevel(verbose ? LOG_INFO : LOG_WARNING);
    unsigned flags = FLAG_WINDOW_RESIZABLE;
    /* Do not require a multisampled GLX framebuffer on old/virtual displays. */
    if (msaa)
        flags |= FLAG_MSAA_4X_HINT;
    SetConfigFlags(flags);
    if (verbose) {
        fprintf(stderr, "PYTHON STUDIO: backend OpenGL %s ; MSAA %s\n", graphics_version(),
                msaa ? "4x" : "désactivé");
    }
    /* The vendored raylib is guarded in CMake: InitWindow must return without
       touching GPU functions when GLFW cannot create a context. */
    InitWindow(width, height, title);
    if (!IsWindowReady()) {
        fprintf(stderr,
                "\nPYTHON STUDIO : impossible de créer le contexte OpenGL %s.\n"
                "Aucun rendu n'a été démarré ; arrêt propre (code %d).\n"
                "Essayez : ./bin/python-studio --safe-mode\n"
#ifdef __linux__
                "Avec Mesa : ./bin/python-studio --safe-mode --software\n"
                "Diagnostic : glxinfo -B\n"
#endif
                "Si nécessaire, recompilez dans un dossier neuf avec\n"
                "  -DPRISME_OPENGL_VERSION=2.1\n",
                graphics_version(), PRISME_GRAPHICS_ERROR);
        return false;
    }
    return true;
}
