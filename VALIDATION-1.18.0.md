# Validation — Python Studio 1.18.0 — édition C

Essais du **3 novembre 2026**, deux véritables éditions, aucun aperçu, image/capture, serveur web ou bureau graphique généré.

## Petit bouton d’exclusion directe

Dans la console d’analyse (**Problèmes Python / Erreurs C**), un bouton **− de 26 px** est placé à droite de chaque diagnostic PyLint éligible. Infobulle courte : **code exact et portée tous les projets**. Le texte du message garde une zone de navigation distincte ; les rectangles ne se superposent pas et le bouton reste dans les bornes/clipping. Les marqueurs dans le code restent navigables comme auparavant ; ce correctif ajoute le bouton dans la console, pas dans la gouttière.

Le clic ajoute le **code réellement affiché**, pas un indice mutable de ligne, à la liste globale persistante de 1.17. Il réutilise le même chemin de sauvegarde/invalidation que Paramètres → Règles, puis demande une nouvelle analyse du fichier actif, même en mode manuel. Retour en haut de la liste après changement. Aucun déplacement de curseur, édition de code, sélection/multicurseur ou historique modifié par le bouton. Réactivation via les paramètres ; aucune règle ignorée par défaut.

Le rapport et son document/version/volet/chemin/projet sont vérifiés avant application. Python capture le DTO et l’identité du rapport ; C transmet le code concret dans l’action et possède une révision monotone des résultats acceptés ainsi qu’une liaison de contexte. Un remplacement de rapport, même sans modification du texte, invalide l’ancienne action. Libération d’un document et remise à zéro de l’analyse effacent la liaison C. Un ancien bouton ne peut pas agir sur une autre ligne/règle après réordonnancement. Clic répété idempotent, dédoublonnage d’un code déjà exclu par son nom. Échec de sauvegarde et limites 64 entrées/4 095 octets gardent le réglage actif et le rapport antérieur.

Pas de bouton sur **E0001/syntax-error**, ni pour les rapports syntaxe/en attente/erreur interne. La compilation de syntaxe sans exécution avant PyLint reste indépendante des exclusions. Les règles continuent de passer réellement à --disable avant la limite de 256 diagnostics ; pas de filtre cosmétique. Les listes globales restent hors des projets et de leurs journaux de session, sans configuration/plugins/hooks projet activés.

## Preuve et tests nouveaux

- **20 tests Python** : modèles, vraie frame/clic du bouton, configuration sauvegardée immédiatement, relance même manuelle, source/carets/historiques intacts, navigation séparée, clipping à quatre tailles, défilement, rapport remplacé, changement de texte/onglet/chemin/projet/génération, modal/console masquée, limite/sauvegarde échouée, alias et double clic.
- La chaîne **vrai clic Python → configuration → vrai processus PyLint** retire W0611, garde E0602 et restitue W0611 après réactivation, sans écriture de code/config projet.
- **79 assertions supplémentaires C** par les vraies fonctions update/mouse et le vrai contrôleur de règle : affichage lié au contexte commun de production, code transmis et non indice de ligne, sauvegarde/rafraîchissement, source/sélection/multicurseur/historique inchangés, révision de rapport, onglet/volet/modal/chemin/texte, syntaxe, erreur disque/limites/alias/double clic. Services et OS non concernés simulés.
- **60 015 assertions du modèle C** : les **60 000 combinaisons catégorie/nombre** d’ID concret sont vérifiées, E0001 refusé, encodage/décodage, ajout/dédoublonnage/limites atomiques et frontières de contexte. Anciennes **53 assertions de profil** conservées.
- Les vraies vues C vérifient les boutons, l’ID dans l’infobulle, les arguments concrets, les bornes et l’absence de chevauchement avec les lignes dans **192 états d’interface**. Rapports syntaxe/erreur interne et E0001 sans bouton. Pas de rendu graphique ou d’image produit.
- **58 assertions de vrai PyLint C**, dont l’identité croissante des résultats, et **211 assertions de diagnostics** au total dans ce driver. Compatibilité backend/configuration réelle : **4.1.3/Python3.12, 2.17.7/Python3.11 et 2.12.2/Python3.10**. Les interpréteurs de QA ne sont pas embarqués.

### Contre-épreuves

Remplacer le code de règle par l’indice zéro **uniquement dans l’include de souris généré pour le banc C** fait échouer l’assertion d’exclusion de W0611. Restaurer le banc le fait réussir. Remplacer le callback Python en mémoire par une simple navigation fait échouer le test du clic réel. Aucune source de production modifiée. Les `FAIL` dans `quick-mutant.txt`/`python-quick-mutant.txt` sont **volontaires et attendus** ; voir `quick-mutation.json`.

## Résultats et non-régression

