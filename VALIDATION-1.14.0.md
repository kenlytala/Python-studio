# Validation — Python Studio 1.14.0 — édition C

Vérifications du **2 novembre 2026**. Deux véritables implémentations indépendantes, sans aperçu, capture, service web ni bureau graphique.

## Changements vérifiés

1. **Résultats pendant le scan.** Les fichiers complètement lus et validés apparaissent avant la fin, avec le contenu surligné. Les workers restent bornés ; pas de file de messages par frame. Python publie des objets figés et partage textes/occurrences. C publie des descripteurs sous un verrou court, acquis par `trylock` côté interface, et partage les textes/occurrences immuables via références atomiques. Aucun verrou sur le parcours, les gardes, lectures ou matching ; aucun `join` ni nouvelle lecture de contenu pour publier dans la boucle graphique.
2. **Consultation stable.** Un nom arrivant avant la sélection dans le tri UTF-8 ne change pas le fichier consulté : conservation par chemin, occurrence, focus, vue en cache, défilements vertical/horizontal et ancre haute de la liste. La fin du scan ne revient pas au fichier zéro. F3/Maj+F3 traverse et boucle sur les fichiers déjà publiés ; compteur et ordre global provisoires pendant le scan. Arrêt et erreur conservent les fichiers déjà validés et les signalent comme partiels ; une réponse périmée ne réintroduit pas de fichiers.
3. **F11 uniquement dans la recherche.** Le même bouton **Agrandir · F11 / Restaurer · F11** occupe/restaure le dialogue dans la surface de l’éditeur. **Zéro appel de mutation de fenêtre système** : ni `ToggleFullscreen`, ni maximisation/restauration système, ni redimensionnement/déplacement. Les quatre combinaisons plein écran préexistant × maximisation préexistante sont exercées. En dehors du dialogue, F11 conserve le plein écran global existant.
4. **Quitter déplacé.** Un seul bouton textuel en haut immédiatement à droite d’Effets ; pas de doublon dans le statut. Contrôles supérieurs dans les limites et sans chevauchement dès 960 pixels. Confirmation des documents modifiés conservée. Statut toujours rempli exactement jusqu’au bord inférieur. F8, palette et Alt+F4 restent accessibles si le menu est replié.

Casse, mots entiers, regex PCRE2, sous-dossiers, quatre filtres d’inclusion/exclusion fichiers/dossiers et navigation globale restent disponibles. Seul le dossier est persisté, pas les options/filtres/requêtes/résultats. Les onglets modifiés et leur historique ne sont pas remplacés par les instantanés du disque.

## Essais réellement exécutés

| Vérification | Résultat |
|---|---|
| Python complet, ResourceWarning strict, SSH réel et profils C courants | **508/515 réussis, 7 exclusions**, 14.739 s |
| Recherche Python dédiée | **138 tests**, dont **22 nouveaux** pour publication/chrome/F11 |
| C Release / OpenGL 2.1, compilation froide | **30/30 groupes**, 11.4 s |
| C après élagage du build et vérification des lanceurs | **30/30 groupes**, 11.45 s |
| Moteur C | **427 assertions** |
| Contrôleurs C réels, seules API de fenêtre simulées | **2382 contrôles** + **325** contrôles clavier multicurseur |
| Vues C réelles extraites dans un harness CPU | **355526 assertions**, dont contrôle du haut/statut ; 6 + 24 dispositions de recherche conservées |
| ASan + UBSan + fuites, non-PIE | **13/13 groupes**, 3.9 s |

Les nouveaux tests bloquent de façon déterministe le chemin suivant après publication des premiers fichiers : **12 fichiers visibles dans chaque contrôleur alors que le worker est toujours bloqué**, puis insertion triée avant la sélection et fin sans saut. F3, focus, cache de contenu, défilements, Stop, erreur avant/après premier poll, fermeture/réouverture sans attente, changement puis rétablissement d’option, changement SSH et projet sont exercés. Les snapshots C sont encore lus après destruction du job, fin de tri, annulation et erreur, puis libérés sous sanitizers. Tests de contention explicite du verrou : le lecteur retourne sans attendre.

