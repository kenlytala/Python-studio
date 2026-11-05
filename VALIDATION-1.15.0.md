# Validation — Python Studio 1.15.0 — édition C

Vérifications du **2 novembre 2026**, sur les deux véritables éditions. Aucun aperçu, capture, serveur web ou bureau graphique généré.

## Correctif effectivement couvert

### Barres du contenu de recherche

Barres **horizontale et verticale manipulables** : clic dans la piste, prise du curseur sans saut de page, glissement hors piste borné, position finale appliquée lorsque déplacement et relâchement arrivent ensemble. Un clic/relâchement dans une seule frame ne laisse pas de prise active ; une perte de capture annule sans saut.

La portée horizontale est mesurée sur **toutes les lignes du fichier sélectionné**, pas les seules lignes visibles, puis mise en cache par sélection. Colonnes Unicode scalaires, tabulations de quatre, une cellule de fin pour les matches vides en fin de ligne. Test limite : **2 Mio de tabulations → 8 388 608 cellules**. Molette, Maj+molette, flèches et pointeur sont bornés à la portée réelle ; très grands deltas de molette finis bornés avant conversion entière en C. Pistes remplies et inactives si le contenu tient déjà.

Bandes inférieure/droite réservées : texte et surlignages découpés avant les barres. La **fin d’une ligne cachée de 200 000 cellules** avec Unicode/tabulations est réellement transmise aux fonctions de dessin simulées dans les deux éditions ; ce n’est pas seulement un test de compteur. C : rectangles de clic, clipping et changement de géométrie vérifiés dans les vraies vues. Python : contenu toujours affichable après suppression du fichier source, sans relecture.

F3/Maj+F3 restent utilisables, révèlent les occurrences et changent de fichier. Échap ferme même pendant une prise. Fichier différent, fermeture, arrêt, F11 ou géométrie différente annulent la prise périmée. Les documents modifiés, sélections, curseurs, historique et onglets restent inchangés ; résultats en lecture seule.

### F11 C : défaut de 1.14 reconnu et test d’entrée corrigé

Le défaut provenait de **`src/app.c:update`**, qui exécutait l’action plein écran globale avant le dispatch modal et empêchait la recherche de recevoir F11. En 1.14, les tests directs du seul contrôleur de recherche passaient mais **ne validaient pas ce routage réel**. Le correctif donne maintenant la priorité contextuelle à la recherche à cette entrée globale, avec consommation unique. Le gestionnaire local reste disponible pour les contrôleurs autonomes.

Le banc CPU extrait directement de la production **`mouse_update`, `modal_keys`, `window_shortcuts` et `update`** ; pas une réécriture de leur logique. Il injecte les événements en file, pressés, répétés et l’alias natif Alt+Entrée, pour **quatre états système et douze focus**. Aucun appel aux API simulées de plein écran, maximisation/restauration, dimensions ou position pendant F11 dans la recherche. Hors recherche, le plein écran de l’éditeur est toujours déclenché une fois. Clic simultané, prise de barre, F3 et Échap sont couverts par les mêmes vrais routeurs.

**Test négatif attendu :** remettre le vieux dispatch global uniquement dans l’include généré du banc QA fait échouer l’assertion d’agrandissement de recherche. Le banc restauré passe ensuite. Le `FAIL` dans `frame-mutant.txt` est donc **volontaire et requis**, pas un échec de la livraison. Les sources de production et leur build ne sont pas modifiés pour cet essai. Empreinte des corps extraits : `242d69f1d2f56d3804625ee8cd8c43762b0ad39533f49bd3c334eda0548d3eae`. Les services non concernés et les API système sont simulés ; aucune preuve de bureau physique n’est revendiquée.

## Résultats exécutés

