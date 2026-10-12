# FASTFPS — Secteur zéro

**FPS 3D natif, style Doom, pensé pour le Raspberry Pi 4.** Base à améliorer progressivement : déplacement nerveux, deux armes et survie dans une arène industrielle. Version **0.3.0** : salons coopératifs pour quatre joueurs, avec un joueur hôte.

Application en **C11**, **SDL2** (fenêtre, clavier, souris et son) et **OpenGL ES 2** (rendu 3D). Graphismes, police et sons sont générés localement.

Le mode solo fonctionne hors ligne. Pour jouer ensemble, sélectionne **Multijoueur / salons** ; voir [MULTIJOUEUR.md](MULTIJOUEUR.md) pour la création, la connexion sur Internet et les limites de cette première version.

## Lancer sur cette machine

```bash
cd ~/Projets/fastfps
./lancer.sh
```

Le lanceur compile les fichiers modifiés puis ouvre la fenêtre. Le binaire compilé peut aussi être lancé directement avec `./build/fastfps`.

Sélectionne **Jouer** avec la souris ou les flèches, puis clique ou appuie sur `Entrée`. Le jeu capture la souris. `Échap` met en pause et libère la souris ; appuie de nouveau sur `Échap` ou sélectionne **Reprendre** pour continuer. Choisis **Quitter** pour fermer l’application.

`FastFPS.desktop` est un raccourci pour `/home/rdb/Projets/fastfps`. Le gestionnaire de fichiers peut demander de l’autoriser avant de le lancer. Si tu déplaces le projet, adapte ses chemins `Exec` et `Path`.

## Installation sur Raspberry Pi 4

Utilise **Raspberry Pi OS avec une session graphique**, un clavier et une souris. Dans un terminal du Pi :

```bash
sudo apt update
sudo apt install build-essential pkg-config libsdl2-dev libgles2-mesa-dev
```

Copie le dossier `fastfps` dans `~/Projets` sur le Pi, puis compile **sur le Pi** :

```bash
cd ~/Projets/fastfps
make clean
make -j2
./lancer.sh --quality pi
```

Le binaire fourni dans `build/` est compilé pour la machine de développement et ne doit pas être utilisé tel quel sur ARM. `make clean` efface uniquement les fichiers compilés. Une fois les dépendances installées et le jeu compilé, il fonctionne hors ligne.

