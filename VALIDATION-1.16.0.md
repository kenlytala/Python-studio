# Validation — Python Studio 1.16.0 — édition C

Essais exécutés le **2 novembre 2026** sur les deux véritables éditions. Aucun aperçu, capture, serveur web ou bureau graphique généré.

## Défaut reproduit et correction

Sur AZERTY, la touche marquée Z occupe la position GLFW **KEY_W/AD02 (87)**. Avant correction, la vraie frame Python fermait l’onglet propre contenant une modification et un historique : **2 onglets → 1**, sans annulation. Ce relevé préalable est conservé dans `keyboard-baseline.json`.

Les deux éditions lisent maintenant la disposition en lecture seule, puis projettent les événements vers les lettres avant le dispatch. Les requêtes natives pressées/répétées/maintenues passent aussi par la projection inverse : normaliser la seule file aurait laissé Ctrl+W actif sur la position brute. Alphabet latin complet, unique, accepté atomiquement sur **49 positions** imprimables ; sinon dernière carte valide, ou QWERTY initial. Pas de permutation Z/W prédéfinie dans la production.

Linux : noms/alias physiques XKB, groupe courant puis autre groupe latin complet déjà chargé, sans keycodes evdev supposés ni modification du système. Bibliothèque X11 Python mise en cache, pas la disposition. ABI ctypes confrontée au vrai compilateur et aux en-têtes natifs : cinq structures, tailles et offsets utilisés. Windows : code de lecture MapVirtualKey sans ToUnicode/activation, double d’API Python seulement. macOS : branche GLFW C non validée ; Python sans fournisseur macOS. Voir **GUIDE-CLAVIER.md** pour les limites et le repli en absence de fournisseur.

## Preuve par les vrais routeurs

- **Python : 41 nouveaux tests** du modèle, lecteurs et vraie frame. Ctrl+Z AZERTY annule sans fermer, même sur un onglet propre avec plusieurs états d’historique ; Ctrl+Maj+Z et Ctrl+Y rétablissent ; le vrai Ctrl+W conserve la fermeture.
- **C : 263 assertions supplémentaires** passant réellement par **update → editor_keys → historique**. Les corps sont extraits de la production, pas réécrits dans le test. Le banc enregistre l’action de fermeture, et vérifie qu’elle n’est pas demandée par Z mais l’est par W. API OS et services non concernés simulés.
- **Quatre dispositions × quatre canaux** : QWERTY, AZERTY, QWERTZ, Dvorak ; file, pressé, répété, file+pressé. Vérification des lettres A et M déplacées, suppression de l’ancien alias, Ctrl/Super et contrôles gauche/droit, changement sans relâcher Ctrl, relecture périodique et conservation du dernier relevé valide.
- Histoires séparées des champs de recherche, dialogue et recherche de dossier ; brouillons de snippets et profil SSH non secret ; vrais snippets C en pressé/répété. Édition multicurseur UTF-8 et XML automatique annulés/rétablis atomiquement. File Unicode/AltGr non réécrite.
- **14 151 assertions** du modèle C, dont 200 permutations de lettres sur les positions imprimables et rejets atomiques ; **648 assertions** du vrai fournisseur XKB lié à des doubles Xlib : alias, keycodes non evdev, groupes, Unicode et libération en cas d’échec. Aucun serveur X utilisé.
- Parité indépendante : **4 096 valeurs** de projections C/Python comparées, plus ABI XKB. Les API de changement de disposition et de consommation des touches mortes ne sont jamais utilisées.

### Tests négatifs requis

Désactiver la conversion uniquement dans l’include généré du banc C réintroduit la demande de fermeture et fait **échouer** l’assertion d’absence de fermeture. Le banc restauré passe. Neutraliser la lecture dans le seul objet Python en mémoire fait également échouer le nouveau test de frame AZERTY. Les sources de production ne sont pas modifiées pendant ces essais. Les `FAIL` dans `keyboard-mutant.txt` et `python-keyboard-mutant.txt` sont **volontaires et attendus**, pas des échecs de la livraison. Empreintes des vrais corps exécutés dans `keyboard-mutation.json`.

## Régressions et résultats

| Vérification | Résultat |
|---|---|
| Python complet, ResourceWarning en erreurs, SSH réel et interop C | **573/580 réussis, 7 exclusions historiques**, 15.927 s |
| Python clavier / recherche dédiée | **41 / 162 tests réussis** |
| C, production froide puis répétition | **32/32**, 20.52 s puis 11.61 s |
| ASan + UBSan + fuites, non-PIE | **19/19**, 2.11 s ; bibliothèques système tierces non instrumentées |
| Contrôles C | **3 493 assertions**, dont 263 clavier, 848 vraie frame recherche/F11, 325 clavier multicurseur |
| Recherche / vues / géométrie des barres | **434 / 356 256 / 4 900** |
| Oracles | **6 réussis** ; options/littéral/clavier également sous sanitizers |

