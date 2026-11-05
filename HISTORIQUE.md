# Historique de Python Studio

## 1.21.0 — 4 novembre2026

- Bouton − de réduction/minimisation à l’extrême droite du menu supérieur dans les trois éditions. Python/GTK : déplacement de l’unique bouton existant ; C : action native MinimizeWindow ajoutée, contrôleur réel commun au banc CPU.
- Quitter reste après Effets ; agrandir/restaurer, plein écran et repli du menu restent distincts. Groupe réorganisé sans recouvrement avec palette/commandes.
- Vérification des clics, de la répétition et du placement, source sale/curseurs/multicurseurs/historique/session/SSH inchangés ; délégation du backend GTK vers Gtk.Window.minimize. Aucun aperçu généré ; validation de bureau physique non revendiquée.

## 1.20.0 — 3 novembre 2026

- Trois éditions : suggestions de chemins actuels par Ctrl+Espace dans tous les navigateurs fichier/dossier/import/XML/recherche ; worker borné, filtrage des modes, réponses périmées et baux SSH protégés, validation explicite et historique du champ.
- Lexeurs JavaScript/XML natifs C et Python, mêmes catégories et parité de couleurs UTF-8 ; langue dans le cache pour les changements d’extension, saisies incomplètes tolérées.
- Vérification Odoo19 : vrai pylint-odoo OCA10.0.12 / PyLint4.0.x dans l’interpréteur local sélectionné, paquet/site importé avant les chemins projet ; mode et exclusions Odoo propres au projet. XML statique borné, DTD/entités externes refusées, structures/IDs locaux et syntaxes, aucune exécution ORM/base. Arch fermé et vide conservé.
- Règles Odoo désactivables par bouton −/réglage projet ; règles génériques restent globales. Mode désactivable pour conserver PyLint générique (dont compatibilité2.x), plugin manquant/vieux incompatible signalé.
- Suppression explicite des 32 archives.tar.gz et de leurs31 sidecars inutiles à la demande de l’utilisateur ; nouveaux paquets.tar.xz seulement. Backend graphique, complétion statique et fonctionnalités précédentes conservés, aucune fenêtre/aperçu/capture/service de validation généré.

## 1.18.0 — 3 novembre 2026

- Petit bouton − par diagnostic dans le panneau d’analyse PyLint (Problèmes Python / Erreurs C). Désactive en un clic le code affiché pour tous les projets, via la liste persistante de règles introduite en 1.17, puis redemande l’analyse même en mode manuel. Le message reste navigable ; la zone du bouton est distincte, dans le clipping et sans déplacement du curseur ni modification du code/historique/multicurseurs.
- L’action capture la règle concrète, jamais un indice mutable de ligne. Vérification de document/version/rapport/volet/contexte et nouvelles identités de rapports natifs ; anciens clics refusés après remplacement, édition, changement de volet/onglet/chemin, fermeture ou modal. Clic répété idempotent, alias symbolique non dupliqué.
- Réutilisation du même chemin de persistance/invalidation que Paramètres → Règles ; échec de sauvegarde ou limite 64 règles/4 095 octets sans changement de la configuration active. Réactivation dans les paramètres. Aucune exclusion par défaut, jamais de bouton pour E0001/syntax-error ou pour un rapport syntaxe/erreur interne.
- Infobulle courte contenant le code et la portée globale, bouton 26 px, texte de message réservé et zones non superposées. Tests des vraies vues et frames sans image, corpus de 60 000 codes natifs, vrais processus PyLint. Backend graphique et adaptateurs d’analyse/complétion inchangés. F11/recherche, clavier AZERTY, fichiers/XML, sessions et SSH fichiers seulement conservés. Aucune validation physique de bureau/GPU/Windows/macOS revendiquée.

## 1.17.0 — 3 novembre 2026

- Configuration globale des règles PyLint ignorées dans les deux éditeurs : Paramètres → Règles…, IDs ou noms, brouillon, appliquer/annuler/tout réactiver. Liste vide par défaut et préférences persistées immédiatement hors des projets.
- Passage effectif de --disable au processus PyLint isolé avant collecte et limitation des diagnostics. Codes/noms normalisés, doublons textuels supprimés, limites 64 règles/4 095 octets/95 caractères par nom ; options, hooks et texte exécutable refusés. Catalogue validé dans l’interpréteur d’analyse choisi ; règles inconnues ou indisponibles signalées explicitement.
- Syntaxe compilée sans exécuter le code avant PyLint, toujours signalée même si E0001 ou syntax-error est demandé parmi les exclusions. Réactivation, sauvegarde échouée sans changement du réglage actif et réponses périmées couvertes. Invalidation des caches de tous les onglets et nouvelle analyse active, même avec analyse automatique désactivée.
- La configuration texte globale n’entre pas dans le journal de disposition par projet : capture, signature et restauration Python restreintes aux anciennes préférences booléennes/numériques. Changements de projet et sessions ne rétablissent pas une ancienne liste globale. C conserve la liste lors d’une réinitialisation d’analyse SSH.
- Vérification réelle avec PyLint 2.12.2/Python3.10, 2.17.7/Python3.11 et 4.1.3/Python3.12. Avertissements d’import de dépendances anciennes capturés afin de ne pas corrompre le protocole natif C ; configuration projet, plugins et init-hook restent ignorés comme auparavant.
- Les deux adaptateurs PyLint sont étendus intentionnellement ; backend graphique Python et ponts de complétion restent inchangés. Tests des vraies frames/modal/champs C/Python, persistance, limites, annulation AZERTY, diagnostics et régressions conservées. Aucun aperçu/capture/serveur ni bureau physique revendiqué. Voir VALIDATION-1.17.0.md.

## 1.16.0 — 2 novembre 2026

- Correction Ctrl+Z qui fermait l’onglet courant sur AZERTY dans les deux véritables éditions. Les codes GLFW représentent des positions QWERTY : AD02/W est la touche marquée Z sur AZERTY. La commande de fermeture interceptait cet événement avant l’historique.
- Nouveau modèle indépendant de disposition dans chaque édition : alphabet latin complet et unique accepté atomiquement sur les 49 positions imprimables connues ; projections directe et inverse. Un relevé partiel/ambigu est rejeté sans modifier la dernière carte valide. Pas d’échange Z/W prédéfini dans le code de production.
- Fournisseur Linux X11/XKB en lecture seule : noms physiques et alias, groupe actuel puis autre groupe latin complet si nécessaire, sans keycodes evdev codés en dur ni changement système. Fournisseur Windows par MapVirtualKey, sans ToUnicode/activation de disposition ; branche GLFW macOS en C. Ces deux plateformes ne sont pas validées sur un bureau physique ; Python sans fournisseur garde sa dernière carte ou QWERTY initial.
- Même traduction pour la file et les API natives pressée/répétée/maintenue, dans l’éditeur et les contrôleurs de champs. Ctrl+Z, Ctrl+Maj+Z, Ctrl+Y, Ctrl+W, Ctrl+A et M déplacé couverts, ainsi que changement de disposition, AltGr, Unicode, multicurseurs et XML atomique.
- Le banc C exécute désormais aussi le véritable editor_keys entre update et l’historique, en plus des routeurs de souris/modal/F11 existants. Pas de simulation de la logique d’annulation ni d’un test limité à un contrôleur local. API OS et services non concernés simulés, aucune fenêtre.
- F11 contextuel, barres de recherche, résultats progressifs, filtres complets, F3 tous fichiers, position de Quitter, sessions et SSH fichiers seulement conservés. Voir VALIDATION-1.16.0.md. Backend graphique Python protégé et archives précédentes inchangés ; aucun aperçu/capture/serveur généré.

## 1.15.0 — 2 novembre 2026

- Barres de contenu horizontale et verticale réellement manipulables dans les deux éditions : clic dans la piste, glissement avec point de prise conservé, capture hors limites et dernier déplacement au relâchement. Clic+relâchement dans une seule frame sans prise persistante.
- Portée calculée sur toutes les lignes du fichier sélectionné, mise en cache, colonnes Unicode scalaires et tabulations de quatre. Défilements bornés à la taille réelle ; bandes réservées et scissor pour séparer texte, surlignages et barres. Une cellule virtuelle de fin conserve les matches vides en fin de ligne.
- Correction du défaut F11 resté en C 1.14 : le routeur global déclenchait A_FULLSCREEN avant le dispatch modal et empêchait le contrôleur de recherche de recevoir la touche. Priorité contextuelle dans la véritable boucle update, consommation unique ; état, dimensions et position système conservés. F11 hors recherche garde son action globale.
- Les anciens tests du seul contrôleur de recherche ne couvraient pas ce défaut. Nouveau banc CPU extrait directement mouse_update, modal_keys et update de la production ; tests de quatre états système, douze focus, événements en file/pressés/répétés et alias natif Alt+Entrée. Réintroduire le vieux dispatch dans l’include de test fait échouer la régression ; aucun changement de source de production pour cet essai négatif.
- Annulation des prises périmées au changement de fichier, fermeture, arrêt, F11 ou géométrie ; F3 et Échap restent utilisables pendant une prise. Instantanés en lecture seule, documents modifiés et historique conservés. Titre de l’édition Python désormais lié à sa version effective, plutôt qu’à un ancien littéral.
- Options, filtres, publication progressive, sélection stable par chemin, F3 sur tous les fichiers et Quitter après Effets conservés. Voir VALIDATION-1.15.0.md ; aucun aperçu/capture/serveur ni validation de bureau physique revendiquée.

## 1.14.0 — 2 novembre 2026