Les deux oracles complets de recherche sont aussi exécutés avec le driver C ASan/UBSan : aucune erreur mémoire/fuite détectée dans ce périmètre. PCRE2 système n’a pas été recompilé avec les sanitizers ; il ne s’agit pas d’un détecteur de races ou d’une preuve physique de latence de bureau.

### Régressions et fichiers réels

- Options avancées : **148 modèles, 37 scans, 650 pas de navigation**, plus **14 cas regex comparés à l’oracle indépendant Python stdlib**. Mode littéral : **110 modèles, 30 scans, 592 pas F3**. C/Python comparés sur fichiers/textes, spans UTF-8, lignes/colonnes, compteurs, filtres, budgets, erreurs et navigation. Les deux modes avancés utilisent PCRE2 ; la parité n’est pas une preuve d’un second moteur regex indépendant.
- Complétion de chemins : **54 contextes, 39 éditions, 37 historiques atomiques, 5 catalogues**. XML : **275 cas, 266 documents valides, 268 historiques, 14 suggestions**. Fragments XML : **48 documents valides, 291 identifiants/fragments, 10 invalides**.
- XML Odoo 18/19 retéléchargés explicitement et vérifiés le 2 novembre : **69 395 / 69 477 octets**, hashes épinglés dans `tests/xml-fixtures.json`. Contrôleur Python réel avec clavier injecté : **2 fichiers / 38 hits / 39 pas F3**, puis filtres/casse/mots entiers/regex donnant **1 fichier / 24 hits**, confrontés à la stdlib. F11 agrandit seulement le dialogue, état/dimensions/position système inchangés. Les deux parcours `sale/ → views/ → fichier XML → ID → champs/arch vide fermé` sont répétés avec undo/redo atomique et sources inchangées. Aucun ORM, framework ou base de données exécuté.
- **13 cas SSH réels par édition** : OpenSSH/SSHFS/SFTP sur sockets Unix de QA, sans service web. Recherche dans deux fichiers montés : 3 hits littéraux, puis 1 hit filtré/regex/casse/mots entiers ; F3 et agrandissement seulement du dialogue, documents/historique/sources distantes inchangés, aucune nouvelle authentification provoquée par la recherche. Les fixtures sont créées avant le montage, sans contourner son cache.

## Limites, dépendances et exclusions

Plafonds conservés : **2 Mio/fichier, 64 Mio lus, 16 Mio d’instantanés, 256 fichiers, 16 384 hits, 20 000 entrées, profondeur 64, 65 536 lignes/fichier, 4 Mio de noms en attente** ; requête 1 024 octets et chemin 4 095 octets UTF-8. Liens enfants, fichiers spéciaux/binaires/non UTF-8 et dossiers VCS/cache ignorés ; normalisation LF en mémoire seulement. Quatre filtres : 512 octets/champ, 16 motifs/champ, 256 octets/motif ; globs `* ? **`, séparateur `;`, `[]` littéral, noms sensibles à la casse.

PCRE2 UTF/UCP/multiligne, sans JIT, `\C` brut interdit : **200 000 unités de travail/appel, profondeur 512, tas 8 Mio, budget 2 s/fichier**, callouts d’annulation ; erreurs explicites. Pas de garantie temps réel. Les gardes ne sont pas une sandbox de système de fichiers hostile ; caches SSHFS habituels. Une I/O noyau bloquée peut retarder la terminaison du worker ou la fermeture complète de l’application, pas provoquer un `join` de scan dans le dialogue. Colonnes Unicode scalaires/tabulations de quatre, pas `wcwidth`.

