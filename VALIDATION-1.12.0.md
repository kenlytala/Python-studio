# Validation — Python Studio 1.12.0 — édition C

Vérifications du **1er novembre 2026**, sans aperçu, capture, serveur web ni bureau graphique. Les deux éditions sont de véritables implémentations distinctes. Le moteur, les contrôleurs et l’affichage de la recherche de l’édition C sont en C, pas un wrapper Python.

## Résultats réellement exécutés

- **Python : 430/437 tests réussis, 7 exclusions**, 12.618 s ; ResourceWarning strict, SSH réels et interopérabilité des profils avec le binaire C courant. **60 tests** de la nouvelle recherche : lexique littéral, Unicode/tabulations, fichiers réels, limites, snapshots, préférences, contrôleurs, clics/clavier, navigation globale, annulation et protection des documents.
- **C : 30/30 groupes**, production Release/OpenGL 2.1, 9.09 s après construction froide et élagage. Le nouveau moteur passe **235 assertions**. Les contrôleurs globaux passent **1 037 contrôles**, dont le nouveau parcours Ctrl+Maj+F/Entrée/F3/Maj+F3, la fermeture non bloquante, l’arrêt partiel, les générations/projets périmés et les caches/alias SSH hors ligne. Les **325** vérifications clavier multicurseur sont conservées.
- Affichage C : **six nouvelles dispositions à deux volets**, de 960×600 à 1920×900, surlignage de toutes les occurrences visibles/occurrence active, boutons, clipping UTF-8 et tabulations ; appels de rendu enregistrés sans fenêtre ni image. Les autres tests de vues sont conservés.
- **ASan + UBSan + détection de fuites : 13/13 groupes**, 18.99 s, exécutables non-PIE. L’oracle de recherche complet a aussi été répété avec le moteur C instrumenté.

## Parité et vrais fichiers

- Nouvelle recherche Python/C : **110 modèles littéraux**, **30 scans de systèmes de fichiers réels**, **592 pas de navigation globale** avant/arrière/bouclage. Comparaison des textes, offsets convertis en octets UTF-8, lignes/colonnes/tabulations, tous les compteurs et limites. Fichiers binaires, UTF-8 invalide, liens/FIFO, budgets, fins de ligne CRLF/CR et frontière 2 Mio couverts.
- Vrais XML **Odoo 18 et 19**, 69 395 et 69 477 octets, empreintes SHA256 épinglées dans `tests/xml-fixtures.json`, vérifiées à nouveau. Ces fixtures sont réutilisées depuis la livraison précédente ; aucune nouvelle récupération réseau n’est revendiquée.
- Fenêtre Python réelle sous clavier injecté : dossier externe, recherche `partner_id`, **2 fichiers / 38 occurrences**, **39 pas F3/Maj+F3**, changement automatique de fichier, bouclage, suivi vertical/horizontal, deux volets, documents/curseurs/historique/imports/XML et sources inchangés. Préférence contenant seulement le dossier. Script et résultat persistés parmi les preuves ; aucun rendu physique.
- Régressions de complétion de dossiers : **54 contextes, 39 éditions, 37 historiques, 5 catalogues**. XML : **275 cas, 266 documents valides, 268 historiques, 14 contextes de suggestion** ; fragments : **48 documents valides, 291 identifiants/fragments, 10 invalides**. Le parcours Python complet `sale/ → views/ → fichier XML → identifiant → champs/arch vide` reste vérifié sur les deux versions Odoo réelles.
- SSH réels : **13 cas par édition**, OpenSSH + SSHFS + serveur SFTP OpenSSH sur sockets Unix de QA. Nouveau cas : **2 fichiers montés, 3 occurrences**, F3/Maj+F3 global, aucun changement du contenu/historique de l’éditeur ni des sources distantes, aucune nouvelle authentification due à la recherche. Les autres cas de clés, mot de passe, refus hôte, annulation, CRUD et reprise restent couverts.

## Portée fonctionnelle et limites

**Ctrl+Maj+F** ou la palette ouvre la fenêtre. Dossier modifiable/mémorisé par projet, recherche récursive explicite, fichiers à gauche et contenu surligné en lecture seule à droite. **F3/Maj+F3 traversent tous les fichiers des résultats** et bouclent. Les instantanés proviennent du disque ; la consultation ne remplace pas le texte non enregistré d’un onglet. Les bases XML, imports, recherche du document, sessions, snippets et historiques sont conservés.

