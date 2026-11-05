# Python Studio 1.8.0 — validation de l’édition C

Date : **1 novembre 2026**. Véritable éditeur C11/raylib, transport/helper/contrôleurs/dialogues/sessions en C, pas un lanceur de l’éditeur Python. Source SHA256 : **`14bbc2d48db56d642d1e3da31958a02ebaf9a91daff6ad17a5d7e74f8f4c90ff`**.

## Résultats exécutés

- **Production Release : 23/23 CTest**, zéro échec, 9.99 s, dans l’unique dossier de production courant `build-1.8.0`.
- Nouveau groupe de sessions : **197 assertions / six groupes**, formats bornés/intégrité, onglets et état, sans-titre et modifications, conflits/manquants/FIFO, rafraîchissement propre/abandon, corruption/bornes, liens/concurrence/préservation de l’ancien journal, reprise SSH hors ligne et `--no-session`.
- Les tests des contrôleurs réels couvrent capture/restauration, absence de sérialisation du champ d’authentification, annulation de fermeture, refus de fermeture SSH révoquant l’abandon, puis conservation des brouillons avant sortie.
- **11 cas SSH/SFTP réels chiffrés** exécutés par le vrai contrôleur C et le helper C sur un socket Unix local : méthodes d’authentification, confiance/refus, CRUD, perte/reconnexion et reprise de session. Le nouveau cas restaure un journal hors ligne sans génération/propriété de montage, refuse l’enregistrement du placeholder, vérifie explicitement le fichier sur SSH puis conserve séparément un brouillon si le fichier distant a changé.
- **ASan+UBSan : 6/6 groupes ciblés**, 9.06 s, avec `detect_leaks=1:halt_on_error=1:strict_string_checks=1` et `UBSAN_OPTIONS=halt_on_error=1`. Sessions, opérations de fichiers, profils/gardes SSH, les 11 cas SSH chiffrés, diagnostics et contrôleurs de fermeture ; aucun défaut ni fuite signalé.
- **Zéro avertissement du code propre**. Quatre avertissements GCC existants dans `raylib 5.5/src/external/jar_mod.h:370`, explicitement conservés comme avertissements amont.
- `./lancer.sh --version` réellement exécuté après compactage du dossier de production ; `./bin/python-studio --version` confirme Python Studio — édition C 1.8.0. Les binaires installés sont identiques au résultat de production après `strip`, modes 0755.

Configuration exécutée :

```bash
cmake -S . -B build-1.8.0 -G 'Unix Makefiles' \
  -DCMAKE_BUILD_TYPE=Release -DPRISME_OPENGL_VERSION=2.1 \
  -DPRISME_TEST_PYTHON=/chemin/qa/bin/python \
  -DPRISME_TEST_FALLBACK_PYTHON=/chemin/qa/bin/python \
  -DPRISME_REQUIRE_PYLINT_TESTS=ON -DPRISME_SSH_INTEGRATION=ON
cmake --build build-1.8.0 --parallel 4
ctest --test-dir build-1.8.0 --output-on-failure
```

Les essais réels nécessitent SSHFS/OpenSSH/SFTP et Paramiko **uniquement pour la QA**, pas pour l’éditeur C. Les deux bridges sont inchangés octet pour octet par rapport à l’archive native 1.7.0.

## Distribution et ABI

`bin/python-studio`, `bin/python-studio-ssh-helper` et `bin/constellation` sont de vrais exécutables ELF x86_64, pas des wrappers Python. Les deux programmes graphiques exigent jusqu’à **GLIBC_2.38** ; le helper, **GLIBC_2.34**. OpenGL 2.1 est le backend par défaut. Pour une distribution plus ancienne, reconstruire localement. Cela n’altère pas le backend portable de l’autre édition, resté à sa cible glibc 2.17.

Les sources raylib 5.5 sont épinglées par SHA256 `aea98ecf5bc5c5e0b789a76de0083a21a70457050ea4cc2aec7566935f5e258e`. Les trois correctifs graphiques existants sont conservés. Le build de production est compacté en retirant seulement les médias/exemples/projets amont inutilisés et l’archive de téléchargement ; ses sources/bibliothèques/exécutables utiles restent présents. L’archive distribuée exclut les builds/QA/environnements virtuels et contient les sources, assets, binaires et tests.

## Limites

Les essais sans fenêtre ne prouvent ni saisie physique sous GNOME, ni plein écran/GPU réel, ni Windows/macOS, ni ORM/base de données. SSHFS intégré reste Linux et ne devient pas un Python distant. F5 exécute le Python local avec les permissions de l’utilisateur ; aucune sandbox générale n’est promise.

La session conserve les textes en clair, dans un répertoire privé ; aucune authentification ni propriété de montage n’est sérialisée. La capture périodique ne garantit pas les dernières secondes après un arrêt brutal, et une erreur de synchronisation peut laisser la durabilité incertaine. Les journaux séparés ne sont pas fusionnés. Voir `GUIDE-SESSIONS.md`.

Aucun aperçu, capture, serveur web ou service persistant n’a été créé. Un seul build de production est conservé ; les archives historiques restent inchangées. Les anciens rapports et manifestes ne valident que leur propre livraison historique.

## Acceptation après extraction

Recompilation propre depuis les sources extraites, puis **23/23 CTest**, 11.34 s, dont les 11 cas SSH chiffrés. L’exécutable précompilé extrait confirme également le nouveau nom/version sans ouvrir de fenêtre.

Les payloads finaux sont comparés aux empreintes/modes du manifeste et aux fichiers fonctionnels testés ; génération gzip/tar reproductible à contenu figé, UID/GID 0 et dates normalisées. Les SHA256 des archives se trouvent dans leurs fichiers `.sha256` externes, afin d’éviter une référence circulaire.
