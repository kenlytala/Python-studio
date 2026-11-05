# Dossiers distants et profils — Python Studio C 1.8.0

## Ce qui est distant, ce qui reste local

**Les fichiers sont sur le serveur ; l’éditeur, Python, PyLint, la complétion et les dépendances restent sur votre machine.** Cette fonction n’installe pas d’agent Python Studio sur le serveur et ne crée pas d’environnement Python distant. Le seul service distant nécessaire est SSH avec son sous-système SFTP.

Python Studio utilise les programmes système **OpenSSH + SSHFS/FUSE**. L’intégration cible **Linux** ; elle n’est pas proposée sur Windows/macOS dans cette version. L’édition locale reste disponible indépendamment de SSHFS. Le binaire C précompilé n’utilise pas le backend portable de l’édition Python : voir les exigences de libc/libm dans README.md.

## Préparer Linux

Sur Debian/Ubuntu, par exemple :

```bash
sudo apt install sshfs openssh-client
```

FUSE doit être disponible et votre utilisateur doit pouvoir accéder à `/dev/fuse`. Le serveur doit accepter votre compte SSH et autoriser SFTP ainsi que les opérations de fichiers demandées. Aucun paquet Python supplémentaire n’est nécessaire pour utiliser SSH dans Python Studio.

**Protection obligatoire : votre SSHFS doit reconnaître `contain_symlinks`.** Vérifiez avec `sshfs -h`. Certaines distributions ont ajouté cette protection à une version 3.7.3 corrigée : le numéro de version seul ne suffit pas. Si elle manque, mettez SSHFS à jour depuis votre distribution. Python Studio refuse la connexion plutôt que de retirer cette protection. N’activez pas `follow_symlinks` ou `no_contain_symlinks` pour contourner ce refus.

## Plusieurs connexions mémorisées, sans secrets

Dans le formulaire SSH :

1. **Profils (N)** ouvre la liste ; cliquez sur une ligne, ou utilisez ↑/↓ puis Entrée. La sélection remplit les cinq champs, **sans se connecter automatiquement**. Le chemin de clé est indiqué pour distinguer deux configurations proches.
2. **Nouvelle** crée une nouvelle configuration. Pour modifier une configuration existante, choisissez-la dans **Profils**, modifiez ses champs puis enregistrez. Son identifiant reste stable, sans ajout de doublon.
3. **Enregistrer le profil** / **Ctrl+S** conserve les paramètres, sans connexion. **Se connecter** les enregistre également **avant** d’essayer le transport. Un échec n’efface donc plus la configuration.
4. **Retirer la sélection** / **Suppr** retire seulement le profil de la liste. Cette action ne supprime aucun fichier distant, aucune clé privée et aucune entrée `known_hosts`.

Une seule session SSH est active par fenêtre ; les profils sont des connexions mémorisées, pas des montages simultanés.

La liste peut contenir **32 profils**. `~/.config/prisme/ssh-profiles.json` est partagé avec **Python Studio Python 1.8.0**. Si `PRISME_CONFIG_DIR` est défini, les deux éditions utilisent plutôt ce dossier (un chemin absolu est recommandé). Ce partage ne fusionne pas les autres préférences des deux éditions.

Seuls **hôte, utilisateur, port, dossier et chemin de clé**, plus un identifiant aléatoire et la sélection courante, sont écrits. Le fichier et son verrou sont privés (0600), les mises à jour atomiques et sérialisées par un verrou OS. Un fichier nouveau mais illisible/corrompu n’est jamais écrasé silencieusement : corrigez-le ou déplacez-le explicitement avant de réessayer.

L’ancien profil unique `ssh.json` est importé automatiquement si le nouveau fichier n’existe pas ; l’ancien fichier est conservé. Une liste volontairement vidée ne le réimporte pas. Une clé temporairement absente peut rester référencée ; la connexion exigera qu’elle soit disponible.

**Aucun mot de passe ni passphrase n’est sauvegardé.** OpenSSH pourra les redemander à chaque connexion ; pour éviter ces demandes sans enregistrer le secret dans Python Studio, utilisez votre `ssh-agent`. Les réponses ne sont placées ni dans les arguments des programmes, ni dans leur environnement, ni dans des fichiers. Le canal local d’authentification est un socket Unix dans un dossier privé temporaire.

## Compatibilité SSHFS — champs Utilisateur et Port

L’erreur `fuse: unknown option(s): '-o User=odoo'`, corrigée dans l’édition Python 1.6.1, est évitée ici également : l’utilisateur et le port renseignés sont transmis directement au client OpenSSH (`-l` / `-p`), pas comme des options FUSE. Vous pouvez conserver `odoo` dans le champ **Utilisateur**, ainsi que votre port. Aucun changement de compte serveur n’est nécessaire pour corriger cette erreur. Les champs vides continuent d’utiliser `~/.ssh/config`.

