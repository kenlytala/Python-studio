# Validation — Python Studio 1.11.0 — édition C

Vérifications du **1 novembre 2026**, sans bureau graphique, sans aperçu, capture ni serveur web. Les deux éditions sont indépendantes ; la complétion des chemins de l’édition C est en C, pas un wrapper Python.

## Résultats réellement exécutés

| Vérification | Résultat |
|---|---|
| Suite complète Python | **369 réussites / 376 tests**, 7 exclusions, 0 échec ; 12.309 s |
| Nouveaux tests de dossiers Python | **46/46** : modèle, worker, entrées et rendu simulés du vrai contrôleur |
| C, production Release/OpenGL 2.1 | **29/29 groupes**, 9.66 s après lancement à chaud |
| ASan + UBSan + détection des fuites, C non-PIE | **12/12 groupes**, 4.61 s, aucun signalement |
| Oracle indépendant chemins C/Python | **54 contextes**, **39 éditions**, **37 historiques atomiques**, **5 instantanés de dossiers** |
| Oracle indépendant édition XML C/Python | **275 cas**, 266 documents bien formés, 268 historiques, 14 contextes de propositions |
| Oracle indépendant fragments XML C/Python | **48 documents valides**, 291 identifiants/fragments, 10 invalides refusés |
| XML réels Odoo 18/19 | Sources retéléchargées et hash-vérifiées, 46/47 identifiants ; **2 parcours complets du vrai contrôleur Python** depuis `sale/`, puis fichier, ID et arch vide, annulation/rétablissement |
| SSH réel | **12 cas dans chaque édition**, dont dossier → fichier → ID, projet distant et base distante depuis projet local ; source non modifiée et pas de nouvelle question d’authentification |
| Profils partagés Python/C | Cinq tests Python avec le véritable exécutable natif |

Le cœur natif de dossiers exécute **360 assertions** ; les contrôles de production **955**, plus **325 vérifications** de touches multicurseurs. Les chemins UTF-8 de plus de 95 octets gardent leur nom complet à l’insertion malgré le libellé natif abrégé. Les touches du contrôleur Python, Entrée/Tab et l’historique natif sont injectés sans fenêtre ; le parcours manuel natif utilise les fonctions de production liées à Ctrl+Espace, ce qui ne constitue pas une vérification physique du raccourci sur un bureau.

## Fonctionnement contrôlé

- `sale/` + Ctrl+Espace : contenu immédiat fusionné des bases XML, puis projet, parent du document et imports ; ordre/priorité/doublons, dossiers en premier, préfixe sensible à la casse.
- Sélection d’un dossier : `/` et prochain niveau immédiat ; fichier : nom complet ; chemin XML complet : sélecteur d’identifiants inchangé, éléments/champs et `arch` fermé vide.
- Aucun parcours de dossier sur frappe ordinaire, ni lecture du contenu des fichiers pour cette liste ; tests de thread, garde avant sondes, alias et montage hors ligne, annulation sans attente et rejet des résultats périmés.
- Guillemets ouverts/fermés, attributs, suffixes, espaces/délimiteurs, Unicode et positions UTF-8 natives ; limites de document, historique atomique, noms dangereux/spéciaux et erreurs explicites.
- Une demande : au plus 32 propositions, 8 192 entrées, 18 candidats et 4 095 octets UTF-8 de chemin ; pas de parcours récursif ou d’index global.
- XML/balises/macros, sessions/brouillons, PyLint/F5/imports et protections SSH précédents restent couverts par les régressions.

## Limites et exclusions

Les **sept exclusions Python** sont la matrice externe de versions PyLint, les anciennes glibc, les anciennes fixtures Python de complétion Odoo et quatre tests de bureau graphique. Les fixtures **XML** Odoo sont bien retéléchargées et réellement testées ici ; ne pas les confondre avec les fixtures Python exclues.

Pas de validation physique GPU/GNOME, Windows/macOS, schéma métier Odoo, ORM ou base de données. Les garde-fous SSH ne sont pas une sandbox générale contre un système de fichiers hostile. Une lecture bloquée dans le système de fichiers peut survivre à Échap ; l’interface ne joint pas le worker à la fermeture de la liste, mais une fermeture de l’application peut attendre son arrêt. Les sessions gardent du texte privé local en clair, jamais de secret d’authentification SSH.

Ruff est exécuté **en mode isolé, F,E9** : édition Python propre ; deux F401 historiques inchangés côté C (`tests/test_completion_bridge.py` et le bridge d’analyse protégé), nouveaux scripts propres. Pas de revendication de conformité à toutes les règles stylistiques. La construction froide émet quatre avertissements tiers `raylib/external/jar_mod.h:370` et la dépréciation CMake amont ; aucun avertissement propre au projet.

## Intégrité et distribution

Backend Python portable inchangé : SHA256 `2cbc839ab2ae06c120f3c83b66ba123b7d2df1360ed35ca409e0d39c3baa4850`, cible historique glibc 2.17/SSE2/OpenGL 2.1, sans nouveau test sous anciennes glibc. Le C précompilé demande GLIBC_2.38, le helper GLIBC_2.34 ; **libxml2.so.2 est requise et non embarquée**. Sources, notices et reconstruction restent fournies.

Une seule construction native de production, **build-1.11.0**, avec raylib 5.5 épinglé possédé dans ce dossier ; l’ancien build généré 1.10 est supprimé, pas ses archives. Binaries installés stripés/0755, dépendance inutilisée élaguée puis lanceur et CTest revérifiés. Les archives historiques et les scripts/backend protégés restent identiques aux empreintes de départ. Les environnements temporaires, caches et builds QA sont exclus des archives.

Empreinte source C : `99410fc0790606eb698bc223ba73937a3c074f647652ce63df58d32dd0000975`. `MANIFESTE-1.11.0.json` inventorie chaque fichier distribué sauf lui-même, sans hash circulaire. Les preuves sont dans `tests/results-1.11.0/` et le résumé JSON voisin ; anciens rapports/manifestes = versions historiques uniquement.

## Acceptation après extraction indépendante

Les deux archives candidates ont été extraites dans des dossiers neufs. Le C a été recompilé indépendamment, puis **29/29 groupes** sont passés ; l’édition Python extraite passe **369/376 tests, 7 exclusions** avec le véritable helper C recompilé et les SSH réels. Les lanceurs/version et les trois oracles ont été réexécutés ; les deux parcours Python sur XML Odoo réels passent à nouveau. Les sources fonctionnelles, leurs modes de distribution et le backend ne changent pas lors du repaquetage final, qui ajoute seulement les preuves. Détails : `tests/results-1.11.0/extracted-acceptance.json`.

## Reproduire sans affichage

```bash
PRISME_SSH_INTEGRATION=1 PRISME_NATIVE_PROFILES=/chemin/test_ssh \
  python -B -W error::ResourceWarning -m unittest discover -s tests -v
ctest --test-dir build-1.11.0 --output-on-failure
# Depuis la racine C, avec l’édition Python pour la parité :
python -I -B tools/check_path_complete.py /chemin/test_path_complete --python-root /chemin/python-studio
python -I -B tools/check_xml_edit.py /chemin/test_xml_edit --python-root /chemin/python-studio
python tools/fetch_xml_fixtures.py --directory /chemin/xml-fixtures
python -I -B tests/test_xml_parity.py /chemin/test_xml_refs --python-root /chemin/python-studio --fixtures /chemin/xml-fixtures
```
