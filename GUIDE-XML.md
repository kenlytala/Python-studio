# Dossiers et édition XML — Python Studio 1.18.0

Fonction disponible dans les **deux éditions indépendantes, Python et C**. Elle lit un fichier XML existant, sans importer Odoo, exécuter ses expressions ou contacter une base de données.

## Prédéfinir les dossiers des chemins relatifs

1. Ouvrez **Paramètres → Dossiers XML**, ou **Ctrl+K → Dossiers XML**.
2. Cliquez **Ajouter un dossier**, choisissez la base souhaitée dans le navigateur, puis validez le dossier. Jusqu’à **huit dossiers par projet** ; les doublons sont refusés.
3. Montez/descendez les dossiers avec les flèches pour définir la priorité. **Retirer** enlève une base de recherche, sans effacer le dossier ni ses fichiers.
4. Les changements sont enregistrés immédiatement pour le projet actuel, sans modifier ses fichiers Python ni les paramètres PyLint. Chaque édition garde ses propres préférences XML ; ce n’est pas un format de profils partagé entre Python et C.

Exemple : ajoutez `/home/moi/sources/odoo` et `/home/moi/addons-personnels`. Dans le document, vous pouvez alors écrire :

```text
addons/sale/views/sale_order_views.xml
```

ou un chemin relatif au second dossier. Si deux bases contiennent le même chemin, la **première** gagne. Les bases sont absolues ; c’est le chemin tapé dans le document qui peut être relatif. Le projet, le dossier du document et les anciens dossiers d’import restent des replis, dans cet ordre. Huit bases XML plus huit imports plus ces deux replis donnent au plus **18 candidats distincts** ; un chemin absolu n’utilise pas ces bases.

Les dossiers servent de **points de départ aux chemins que vous saisissez**. Il n’y a ni parcours récursif global, ni recherche d’un identifiant dans tous les fichiers, ni index XML chargé au démarrage. Ajouter une base ne change pas le Python choisi, PyLint, la complétion Python ou F5.

Sous Linux, les préférences Python sont `~/.config/prisme/xml-roots-<empreinte>.json`, celles du C `~/.config/prisme-c/xml-roots-<empreinte>.cfg` ; `$PRISME_CONFIG_DIR` remplace le dossier de configuration s’il est défini. Ne renommez pas manuellement les empreintes, liées au projet. Les dossiers déjà enregistrés peuvent être hors ligne : retirer/réordonner leur entrée ne les ouvre pas. L’ajout d’un dossier SSH demande une connexion déjà établie explicitement.

## Voir le contenu d’un dossier avec Ctrl+Espace

Avec `/home/moi/odoo/addons` comme base, saisissez **`sale/`**, puis **Ctrl+Espace**. Les fichiers et sous-dossiers immédiats apparaissent dans le menu de complétion, sans ouvrir de dialogue de chemin séparé. Cela fonctionne aussi dans un document Python ou un attribut entre guillemets, par exemple `file="sale/"`.

1. **↑ / ↓** choisit une ligne ; **Entrée**, **Tabulation** ou un clic l’insère.
2. Un sous-dossier `views` donne **`sale/views/`** puis affiche immédiatement le contenu de `views`.
3. Choisissez `sale_views.xml` pour obtenir `sale/views/sale_views.xml`.
4. Sur ce chemin XML complet, **Ctrl+Espace** conserve la priorité de l’ancien sélecteur d’identifiants : choisissez l’élément, ses champs sont insérés, avec les `arch` vides.

Les propositions sont fusionnées dans l’ordre **bases XML → projet → dossier du document → imports Python**, avec priorité à la première entrée de même nom et type. Les dossiers précèdent les fichiers ; les noms sont triés selon leurs octets UTF-8, sans changement de casse. Tous les types de fichiers, dont les fichiers cachés, sont admissibles ; aucun fichier n’est exécuté, importé ou lu pour son contenu pendant cette liste.

Vous pouvez taper **`sale/vi`**, puis demander à nouveau **Ctrl+Espace**. La saisie ordinaire ferme l’ancienne liste sans parcourir le disque. **Échap** ferme aussi la liste, y compris pendant une lecture ; un résultat devenu périmé (document, curseur, panneau, projet, bases ou session SSH) ne remplace pas votre texte. Une insertion de chemin est une transaction d’annulation/rétablissement ; un simple déplacement dans un nom inchangé n’ajoute pas de fausse entrée.

