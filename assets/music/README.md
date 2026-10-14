# Musiques locales

Fichiers copiés depuis `/home/rdb/Musique` à la demande du propriétaire du projet :

| Fichier du jeu | Fichier original | Utilisation |
| --- | --- | --- |
| `menu.mp3` | `01. Blank Shell.mp3` | Menu principal, options hors partie et salons |
| `gameplay.mp3` | `02. General Release.mp3` | Combat, pause et choix de modules |

Les fichiers sont inchangés. Lecture MP3 en continu, 22 050 Hz stéréo, avec mpg123 ; aucune conversion nécessaire au lancement. Volume nul par défaut. Le jeu cherche les fichiers relativement à l’exécutable (`build/../assets/music`), puis au dossier courant ; `lancer.sh` fonctionne aussi depuis un autre dossier.
