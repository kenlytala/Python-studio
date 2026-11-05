# Validation — Python Studio1.21.0 — édition C

**4 novembre2026.** Bouton de réduction à l’extrême droite du menu, dans les trois éditions réelles. Aucune fenêtre d’éditeur, capture, aperçu ou serveur graphique/web généré.

## Action et placement

Un bouton **−** avec infobulle « Réduire l’éditeur sans quitter » termine le menu supérieur. Dans Python/raylib et GTK, le bouton existant situé plus à gauche est **déplacé**, pas dupliqué. C reçoit une nouvelle action native. Le groupe est réorganisé sans toucher à l’ordre **Effets → Quitter** ; Quitter, agrandir/restaurer, plein écran et repli du menu restent distincts. Dans C, le repli du menu est déplacé avant les autres commandes pour libérer la position extrême droite ; le budget de largeur de la palette réserve cet espace.

L’action appelle **MinimizeWindow** en C/raylib et **Gtk.Window.minimize** via le backend GTK. Elle ne quitte pas, ne ferme pas de document, ne replie pas les panneaux, n’arrête ni l’exécution ni SSH, ne change pas la source, les curseurs/sélections/multicurseurs ou les piles undo/redo. Les documents restent en mémoire et les brouillons/sessions ne sont pas remplacés par une décision de fermeture. La minimisation native est une demande au gestionnaire de fenêtres, pas une fermeture cachée.

## Tests nouveaux

- **5 tests communs** dans chacune des deux éditions Python : une seule poignée/bouton tout à droite, bornes et non-superposition sur5tailles, Quitter après Effets, vrai clic avec document sale et historique intact, répétition sans quitter/arrêter/déconnecter, absence de hit caché quand le menu est replié, modal bloquant le bouton du fond.
- **1 test GTK** appelle le vrai corps du backend et vérifie sa délégation vers `Gtk.Window.minimize` sur un objet de fenêtre contrôlé ; aucune destruction/plein écran, fenêtre toujours considérée prête. Méthode GTK réelle vérifiée par introspection, pas de construction de widget graphique.
- **10 assertions C** passent par la vraie update/mouse et le **vrai corps app_minimize_window extrait de la production**. L’appel natif est compté par le banc, sans fenêtre. Répétition, absence d’action si app arrêtée/null, document sale/version/curseur/multicurseurs/historique, état de fermeture/session/SSH inchangés.
- Les vraies vues C vérifient bouton unique, icône−/infobulle, position plus à droite que toutes les autres zones et absence de recouvrement dans **192 états d’interface**, avec tailles/volets/menu/panneaux différents. Les contrôles de géométrie sont partagés avec les tests de vues précédents.

### Contre-épreuves

Neutraliser uniquement l’appel MinimizeWindow dans l’include QA généré fait échouer le test C du compteur natif. Remplacer uniquement en mémoire le callback Python par Quitter fait échouer le test du clic sur document modifié. Restauration réussie, aucune source de production modifiée. Les `FAIL` dans les fichiers *mutant* sont **attendus**, pas des échecs de livraison. Voir `minimize-mutation.json`.

## Résultats et fonctions conservées

| Vérification | Résultat |
|---|---|
| Python strict, ResourceWarning en erreurs, SSH réel/interop et interpréteurs externes | **665/671 réussis, 6 exclusions historiques**, 40.354s |
| GTK strict, mêmes options | **711/712 réussis, 1 exclusion historique**, 39.309s |
| C production complète finale | **37/37**, 59.40s |
| ASan/UBSan/fuites, non-PIE | **23/23**, 41.99s |
| Contrôles C / vues C | **3 668 / 366 745 assertions** |
| Oracles indépendants | **8 pour Python et GTK**, dont coloration JS/XML6020cas ; **5 sous sanitizers** |

Les exclusions concernent les anciennes fixtures externes non configurées, pas la minimisation. Les fenêtres/appels OS sont simulés, les vrais corps de contrôleurs/vues sont exercés. Cairo RecordingSurface commande-vectorielle seulement, pas d’image/pixel-comparaison. Les sondes CPU de police raylib héritées utilisent éventuellement un atlas en RAM, jamais un fichier ou aperçu.

Toutes les fonctions1.20 sont conservées : Ctrl+Espace dans les navigateurs, filtres/modes/undo du champ, coloration Python/JS/XML et cache par langue, vrai pylint-odoo10.0.12/PyLint4.0.10 ciblant19.0, vérification XML statique/DTD interdit/arch fermé vide, réglages Odoo par projet et génériques globaux, bouton− de règle distinct. Cet ajout ne réutilise pas le bouton de désactivation d’une règle pour agir sur la fenêtre.