Le Pi 4 possède un GPU compatible OpenGL ES ([documentation Raspberry Pi](https://www.raspberrypi.com/documentation/computers/processors.html)). Le jeu utilise uniquement les API OpenGL ES 2. Le démarrage direct sur Raspberry Pi OS Lite sans session graphique n’est pas pris en charge dans ce guide.

## Commandes

| Action | Commande |
| --- | --- |
| Se déplacer | ZQSD / WASD ou les flèches |
| Viser / tirer | Souris / clic gauche, maintenir pour tirer en continu |
| Sprint | Maj |
| Sauter | Espace ; maintenir pour enchaîner à l’atterrissage |
| Esquive dans la direction du mouvement | Clic droit ou Alt gauche ; recharge de 1,25 seconde |
| Frappe au corps à corps | F ; une élimination rend 10 points de santé |
| Blaster / fusil à pompe | 1 / 2 ; molette pour changer |
| Pause / reprendre | Échap |
| Plein écran | F11, ou le menu |
| Afficher / masquer les FPS | F1 |
| Changer le profil de rendu | F2, ou le menu |
| Activer / couper le son | F3, ou le menu |
| Réduire les mouvements de caméra | F4, ou le menu |

Les déplacements utilisent les positions physiques du clavier : ZQSD sur AZERTY et WASD sur QWERTY.

## Gameplay

- Arène avec piliers, caisses, passages latéraux et plateforme accessible par des marches.
- Blaster précis à tir continu et fusil à pompe à neuf plombs ; munitions illimitées pour cette base.
- Trois types d’ennemis avec silhouettes, couleurs et animations distinctes ; voir le tableau ci-dessous.
- Vagues progressives : quatre ennemis au départ, douze maximum. Navigation en grille pour contourner les obstacles.
- Croix vertes : +30 santé, réapparition après 12 secondes. Terminer une vague : +15 santé.
- Score, record et réglages persistants. Ils sont enregistrés à la fermeture d’une partie normale via `SDL_GetPrefPath`, généralement dans `~/.local/share/fastfps/fastfps/settings.txt` sous Linux.

Direction : **Doom moderne simplifié**, mouvement direct, sprint, combat à courte et moyenne distance, arme centrée et ambiance industrielle. Les modèles géométriques sont provisoires.

| Ennemi | Apparence | Comportement |
| --- | --- | --- |
| Traqueur | Rouge, cornes, silhouette légère | Poursuit vite et prépare une frappe au contact |
| Invocateur | Violet, silhouette étroite, coiffe pointue et énergie verte | Se déplace latéralement, charge une boule d’énergie avant de tirer |
| Brute, à partir de la vague 3 | Vert olive, grandes épaulières dorées, corps plus massif | Avance lentement, résiste davantage et prépare une frappe plus longue et plus forte |

![Les trois types d'ennemis dans une scène de présentation](docs/ennemis-v02.png)

Une attaque ennemie est annoncée par un mouvement et un son. Évite-la en changeant de direction ou avec l’esquive. Les impacts font reculer les ennemis ; les créatures légères sont davantage interrompues que la brute. Le nom et la santé de l’ennemi visé s’affichent sous le haut de l’écran.

Une série d’éliminations espacées de moins de 3,5 secondes augmente le score : multiplicateur ×2 dès la quatrième élimination, ×3 dès la septième et ×4 dès la dixième.

## Animations et immersion

- Créatures orientées vers leur cible, bras et jambes articulés, respiration au repos et démarches propres aux types.
- Préparation des attaques, concentration d’énergie de l’invocateur, réactions aux impacts et chute à la mort.
- Corps visibles quelques secondes, avec un maximum de huit pour limiter le coût.
- Mains autour des armes, recul, flamme de bouche, changement d’arme animé et mouvement de pompe après chaque tir du fusil.
- Balancement de l’arme pendant la marche, inertie légère lors de la visée et amortissement de l’atterrissage.
- Pas, réarmement, esquive et cris synthétiques ; les sons des ennemis sont atténués avec la distance et placés à gauche ou à droite en stéréo.

`F4` réduit le balancement, l’inertie de visée et les mouvements de caméra. Les attaques et poses des personnages restent visibles. Le changement d’arme dure 0,32 seconde ; il interrompt brièvement le tir et conserve le délai entre tirs.

## Réglages Raspberry Pi

| Profil | Résolution interne | Limite | Particules maximum |
| --- | --- | --- | --- |
| Pi 4, par défaut | 640 × 360 | 30 FPS | 28 |
| Équilibré | 960 × 540 | 60 FPS | 64 |
| PC | 1280 × 720 | 60 FPS | 100 |

En plein écran, la 3D reste à la résolution choisie puis est agrandie avec un filtre donnant un aspect rétro. Des bandes noires préservent les proportions 16:9.

Cinq appels de dessin par image : décor, objets mobiles, arme, interface, agrandissement. Décor chargé une seule fois sur le GPU, tableaux et buffers réutilisés, aucune allocation dans la boucle normale de jeu. Les animations transforment les sommets dans ces mêmes buffers, sans ajouter d’appels de dessin. Pas de textures de décor, ombres dynamiques, anticrénelage ou traitement d’image. La seule texture contient l’image intermédiaire à agrandir. Simulation à pas fixe de 60 Hz ; navigation partagée calculée environ trois fois par seconde.

**30 FPS sur Pi 4 est une cible, pas une mesure sur un Pi réel.** La fluidité dépendra du matériel, des pilotes et de la température. Les inspections visuelles ici utilisent un rendu logiciel hors écran sur la machine de développement ; elles ne mesurent pas les performances du Pi. Le compteur FPS permet une première mesure en jeu.

## Options

```bash
./lancer.sh --quality pi --fullscreen
./lancer.sh --quality balanced
./lancer.sh --mute
./build/fastfps --help
```

Démonstration automatique pour inspecter le rendu :

```bash
./build/fastfps --demo --mute --frames 120 --screenshot capture.bmp
```

Le mode démo contrôle le joueur, ferme le jeu après le nombre d’images demandé et n’enregistre pas les réglages. Capture BMP à la résolution interne.

## Développer

```bash
make -j2
make run
make clean
```

| Fichier | Rôle |
| --- | --- |
| `src/config.h` | Vitesses, gravité, saut, limites d’entités |
| `src/fastfps.h` | Types et interfaces entre modules |
| `src/main.c` | Fenêtre, événements, menu, réglages et boucle |
| `src/world.c` | Carte, obstacles et navigation |
| `src/physics.c` | Collisions, déplacement, gravité et marches |
| `src/game.c` | Armes, ennemis, projectiles, soins, vagues et score |
| `src/renderer.c` | OpenGL ES, caméra, modèles et résolution |
| `src/animation.c` | Poses des ennemis, articulations et animations des armes |
| `src/network.c`, `src/network.h` | Connexions, salon, commandes et prédiction |
| `src/protocol.c` | Encodage portable et validation des messages |
| `src/lobby.c` | Écrans de création, connexion et attente |
| `src/mesh.c` | Maillages et police bitmap locale |
| `src/ui.c` | Menu et interface de combat |
| `src/audio.c` | Synthèse et mélange des sons |
| `src/math.c` | Vecteurs, matrices et intersections |

Carte et paramètres sont actuellement dans les sources. Aucune option de compilation spécifique à x86 : les mêmes fichiers sont compilables sur ARM.

## Limites et suite

Une carte et un mode survie solo ou coopératif. Collisions par boîtes, trajectoires parfois anguleuses. Pas encore de rampes, accroupissement, rechargement, sauvegarde de partie ou manette.

La coopération utilise un joueur hôte qui fait autorité sur le monde, un état d’entrée `Input`, des identifiants d’ennemis stables, une prédiction du déplacement et une interpolation à l’affichage. La connexion se fait par IPv4 et port TCP, sans service central. La validation entre deux machines et sur Internet reste à faire : voir [VALIDATION.md](VALIDATION.md).

Voir [ROADMAP.md](ROADMAP.md). Le prototype navigateur est conservé dans `prototype-web/` comme archive ; le lanceur principal utilise **l’application native**.

API utilisées : [SDL2](https://wiki.libsdl.org/SDL2/FrontPage), [capture relative de souris](https://wiki.libsdl.org/SDL2/SDL_SetRelativeMouseMode).