- Publication des fichiers validés pendant le scan : vues immuables et bornées, sans relecture ni attente d’I/O dans la boucle graphique. Python partage ses objets figés ; C partage texte/occurrences via un comptage de références et publie sous un verrou court, acquis sans attente côté interface.
- Arrivées triées par chemin UTF-8 : sélection conservée par chemin, occurrence, focus, vue mise en cache et défilements conservés. F3/Maj+F3 utilisables pendant la recherche ; fin du scan sans retour au premier résultat. Arrêt/erreur conservent les fichiers déjà validés et indiquent leur caractère partiel.
- Correction du comportement 1.13 : F11 agrandit/restaure uniquement le dialogue de recherche, pas la fenêtre système de l’éditeur. Son plein écran préexistant, sa maximisation, ses dimensions et sa position restent inchangés. F11 hors recherche conserve son action globale.
- Quitter déplacé en haut immédiatement à droite d’Effets, doublon inférieur supprimé ; confirmations des documents modifiés inchangées. Espace inférieur toujours rempli exactement jusqu’au bord.
- Tests déterministes avec lecture suivante bloquée, insertion avant la sélection, annulation/erreur et durée de vie des instantanés, fermeture non bloquante, aucun appel de mutation de fenêtre par F11 et contrôles en haut bornés dès 960 pixels. Voir VALIDATION-1.14.0.md ; aucun aperçu/capture/serveur web.

## 1.13.0 — 2 novembre 2026

- Recherche : Agrandir/Restaurer dans toute la fenêtre et plein écran F11 propre à la recherche, avec restauration du mode précédent à la fermeture. Les deux véritables éditions sont conservées.
- Options combinables : casse Unicode, mots entiers, expressions régulières PCRE2, avec/sans sous-dossiers ; quatre filtres d’inclusion/exclusion fichiers/dossiers, globs relatifs `* ? **` et `;`.
- Expressions avec groupes/alternatives/lookarounds, fins de ligne et occurrences vides : défilement F3 global, surlignage multiligne et marqueurs visibles. PCRE2 système, UTF/UCP, sans JIT, budgets de travail/mémoire/temps et annulation ; erreurs de syntaxe/limite explicites, `\C` interdit pour ne pas découper un caractère UTF-8.
- Aucun changement du contenu, des curseurs ou de l’historique des documents. Une modification d’option invalide les réponses en vol, y compris après remise de l’option à sa valeur initiale ; seul le dossier est persisté, pas les flags/filtres/requêtes/résultats.
- Vérifications de moteurs, vrais fichiers/Odoo XML épinglés, clavier/contrôleurs, vues CPU, SSHFS réel et sanitizers, sans aperçu/capture/serveur web : voir VALIDATION-1.13.0.md et GUIDE-RECHERCHE.md.

## 1.12.0 — 1 novembre 2026

- Recherche récursive d’une chaîne littérale dans un dossier modifiable et mémorisé par projet, distinct des bases XML/imports, dans les deux véritables éditions.
- Fenêtre à deux volets : fichiers/compteurs à gauche, instantanés UTF-8 en lecture seule et occurrences surlignées à droite ; l’occurrence active se distingue.
- Ctrl+Maj+F et commande dédiée ; F3/Maj+F3 parcourent toutes les occurrences et changent automatiquement de fichier, avec bouclage et défilement.
- Workers bornés, Arrêter/Échap sans attente de lecture, invalidation des réponses périmées et générations SSH ; pas de lecture de contenu sur la frappe, d’authentification automatique, d’édition ou d’exécution des fichiers recherchés.
- Seul le dossier est persisté, de façon privée et atomique. Le texte et les résultats ne sont pas enregistrés ; ouverture facultative préservant les onglets modifiés.
- Régressions, parité Python/C sur vrais fichiers et XML Odoo épinglés, contrôleurs/clavier, vues CPU et SSH réels : voir VALIDATION-1.12.0.md. Aucun aperçu/capture/serveur web.

## 1.11.0 — 1 novembre 2026

- `sale/` puis Ctrl+Espace : complétion inline des fichiers et sous-dossiers depuis les bases XML configurées, dans les deux véritables éditions.
- Dossiers en premier, fusion/priorité des bases, filtre par préfixe ; Entrée/Tab/clic, ajout de `/` et parcours immédiat du sous-dossier.
- Lecture non récursive et bornée en arrière-plan, annulation sans attente et rejet des réponses périmées ; chemins SSH déjà connectés et génération de montage protégés.
- Noms UTF-8 longs conservés à l’insertion, guillemets et suffixes de chemins préservés, historique atomique. Les identifiants XML, champs/arch vides, balises automatiques et macros sont conservés.
- Vérifications automatisées sans aperçu ; résultats et exclusions dans VALIDATION-1.11.0.md.

## 1.10.0 — dossiers XML, balises automatiques et historique unifié

- Huit dossiers XML par projet, distincts des imports Python : ajout, retrait, ordre enregistré et résolution des chemins relatifs avant les bases historiques. Pas d’index global des identifiants ni de scan au démarrage.
- Dans les documents `.xml`, création de la balise fermante après `>`, fermeture du parent après `</`, indentation entre les balises et suggestions de balises avec Ctrl+Espace. Contexte lexical conservateur : attributs entre guillemets, commentaires, CDATA, instructions et déclarations ne déclenchent pas de paire.
- Balises déjà fermées non dupliquées, noms XML Unicode/qualifiés, option globale « Balises XML automatiques ». Macros globales, dont `xml_form`, et insertion XML 1.9 conservées.
- Ctrl+Z annule, Ctrl+Y et Ctrl+Maj+Z rétablissent dans les deux éditions. Une génération XML, une complétion, un collage ou une édition multicurseur constitue une transaction atomique ; refaire une saisie après annulation efface la branche de rétablissement.
- Dossiers XML sans effet sur Python/PyLint/F5 ; protections SSH, sessions/brouillons privés et absence d’authentification automatique conservées. Aucun aperçu ni capture.

## 1.9.0 — insertion XML depuis le code et alignement de l’explorateur

- Ctrl+Espace sur un chemin `.xml` tapé/collé dans le document : lecture/indexation sur demande, filtre de tous les identifiants, insertion de l’élément choisi en remplacement du chemin.
- Attributs et champs conservés ; champs `arch` vides avec balises de fermeture correctes. Une seule annulation, indentation, Unicode, occurrences d’identifiants dupliqués et espaces de noms.
- Implémentations distinctes : bibliothèque standard Python et parseur libxml2 natif C. Pas de wrapper Python dans le C, pas d’exécution Odoo/ORM/base de données.
- Colonnes fixes pour toutes les actions du menu contextuel, y compris Coller ici et Supprimer ; confirmations et opérations de fichiers préservées.
- Annulation non bloquante dans l’interface, instantané source en lecture seule, vérifications des contextes périmés et des générations de montages SSH.
- Sessions/brouillons 1.8, profils et secrets, PyLint, complétion, macros, multicurseurs et split conservés. Aucun aperçu ni capture.

## 1.8.0 — nom Python Studio et reprise des projets

Fichiers ouverts, ordre, onglet actif, positions/sélections/multicurseurs, défilements et panneaux ; conservation locale des modifications et sans-titre, reprise SSH hors ligne, conflits récupérés séparément, option `--no-session`. Journaux privés en clair. Rapports et archives 1.8 conservés comme historiques.

---

> Document historique de l’édition 1.7.0, avant le renommage. Pour les commandes et limites actuelles, consulter README.md et GUIDE-SESSIONS.md.

# Prisme 1.7.0 — édition C + raylib

**Un véritable éditeur natif pour écrire et exécuter du Python 3, développé en C11 avec raylib 5.5.**
L'interface, le modèle de texte, la coloration, l'historique et les effets sont écrits en C.
Il ne s'agit pas d'un lanceur de la précédente interface Python. Aucun binding `pyray` n'est nécessaire.

## Nouveautés 1.7.0 — SSH natif et connexions mémorisées

**Ouvrez des fichiers distants avec SSHFS/OpenSSH, directement dans l’éditeur C.** Navigation, ouverture, édition, sauvegarde, création, copie et suppression restent intégrées à l’explorateur. Aucun lanceur de l’édition Python : transport, profils, authentification, contrôleur et dialogues sont en C, avec le petit exécutable natif `prisme-ssh-helper`.

- Bouton **SSH** dans la barre d’état ; palette **Ctrl+K**, puis « SSH ».
- **Profils / Nouvelle / Enregistrer le profil / Retirer la sélection** : jusqu’à 32 connexions. Hôte, utilisateur, port, dossier distant et chemin de clé sont conservés, **même si la connexion échoue**. Aucun mot de passe ni passphrase enregistré.
- Fichier partagé avec Python 1.7.0 : `~/.config/prisme/ssh-profiles.json` (ou `$PRISME_CONFIG_DIR/ssh-profiles.json`), privé 0600, atomique et verrouillé. Migration de l’ancien `ssh.json` Python.
- Clé/agent, clé chiffrée et mot de passe ; réponse masquée, presse-papiers de copie/coupe désactivé dans le champ secret ; vérification SHA256 explicite des nouveaux serveurs.
- Reconnexion explicite, brouillons conservés après coupure, Enregistrer sous local ; refus de déconnecter des documents modifiés. Démontage **normal uniquement**, jamais forcé ; contrôle de génération et de propriété du montage.
- **Fichiers distants uniquement**. Python, les dépendances, PyLint et la complétion s’exécutent localement. Aucune installation d’agent Prisme sur le serveur, aucun terminal distant.

Sous Linux : `sudo apt install sshfs openssh-client`. SSHFS doit supporter **`contain_symlinks`** (`sshfs -h`) ; sinon la connexion est refusée. L’édition locale fonctionne sans SSHFS. Cette intégration distante n’est pas proposée pour Windows/macOS.

**Démarrer le précompilé Linux x86_64** : `./bin/prisme` (garder `bin/prisme-ssh-helper` à côté). **Recompiler localement** : `./lancer.sh`. Le précompilé C utilise la libc/libm du système de construction, avec des exigences jusqu’à **GLIBC_2.38** ; sur un Linux plus ancien, reconstruisez localement. Cela ne change pas le backend **Python**, qui reste celui ciblant glibc 2.17.

[Guide SSH et profils](GUIDE-SSH.md) · [Validation 1.7.0 et limites](VALIDATION-1.7.0.md). Aucun aperçu, serveur web ni capture créé.

## Correctif conservé de la 1.5.1 — placement des curseurs au clavier

**Maintenez Ctrl + Alt, puis appuyez sur ↑ ou ↓** pour placer un curseur supplémentaire au-dessus ou au-dessous. Répétez les appuis, ou maintenez la flèche pour utiliser la répétition du clavier. Relâchez ensuite Ctrl et Alt : le texte saisi est inséré à **tous les curseurs**.

