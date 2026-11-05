# Prisme C 1.1.1 — liaison de `buf_apply_edits`

> Protection conservée en 1.5.1 ; les commandes ci-dessous utilisent le dossier de construction de la version actuelle.

## Symptôme

```text
libprisme_core.a(multicursor.c.o): undefined reference to `buf_apply_edits'
[100%] Built target test_core
... test_multi ... Error 1
```

`buf_apply_edits` est une fonction publique **définie dans `src/buffer.c`** et déclarée dans `src/buffer.h`. `multicursor.c` l'appelle pour appliquer les modifications comme une seule transaction annulable.

Les sources de l'archive 1.1.0 contiennent bien cette définition et une compilation entièrement neuve réussit. Le message ci-dessus a également été reproduit en conservant un **ancien `buffer.c.o` de 1.0.1** pendant une mise à jour vers 1.1.0. Les dates des sources extraites étaient antérieures aux objets existants : Make ne recompilait pas `buffer.c`, mais compilait le nouveau `multicursor.c`.

Ce cas explique comment `test_core` peut se lier avec l'ancien objet, alors que `test_multi` échoue. Des fichiers source mélangés peuvent aussi produire une erreur similaire ; le journal seul ne permet pas de distinguer tous les cas.

## Solution immédiate

Depuis un dossier contenant **toutes les sources de la même version**, utilisez un nouveau dossier de compilation. Il n'est pas nécessaire de supprimer vos scripts ni de désactiver les tests :

```bash
cmake -S . -B build-1.7.0 -DCMAKE_BUILD_TYPE=Release -DPRISME_OPENGL_VERSION=2.1
cmake --build build-1.7.0 --parallel
ctest --test-dir build-1.7.0 --output-on-failure
./build-1.7.0/prisme --split
```

Autre possibilité avec l’archive complète actuelle :

```bash
chmod +x lancer.sh
./lancer.sh --split
```

Si vous avez copié seulement certains fichiers de 1.1 par-dessus une version antérieure, extrayez l'archive complète dans un nouveau dossier.

**N'ajoutez pas une fonction vide pour satisfaire le linker.** Une telle fonction casserait l'édition multiple et l'historique.

## Protection ajoutée en 1.1.1

`cmake/CheckPrismeSources.cmake` :

1. refuse une archive incomplète ou un `buffer.c` qui ne définit pas `buf_apply_edits` ;
2. calcule une empreinte SHA-256 du contenu des sources/en-têtes à la configuration ;
3. ajoute cette empreinte aux commandes de compilation de tous les objets Prisme — noyau, interface et tests — pour invalider les anciens objets même si les dates des fichiers sont anciennes.

Les deux lanceurs utilisent désormais `build-1.7.0/`. `lancer.sh` relance toujours la configuration avant de construire. Raylib n'est pas recompilée uniquement à cause de ce changement d'empreinte.

## Vérifications effectuées

- Archive 1.1.0 originale extraite dans un dossier neuf : compilation complète et CTest **4/4** réussis.
- Ancien noyau 1.0.1 conservé, puis nouvelles sources extraites avec dates anciennes : erreur `buf_apply_edits` reproduite ; `test_core` se construit et `test_multi` échoue.
- Même cache défectueux, sans supprimer les objets, avec la construction 1.1.1 : `buffer.c.o` est bien recompilé et les **42 groupes C** passent.
- Bibliothèque corrigée : `nm` montre `T buf_apply_edits` dans `buffer.c.o`.
- Ancien fichier `buffer.c` mélangé aux nouvelles sources : refus dès la configuration avec diagnostic explicite.
- Version 1.1.1 : CTest Release et ASan/UBSan **4/4** réussis.

Sous Linux, pour vérifier le symbole :

```bash
nm -A build-1.7.0/libprisme_core.a | grep buf_apply_edits
```

Une ligne **`T buf_apply_edits` dans `buffer.c.o`** est attendue. Une ligne `U buf_apply_edits` dans `multicursor.c.o` est normale : ce fichier appelle la définition fournie par `buffer.c.o`.

Si la liaison échoue encore après une construction neuve, transmettez la sortie de la commande ci-dessus et de :

```bash
cmake --build build-1.7.0 --verbose
```