| Vérification | Résultat |
|---|---|
| Python complet, ResourceWarning en erreurs, SSH réel, interop native et matrice PyLint externe | **627/633 réussis, 6 exclusions historiques**, 38.687 s |
| C production puis répétition après configuration finale | **35/35**, 75.55 s puis 74.57 s |
| ASan + UBSan + fuites, non-PIE | **21/21**, 50.94 s |
| Contrôles C | **3 613 assertions**, dont 79 clic direct, 41 paramètres PyLint, 263 clavier, 848 frame recherche/F11 et 325 multicurseur |
| Vues C / recherche / géométrie des barres | **362 521 / 434 / 4 900** |
| Oracles | **7 réussis**, dont 4 sous sanitizers |

Les bibliothèques système et le processus Python/PyLint ne sont pas eux-mêmes instrumentés. Pas de garantie de durée stricte ; les temps incluent l’environnement de QA. Les six exclusions historiques restent explicites ; la matrice externe est exécutée, pas exclue.

Recherche seule avec F11, plein écran éditeur hors recherche, barres h/v manipulables, longues lignes cachées, clipping, captures de souris annulées, résultats progressifs/stables, filtres complets et F3 sur tous les fichiers conservés. Quitter reste en haut après Effets. Ctrl+Z AZERTY/redo/Ctrl+W, Unicode/AltGr, champs, multicurseurs et XML atomique restent couverts.

Odoo XML réel **18/19** revérifié par bytes/SHA256 épinglés : recherche 2 fichiers/38 occurrences/39 étapes ; options 1 fichier/24 occurrences ; deux parcours `sale/ → views/ → XML → ID → élément/arch fermé vide` avec vraie annulation AZERTY/rétablissement, sources intactes et XML vérifié par stdlib. Pas d’ORM/base/serveur Odoo. **13 cas SSH réels dans chaque édition**, fichiers distants seulement, Python/PyLint locaux, aucun secret enregistré ou auth automatique.

## Intégrité et distribution

**55 archives/sidecars historiques et 14 fichiers protégés inchangés**, notamment backend graphique Python et adaptateurs PyLint/complétion. Les grammaires stdlib sont étendues par des helpers purs et identiques dans leurs deux copies ; les contrôleurs C restent en C, pas des wrappers.

C : seule production **build-1.18.0**, raylib5.5 téléchargé avec SHA256 épinglé sans override, source possédée par le build puis élaguée ; ancien build1.17 retiré. Trois binaires stripés 0755 identiques aux copies de production, figés après la dernière configuration/reconstruction. PCRE2-8 **10.46** et libxml2 **2.9.14** dynamiques/non embarqués, aucune dépendance introuvable. Éditeur C GLIBC2.38/helper2.34 ; backend Python GLIBC2.17/SSE2/GL2.1 conservé octet pour octet, sans nouvelle preuve de bureau ancien.

Installation Python neuve, lanceurs, aide/version du programme principal et pip check réussis. Archives tar/gzip déterministes sans builds/caches/venv/ZIP/aperçu ; manifeste des tailles/modes/SHA256 de tous les fichiers sauf lui-même. Empreinte source C : `e54842fb010b87359eaa79c7b23eda96965ec6d478a182f2163be92d1964ad61`.

### Acceptation indépendante des archives extraites

Deux candidats extraits dans des dossiers neufs : tous les octets/modes vérifiés. C recompilé avec **objets neufs**, réutilisant seulement une copie de source tierce raylib : **35/35 groupes**. Python extrait : **627/633 réussis et 6 exclusions**, versions PyLint externes/SSH réels et interop avec le driver C extrait reconstruit. Sept oracles et deux workflows Odoo répétés et identiques ; installations/lanceurs et binaires fournis validés. Finalisation limitée aux preuves/manifeste 1.18 : **tout le payload source/binaire/documentation accepté conserve ses octets et modes**. Voir `tests/results-1.18.0/extracted-acceptance.json`.

## Limites et reproduction

API graphiques/OS simulées pour les vraies frames/vues : pas de preuve de bureau physique, GPU, GNOME, Windows ou macOS. Deux F401 historiques natifs et avertissements raylib amont conservés. Colonnes scalaires/tabulations4, pas wcwidth ; regex non temps réel ; gardes de fichiers non assimilables à un sandbox hostile ; I/O noyau pouvant retarder un worker. Aucun aperçu/capture/service graphique ou web généré, aucun secret SSH dans ce réglage.

```bash
PRISME_COMPAT_PYTHONS=/python-pylint-2.17:/python-pylint-2.12 PRISME_SSH_INTEGRATION=1 PRISME_NATIVE_PROFILES=/chemin/test_ssh python -B -W error::ResourceWarning -m unittest discover -s tests -v
# CMake : PRISME_REQUIRE_PYLINT_TESTS=ON, PRISME_SSH_INTEGRATION=ON,
# PRISME_TEST_PYTHON=/python-pylint-4,
# PRISME_TEST_COMPAT_PYTHONS="/python-pylint-2.17;/python-pylint-2.12"
ctest --test-dir build-1.18.0 --output-on-failure
```

Preuves : `tests/results-1.18.0/` ; synthèse : `tests/validation-summary-1.18.0.json`. Les anciens rapports/manifeste ne décrivent que leurs archives historiques. Dépendances de QA distinctes de celles de l’application.