### Limites et protections de la liste

- Une barre de dossier doit déjà précéder le curseur. `sale` seul reste une demande de complétion ordinaire ; `sale/` demande le dossier. Les URL et les barres de fermeture de balises comme `<field />` ne déclenchent pas de lecture de dossier.
- Chemins relatifs, absolus et `~/…` ; les guillemets déjà présents et les composants suivants sont conservés. Un nom avec espaces ou délimiteurs reçoit des guillemets autour du chemin si nécessaire. Dans un composant parent, seuls les dossiers sont proposés. Les sélections et multicurseurs n’ouvrent pas cette liste.
- **32 propositions**, **8 192 entrées examinées** et **18 candidats** par demande ; uniquement le contenu immédiat, jamais de recherche récursive. Une indication de limite demande de préciser le préfixe. Un chemin complété reste borné à **4 095 octets UTF-8**.
- Noms contenant des contrôles, des guillemets impossibles à conserver sans échappement, chemins trop longs, fichiers spéciaux ou entrées inaccessibles : ignorés, avec indication. Pas d’échappement général de chaînes Python/XML ; vérifiez la syntaxe de l’endroit où vous insérez le chemin.
- Un seul worker de dossier à la fois. Après annulation d’une lecture encore bloquée par le système de fichiers, attendez sa fin avant une nouvelle demande ; l’interface n’attend pas son arrêt. Un appel système bloqué peut néanmoins retarder la fermeture de l’application.
- En SSH : connexion **déjà établie explicitement**, garde avant les sondes, génération/possession du montage vérifiées, aliases locaux couverts. Ni authentification automatique, ni Python distant, ni index global. Ces garde-fous ne constituent pas une sandbox contre un système de fichiers hostile.

## Complétion et fermeture automatiques des balises

Active dans un document nommé **`.xml`**, sans distinction de casse, avec l’option globale **Balises XML automatiques** des paramètres (activée par défaut). Cette option n’active pas de génération dans les fichiers `.py`, et ne désactive pas vos macros globales.

- Après `<record id="mon_id"`, tapez **`>`** : l’éditeur produit `<record id="mon_id"></record>` et place le curseur entre les balises.
- Après `<r><form><`, tapez **`/`** : l’éditeur complète la fermeture du parent encore ouvert, ici `</form>`.
- Dans `<fi`, appuyez sur **Ctrl+Espace**, puis choisissez `field` : vous obtenez `<field></field>`, avec le curseur au centre. Une ouverture déjà munie d’attributs conserve ses attributs. Les propositions incluent les noms rencontrés dans le document et des balises XML/Odoo courantes : `odoo`, `record`, `field`, `form`, `list`, `tree`, `kanban`, `xpath`, `template`, `t`, etc. Après `</`, la priorité va aux parents ouverts.
- **Entrée** entre `<form>` et `</form>` crée une ligne indentée pour le contenu et aligne la fermeture. Les tabulations existantes sont respectées ; sinon, l’indentation ajoutée est de quatre espaces.
- Une fermeture déjà présente n’est pas ajoutée une deuxième fois, même après du texte ou des éléments imbriqués. `/>` reste une balise autofermante. Les noms Unicode et qualifiés, par exemple `q:élément`, sont acceptés.
- Attributs entre guillemets, commentaires, CDATA, instructions de traitement et déclarations sont traités comme opaques : pas de fermeture indésirable à l’intérieur. Une sélection remplacée ou un collage reste littéral.

Cette aide est **lexicale et prudente**, pas un validateur Odoo/XML complet, un moteur de schéma ou une complétion exhaustive d’attributs. Un document déséquilibré, un nom invalide, une profondeur supérieure à 128 ou un contexte dépassant le budget de 32 768 événements de balisage entraîne un repli sans génération. Le nom d’une balise est borné à 127 octets UTF-8 ; 64 noms locaux sont retenus pour les suggestions. Ces limites de frappe ne tronquent pas l’index d’identifiants lu explicitement en arrière-plan, dont les limites figurent plus bas. La résolution des préfixes XML et la validité métier Odoo restent de votre responsabilité.

