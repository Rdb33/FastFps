# Contrôles de FASTFPS

Historique des contrôles sur la machine de développement, les 12 et 13 octobre 2026.

## Version 0.9.2 — 14 octobre 2026

- Compilation C11 avec les avertissements habituels et revue des changements ; aucune dépendance ajoutée.
- Inspection des trois layouts : nouveaux balcons à 3,48 m, paliers des plateformes à la même hauteur, jeux de 5 à 10 cm entre les plaques et les balcons, ouvertures vers la navette et passages au sol conservés. La navette passe au nord du réacteur, à l’écart des piliers et machines.
- Mouvement des plateformes et vitesse emportée calculés depuis le même temps de simulation côté hôte et prédiction. Protocole porté à 8 pour refuser les anciennes collisions / trajectoires. Les positions et délais des soins sont déjà transmis dans les snapshots.
- Modèles et étiquettes dessinés dans les buffers existants ; aucune allocation par coéquipier à chaque image, aucune passe de rendu supplémentaire. Détail limité à six côtés sur le profil Pi, huit sur les profils supérieurs.
- Fixtures des tests de plateformes existants adaptées ; **tests non exécutés pour cette version**. Les résultats historiques ci-dessous concernent leurs versions respectives.
- Déplacements entre les paliers, lisibilité des personnages en jeu, session multijoueur réelle et performances sur Raspberry Pi à vérifier en jouant ; pas de nouvelle mesure matérielle annoncée.

## Version 0.9.1 — 14 octobre 2026

- Compilation C11 sans avertissement et contrôle du diff.
- Rappel local de huit secondes, une seule fois par partie, réinitialisé pour une nouvelle partie solo ou un retour au salon. Son délai est suspendu pendant la pause, les modules, les joueurs à terre et le panneau de progression ; activer la musique le retire. Démo et `--mute` ne l’affichent pas.
- Aucun changement de simulation ou de protocole réseau. Affichage en jeu non testé dans cette version.

## Version 0.9.0 — 14 octobre 2026

- Compilation complète C11 avec `-O2 -Wall -Wextra -Wpedantic`, sans avertissement, avec SDL2 / OpenGL ES / mpg123.
- Les deux MP3 du dossier Musique sont copiés sans conversion dans `assets/music` ; durées annoncées : 128 s et environ 130,4 s.
- Lecture du code : aucune lecture de fichier, allocation ou prise de mutex dans le callback audio ; les buffers partagés sont protégés avec le verrou du périphérique SDL côté producteur et réglages. Les changements d’état du menu, de pause, de modules et de joueur à terre pilotent le mélange local.
- Protocole réseau 7 et simulation inchangés. Les tests de la 0.8 ci-dessous décrivent cette base ; ils n’ont pas été relancés pour cet ajout audio. Écoute des transitions, nouvelle dépendance et coût sur Raspberry Pi restent à vérifier en jouant.

## Version 0.8 — 14 octobre 2026

- Compilation C11 avec `-O2 -Wall -Wextra -Wpedantic`, sans avertissement.
- `make test` réussi : protocole et paquets fragmentés, session à quatre joueurs, prédiction, maps et manches ; nouvelles phases du champion et projectiles roquettes transmis aux trois invités ; un invité réanime un autre invité, les invités restent synchronisés en montant sur les trois plateformes avec quatre mondes séparés, et la disparition du lanceur est confirmée sur tous les clients.
- Tests de réanimation : absence de retour automatique après douze secondes, aide maintenue, interruption par relâchement / distance / ligne de vue / dégâts, retour à 50 % de santé, protection et fin immédiate lorsque tous sont à terre.
- Tests des roquettes : prêt à tous, y compris un joueur à terre ; chargeur, projectile, dégâts à plusieurs ennemis, murs qui protègent, absence de dégâts directs aux alliés, impulsion au tireur et retrait après le round.
- Tirage rare déterministe de contrôle : 95 événements sur 1 000 rounds éligibles pour cette graine ; délai minimal entre événements vérifié. Ce résultat ponctuel ne modifie pas la règle de tirage de 8 %.
- Champion : préparation sans dégâts, phase aérienne réelle, hauteur supérieure à 1,5 m, impact unique, protection par un mur et esquive en hauteur.
- IA : recul de l’invocateur, approches latérales des traqueurs, exclusion des joueurs à terre, navigation distincte vers deux joueurs et sonde d’évitement.
- Plateformes : transport vertical et horizontal, saut depuis une plaque, absence d’enfermement lors de la descente ; positions d’un hôte et d’une prédiction locale concordantes sur quatre secondes avec deux mondes séparés.
- Snapshot rempli : 15 544 octets pour une limite de 24 576 ; phases de champion, réanimation, roquettes, explosions et pop-up conservés au décodage. Paquets tronqués / map invalide rejetés sans modifier le monde.
- Suites réseau, gameplay et mécanique exécutées avec AddressSanitizer et UndefinedBehaviorSanitizer, sans erreur signalée ; contrôle des fuites désactivé pour SDL.
- Rendu hors écran avec Mesa llvmpipe : menu, Réglages, pop-up d’arme / événement, modèle du lance-roquettes et rechargement, trois phases du champion, réanimation, vue à terre et game over. Captures inspectées ; capacités des maillages respectées.
- Démonstrations de 180 images terminées avec tirs et éliminations. Le rendu logiciel sur cette machine ne fournit pas une mesure des performances du Pi ; les mesures comparatives ponctuelles sont indiquées ci-dessous.
- Comparaison ponctuelle à scène fixe (cinq ennemis, même caméra, 30 images, `glFinish` après chaque image), sans compilation simultanée :

  | Profil / rendu logiciel | 0.7 | 0.8 |
  | --- | --- | --- |
  | Pi+ | 105,12 ms / image | 102,60 ms / image |
  | Pi | 53,99 ms / image | 52,75 ms / image |

  Les temps sont proches sur cet échantillon ; ils ne constituent pas une mesure de l’ARM, du GPU du Pi ou d’une partie à quatre joueurs. Les profils conservent leurs limites de rendu et aucune dépendance graphique n’a été ajoutée.

