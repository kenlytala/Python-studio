# Vérification Odoo 19 — Python Studio 1.20.0

Disponible dans les trois éditions C, Python et GTK4. Aucun serveur Odoo, ORM ou accès à une base n’est nécessaire pour cette analyse statique.

## Activer/configurer ce projet

**Paramètres → Odoo 19 — Python / XML — Ce projet** :

- Mode Odoo19 actif par défaut pour un nouveau projet ; désactivez-le pour un projet Python générique.
- Liste des règles Odoo ignorées : codes ou noms, mêmes limites de saisie que les exclusions existantes. Exemples : `W8165, odoo-xml-duplicate-id`.
- Appliquer sauvegarde dans les préférences privées de l’éditeur, sous un profil lié au chemin du projet, invalide les anciens diagnostics et redemande l’analyse active. Annuler ne change pas le réglage.
- Les règles Odoo ne sont pas les préférences globales de disposition et ne sont pas restaurées depuis un ancien journal de session. Chaque projet possède sa propre liste.

Le bouton **−** d’un diagnostic Odoo ajoute la règle à **ce projet**. Celui d’un diagnostic PyLint générique conserve sa portée globale. L’infobulle précise la portée. Réactivation en retirant le code/nom dans le dialogue correspondant ; une liste vide ne crée aucune exclusion.

## Python : vrai plugin OCA

L’analyse appelle le vrai **pylint-odoo10.0.12**, avec `--valid-odoo-versions=19.0`. Cette version requiert **PyLint4.0.x**, installé par les dépendances du paquet. Exemple réellement testé : accès à `self._cr` → `W8165 / deprecated-self-cr` en Odoo19. D’autres diagnostics OCA portent sur manifests, méthodes, champs, traductions et usages déconseillés ; leur disponibilité exacte relève du plugin installé.

L’interpréteur est celui configuré par **--python** (comme F5). Si vous choisissez le venv de votre projet, installez-y la combinaison :

```bash
/chemin/venv/bin/python -m pip install 'pylint-odoo==10.0.12'
```

Pour l’édition C, qui ne gère pas automatiquement le pip de l’utilisateur :

```bash
python3 -m venv .analysis-venv
.analysis-venv/bin/python -m pip install 'pylint-odoo==10.0.12'
./lancer.sh --python "$PWD/.analysis-venv/bin/python"
```

Ce Python doit également disposer des dépendances nécessaires à l’exécution F5 si vous l’utilisez pour lancer votre code. Les racines **Imports Python/Odoo** continuent de servir à la résolution statique. Aucun import/exécution de votre framework Odoo n’est demandé par l’éditeur pour compléter/contrôler le code.

Un plugin absent ou ne reconnaissant pas19.0 est signalé explicitement, sans prétendre que le code Odoo a été validé. La syntaxe Python est contrôlée avant le plugin. En désactivant le mode Odoo pour le projet, le PyLint générique reste utilisable, y compris avec les interpréteurs2.x de la matrice de compatibilité ; les exclusions propres à Odoo ne lui sont pas transmises.

L’analyse charge **uniquement le plugin approuvé de l’interpréteur**, avant d’ajouter les chemins du projet. Un fichier projet `pylint_odoo.py`/`pylint.py`, `.pylintrc`/init-hook ou un plugin arbitraire de configuration projet ne remplace pas ce chargement.

## XML : contrôles statiques locaux

Pour `.xml/.xsl/.xslt/.svg`, F4 et l’analyse automatique utilisent un parseur XML, jamais la compilation Python. La syntaxe XML demeure contrôlée même si le mode Odoo est désactivé. `E0001 / syntax-error` reste non désactivable par le bouton.

Mode Odoo actif : contrôles bornés et prudents, affichés comme avertissements :

| Code | Règle |
|---|---|
| W9001 | Identifiant local (ou t-name) réutilisé ; peut être une mise à jour volontaire |
| W9002 | model absent/vide sur record |
| W9003 | Identifiant XML vide |
| W9004 | Espace/contrôle dans un identifiant |
| W9005 | Vue tree ancienne, list attendu pour Odoo19 |
| W9006 | name absent/vide sur field |
| W9007 | attrs/states anciens dans une architecture de vue |
| W9008 | Racine openerp ancienne |
| W9009 | Racine non standard pour données/vues Odoo |
| W9010 | Inclusion externe non résolue par l’analyse statique |

Les IDs externes et références à d’autres fichiers/modules **ne sont pas validés globalement**. Une réutilisation d’ID peut être intentionnelle et peut être ignorée. Les modèles/champs de la base ne sont pas vérifiés. Les templates Owl, namespaces et CDATA sont lus sans évaluation de leurs expressions. **Un field arch fermé et vide est accepté**, conformément à la génération XML existante.

Limites :2Mio de source,20000 éléments,profondeur256,4096IDs locaux,256diagnostics stockés avec priorité à l’erreur de syntaxe. Les DTD/déclarations d’entités sont refusées avant expansion ; aucune ressource externe/file/http n’est ouverte. Une limite ou syntaxe invalide est affichée, pas un faux rapport sans problèmes.

## Coloration et portée

Python, JavaScript et XML sont colorés selon l’extension (majuscules comprises). JavaScript n’est pas envoyé à la compilation/PyLint Python : cette livraison ajoute sa **coloration**, pas un moteur ESLint/Node. Les lexeurs sont tolérants et lexicaux, pas des parseurs ECMAScript complets.

Les diagnostics Odoo Python et XML sont navigables dans le même panneau. Les positions XML UTF-8 sont converties aux coordonnées de chaque éditeur. Les anciens gardes de réponse/version/projet restent actifs, de même que l’historique, les sessions et le SSH fichiers seulement.

Dépendance externe : OCA/pylint-odoo, https://pypi.org/project/pylint-odoo/ et https://github.com/OCA/pylint-odoo ; version10.0.12 vérifiée localement. Le plugin n’est pas redistribué ici et garde sa licence propre. Pas de certification de conformité complète Odoo ou de migration automatique. Voir VALIDATION-1.20.0.md pour les essais et limites sans bureau physique.
