# Python Studio 1.8.0 — état du projet et brouillons

Cette fonction est intégrée dans les **deux véritables éditions**, Python et C. Leurs sessions sont indépendantes ; les profils SSH restent partagés au format existant.

## Ce qui revient au prochain lancement

- Les documents ouverts, leur ordre et l’onglet actif.
- Les curseurs, sélections et multicurseurs, ainsi que le défilement de chaque document.
- La vue fractionnée, ses documents, le volet actif et les proportions des panneaux.
- La visibilité de l’explorateur, du menu supérieur et de la console, la taille du texte et certains réglages de disposition.
- Les dossiers développés dans l’explorateur et son défilement.
- **Le contenu non enregistré**, y compris les documents sans nom.

Les piles d’annulation, processus F5, saisies de console, résultats PyLint, recherches, presse-papiers, dialogues et saisies d’authentification ne sont pas restaurés. Les dimensions et la position de la fenêtre sur le bureau ne font pas partie de la session.

L’état est vérifié environ **toutes les deux secondes** et enregistré s’il a changé. Une capture finale accompagne la fermeture normale. Un arrêt brutal peut donc perdre les toutes dernières secondes ; ce mécanisme n’est pas une garantie contre une panne matérielle ou une erreur de disque.

Sans argument de projet/fichier, Python Studio reprend le dernier projet local. `--project DOSSIER` choisit explicitement le projet et restaure sa propre session. Un fichier passé sur la ligne de commande est ouvert/sélectionné après la restauration. Le changement explicite de projet local préserve d’abord la session précédente, puis charge celle du nouveau projet.

## Quitter avec des modifications

La confirmation propose toujours d’enregistrer, d’abandonner les modifications ou d’annuler. Elle offre en plus **Conserver les brouillons et quitter** : rien n’est écrit dans les fichiers du projet, mais le contenu de travail est conservé dans la session locale.

- Un abandon explicitement confirmé pour la fermeture ne ressuscite pas les modifications abandonnées au lancement suivant.
- Annuler la revue de fermeture conserve les documents et leurs brouillons.
- Si l’écriture de session échoue, cette fermeture conservant les brouillons est refusée avec un message ; utilisez Enregistrer/Enregistrer sous.
- Un échec de déconnexion SSH révoque la fermeture et remet les brouillons de travail dans la session. Pendant le démontage engagé, les entrées d’édition sont suspendues afin de ne pas modifier les documents après la capture finale.

Fermer volontairement un onglet, puis confirmer l’abandon de son contenu, le retire de la session courante : les journaux ne constituent pas un historique de versions.

## Si un fichier a changé ailleurs

La restauration ne sauvegarde **jamais** automatiquement un brouillon dans son fichier d’origine.

- Un document propre est relu depuis le disque pour afficher la version actuelle.
- Pour un document modifié, la version enregistrée à l’époque est comparée au disque (texte et caractéristiques de format).
- Si l’original a changé, a disparu, est illisible ou n’est pas un fichier régulier, le contenu conservé revient comme **document récupéré sans destination d’enregistrement**. Un message l’indique. Utilisez **Enregistrer sous** et comparez les versions ; aucun original n’est écrasé par la restauration.

Ce n’est pas une fusion automatique ni une protection contre toutes les modifications concurrentes survenant après cette vérification. Les protections habituelles de sauvegarde restent nécessaires.

## Reprise SSH : hors ligne par défaut

Une session SSH est restaurée **sans connexion, authentification ni lecture des fichiers distants**. Les brouillons viennent du journal local et le profil non secret est disponible pour une reconnexion explicite.

Les documents distants restent en attente de vérification. L’ancien dossier de montage n’est jamais considéré comme un dossier local d’enregistrement. Après une reconnexion explicite, la version distante est vérifiée avant de réautoriser l’enregistrement sur place. Un conflit ou une lecture impossible produit un document récupéré à enregistrer séparément.