- Prise en compte des touches Ctrl/Alt gauches ou droites, des événements rapides capturés dans la même image et de la répétition des flèches. La saisie des caractères **AltGr** reste indépendante : la combinaison n’est reconnue que pour les flèches avec Ctrl.
- La colonne visuelle d’origine est conservée pendant un placement continu, même après une ligne courte. Sur une ligne trop courte, le curseur est placé en fin de ligne ; UTF-8 et tabulations restent alignés sur des limites de caractères valides.
- En inversant le sens, les positions déjà occupées sont sautées : un nouveau curseur est ajouté à la prochaine ligne disponible, sans supprimer ceux déjà placés ni fusionner une sélection existante.
- La création des curseurs ne modifie ni le texte ni l’historique. Chaque édition simultanée reste **une seule annulation Ctrl+Z**. **Échap** revient à un curseur.
- Le raccourci agit uniquement dans le document, pas dans la recherche, la console ou les dialogues. Les propositions de complétion sont fermées pendant le placement.
- Limite inchangée : **256 curseurs par document** ; aucune duplication à la première/dernière ligne, message explicite à la limite.

Exemple : placez le premier curseur dans une ligne, faites **Ctrl+Alt+↓** deux fois pour obtenir trois curseurs, relâchez les modificateurs puis tapez le texte commun. Alt+clic, Ctrl+D, sélection d’occurrences et vue fractionnée restent disponibles.

Si Linux réserve **Ctrl+Alt+↑/↓** au changement d’espace de travail, utilisez **Ctrl+Maj+K / J**, désormais également disponibles dans cette édition C. Vous pouvez aussi réaffecter la combinaison dans votre gestionnaire de fenêtres : Prisme ne peut pas recevoir une touche interceptée par le bureau et ne modifie pas ces réglages à votre place.

## Nouveautés 1.5.0 — complétion Odoo et macros configurables

### Complétion du framework

Les dossiers d’import configurés servent maintenant aussi à **proposer les symboles d’Odoo**, et non seulement à résoudre les imports PyLint. Choisissez dans **Paramètres → Imports Python / complétion → Gérer** le dossier **contenant `odoo/`** : par exemple `/opt/odoo-18` si les sources se trouvent dans `/opt/odoo-18/odoo/`.

- Suggestions automatiques après une courte pause ; **Ctrl+Espace** pour les demander immédiatement.
- Exemples : `fields.Ch` → `Char`, `models.Mo` → `Model`, `@api.dep` → `depends`, `self.ens` → `ensure_one`, champs déclarés dans le document non enregistré.
- **↑↓**, molette, **Tab / Entrée**, **Échap** ; jusqu’à 32 propositions, sans duplication du suffixe lors d’une insertion au milieu d’un nom.
- Analyse **statique en lecture seule**, sans importer/exécuter Odoo ni démarrer une base. Les sources `.py` / `.pyi`, alias, réexports et héritages sont pris en compte. Cas vérifiés sur des sources Odoo 18 et 19 ; cela ne garantit pas un registre ORM complet ou toutes les versions.
- Même Python sélectionné avec `--python`, mais **PyLint n’est pas nécessaire pour cette complétion**. Le Python doit pouvoir parser la syntaxe des sources choisies.
- Cache réinitialisable avec **Actualiser** dans les imports ; les réponses périmées après une modification, un changement de contexte ou Échap ne sont pas réutilisées.
- Les dossiers d’analyse **ne changent pas les imports de F5**. Le mode PyLint manuel reste indépendant de ces suggestions de saisie.

### Macros / extraits de code communs à tous les projets

Ouvrez **Paramètres → Macros / extraits de code → Gérer** pour **créer, modifier ou supprimer** un extrait : déclencheur, titre facultatif, **code multiligne**. **Ctrl+S** l’enregistre immédiatement dans la bibliothèque globale.

Un exemple **`xml_form`** est fourni : tapez-le dans un fichier, choisissez la proposition **Macro**, puis pressez **Tab** ou **Entrée**. Le déclencheur est remplacé par un squelette XML de formulaire Odoo, entièrement personnalisable.

- Collage multiligne, sélection, indentation, annuler/rétablir ; **Ctrl+Tab** change de champ depuis le code.
- Indentation du document conservée ; **une expansion = une annulation**. Le document n’est pas enregistré automatiquement.
- Texte **littéral**, sans exécution ni interprétation de `$0` / `${nom}`. Un seul curseur pour la complétion et les extraits ; le multi-curseur normal reste disponible.
- Bibliothèque globale `snippets.cfg`, à côté des préférences : aucune configuration créée dans les projets. **64 extraits**, 63 caractères par déclencheur, titre de 95 octets, code UTF-8 de 16 Kio par extrait.
- Confirmation avant suppression ou abandon d’un brouillon modifié ; échec de sauvegarde sans perte du brouillon ni remplacement de la bibliothèque précédente.

**[Guide détaillé Odoo et macros →](GUIDE-ODOO-MACROS.md)**

Les vues et la saisie restent natives C/raylib. Aucun aperçu, capture ou service graphique n’est généré pour cette livraison.

## Nouveautés 1.4.2 — polices complètes et dossiers d’import PyLint

### Avertissement sur le caractère `|`

L’avertissement `FONT: Character [0x0000007c] size is bigger than expected font size` a été reproduit sans fenêtre. Dans **Inter à 34 px**, le bitmap du caractère `|` mesure légitimement **35 px**. Raylib 5.5 estimait les rangées de son atlas d’après la taille nominale seulement.

Le correctif CMake conserve le **bitmap complet et les métriques** et dimensionne les rangées selon la hauteur réelle maximale des glyphes. Ce dépassement normal devient une information de débogage, pas un avertissement. Aucune réduction de police, suppression de caractère ni désactivation générale des avertissements. Les fichiers de fontes/licences restent inchangés.

Test CPU réel : **15 atlas**, **409 caractères** chacun, trois fontes et cinq tailles ; **1 819 614 pixels alpha d’origine conservés**, sans chevauchement, rognage ni avertissement de police. Aucun aperçu/image exportée/GPU utilisé.

### Ajouter le dossier d’un framework à l’analyse

1. Ouvrez la **roue dentée / Paramètres**.
2. **Imports Python / complétion → Gérer → Ajouter un dossier**.
3. Choisissez le **dossier parent du paquet Python** de votre framework, puis **Ajouter ce dossier**.

Exemple : si votre installation contient `/opt/mon_sdk/python/mon_framework/__init__.py`, ajoutez **`/opt/mon_sdk/python`**, pas nécessairement le dossier `mon_framework` lui-même.

- Jusqu’à **8 dossiers supplémentaires**, affichés dans une liste et retirables avec l’icône corbeille. Retirer un chemin **ne supprime aucun fichier**.
- Les chemins sont **enregistrés immédiatement par projet** dans le dossier des préférences, sans créer de fichier dans votre projet. Changer d’atelier charge la liste de cet atelier ; le choix d’un dossier d’import ne change pas la racine de l’explorateur. En mode d’analyse manuel, utilisez F4 après un changement d’atelier ; ce changement ne réactive pas l’analyse automatique.
- Ajout/retrait invalide les diagnostics en cache et **relance l’analyse du fichier actif**, y compris en mode manuel. Les vrais problèmes restent signalés : aucune désactivation globale de `import-error`, `no-name-in-module` ou des autres contrôles.
- Le sélecteur de dossiers d’import inclut les dossiers cachés et ceux normalement filtrés dans l’explorateur (par exemple `.venv`, `build`). Vous pouvez aussi saisir/coller un chemin complet. Échap annule le choix et revient à la liste.
- **Ces chemins concernent PyLint et la complétion statique**, pas l’exécution F5. L’interpréteur choisi avec `--python` reste utilisé pour l’analyse. Les chemins sont des données, pas des commandes/hooks ; les modules du framework ne sont pas lancés pour leur analyse statique.
- Si un dossier enregistré n’existe plus, l’analyse indique le chemin indisponible au lieu de déclarer tout correct. Accents, espaces, apostrophes et séparateurs dans les noms sont conservés sans découpage de type `PYTHONPATH`.

Option en ligne de commande, répétable :

```bash
./bin/prisme --project "/chemin/mon-projet" \
    --python "/chemin/mon-venv/bin/python" \
    --import-dir "/opt/mon_sdk/python"
```

Les options `--import-dir` complètent la liste du projet pour **la session** ; elles ne réécrivent pas seules les préférences. La palette de commandes propose aussi **Configurer les imports Python / complétion**.

Les chemins sont supplémentaires à ceux de l’interpréteur/projet. Si plusieurs installations portent le même nom, choisissez l’environnement Python approprié. Un framework générant ses modules/attributs uniquement à l’exécution, ou une extension native, peut nécessiter des sources/fichiers d’interface compatibles avec sa version de PyLint ; Prisme **n’active pas l’importation non sécurisée d’extensions** et ne masque pas globalement les erreurs pour contourner cela.

## Correctif 1.4.1 — compatibilité PyLint 2.x et erreurs de démarrage

Prisme 1.4.0 utilisait l’option `--source-roots`, inconnue de certaines versions de **PyLint 2.x**. Cela pouvait interrompre l’analyse dès le démarrage (erreur d’usage, code 2 ou 32 selon le chemin d’appel/version), sans rapport avec une erreur dans votre document.

- Cette option n’est plus passée. Le dossier du projet et celui du document restent accessibles à l’analyse des imports, sans enregistrer ni exécuter votre code.
- Les erreurs de démarrage ne sont plus réduites à `SystemExit: 32` : Prisme conserve la **cause détaillée** émise par PyLint et affiche sa **version** avec le code d’échec. Le début d’une longue aide n’écrase plus le message utile situé à la fin.
- Vérification réelle sous Linux : **PyLint 2.7.4, 2.12.2, 2.13.9, 2.17.7, 3.3.9 et 4.1.3**, avec **113 assertions par version**. Les anciennes versions qui ne donnent qu’une position, sans colonne de fin, soulignent le même identifiant que les versions récentes.
- **20/20 tests CTest** dans la matrice de compatibilité, en Release et en Debug ASan/UBSan avec détection des fuites. Les tests supplémentaires couvrent aussi les erreurs 2/32, stderr/stdout, Unicode et la capture bornée de messages très longs.

