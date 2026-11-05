# Python Studio 1.5.1 — complétion Odoo et macros de code

L’éditeur et ses fenêtres restent **en C11 avec raylib**. L’analyse des sources utilise un petit processus Python externe, isolé et en lecture seule. Aucun serveur Odoo ni base de données n’est lancé.

## 1. Relier les sources d’Odoo

1. Ouvrez **Paramètres → Imports Python / complétion → Gérer**.
2. Choisissez **Ajouter un dossier**, puis le dossier **contenant `odoo/`**.
   - Exemple : `/opt/odoo-18/odoo/fields.py` → ajoutez **`/opt/odoo-18`**.
   - Pour une installation dans un venv, choisissez le `site-packages` qui contient les sources `odoo`.
3. Ajoutez éventuellement les dossiers de vos modules personnalisés pour leur index statique.
4. Revenez dans un fichier Python et commencez à saisir. **Ctrl+Espace** demande immédiatement des propositions.

Les huit dossiers maximum sont les **mêmes que pour PyLint**, enregistrés **par projet**. Les macros, elles, sont **communes à tous les projets**. Ces chemins d’analyse **ne modifient pas les imports de l’exécution F5**.

L’équivalent pour la session courante :

```bash
./bin/python-studio --project "/chemin/mes-addons" \
  --python "/chemin/mon-venv/bin/python" \
  --import-dir "/opt/odoo-18"
```

Choisissez un Python capable de **parser la syntaxe de votre version d’Odoo**. La complétion n’a besoin que de sa bibliothèque standard : PyLint n’est pas une dépendance de cette fonction. PyLint reste nécessaire pour les diagnostics PyLint.

## 2. Utiliser les suggestions

Exemples de préfixes reconnus lorsque les déclarations sont disponibles :

```python
from odoo import fields, models, api

# fields.Ch          → Char
# models.Mo          → Model
# @api.dep           → depends / depends_context
# self.ens           → ensure_one
# self.ti            → title, si le champ est déclaré dans la classe
# self.env['res.p    → noms de modèles déclarés dans les sources indexées
```

- Les propositions apparaissent après une courte pause de saisie, sans bloquer l’interface.
- **↑ / ↓** choisissent une proposition ; la **molette** fonctionne sur la liste.
- **Tab** ou **Entrée** l’insèrent ; **Échap** ferme la liste.
- Une insertion au milieu d’un nom remplace aussi son suffixe : pas de `Charar` ajouté par erreur.
- Une réponse ancienne n’est pas réutilisée après modification, changement de volet, sélection, changement de focus ou fermeture de la liste.
- Le bouton **Actualiser** des dossiers d’import et la commande de réindexation vident le cache. Utilisez-les après des changements externes dans les sources/modules ; les sauvegardes et modifications des dossiers d’import invalident aussi le cache.

## 3. Créer, modifier et supprimer une macro

Ouvrez **Paramètres → Macros / extraits de code → Gérer**. La palette **Ctrl+Maj+P** donne également accès au gestionnaire.

Un exemple **`xml_form`** est proposé dès la première utilisation. Il insère un squelette XML de vue formulaire Odoo avec des noms à adapter. Vous pouvez le modifier ou le supprimer.

1. Cliquez sur **Nouvel extrait** ou sur le bouton de modification d’un extrait existant.
2. Saisissez un **déclencheur**, par exemple `xml_form`.
3. Ajoutez éventuellement un titre descriptif.
4. Écrivez ou collez votre **code multiligne** dans le grand champ.
5. Cliquez sur **Enregistrer**, ou pressez **Ctrl+S**.

Dans un fichier `.xml`, `.py` ou autre, tapez le déclencheur, sélectionnez la proposition marquée **Macro**, puis pressez **Tab** ou **Entrée**. Le déclencheur est remplacé par le texte enregistré.

### Édition du code de la macro

- **Entrée** ajoute une ligne en conservant l’indentation courante.
- **Tab / Maj+Tab** indentent / désindentent le code.
- **Ctrl+Tab / Ctrl+Maj+Tab** changent de champ, y compris depuis le code.
- Coller, couper, copier, sélectionner, annuler et rétablir utilisent les raccourcis habituels.
- Le texte collé conserve ses lignes ; CRLF/CR sont normalisés.
- **Annuler / Échap** demandent confirmation si le brouillon a changé. Vous pouvez continuer l’édition, enregistrer ou abandonner.
- La suppression d’un extrait demande confirmation. Elle ne supprime aucun fichier du projet.
- Un échec d’écriture conserve la bibliothèque précédente et le code du brouillon.

