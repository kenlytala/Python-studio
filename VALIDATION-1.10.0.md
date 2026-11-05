# Validation — Python Studio 1.10.0 — édition C

Vérifications exécutées le **1 novembre 2026**, sans bureau graphique, sans aperçu ni capture. Les deux éditions restent de véritables implémentations distinctes ; la création/fermeture de balises du C est en C, sans délégation à Python.

## Résultats source et production

| Vérification | Résultat |
|---|---|
| Édition Python, suite complète | **323 réussites / 330 tests**, 7 exclusions, 0 échec, 18.601 s |
| Nouveaux tests Python XML/chemins/historique | **43 / 43**, modèles + contrôleurs de production avec entrées/affichage simulés |
| Édition C, production Release/OpenGL 2.1 | **28 / 28 groupes**, 11.3 s |
| ASan + UBSan + fuites, sélection C seulement | **11 / 11 groupes**, 12.32 s, zéro signalement |
| Oracle indépendant d’édition XML C/Python | **275 cas**, 266 documents relus par la bibliothèque standard, 268 transactions d’historique, 14 contextes de suggestions |
| Oracle indépendant des fragments XML C/Python | **48 documents valides**, 291 identifiants/fragments, 10 documents invalides refusés |
| XML réels Odoo 18/19 complets | 46 / 47 identifiants ; **6 cas d’édition complets C/Python**, XML bien formé, curseurs/annulation/rétablissement ; 2 cas supplémentaires avec vrai contrôleur Python et base configurée |
| SSH réel de QA | **12 cas dans chaque édition**, SSHFS/OpenSSH/SFTP, source XML non modifiée, base distante configurée depuis un projet local, annulation/rétablissement ; pas de Python distant |
| Profils SSH partagés Python/C | **5 tests Python** avec le véritable exécutable C |

Les 28 groupes C couvrent les modèles, sessions/brouillons, multicurseurs, fichiers, profils SSH, parseur XML et workers, contrôleur d’insertion, lexeur/édition/historique/bases XML, complétion, analyse PyLint réelle, vues simulées et échecs graphiques propres. Les nouveaux contrôleurs couvrent les 18 candidats distincts (8 bases XML + projet + dossier du document + 8 imports), leur priorité, la persistance indépendante, le retrait/réordre hors ligne, les alias/montages réservés et les raccourcis d’historique réellement partagés par l’éditeur et les champs. Les vues à 960×640 avec huit racines sont vérifiées sans dessiner de fenêtre réelle.

Le cœur C d’édition/historique/profils XML exécute **818 assertions** ; le contrôleur de références XML **195**, le parseur **104** et les vues simulées **345 405**. Les raccourcis sont injectés dans les vrais contrôleurs ; cela ne prouve pas leur disponibilité physique lorsqu’un bureau les intercepte.

## Portée fonctionnelle contrôlée

- Huit bases XML **par projet**, séparées des imports Python ; ajout/retrait/priorité enregistrés sans affecter F5/PyLint ou lancer un scan d’identifiants global.
- Chemins saisis directement dans le document, Ctrl+Espace, choix d’un identifiant et remplacement atomique ; les champs `arch` restent explicitement fermés et vides.
- Création après `>`, fermeture du parent après `</`, indentation, suggestions locales/courantes, Unicode et noms qualifiés ; pas de doublon avec une fermeture déjà présente après du texte ou un sous-élément.
- Attributs entre guillemets, commentaires, CDATA, PI et déclarations opaques ; sélection/collage littéraux ; budgets et limites conservateurs documentés dans GUIDE-XML.md.
- Ctrl+Z, Ctrl+Y et Ctrl+Maj+Z : texte et positions/sélections/multicurseurs rétablis atomiquement, pas de fausse entrée pour un simple saut par-dessus `>`, branche de rétablissement invalidée par une nouvelle saisie.
- Macros globales (dont `xml_form`), XML 1.9, menus contextuels alignés, sessions et protections SSH préservés.

## Exclusions, avertissements et corrections de QA

Les **sept exclusions Python** restent : une matrice externe de versions PyLint, les sondes réelles d’anciennes glibc, les anciennes fixtures Python de complétion Odoo et quatre tests de bureau graphique. **Les nouvelles fixtures XML Odoo 18/19 sont, elles, effectivement téléchargées, hash-vérifiées et testées** ; ne pas confondre ces deux familles de fixtures.

Aucune validation physique de bureau/GPU/GNOME, Windows/macOS, schéma métier Odoo, ORM ou base de données. Les garde-fous SSH ne sont pas une sandbox générale contre un système de fichiers hostile. Les sessions peuvent contenir les textes des documents **en clair dans un journal privé local** ; aucun secret d’authentification SSH n’est ajouté.

