# Python Studio 1.21.0 — édition C

## Nouveau en1.21 : réduire la fenêtre depuis le menu

Un petit bouton **−** est placé à **l’extrême droite du menu supérieur**. Il réduit/minimise la fenêtre auprès du gestionnaire de fenêtres, sans quitter l’application, fermer d’onglet, arrêter l’exécution/SSH ou perdre le texte/historique. Il ne replie pas les panneaux. Dans Python/GTK, le bouton de réduction existant est déplacé à cet endroit, pas dupliqué ; dans C il est ajouté. Quitter reste immédiatement après Effets et conserve sa confirmation distincte. Le bouton de repli du menu C reste disponible à gauche du groupe de commandes.

Voir **VALIDATION-1.21.0.md**. Aucun aperçu/fenêtre/capture/service de validation généré. Paquets `.tar.xz` ; les archives1.20 sont conservées et les anciennes.tar.gz restent absentes.


## Nouveau en 1.20 — les trois éditions

- **Ctrl+Espace dans les dialogues fichier/dossier** : suggère le contenu du chemin saisi (ou du dossier courant si le champ est vide), y compris chemin relatif, `~/`, espaces et préfixe partiel. Modes dossier/import/XML/recherche limités aux dossiers. Lecture explicite dans un worker borné, pas de scan sur chaque frappe. Flèches pour choisir, **Tab/Entrée pour compléter**, puis Entrée pour ouvrir/valider ; aucune ouverture involontaire pendant la suggestion. Complétion annulée si cible/chemin/contexte/montage change ; insertion de champ annulable avec Ctrl+Z.
- **Coloration JavaScript et XML**, en plus de Python : extensions `.js/.mjs/.cjs` et `.xml/.xsl/.xslt/.svg`, cache incluant la langue même si seul le nom change. Commentaires/blocs/chaînes/nombres et templates `${…}`/regex JS ; balises/attributs/entités/commentaires/CDATA XML. Lexeurs tolérants aux saisies incomplètes, sans exécuter ou charger le code.
- **Odoo 19 Python et XML**, actif par défaut et configurable par projet dans **Paramètres → Odoo 19 — Python/XML**. Python utilise le vrai plugin OCA **pylint-odoo10.0.12**, qui exige PyLint4.0.x ; le paquet installe cette combinaison compatible. XML est analysé statiquement sans ORM/base/serveur ni résolution externe de ressources. Syntaxe toujours contrôlée, y compris mode Odoo désactivé ; arch fermé/vide accepté.
- **Règles Odoo ignorées par projet** et bouton − dans les diagnostics ; les exclusions PyLint génériques restent globales. Un interpréteur choisi sans plugin donne une erreur explicite : pas de faux rapport Odoo propre. Les configurations/plugins/hooks du projet ne sont pas activés arbitrairement.

Voir **GUIDE-ODOO19.md**, **GUIDE-CHEMINS.md** et **VALIDATION-1.21.0.md**. Les anciennes archives `.tar.gz` ont été supprimées à la demande ; les nouveaux livrables sont des `.tar.xz`, sans ZIP ni aperçu.


Anciennement **Prisme C**. Éditeur natif en **C11 + raylib**, distinct de l’édition Python : le transport SSH, les profils, les contrôleurs, dialogues, documents, sessions et interactions sont en C. Aucun lancement de l’éditeur Python n’est utilisé pour fournir ces fonctions.

Python reste nécessaire pour exécuter vos programmes et pour les services locaux d’analyse/complétion, pas pour implémenter l’interface de cet éditeur.

## Nouveau en 1.18 : ignorer une règle depuis son diagnostic

Dans la console d’analyse PyLint (**Problèmes** en Python / **Erreurs** en C), un petit bouton **−** à droite de chaque diagnostic permet d’**ignorer directement sa règle pour tous les projets**. Survol : code exact et portée globale. La liste des règles ignorées est enregistrée et l’analyse active relancée, même en mode manuel. Cliquez sur le message pour naviguer ; le bouton séparé ne déplace pas le curseur et ne modifie pas le code.

Réactivation dans **Paramètres → Règles…**. Pas de bouton pour les erreurs de syntaxe ou les rapports internes ; clics périmés, doublons et erreurs d’enregistrement protégés. Les règles passent toujours réellement à PyLint, avant la limite des diagnostics. Voir **GUIDE-PYLINT.md** et **VALIDATION-1.21.0.md**. Les deux véritables éditions sont conservées, sans aperçu.

## Configuration des règles PyLint (ajoutée en 1.17)