## Annuler et rétablir

Dans **les deux éditions** :

| Action | Raccourci |
|---|---|
| Annuler la dernière modification du document | **Ctrl+Z** |
| Rétablir | **Ctrl+Y** ou **Ctrl+Maj+Z** |

Une paire créée automatiquement, une complétion de balise, une insertion d’identifiant, un collage ou une opération multicurseur se défait et se rétablit **en une seule transaction**, avec les positions/sélections des curseurs. Une nouvelle saisie après annulation invalide la branche de rétablissement. Le saut par-dessus un `>` déjà présent ne crée pas de faux changement dans l’historique.

Il s’agit de l’historique du **texte du document**, pas d’une restauration des fichiers supprimés dans l’explorateur ou d’un historique persistant entre deux lancements. Sauvegardes et brouillons de session restent des mécanismes distincts.

## Utilisation directement dans le document

1. Tapez ou collez le chemin du fichier dans votre code, par exemple :

   ```text
   odoo/addons/sale/views/sale_views.xml
   ```

2. Laissez le curseur sur ce chemin, ou sélectionnez-le, puis appuyez sur **Ctrl+Espace**.
3. Le sélecteur affiche **tous les éléments portant un attribut `id` non vide**, dans l’ordre du fichier : records, templates, menus, éléments imbriqués, etc. Les doublons restent des occurrences distinctes, avec leur modèle/balise et leur ligne.
4. Filtrez par identifiant, balise ou modèle ; utilisez **↑ / ↓ puis Entrée**, cliquez une ligne, ou cliquez **Insérer**.
5. **Le chemin est remplacé dans le document** par l’élément choisi et ses descendants. Il ne s’agit pas d’une simple copie dans le presse-papiers. **Ctrl+Z** annule l’insertion en une seule opération ; **Ctrl+Y** ou **Ctrl+Maj+Z** rétablit le texte, dans les deux éditions.

La palette **Ctrl+K → Insérer un bloc depuis un chemin XML** propose la même action. Il n’y a pas de dialogue séparé demandant le chemin. Un seul curseur est requis ; Échap permet de revenir à un curseur si nécessaire. Hors d’un chemin `.xml`, Ctrl+Espace conserve la complétion habituelle, notamment `xml_form` et Odoo.

### Chemins acceptés

- Relatif aux **dossiers XML configurés**, dans leur ordre, puis au **projet**, au **dossier du document actuel**, puis aux **dossiers d’import Python** (dans cet ordre). En cas de plusieurs fichiers de même nom, le premier fichier trouvé est utilisé.
- Absolu, ou commençant par **`~/`**.
- Avec des espaces : sélectionnez le chemin complet ou entourez-le de guillemets simples/doubles, par exemple `"/opt/odoo sources/addons/sale/views/sale_views.xml"`. Les guillemets font partie du texte remplacé, pas du nom du fichier.
- Les URL HTTP/HTTPS/`file://` ne sont pas des chemins acceptés. Le nom peut avoir une extension `.XML` en majuscules.

Le fichier peut s’appeler autrement : le nom est libre. Les fixtures des sources officielles Odoo 18/19 utilisées en QA se nomment `addons/sale/views/sale_order_views.xml` ; cela n’impose aucun nom particulier à vos fichiers.

## Contenu inséré : champs conservés, architecture vide

Par exemple, si l’élément contient les champs `name`, `model` et `arch`, le résultat est :

```xml
<record id="view_order_form" model="ir.ui.view">
    <field name="name">sale.order.form</field>
    <field name="model">sale.order</field>
    <field name="arch" type="xml"></field>
</record>
```

Tous les **autres champs et attributs de l’élément source** restent présents : `inherit_id`, `priority`, `groups_id`, références, valeurs, etc. L’exemple ci-dessus est volontairement simplifié ; la fonction ne réduit pas les véritables records à trois champs.

Chaque descendant `<field name="arch">` est vidé, y compris lorsqu’il est imbriqué. **Le champ est gardé avec une véritable balise de fermeture**, jamais une fermeture manquante. Ses attributs `eval`, `ref` et `file`, qui fourniraient encore une architecture, sont retirés ; ses autres attributs, dont `type="xml"` s’il existe, sont conservés. Un identifiant situé à l’intérieur d’une architecture reste lui aussi proposé : le choisir copie cet élément précis, pas le record parent.

