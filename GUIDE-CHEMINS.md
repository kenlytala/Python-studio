# Ctrl+Espace dans les champs de chemin — 1.20.0

Dans **Ouvrir fichier**, **Ouvrir dossier**, **Enregistrer sous**, sélection des dossiers **Imports/Odoo**, **XML** et **recherche de texte** :

1. Saisissez le chemin ou un début de chemin dans le champ.
2. Appuyez sur **Ctrl+Espace** pour lire explicitement son contenu.
3. Utilisez **↑/↓** pour choisir, puis **Tab ou Entrée** pour compléter le champ.
4. Appuyez ensuite sur Entrée ou utilisez le bouton du dialogue pour ouvrir/choisir/valider.

Exemples : `sale/`, `/chemin/mon dossier/`, `~/projets/`, `/chemin/préfixe`. Un dossier existant sans `/` final propose aussi son contenu. Un champ vide propose le dossier du dialogue. Les modes de sélection de dossier ne proposent que les dossiers ; le mode fichier propose dossiers et fichiers. Les dossiers sont classés avant les fichiers, avec ordre stable.

**La suggestion ne lance rien et n’ouvre pas automatiquement un fichier.** Elle est affichée dans la liste du dialogue. Choisir une suggestion complète le champ ; le code de l’éditeur, ses curseurs et son projet ne changent pas pendant cette étape. L’insertion est une édition du champ annulable/rétablissable par Ctrl+Z/Y. Le chemin relatif garde son dossier de base même après lecture des suggestions.

La lecture est non récursive et explicite, dans un worker annulable : pas de scan sur chaque frappe, ni d’index global. Au plus8192 entrées examinées et256 suggestions conservées ; un statut indique les listes limitées. Chemins UTF-8 jusqu’à4095octets, espaces et guillemets littéraux pris en charge ; caractères de contrôle refusés. Aucun contenu de fichier n’est lu pour proposer son nom. Les fichiers spéciaux/FIFO ne sont pas proposés comme fichiers ordinaires.

Un changement du champ, de sa sélection/curseur, du dialogue, du projet ou du bail SSH rend la réponse périmée. Échap/fermeture annule la demande. Une ancienne lecture encore bloquée dans le noyau doit terminer avant une nouvelle demande ; un message le précise. Les gardes de montages et de symlinks sont conservés, sans connexion ou authentification SSH automatique. Ce n’est pas un sandbox contre un système de fichiers hostile.

La complétion **dans le code** après `sale/` ou un chemin XML reste inchangée. Ce raccourci supplémentaire est spécifique au champ de chemin du dialogue ; il ne substitue pas une interprétation de code à la saisie littérale du chemin.