Dans **Paramètres → Règles…**, à côté de l’analyse automatique, saisissez les messages à ignorer : `C0114, C0115, C0116, W0611` ou leurs noms, par exemple `missing-function-docstring`. **Appliquer** enregistre la liste globale immédiatement et relance l’analyse active ; un champ vide réactive les messages. Annuler abandonne le brouillon.

Les règles sont transmises au **vrai PyLint**, avant la production du rapport : pas de filtrage cosmétique après troncature. Les erreurs de syntaxe restent toujours vérifiées. Une règle inconnue de l’interpréteur choisi produit un message explicite, pas une fausse analyse sans problèmes. Aucun message n’est ignoré par défaut. Configuration globale propre à chaque édition, **jamais restaurée depuis les sessions de projet**.

Les deux implémentations et PyLint **2.12.2, 2.17.7 et 4.1.3** sont vérifiés sans fenêtre. Voir **[GUIDE-PYLINT.md](GUIDE-PYLINT.md)** et **[VALIDATION-1.21.0.md](VALIDATION-1.21.0.md)**. Le correctif AZERTY/Ctrl+Z, la recherche, F11, les sessions et SSH fichiers seulement sont conservés ; aucun aperçu généré.

## Clavier : correctif 1.16 conservé

- **Correction dans les deux éditions** : sur AZERTY, **Ctrl+Z annule** au lieu de fermer l’onglet. Le moteur recevait la position QWERTY W de la touche marquée Z et exécutait à tort Ctrl+W.
- **Ctrl+Maj+Z / Ctrl+Y rétablissent** ; **Ctrl+W continue de fermer** l’onglet, avec les confirmations existantes. Ctrl+A et les autres raccourcis alphabétiques suivent également leurs lettres, y compris M déplacé sur une position de ponctuation.
- Conversion cohérente des événements en file **et** des touches natives pressées/répétées/maintenues ; changement de disposition relu pendant l’utilisation. Ce n’est pas un échange Z/W codé en dur. La saisie Unicode et AltGr garde son circuit séparé.
- Historique du document, champs, multicurseurs et transactions XML conservés. Vérifications CPU par les véritables routeurs de frame Python et C, sans aperçu, capture ni service. Voir **[GUIDE-CLAVIER.md](GUIDE-CLAVIER.md)** pour le fonctionnement et les limites des fournisseurs système ; pas de validation de bureau physique revendiquée.

## Recherche et F11 : fonctionnalités 1.15 conservées

- Une **barre horizontale interactive**, sous le contenu de la recherche, permet de voir la fin des longues lignes : cliquez dans la piste ou faites glisser le curseur. La barre verticale du contenu est également manipulable. Le texte reste **en lecture seule**, sans retour automatique à la ligne.
- La portée horizontale dépend de la plus longue ligne de **tout le fichier sélectionné**, y compris les lignes hors écran. Elle est mesurée une fois au chargement du fichier ; molette, Maj+molette, flèches et glissement s’arrêtent aux limites réelles. Pas de défilement sans fin vers une page vide.
- **Correction effective de F11 dans l’édition C** : le gestionnaire global interceptait encore la touche avant le dialogue en 1.14. La boucle principale donne maintenant priorité à la recherche : F11 agrandit/restaure **uniquement son dialogue**, sans modifier la fenêtre système de l’éditeur. Hors recherche, F11 conserve le plein écran de l’éditeur. La régression est vérifiée à travers les vraies fonctions de frame, de souris et de dispatch modal, et par réintroduction temporaire du défaut dans le banc de test uniquement.
- **F3/Maj+F3**, les résultats progressifs, la sélection stable par chemin, les surlignages, toutes les options et les protections des documents modifiés sont conservés. Changer de fichier, fermer, agrandir/restaurer ou modifier la géométrie du panneau annule une prise de souris périmée. Le dernier mouvement et le relâchement reçus dans une même frame sont appliqués.
- **Quitter reste en haut immédiatement à droite d’Effets**, avec confirmation des documents modifiés et sans doublon inférieur. Les deux éditions sont de véritables implémentations indépendantes. Aucun aperçu, capture ni service web généré.

**PCRE2-8 du système** est utilisé pour les options avancées, avec UTF-8/Unicode et budgets d’exécution/mémoire. Python : sur Debian/Ubuntu, `sudo apt install libpcre2-8-0` ; C : ajouter `libpcre2-dev` et `pkg-config` aux dépendances de compilation. La bibliothèque système n’est pas embarquée. Sans PCRE2, le mode littéral sensible à la casse Python reste utilisable ; un message explicite indique la dépendance lorsqu’une option l’exige. Notice : `third_party/PCRE2-NOTICE.txt`.