**Ruff est exécuté explicitement avec les règles F,E9 en mode isolé** : Python propre ; C, deux F401 historiques inchangés, nouveaux scripts propres. Cela n’est pas une revendication de conformité à toutes les règles stylistiques possibles. La construction C à froid émet quatre avertissements tiers `raylib/external/jar_mod.h:370` et une dépréciation CMake amont ; **zéro avertissement du code du projet**. Le tampon d’ordinal du nouveau dialogue de bases a été corrigé avant la validation finale.

Sous charge CPU simultanée, une première régression Python a observé un descendant entre réception de SIGKILL et état final `/proc`. Le test attend maintenant, pendant deux secondes au maximum, sa disparition ou son état zombie, au lieu d’exiger un changement d’état instantané. La condition finale n’a pas été affaiblie et le runner de production n’a pas changé. La suite finale ci-dessus passe. Les corrections initiales de fixtures (candidat de document distinct, noms Unicode supportés par Expat) ne remplacent pas les vérifications croisées : les sorties attendues et le parseur standard sont indépendants des deux lexeurs testés.

## Compatibilité et intégrité

Backend Python x86_64 **inchangé**, SHA256 `2cbc839ab2ae06c120f3c83b66ba123b7d2df1360ed35ca409e0d39c3baa4850`, cible historique glibc 2.17/SSE2/OpenGL 2.1 (pas de nouveau test sous anciennes glibc). Précompilé C et démonstration : GLIBC_2.38 ; helper : GLIBC_2.34. C requiert **libxml2.so.2**, non embarquée, compilation libxml2 2.9+.

Une seule construction native de production : **build-1.10.0**, dépendance épinglée possédée dans ce dossier, anciens objets de production supprimés ; les anciennes archives et leurs sommes restent inchangées. Les constructions QA/ASan sont séparées et exclues des archives. Les exécutables installés sont stripés et ont le mode 0755 ; détails dans les preuves C.

Empreinte fonctionnelle Python : `0f22b0561a6967fe1f057f7ba7d325e602553319bf7ce0867d82801faad6644b`. Empreinte cohérente C : `22008fe1b5093bda6517cdf8f4f49a2b48f9088d8fe54800425aaea0265aec82`. Les scripts de complétion/analyse protégés et le backend portable sont inchangés. `MANIFESTE-1.10.0.json` inventorie chaque fichier distribué sauf lui-même, pour éviter un hash circulaire ; les anciens manifestes ne concernent que leurs archives historiques.

## Reproduire

```bash
# Python : environnement avec raylib 5.5.0.4, PyLint et Paramiko pour les SSH réels.
PRISME_SSH_INTEGRATION=1 PRISME_NATIVE_PROFILES=/chemin/test_ssh \
  python -B -W error::ResourceWarning -m unittest discover -s tests -v
# C : configurer avec les deux Python de QA et PRISME_REQUIRE_PYLINT_TESTS=ON,
# PRISME_SSH_INTEGRATION=ON, puis construire et lancer ctest.
ctest --test-dir build-1.10.0 --output-on-failure
# Fixtures XML : téléchargement explicite, refus si les octets ne sont plus épinglés.
python tools/fetch_xml_fixtures.py --directory /chemin/fixtures-xml
# Dans la racine C :
python -I -B tools/check_xml_edit.py /chemin/test_xml_edit --python-root /chemin/python-studio
python -I -B tests/test_xml_parity.py /chemin/test_xml_refs \
  --python-root /chemin/python-studio --fixtures /chemin/fixtures-xml
```

## Archives extraites

**Validation indépendante après extraction réussie.**

- Python : **330 tests, 323 réussites, 7 exclusions**, zéro échec/erreur, **13.794 s**, avec le helper de profils issu de la reconstruction C extraite.
- C : configuration/reconstruction depuis les sources extraites, empreinte identique ; **28 / 28 groupes**, **11.4 s**. Précompilé extrait exécutable et version 1.10.0 vérifiés.
- Les deux lanceurs source ont été exécutés avec `--version` ; environnement Python de QA provisoire retiré, construction C de QA explicitement séparée. Aucun lancement graphique.
- Les oracles d’édition (275 cas) et de fragments (291), ainsi que les six éditions dans les deux XML Odoo complets et les deux contrôleurs Python avec bases configurées, ont été répétés contre les fichiers/exécutables extraits.
- **162 fichiers fonctionnels strictement identiques**, contenu et mode, entre les sources livrées et l’archive candidate réellement testée. Après cette acceptation, seuls les rapports/preuves/résumés/inventaires ont été complétés ; aucun changement fonctionnel.

Les archives finales sont contrôlées fichier par fichier avec leurs manifestes, sans liens ni chemins dangereux. TAR ordonné, date fixe, UID/GID nuls, GZIP mtime=0, modes 0644/0755 ; une seconde production vérifie l’égalité binaire des archives. Les sidecars SHA256 permettent de contrôler les téléchargements. Les anciens fichiers d’archives et sidecars restent inchangés.

Preuves : `tests/results-1.10.0/` ; commandes, détails et exclusions structurés dans `tests/validation-summary.json`.
