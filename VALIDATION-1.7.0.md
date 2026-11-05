# Validation — Prisme C 1.7.0

Date : **1er novembre 2026**. Intégration SSH **native C** et profils multiples partagés avec l’édition Python. Aucun aperçu, capture, serveur d’affichage ou service TCP n’a été créé.

## Fonction livrée

- Éditeur et helper SSH compilés en C : `bin/prisme` et `bin/prisme-ssh-helper`. Ce n’est pas un lanceur de l’édition Python ; OpenSSH et SSHFS sont les outils système de transport.
- Jusqu’à **32 profils** : création, liste/sélection, modification, enregistrement et retrait. Hôte, utilisateur, port, dossier et chemin de clé sont conservés **avant** la connexion, y compris après un échec. Sélectionner ne connecte pas automatiquement.
- Format JSON commun, identifiants stables, migration de l’ancien `ssh.json`, verrou OS partagé, écriture atomique et permissions 0600. Les données corrompues ne sont pas écrasées silencieusement.
- **Aucun mot de passe ni passphrase enregistré.** Authentification mot de passe, clé chiffrée et agent ; champ masqué, pas de copie vers le presse-papiers ni d’historique de secrets réutilisable. Vérification explicite des clés serveur inconnues ; clés modifiées refusées.
- Explorateur, ouverture/édition/sauvegarde, opérations de fichiers, annulation, déconnexion et reconnexion intégrés au contrôleur C. Brouillons conservés en RAM après coupure ; sauvegardes vers un montage disparu/remplacé/superposé refusées, y compris via un alias local.
- **Python, PyLint, l’index et les dépendances restent locaux.** L’éditeur C n’exécute pas de Python sur le serveur.

## Résultats sur les sources de production

| Vérification | Résultat |
|---|---:|
| CTest complet, **Release**, `build-1.7.0` | **22/22**, 0 échec, **11,06 s** |
| Intégration SSH chiffrée dans ce CTest | **10/10 cas réels** |
| ASan + UBSan, avec détection de fuites | **5/5 groupes ciblés**, 0 échec, **10,65 s** |
| Intégration SSH chiffrée sous ASan/UBSan | **10/10 cas réels** |
| Interopérabilité du store avec Python | **5/5 tests**, dont **16 écritures concurrentes** |

Les cinq groupes instrumentés couvrent les opérations de fichiers, le store/les commandes/les gardes SSH, les dix connexions réelles, les vues masquées et les vrais contrôleurs de l’éditeur. Le CTest complet conserve aussi multi-curseurs/annulation, dispositions et typographie CPU, complétion/macros Odoo statiques, PyLint réel, syntaxes, presse-papiers X11 simulé et échecs d’initialisation sans affichage.

Les raccourcis **Ctrl+Maj+J/K** permettent également l’ajout vertical de curseurs sans dépendre des flèches réservées par GNOME. Les tests de routage et d’annulation utilisent le vrai contrôleur, pas un bureau physique.

**Compilation :** aucun diagnostic dans les sources Prisme. La compilation fraîche émet **4 avertissements GCC `-Wstringop-overflow` dans `jar_mod.h`**, dépendance audio de raylib 5.5 non utilisée par l’éditeur. Ils ne sont ni supprimés ni présentés comme résolus. La bibliothèque graphique C garde les trois correctifs existants.

## Transport réellement vérifié

Le serveur de QA est éphémère : SSH chiffré Paramiko sur **AF_UNIX**, vrai client OpenSSH, vrais SSHFS/FUSE et SFTP OpenSSH. Pas de port TCP, compte système ajouté ou serveur personnel contacté. Paramiko n’est pas une dépendance de l’application.

Le driver invoque le contrôleur/transport C de production ; les effets de fenêtre/projet/index sont remplacés pour tourner sans affichage. Les contrôleurs de formulaire/authentification et les vues sont testés séparément avec les appels graphiques simulés. Ce n’est pas une validation visuelle complète.

Les dix cas couvrent mot de passe et CRUD UTF-8, clé chiffrée avec espaces, agent réel, refus/acceptation de clé inconnue, clé modifiée, mauvais secret, annulation, démontage occupé, coupure/reconnexion et préservation des brouillons. Les tests refusent les identifiants secrets dans les sorties, arguments/environnements de processus et profils persistés.

