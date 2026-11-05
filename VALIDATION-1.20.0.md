# Validation — Python Studio1.20.0 — édition C

**3 novembre2026.** Trois véritables éditions C, Python/raylib et GTK4. Aucun aperçu, fenêtre d’éditeur, capture ou serveur graphique/web généré. Les demandes nouvelles utilisent **Odoo19**, Python et XML, conformément au choix de l’utilisateur.

## Archives supprimées à la demande

Les **32 anciennes archives.tar.gz** (71 363 275 octets) et leurs **31 fichiers de contrôle correspondants** ont été supprimés avant la reprise. Elles ne sont pas recréées. Les sources restent en place, les anciens rapports/manifeste sont historiques et ne prétendent pas que leurs archives existent encore. Les trois livrables à jour sont exclusivement **.tar.xz**, sans ZIP. Détail : `archive-deletion.json`.

## Fonctionnalités intégrées aux trois éditions

### Champs de chemin : Ctrl+Espace

Les navigateurs Ouvrir fichier/dossier, Enregistrer sous, Imports/Odoo, racines XML et dossier de recherche suggèrent le contenu du chemin saisi, ou le dossier courant si vide. Dossier existant sans slash final, chemins absolus/relatifs/~/, espaces, guillemets littéraux et préfixes supportés. Modes dossier filtrés sur les répertoires ; ordre stable dossiers puis fichiers. **Aucune ouverture/édition du code pendant la suggestion.** Flèches pour choisir, Tab/Entrée pour compléter le champ, validation ensuite ; insertion annulable par Ctrl+Z/Y.

Worker non récursif, au plus **8 192 entrées** et **256 suggestions**, statut explicite de limitation. Pas de contenu de fichier lu ni de scan à chaque frappe. Champ/curseur/sélection/modal/projet et bail SSH lient la réponse ; saisie/fermeture rend la lecture périmée. Les métadonnées de type sont cachées pour l’affichage des suggestions. Le dossier de base d’un chemin relatif n’est pas changé par la publication des résultats, afin d’éviter `sale/sale/...` lors d’une saisie poursuivie. Gardes de liens/montage, aucun SSH auto-auth ; I/O noyau pouvant retarder un worker, pas de sandbox hostile.

### Coloration JavaScript et XML

Extensions insensibles à la casse : **.js/.mjs/.cjs**, **.xml/.xsl/.xslt/.svg** ; Python existant conservé. Lexeurs bornés et tolérants aux saisies incomplètes : commentaires, chaînes, nombres, mots-clés/fonctions/constantes, templates JS imbriqués et `${…}`, regex/division ; balises/attributs/entités/commentaires/CDATA/déclarations XML. Aucune évaluation, import ou I/O. La langue fait partie du cache : renommer sans modifier la version du texte recalcule les couleurs. **6 020 cas de couleurs UTF-8 C/Python identiques**, dont Unicode et source partielle. JavaScript n’est pas envoyé au compilateur Python : coloration, pas ESLint/Node ou preuve de conformité ECMAScript.

### Vérification Odoo19 Python et XML

**Paramètres → Odoo19 — Python/XML — Ce projet** : actif par défaut, désactivable, exclusions propres au projet, sauvegarde/invalidation/rafraîchissement même en manuel. Les exclusions PyLint génériques restent globales. Bouton − Odoo Python/XML lié au projet, infobulle de portée ; E0001/syntax-error toujours protégé. Les sessions de disposition ne rétablissent pas ces profils d’analyse.

Python : vrai **pylint-odoo10.0.12** avec `--valid-odoo-versions=19.0`, **PyLint4.0.10** compatible. Import du plugin installé approuvé **avant** les chemins projet/imports, rcfile nul conservé : un `pylint_odoo.py`/`pylint.py`/init-hook projet empoisonné n’est pas exécuté. Plugin absent/vieux ne connaissant pas19 signalé explicitement ; pas de faux contrôle Odoo sans problèmes. Les listes Odoo ne sont transmises qu’en mode actif ; PyLint générique2.12/2.17 reste testé en mode sans plugin. Exemple réel : **W8165 deprecated-self-cr** sur `self._cr`, E0602 véritable conservé, exclusion et mode off vérifiés.

