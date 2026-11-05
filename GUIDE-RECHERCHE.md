# Recherche dans un dossier — Python Studio 1.18.0

Cette fonction est disponible dans les **deux éditions indépendantes, Python et C**. Elle ne remplace ni la recherche du document, ni les bases XML, ni les dossiers d’import Python/PyLint.

## Ouvrir et définir le dossier

1. Appuyez sur **Ctrl+Maj+F**, ou ouvrez les commandes avec **Ctrl+K** et choisissez **Rechercher dans un dossier**.
2. Modifiez le champ **Dossier**, ou utilisez son bouton de choix de dossier. Par défaut, il contient le projet courant. Un chemin relatif est interprété depuis ce projet ; `~/…` et les chemins absolus sont acceptés.
3. Saisissez la chaîne dans **Texte** et cliquez sur **Rechercher**, ou appuyez sur **Entrée** dans un champ. **Ctrl+Entrée** relance depuis n’importe quelle partie de cette fenêtre.

Le dernier dossier lancé ou explicitement choisi est **mémorisé par projet**, dans les préférences locales. Une erreur de mémorisation est affichée : la recherche peut néanmoins fonctionner. Un chemin inexistant ou momentanément inaccessible reste corrigeable dans le champ ; aucun montage ou authentification SSH n’est démarré par cette action.

La frappe seule ne lance pas de parcours. Par défaut, la recherche est **récursive, littérale, sensible à la casse et non chevauchante** : `a.b` cherche ces trois caractères, pas une expression régulière. Le texte doit tenir sur une ligne ; les tabulations sont acceptées.

## Résultats pendant la recherche

Les fichiers validés **s’affichent au fur et à mesure du scan** : la liste gauche et le contenu surligné de droite sont utilisables avant la fin. Le premier fichier apparaît sans voler le focus du champ. Cliquez sur un résultat, ou utilisez **F3/Maj+F3** pour parcourir tous les fichiers déjà trouvés.

Le tri par chemin relatif UTF-8 est maintenu. Un nouveau nom peut s’insérer avant le fichier consulté : **le fichier sélectionné, son occurrence, le focus, le contenu et les défilements restent conservés**. La fin du scan ne ramène pas au premier fichier. Le compteur et l’ordre global sont provisoires tant que la recherche continue ; F3 boucle sur les fichiers déjà disponibles et intègre les suivants à leur publication.

Seuls des fichiers entièrement lus, validés et contenant au moins un match sont publiés. Pas de relecture de contenu ni d’attente d’I/O pour afficher un nouveau lot ; les instantanés restent soumis aux limites plus bas. Une seule vue courante, pas de file de messages qui grossit avec les frames. **Arrêter** et les erreurs terminales conservent les fichiers déjà validés, signalés comme partiels.

## Lire les longues lignes : barres de défilement

Sous le contenu à droite, la **barre horizontale** permet de voir toute une longue ligne sans la couper ni l’enrouler :

- **Cliquez dans la piste** pour amener le curseur à cet endroit, ou **faites glisser le curseur**. Il garde le point de prise et suit la souris même hors de la piste, jusqu’au relâchement.
- La **barre verticale** à droite du contenu fonctionne de la même façon pour rejoindre les autres lignes. La liste des fichiers à gauche garde son propre défilement à la molette.
- **Maj+molette** et les **flèches gauche/droite dans le contenu** défilent horizontalement ; molette, haut/bas, Page préc./suiv., Début/Fin défilent verticalement. Tous s’arrêtent aux limites réelles. Une piste complètement remplie indique que le contenu tient déjà dans cette direction.
- La largeur est calculée sur la **plus longue ligne de tout le fichier sélectionné**, même si elle n’est pas encore visible. Elle n’est pas recalculée à chaque frame. Colonnes Unicode scalaires et tabulations de quatre, pas une mesure `wcwidth`.
- Le texte et les surlignages sont découpés au-dessus des barres : ils ne passent pas sous les curseurs. Le contenu reste un **instantané en lecture seule** ; le défilement ne change pas le fichier ni les onglets modifiés.
- **F3/Maj+F3** continuent à révéler l’occurrence et à changer automatiquement de fichier. Changement de fichier, F11, fermeture, arrêt ou modification de la géométrie annulent une prise périmée. F3 et Échap restent utilisables pendant un glissement. Un dernier mouvement reçu avec le relâchement est appliqué.

## Agrandir uniquement la recherche