Le fragment est du XML bien formé, sans déclaration `<?xml …?>`. Les espaces de mise en forme peuvent être normalisés (quatre espaces, ajoutés à l’indentation de la ligne). Les valeurs XML, contenus mixtes, commentaires, instructions de traitement, espaces préservés et espaces de noms utiles restent représentés ; des caractères de texte peuvent être réécrits sous forme d’entités XML. Le résultat n’est donc pas une reproduction octet pour octet. L’identifiant n’est **pas renommé** automatiquement : adaptez-le avant de réutiliser le bloc dans votre module.

## Lecture explicite, annulation et SSH

- **Aucune lecture d’un fichier XML source n’est déclenchée automatiquement pendant la frappe.** L’assistance aux balises travaille uniquement sur le texte du document en mémoire. La lecture/indexation se fait dans un worker après Ctrl+Espace ou l’action explicite de la palette.
- **Échap / Annuler** ferme le sélecteur sans changer le chemin ni l’historique. Un XML introuvable/invalide, une limite dépassée ou un contexte devenu obsolète ne produit aucune insertion partielle.
- La source n’est jamais enregistrée ou modifiée par cette fonction. Le choix utilise l’instantané lu à l’ouverture du sélecteur ; une modification ultérieure du fichier source n’est pas relue silencieusement. Fermez et relancez la commande pour actualiser la liste.
- Si le document, sa version, le curseur/sélection, le volet actif ou le projet a changé, la proposition est refusée : relancez Ctrl+Espace.
- Un fichier distant exige une **connexion SSH déjà ouverte explicitement**. Les contrôles de propriété/génération et les protections hors ligne sont conservés, y compris pour les alias locaux vers les montages réservés. Aucun secret d’authentification SSH n’est ajouté à l’index ou mémorisé ; aucun auto-login ou Python distant n’est ajouté.
- Un candidat situé sur un montage SSH absent/remplacé bloque la requête, plutôt que de lire un répertoire local vide laissé au même emplacement. Pour utiliser une source locale à la place, indiquez un chemin local explicite.
- Annuler ne bloque pas le thread de l’interface pour attendre une lecture réseau. Une lecture système déjà bloquée peut toutefois finir plus tard ; une nouvelle requête XML attend la fin de cette annulation. À la fermeture de l’application, le C rejoint son worker avant de libérer SSH ; Python attend jusqu’à dix secondes. Il n’existe pas de garantie générale d’interruption immédiate des appels système, ni de sandbox contre toutes les courses d’un système de fichiers hostile.

Le sélecteur, son filtre et son index ne sont pas restaurés par les sessions. **Le texte inséré devient un brouillon normal** : avec les sessions activées, il est susceptible d’être conservé dans le journal local privé **en clair**. Voir [GUIDE-SESSIONS.md](GUIDE-SESSIONS.md) et `--no-session`.

## Bornes et dépendances

- Texte source décodé en **UTF-8**, avec ou sans BOM ; fichier régulier de **8 Mio maximum**.
- **4 096 identifiants maximum**, **128 niveaux** ; structure/attributs bornés à environ 100 000 événements. Une limite dépassée est une erreur visible, jamais une liste tronquée silencieusement.
- Identifiant/modèle : **1 023 octets UTF-8** chacun ; nom local de balise : **127 octets**.
- Fragment final, indentation comprise : **256 Kio maximum** ; document cible : limite existante de **2 Mio**.
- **DTD et entités externes interdites** ; aucun chargement réseau, XInclude ou évaluation XML.
- Python : parseurs de la bibliothèque standard, sans nouvelle dépendance PyPI.
- C : véritable parseur natif **libxml2 2.9+** ; développement `libxml2-dev` pour compiler, bibliothèque d’exécution `libxml2.so.2` pour le précompilé Linux. Pas de délégation du XML à Python.

Les essais et exclusions de la livraison sont dans **[VALIDATION-1.18.0.md](VALIDATION-1.18.0.md)**. Aucun aperçu ni capture n’a été généré.