- `make test-tcp` tenté : sortie 77, `Operation not permitted` lors de la création de socket. L’environnement interdit les connexions réelles, y compris locales. Partie entre deux ordinateurs, comportement avec latence et performances sur Raspberry Pi restent à vérifier sur les machines des joueurs.
- Diff Git contrôlé ; protocole 7. Tous les joueurs doivent installer la version 0.8.

## Version 0.7 — 13 octobre 2026

- Compilation C11 avec `-O2 -Wall -Wextra -Wpedantic`, sans avertissement.
- `make test` réussi : encodage et messages fragmentés, quatre joueurs en mémoire, prédiction, transitions de manche, changement d’arène et rechargement transmis aux trois invités.
- Nouveaux tests : trois morts solo par projectile, vies et réapparition avec modules conservés, rechargement automatique / manuel, blocage des armes, déblocages aux rounds 10 / 20 / 30, rotation sur 32 rounds, points d’apparition et barils libres, navigation entre les apparitions sur les trois maps.
- Game over coopératif testé alors que tous les compteurs de retour sont déjà à zéro : la partie termine avant toute réapparition.
- Tests de sliders et saisie avec fenêtre SDL fictive : glissement, limites, virgule décimale, saisie au-delà de la borne ; modules bloqués pendant le délai et le tir maintenu, clics répétés sur une carte sans installation, bouton de confirmation distinct.
- Snapshot maximal (18 ennemis, 8 corps, 72 projectiles, 160 particules et quatre joueurs) : 14 473 octets pour une limite de 16 384. Lecture tronquée rejetée sans modifier la map ; chargeurs et rechargement conservés après décodage.
- Tests de gameplay et de réseau exécutés avec AddressSanitizer et UndefinedBehaviorSanitizer, sans erreur signalée. Détection des fuites désactivée pour ces lancements SDL.
- Shaders, menu, trois maps, quatre poses de rechargement, modules et game over rendus hors écran avec Mesa llvmpipe ; captures inspectées. Les quatre modèles d’armes restent sous 1 400 sommets pour une capacité de 3 200.
- Démonstration Pi+ / Moderne de 180 images : terminée, tirs et éliminations, compteur final de 29 FPS sur cette machine avec rendu logiciel. Ce chiffre ne mesure pas le Pi.
- Diff Git contrôlé. Connexion TCP réelle entre machines, comportement sous latence et performances Pi restent à vérifier sur les machines des joueurs ; aucun changement de VPN effectué.
- Protocole réseau 6 : tous les joueurs doivent mettre à jour leur jeu.

## Version 0.6 — 13 octobre 2026

- Compilation C11 avec `-O2 -Wall -Wextra -Wpedantic` réussie sans avertissement.
- Diff Git vérifié sans erreur d’espacement.
- Aucun test automatisé, lancement de jeu ou contrôle de shader / capture exécuté pour cette itération.
- Les profils Pi, Pi+, Équilibré et PC, le changement Moderne / Rétro, la composition du HUD et les exports BMP restent à observer en jeu.
- Les modifications graphiques n’ont pas encore été mesurées sur Raspberry Pi. Le retour positif de l’utilisateur concerne une version précédente.
- Protocole réseau 5 conservé ; coordonnées des collisions et règles de simulation conservées.

## Version 0.5 — 13 octobre 2026

- Compilation C11 avec `-O2 -Wall -Wextra -Wpedantic` réussie sans avertissement.
- Diff Git vérifié sans erreur d’espacement.
- Aucun test automatisé, test de gameplay, rendu exécuté ou contrôle réseau lancé pour cette itération.
- Les nouveaux shaders, cartes de modules, champions et choix en coopération doivent être observés en jeu sur les machines des joueurs.
- L’utilisateur rapporte un essai satisfaisant de la version précédente sur Raspberry Pi. Les nouveaux budgets graphiques n’ont pas encore été mesurés sur Pi ; aucun FPS réel n’a été fourni.

