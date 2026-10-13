# Contrôles de FASTFPS

Historique des contrôles sur la machine de développement, les 12 et 13 octobre 2026.

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
