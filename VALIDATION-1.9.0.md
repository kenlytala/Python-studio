# Python Studio 1.9.0 — validation de l’édition C

Date : **1 novembre 2026**. Véritable C11/raylib : contrôleurs, SSH, sessions et **XML via libxml2 native**, pas un wrapper ou parseur Python. Source SHA256 : **`0771bf388cb5a64473c443f34773610e8284519021e2ca1934424a59b33b083c`**.

## Résultats réellement exécutés

- **Production Release : 26/26 CTest**, zéro échec, 9.77 s, dans l’unique dossier courant **`build-1.9.0`** (35 Mio environ). Pas de rétention de l’ancien build de production.
- **104 assertions XML cœur/worker**, **149 assertions des vrais contrôleurs XML**, **240 états du vrai sélecteur XML sans fenêtre** : liste vide/chargement/erreur/filtre, première/milieu/dernière occurrence jusqu’à 4096 IDs, toutes tailles testées. Colonnes icône/texte du menu contextuel vérifiées, notamment Coller ici/Supprimer ; le placement C déjà conforme a été préservé.
- Parseur sûr borné : refus préalable et linéaire des DTD, pas de chargement externe/réseau, UTF-8 sans mojibake même avec une déclaration d’encodage contradictoire, occurrences dupliquées exactes, espaces de noms/contenus mixtes/espaces préservés, arch vides avec fermeture explicite. Snapshot source en lecture seule, annulation sans joindre un appel système dans le thread UI et garde SSH à chemins immuables/génération épinglée, sans copier la session d’authentification.
- **Oracle indépendant + parité Python/C : 48 documents valides, 291 identifiants/fragments et 10 documents invalides**, comprenant chaque ID/fragment des vraies vues Sale Odoo 18/19. Il s’agit de lecture XML statique, pas d’une validation ORM/base de données.
- **12 cas SSH chiffrés** via le vrai contrôleur/helper C sur socket Unix local, dont le nouveau parcours XML depuis le vrai montage SSHFS : source inchangée, champs/arch vides et une annulation. Les méthodes d’authentification, refus, CRUD, perte/reconnexion et sessions sont conservés.
- Sessions : **197 assertions / six groupes**. Les contrôleurs réels de fermeture/brouillons, conflits, absence de secrets sérialisés et reprise hors ligne restent couverts.
- **ASan+UBSan : 9/9 groupes ciblés**, 7.75 s, avec `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:strict_string_checks=1` et `UBSAN_OPTIONS=halt_on_error=1`. XML cœur/worker/contrôleurs/oracle, sessions, fichiers, profils/gardes SSH, les 12 cas SSH chiffrés, vues/menus et contrôleurs généraux ; zéro défaut/fuite signalé. Ce n’est pas une affirmation d’exécution de tous les 26 groupes sous ASan.
- **Zéro avertissement du code propre** ; quatre avertissements GCC amont existants dans `raylib 5.5/src/external/jar_mod.h:370` lors du build froid. Les deux anciens F401 Ruff de `test_completion_bridge.py` et `lint_bridge.py` restent inchangés ; les nouveaux scripts XML sont propres.
- `./lancer.sh --version` et `./bin/python-studio --version` réellement appelés : **Python Studio — édition C 1.9.0**. Binaires ELF x86_64 authentiques, installés après `strip`, mode **0755** ; SHA256/taille/ABI dans `tests/results-1.9.0/binaries.json`.

## Construction et dépendances

```bash
cmake -S . -B build-1.9.0 -G 'Unix Makefiles' \
  -DCMAKE_BUILD_TYPE=Release -DPRISME_OPENGL_VERSION=2.1 \
  -DPRISME_TEST_PYTHON=/chemin/qa/bin/python \
  -DPRISME_TEST_FALLBACK_PYTHON=/chemin/qa/bin/python \
  -DPRISME_REQUIRE_PYLINT_TESTS=ON -DPRISME_SSH_INTEGRATION=ON
cmake --build build-1.9.0 --parallel 4
ctest --test-dir build-1.9.0 --output-on-failure
```

La QA a réutilisé les sources raylib 5.5 épinglées et déjà contrôlées, avec une copie possédée par le build de production (override `FETCHCONTENT_SOURCE_DIR_RAYLIB`). Le téléchargement amont d’origine est épinglé par SHA256 `aea98ecf5bc5c5e0b789a76de0083a21a70457050ea4cc2aec7566935f5e258e` ; les trois correctifs graphiques précédents sont vérifiés et conservés. Aucune bibliothèque raylib système non corrigée ne les remplace.

**Nouvelle dépendance : libxml2 2.9+** (`libxml2-dev` pour compiler). La QA lie la **2.9.14** du système ; la bibliothèque n’est pas distribuée. Le précompilé exige `libxml2.so.2`, OpenGL/bureau et jusqu’à **GLIBC_2.38** pour les deux programmes graphiques ; le helper exige **GLIBC_2.34**. Sur une distribution incompatible, reconstruire localement. Le backend portable Python reste indépendant et inchangé (cible glibc 2.17).

Les bridges de lint/complétion sont inchangés octet pour octet par rapport à la 1.8.0. Python est utilisé pour les programmes/services locaux existants et certains tests, **pas pour implémenter le XML C**. Les fixtures réelles, non embarquées, sont épinglées dans `tests/xml-fixtures.json` et téléchargées seulement sur demande explicite.

## Limites

Pas de test physique GNOME/GPU/plein écran, Windows/macOS, ORM ou base de données. Les sources Windows/macOS ne sont pas présentées comme validées. SSHFS intégré reste Linux ; aucune exécution Python distante n’est ajoutée. Les gardes de montage et lectures bornées ne constituent pas une sandbox générale. Voir `GUIDE-XML.md` pour les appels système dont l’arrêt immédiat n’est pas garanti ; le worker est rejoint avant libération SSH lors de la sortie C.

Les sessions sont privées mais **en clair** ; le texte inséré devient un brouillon normal et peut donc être journalisé. `--no-session` est conservé. L’index/filtre XML n’est pas sérialisé, les secrets SSH non plus. Aucune garantie des dernières secondes après crash ni de durabilité après échec de synchronisation. Voir `GUIDE-SESSIONS.md` et `GUIDE-SSH.md`.

## Acceptation après extraction

Reconstruction indépendante des sources extraites puis **26/26 CTest**, 10.41 s, dont les 12 cas SSH réels. Le précompilé extrait confirme lui aussi le nom/version sans fenêtre.

Parité XML après extraction : **48 documents valides / 291 fragments / 10 invalides**, y compris les deux fixtures Sale véritables, avec le parseur Python extrait et le C recompilé extrait. Les empreintes des sources fonctionnelles testées, binaires précompilés, modes et payloads sont comparées au manifeste final. Les fichiers modifiés après les tests ne sont que les rapports d’acceptation et leurs inventaires.

Aucun aperçu, capture, serveur web ou service persistant n’a été créé. Les archives historiques et leurs sidecars restent inchangés. Les rapports/manifestes plus anciens décrivent seulement leur livraison historique. Gzip/tar à contenu figé, dates/UID/GID normalisés ; les sommes SHA256 des archives sont dans leurs fichiers `.sha256` externes pour éviter une référence circulaire.