## Rechercher une chaîne dans un dossier (depuis 1.12)

**Ctrl+Maj+F**, ou **Ctrl+K → Rechercher dans un dossier** : choisissez un dossier éditable, mémorisé **par projet**, saisissez le texte et lancez la recherche récursive.

- **À gauche :** les fichiers contenant la chaîne, avec leur nombre d’occurrences.
- **À droite :** le contenu en lecture seule, toutes les occurrences visibles surlignées et l’occurrence active distincte.
- **F3 / Maj+F3** parcourent **tous les fichiers trouvés**, changent automatiquement le contenu affiché et bouclent en fin/début de liste.
- Par défaut, recherche littérale sensible à la casse, en arrière-plan ; **Arrêter**, **Échap**, défilement vertical/horizontal et ouverture facultative dans l’éditeur. Les documents, leurs modifications et historiques ne sont pas remplacés par la consultation.
- Dossier enregistré séparément des bases XML et imports ; chaîne et résultats non persistés. Les fichiers UTF-8 locaux ou SSH **déjà montés** sont lus sans exécution ni authentification automatique.

Limites et exclusions sont signalées ; les résultats sont des **instantanés du disque**, actualisés par une nouvelle recherche. Voir **[GUIDE-RECHERCHE.md](GUIDE-RECHERCHE.md)** pour l’utilisation et les limites détaillées.

## Contenu des dossiers avec Ctrl+Espace (depuis 1.11)

Ajoutez par exemple `/home/moi/odoo/addons` dans **Paramètres → Dossiers XML**. Dans votre document :

```text
sale/  → Ctrl+Espace → views/ → Entrée → sale/views/ → sale_views.xml
```

- Le menu propose les **sous-dossiers et tous les types de fichiers** du dossier, dossiers en premier.
- **↑ / ↓**, puis **Entrée** ou **Tabulation**, ou un clic pour choisir. Un dossier ajoute `/` et affiche immédiatement son contenu ; un fichier complète son nom.
- Vous pouvez taper `sale/vi` puis **Ctrl+Espace** pour filtrer. La frappe ordinaire ne lance aucune lecture de dossier.
- Après le nom XML complet, **Ctrl+Espace** ouvre toujours le choix des identifiants. L’insertion des champs et l’`arch` vide sont conservés.
- Chemins relatifs aux bases configurées, absolus, `~/…`, noms entre guillemets et dossiers SSH déjà connectés ; **Ctrl+Z / Ctrl+Y** annulent/rétablissent chaque insertion en une seule opération.

La liste est non récursive, fusionnée entre les bases, bornée à 32 propositions et lue en arrière-plan. Précisez le préfixe pour atteindre les autres noms. Aucun aperçu n’est généré.

## Dossiers XML, balises automatiques et Ctrl+Z / Ctrl+Y (depuis 1.10)

- **Paramètres → Dossiers XML**, ou **Ctrl+K → Dossiers XML** : ajoutez jusqu’à huit bases de chemins **par projet**, puis réordonnez-les si nécessaire. Elles sont enregistrées séparément des dossiers d’import Python.
- Dans le document, un chemin relatif `.xml` suivi de **Ctrl+Espace** cherche d’abord dans ces dossiers, puis dans le projet, le dossier du document et les imports. Choisissez l’identifiant pour remplacer le chemin par son élément et ses champs ; les champs `arch` restent **vides et correctement fermés**.
- Dans un document **`.xml`**, `>` crée la fermeture, `</` ferme le parent encore ouvert et **Entrée** indente entre les balises. **Ctrl+Espace** propose des balises du document et les balises XML/Odoo courantes. Les attributs entre guillemets, commentaires, CDATA et instructions ne déclenchent pas de génération.
- **Ctrl+Z annule ; Ctrl+Y ou Ctrl+Maj+Z rétablit**, y compris une génération XML, une insertion d’identifiant, un collage ou une édition multicurseur en une seule opération.
- L’option **Balises XML automatiques** se désactive dans les paramètres. Les macros globales, dont `xml_form`, et la complétion Python/Odoo sont conservées.

Voir **[GUIDE-XML.md](GUIDE-XML.md)** pour les exemples, priorités, limites et détails SSH. Pas de recherche globale d’identifiants : les dossiers servent de bases aux chemins que vous saisissez. Aucune source XML n’est modifiée par la lecture ; les menus contextuels de l’explorateur restent alignés.