La session ne confère **aucune propriété d’un montage** et ne permet jamais de démonter le montage d’une autre fenêtre. Changer d’atelier local depuis un contexte SSH encore actif peut nécessiter sa déconnexion préalable.

**Aucun mot de passe SSH, phrase secrète, jeton askpass, contenu de clé privée ou socket d’authentification n’est sérialisé.** Le mot de passe/secret d’une future reconnexion doit être fourni de nouveau. Les scripts que vous éditez peuvent naturellement contenir des informations sensibles : leur texte est une donnée de brouillon et est donc conservé si les sessions sont activées.

F5, PyLint, la complétion et les dépendances Python restent **locaux**, comme auparavant. Ce changement n’ajoute pas d’exécution Python distante.

## Emplacement et confidentialité

Les noms de configuration historiques restent volontairement inchangés afin de préserver les préférences et profils existants :

| Édition | Sessions sous Linux, par défaut |
|---|---|
| Python | `~/.config/prisme/sessions-python/` |
| C | `~/.config/prisme-c/sessions-c/` |

Avec `PRISME_CONFIG_DIR=/chemin/absolu`, les sous-dossiers deviennent respectivement `sessions-python/` et `sessions-c/` dans ce dossier. Le fichier partagé `ssh-profiles.json` reste dans ce même dossier de configuration lorsqu’il est explicitement choisi.

Les brouillons sont des **fichiers locaux en clair, non chiffrés**. Sous Linux/POSIX, le dossier des sessions est privé (0700) et les fichiers créés sont privés (0600). Les remplacements utilisent un fichier temporaire puis une publication atomique, avec synchronisation sur disque. Une erreur de synchronisation peut néanmoins laisser la durabilité incertaine et est signalée.

Le journal Python est du JSON borné ; le journal C utilise un format binaire UTF-8 borné avec somme de contrôle. Ils ne sont **pas interchangeables**. Les chemins des projets sont utilisés comme identifiants : déplacer ou renommer le dossier d’un projet ne migre pas automatiquement sa session.

Pour deux fenêtres du même projet, un verrou évite qu’elles écrasent le même journal ; la seconde utilise un fichier distinct. Au prochain lancement, la session la plus récente est choisie, **sans fusion des deux fenêtres**. Les autres journaux et les fichiers corrompus sont préservés, pas effacés automatiquement. Les limites sont de 64 documents/128 Mio de journal pour Python et 24 documents/104 Mio pour C ; un dépassement est signalé, jamais tronqué silencieusement.

Les sessions ont été validées sans affichage sous Linux. Le fonctionnement physique du bureau, Windows et macOS n’a pas été validé pour cette livraison.

## Démarrage sans session

```bash
# Édition Python
python3 -B main.py --no-session --project /chemin/projet

# Édition C
./bin/python-studio --no-session --project /chemin/projet
```

`--no-session` empêche la lecture et l’écriture des sessions/brouillons pour ce lancement, sans supprimer les anciens journaux. Le bouton de fermeture conservant les brouillons n’est alors pas proposé. Les préférences ordinaires et profils SSH gardent leur comportement existant.

Pour effacer définitivement les sessions, fermez d’abord toutes les fenêtres, vérifiez le dossier de configuration réellement utilisé, puis supprimez **uniquement** `sessions-python/` ou `sessions-c/`. Cette opération détruit les brouillons qui n’existent pas ailleurs ; elle ne doit jamais viser le cache de montages SSHFS.

## Sélecteur XML (1.9)

Le sélecteur d’identifiants, son filtre et l’instantané XML source restent transitoires en mémoire : ils ne sont pas restaurés ni sérialisés dans le journal. Le chemin tapé et, après sélection, le bloc inséré font partie du document et suivent la politique habituelle des brouillons **privés en clair**. `--no-session` reste disponible. Voir [GUIDE-XML.md](GUIDE-XML.md).