XML : syntaxe dédiée au parseur XML, jamais compilation Python ; toujours disponible si Odoo off. W9001..W9010 sur IDs/t-name locaux réutilisés (avertissement, mise à jour possible), modèle de record/nom de field vides, identifiants vides/espaces, tree et attrs/states anciens en vue19, openerp, racine non standard et inclusion externe non résolue. Templates Owl/namespaces/CDATA sans évaluation, **arch fermé vide accepté**. Pas d’ORM/base/serveur, pas de résolution globale des références ni de validation des champs de la base.

DTD et entités refusées avant expansion ; aucune ressource file/http ouverte. Source2Mio,20 000éléments,profondeur256,4096IDs,256diagnostics stockés avec priorité syntaxe ; limite/syntaxe incomplète signalée. L’éditeur a déjà décodé l’encodage d’origine : le parseur force le transport UTF-8 interne sans réécrire la déclaration UTF-16/Latin1 ni décaler les coordonnées. Les diagnostics sont convertis aux positions UTF-8/scalaires de chaque éditeur.

## Vérifications exécutées

| Domaine | Résultat |
|---|---|
| Python strict / SSH réel / interop et versions externes | **660/666 réussis, 6 exclusions historiques**, 27.884s |
| GTK strict / SSH réel / interop et versions externes | **705/706 réussis, 1 exclusion historique**, 27.465s |
| C production complète finale | **37/37**, 46.19s |
| ASan/UBSan/fuites, non-PIE | **23/23**, 11.22s |
| Nouveaux tests Python partagés | **33** dans chacune des deux éditions Python |
| C modèles nouveaux / vraie frame nouvelle | **50 / 45 assertions** |
| Contrôles C / vues C | **3 658 / 363 097 assertions** |
| Vérification native réelle Odoo19 | **39 assertions**, en plus du driver PyLint générique |
| Oracles indépendants | **8 pour Python et GTK**, dont coloration6020cas ; **5 sous sanitizers** |

Les exclusions concernent uniquement les anciennes fixtures externes spécifiques (dont sources Python Odoo absentes), pas les ajouts. Le protocole/objets/services non concernés sont simulés quand nécessaire ; les vrais corps update/modal/historique sont exécutés. Widgets GTK remplacés sans display, vrais keyvals/Pango/Cairo RecordingSurface en mémoire, pas d’image/pixel-comparaison. Les anciennes sondes CPU de police raylib peuvent utiliser un atlas en RAM, sans fichier, fenêtre ou aperçu. Pas de preuve de bureau physique.

### Tests négatifs

Retirer le dispatch Ctrl+Espace navigateur uniquement dans l’include QA généré C fait échouer le test de frame. Changer la cible du vrai plugin de19 à18 fait disparaître W8165 et échouer la preuve native Odoo19. Neutraliser le callback Python en mémoire fait échouer le test de suggestions. Restauration réussie, production inchangée. Les `FAIL` dans les fichiers *mutant* sont **volontaires et attendus**, pas des échecs de livraison ; voir `mutation.json`.

## Non-régressions / corpus

Clavier AZERTY/redo/Ctrl+W, Unicode/AltGr, champs,256curseurs/deux volets et historiques ; F5 local ; explorateur CRUD ; sessions/brouillons et profils privés ; recherche deux panneaux/progressive/tous filtres/regex/barres/globalF3 et F11 uniquement dans la recherche ; Quitter après Effets ; macros/Odoo/chemins/XML/arch vide atomique ; GTK1.19 barres/mini-map glissables conservés.

Odoo XML18/19 réel revérifié SHA/bytes : recherche2fichiers/38occurrences/39étapes, option regex1fichier/24occurrences ; deux parcours chemin→ID→fragment, undoAZERTY/redo atomiques, XML stdlib et source intacte, dans Python et GTK. **13 cas SSH réels par édition** via AF_UNIX, fichiers distants seulement, exécution/analyse locales, pas d’auto-auth ou secrets enregistrés. Aucun serveur Odoo/base de données.