- Python strict : **532/539 réussis, 7 exclusions historiques**, 17.127 s, ResourceWarning traités en erreurs ; SSH réel et interop avec le driver C courant.
- Recherche dédiée : **162 tests réussis** (60 moteur, 56 options, 22 progressifs/chrome, **24 nouveaux défilement/événements**).
- C : **30/30 groupes**, configuration de production finale 11.45 s et répétition 11.68 s.
- Contrôles C : **3 230 assertions**, dont **848 nouvelles par les vraies frames** et 325 clavier multicurseur ; moteur recherche 434 ; vues **356 256** ; géométrie des barres **4 900 états**.
- ASan/UBSan/fuites : **13/13 groupes**, non-PIE, 3.84 s, plus les deux oracles de recherche ; bibliothèques système tierces non instrumentées.
- Cinq oracles indépendants/parité conservés : options **148 modèles / 37 scans / 650 navigations / 14 regex stdlib** ; littéral **110/30/592** ; dossiers **54 contextes / 39 éditions / 37 historiques / 5 catalogues** ; XML **275 cas / 266 valides / 268 historiques / 14 contextes** ; fragments **48 documents valides / 291 fragments et IDs / 10 invalides**.
- Odoo 18/19 réel, bytes/SHA256 épinglés et revérifiés : parcours recherche **2 fichiers / 38 occurrences / 39 étapes**, options filtrées **1 fichier / 24 occurrences** ; deux parcours dossier → XML → ID → fragment/arch fermé vide/undo-redo. Pas de serveur Odoo, ORM ou base de données exécuté.
- **13 cas SSH réels dans chaque édition**, serveur de test AF_UNIX chiffré, fichiers distants uniquement et Python/PyLint locaux. Publication progressive, annulation, instantanés stables par chemin et protections des documents modifiés conservés.

## Intégrité et distribution

**43 archives/sidecars historiques et 14 fichiers protégés inchangés**, confrontés au relevé préalable. Le backend Python glibc 2.17/SSE2/OpenGL 2.1 et ses trois correctifs restent inchangés. Une seule production C courante : **build-1.15.0**, source raylib 5.5 épinglée téléchargée sans override et possédée par ce build, **34 Mio dont 14 Mio de source raylib** après élagage des exemples/outils inutiles ; ancien build 1.14 supprimé. Trois binaires stripés 0755 identiques à la production.

PCRE2-8 **10.46** et libxml2 **2.9.14** dynamiques, non embarqués ; aucune bibliothèque introuvable. Éditeur C GLIBC2.38, helper2.34 ; pas de nouvelle exécution sur ancien système physique. Installation Python neuve, lanceurs exécutables, aide/version du programme principal et `pip check` réussis ; venv temporaire supprimé.

Archives tar/gzip déterministes, sans build/cache/venv/ZIP/aperçu. Manifestes : taille, mode, SHA256 de tous les fichiers distribués sauf d’eux-mêmes. Empreinte source C : `b7a4c7e2f9d15753359e7ed7813ba5c223335cefaafd0439e3da1c5ef62e4de6`. Documentation et preuves couvertes par le manifeste, pas par l’empreinte source.

### Acceptation indépendante des archives extraites

Deux archives candidates extraites dans des dossiers neufs ; fichiers et modes confrontés au manifeste. C **recompilé avec objets neufs**, en réutilisant seulement une copie de la source tierce raylib : **30/30 groupes**. Python extrait : **532/539 réussis et 7 exclusions**, ResourceWarning strict, SSH réels et interop avec le driver C extrait/recompilé. Cinq oracles et deux parcours Odoo répétés, installations/lanceurs et binaires fournis vérifiés. Finalisation limitée aux preuves et au manifeste de 1.15 ; **tous les autres fichiers et modes acceptés restent identiques**. Voir `tests/results-1.15.0/extracted-acceptance.json`.

## Limites et reproduction

Tests d’entrée et dessin avec API simulées, sans validation GPU/GNOME/Windows/macOS physique. Colonnes scalaires, pas `wcwidth`. Gardes de fichiers non assimilables à un sandbox contre système hostile ; budgets regex non temps réel, I/O noyau pouvant retarder la terminaison d’un worker. Sept exclusions Python historiques, deux F401 historiques C/Python et avertissements amont raylib conservés. Seul le dossier de recherche est persisté, jamais les secrets SSH.

```bash
PRISME_SSH_INTEGRATION=1 PRISME_NATIVE_PROFILES=/chemin/test_ssh python -B -W error::ResourceWarning -m unittest discover -s tests -v
ctest --test-dir build-1.15.0 --output-on-failure
python -I -B tools/check_search_options.py /chemin/test_folder_search --python-root /chemin/python-studio --fixtures /chemin/xml-real --output /chemin/options.json
python -I -B tools/check_folder_search.py /chemin/test_folder_search --python-root /chemin/python-studio --fixtures /chemin/xml-real --output /chemin/litteral.json
```

Dépendances de QA (Paramiko, SSHFS/FUSE, compilateur, sanitizers) distinctes de l’exécution de l’éditeur. Preuves : `tests/results-1.15.0/` ; synthèse : `tests/validation-summary-1.15.0.json`. Quitter reste en haut immédiatement après Effets ; options complètes et résultats progressifs conservés.
