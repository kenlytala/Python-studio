# Validation — Python Studio 1.13.0 — édition C

Vérifications du **2 novembre 2026**, sans aperçu, capture, serveur web ou bureau graphique. Les deux éditions sont de véritables implémentations indépendantes ; la recherche C, ses contrôleurs et son rendu sont en C. PCRE2 est une bibliothèque système standard, pas un lancement de l’autre éditeur.

## Fonctionnalités acceptées

- **Agrandir/Restaurer** utilise toute la fenêtre. **F11** donne le plein écran de la recherche, puis rétablit le mode précédent. Fermer, choisir un dossier ou changer de projet restaure l’affichage antérieur. Une application déjà en plein écran y reste. Aucun redémarrage de scan ni changement des documents dû à l’agrandissement.
- **Casse, Mots entiers, Regex, Sous-dossiers**, et quatre filtres **fichiers inclus/exclus, dossiers inclus/exclus**, appliqués explicitement après Rechercher/Entrée. PCRE2 UTF/UCP/multiligne, sans JIT. Globs relatifs `* ? **`, `;` ; les filtres de noms restent sensibles à la casse, `[]` littéral.
- F3/Maj+F3 traverse toujours tous les fichiers et boucle. Surbrillance de chaque ligne d’un match multiligne ; marqueur visible d’une cellule pour un match vide. Curseurs, sélections, multicurseurs et historiques non modifiés, lecture seule des instantanés du disque.
- Options/filtres immuables pour une demande ; changements de paramètres/génération/projet et changements annulés à nouveau rejettent les réponses en vol. Fermeture d’une demande bloquée sans joindre son worker dans l’interface. Seul le **dossier** est persisté, pas les flags, motifs, requêtes ou résultats.

## Essais réellement exécutés

| Vérification | Résultat |
|---|---|
| Python complet, ResourceWarning strict, SSH réel, profils natifs courants | **486/493 réussis, 7 exclusions**, 14.456 s |
| Recherche Python dédiée | **116 tests**, dont **56 nouveaux** pour cette livraison |
| C production Release/OpenGL 2.1, compilation froide et reprise après élagage | **30/30 groupes**, reprise 10.26 s |
| Moteur de recherche C | **254 assertions**, Unicode/regex/vides/multilignes/erreurs/budgets/préférences/annulation |
| Contrôleurs C réels, API de fenêtre simulée | **1 069 contrôles**, plus **325** vérifications clavier multicurseur conservées |
| Vues CPU C | **347 908 assertions** globales ; six dispositions de recherche conservées + **24** combinaisons options/filtres/agrandissement et marqueurs vides/multilignes |
| ASan + UBSan + fuites, non-PIE | **13/13 groupes**, 3.84 s |

Deux oracles de recherche sont aussi répétés avec l’exécutable C instrumenté : aucune erreur ASan/UBSan/fuite détectée dans ce périmètre. Le PCRE2 système n’a pas lui-même été recompilé avec les sanitizers.

### Parité, oracles indépendants et sources réelles

- Options : **148 modèles**, **37 scans réels**, **650 pas de navigation globale**, **14 cas regex avec oracle indépendant Python stdlib** pour un sous-ensemble commun non vide. C/Python comparés sur tous les spans UTF-8, lignes/colonnes, fichiers/textes, compteurs, budgets, quatre filtres, Unicode, regex vides/multilignes, erreurs et F3. Les deux implémentations utilisent PCRE2 pour ces syntaxes : cette parité ne constitue pas une preuve d’un second moteur regex indépendant.
- Mode littéral 1.12 conservé : **110 modèles, 30 scans, 592 pas F3**. Complétion de dossiers : **54 contextes, 39 éditions, 37 historiques, 5 catalogues**. XML : **275 cas, 266 documents valides, 268 historiques, 14 suggestions** ; fragments : **48 documents valides, 291 identifiants/fragments, 10 invalides**.
- XML Odoo 18/19 **retéléchargés explicitement le 2 novembre**, 69 395 / 69 477 octets, SHA256 épinglés dans `tests/xml-fixtures.json` et revalidés par les scripts. Aucune exécution du framework, ORM ou base de données.
- Contrôleur Python réel sous clavier injecté, sources Odoo : **2 fichiers / 38 hits / 39 pas F3-Maj+F3**, puis **1 fichier filtré / 24 hits regex insensibles à la casse et mots entiers**, nombre confronté à un oracle stdlib séparé. Agrandissement, F11, fermeture et restauration du mode précédent, sources/onglets/historique/XML/imports inchangés. Le parcours réel de complétion de dossiers/XML reste répété sur les deux versions Odoo.
- **13 cas SSH réels par édition**, OpenSSH + SSHFS + SFTP OpenSSH via sockets Unix de QA, sans serveur web. Cas de recherche enrichi : **2 fichiers montés / 3 hits littéraux**, puis regex/casse/mots entiers/filtres donnant **1 hit** ; F3 et logique plein écran sous API simulée. Texte/historique de l’éditeur et sources distantes inchangés, aucune nouvelle authentification due aux options.

## Limites et dépendances

Les plafonds 1.12 sont conservés : 2 Mio/fichier, 64 Mio lus, 16 Mio d’instantanés, 256 fichiers, 16 384 hits, 20 000 entrées, profondeur 64, 65 536 lignes/fichier, 4 Mio de noms en attente, requête 1 024 octets et chemin 4 095 octets UTF-8. Les liens enfants, fichiers spéciaux/binaires/non UTF-8 et dossiers VCS/cache restent ignorés. Fin de lignes normalisée en mémoire uniquement. Filtres : **512 octets/champ, 16 motifs/champ, 256 octets/motif**.