**Il n’est pas nécessaire de remplacer votre PyLint 2.x par PyLint 4.** Extrayez la nouvelle archive dans un dossier neuf et utilisez son exécutable, ou recompilez avec `./lancer.sh`. L’adaptateur est embarqué dans l’exécutable C : modifier seulement `tools/lint_bridge.py` sans reconstruire ne met pas à jour un ancien binaire. Conservez le même `--python`, puis relancez l’analyse avec **F4**.

## Nouveautés 1.4 — ouvrir un fichier et diagnostics PyLint dans le code

### Bouton de l’explorateur

Le premier bouton de l’en-tête **Dossiers** devient **Ouvrir un fichier · Ctrl+O** (icône de fichier), à la place de Nouveau fichier. Il ouvre le sélecteur de fichiers de Prisme. Le **clic droit conserve Créer un fichier / Créer un dossier**, Copier, Coller ici et Supprimer ; le bouton `+` des onglets et Ctrl+N créent toujours un document sans titre.

### Erreurs directement dans le code

- **Vérification de syntaxe et véritable PyLint**, sans lancer le programme analysé et sans imposer d’enregistrement. Le texte courant de l’éditeur est transmis à l’analyseur, même s’il diffère du fichier sur disque.
- Analyse automatique après une **pause d’une seconde** sur les fichiers Python visibles, y compris les deux volets. Un processus externe distinct laisse l’éditeur et l’exécution F5 indépendants.
- **Soulignements ondulés et marqueurs dans la marge** : rouge pour les erreurs, jaune pour les avertissements, bleu pour les conseils/conventions. Au survol, une infobulle affiche le message complet ; cliquer un marqueur rejoint sa position.
- **Panneau Erreurs** : liste cliquable du fichier actif avec ligne, colonne, code (par exemple `E0602`, `W0611`) et message PyLint. La molette fait défiler la liste. Une erreur de la dernière exécution reste accessible par un lien vers Sortie.
- **F4** lance/recommence l’analyse et affiche Erreurs ; **F2 / Maj+F2** rejoignent le diagnostic suivant/précédent ; **Ctrl+Maj+M** ouvre Erreurs. Le panneau propose également un bouton d’analyse.
- Les résultats d’une ancienne version du texte sont masqués puis remplacés : pas de soulignements périmés après modification, fermeture d’un onglet ou Enregistrer sous.
- Paramètre **Analyse Python automatique**, enregistré avec les préférences. `--no-lint` et `--safe-mode` la désactivent au lancement ; **F4 reste utilisable manuellement**.

### Installer PyLint dans l’interpréteur utilisé

L’interface et le contrôleur restent **en C11**. PyLint est un **outil Python externe**, non fourni dans l’archive. La vérification de syntaxe utilise le Python sélectionné ; PyLint doit être installé **dans ce même environnement**. Un environnement virtuel est recommandé :

```bash
python3 -m venv .venv
.venv/bin/python -m pip install pylint
./bin/prisme --python "$PWD/.venv/bin/python"
# ou : ./lancer.sh --python "$PWD/.venv/bin/python"
```

Sous Windows, après compilation :

```bat
py -m venv .venv
.venv\Scripts\python.exe -m pip install pylint
build-1.7.0\Release\prisme.exe --python "%CD%\.venv\Scripts\python.exe"
```

Avec Ninja, le binaire Windows est généralement `build-1.7.0\prisme.exe`. L’option `--python` ou la variable `PRISME_PYTHON` choisit le même interpréteur pour F5 et l’analyse. Les chemins d’interpréteur relatifs sont résolus depuis le dossier de lancement.

**Si PyLint manque, Prisme ne prétend pas que tout est correct** : il continue à signaler les erreurs de syntaxe et affiche « PyLint absent » avec une indication d’installation dans Erreurs. Après installation, appuyez sur F4. Une analyse trop longue (>15 s) ou un interpréteur invalide produit un diagnostic d’indisponibilité, pas une fausse liste vide.

Pour éviter l’exécution automatique d’un module ou d’un hook du projet, l’analyse démarre Python avec **`-I -B`**, utilise une configuration PyLint vide et ne charge pas votre `.pylintrc` / `pyproject.toml` ni leurs plugins/hooks. Les imports locaux restent pris en compte pour l’analyse statique. Les installations `pip --user` peuvent être invisibles en mode isolé : utilisez plutôt le venv ci-dessus. Les commentaires PyLint dans le code restent utilisables.

La liste conserve au plus **256 diagnostics par document**, avec priorité aux erreurs puis aux avertissements lorsque la limite est atteinte. Les fichiers ne sont ni réécrits ni exécutés par cette analyse. Ce n’est toutefois pas une sandbox pour des outils/environnements Python non fiables. **Aucun aperçu, capture ou serveur graphique n’est généré.**

## Nouveautés 1.3 — fenêtre sans titre et clic droit dans l’explorateur

### Fenêtre et fermeture

- **Barre de titre du système supprimée** pour l’éditeur. Le menu supérieur de Prisme reste présent et repliable avec F8 ; la démo Constellation garde sa fenêtre habituelle.
- Bouton **Quitter** toujours disponible à droite de la barre d’état, même quand les autres panneaux sont repliés. Il utilise la confirmation d’enregistrement des documents modifiés ; Alt+F4 et la palette de commandes permettent aussi de quitter.
- Déplacez la fenêtre par glisser sur le logo/bandeau, une zone libre des onglets ou les informations non cliquables de la barre d’état. Double-cliquez sur ces zones pour maximiser/restaurer. Les bords et coins redimensionnent la fenêtre en mode non maximisé.
- Alt+clic conserve son rôle de multi-curseur ; F11 garde le plein écran corrigé sans espace sous la barre d’état.

### Menu au clic droit

Faites un **clic droit sur un dossier, un fichier ou une zone vide de l’explorateur** :

| Commande | Comportement |
|---|---|
| **Créer un fichier…** | Demande un nom, crée réellement le fichier dans le dossier cible et l’ouvre dans un onglet. |
| **Créer un dossier…** | Demande un nom et crée le dossier, sans écraser un élément existant. |
| **Copier** | Place l’élément choisi dans le presse-papiers pour le recoller dans Prisme. |
| **Coller ici** | Copie les fichiers/dossiers du presse-papiers dans le dossier cible, récursivement. |
| **Supprimer…** | Ouvre une confirmation avant suppression de l’élément et, pour un dossier, de son contenu. |

Sur un fichier, les créations/collages visent son dossier parent ; sur un dossier, ils visent ce dossier. Une zone vide vise la racine du projet. Le menu reste dans l’écran et se ferme avec Échap ou un clic ailleurs ; Haut/Bas/Entrée fonctionnent aussi.

**Pour coller un élément copié depuis le gestionnaire de fichiers** : copiez-le normalement, revenez dans Prisme, faites un clic droit sur le dossier cible, puis **Coller ici**. Les formats natifs sont prévus pour X11/XWayland (`text/uri-list`, fichiers copiés GNOME/KDE), Windows (`CF_HDROP`) et macOS (Pasteboard). Les chemins/URI locaux sont reconnus ; pas de téléchargement d’URL distante. Le code Windows/macOS n’a pas été exécuté dans cet environnement.

La copie se déroule sur un **thread C**, avec un compteur et une commande d’annulation ; l’interface reste réactive. Les noms en conflit deviennent `nom (copie).ext`, `nom (copie 2).ext`, etc. Aucun fichier existant n’est remplacé. Une annulation/erreur déclenche le nettoyage des copies créées ; une erreur de nettoyage est signalée explicitement. Les presse-papiers de type « couper » sont traités comme une **copie**, jamais comme un déplacement.

**Attention : la suppression est définitive, sans corbeille ni annulation.** Elle nécessite une confirmation, refuse la racine du projet et reste limitée au projet. Enregistrez ou fermez les documents modifiés concernés avant de supprimer ; arrêtez également un script Python exécuté depuis la cible. Sous POSIX, les liens symboliques sont copiés/supprimés comme liens, jamais parcourus par les opérations récursives. Sous Windows, la copie des points de réanalyse est refusée.

Le bouton `+` habituel continue de créer un onglet sans titre ; le menu de l’explorateur crée un fichier **sur disque**. Aucun aperçu, capture ou serveur n’est généré.

## Ajustement 1.2.4 — explorateur épuré

- Suppression complète de la carte animée **« Moins de bruit. Plus d’idées. »** et de son sous-titre, en bas de l’explorateur.
- Suppression du bloc **« MOTEUR NATIF »**, de sa version raylib et de la mention **« 100 % natif »** dans ce panneau.
- La liste des dossiers et fichiers utilise désormais toute la hauteur disponible sous son en-tête, jusqu’à la barre d’état avec une marge intérieure de 8 px. Les 269 px récupérés permettent d’afficher environ huit lignes supplémentaires à taille de fenêtre identique.
- Les dernières lignes sont entièrement lisibles et cliquables ; le défilement s’adapte automatiquement quand la fenêtre grandit ou passe en plein écran.
- Les commandes Nouveau fichier, Ouvrir un dossier, Replier, ainsi que le correctif plein écran 1.2.3, restent conservés. Les effets visuels de l’éditeur sont inchangés ; seule l’animation décorative de l’explorateur est retirée.
- Aucun aperçu ni capture n’est généré.

## Correctif 1.2.3 — bas de l’écran en plein écran

La correction 1.2.2 plaçait la barre au bas de la **surface logique**, mais raylib 5.5 pouvait conserver la hauteur de l’ancienne fenêtre après F11. Par exemple, une fenêtre maximisée de 1 920 × 1 040 devenait une surface de rendu de 1 920 × 1 080, alors que `GetScreenHeight()` renvoyait encore 1 040 : les 40 pixels restants étaient hors de la géométrie de l’éditeur.

Cette version corrige **le backend plein écran**, pas seulement la position du pied de fenêtre :