QA : **PCRE2-8 10.46 / libxml2 2.9.14 système**, non embarqués. Pour compiler le C : PCRE2 dev/pkg-config, libxml2 et dépendances raylib graphiques. Python conserve son mode littéral sensible à la casse sans PCRE2 ; les modes qui l’exigent donnent un message explicite si elle manque. Binaire éditeur Linux distribué : GLIBC_2.38, helper GLIBC_2.34. Backend graphique Python protégé inchangé, SHA256 `2cbc839ab2ae06c120f3c83b66ba123b7d2df1360ed35ca409e0d39c3baa4850`, cible historique glibc 2.17/SSE2/OpenGL 2.1 ; pas de nouvel essai sous anciennes glibc pour cette livraison.

**Sept exclusions Python historiques** inchangées (matrice externe PyLint, anciennes glibc, anciennes fixtures Python Odoo et quatre tests de bureau physique). Aucun test physique GPU/GNOME/Windows/macOS : disponibilité des raccourcis système non certifiée. Ruff F,E9 Python propre ; deux F401 historiques natifs (test et bridge protégé) inchangés. Quatre avertissements amont `raylib/external/jar_mod.h:370` et dépréciation CMake amont ; aucun avertissement de nos sources pendant la compilation froide. Pas de revendication de style intégral.

## Intégrité et paquet

**39 archives/sidecars historiques et 14 fichiers protégés inchangés**, confrontés aux empreintes prises avant la modification. Les anciens manifestes/rapports décrivent leurs propres versions uniquement. Une seule production native courante **build-1.14.0**, source raylib 5.5 épinglée téléchargée sans override et possédée dans ce build : **34 Mio dont 14 Mio de raylib** après élagage des exemples/logos/outils inutiles ; ancienne production 1.13 supprimée. Trois binaires stripés 0755 identiques à la production. Lanceurs 0755, installation Python neuve, versions/aides et pip check réussis ; venv temporaire supprimé.

Les archives tar/gzip sont déterministes, sans builds, caches, venv, ZIP, aperçu ou capture. Manifestes : taille, mode, SHA256 de chaque fichier distribué, sauf du manifeste lui-même. Empreinte source C : `2f2a7ffaff0905699548021d32789f8a73a4da23e9208b7a88d0011ebc07bf3e`. L’empreinte source ne couvre pas la documentation et les preuves ; le manifeste couvre tout le paquet.

### Acceptation indépendante des archives extraites

Les deux archives candidates sont extraites dans des dossiers neufs ; chaque fichier/mode est confronté au manifeste. C **recompilé avec objets neufs** (seule une copie de la source tierce raylib est réutilisée) : **30/30 groupes**. Python extrait : **508/515 réussis, 7 exclusions**, ResourceWarning strict, SSH réels et interop avec le driver C extrait/recompilé. Les **cinq oracles et deux parcours Odoo réels** sont répétés, ainsi que lanceurs/installations et vérification des binaires fournis. Le paquet final modifie seulement les preuves/manifeste de 1.14 ; les fichiers et modes acceptés restent identiques. Voir `tests/results-1.14.0/extracted-acceptance.json`.

## Reproduire sans affichage

```bash
PRISME_SSH_INTEGRATION=1 PRISME_NATIVE_PROFILES=/chemin/test_ssh python -B -W error::ResourceWarning -m unittest discover -s tests -v
ctest --test-dir build-1.14.0 --output-on-failure
python -I -B tools/check_search_options.py /chemin/test_folder_search --python-root /chemin/python-studio --fixtures /chemin/xml-real --output /chemin/options.json
python -I -B tools/check_folder_search.py /chemin/test_folder_search --python-root /chemin/python-studio --fixtures /chemin/xml-real --output /chemin/litteral.json
```

Les outils de QA (notamment paramiko/SSHFS/FUSE, compilateur et sanitizers) sont distincts des dépendances d’exécution de l’éditeur. Logs et JSON : `tests/results-1.14.0/`, synthèse : `tests/validation-summary-1.14.0.json`.