PCRE2 : **200 000 unités de travail/appel, profondeur 512, tas de match 8 Mio, budget 2 s/fichier**, callouts d’annulation et `\C` interdit. Pas de JIT ni de code fourni par l’utilisateur exécuté. Les sujets UTF-8 sont validés avant d’éviter leur revalidation à chaque match (cas 2 Mio / 16 384 hits testé). Une syntaxe invalide ou un budget atteint produit une erreur explicite, pas « aucun fichier ». Ce n’est pas une garantie temps réel ; une I/O noyau bloquée peut retarder la terminaison/fermeture.

PCRE2-8 système **10.46** et libxml2 **2.9.14** utilisés en QA, non embarqués. Le Python garde son mode littéral sensible à la casse sans PCRE2 ; les modes avancés/globs réclament cette bibliothèque avec message explicite. C : `libpcre2-dev` + `pkg-config` pour compiler, runtime PCRE2-8 et libxml2 requis. Notice `third_party/PCRE2-NOTICE.txt`. C Linux distribué : GLIBC_2.38, helper GLIBC_2.34. Backend graphique Python portable inchangé, SHA256 `2cbc839ab2ae06c120f3c83b66ba123b7d2df1360ed35ca409e0d39c3baa4850` ; cible historique glibc 2.17/SSE2/GL2.1, pas de nouvel essai sous anciennes glibc.

Les gardes SSH/alias ne sont pas une sandbox de système de fichiers hostile. Les caches habituels de SSHFS s’appliquent. Colonnes scalaires et tabulations de quatre, pas `wcwidth`. Aucune disponibilité physique de raccourci sous GNOME/autre gestionnaire de fenêtres certifiée.

## Exclusions, intégrité, construction et packaging

**Sept exclusions Python inchangées** : matrice externe PyLint, anciennes glibc, anciennes fixtures Python Odoo et quatre tests de bureau physique. Les nouveaux tests et les fixtures XML ci-dessus sont exécutés. Aucun essai physique GPU/Windows/macOS ou ORM/base de données. Ruff isolé F,E9 Python propre ; deux F401 historiques natifs inchangés (test et bridge protégé), pas de revendication de style complet. Quatre avertissements amont `raylib/external/jar_mod.h:370` et dépréciation CMake amont ; aucun avertissement de nos sources pendant la construction froide.

**35 archives/sidecars historiques et 14 fichiers protégés inchangés** depuis les empreintes de début de tâche. Les anciens rapports/manifestes ne décrivent que leurs propres versions. Une seule construction native de production **build-1.13.0**, raylib 5.5 épinglé téléchargé sans override et possédé dans ce build ; après élagage **34 Mio dont 14 Mio de source raylib**, ancien build généré supprimé. Trois binaires stripés 0755 identiques à la production courante, bibliothèques résolues. Lanceurs 0755, installation Python dans un venv neuf/version/aide/pip check réussis, venv temporaire supprimé.

Builds, caches, venv et images historiques d’aperçu exclus des nouvelles archives. Pas de ZIP ni de nouveau preview/capture/service. Archives gzip/tar déterministes, membres réguliers/modes/hashes inventoriés sauf le manifeste lui-même ; deux repaquetages finaux confrontés extérieurement. L’empreinte source ne couvre pas la documentation/preuves ; le manifeste couvre le paquet.

Empreinte source C : `891b804f34e5814738e0cb1674f9d175a726c1d027e91c51b0cfc3e3a98269a2`. Voir `tests/results-1.13.0/` et `tests/validation-summary-1.13.0.json` pour les logs, versions, scripts d’acceptation au format texte et résultats JSON.

## Acceptation indépendante des archives extraites

Les deux archives candidates ont été extraites dans des dossiers neufs ; tous les fichiers/modes ont été confrontés au manifeste. C recompilé indépendamment (objets neufs, seule une copie propre des sources tierces raylib est réutilisée), **30/30 groupes**. Python extrait : **486/493, 7 exclusions**, ResourceWarning strict, SSH réels et profils avec le driver C extrait/recompilé. Les **cinq oracles**, deux parcours Odoo réels et lanceurs/installations ont été répétés. Les trois binaires fournis et leurs dépendances sont vérifiés. Le paquet final ne change que les preuves/manifeste courants ; les fichiers acceptés et leurs modes restent identiques. Détails : `tests/results-1.13.0/extracted-acceptance.json`.

## Reproduire hors affichage

```bash
PRISME_SSH_INTEGRATION=1 PRISME_NATIVE_PROFILES=/chemin/test_ssh python -B -W error::ResourceWarning -m unittest discover -s tests -v
ctest --test-dir build-1.13.0 --output-on-failure
python -I -B tools/check_search_options.py /chemin/test_folder_search --python-root /chemin/python-studio --fixtures /chemin/xml-real --output /chemin/options.json
python -I -B tools/check_folder_search.py /chemin/test_folder_search --python-root /chemin/python-studio --fixtures /chemin/xml-real --output /chemin/litteral.json
```

Les outils QA, notamment paramiko/SSHFS/FUSE pour SSH, sont distincts des dépendances d’exécution de l’éditeur. Aucun affichage ni serveur web requis.