## Version 0.4

- Compilation C11 avec `-O2 -Wall -Wextra -Wpedantic` réussie sans avertissement.
- Aucun test de gameplay, de protocole ou de réseau exécuté pour cette itération ; les résultats ci-dessous concernent les versions précédentes.
- Les nouvelles invitations, la découverte UDP, les explosions, les effets répliqués et les difficultés restent à vérifier en jouant.
- Connexions réelles, copie du presse-papiers, aide Tailscale, performances et compilation sur Pi 4 restent à valider sur les machines des joueurs. Le script VPN a été créé, sans installer ni activer Tailscale sur cette machine.

## Version 0.3

- Compilation C11 et contrôles de protocole et coopération via `make test`.
- Transport en mémoire : un hôte et trois invités, messages divisés en fragments de 1 à 31 octets, arrivée dans le salon, statut prêt, lancement, commandes, états et manche suivante.
- Vérification de l’encodage explicite, des messages tronqués, des valeurs non finies et d’entrées aléatoires avec AddressSanitizer et UndefinedBehaviorSanitizer.
- Déplacements prédits et positions de l’hôte concordants dans la simulation sans délai ajouté.
- Contrôles de retour en jeu, de fin d’équipe et de cooldown des soins indépendant du nombre de joueurs.
- Régression des attaques, armes et esquive de la version 0.2 ; démonstration solo de 180 images avec profil Pi terminée, compteur de 30 FPS sur cette machine.
- Interface des salons rendue hors écran et inspectée.
- **La création de sockets est interdite par l’environnement d’exécution (`Operation not permitted`), y compris les sockets locales. Une connexion TCP réelle et une partie entre deux machines n’ont pas été vérifiées ici.** Le contrôle `make test-tcp` est fourni pour les vérifier hors de cet environnement.
- L’ARM, les conditions réseau réelles, les pertes de paquets et les performances du Pi restent à valider.

## Version 0.2

- Compilation C11 avec les mêmes avertissements activés : réussie, sans avertissement.
- Menu et démonstration de 270 images rendus avec le profil Pi et Mesa llvmpipe : terminé, armes utilisées, ennemis éliminés, compteur de 30 FPS sur cette machine.
- Capture rapprochée des trois silhouettes avec poses de marche et charge de l'invocateur : inspectée visuellement.
- Audio stéréo synthétique exécuté avec le pilote SDL fictif. L'écoute réelle reste à faire.
- Contrôles ciblés : délai avant l'attaque, projectile émis une seule fois par charge, frappe et soin, obstacles qui bloquent la frappe, interruption du tir pendant un changement d'arme, touche d'esquive maintenue sans répétition automatique et présence de la brute dès la vague 3.
- Poses des trois ennemis et des deux armes : coordonnées finies et respect de la capacité des maillages.
- Dix minutes simulées avec tir, saut, sprint, esquive et frappe : aucun dépassement des limites d'entités ni sortie de l'arène.
- Contrôles de simulation effectués avec AddressSanitizer et UndefinedBehaviorSanitizer. Détection des fuites désactivée pour la même raison que la version 0.1.

## Version 0.1

- Compilation C11 avec `-O2 -Wall -Wextra -Wpedantic` : réussie, sans avertissement.
- Menu et combat rendus hors écran avec SDL2 et Mesa llvmpipe ; shaders OpenGL ES compilés, captures inspectées.
- Démonstration de 180 images avec le profil Pi : terminée, première vague active, ennemis éliminés et compteur de 30 FPS sur cette machine.
- Synthèse audio exécutée avec le pilote SDL fictif : aucun fichier audio externe nécessaire. L'écoute sur une sortie audio réelle reste à faire.
- Contrôles de logique : marches, sprint contre les murs, matrices, deux armes, obstacles qui bloquent les tirs, apparition des vagues, mort du joueur et limites des tableaux.
- Dix minutes simulées de déplacement et combat avec AddressSanitizer et UndefinedBehaviorSanitizer : aucune erreur détectée. La détection des fuites a été désactivée car LeakSanitizer ne fonctionne pas sous le traçage imposé par cet environnement.
- Syntaxe du lanceur Bash et format du raccourci `.desktop` : valides.

La session graphique interactive de cette machine n'est pas accessible à l'environnement d'exécution. La capture réelle du clavier/souris et les changements de fenêtre restent à vérifier en jouant depuis le bureau.

**Pour les contrôles historiques de développement ci-dessus, aucun Raspberry Pi 4 réel n’avait été utilisé. Un essai satisfaisant sur Raspberry Pi a depuis été rapporté par l’utilisateur pour la version précédente.** Les résultats hors écran ne prouvent pas les performances sur Pi. La prochaine validation doit être une compilation ARM et une partie sur le Pi, en observant le compteur FPS et la température.