## Sessions et brouillons conservés (depuis la 1.8)

Python Studio retrouve les documents ouverts, leur ordre et l’onglet actif, les curseurs/sélections/multicurseurs, les défilements, la vue fractionnée et la disposition des panneaux. **Les modifications non enregistrées et les documents sans nom sont conservés localement.**

L’état modifié est vérifié environ toutes les deux secondes et capturé lors de la fermeture normale. La confirmation ajoute **Conserver les brouillons et quitter**. Si un original a disparu ou changé, le texte revient dans un document récupéré à enregistrer séparément : la restauration n’écrase aucun fichier du projet.

Les documents SSH reviennent **hors ligne**. Une reconnexion explicite, puis la comparaison avec les fichiers distants, est nécessaire avant l’enregistrement sur place. Aucun mot de passe, phrase secrète, jeton askpass ou droit de propriété sur un montage n’est restauré.

**Confidentialité : le texte des documents est conservé en clair dans des fichiers privés locaux, non chiffrés.** `--no-session` désactive les sessions pour ce lancement sans effacer les journaux existants. Voir **[GUIDE-SESSIONS.md](GUIDE-SESSIONS.md)** pour les détails, limites et exclusions.

## Lancer le précompilé Linux x86_64

```bash
cd python-studio-c
./bin/python-studio
./bin/python-studio --project "/chemin/mon-projet" --python "/chemin/venv/bin/python"
./bin/python-studio --no-session --project "/chemin/mon-projet"
./bin/python-studio --version
```

Gardez **`bin/python-studio-ssh-helper` à côté de `bin/python-studio`**, et conservez les dossiers `assets/` et `examples/`. Le dossier `bin/` contient aussi la démonstration C indépendante `constellation`.

Un bureau graphique, un pilote OpenGL et la bibliothèque dynamique **libxml2 (`libxml2.so.2`)** sont nécessaires. Le précompilé est destiné à **Linux x86_64 avec glibc 2.38 ou ultérieure** ; sur une distribution plus ancienne ou une autre architecture, recompilez localement. Le helper a ses propres exigences ABI, détaillées dans le rapport de validation. Cela ne modifie pas le backend de l’édition Python, qui conserve sa cible glibc 2.17.

Sans projet/fichier explicite, le dernier projet local est repris. `--project` sélectionne sa propre session. Un fichier passé à la ligne de commande est ouvert après la restauration.

## Compiler localement

CMake **3.24+**, un compilateur C11, Make (ou Ninja), **libxml2 2.9+**, Python 3 et les bibliothèques de développement X11/OpenGL sont nécessaires sous Linux. Exemple Debian/Ubuntu :

```bash
sudo apt install build-essential cmake pkg-config libx11-dev libxrandr-dev \
  libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev libxml2-dev
./lancer.sh
```

Le lanceur maintient **un seul dossier de production courant, `build-1.21.0`**. Première configuration : téléchargement des sources raylib **5.5 épinglées par SHA256**, puis application des trois corrections graphiques existantes. Une connexion est donc nécessaire pour ce téléchargement initial.

```bash
cmake -S . -B build-1.21.0 -DCMAKE_BUILD_TYPE=Release -DPRISME_OPENGL_VERSION=2.1
cmake --build build-1.21.0 --parallel
./build-1.21.0/python-studio
```

Les noms d’options CMake `PRISME_*` restent compatibles. Le lanceur source Windows s’appelle `LANCER_WINDOWS.bat` ; **Windows/macOS et leurs bureaux ne sont pas validés dans cette livraison**. SSHFS intégré reste une fonction Linux. Aucun binaire Windows/macOS n’est livré.

## Fonctions conservées