### Insertion et limites

- **Une insertion = une annulation Ctrl+Z**.
- Les lignes suivantes héritent des espaces/tabulations placés avant le déclencheur, en plus de l’indentation du modèle.
- Le texte est **littéral** : `$0`, `${nom}` et le code Python ne sont pas évalués. Ce ne sont ni des scripts exécutables ni un enregistrement de touches.
- L’insertion ne sauvegarde pas automatiquement votre document.
- La complétion et les macros utilisent **un seul curseur**. Pressez Échap pour revenir d’abord à un curseur ; l’édition multi-curseur normale reste disponible.
- Bibliothèque : **64 extraits**, déclencheur ASCII de **63 caractères**, titre UTF-8 de **95 octets**, code UTF-8 de **16 Kio** par extrait. Déclencheurs uniques : lettre ou `_`, puis lettres/chiffres/`_`.

## 4. Bibliothèque commune et sauvegarde

La bibliothèque est enregistrée immédiatement après une création, modification ou suppression réussie dans **`snippets.cfg`**, à côté des préférences de l’éditeur :

- Linux/macOS : `~/.config/prisme-c/snippets.cfg` ;
- Windows : `%APPDATA%\prisme-c\snippets.cfg` ;
- avec `PRISME_CONFIG_DIR` : dans ce dossier explicite.

Aucun fichier de configuration n’est créé dans vos projets. Tous les ateliers utilisent cette même bibliothèque. Une bibliothèque volontairement vide reste vide au redémarrage.

Pour une sauvegarde ou un transfert, copiez ce fichier lorsque l’éditeur est fermé. Le format interne est encadré par des longueurs en octets pour préserver exactement le texte multiligne : préférez le gestionnaire à l’édition manuelle du fichier.

## 5. Portée de la complétion statique

L’analyse lit des fichiers `.py` / `.pyi` : imports, alias, réexports, classes, héritage, méthodes, champs déclarés dans le document non enregistré, `_name`, `_inherit` et relations dont le nom cible est littéral. Les dispositions monolithiques Odoo 18 et les réexports/paquets séparés Odoo 19 ont des cas de régression sur des sources réelles.

Ce n’est **pas le registre ORM d’une instance Odoo**. Les modèles/champs générés dynamiquement, modules chargés via des hooks, métadonnées en base, droits, vues installées, types calculés et dépendances optionnelles ne sont pas intégralement déductibles. Aucune garantie exhaustive pour une autre version ou votre installation exacte. Il ne s’agit pas d’un serveur LSP complet.

L’index des modèles est borné à **4 096 fichiers / 64 Mio / environ 2,5 secondes**, avec également des limites de parcours par dossier. Un message **Index Odoo partiel** apparaît dans les informations de la liste lorsque ces limites sont atteintes. Privilégiez des dossiers ciblés. Chaque source est limitée à 2 Mio ; la requête native est interrompue après 15 secondes. Les erreurs d’interpréteur, de dossier ou de protocole sont signalées.

L’analyse ne fait ni `import odoo`, ni `eval`, ni appel de vos méthodes, et ne réécrit pas les documents. Le Python choisi et les paquets déjà installés dans cet environnement restent de confiance ; l’isolation `-I -B` n’est pas une sandbox générale. **F5 reste une exécution réelle**, distincte de la complétion et réservée à votre code de confiance.

## 6. Vérifications et plateformes

Tests sans fenêtre : persistance globale/écriture atomique, profils invalides/tronqués, limites, UTF-8, collage multiligne, indentation, annulation, clavier/souris simulés, dialogues, listes, transport de documents de plus de 4 Ko, réponses périmées, annulation/relance du worker, arrêt inattendu, délais et nettoyage des fichiers privés.

Les binaires fournis sont **Linux x86-64**. L’interface de bureau réelle et les compilations Windows/macOS ne sont pas validées dans cet environnement. Aucun aperçu, capture ou serveur graphique n’a été généré pour cette version.