Le bouton **Agrandir · F11**, ou **F11 lorsque la recherche est ouverte**, étend le dialogue de recherche à toute la surface disponible **dans la fenêtre de l’éditeur**. **Restaurer · F11**, ou F11 de nouveau, revient au panneau initial, sans relancer le scan ni perdre les résultats.

**Aucun changement de la fenêtre système de l’éditeur** : ni bascule plein écran, ni maximisation/restauration système, ni modification de ses dimensions ou de sa position. Son état plein écran ou maximisé préexistant est préservé. Fermer, choisir un dossier ou changer de projet n’a donc aucun mode système à restaurer.

**Correctif C 1.15.** En 1.14, le contrôleur de recherche gérait déjà F11, mais la boucle principale interceptait la touche avant lui et exécutait encore le plein écran global. La priorité est désormais corrigée à cette entrée globale ; les essais passent par les véritables fonctions de frame, souris et dispatch modal. Les seuls tests directs de l’ancien contrôleur ne démontraient pas le bon routage.

**En dehors de la recherche**, F11 garde l’action plein écran habituelle de l’éditeur. Les tests de fenêtre utilisent des API simulées ; aucune validation de bureau physique/GPU n’est revendiquée.

## Options combinables

Les quatre boutons indiquent **[x]** si l’option est active et **[ ]** sinon. Les options ne lancent pas de recherche à la frappe : relancez explicitement avec Rechercher/Entrée.

- **Casse** : cochée par défaut, respecte majuscules/minuscules ; décochée, recherche Unicode insensible à la casse selon PCRE2. Ce n’est pas une suppression des accents (`e` et `é` restent distincts), ni une normalisation Unicode.
- **Mots entiers** : exige une frontière avant et après le match. Lettres, nombres, marques combinantes Unicode et `_` appartiennent au mot. Chercher `cat` ne retient donc ni `catfish`, ni `_cat`, ni `cat2`.
- **Regex** : interprète la requête comme une expression **PCRE2 UTF-8/UCP**, pas comme du code. Exemples : `partner_id|order_line`, `name="[^"]+"`, `(?<=id=)\d+`, ou `^class\s+\w+`. Les ancres `^`/`$` suivent les règles multilignes PCRE2 ; `\A` et `\z` désignent le début et la fin du fichier. Les séquences `\n` peuvent traverser les lignes, et `(?s)` autorise `.` à inclure les sauts de ligne. Les groupes, alternatives et lookarounds suivent la version de PCRE2 installée ; ce n’est pas une promesse de compatibilité intégrale avec Python `re`.
- **Sous-dossiers** : cochée par défaut ; décochée, seuls les fichiers directement dans le dossier choisi sont examinés.

La requête reste un champ sur une seule ligne, de 1 024 octets UTF-8. Les expressions multilignes utilisent des échappements. Les matchs restent non chevauchants ; après un match vide, la recherche avance d’un caractère Unicode (un match vide à la fin est émis au plus une fois). Un match vide a un marqueur visible d’une cellule et participe à F3. Un match multiligne surligne chaque ligne couverte ; F3 amène le début du match dans le volet droit.

### Filtres d’inclusion et d’exclusion

Ouvrez **Filtres +** pour afficher quatre champs : **Fichiers inclus**, **Fichiers exclus**, **Dossiers inclus**, **Dossiers exclus**. Refermer le panneau ne désactive pas les règles saisies.

| Champ | Exemple | Effet |
|---|---|---|
| Fichiers inclus | `*.py;*.xml` | Ne lit le contenu que des noms ayant une de ces extensions |
| Fichiers exclus | `*.min.js;generated/**` | Retire ces fichiers, même s’ils sont inclus ailleurs |
| Dossiers inclus | `src;addons` | Retient les fichiers dans ces sous-arbres ; les parents sont parcourus pour pouvoir les trouver |
| Dossiers exclus | `tests;build*` | Écarte ces dossiers et tous leurs descendants avant leur lecture |

Règles communes :