- Multicurseurs simultanés (jusqu’à 256), occurrences et annulation atomique ; Ctrl+Alt+↑/↓ et **Ctrl+Maj+K / J** en secours lorsque GNOME intercepte les premières combinaisons. L’éditeur ne modifie pas les réglages système.
- Deux vues côte à côte, onglets, recherche/remplacement et navigation dans les diagnostics.
- Fenêtre sans barre de titre système, F11 plein écran et Quitter en haut à droite d’Effets ; explorateur, menu supérieur et console repliables, mode concentration F9. Pas de bande de raccourcis à l’extrême gauche ni de carte promotionnelle dans l’explorateur.
- **Exécuter à côté des trois points de la rangée d’onglets**, et aucun espace ajouté sous la barre d’état.
- Explorateur avec bouton **Ouvrir** et menu contextuel créer/copier/coller/supprimer ; opérations asynchrones, protections des fichiers ouverts/modifiés et publication contrôlée des copies.
- F5 exécute le Python externe choisi par `--python` ou `PRISME_PYTHON` ; interpréteur local, sortie asynchrone et arrêt du programme.
- PyLint externe réel, vérification syntaxique et F4 ; huit dossiers d’import supplémentaires par projet. Les échecs de lancement et le repli sans PyLint sont explicitement signalés.
- Complétion **statique Odoo 18/19**, sans importer le framework ni lancer une base de données ; macros globales multilignes modifiables, dont `xml_form`.
- Sauvegardes protégées, UTF-8/BOM/CRLF, CLI, modes de compatibilité, OpenGL 2.1 par défaut et MSAA uniquement sur demande.
- SSHFS/OpenSSH, profils multiples, clé/agent ou mot de passe, secrets masqués, vérification des clés serveur, reconnexion et garde-fous hors ligne.

Les raccourcis et détails historiques restent dans [HISTORIQUE.md](HISTORIQUE.md), dont les anciens noms de commandes ne sont plus ceux de cette livraison. L’aide courante est `./bin/python-studio --help`.

## Python, PyLint et Odoo

Les services sont **locaux** et n’exécutent pas les modules analysés pour fournir la complétion statique. Installez PyLint et les dépendances de votre programme dans le Python sélectionné :

```bash
python3 -m venv /chemin/venv
/chemin/venv/bin/python -m pip install pylint
./bin/python-studio --python /chemin/venv/bin/python --project /chemin/projet
```

Les dossiers de sources additionnels se configurent dans l’application ou par `--import-dir DOSSIER` (répétable, maximum huit). Les réglages par projet et macros globales conservent leur format existant.

[GUIDE-ODOO-MACROS.md](GUIDE-ODOO-MACROS.md) détaille la complétion, les imports et les macros. F5 lance volontairement votre programme avec vos permissions ; ce n’est pas une sandbox. Aucun programme du projet n’est exécuté au démarrage normal ; `--demo` est une demande explicite d’exécution.

## SSH et compatibilité des paramètres

SSH intégré nécessite `sshfs` et `openssh-client` sous Linux, avec prise en charge de `contain_symlinks`. Les 32 profils possibles mémorisent hôte, utilisateur, port, dossier distant et chemin de clé, **jamais les secrets**. Ils restent partagés avec Python via `~/.config/prisme/ssh-profiles.json` ou `$PRISME_CONFIG_DIR/ssh-profiles.json`.

Les sessions C se trouvent dans `~/.config/prisme-c/sessions-c/` ou `$PRISME_CONFIG_DIR/sessions-c/`, séparément des sessions Python. Les noms des dossiers historiques et les variables `PRISME_*` sont conservés afin de préserver les données existantes.

**Les fichiers sont distants, mais F5, Python, PyLint et les dépendances restent locaux.** Pas d’installation d’agent Python Studio sur le serveur, pas d’exécution Python distante. Les configurations OpenSSH/ProxyCommand restent des configurations locales de confiance. L’application ne démonte pas le montage d’une autre instance et n’effectue pas de démontage forcé pour quitter.

Voir **[GUIDE-SSH.md](GUIDE-SSH.md)** et [GUIDE-SESSIONS.md](GUIDE-SESSIONS.md) pour les coupures, conflits, liens et limites.

## Validation et provenance

**[VALIDATION-1.21.0.md](VALIDATION-1.21.0.md)** présente les essais réellement exécutés et leurs exclusions. `MANIFESTE-1.21.0.json` inventorie les fichiers distribués et empreintes. Les anciens rapports et manifestes concernent uniquement leurs versions historiques.

```bash
ctest --test-dir build-1.21.0 --output-on-failure
```

Les essais SSH réels sont optionnels et nécessitent les outils de QA documentés. Les tests sans fenêtre, dont la géométrie plein écran, ne prouvent pas le fonctionnement physique sur votre bureau/GPU. Aucun aperçu, capture ni serveur web n’a été lancé pour cette livraison.

La dépendance libxml2 est documentée dans `third_party/LIBXML2-NOTICE.txt` ; elle n’est pas embarquée. Les trois correctifs raylib restent documentés dans `third_party/RAYLIB-PATCH.md`, `CORRECTIF-GLX.md` et `CORRECTIF-LIAISON.md`. L’application, raylib et les polices conservent leurs licences/notices respectives (`LICENSE`, `third_party/`, `assets/fonts/`).