Recherche littérale sensible à la casse/non chevauchante. Limites : **2 Mio/fichier, 64 Mio lus, 16 Mio d’instantanés, 256 fichiers, 16 384 occurrences, 20 000 entrées, profondeur 64, 65 536 lignes/fichier**, 4 Mio de noms en attente, 1 024 octets de requête et 4 095 octets de chemin. Résultats partiels/ignorés signalés. Liens enfants, fichiers spéciaux/binaires/non UTF-8 et principaux dossiers VCS/générés exclus ; voir `GUIDE-RECHERCHE.md`. Colonnes en cellules scalaires/tabulations de quatre, pas `wcwidth`.

Un seul worker de recherche peut rester en vol. Fermeture/annulation sans jointure du worker actif dans l’interface ; une entrée/sortie bloquée peut retarder sa terminaison et la fermeture de l’application. Les gardes de génération/alias SSH ne constituent pas une sandbox générale. Les caches normaux du système de fichiers/SSHFS s’appliquent. Seul le dossier est enregistré dans une préférence privée locale en clair, pas la chaîne ou les résultats ; aucun secret d’authentification n’est ajouté.

## Exclusions et intégrité

**Sept exclusions Python** : matrice externe de versions PyLint, anciennes glibc, anciennes fixtures Python de complétion Odoo et quatre tests de bureau graphique. Les fixtures XML Odoo ci-dessus sont bien testées. Aucune validation physique GPU/GNOME/Windows/macOS, ORM ou base de données. Ruff isolé F,E9 : Python propre ; deux F401 historiques C inchangés dans `tests/test_completion_bridge.py` et le bridge d’analyse protégé. Pas de revendication de conformité à toutes les règles stylistiques. Quatre avertissements tiers `raylib/external/jar_mod.h:370` et la dépréciation CMake amont, aucun avertissement propre au projet dans la construction froide.

**30 archives/sidecars historiques et 14 fichiers protégés inchangés** depuis les empreintes de début de tâche. Backend Python portable SHA256 `2cbc839ab2ae06c120f3c83b66ba123b7d2df1360ed35ca409e0d39c3baa4850`, cible historique glibc 2.17/SSE2/OpenGL 2.1 ; pas de nouvel essai sous anciennes glibc. Le C Linux distribué requiert GLIBC_2.38 et le helper GLIBC_2.34 ; **libxml2.so.2 requise, non embarquée**. Les notices restent fournies.

Une seule construction native de production : **build-1.12.0**, raylib 5.5 épinglé possédé dans ce dossier, 34 Mio dont 14 Mio de sources raylib après élagage. L’ancien build généré est supprimé, ses archives conservées. Trois binaires stripés/0755, comparés à la production courante. Aucun ZIP, dossier preview ni environnement virtuel temporaire maintenu dans la livraison. Builds/caches/QA et images historiques d’aperçu exclus des nouvelles archives seulement.

Empreinte source C : `2a740631338e3ff5fcee1415b0885a4ae245c75c1e6cf2689999e6e5ba736708`. Le manifeste courant inventorie les fichiers distribués et modes sauf lui-même. Les anciens rapports/manifestes décrivent uniquement leurs versions historiques. Preuves : `tests/results-1.12.0/` et `tests/validation-summary-1.12.0.json`.

## Acceptation des archives extraites

Les deux archives candidates ont été extraites dans des dossiers neufs. Le C a été recompilé indépendamment puis **30/30 groupes** sont passés. L’édition Python extraite passe **430/437 tests, 7 exclusions**, avec ResourceWarning strict, SSH réels et le binaire de profils natif issu de cette recompilation. Les quatre oracles, le parcours réel Ctrl+Maj+F/F3 sur Odoo 18/19 et le parcours de complétion XML conservé ont été répétés. Les lanceurs et les trois binaires distribués ont été contrôlés. Le repaquetage final ajoute uniquement les preuves/rapports/manifeste, sans modifier un fichier fonctionnel accepté. Détails : `tests/results-1.12.0/extracted-acceptance.json`.

## Reproduire sans affichage

```bash
PRISME_SSH_INTEGRATION=1 PRISME_NATIVE_PROFILES=/chemin/test_ssh \
  python -B -W error::ResourceWarning -m unittest discover -s tests -v
ctest --test-dir build-1.12.0 --output-on-failure
python -I -B tools/check_folder_search.py /chemin/test_folder_search \
  --python-root /chemin/python-studio --output /chemin/parite.json
# Ajouter --fixtures /chemin/xml-fixtures pour les deux Odoo épinglés.
```

Les dépendances de QA/fixtures optionnelles sont documentées par les tests. Aucun serveur web, aperçu ou capture n’est nécessaire.
