# Validation — Python Studio 1.17.0 — édition C

Essais du **3 novembre 2026** sur les deux véritables éditions. Aucun aperçu, capture, serveur web ou bureau graphique généré.

## Configuration PyLint réellement intégrée

**Paramètres → Règles…** ouvre le nouveau champ natif. Codes (`W0611`, `C0114`) ou noms (`unused-import`, `missing-module-docstring`) ; normalisation de casse, séparateurs virgule/espace/point-virgule, dédoublonnage textuel. **Aucune exclusion par défaut.** Limites : **64 entrées, 4 095 octets, 95 caractères ASCII par nom** ; options, hooks et texte exécutable refusés.

Appliquer sauvegarde immédiatement la liste globale dans les préférences, invalide tous les caches de diagnostics et redemande l’analyse active, même en mode manuel. Annuler/Échap abandonne le brouillon ; Tout réactiver le vide sans modifier le réglage avant Appliquer. Collage trop long et sauvegarde échouée ne perdent ni la liste active précédente ni l’historique du brouillon. Documents, sélections, multicurseurs et historiques de code conservés.

La liste est transmise au **vrai PyLint par --disable**, avant la collecte et la limite de 256 diagnostics. Un corpus de plus de 256 avertissements donne ensuite seulement le véritable E0602 non exclu : ce n’est pas un masquage d’affichage. Les erreurs de syntaxe sont toujours vérifiées par compilation sans exécution avant PyLint, y compris avec E0001/syntax-error dans la liste. Les règles inconnues/retirées sont signalées depuis le catalogue de l’interpréteur choisi ; un avertissement annexe du programme ne peut pas masquer le nom de la règle en erreur.

Configuration globale propre à chaque édition, hors des projets. Python : clé `lint_disabled` des préférences ; C : profil `pylint-rules.cfg` atomique à côté des préférences. Le nouveau texte global **n’entre pas dans les captures/signatures/restaurations de sessions par projet**. Les anciennes préférences booléennes/numériques et les brouillons restent compatibles. Changer de projet ne restaure pas une ancienne liste ; la remise à zéro d’analyse SSH C conserve la configuration.

## Tests ciblés et compatibilité

- **31 nouveaux tests dédiés Python**, plus **2 régressions de pont** : grammaire/limites, stockage, vraie frame/clic Paramètres, appliquer/annuler/vider, sauvegarde échouée, vieux résultats refusés, champ AZERTY Ctrl+Z/Y, sessions/projets, UI en bornes, vrai PyLint et réactivation.
- **53 assertions** du modèle/profil C, **41 assertions** supplémentaires par la vraie chaîne **update → mouse/modal → contrôleur de configuration → champ/profil/cache**, **54 assertions de vrai PyLint** dans le driver C. Aucun corps d’annulation réécrit dans le banc.
- **3 023 cas** de parité des grammaires C/Python (79 acceptés, autres rejets attendus), limites et entrées malformées/injections. Les deux copies de grammaire stdlib sont identiques ; le parseur C est indépendant.
- **8 dispositions supplémentaires du vrai dialogue C**, avec état normal/erreur à quatre tailles ; boutons appliquer/annuler/vider dans les bornes, bouton Règles non superposé au réglage automatique. Dessin enregistré avec API simulées, sans image.
- Processus réels : **PyLint 4.1.3/Python3.12, 2.17.7/Python3.11 et 2.12.2/Python3.10**, codes et noms, restauration des messages, syntaxe, vrai E0602, règle inconnue et annulation de processus périmé. Les avertissements d’import d’anciennes dépendances sont capturés pour ne pas corrompre le protocole C qui fusionne stdout/stderr. Ces environnements de QA ne deviennent pas des dépendances embarquées.
- Configuration projet/utilisateur PyLint toujours remplacée par un rcfile nul comme auparavant ; aucun plugin ou init-hook projet activé, pas d’exécution du code analysé ni du framework. F5 et l’interpréteur choisi restent inchangés.

### Tests négatifs exigés

Retirer volontairement l’ajout --disable **uniquement dans l’include QA généré** réintroduit W0611 et fait échouer le test C. Le banc restauré réussit. Même résultat dans une **copie QA isolée du worker Python**, sans modification de la production. Les `FAIL` dans `lint-mutant.txt` et `python-lint-mutant.txt` sont donc **volontaires et requis**, pas des échecs de livraison. Voir `lint-mutation.json`.

## Résultats complets et fonctions conservées

| Vérification | Résultat |
|---|---|
| Python strict, ResourceWarning en erreurs, SSH réel, interop C, matrice externe PyLint | **607/613 réussis, 6 exclusions historiques**, 25.061 s |
| C production, configuration finale puis répétition | **35/35**, 40.18 s puis 33.67 s |
| ASan + UBSan + fuites, non-PIE | **21/21**, 10.47 s |
| Contrôles C | **3 534 assertions**, dont 41 configuration, 263 clavier, 848 vraie frame recherche/F11, 325 clavier multicurseur |
| Vues / moteur recherche / géométrie des barres | **357 712 / 434 / 4 900** |
| Oracles | **7 réussis**, dont 4 répétés sous sanitizers |