- F11, Alt+Entrée, le bouton plein écran et `--fullscreen` sélectionnent le mode courant du **moniteur actif**, sans réutiliser la taille plus petite de l’ancienne fenêtre.
- Les événements de taille synchronisent désormais la hauteur logique et le viewport **également en plein écran**, y compris lorsqu’un événement arrive après le basculement.
- Les décalages de letterboxing sont remis à zéro ; la barre d’état atteint le dernier pixel de la surface rendue.
- En quittant le plein écran, la taille et la position précédentes sont restaurées. Si la fenêtre était maximisée, elle redevient maximisée.
- Les coordonnées de l’interface restent logiques : on ne remplace pas naïvement `GetScreenHeight()` par la hauteur physique, ce qui doublerait les dimensions sur Retina/HiDPI.
- Aucun dossier `preview`, serveur graphique ou aperçu n’est recréé.

La régression est reproduite **sans fenêtre ni GPU** avec les fonctions réelles de raylib : le même scénario donne **40 px avant correction, 0 px après correction**. Les tests complètent les vérifications de rectangles de 1.2.2 en contrôlant aussi la taille du viewport et la projection.

## Ajustement 1.2.2 — lisibilité et bord inférieur

- **Barre d’état collée au bord inférieur de la fenêtre** : sa hauteur est déduite de la taille actuelle de la surface, sans marge supplémentaire en dessous. L’explorateur et la console utilisent le même point d’ancrage, y compris après redimensionnement.
- **Fenêtre maximisée au lancement**, sans imposer le plein écran. `--windowed`, `--width` et `--height` conservent un lancement en fenêtre classique ; F11 reste indépendant.
- **Texte d’interface d’au moins 17 px, libellés d’au moins 15 px**, y compris onglets, menus, console, barre d’état, champs, aide et dialogues.
- **Code en 20 px par défaut**, avec interligne augmenté et police **JetBrains Mono Medium**, plus soutenue. Ctrl+plus / Ctrl+moins permettent de régler le code entre 17 et 32 px.
- **Contrastes renforcés** pour les textes secondaires, commentaires et numéros de ligne ; le fond sombre et les accents menthe sont conservés.
- Les légendes trop longues sont **abrégées avec `…`**, jamais rendues minuscules. La barre d’état adapte ses informations secondaires à la largeur disponible pour éviter les chevauchements.
- Les anciennes préférences passent automatiquement à la nouvelle taille lisible ; les réglages de police des nouvelles préférences restent personnalisables et persistants.
- Le dossier de service **`preview` a été supprimé**, ainsi que ses services actifs. Aucun aperçu ni capture n’est créé ou inclus dans cette livraison.

```bash
./bin/prisme                 # fenêtre maximisée
./bin/prisme --windowed      # fenêtre classique
./bin/prisme --width 1100 --height 740
```

## Ajustement 1.2.1 — Exécuter dans les onglets

Le bouton **Exécuter** se trouve désormais **dans la barre des onglets, juste à gauche des trois points `…`**, et non dans le menu supérieur.

- Il reste accessible lorsque le menu supérieur est replié (F8), y compris en mode concentration (F9).
- Pendant l’exécution, ce même bouton devient **Arrêter** ; F5 / Maj+F5 restent inchangés.
- L’exécution concerne toujours le fichier du volet actif en vue fractionnée.
- La place des commandes est réservée avant de calculer le nombre d’onglets visibles : aucun chevauchement avec les fichiers ou le bouton `+`.
- Aucun nouvel aperçu ni capture n’a été généré pour cette mise à jour.

## Nouveautés 1.2 — plus de place pour le code

La **colonne verticale de raccourcis à l’extrême gauche a été supprimée**, sans laisser de marge vide. L’explorateur commence au bord de la fenêtre ; quand il est replié, l’éditeur utilise toute la largeur.

Les trois panneaux se replient **indépendamment**, sans fermer les documents ni interrompre l’exécution Python :

| Panneau | Replier / réafficher au clavier | À la souris |
|---|---|---|
| Explorateur de dossiers | **Ctrl+B** | Flèche dans son en-tête / **Dossiers** en bas |
| Menu supérieur (logo, commandes, effets…) | **F8** | Flèche à droite du menu / **Menu** en bas |
| Console | **Ctrl+J** | Flèche dans son en-tête / **Console** en bas |
| Les trois à la fois | **F9** | **Tout replier / Restaurer** en bas |

La barre d’état inférieure reste visible : ses boutons permettent de retrouver chaque panneau **même si le menu supérieur est masqué**. La roue dentée et l’aide y sont également accessibles. Les boutons surlignés indiquent les panneaux ouverts.

- **F9** mémorise la combinaison actuelle, replie les trois panneaux, puis la restaure à la pression suivante. Un bouton individuel quitte ce mode et ne change que le panneau choisi.
- L’éditeur récupère réellement la largeur et la hauteur libérées. Les onglets, la recherche, les deux volets, leurs en-têtes et les zones cliquables se repositionnent ensemble.
- **Glissez le bord droit de l’explorateur** pour régler sa largeur ; **glissez le bord supérieur de la console** pour régler sa hauteur. Replier un panneau ne réinitialise pas sa taille.
- La visibilité des trois panneaux et leurs dimensions sont conservées à la fermeture. Les anciennes préférences à quatre champs restent lisibles.
- Le mode temporaire **F9 / `--zen`** n’écrase pas la visibilité précédente dans les préférences. En mode concentration, F5 exécute toujours le script et conserve sa sortie, sans rouvrir automatiquement la console ; Ctrl+J la révèle.
- Multi-curseurs, vue fractionnée, plein écran et protections graphiques des versions précédentes sont conservés.

```bash
./bin/prisme --zen
./bin/prisme --split --zen --fullscreen
```

## Nouveautés 1.1 — édition multiple, deux volets, plein écran

### Multi-curseurs (jusqu'à 256 par document)

- **Alt+clic** ajoute ou retire un curseur à l'endroit cliqué.
- **Ctrl+Maj+K / J** ajoute un curseur au-dessus / au-dessous sans conflit avec les raccourcis GNOME usuels. **Ctrl+Alt+Haut / Bas** reste disponible ; les appuis répétés (ou la répétition de la flèche maintenue) conservent la colonne d’origine et sautent les positions déjà occupées. Relâchez Ctrl et Alt pour saisir le même texte à tous les curseurs.
- **Ctrl+D** sélectionne le mot courant, puis ajoute l'occurrence suivante à chaque pression.
- **Ctrl+Maj+L** sélectionne toutes les occurrences exactes du texte sélectionné, dans la limite de 256. Sans sélection, le mot sous le curseur est utilisé.
- Tapez ou collez pour modifier simultanément toutes les sélections. Les curseurs et sélections supplémentaires sont visibles et leur nombre apparaît dans la barre d'état.
- **Échap** revient à un seul curseur. Un clic normal ou une navigation vers une occurrence de recherche remplace aussi la sélection multiple.

La saisie UTF-8, les paires de caractères, les nouvelles lignes avec indentation, Tab/Maj+Tab, les commentaires, Retour arrière/Suppr et les déplacements avec Maj fonctionnent avec plusieurs curseurs.
Une opération simultanée constitue **une seule étape d'annulation** : Ctrl+Z/Ctrl+Maj+Z restaure aussi les curseurs et sélections.
Les sélections qui se chevauchent et les curseurs identiques sont fusionnés pour éviter de modifier deux fois le même texte.

**Copier/couper/coller :** les sélections sont copiées dans l'ordre du document, séparées par des sauts de ligne. Sans sélection, les lignes visées sont copiées une seule fois. Si le presse-papiers contient exactement autant de lignes qu'il y a de curseurs, une ligne est collée par curseur ; sinon, le même texte est inséré à chaque curseur.

**Changement de raccourci :** Ctrl+D sert maintenant à l'occurrence suivante. La duplication passe à **Ctrl+Maj+D** (lignes visées en mode multi-curseurs).
La complétion lexicale Ctrl+Espace reste réservée à un seul curseur ; un message rappelle d'utiliser Échap.
Si votre gestionnaire de fenêtres réserve Alt+clic ou Ctrl+Alt+Haut/Bas, utilisez **Ctrl+Maj+K / J**. Le test automatisé vérifie le routage des touches, pas leur disponibilité sur votre bureau GNOME physique.

### Deux fichiers côte à côte

