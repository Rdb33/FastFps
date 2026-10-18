# Musiques locales facultatives

La version actuelle du dépôt ne fournit aucun morceau de musique. Le lecteur accepte deux fichiers MP3 locaux, cherchés relativement à l'exécutable (`build/../assets/music`), puis au dossier courant :

| Emplacement | Utilisation |
| --- | --- |
| `assets/music/menu.mp3` | Menu principal, réglages hors partie et salons |
| `assets/music/gameplay.mp3` | Combat, pause et choix de modules |

Place uniquement des morceaux pour lesquels tu as les droits adaptés à ton usage. Pour une distribution de FastFps à d'autres personnes, assure-toi que l'autorisation couvre aussi la redistribution avec le jeu. Les fichiers `*.mp3` de ce dossier sont ignorés par Git.

Lecture MP3 en continu, 22 050 Hz stéréo, avec mpg123 ; volume nul par défaut, réglable dans les paramètres. Les commandes de volume et les fondus restent disponibles. Sans fichier valide, le jeu continue avec ses bruitages.

## Anciennes versions

Les anciennes copies nommées `menu.mp3` et `gameplay.mp3` étaient respectivement **Blank Shell** et **General Release**, de **Mike Klubnika**, issus de **Buckshot Roulette**. Ces enregistrements sont marqués « all rights reserved » sur leur publication officielle et ne relèvent pas de la licence MIT du projet. Voir [THIRD_PARTY_NOTICES.md](../../THIRD_PARTY_NOTICES.md).

Si tu conserves personnellement ces anciennes copies, sauvegarde-les hors du dépôt **avant** de mettre à jour un clone qui les suit encore : Git retirera les fichiers suivis. Retirer les morceaux de la version courante ne les efface pas des anciens commits ; cette modification ne réécrit pas l'historique public.