Undo/redo AZERTY/Ctrl+W, Unicode/AltGr,256curseurs/deux volets ; F5 et entrée/sortie/arrêt locaux ; explorateur CRUD ; sessions/brouillons/profils privés ; SSH fichiers seulement/no auth automatique/secrets ; recherche progressive, tous filtres, F3 tous fichiers et F11 recherche seulement ; GTK1.19 barres/minimap glissables conservés.

Odoo XML18/19 réel, bytes/SHA256 revérifiés : recherche2fichiers/38occurrences/39étapes, filtre regex1fichier/24occurrences, deux workflows chemin→ID→fragment/arch vide avec undoAZERTY/redo, source intacte et XML stdlib, répétés pour les deux racines Python. **13 cas SSH réels par édition** AF_UNIX, pas de serveur TCP/hôte externe. Aucun ORM, serveur Odoo ou base.

## Intégrité et distribution

**24 fichiers non concernés de backend/analyse/complétion protégés inchangés**, **6 archives/sidecars1.20 conservés octet pour octet**. Les anciennes.tar.gz supprimées à la demande restent absentes ; nouveaux paquets.tar.xz, aucun ZIP. Les anciens rapports/manifeste ne décrivent que leurs livraisons historiques.

C : seul build maintenu **build-1.21.0**, raylib5.5 téléchargé/pinné sans override, source possédée et élaguée, ancien build1.20 retiré. Trois binaires stripés0755 identiques à la production, figés après la dernière configuration. Éditeur GLIBC2.38/helper2.34 ; backend Python raylibGLIBC2.17/SSE2/GL2.1 et corrections conservés ; pas de nouvelle preuve de bureau ancien. GTK utilise sa pile système sans bibliothèque raylib.

Runtime : {'python': '3.13.5', 'pylint': '4.0.10', 'pylint_odoo': '10.0.12', 'gtk': '4.18.6', 'pygobject': '3.50.0', 'cairo': '1.18.4', 'libraries': {'libpcre2-8': '10.46', 'libxml-2.0': '2.9.14'}}. Bibliothèques système et analyzers externes non instrumentés par ASan. Installation neuve/lanceurs/aide/version/pip check réussis. Archive déterministe, manifeste taille/mode/SHA256 de tous les fichiers sauf lui-même, pas de builds/caches/venv/preview dans le paquet. Empreinte source C : `bdca0767cb8d60d798551f8b754cd1f23180450f2417020dc4dcfdb0ccb37196`.

### Acceptation indépendante après extraction

Trois candidats extraits dans des dossiers neufs, octets/modes vérifiés. C recompilé avec objets neufs et seule copie de source tierce raylib : **37/37** ; Python **665/671+6exclusions**, GTK **711/712+1exclusion**, avec driver C extrait reconstruit, SSH et matrice externe. Huit oracles et deux workflows Odoo répétés et identiques pour les deux racines Python ; installations/lanceurs/pip check et binaires fournis validés. Tout le payload accepté conserve ses octets/modes ; la finalisation ne change que les preuves/manifeste1.21. Voir `extracted-acceptance.json`.

## Limites et reproduction

Aucun bureau physique/GPU/GNOME/compositeur/HiDPI/Windows/macOS validé, pas de certification pixel à pixel. La demande de minimisation reste soumise aux règles du gestionnaire de fenêtres. Les widgets/OS sont remplacés où un affichage serait requis. Pas de sandbox hostile ni regex temps réel ; colonnes scalaires/tab4, pas wcwidth, I/O noyau pouvant retarder un worker. Deux F401 natifs historiques et avertissements raylib amont conservés.

```bash
PRISME_SSH_INTEGRATION=1 PRISME_NATIVE_PROFILES=/chemin/test_ssh PRISME_COMPAT_PYTHONS=/python-pylint2.17:/python-pylint2.12 python -B -W error::ResourceWarning -m unittest discover -s tests -v
# CMake : PRISME_REQUIRE_PYLINT_TESTS=ON, PRISME_REQUIRE_ODOO_TESTS=ON,
# PRISME_SSH_INTEGRATION=ON, PRISME_TEST_PYTHON=/python-pylint4-odoo,
# PRISME_TEST_COMPAT_PYTHONS="/python-pylint2.17;/python-pylint2.12"
ctest --test-dir build-1.21.0 --output-on-failure
```

Preuves : `tests/results-1.21.0/` ; synthèse : `tests/validation-summary-1.21.0.json`. Dépendances de QA distinctes des dépendances d’application. Aucun aperçu/capture/service généré.