## Intégrité et livraison

**18 fichiers de backend/complétion protégés inchangés.** Les adaptateurs d’analyse sont intentionnellement étendus pour le plugin/XML ; les trois éditions restent réelles, pas des wrappers d’un autre éditeur. C : seul build maintenu **build-1.20.0**, raylib5.5 téléchargé/pinné sans override et source possédée/élaguée, ancien build1.18 retiré. Trois binaires stripés0755 identiques à la production, figés après la dernière configuration. Éditeur GLIBC2.38/helper2.34 ; backend Python raylibGLIBC2.17/SSE2/GL2.1 conservé, sans nouvelle preuve de bureau ancien. GTK utilise les bibliothèques système, sans raylib.

Runtime : {'python': '3.13.5', 'pylint': '4.0.10', 'pylint_odoo': '10.0.12', 'gtk': '4.18.6', 'pygobject': '3.50.0', 'cairo': '1.18.4', 'libraries': {'libpcre2-8': '10.46', 'libxml-2.0': '2.9.14'}, 'no_old_tar_gz': True}. PCRE2/libxml2/GTK/Pango/Cairo et analyzers Python externes non instrumentés par ASan et non embarqués comme nouvelles bibliothèques système. Installation neuve et lanceurs/aide/version/pip check réussis. Manifestes taille/mode/SHA256 de tous les fichiers sauf eux-mêmes ; pas de builds/caches/venv/ZIP/aperçu. Empreinte source C : `c8dfdbf180046fc66e5d16401dbd614777a72fba055b66223344507337ca8487`.

### Acceptation indépendante après extraction

Trois candidats extraits dans des dossiers neufs, tous les octets/modes vérifiés. C recompilé avec objets neufs et seulement copie de source tierce raylib : **37/37** ; Python **660/666 +6exclusions**, GTK **705/706 +1exclusion** avec driver natif extrait reconstruit, SSH et matrice externe. Huit oracles et workflows Odoo répétés pour les deux racines Python, installations/lanceurs/pip check et binaires fournis validés. Finalisation limitée aux preuves/manifeste1.20 : **tout le payload accepté conserve ses octets/modes**. Voir `extracted-acceptance.json`.

## Limites et reproduction

Aucun bureau physique/GPU/GNOME/compositeur/HiDPI/Windows/macOS validé ; pas de certification pixel à pixel. Lexeurs tolérants, XML heuristique/local et plugin externe ne remplacent pas une validation complète Odoo/DB. Colonnes scalaires/tab4, pas wcwidth ; gardes non assimilables à un sandbox hostile ; regex non temps réel, I/O noyau pouvant retarder un worker. Deux F401 natifs historiques et avertissements raylib amont conservés.

```bash
# Dans les racines Python/GTK, avec interpréteur QA disposant des dépendances :
PRISME_SSH_INTEGRATION=1 PRISME_NATIVE_PROFILES=/chemin/test_ssh PRISME_COMPAT_PYTHONS=/python-pylint2.17:/python-pylint2.12 python -B -W error::ResourceWarning -m unittest discover -s tests -v
# CMake : PRISME_REQUIRE_PYLINT_TESTS=ON, PRISME_REQUIRE_ODOO_TESTS=ON,
# PRISME_SSH_INTEGRATION=ON, PRISME_TEST_PYTHON=/python-pylint4-odoo,
# PRISME_TEST_COMPAT_PYTHONS="/python-pylint2.17;/python-pylint2.12"
ctest --test-dir build-1.20.0 --output-on-failure
python -I -B tools/check_languages.py /chemin/test_editor_features --python-root /chemin/python-studio --output /chemin/langues.json
```

Preuves : `tests/results-1.20.0/` ; synthèse : `tests/validation-summary-1.20.0.json`. Les anciens rapports décrivent uniquement leurs livraisons historiques dont les.tar.gz ont été supprimées à la demande. Aucun aperçu/capture/service créé.