- **F7** ou `Ctrl+\` active/désactive la vue fractionnée.
- Cliquez dans un volet, ou utilisez **Ctrl+1 / Ctrl+2**, puis choisissez un onglet ou ouvrez un fichier : il s'affiche dans le volet actif.
- **Glissez le séparateur central** pour régler la largeur des volets.
- Chaque fichier garde son texte, ses curseurs, son historique et son défilement. Ctrl+S et F5 s'appliquent au **volet actif** ; la console est commune.
- Masquer la séparation ne ferme pas les documents et n'efface pas les modifications.
- Si le même document est affiché dans les deux volets, ils partagent son texte, ses curseurs et son défilement : ce ne sont pas deux copies indépendantes.

### Plein écran

**F11** ou **Alt+Entrée** bascule entre fenêtre et plein écran natif. La taille de la fenêtre est restaurée au retour, sans fermer les documents.
Échap quitte d'abord une sélection multiple, une recherche ou un dialogue ; en plein écran sans ces états, il revient à la fenêtre.

```bash
./bin/prisme --split
./bin/prisme --split --fullscreen
# Tous ces modes restent compatibles avec le mode de secours graphique :
./bin/prisme --split --safe-mode --software
```

Toutes ces fonctions de l'éditeur sont implémentées **en C**, y compris le rendu et la gestion des deux volets. Le pont navigateur n'est pas nécessaire à l'application native.

## La différence importante

- **L'éditeur fonctionne sans Python** : fenêtre, fichiers, édition, coloration et effets sont natifs en C.
- **Python 3 est nécessaire pour F5 et l’analyse**, puisque les documents édités sont des scripts Python. PyLint reste une dépendance optionnelle de l’analyseur externe.
- Le script est exécuté par un processus externe, jamais par un interpréteur Python embarqué dans l'interface.
- `demos/constellation.c` est une démo graphique **entièrement en C**, sans Python.

## Lancement immédiat sous Linux x86-64

L’archive contient l’éditeur, la démo et le helper SSH natif déjà compilés :

```bash
cd prisme-c
chmod +x bin/prisme bin/constellation bin/prisme-ssh-helper
./bin/prisme
```

Le binaire fourni a été testé sous Linux x86-64 et demande **glibc 2.38 ou plus**, un bureau X11/XWayland et **OpenGL 2.1**. Le MSAA est désactivé par défaut.
Sur une distribution plus ancienne, une autre architecture, Windows ou macOS, compilez les sources ci-dessous.
Les dossiers `assets/` et `examples/` doivent rester à côté de `bin/`, comme dans l'archive.

## Correctif du crash `GLXBadFBConfig`

Cette version remplace la précédente compilation OpenGL 3.3/MSAA 4x :

- **OpenGL 2.1 par défaut**, compatible avec davantage de pilotes et de machines virtuelles.
- **Pas de MSAA imposé** ; `--msaa` l'active seulement à votre demande.
- Garde-fous dans raylib 5.5 : un échec de création du contexte ne doit plus accéder à une fenêtre nulle ni initialiser le GPU. Il produit un diagnostic et le **code de sortie 3**, pas un segfault.
- `--safe-mode` : effets, minimap et MSAA coupés, 30 FPS ; vos préférences enregistrées restent inchangées.
- `--software` : demande le rendu logiciel Mesa sous Linux, si Mesa/llvmpipe est installé. Il faut toujours un bureau X11/XWayland.

Après extraction de **la nouvelle archive**, essayez :

```bash
chmod +x bin/prisme bin/constellation bin/prisme-ssh-helper
./bin/prisme --safe-mode
# Si le pilote matériel ou la session virtuelle pose encore problème :
./bin/prisme --safe-mode --software
# Pour obtenir les informations sur le contexte et le pilote :
./bin/prisme --safe-mode --software --verbose-gl
```

`--no-effects` seul ne répare pas la création d'un contexte. `--safe-mode` ne change pas non plus le backend d'un ancien binaire : remplacez-le ou recompilez **les sources corrigées**.
L'erreur GLX seule ne permet pas de conclure que votre carte graphique est trop ancienne.
Si le problème persiste : `glxinfo -B` (`sudo apt install mesa-utils` sous Debian/Ubuntu), puis transmettez sa sortie et celle de `--verbose-gl`.

Voir aussi [CORRECTIF-GLX.md](CORRECTIF-GLX.md) pour la cause et les tests de non-régression.

## Correctif de liaison 1.1.1 — `undefined reference to buf_apply_edits`

Le diagnostic détaillé et le test de reproduction sont dans [CORRECTIF-LIAISON.md](CORRECTIF-LIAISON.md).

`buf_apply_edits` est défini dans `src/buffer.c`, pas dans `multicursor.c`.
Un ancien objet `buffer.c.o` conservé après extraction d'une nouvelle archive peut ne pas contenir cette fonction. Ce cas a été reproduit : `test_core` se construit, puis `test_multi` échoue à l'édition des liens.

Sans supprimer vos fichiers ni désactiver les tests, construisez dans un **nouveau dossier** :

```bash
cmake -S . -B build-1.7.0 -DCMAKE_BUILD_TYPE=Release -DPRISME_OPENGL_VERSION=2.1
cmake --build build-1.7.0 --parallel
ctest --test-dir build-1.7.0 --output-on-failure
./build-1.7.0/prisme --split
```

La version 1.1.1 ajoute :
- une vérification de la présence de l'implémentation avant de construire ;
- une empreinte du **contenu** des sources/en-têtes dans les commandes de compilation, recalculée à la configuration : les objets Prisme sont reconstruits même si les dates d'une archive extraite sont anciennes ;
- des dossiers de construction versionnés pour les deux lanceurs.

Si `src/buffer.c` provient encore d'une ancienne version, extrayez **toute l'archive actuelle dans un dossier neuf**. Copier seulement `multicursor.c` ne suffit pas.
N'ajoutez pas de fonction vide pour contourner l'erreur : elle supprimerait l'édition simultanée et l'annulation atomique.

## Compiler à partir des sources

Prérequis : un **compilateur C11** (GCC, Clang ou MSVC) et **CMake 3.24 ou supérieur**.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/prisme
```

CMake télécharge les sources officielles **raylib 5.5**, vérifie leur SHA-256, applique les garde-fous de démarrage ainsi que les corrections plein écran/polices, puis les compile en bibliothèque statique.
Il n'utilise pas une raylib système potentiellement non corrigée.
Une connexion est nécessaire au premier build, sauf si les sources officielles sont déjà en cache.
Après compilation, l'éditeur ne dépend d'aucun service réseau.
Les ressources sont copiées automatiquement dans le dossier du binaire construit.

### Linux — dépendances de compilation Debian/Ubuntu

```bash
sudo apt install build-essential cmake libgl1-mesa-dev \
  libx11-dev libxrandr-dev libxi-dev libxcursor-dev libxinerama-dev
./lancer.sh
```

`lancer.sh` configure et met à jour un dossier versionné `build-1.7.0/` en OpenGL 2.1, puis démarre le binaire C. Il ne réutilise pas aveuglément l’ancien exécutable `build/prisme`.
Il ne lance pas de programme Python pour afficher l'interface.

### Windows

Installez CMake et les outils de compilation C de Visual Studio (ou un environnement MinGW compatible).
Dans une invite de commandes de développement :

```powershell
cmake -S . -B build-1.7.0 -DPRISME_OPENGL_VERSION=2.1
cmake --build build-1.7.0 --config Release --parallel
.\build-1.7.0\Release\prisme.exe
```

Le script `LANCER_WINDOWS.bat` utilise un dossier versionné `build-1.7.0/`. Avec un générateur mono-configuration comme Ninja, le binaire peut se trouver dans `build-1.7.0\prisme.exe`.
Le code Windows est inclus, mais **seule la version Linux a été compilée et testée ici**.
Pour cette version Windows, utilisez de préférence un chemin de projet ASCII : les opérations de fichiers C ne disposent pas encore d'une couche complète de chemins Unicode Windows.

### macOS

Installez les outils de ligne de commande Xcode et CMake, puis utilisez les commandes génériques de compilation.
CMake s'occupe des frameworks nécessaires à raylib. macOS n'a pas été testé ici.

### Recompiler sans réutiliser l'ancien cache OpenGL

```bash
cmake -S . -B build-1.7.0 -DCMAKE_BUILD_TYPE=Release -DPRISME_OPENGL_VERSION=2.1
cmake --build build-1.7.0 --parallel
./build-1.7.0/prisme --safe-mode
```

Option avancée uniquement si votre pilote le permet : `-DPRISME_OPENGL_VERSION=3.3`, dans un autre dossier de construction. Le backend est fixé **à la compilation** et indiqué par `prisme --help`.
Les garde-fous s'appliquent aux deux backends, ainsi qu'à la démo C.

## Choisir le Python à exécuter

Par défaut : `python3` sous Linux/macOS, `python` sous Windows.

```bash
./build/prisme --python /chemin/vers/venv/bin/python
./build/prisme --project /chemin/vers/mon_projet
./build/prisme /chemin/vers/script.py
./build/prisme --no-effects
```

Vous pouvez également définir `PRISME_PYTHON` avec le chemin de l'exécutable de votre environnement virtuel.
L'option `--python` attend **un exécutable**, pas une commande shell avec des arguments.
Les dépendances de vos scripts et PyLint doivent être installés dans cet environnement. L’interface native C ne dépend d’aucun paquet Python.

Sans interpréteur disponible, l'éditeur reste utilisable ; l'erreur est affichée dans la console lorsque vous demandez une exécution.
Aucun programme n'est lancé automatiquement, sauf avec l'option explicite `--demo`.

## Fonctionnalités incluses

### Édition native