**Régression `User=odoo` :** l’utilisateur explicite est transmis à OpenSSH par **`-l odoo`**, le port par **`-p 2222`**, pas comme `User=` / `Port=` externes à FUSE. La fixture vérifie réellement le compte reçu et le port effectif malgré ses défauts `test` / `22`. Les champs vides n’ajoutent pas ces drapeaux et laissent la configuration SSH s’appliquer.

## Intégrité et limites

Empreinte des sources configurées par CMake :

```text
2b3d309da9a7400ab39f6c104e714b5d3c0ee2264cf13d3a2895cc8abe6bc2ce
```

Les exécutables de `bin/` sont remplacés par cette production, dépouillés des symboles superflus et exécutables. **Un seul dossier de build courant** demeure : `build-1.7.0`. L’archive historique C 1.5.1 reste inchangée. Le paquet ne fournit pas de cache CMake lié à la machine de QA ; `./lancer.sh` régénère la configuration pour votre chemin.

Le binaire C x86-64 demande des symboles jusqu’à **GLIBC_2.38** (helper : **GLIBC_2.34**) ; ce n’est pas la portabilité glibc 2.17 du backend Python. Recompilez sur une distribution plus ancienne. La compilation et les tests CPU ne prouvent pas le fonctionnement matériel GPU/fullscreen/GNOME, Windows/macOS, un vrai réseau externe, FIDO ou un registre ORM/base Odoo.

Les lectures/sauvegardes réseau ne sont pas toutes asynchrones. Pas de journal de crash, fusion distribuée ou sandbox ; code/config SSH locaux de confiance nécessaires. Les liens distants ne sont pas suivis et `contain_symlinks` est obligatoire. Un démontage occupé ne déclenche ni force/lazy unmount ni suppression récursive d’un cache monté.

## Acceptation de l’archive extraite

Le candidat a été extrait dans un **dossier neuf** : **136 fichiers**, dont **135 charges utiles** vérifiées par SHA256, taille et mode. Les binaires et le lanceur conservent leurs bits exécutables. Un seul build est distribué ; les caches/objets CMake liés au chemin de QA et les captures historiques sont exclus.

Après simple configuration des chemins de QA, **avant toute recompilation**, les exécutables fournis passent **22/22 CTests en 10,45 s**, dont les **10 cas SSH/SFTP réels**. Puis `./lancer.sh --version` reconstruit effectivement les sources et affiche **Prisme C 1.7.0** ; le CTest complet des nouveaux exécutables passe encore **22/22 en 9,98 s**. L’empreinte configurée reste identique. Les quatre avertissements audio tiers restent les seuls diagnostics.

Après ces parcours, seuls la documentation, le résumé JSON et le manifeste sont finalisés ; toutes les autres charges utiles, dont les binaires originaux, sont comparées au manifeste du candidat testé. L’archive est régénérée de façon déterministe et possède son fichier `.tar.gz.sha256`.

## Reproduire

Dans un environnement Linux disposant des prérequis du [guide SSH](GUIDE-SSH.md), d’un Python de QA avec PyLint et Paramiko, et du sous-système OpenSSH `sftp-server` :

```bash
cmake -S . -B build-1.7.0 -DCMAKE_BUILD_TYPE=Release \
  -DPRISME_TEST_PYTHON=/chemin/qa/bin/python \
  -DPRISME_TEST_FALLBACK_PYTHON=/chemin/qa/bin/python \
  -DPRISME_REQUIRE_PYLINT_TESTS=ON -DPRISME_SSH_INTEGRATION=ON
cmake --build build-1.7.0 --parallel
ctest --test-dir build-1.7.0 --output-on-failure
./bin/prisme --version
```

Les cinq tests d’interopérabilité se lancent depuis l’édition Python avec `PRISME_NATIVE_PROFILES=/chemin/prisme-c/build-1.7.0/test_ssh` ; voir son rapport de validation. Les archives séparées contiennent leur manifeste SHA256 ; extrayez dans un dossier neuf.