## Se connecter

1. Lancez le précompilé avec `./bin/python-studio`, ou reconstruisez localement avec `./lancer.sh`. Gardez le binaire natif `python-studio-ssh-helper` à côté de `python-studio` : il sert à askpass et à la vérification asynchrone du dossier, sans Python.
2. Cliquez sur **SSH** dans la barre d’état, ou cherchez « SSH » dans la palette **Ctrl+K**.
3. Renseignez :

   | Champ | Exemple / comportement |
   |---|---|
   | Hôte ou alias | `mon-serveur`, `example.org`, `192.0.2.10`, ou une adresse IPv6 sans crochets |
   | Utilisateur | `alice` ; vide pour utiliser votre configuration SSH |
   | Port | `22` ; vide pour utiliser votre configuration SSH |
   | Dossier distant | `/srv/odoo/custom-addons`, ou `.` pour le dossier personnel du compte |
   | Clé privée locale | `/home/alice/.ssh/id_ed25519` ; facultatif, vide pour l’agent et les clés/configurations habituelles |

4. Cliquez sur **Connecter**. La connexion et les demandes d’authentification sont traitées sans démarrer de serveur web ou d’aperçu.
5. Pour un serveur inconnu, **comparez l’empreinte SHA256 par un canal de confiance** avec son administrateur. N’acceptez que si elle correspond. Le bouton d’acceptation est explicite ; **Ctrl+Entrée** fonctionne également, pas Entrée seule ni sa répétition.
6. Saisissez le mot de passe ou la phrase secrète quand OpenSSH le demande. Les caractères sont masqués et les commandes de copie/coupe sont désactivées dans ce champ. Échap annule l’authentification.

Les alias de `~/.ssh/config`, les ports, clés par défaut et `ssh-agent` sont utilisés par OpenSSH. Un chemin de clé contenant une virgule n’est pas accepté par cette intégration SSHFS ; utilisez un alias SSH pour ce cas. L’authentification dépend aussi des méthodes autorisées par votre client et le serveur.

Une clé serveur **modifiée** provoque un refus : Python Studio ne remplace pas silencieusement l’entrée `known_hosts`. Contactez l’administrateur et vérifiez la nouvelle clé avant toute modification manuelle de votre configuration. Si aucune empreinte SHA256 n’est disponible pour une nouvelle clé, Python Studio refuse l’approbation.

## Éditer et gérer les fichiers

Après connexion, l’explorateur affiche **SSH · utilisateur@hôte:dossier**. Ouvrez les fichiers et enregistrez avec **Ctrl+S**, comme dans un projet local. La sauvegarde garde les protections existantes : encodage Python, BOM, CRLF, fichier temporaire puis remplacement atomique lorsque le serveur le permet.

Le clic droit conserve **créer fichier/dossier**, **copier**, **coller** et **supprimer**. Une copie peut aller du dossier distant vers un dossier local et inversement tant que le montage est actif. Il n’existe pas de synchronisation bidirectionnelle ou de miroir hors ligne.

**Liens symboliques distants :** ils ne sont ni ouverts, ni parcourus, ni copiés comme sources par Python Studio. Utilisez le chemin réel sur le serveur. Vous pouvez supprimer **le lien lui-même** ; sa cible n’est jamais supprimée par cette action. La suppression d’un vrai dossier ne descend pas dans les cibles de ses liens. Une copie de dossier contenant un lien distant peut donc être refusée plutôt que suivre ce lien. SSHFS bloque en plus les liens absolus ou contenant `..`, pour éviter qu’un lien fourni par le serveur ne désigne un fichier de votre machine.

Certains serveurs SFTP ne prennent pas en charge les liens durs, `fsync` ou le renommage POSIX nécessaires à certaines copies/sauvegardes. L’opération peut alors échouer explicitement : Python Studio ne remplace pas ces protections par une publication écrasant un fichier existant ou une sauvegarde non atomique.

## F5, PyLint et dépendances

**F5 utilise toujours le Python local sélectionné**, éventuellement avec le dossier distant monté comme répertoire de travail. Les imports du projet peuvent être lus via SSHFS, mais les bibliothèques doivent être installées dans cet environnement **local**. Les ressources propres au serveur, ses services, sa base de données et ses chemins absolus ne deviennent pas automatiquement disponibles localement.

PyLint et l’index statique Odoo restent également locaux. Les huit dossiers d’import par projet et les macros globales restent disponibles. Aucun `pip install`, PyLint ou processus Python n’est lancé sur le serveur par cette fonction.

**F5 n’est pas une sandbox.** Le programme exécuté possède les permissions de votre utilisateur local, y compris l’accès aux dossiers montés. N’exécutez que du code de confiance.

## Coupure, reconnexion et fermeture