- Motifs **séparés par `;`** ; espaces autour des motifs ignorés, champs vides = tout inclure/rien exclure. **512 octets/champ, 16 motifs/champ, 256 octets/motif**.
- Sans `/`, le motif porte sur le **nom de base**, à toute profondeur. Avec `/`, il porte sur le **chemin relatif au dossier recherché**. Les filtres restent sensibles à la casse, indépendamment de l’option Casse du contenu.
- `*` = zéro ou plusieurs caractères sauf `/`, `?` = un caractère Unicode sauf `/`, `**` = zéro ou plusieurs caractères y compris `/`. `**/` peut représenter zéro dossier : `**/*.py` inclut aussi les fichiers Python à la racine.
- `src/*` ne retient que les entrées immédiates ; `src/**` retient aussi leur profondeur. Pour les dossiers, un motif inclus sélectionne tout le sous-arbre ; `.` inclut la racine et ses descendants (pour la racine seule, désactivez Sous-dossiers).
- Ce ne sont pas des expressions régulières : `[]` est littéral. Les chemins absolus, `..` et les contrôles sont refusés. Les `\` des motifs sont interprétés comme des séparateurs de chemins `/`.
- Les exclusions l’emportent ; les liens enfants, fichiers spéciaux/binaires/non UTF-8 et dossiers VCS/cache listés plus bas restent ignorés. Ces filtres ne lèvent pas les limites globales ; découvrir un sous-arbre inclus peut consommer le budget d’entrées.

### PCRE2 et budgets

Les deux éditions utilisent les mêmes règles **PCRE2-8 système** pour les modes avancés/globs (minimum 10.34 pour compiler le C). La bibliothèque n’est pas embarquée : Linux Python, par exemple **`sudo apt install libpcre2-8-0`** ; C, **`libpcre2-dev` + `pkg-config`** en plus de ses dépendances graphiques/libxml2. Sous macOS : PCRE2 installé par le gestionnaire de paquets ; sous Windows : PCRE2-8 disponible dans le chemin de bibliothèques. Ces deux bureaux ne sont pas testés. Le mode littéral sensible à la casse Python fonctionne sans PCRE2 ; les options qui l’exigent affichent une erreur explicite si elle manque. Voir `third_party/PCRE2-NOTICE.txt`.

Aucun JIT ni callback de code utilisateur. Les motifs sont compilés dans le worker, pas dans la boucle graphique. Limites : **200 000 unités de travail par appel PCRE2, profondeur 512, tas de match 8 Mio et budget de 2 secondes par fichier** pour l’expression, avec callouts d’annulation. Le temps n’est pas une garantie temps réel du noyau. Les globs ont aussi ces gardes lors de leur évaluation, et tous les autres budgets de scan restent actifs. Les syntaxes invalides ou limites atteintes produisent une **erreur explicite**, pas un résultat « aucun fichier » trompeur. `\C` est interdit pour ne pas découper les octets d’un caractère UTF-8 ; les offsets et F3 restent valides.

## Les deux parties de la fenêtre

- **À gauche :** les fichiers contenant la chaîne, classés par chemin relatif UTF-8. Chaque ligne affiche le nom, le chemin relatif et le nombre d’occurrences. Cliquez sur un fichier pour afficher sa première occurrence, sans ouvrir d’onglet.
- **À droite :** le contenu intégral du fichier trouvé, sous forme d’un **instantané en lecture seule**. Les occurrences visibles sont surlignées ; l’occurrence active se distingue par une couleur et un soulignement. La position globale, le fichier courant et la ligne sont indiqués en bas.

Le texte provient **du disque**, pas des modifications non enregistrées dans les onglets. La fenêtre n’édite, n’enregistre et n’exécute aucun fichier recherché ; les curseurs, sélections, multicurseurs et historiques des documents restent inchangés.

### Passer d’une occurrence à l’autre

| Action | Raccourci |
|---|---|
| Occurrence suivante, dans **tous** les résultats | **F3** |
| Occurrence précédente, dans **tous** les résultats | **Maj+F3** |
| Agrandir/restaurer uniquement la recherche | **F11** |
| Activer/désactiver une option ayant le focus | **Espace** |
| Relancer explicitement la recherche | **Ctrl+Entrée** |
| Parcourir champs, options, fichiers et contenu | **Tab / Maj+Tab** |
| Choisir le fichier précédent/suivant lorsque la liste a le focus | **↑ / ↓** |
| Défiler dans le volet sous la souris | **Molette** |
| Défiler horizontalement dans le contenu | **Maj+molette** |
| Défiler dans le contenu lorsqu’il a le focus | **Flèches / Page haut / Page bas** |
| Fermer et annuler la demande en cours | **Échap** |

**F3 traverse automatiquement les fichiers.** Après la dernière occurrence d’un fichier, il sélectionne la première du fichier suivant et actualise le volet droit. Après le dernier fichier, il revient au premier ; Maj+F3 effectue le parcours inverse. La liste gauche et le contenu défilent pour rendre l’occurrence active visible.

La navigation utilise les instantanés en mémoire : elle ne relit pas les fichiers. Les arrivées du scan en cours sont intégrées automatiquement. Pour prendre en compte une modification externe après le scan, relancez la recherche. Les caches normaux du système de fichiers, notamment SSHFS, restent applicables.

**Ouvrir dans l’éditeur** est une action facultative, distincte de la sélection d’un résultat. Elle utilise l’ouverture habituelle du fichier actuel ; si un onglet de ce fichier contient déjà des modifications, il est sélectionné sans écraser son contenu. Une reconnexion SSH reste explicite si le fichier n’est plus accessible.

## Annulation, résultats partiels et limites

L’exploration et les lectures ont lieu dans un worker. **Arrêter** conserve les résultats déjà trouvés, signalés comme partiels. Une erreur terminale conserve aussi les fichiers déjà validés et affiche son motif ; elle ne transforme pas l’erreur en « aucun fichier ». **Échap/Fermer** annule la demande sans attendre une lecture en cours ; ses réponses tardives ne repeuplent pas la fenêtre. Une seule demande reste en vol jusqu’à la fin de son annulation. Si nécessaire, attendez sa fin avant de relancer.

Modifier les paramètres pendant une recherche annule la demande précédente. Modifier les champs après une recherche terminée laisse ses instantanés visibles avec un avertissement : cliquez sur Rechercher pour les actualiser. Changer de projet abandonne les résultats du projet précédent. Seul le dossier est enregistré ; **ni les options/filtres, ni la chaîne recherchée, ni les résultats ne sont persistés** par cette fonction. Fermer puis rouvrir la fenêtre dans le même projet peut retrouver les derniers instantanés en RAM ; quitter l’application les supprime.

Limites d’une demande, exprimées en octets UTF-8 lorsque pertinent :

- texte recherché : **1 024 octets** ; chemin : **4 095 octets** ;
- fichier : **2 Mio**, avec au plus **65 536 lignes** ; total lu : **64 Mio** ;
- instantanés retenus : **16 Mio**, **256 fichiers**, **16 384 occurrences** ;
- **20 000 entrées** et profondeur **64** ; noms en attente bornés à **4 Mio**.

Les fichiers trop grands, illisibles, binaires/avec contrôles, non UTF-8 ou dont le changement est détecté pendant leur lecture sont ignorés ; un compteur l’indique. Les fins de ligne CRLF/CR sont normalisées en LF dans l’affichage, sans réécriture du fichier source. Les liens enfants, FIFO et fichiers spéciaux ne sont pas parcourus. Les dossiers `.git`, `.hg`, `.svn`, `.venv`, `venv`, `__pycache__`, `node_modules`, `.cache` et `.pytest_cache` sont exclus. Une limite globale atteinte est signalée comme résultat partiel ; tous les fichiers du disque ne sont donc pas nécessairement examinés.

Les positions visuelles utilisent des tabulations de quatre cellules et une cellule par caractère Unicode scalaire ; ce n’est pas une implémentation de la largeur terminale `wcwidth`. Les offsets internes sont des caractères Python ou des octets UTF-8 en C, comparés après conversion dans les tests de parité.

## Quitter l’éditeur

Le bouton **Quitter** est en haut, immédiatement à droite d’**Effets**, avec les confirmations de sauvegarde habituelles. Il n’est plus dupliqué dans la barre d’état. Si le menu supérieur est replié, **F8** le réaffiche ; **Ctrl+K → Quitter** et **Alt+F4** restent disponibles.

## SSH et confidentialité

On peut rechercher dans un dossier **déjà monté** par la connexion SSH de l’éditeur, y compris depuis un projet local. Les gardes refusent les emplacements SSH hors ligne et les alias qui y mènent : pas de recherche dans un répertoire de remplacement local. Un changement de génération/état du montage invalide la demande. Aucun mot de passe ou passphrase n’est copié dans le worker ou les préférences de recherche.

Les préférences `folder-search-….json` (Python) et `folder-search-….cfg` (C) sont privées, écrites atomiquement dans le répertoire local de configuration de l’édition. Elles contiennent un chemin en clair, pas un secret chiffré, et restent séparées des profils XML/imports/SSH.

Ces gardes ne constituent pas une sandbox contre un système de fichiers hostile. Une entrée/sortie bloquée dans le noyau peut retarder l’arrêt du worker et la fermeture de l’application. Les vérifications ont lieu sans bureau physique, sans aperçu ni capture ; elles ne certifient pas la disponibilité des raccourcis sur un gestionnaire de fenêtres particulier.

Voir **[VALIDATION-1.18.0.md](VALIDATION-1.18.0.md)** pour les preuves et exclusions de cette livraison.