F11 recherche uniquement, plein écran éditeur hors recherche, barres de contenu manipulables et bornées, lignes cachées très longues, clipping, prise/relâchement souris, F3 tous fichiers, résultats progressifs et tous les filtres restent couverts. Quitter reste en haut immédiatement après Effets. Aucun changement d’interface demandé pour ce correctif.

Odoo XML **18/19 réel**, bytes/SHA256 épinglés et revérifiés : recherche 2 fichiers/38 occurrences/39 étapes, options 1 fichier/24 occurrences ; deux vrais parcours Python `sale/ → views/ → XML → ID → élément/arch fermé vide`, cette fois avec **Ctrl+Z AZERTY dans la vraie frame** puis Ctrl+Y. Sources intactes, stdlib XML et historiques vérifiés. Pas d’ORM, de base de données ou de serveur Odoo exécuté. **13 cas SSH réels dans chaque édition**, fichiers distants seulement et Python/PyLint locaux ; pas d’authentification automatique ou de secrets enregistrés.

## Intégrité et distribution

**47 archives/sidecars historiques et 14 fichiers protégés inchangés**. Backend graphique Python glibc 2.17/SSE2/OpenGL 2.1 et trois correctifs conservés octet pour octet. C : seule production **build-1.16.0**, raylib 5.5 épinglé téléchargé sans override, source tierce possédée par ce build puis élaguée ; ancien build 1.15 supprimé. Trois binaires stripés 0755 identiques aux copies de production. Une reconfiguration peut relier des sorties non stripées ; la copie distribuée a été figée après la dernière configuration/construction.

PCRE2-8 **10.46** et libxml2 **2.9.14** dynamiques/non embarqués, aucune bibliothèque introuvable. Éditeur C GLIBC2.38, helper2.34 ; backend Python2.17 inchangé, sans nouvelle revendication de bureau ancien. Installation Python neuve, lanceurs, aide/version du programme principal et pip check réussis. Pas de build/cache/venv/ZIP/aperçu dans les archives. Manifestes de taille/mode/SHA256 de tous les fichiers distribués sauf eux-mêmes. Empreinte source C : `d94c9b806e935c2a6d1eb49b331fc2b64af2dd49e80539d06de0b6e14a4b5d5e`.

### Acceptation indépendante des archives extraites

Archives candidates extraites dans des dossiers neufs ; chaque fichier et mode confronté au manifeste. C recompilé avec **objets neufs**, réutilisant seulement une copie de la source tierce raylib : **32/32 groupes**. Python extrait : **573/580 réussis et 7 exclusions**, SSH réels/interop avec le driver C extrait reconstruit. Six oracles et deux parcours Odoo répétés et identiques ; installations/lanceurs et binaires fournis vérifiés. La finalisation ne change que les preuves et le manifeste de 1.16 : **tout le payload source/binaire/documentation déjà accepté conserve ses octets et modes**. Voir `tests/results-1.16.0/extracted-acceptance.json`.

## Limites et reproduction

API graphiques simulées : pas de preuve de bureau physique AZERTY, GPU, GNOME, Windows ou macOS. Lecteur XKB exercé sans connexion à un serveur. Sept exclusions historiques, deux F401 historiques C/Python et avertissements amont raylib conservés. Colonnes scalaires/tabulations4, pas wcwidth ; gardes de fichiers non assimilables à un sandbox hostile, budgets regex non temps réel, I/O noyau pouvant retarder un worker. Seul le dossier de recherche est persisté, jamais les secrets SSH.

```bash
PRISME_SSH_INTEGRATION=1 PRISME_NATIVE_PROFILES=/chemin/test_ssh python -B -W error::ResourceWarning -m unittest discover -s tests -v
ctest --test-dir build-1.16.0 --output-on-failure
python -I -B tools/check_keyboard.py /chemin/test_keyboard /chemin/test_keyboard_native --python-root /chemin/python-studio --output /chemin/clavier.json
```

Dépendances de QA (Paramiko, SSHFS/FUSE, compilateur, sanitizers) distinctes de l’exécution. Preuves de cette version : `tests/results-1.16.0/` ; synthèse : `tests/validation-summary-1.16.0.json`. Les rapports et manifestes plus anciens ne décrivent que leurs archives historiques.
