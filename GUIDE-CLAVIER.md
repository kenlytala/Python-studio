# Clavier et annulation — Python Studio 1.18.0

## Raccourcis

Les lettres ci-dessous désignent **les lettres de votre disposition**, pas la position d’une touche sur un clavier américain.

| Raccourci | Action |
|---|---|
| Ctrl+Z | Annuler la dernière modification |
| Ctrl+Maj+Z ou Ctrl+Y | Rétablir |
| Ctrl+W | Fermer l’onglet, avec la protection existante des modifications non enregistrées |
| Ctrl+A | Tout sélectionner |

Les autres raccourcis alphabétiques suivent la même conversion. Les touches de fonction, flèches et modificateurs ne sont pas permutés. La touche M d’AZERTY est également reconnue à sa position réelle, par exemple dans Ctrl+Maj+M. Les histoires des champs restent indépendantes de celle du document ; une édition multicurseur ou une insertion XML garde sa transaction d’annulation atomique.

## Pourquoi Ctrl+Z fermait un onglet

GLFW, utilisé par raylib, livre des **positions physiques QWERTY**. Sur AZERTY, la touche marquée **Z** occupe la position `AD02`, dont le code est `KEY_W` (87). Le gestionnaire global y voyait donc Ctrl+W et demandait la fermeture avant d’atteindre l’annulation.

La version 1.16 traduit ces positions en lettres avant le dispatch, dans **les deux véritables éditions**. Elle traduit aussi en sens inverse les demandes aux API de touches natives pressées, répétées et maintenues : convertir uniquement la file aurait laissé un second chemin capable d’exécuter la mauvaise commande.

Ce n’est **pas** un simple échange Z/W. Un alphabet complet de 26 lettres, sans doublon, est accepté atomiquement à partir de 49 positions imprimables connues. Cela couvre également QWERTZ, les lettres déplacées sur de la ponctuation et les changements de disposition. Un relevé incomplet ou ambigu conserve la dernière carte valide au lieu de produire une carte partiellement changée.

## Lecture de la disposition

- **Linux/X11** : lecture de XKB via libX11. Les keycodes sont recherchés par noms physiques et alias, et non par une table evdev supposée. Le groupe courant est préféré ; s’il n’offre pas les 26 lettres latines, les autres groupes déjà chargés sont examinés. Aucun groupe ni disposition n’est activé ou modifié.
- **Windows** : fournisseur de lecture par `GetKeyboardLayout` / `MapVirtualKeyExW`, avec les dispositions déjà chargées seulement. Pas de `ToUnicode`, de consommation de touche morte, de chargement ou d’activation. La logique Python fait l’objet d’un double d’API CPU ; pas de validation Windows physique.
- **macOS** : branche GLFW pour l’édition C, non validée sur macOS. L’édition Python n’a pas de fournisseur macOS dans cette livraison.
- Si le fournisseur n’est pas disponible (`DISPLAY` absent, bibliothèque manquante, environnement non pris en charge…), la dernière carte valide est conservée ; avant tout relevé valide, le comportement initial reste QWERTY. Un environnement Wayland sans le fournisseur X11 accessible n’est donc pas une nouvelle compatibilité revendiquée.

Le relevé est rafraîchi au début d’un raccourci Ctrl/Super, à l’arrivée d’un événement pendant son maintien et périodiquement pendant un maintien prolongé. Les lettres de raccourcis ne sont pas persistées dans les préférences, et le système n’est jamais reconfiguré.

## La saisie de texte ne change pas

Les caractères proviennent toujours de la **file Unicode**, distincte des commandes. Pas de substitution Z/W dans les documents, le presse-papiers ou les chemins. AltGr reste une saisie de texte, pas Ctrl. Les mécanismes existants de paires automatiques et d’édition XML continuent de fonctionner normalement.

## Vérifications et limites

`tests/test_keyboard.py` exécute la vraie boucle d’événements Python avec API graphiques simulées. En C, `tests/test_keyboard_frame.h` passe par les vrais `update`, `editor_keys` et historique, extraits sans réécriture des corps de production. Les événements en file, pressés, répétés et combinés sont couverts. La demande d’action de fermeture C est enregistrée par le banc, pas exécutée par une fenêtre réelle.

Le modèle C, le lecteur XKB réel lié à des doubles Xlib, les structures ctypes et les projections C/Python sont également confrontés. Aucun serveur X, aperçu, capture, service web ou changement de disposition n’est utilisé. Ces essais ne sont pas une preuve d’exécution sur un bureau AZERTY physique, GNOME, Windows, macOS ou GPU.

Voir **[VALIDATION-1.18.0.md](VALIDATION-1.18.0.md)** pour les résultats exécutés et les régressions conservées.