L’ancienne exclusion « interpréteurs PyLint externes non configurés » est maintenant exécutée ; les six autres exclusions historiques restent explicites. Le groupe C comprend les deux versions externes en plus des tests normaux. Les bibliothèques système et le processus Python/PyLint ne sont pas eux-mêmes instrumentés par ASan.

Ctrl+Z AZERTY, Ctrl+Maj+Z/Y et Ctrl+W, Unicode/AltGr, champs et multicurseurs restent couverts dans les vraies frames. F11 n’agrandit que la recherche quand elle est ouverte ; plein écran éditeur hors recherche inchangé. Barres h/v réellement manipulables, bornes de longues lignes cachées, clipping, publication progressive stable, filtres complets et F3 sur tous les fichiers conservés. Quitter reste en haut immédiatement après Effets.

Odoo XML **18/19 réel** revérifié par bytes/SHA256 épinglés : recherche 2 fichiers/38 occurrences/39 étapes, options 1 fichier/24 occurrences ; deux parcours `sale/ → views/ → XML → ID → élément/arch fermé vide`, puis vraie annulation AZERTY/rétablissement. Sources intactes et XML contrôlé par stdlib ; pas d’ORM/base/serveur Odoo. **13 cas SSH réels dans chaque édition**, fichiers distants uniquement, Python/PyLint locaux, pas d’authentification automatique ou de secrets enregistrés.

## Intégrité et livraison

**51 archives/sidecars historiques et 12 fichiers de backend graphique/complétion protégés inchangés**. Les **deux adaptateurs PyLint sont intentionnellement étendus** pour cette fonctionnalité, pas prétendus inchangés. Backend Python glibc2.17/SSE2/OpenGL2.1 et correctifs conservés octet pour octet.

C : seule production **build-1.17.0**, raylib5.5 épinglé téléchargé sans override, source tierce possédée par ce build puis élaguée ; ancien build1.16 retiré. Trois binaires stripés 0755 identiques à la production, figés après la dernière configuration/reconstruction. PCRE2-8 **10.46** / libxml2 **2.9.14** dynamiques non embarqués, aucune dépendance introuvable. Éditeur C GLIBC2.38, helper2.34 ; pas de nouvelle preuve de bureau ancien. Installation Python neuve, lanceurs, aide/version principale et pip check réussis.

Archives tar/gzip déterministes, sans build/cache/venv/ZIP/aperçu. Manifestes de tous les fichiers distribués sauf eux-mêmes, avec tailles/modes/SHA256. Empreinte source C : `5f190b59835d9e2dadbb95f0d33becfa0e55f55d592de78b543fdff84ce8c546`.

### Acceptation indépendante des archives extraites

Deux candidats extraits dans des dossiers neufs, octets/modes confrontés au manifeste. C recompilé avec **objets neufs** et seulement une copie de source tierce raylib : **35/35 groupes**. Python extrait : **607/613 réussis et 6 exclusions**, SSH réels, versions PyLint externes et interop avec le driver C extrait reconstruit. Sept oracles et deux parcours Odoo répétés et identiques ; installations/lanceurs et binaires fournis vérifiés. Finalisation limitée aux preuves/manifeste de 1.17 : **tout le payload source/binaire/documentation accepté conserve ses octets et modes**. Voir `tests/results-1.17.0/extracted-acceptance.json`.

## Limites et reproduction

Tests de vraies fonctions avec API graphiques/OS simulées : aucune preuve de bureau physique, GPU, GNOME, Windows ou macOS. Pas d’aperçu/capture/serveur. Colonnes scalaires/tabulations4, pas wcwidth ; budgets regex non temps réel, gardes non assimilables à un sandbox hostile et I/O noyau pouvant retarder la terminaison. Deux F401 historiques des outils/tests natifs et avertissements amont raylib conservés. Les secrets SSH ne font jamais partie de cette configuration.

```bash
PRISME_COMPAT_PYTHONS=/python-pylint-2.17:/python-pylint-2.12 PRISME_SSH_INTEGRATION=1 PRISME_NATIVE_PROFILES=/chemin/test_ssh python -B -W error::ResourceWarning -m unittest discover -s tests -v
# CMake : PRISME_REQUIRE_PYLINT_TESTS=ON, PRISME_SSH_INTEGRATION=ON,
# PRISME_TEST_PYTHON=/python-pylint-4,
# PRISME_TEST_COMPAT_PYTHONS="/python-pylint-2.17;/python-pylint-2.12"
ctest --test-dir build-1.17.0 --output-on-failure
python -I -B tools/check_lint_rules.py /chemin/test_lint_rules --python-root /chemin/python-studio --output /chemin/regles.json
```

Preuves : `tests/results-1.17.0/` ; synthèse : `tests/validation-summary-1.17.0.json`. Les anciens rapports/manifeste décrivent seulement leurs archives historiques. Dépendances de QA (Paramiko, FUSE/SSHFS, compilateur/sanitizers, interpréteurs de matrice) distinctes de l’application.