- Plusieurs onglets (jusqu'à 24), explorateur de dossiers repliables et ouverture rapide.
- Navigateur de fichiers intégré à raylib : ouvrir un fichier, ouvrir un dossier et enregistrer sous.
- Texte UTF-8, saisie des accents, sélection à la souris, double-clic sur un mot, copier/couper/coller.
- Sauvegarde atomique avec fichier temporaire, conservation du BOM UTF-8 et des fins de ligne CRLF.
- Confirmation avant la fermeture d'un document modifié ; confirmation avant remplacement d'un autre fichier.
- Annuler/rétablir (128 états au maximum, historique borné en mémoire).
- Indentation automatique, paires de parenthèses/guillemets et indentation de blocs.
- Commenter/décommenter et dupliquer une ligne ou une sélection.
- Recherche, navigation entre occurrences, accès à une ligne et zoom du code.
- Palette de commandes et complétion de mots sur demande.
- Coloration lexicale Python **implémentée en C**, y compris commentaires et chaînes multilignes.
- Minimap cliquable ; explorateur et console redimensionnables ; explorateur, menu supérieur et console repliables indépendamment.

### Exécution

- Exécution du contenu courant, même non enregistré, dans un processus Python séparé.
- Capture non bloquante de stdout/stderr sous POSIX ; suivi des sorties sous Windows.
- Saisie de réponses pour `input()` dans la console.
- Arrêt du programme avec Maj+F5 ; arrêt du groupe de processus sous POSIX.
- Noms du fichier d'origine conservés dans `__file__` et les traces d'erreur.
- Répertoire de travail : dossier du document ; les imports de modules locaux fonctionnent.

### Effets raylib

- Halo ambiant animé et accents menthe sur fond sombre.
- Curseur fluide et lumineux.
- Particules à la frappe.
- Survols, sélection, occurrences et ligne active éclairée.

**F6** bascule immédiatement vers le mode calme. La roue dentée règle les effets, la minimap, l’analyse, les imports Python, les macros et la police.
Hors `--safe-mode`, les préférences sont enregistrées à la fermeture, dans `~/.config/prisme-c/settings.cfg` sous Linux/macOS, ou dans `%APPDATA%/prisme-c/settings.cfg` sous Windows.
`PRISME_CONFIG_DIR` permet d'utiliser un autre dossier existant (`settings-c.cfg`).

## Raccourcis principaux

| Raccourci | Action |
|---|---|
| Ctrl+N / Ctrl+O | Nouveau / ouvrir un fichier |
| Ctrl+Maj+O | Ouvrir un dossier |
| Ctrl+S / Ctrl+Maj+S | Enregistrer / enregistrer sous |
| Ctrl+W / Ctrl+Tab | Fermer / changer d'onglet |
| Ctrl+Z / Ctrl+Maj+Z / Ctrl+Y | Annuler / rétablir |
| Ctrl+A, C, X, V | Sélectionner, copier, couper, coller |
| Ctrl+F / Ctrl+G | Rechercher / aller à une ligne |
| Entrée / Maj+Entrée dans la recherche | Occurrence suivante / précédente |
| Ctrl+P | Ouvrir rapidement un fichier |
| Ctrl+Maj+P / Ctrl+K | Palette de commandes |
| Ctrl+Espace | Compléter un mot |
| Tab / Maj+Tab | Indenter / désindenter |
| Ctrl+/ / Ctrl+Maj+D | Commenter / dupliquer |
| Alt+clic | Ajouter / retirer un curseur |
| Ctrl+Maj+K / J ou Ctrl+Alt+Haut / Bas | Ajouter un curseur à la ligne voisine |
| Ctrl+D / Ctrl+Maj+L | Occurrence suivante / toutes les occurrences |
| Échap | Revenir à un seul curseur / fermer l’outil actif |
| F7 / Ctrl+\ | Basculer la vue côte à côte |
| Ctrl+1 / Ctrl+2 | Activer le volet gauche / droit |
| F11 / Alt+Entrée | Plein écran / fenêtre |
| Alt+F4 / bouton Quitter | Quitter avec proposition d’enregistrement |
| Clic droit dans l’explorateur | Créer / copier / coller / supprimer |
| F4 | Analyser le texte courant avec syntaxe + PyLint |
| F2 / Maj+F2 | Diagnostic suivant / précédent |
| Ctrl+Maj+M | Ouvrir le panneau Erreurs |
| F5 / Ctrl+R | Exécuter le Python courant |
| Maj+F5 | Arrêter le programme |
| Ctrl+J / Ctrl+B | Afficher / replier console / explorateur |
| F8 | Afficher / replier le menu du haut |
| F9 | Tout replier / restaurer la disposition précédente |
| Ctrl+accent grave | Focus de la console |
| Ctrl+Maj+C dans la console | Copier la sortie |
| Ctrl+plus / Ctrl+moins / Ctrl+molette | Taille du texte |
| Maj+molette | Défilement horizontal du code |
| F6 / F1 | Effets / aide |

Commande est aussi reconnu comme modificateur principal sur macOS.
Pour répondre à `input()`, cliquez dans la console ou utilisez Ctrl+accent grave, puis validez par Entrée.

## Démo de particules 100 % C

```bash
./bin/constellation
# ou, après compilation :
./build/constellation
```

Déplacez la souris pour connecter les étoiles. Cliquez pour produire des gerbes lumineuses. Échap ferme la démo.
Source : `demos/constellation.c`, compilée par le même projet CMake.

## Architecture

```text
src/
  main.c             Arguments et point d'entrée C
  app.c              Événements, documents, fichiers, commandes et exécution
  app_multicursor.c   Raccourci Ctrl+Alt+↑/↓, modificateurs/répétition et colonne visuelle
  view.c             Dessin de l'interface et effets raylib
  ui.c / ui.h        Typographie lisible, contrastes, ellipses UTF-8, icônes et contrôles
  buffer.c / .h      Modèle UTF-8, historique atomique et sauvegarde
  multicursor.c / .h Sélections multiples, édition simultanée, navigation et collage
  syntax.c / .h      Lexeur Python et complétion en C
  runner.c / .h      Processus externes POSIX / Windows, isolation de l’analyse, pipes et arrêt
  linter.c / .h      Protocole, processus d’analyse, versions, annulation et positions UTF-8
  import_paths.*     Imports d’analyse bornés, profils par projet et sauvegarde atomique
  completion.*       Service AST externe asynchrone, protocole UTF-8 et durée de vie des requêtes
  snippets.*         Bibliothèque globale, stockage atomique et expansion littérale
  app_completion.c   Suggestions natives, contexte/version/volet et insertion
  app_snippets.c     Gestion native des brouillons et de la bibliothèque commune
  app_snippet_input.c Saisie/clipboard/historique multiligne des extraits
  graphics.c / .h    Création sûre du contexte, diagnostic, option Mesa
  layout.c / .h      Géométrie des panneaux/menus/pied de fenêtre, sans GPU
  files.c / .h       Création, copie, suppression, liens, collisions et URI de fichiers
  file_job.c / .h    Opérations en arrière-plan, progression et annulation
  file_clipboard.*   Presse-papiers natif X11 / Windows / macOS
  app.h              API de lancement
  app_internal.h     État partagé entre contrôleur et vue

demos/constellation.c Démo graphique native C
tools/lint_bridge.py   Adaptateur PyLint externe, embarqué en octets C
tools/completion_bridge.py Analyse AST statique Odoo/projet, stdlib uniquement, embarquée aussi
examples/              Scripts Python d'exemple, pas l'interface
assets/fonts/          Inter et JetBrains Mono, avec licences
bin/                   Binaires Linux x86-64 fournis
cmake/                 Garde-fous, plein écran, atlas de fontes et tests du backend raylib 5.5
CMakeLists.txt         Construction C11 + raylib
```

## Tests

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/test_core
```

`test_multi` ajoute **22 tests C** dédiés aux multi-curseurs : éditions atomiques, sélection des occurrences, limite de 256 curseurs, plages chevauchantes, UTF-8, paires, indentation, copier/couper/coller, sauvegarde, historique borné et 500 scénarios aléatoires. Avec le noyau existant, **42 groupes de tests C** sont exécutés.

`test_core` exécute **20 tests C**, notamment 500 modifications aléatoires comparées à une référence, UTF-8, limites de suppression, sélection, annuler/rétablir, sauvegarde BOM/CRLF, lexeur, complétion, exécution, imports locaux, traces d'erreur, stdin et interruption.
`test_layout` vérifie **384 combinaisons de géométrie** : présence/absence des trois panneaux et de la recherche, tailles de fenêtres et de panneaux, bornes et petits écrans. Il contrôle aussi explicitement la récupération des 226 pixels de l’explorateur, des 69 pixels du menu et des 211 pixels de la console, sans ancienne réserve de 62 pixels.
La version 1.2.4 ajoute **129 660 cas de bornes et de défilement pour l’explorateur** : hauteurs de 0 à 2 160 px, menu visible/masqué, listes vides/courtes/longues et offsets extrêmes. Les lignes visibles sont toujours complètes ; la dernière entrée reste accessible après récupération de la zone décorative et redimensionnement.
La barre d’onglets ajoute **9 600 cas sans fenêtre** : 1 à 24 fichiers, chaque onglet actif, plusieurs largeurs d’explorateur et de fenêtre, avec/sans menu. Le bouton Exécuter reste aligné avec les trois points, sans recouvrir le bouton `+`, et l’onglet actif reste visible.
La version 1.3 ajoute **103 350 positions de menu et de bouton Quitter** dans `test_layout`. `test_files` vérifie **332 assertions** sur les opérations réelles : fichiers binaires, UTF-8, dossiers imbriqués, liens, collisions, périmètre du projet, annulation, nettoyage et concurrence à la création, ainsi que le worker.
`test_controls` compile les fonctions réelles du contrôleur avec les effets de fenêtre/thread simulés : **82 contrôles** de fermeture, protection des modifications, suppression, déplacement/redimensionnement/maximisation sans titre, cache et navigation des diagnostics.
`test_file_clipboard` exerce le transport X11 réel contre un propriétaire de sélection simulé : **73 assertions** sur les URI, formats GNOME, transferts INCR et indisponibilités. Aucune fenêtre d’affichage ni serveur X n’est démarré ; ce n’est pas une validation visuelle sur un bureau réel.
La version **1.5.1** porte `test_multi` à **27 groupes** : 5 groupes supplémentaires pour les séries verticales, les changements de direction, l’UTF-8 et les tabulations, les sélections existantes, les bornes et la limite de 256 curseurs, avec **196 matrices de placement**. `test_controls` appelle le vrai gestionnaire clavier natif (touches maintenues/capturées/répétées, modificateurs gauches/droits, AltGr, focus, modales, changement de volet, saisie simultanée et annulation). Ces tests sont sans fenêtre ; les raccourcis réservés par le système ne sont pas capturés artificiellement.

**Matrice 1.5.0 vérifiée sans fenêtre : 25/25 tests en Release et 25/25 en Debug ASan/UBSan, détection des fuites activée**, avec six versions réelles de PyLint (2.7.4, 2.12.2, 2.13.9, 2.17.7, 3.3.9, 4.1.3). Les nouveaux tests comptent 2 091 vérifications de bibliothèque, 178 de protocole/worker et 99 de contrôleur/saisie ; les vues réelles couvrent aussi 1 560 états de dialogue et 264 listes de suggestions. Le worker est également vérifié avec Python 3.9.25, en plus de Python 3.13.16. Les validations de bureau Windows/macOS ne sont pas revendiquées.

La version **1.5.0** ajoute quatre groupes : `test_snippets`, `test_completion`, `test_snippet_input` et `test_completion_bridge.py`. Ils couvrent la bibliothèque commune et les profils corrompus/tronqués, les 64 entrées maximales, l’UTF-8, le XML littéral, l’indentation et l’annulation, l’édition native multiligne et la sauvegarde transactionnelle, le transport des grandes sources non enregistrées, les alias/réexports Odoo, relations et héritages déclaratifs, les réponses périmées, les délais/arrêts/reprises et le nettoyage des fichiers temporaires. Aucun import de framework ni écriture du document n’est nécessaire.

Le test des fonctions de vue réelles couvre aussi les gestionnaires/confirmations, 0 à 64 extraits, les erreurs d’écriture et les listes de 0 à 32 propositions à partir de 960×640, **sans fenêtre**. Le test Python comprend **33 cas** ; les cas supplémentaires de sources Odoo réelles sont optionnels et explicitement ignorés si aucun chemin n’est fourni.

Pour inclure vos copies locales des sources réelles dans ces tests :

```bash
cmake -S . -B build-1.7.0 \
  -DPRISME_TEST_ODOO_ROOTS="/chemin/odoo-18;/chemin/odoo-19"
cmake --build build-1.7.0 --parallel
ctest --test-dir build-1.7.0 --output-on-failure
```

Le test `test_linter` de la version 1.4.2 vérifie **153 assertions** avec le **vrai PyLint 4.1.3 / Python 3.13**, et **60** pour le repli syntaxe lorsque PyLint est réellement absent. Sont contrôlés : texte non enregistré, absence d’exécution du document et des imports locaux, imports du projet reconnus, résolutions UTF-8 après un accent/tabulation, délais, annulation, fermeture pendant une analyse, résultats périmés, absence d’interpréteur et priorité des erreurs parmi plus de 256 messages.
`test_diagnostics_view` exécute les fonctions réelles de `view.c` avec les appels graphiques enregistrés, sans fenêtre : **109 877 assertions**, **192 états d’interface**, **288 volets annotés**, **144 en-têtes Ouvrir un fichier**, liste de diagnostics cliquable, soulignements rouges prioritaires, bouton Quitter et création au clic droit conservés.
La version 1.4.2 ajoute `test_import_paths` : **104 assertions** sur la limite, Unicode, noms spéciaux, doublons, chargement transactionnel, profils distincts par projet, sauvegarde atomique et profils corrompus/binaires/tronqués. Le contrôleur réel est aussi testé pour le retour Échap, la sauvegarde, l’échec d’écriture et l’invalidation des caches/propriétaires sans modification du document.
Le test réel de framework externe confirme `E0401` avant ajout, sa disparition avec le dossier configuré, le maintien d’un vrai `E0602`, le retour d’`E0401` après retrait, l’absence d’exécution et l’état explicite après disparition du dossier, avec les six versions de PyLint de la matrice.
Les fonctions réelles de vue sont exercées sur **72 configurations supplémentaires** de gestion des imports : 0 à 8 dossiers, erreur présente/absente, quatre tailles d’écran dont 960×640, liste/paramètres/sélecteur et boutons de retrait.
Sous Linux, **CTest : 21/21** dans la matrice configurée avec cinq interpréteurs de compatibilité supplémentaires (**16 tests** par défaut), en Release et en Debug sous AddressSanitizer + UndefinedBehaviorSanitizer avec détection des fuites. Les sous-processus Python ne sont pas instrumentés par les sanitizers C.

Pour exiger le vrai PyLint dans les tests, plutôt que seulement le repli syntaxe :

```bash
cmake -S . -B build-1.7.0 -DPRISME_TEST_PYTHON="$PWD/.venv/bin/python" -DPRISME_REQUIRE_PYLINT_TESTS=ON
cmake --build build-1.7.0 --parallel
ctest --test-dir build-1.7.0 --output-on-failure
```

Sans cette option, le test indique explicitement si PyLint manque. `PRISME_TEST_FALLBACK_PYTHON` permet de choisir un interpréteur sans PyLint pour vérifier ce cas séparément.

`PRISME_TEST_COMPAT_PYTHONS` accepte une liste d’interpréteurs séparés par des points-virgules : chaque environnement ajoute un test complet exigeant le vrai PyLint. La matrice 1.4.2 vérifie les versions 2.7.4, 2.12.2, 2.13.9 et 2.17.7 sous Python 3.9.25, puis 3.3.9 et 4.1.3 sous Python 3.13.16. Ces environnements de test ne sont pas distribués. `test_lint_bridge.py` ajoute **12 régressions sans fenêtre** sur les arguments compatibles et les erreurs de démarrage ; ses doubles ne remplacent pas les tests d’intégration réels.

`test_fullscreen` compile les fonctions réelles `ToggleFullscreen`, `WindowSizeCallback`, `SetupViewport` et les accesseurs de dimensions extraits du backend corrigé. Seuls les appels GLFW/rlgl sont simulés : aucun affichage n’est ouvert. Il vérifie **1 536 configurations et 4 608 cycles plein écran / restauration**, sur deux moniteurs simulés, 12 résolutions (dont portrait et ultra-large), fenêtres classiques/maximisées, marges de bureau et événements de taille immédiats ou retardés. Les indisponibilités de moniteur sont également couvertes.
Sous Linux, `test_fullscreen_retina` reprend les **mêmes 1 536 configurations et 4 608 cycles** avec la branche Retina du viewport et des facteurs DPI de 100 %, 125 %, 150 % et 200 %. Les binaires Windows/macOS et le rendu visuel réel de cette version ne sont pas validés dans cet environnement ; ces contrôles sont volontairement sans aperçu.

Python 3 est nécessaire aux tests d’exécution et d’analyse. Ces tests ne demandent pas d’affichage graphique.
Sous Linux, CTest vérifie aussi que l'éditeur et la démo renvoient proprement 3 avec un affichage invalide, sans segfault.
Lors des premiers correctifs, la régression GLX a été vérifiée séparément : le binaire 2.1 démarre avec Mesa limité à 2.1, et le binaire 3.3 échoue proprement dans ces mêmes conditions.

Vérifications mémoire avec GCC/Clang :

```bash
cmake -S . -B build-asan -DPRISME_SANITIZERS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-asan --parallel
ctest --test-dir build-asan --output-on-failure
```

La version corrigée a repassé les tests du noyau et les deux échecs d'affichage contrôlés sous AddressSanitizer et UndefinedBehaviorSanitizer.
Dans les versions 1.1/1.2.0, le démarrage en vue fractionnée + plein écran sous OpenGL 2.1 a aussi été vérifié sous instrumentation, avec la détection des fuites du pilote désactivée pour le rendu.
Les validations graphiques des versions 1.1/1.2.0 couvraient : édition, collage UTF-8, sauvegarde, exécution, recherche, menus, complétion, historique, édition multi-curseurs, édition/sauvegarde de deux fichiers, changement de volet, fermeture d'onglet et redimensionnement du séparateur.
Le test de plein écran natif de ces versions a vérifié F11, la restauration de la taille, Alt+Entrée et Échap.
La version 1.2 a également vérifié les huit combinaisons des trois panneaux, les flèches de repli, la réouverture par la barre d’état, les raccourcis, les deux redimensionnements, l’édition et la recherche sans menu, le retour du focus après masquage de la console, l’accès aux paramètres/aide et la restauration F9. Sept parcours natifs supplémentaires contrôlent migration, persistance, dimensions et mode de secours.

La version 1.2.2 ajoute `test_ui`, qui vérifie les tailles minimales, les métriques des polices réelles en mémoire, la migration des tailles de police, 4 961 ajustements UTF-8 et un contraste minimal mesuré de **5,64:1** pour les textes sur les fonds utilisés. Les rectangles et informations de pied de fenêtre sont vérifiés sans fenêtre graphique.
La géométrie vérifie aussi **4 944 états de redimensionnement**, avec l’égalité `status_y + status_h == hauteur de fenêtre` : aucun pixel de marge n’est réservé sous la barre d’état.
Ces tests, comme les 384 configurations de panneaux et 9 600 dispositions d’onglets, sont exécutables sans aperçu ni serveur X. Le dossier `preview` n’est pas nécessaire à l’application et a été supprimé du workspace.

## Limites et sécurité

- **L'exécution Python n'est pas une sandbox.** Le script a les permissions de votre utilisateur. N'exécutez que du code de confiance.
- Les opérations de fichiers acceptent jusqu’à **64 éléments de premier niveau** par collage (presse-papiers limité à 512 Ko) et 128 niveaux de récursion. Le contenu binaire peut être copié même s’il ne peut pas être édité.
- **Les suppressions sont définitives après confirmation**, sans corbeille. Une opération de fichiers se termine, ou une copie s’annule, avant de fermer l’éditeur.
- Les documents doivent être en **UTF-8** ; les fichiers binaires et les autres encodages sont refusés.
- Les multi-curseurs sont limités à **256 par document**. La sélection des occurrences est sensible à la casse ; un avertissement apparaît si la limite est atteinte.
- Cette version vise les scripts et petits projets : limite de 2 Mo par fichier, palette limitée à 400 fichiers, recherche affichant au plus 2 048 occurrences et console limitée à 2 000 lignes.
- La recherche ignore la casse ASCII ; les accents doivent être saisis avec leur casse exacte.
- La coloration reste un lexeur C ; la syntaxe et PyLint sont analysés séparément, en arrière-plan, après une pause. PyLint ne garantit pas l’absence d’erreurs d’exécution ni une vérification de types exhaustive.
- La complétion réunit les mots du document, les macros globales et une analyse statique des sources Odoo/projet. Elle ne reconstruit pas le registre ORM ni les champs dynamiques ; ce n’est pas un serveur LSP. Voir [le guide Odoo/macros](GUIDE-ODOO-MACROS.md).
- Pas de points d'arrêt, débogueur, terminal shell/ANSI, Git intégré ni remplacement global multi-fichiers. Les multi-curseurs agissent dans un document à la fois.
- La police couvre le français et l'alphabet latin étendu ; le rendu de tous les alphabets et emoji n'est pas garanti.
- Les champs de dialogue habituels sont à une seule ligne ; l’éditeur de code des macros est multiligne (16 Kio). La console accepte des réponses d’au plus 4 Ko.
- Windows et macOS sont prévus dans le code mais n'ont pas été validés dans cet environnement. La fermeture des descendants sous Windows utilise un Job Object quand l'OS permet l'association ; sinon seul le processus direct est garanti.

## Licences

Code Prisme : MIT (`LICENSE`).
Raylib : licence zlib/libpng (`third_party/raylib-LICENSE.txt`) ; les modifications locales sont décrites dans `third_party/RAYLIB-PATCH.md`.
Inter et JetBrains Mono : SIL Open Font License (`assets/fonts/*-OFL.txt`).
Le binaire inclut raylib et ses dépendances intégrées ; les notices correspondantes sont regroupées dans `third_party/raylib-NOTICES.txt`.
