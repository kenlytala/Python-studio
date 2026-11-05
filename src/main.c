#define _POSIX_C_SOURCE 200809L
#include "app.h"
#include "graphics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(void) {
    puts("Python Studio — édition C " PRISME_VERSION " / C11 + raylib — éditeur pour Python 3\n"
         "Usage : python-studio [fichier.py] [options]\n\n"
         "  --project DOSSIER   Dossier de l'explorateur\n"
         "  --python CHEMIN     Interpréteur externe (défaut : python3 / python)\n"
         "  --import-dir DOSSIER  Import PyLint / complétion statique (répétable, max. 8)\n"
         "  --assets DOSSIER    Emplacement des polices\n"
         "  --no-session        Sans lecture/écriture des sessions et brouillons\n"
         "  --split             Deux fichiers côte à côte (F7 / Ctrl+\\)\n"
         "  --zen               Replier explorateur, menu et console (F9)\n"
         "  --fullscreen        Démarrer en plein écran (F11)\n"
         "  --no-effects        Désactiver les effets visuels\n"
         "  --no-lint           Analyse Python manuelle uniquement (F4)\n"
         "  --safe-mode         Sans effets/MSAA, 30 FPS, analyse manuelle\n"
         "  --software          Rendu logiciel Mesa/llvmpipe sous Linux\n"
         "  --msaa              MSAA 4x optionnel (désactivé par défaut)\n"
         "  --verbose-gl        Afficher le diagnostic de démarrage OpenGL\n"
         "  --demo              Exécuter explicitement le fichier à l'ouverture\n"
         "  --windowed          Démarrer dans une fenêtre non maximisée\n"
         "  --width N --height N  Taille explicite en mode fenêtre\n"
         "  --frames N --screenshot image.png   Test graphique à durée limitée\n"
         "  --version           Afficher la version\n"
         "  --help              Afficher cette aide");
    printf("\nBackend compilé : OpenGL %s\n", graphics_version());
}
int main(int argc, char **argv) {
    AppOptions options = {0};
    options.width = 1440;
    options.height = 900;
    for (int i = 1; i < argc; i++) {
        const char *s = argv[i];
        if (!strcmp(s, "--version")) {
            puts("Python Studio — édition C " PRISME_VERSION);
            return 0;
        }
        if (!strcmp(s, "--help") || !strcmp(s, "-h")) {
            usage();
            return 0;
        }
        if (!strcmp(s, "--no-session")) {
            options.no_session = true;
            continue;
        }
        if (!strcmp(s, "--windowed")) {
            options.windowed = true;
            continue;
        }
        if (!strcmp(s, "--zen")) {
            options.zen = true;
            continue;
        }
        if (!strcmp(s, "--split")) {
            options.split = true;
            continue;
        }
        if (!strcmp(s, "--fullscreen")) {
            options.fullscreen = true;
            continue;
        }
        if (!strcmp(s, "--no-lint")) {
            options.no_lint = true;
            continue;
        }
        if (!strcmp(s, "--no-effects")) {
            options.no_effects = true;
            continue;
        }
        if (!strcmp(s, "--safe-mode") || !strcmp(s, "--compat")) {
            options.safe_mode = true;
            continue;
        }
        if (!strcmp(s, "--msaa")) {
            options.msaa = true;
            continue;
        }
        if (!strcmp(s, "--verbose-gl")) {
            options.verbose_gl = true;
            continue;
        }
        if (!strcmp(s, "--software")) {
            options.software = true;
            continue;
        }
        if (!strcmp(s, "--demo")) {
            options.demo = true;
            continue;
        }
        if (!strcmp(s, "--project") || !strcmp(s, "--python") || !strcmp(s, "--import-dir") ||
            !strcmp(s, "--assets") || !strcmp(s, "--screenshot") || !strcmp(s, "--width") ||
            !strcmp(s, "--height") || !strcmp(s, "--frames")) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Valeur manquante pour %s\n", s);
                return 2;
            }
            const char *value = argv[++i];
            if (!strcmp(s, "--project"))
                options.project = value;
            else if (!strcmp(s, "--python"))
                options.python = value;
            else if (!strcmp(s, "--import-dir")) {
                if (options.import_count == IMPORT_PATHS_MAX) {
                    fprintf(stderr, "Limite : huit dossiers d’import PyLint.\n");
                    return 2;
                }
                options.import_dirs[options.import_count++] = value;
            } else if (!strcmp(s, "--assets"))
                options.assets = value;
            else if (!strcmp(s, "--screenshot"))
                options.screenshot = value;
            else {
                char *end;
                long n = strtol(value, &end, 10);
                if (*end || n < 0 || n > 32768) {
                    fprintf(stderr, "Valeur numérique invalide : %s\n", value);
                    return 2;
                }
                if (!strcmp(s, "--width")) {
                    options.width = (int)n;
                    options.windowed = true;
                } else if (!strcmp(s, "--height")) {
                    options.height = (int)n;
                    options.windowed = true;
                } else
                    options.frames = (int)n;
            }
        } else if (s[0] == '-') {
            fprintf(stderr, "Option inconnue : %s\n", s);
            return 2;
        } else if (!options.file)
            options.file = s;
        else {
            fprintf(stderr, "Un seul fichier peut être indiqué au lancement.\n");
            return 2;
        }
    }
    if (options.width < 960)
        options.width = 960;
    if (options.height < 640)
        options.height = 640;
    if (options.screenshot && !options.frames)
        options.frames = 120;
#ifdef __linux__
    if (!getenv("DISPLAY") && !getenv("WAYLAND_DISPLAY")) {
        fprintf(stderr, "Un bureau graphique est nécessaire (X11/XWayland).\n"
                        "Le rendu logiciel ne remplace pas un serveur d’affichage.\n");
        return PRISME_GRAPHICS_ERROR;
    }
#endif
    if (options.software && !graphics_request_software())
        return 2;
    return app_run(&options);
}
