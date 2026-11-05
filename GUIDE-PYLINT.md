# Messages PyLint à ignorer — Python Studio 1.18.0

## Ignorer directement depuis un diagnostic

Dans la console d’analyse, onglet **Problèmes** (édition Python) ou **Erreurs** (édition C), chaque message PyLint propose un petit bouton **−** à droite.

- Survolez-le pour voir **le code exact de la règle** et la portée **tous les projets**.
- Cliquez une fois : ce code est ajouté à la configuration globale enregistrée, et l’analyse active redémarre, même si l’analyse automatique est désactivée. Le même type de message sera ignoré dans les autres fichiers/projets de cette édition.
- Le bouton est séparé du message : **cliquer le message navigue**, cliquer **−** ne déplace pas le curseur et ne modifie ni le code, ni ses sélections/multicurseurs, ni son historique.
- Pour réactiver la règle, ouvrez **Paramètres → Règles…**, retirez son code ou son nom puis appliquez. Vider la liste réactive toutes les exclusions de ce réglage.
- Un clic sur un ancien résultat est refusé si le rapport/document/contexte a changé ; il ne peut pas sélectionner une autre règle à cause d’un réordonnancement des lignes. Une règle déjà ignorée par son nom n’est pas ajoutée une deuxième fois par son code.
- Si l’enregistrement échoue ou si une limite est atteinte, une notification l’indique et la configuration active précédente reste en place.

Les **erreurs de syntaxe** restent non désactivables : pas de bouton sur `E0001`/`syntax-error`, ni dans les rapports syntaxe, en attente ou d’erreur interne. La vérification de syntaxe avant PyLint demeure indépendante de la liste des règles.

## Choisir les règles

1. Ouvrez **Paramètres** avec l’icône d’engrenage.
2. À côté de l’analyse automatique, cliquez sur **Règles…**.
3. Saisissez les identifiants ou les noms des messages, puis cliquez sur **Appliquer** ou appuyez sur **Entrée**.

Exemples :

```text
C0114, C0115, C0116, W0611
```

ou :

```text
missing-module-docstring, missing-class-docstring, missing-function-docstring, unused-import
```

Les codes figurent dans le panneau **Problèmes** avec les diagnostics. Vous pouvez combiner codes et noms. Virgules, espaces, points-virgules et sauts de ligne collés servent de séparateurs. Les codes sont mis en majuscules, les noms en minuscules et les doublons textuels sont retirés. Les catégories globales et `all` ne sont pas pris en charge : indiquez les messages précis.

**Aucune règle n’est ignorée par défaut.** L’exemple affiché dans le champ est seulement une aide, jamais une exclusion activée automatiquement.

## Appliquer, annuler ou réactiver

- **Appliquer / Entrée** : sauvegarde immédiate, abandon des anciens résultats et demande d’une nouvelle analyse du fichier Python actif, même si l’analyse automatique est désactivée. Les autres onglets perdent leur cache et seront analysés selon le mode automatique ou avec **F4**.
- **Annuler / Échap / Fermer** : retour aux paramètres sans appliquer le brouillon.
- **Tout réactiver** : vide seulement le brouillon. Cliquez ensuite sur **Appliquer** pour réactiver les messages. Une liste vide enlève toutes les exclusions ajoutées dans ce réglage.
- En cas d’échec d’enregistrement, le dialogue reste ouvert et la liste active précédente reste en place. Un collage dépassant la limite est refusé sans perdre le brouillon ou son historique.

Le champ prend en charge sélection, copier/coller, déplacement du curseur et **Ctrl+Z / Ctrl+Y**, y compris le routage AZERTY corrigé en 1.16. Les documents, modifications non enregistrées, sélections et multicurseurs du code ne sont pas modifiés.

## Portée et stockage

La liste est **globale pour l’utilisateur, dans chaque édition indépendamment**. Elle reste en vigueur après un changement de projet, une reprise de session ou un redémarrage. Elle n’est pas copiée dans les journaux de disposition par projet et ceux-ci ne peuvent pas restaurer une ancienne liste.

- Édition Python : clé `lint_disabled` des préférences `settings.json`.
- Édition C : profil `pylint-rules.cfg`, à côté de son fichier de préférences.

Ces fichiers restent dans les préférences de l’application, **pas dans les projets**. Aucun fichier `.pylintrc`, `pyproject.toml` ou code du projet n’est écrit. Chaque édition conserve son propre emplacement de configuration, comme pour ses autres préférences.

## Ce qui reste contrôlé

Les règles sont réellement transmises à **PyLint avec `--disable`**, avant la production et la limite de 256 diagnostics du protocole. Ce n’est pas un simple masquage dans le panneau : les compteurs, marqueurs et résultats viennent de la même analyse configurée.

La **syntaxe Python reste toujours vérifiée** par compilation sans exécution, avant PyLint. Ignorer `E0001` ou `syntax-error` ne masque donc pas une erreur de syntaxe. Les autres véritables erreurs continuent d’être signalées tant que vous ne les avez pas explicitement exclues.

Les noms et codes disponibles peuvent varier selon la version de PyLint. Le processus vérifie la liste dans **le catalogue de l’interpréteur sélectionné pour l’analyse**. Un identifiant inconnu, supprimé ou indisponible est signalé comme un problème de configuration, jamais présenté comme une analyse réussie sans diagnostics. Corrigez alors la liste ou videz-la, puis appliquez.

Si PyLint n’est pas installé dans cet interpréteur, le message existant l’indique et la vérification de syntaxe demeure disponible. F5 et l’interpréteur d’exécution ne sont pas changés par ce réglage.

## Limites et sécurité

- **64 règles distinctes**, **4 095 octets** de saisie, **95 caractères ASCII par nom**.
- IDs à cinq caractères (`W0611`, `C0114`…), ou noms de messages avec lettres/chiffres/tirets.
- Pas de paramètres arbitraires, plugins, hooks, commandes ou catégories globales dans le champ.
- Comme auparavant, l’analyse isolée n’active pas les configurations/plugins/init-hook du projet ou de l’utilisateur de PyLint. Le projet reste lu statiquement et son code n’est pas lancé par ce réglage.
- Les secrets SSH ne sont ni consultés ni enregistrés dans cette configuration. Les fichiers distants restent des fichiers ; Python/PyLint s’exécutent localement.

## Vérifications

Vraies boucles de frame, dialogues/champs, contrôleurs, persistance et processus PyLint testés sans fenêtre. Compatibilité réelle vérifiée avec **PyLint 4.1.3 / Python 3.12**, **2.17.7 / Python 3.11** et **2.12.2 / Python 3.10**. Ces dépendances de QA ne sont pas ajoutées à l’application. Les API graphiques sont simulées ; aucune preuve de bureau physique, GPU, Windows ou macOS n’est revendiquée. Aucun aperçu, capture ou serveur graphique/web généré.

Résultats et reproduction : **[VALIDATION-1.18.0.md](VALIDATION-1.18.0.md)**.
