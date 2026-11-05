# Prisme 1.5.1 — validation et nettoyage

31 octobre 2026. Interface C11/raylib ; aucun aperçu, capture ni service graphique créé.

## Ctrl + Alt + Haut / Bas

- Gestionnaire natif réel testé sur événements maintenus, capturés dans la même image et répétés ; Ctrl et Alt gauches/droits.
- Ajouts au-dessus / au-dessous, changement de direction sans doublon ni perte des curseurs existants.
- Colonne visuelle d’origine pendant un geste continu, lignes courtes/vides, tabulations, UTF-8 et sélections existantes.
- Saisie simultanée après relâchement des modificateurs, annulation/rétablissement atomiques, limites de document et 256 curseurs.
- Pas de détournement des flèches ordinaires, du texte AltGr, de la recherche, de la console ou des dialogues ; contexte réinitialisé après changement de volet/édition/relâchement.

Le noyau multi-curseur passe **27/27 groupes** dont **196 matrices de placement** ; le vrai contrôleur clavier passe **316 vérifications supplémentaires** (405 au total dans le test du contrôleur).

| Construction | CTest | Durée |
|---|---:|---:|
| Release Linux x86-64 / OpenGL 2.1 | **25/25** | 44.59 s |
| Debug ASan + UBSan + détection des fuites | **25/25** | 41.29 s |

Les deux matrices incluent les six versions réelles de PyLint 2.7.4, 2.12.2, 2.13.9, 2.17.7, 3.3.9 et 4.1.3, la complétion Odoo statique, les macros globales, UTF-8/historique, fichiers, typographie et géométrie. Aucun avertissement de compilation des sources Prisme ; quatre avertissements connus de `jar_mod.h` amont lors de la première compilation fraîche Release.

Empreinte cohérente des sources/tests : `81183d7fce358a26e64a89e15cfd99f4bd9e1d8802b8432f2ceeaffda730487a`.

Runner/F5, analyse PyLint, complétion Odoo, macros, profils d’import, noyau des transactions texte et trois patches raylib inchangés par rapport à 1.5.0. Le placement des curseurs et le contrôleur clavier ont leurs tests dédiés.

## Nettoyage demandé

- **15 archives ZIP** supprimées à la racine du workspace, aucun nouveau ZIP créé.
- **9 dossiers `verification-*`** regroupés dans **`verifications-2026-10-31.tar.gz`**, puis supprimés.
- **332 fichiers** vérifiés par SHA-256, tailles et permissions avant suppression des dossiers.
- L’archive de vérification conserve les neuf arborescences et un `MANIFESTE-verifications.json` avec les empreintes et la liste des ZIP supprimés.
- Taille de cette archive : 2036274 octets ; SHA-256 : `35f05d6cb448bcecaf5e304231be30237fa4497c827d59747006b25f83a0a405`.

La nouvelle livraison utilise **`.tar.gz`** pour ne pas recréer d’archive ZIP. Les données internes des dépendances Python installées ne sont pas des archives de livraison et n’ont pas été modifiées.

## Limites de validation

Tests CPU et entrées clavier simulées : aucun essai visuel sur un bureau physique, Windows/macOS non compilés ici. Si le gestionnaire de fenêtres intercepte Ctrl+Alt+Haut/Bas, il faut réaffecter ce raccourci dans les paramètres système ; l’éditeur ne modifie pas ces réglages. Aucune exécution d’Odoo ni écriture des documents par l’analyse.
