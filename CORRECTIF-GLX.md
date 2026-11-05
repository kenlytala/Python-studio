# Correctif du démarrage GLX (introduit en 1.0.1, conservé en 1.1.0)

## Symptôme et cause

L'ancienne compilation demandait OpenGL 3.3 et le MSAA 4x. Une création de contexte refusée pouvait produire :

```text
WARNING: GLFW: Error: 65543 Description: GLX: Failed to create context: GLXBadFBConfig
Erreur de segmentation
```

Ce message n'identifie pas à lui seul la cause du refus : pilote, serveur X, session distante, machine virtuelle, configuration du framebuffer, etc.

Deux chemins dangereux ont été identifiés dans les sources raylib 5.5 utilisées :

1. Après un `glfwCreateWindow()` nul, le chemin fenêtré pouvait accéder à la fenêtre via `GetCurrentMonitor()` avant le contrôle d'erreur existant.
2. `InitWindow()` ignorait le résultat de `InitPlatform()` et continuait vers l'initialisation du GPU et de la police par défaut.

Un simple `IsWindowReady()` dans l'éditeur, après `InitWindow()`, arrivait donc trop tard.

## Modifications livrées

- Backend raylib **OpenGL 2.1 par défaut** ; OpenGL 3.3 reste une option de compilation.
- MSAA 4x **optionnel**, jamais requis au démarrage par défaut.
- Contrôle immédiat du résultat de `glfwCreateWindow()` dans les deux branches de création GLFW.
- Arrêt de `InitWindow()` si la plateforme n'a pas été initialisée ; pas d'appel GPU sur ce chemin.
- Protection de `CloseWindow()` si aucune fenêtre n'a été initialisée.
- Les deux programmes C, `prisme` et `constellation`, utilisent le même démarrage contrôlé et renvoient **3** en cas d'échec graphique.
- Les sources raylib officielles sont épinglées et corrigées par CMake ; pas de liaison involontaire avec une raylib système non corrigée.

Le patch est appliqué par `cmake/PatchRaylibInit.cmake`, de manière idempotente. La configuration s'arrête si les emplacements attendus ne sont pas trouvés. Voir `third_party/RAYLIB-PATCH.md` pour les modifications et la licence.

## Lancer la version corrigée

Extrayez la **nouvelle archive**, pas uniquement un ancien binaire :

```bash
cd prisme-c
chmod +x bin/prisme bin/constellation
./bin/prisme --safe-mode
```

Si la session ou le pilote matériel reste problématique, sous Linux avec Mesa installé :

```bash
./bin/prisme --safe-mode --software --verbose-gl
```

- `--safe-mode` désactive effets, minimap et MSAA et limite à 30 FPS, sans réécrire les préférences. Il prime sur `--msaa`.
- `--software` définit `LIBGL_ALWAYS_SOFTWARE=1` pour ce processus Linux. Cela demande un pilote logiciel Mesa, sans falsifier les capacités OpenGL. Ce n'est pas un mode sans serveur X.
- `--verbose-gl` affiche le backend compilé et le diagnostic raylib/GLFW/OpenGL.
- `--no-effects` ne modifie que le dessin : seul, il ne résout pas un problème de création de contexte.
- `--msaa` permet d'essayer le MSAA 4x sur un système compatible.

Le binaire Linux fourni est x86-64 et demande glibc >= 2.38. Sur une autre architecture ou une distribution plus ancienne, recompilez :

```bash
cmake -S . -B build-compat -DCMAKE_BUILD_TYPE=Release -DPRISME_OPENGL_VERSION=2.1
cmake --build build-compat --parallel
./build-compat/prisme --safe-mode
```

`./lancer.sh --safe-mode` effectue cette compilation. Le backend n'est pas interchangeable à l'exécution : `--safe-mode` ne transforme pas un ancien binaire 3.3 en binaire 2.1.

Si aucun contexte n'est disponible, un message explicite et le code 3 sont normaux. Dans ce cas, transmettez `glxinfo -B` et le journal `--verbose-gl` ; n'utilisez pas `MESA_GL_VERSION_OVERRIDE` pour prétendre supporter une version supérieure.

## Vérifications effectuées sous Linux

Environnement de test : Xvfb, Mesa 25.0.7, llvmpipe LLVM 19.1.7. Ce pilote annonce normalement OpenGL 4.5. Les restrictions ci-dessous sont des **conditions de test**, pas des options de lancement recommandées.

| Cas | Résultat observé |
|---|---|
| Ancien binaire 3.3, Mesa limité à 2.1 | Message exact `GLXBadFBConfig`, segfault, sortie 139 |
| Nouveau binaire 2.1, Mesa limité à 2.1 | Démarrage et rendu natif, sortie 0 |
| Nouveau binaire 2.1, effets actifs, exécution Python et capture | Fonctionne, sortie 0 |
| Nouveau binaire 3.3, Mesa limité à 2.1 | Même refus GLX, arrêt propre, sortie 3 |
| Démo C compilée en 3.3, Mesa limité à 2.1 | Arrêt propre, sortie 3 |
| Nouveau binaire 2.1, Mesa limité à 1.0 | Refus GLX, arrêt propre, sortie 3 |
| Affichage X invalide, éditeur et démo | Arrêt propre, sortie 3 |
| Éditeur sans variable d'affichage | Message explicite, sortie 3 |
| `--safe-mode --msaa` | MSAA reste désactivé |
| Mode de secours avec préférences préexistantes | Fichier de préférences inchangé |
| MSAA optionnel sur le pilote de test | Démarrage et rendu, sortie 0 |
| Noyau C | 20/20 tests réussis, dont 500 modifications aléatoires |
| CTest Release et ASan/UBSan | 3/3 cibles réussies, y compris les deux échecs d'affichage contrôlés |
| Démarrage et 30 images en 2.1 sous ASan/UBSan | Sortie 0, pas de diagnostic mémoire ; détection des fuites du pilote désactivée pour ce rendu |
| Parcours graphique Release via aperçu natif | Édition/collage UTF-8, sauvegarde, F5, recherche, menus, complétion, annuler/rétablir réussis ; 0 erreur JavaScript du pont |

Pour réexécuter les tests sans affichage :

```bash
cmake -S . -B build-test -DPRISME_OPENGL_VERSION=2.1
cmake --build build-test --parallel
ctest --test-dir build-test --output-on-failure
```

Pour vérifier le rendu 2.1 sous Xvfb/Mesa :

```bash
LIBGL_ALWAYS_SOFTWARE=1 MESA_GL_VERSION_OVERRIDE=2.1 \
  xvfb-run -a -s '-screen 0 1440x900x24 +extension GLX' \
  ./build-test/prisme --frames 30 --screenshot test-gl21.png
```

La validation ne couvre pas tous les pilotes ni toutes les sessions distantes. Windows et macOS restent non testés dans cet environnement.