- Le bouton SSH indique une connexion active, une opération en cours ou **SSH !** après une erreur/perte. Ouvrez le dialogue pour le détail.
- À la perte du transport, les documents déjà ouverts et leurs modifications restent **en mémoire**. Le montage absent, remplacé, superposé ou dont le processus est mort n’est pas une destination d’enregistrement autorisée.
- Utilisez **Reconnecter** pour fermer normalement l’ancienne session et rétablir le même dossier. Les onglets et les brouillons sont conservés, y compris si cette tentative échoue. Une nouvelle demande de mot de passe/phrase secrète peut apparaître ; il n’y a pas de reconnexion automatique cachée.
- **Enregistrer sous** propose un dossier local si la session distante est inutilisable. C’est la solution de secours pour préserver un brouillon. La session 1.8.0 conserve désormais les brouillons localement : voir [GUIDE-SESSIONS.md](GUIDE-SESSIONS.md). À la reprise, la reconnexion est explicite et les fichiers distants sont vérifiés avant tout enregistrement sur place.
- Pour changer de serveur/dossier, fermez les onglets de la connexion précédente après avoir préservé les modifications.
- **Déconnecter** est refusé tant que des documents distants sont modifiés, qu’une opération de fichiers est en cours ou que F5 tourne. Enregistrez, fermez les onglets après confirmation, ou arrêtez F5, puis réessayez.
- **Quitter** demande confirmation pour les modifications puis essaie un démontage normal. Un montage occupé n’est ni démonté de force, ni démonté paresseusement, ni détruit par `rmtree`. Aucun processus SSHFS encore monté n’est tué pour forcer la déconnexion normale.

Après une fermeture forcée de l’application ou un incident système, un montage peut rester présent. Fermez les terminaux/fichiers qui l’utilisent, vérifiez le chemin et démontez-le explicitement :

```bash
fusermount3 -u -- "/chemin/exact/du/montage"
```

Le chemin est sous `${XDG_CACHE_HOME:-$HOME/.cache}/prisme/sshfs-c/`, avec un identifiant de profil. Ne supprimez jamais ce dossier récursivement tant qu’un montage y demeure. Python Studio refuse de masquer un dossier local non vide ou de réutiliser le montage d’une autre instance. Un verrou système par profil empêche deux fenêtres de démarrer le même montage simultanément ; son petit fichier `.lock` vide peut rester dans ce cache après déconnexion.

## Données enregistrées et limites

- Les clés serveur sont gérées par OpenSSH dans ses fichiers `known_hosts` habituels. La clé privée reste dans le fichier que vous avez choisi ; Python Studio n’en crée pas de copie.
- Aucun mot de passe/phrase secrète n’est enregistré, ajouté aux arguments de processus ou aux variables d’environnement. Les réponses traversent un socket Unix privé à jeton, le helper natif C et l’entrée askpass d’OpenSSH. Elles sont éphémères en mémoire, sans garantie d’effacement physique de toutes les copies système (le champ, ses historiques et les buffers de réponse C sont effacés après usage).
- La configuration OpenSSH doit être de confiance : ses éventuels `ProxyCommand`, `ProxyJump` ou `Match exec` peuvent déclencher des programmes **locaux**. Le transfert d’agent, X11, les redirections et le partage d’un ancien ControlMaster sont désactivés pour cette session.
- La connexion et le démontage sont suivis par une machine à états non bloquante ; la vérification du dossier utilise un processus auxiliaire et les copies/suppressions un worker. **Toutes les lectures/écritures réseau ne sont pas asynchrones** : ouvrir, sauvegarder ou rafraîchir un arbre peut attendre un serveur lent. Les keepalives n’imposent pas un délai absolu à chaque requête SFTP.
- Les éditions concurrentes par un autre programme/utilisateur ne disposent pas d’un protocole de fusion ou de verrouillage distribué. Une sauvegarde peut remplacer une modification distante intervenue entre-temps. Vérifiez les conflits avec votre gestionnaire de versions.
- La validation fournie utilise un véritable protocole SSH chiffré, OpenSSH, SSHFS/FUSE et SFTP OpenSSH sur un serveur éphémère local via socket Unix. Aucun serveur externe personnel, environnement réseau réel, bureau physique GNOME ou mécanisme matériel FIDO n’est présenté comme testé.

## Chemins XML (1.9)

Ctrl+Espace peut lire un XML d’un dossier SSH **déjà connecté explicitement** et insérer son élément dans le document. La source reste en lecture seule ; aucune authentification automatique ou exécution distante n’est ajoutée. Une génération remplacée ou un montage hors ligne bloque la lecture, y compris pour les alias locaux vers le cache réservé. L’index est transitoire ; le texte choisi devient un brouillon normal, éventuellement journalisé en clair. Annuler ne rejoint pas une lecture réseau dans le thread de l’interface ; voir les limites d’arrêt dans [GUIDE-XML.md](GUIDE-XML.md).
