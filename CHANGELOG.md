# Version 0.8.0

- Champions : préparation annoncée, cible fixée, saut avec collisions, choc unique au sol et récupération. Murs et hauteur protègent du choc.
- IA : choix de cible tenant compte de la visibilité, hystérésis, itinéraires par joueur, flancs, recul / déplacement latéral des invocateurs, tir anticipé, séparation et sortie des blocages.
- Menu principal avec grandes actions, page Réglages séparée, raccourcis et informations de partie.
- Coopération : remplacement du retour automatique par E maintenu 2,5 s à moins de 2,8 m ; dégâts, portée et ligne de vue interrompent l’aide. Réanimation à 50 % de santé et deux secondes de protection. Game over si tous sont à terre.
- Cartes de déblocage d’arme et d’événement pendant six secondes.
- Événement rare pour un round : lance-roquettes offert et équipé à tous, touche 5, projectiles et dégâts de zone avec occlusion, impulsion de rocket jump pour le tireur et retrait du lanceur après le round.
- Deux élévateurs et une navette sur chaque map ; transport des personnages et corps, dégagement lors de la descente, prédiction et reconstruction depuis le temps de l’hôte.
- Protocole 7, incompatible avec les anciennes versions ; limite de paquet 24 Ko. Simulation des nouvelles règles réservée à l’hôte.
- Tests de gameplay, mécanique et session virtuelle à quatre joueurs ; vérification mémoire et rendu hors écran. TCP réel et Pi restent à valider sur les machines des joueurs.

# Version 0.7.0

- Luminosité réglable de 50 à 200 %, sensibilité de 0,30 à 3,00 : sliders, saisie directe, limites, validation et sauvegarde.
- Trois vies solo, retour après trois secondes avec progression conservée, écran de game over après la troisième mort.
- Game over collectif en coopération dès que tous les joueurs sont morts ; retour au salon et relancement par l’hôte.
- Trois layouts jouables : Réacteur, Fonderie et Bastion ; changement tous les cinq rounds, navigation et collisions reconstruites, réapparition sûre de l’équipe.
- Vagues jusqu’à 18 ennemis, progression de résistance accélérée, déplacements et projectiles plus rapides, soins moins abondants.
- Modules : blocage initial d’une seconde, relâchement du tir et confirmation explicite par Entrée ou un bouton distinct des cartes ; nom du module installé dans le HUD.
- Chargeurs indépendants, R pour recharger, rechargement automatique à vide, blocage du tir / changement d’arme pendant le geste ; modèles et mains animés.
- Arsenal progressif : Pulse au départ, fusil à pompe au round 10, mitrailleuse au round 20, lance rail au round 30 ; molette limitée aux armes disponibles et liste dans Tab.
- Protocole réseau 6 : maps, chargeurs, rechargements et verrouillage des modules validés par l’hôte ; mise à jour de tous les joueurs nécessaire.
- Tests de gameplay et de quatre joueurs en mémoire, encodage maximal sous 16 Ko, contrôles de saisie et rendu hors écran. Performances Pi et connexion réelle restent à confirmer.

# Version 0.6.0

- Arène enrichie : panneaux industriels, conduites, traverses, grilles, caisses renforcées et supports de réacteur.
- Géométrie biseautée, primitives cylindriques, modèles plus détaillés et normales des diamants corrigées.
- Matières procédurales, surfaces émissives, deux sources de lumière de combat et éclairage par pixel sur les profils avancés.
- Mode Moderne avec contraste, agrandissement lissé et bloom léger ; mode Rétro avec éclairage plus simple et filtre sans lissage.
- Nouveau profil Pi+ à 854 × 480 / cible 30 FPS / 80 particules ; profil Pi conservé à 640 × 360.
- F5 pour changer de style, réglage persistant, options --modern / --retro / --quality pi-plus.
- Interface rendue après l’agrandissement pour conserver des textes nets et éviter le bloom sur le HUD.
- Export BMP composé avec les effets et l’interface dans une cible temporaire.
- Cinq appels de dessin par image conservés ; protocole 5 et collisions compatibles avec la 0.5.
- Compilation C11 réussie sans avertissement ; nouveaux shaders et performances en jeu restent à valider sur les machines des joueurs.

# Version 0.5.0

- Progression limitée à la partie : un module à choisir après chaque vague, niveau de personnage et six branches à cinq rangs.
- Surcharge, Cadence, Réflexes, Vitalité, Blindage et Vampirisme ; effets réels sur les armes, la frappe, les déplacements et la survie.
- Cartes animées, choix au clavier ou à la souris, panneau de configuration avec Tab, intermission de 15 secondes et sélection automatique à expiration.
- Propositions individuelles en coopération, choix validés par l’hôte, conservation des modules après réapparition et remise à zéro entre les parties.
- Champions dorés toutes les cinq vagues, attaque de zone annoncée au sol, bonus de score et secteurs aux ambiances lumineuses distinctes.
- Réacteur animé, anneaux visibles des deux côtés, particules d’ambiance, ombres stylisées et éclairage des tirs / explosions sur les sommets.
- Détails et évolution visuelle des armes, flashs de bouche étoilés, douilles, respiration et mouvement d’esquive.
- Points d’élimination animés et résumé de fin de partie.
- Budgets de particules : 64 sur Pi, 100 en Équilibré, 160 sur PC ; cinq appels de dessin conservés.
- Protocole réseau 5, incompatible avec les versions précédentes.
- Compilation C11 réussie sans avertissement ; gameplay, nouveau rendu et coopération de cette version restent à vérifier en jouant.

# Version 0.4.0

- Découverte automatique de trois salons sur le réseau local, sans service central.
- Invitations `fastfps://IP:port`, copier/coller natif, adresses VPN prioritaires et mémorisation des champs.
- Invités prêts automatiquement, ping affiché dans le salon et en jeu, aide réseau avec H.
- Assistant Linux / Pi `reseau.sh` : installation et connexion Tailscale à la demande, état et lancement des salons.
- Six barils explosifs, réactions en chaîne, explosions bloquées par le décor et réapparition entre les vagues.
- Tirs à la tête, dégâts doublés, bonus de score et confirmations distinctes.
- Direction des dégâts, alerte de santé basse, santé des alliés et compte à rebours des vagues.
- Difficultés Détente / Normal / Brutal, choix de l'hôte et adaptation des vagues à l'équipe.
- Effets de tirs et explosions transmis aux invités, avec respect du budget de particules local.
- Repères industriels et recharge visible des soins ; rendu conservé à cinq appels de dessin.
- Protocole réseau 4, incompatible avec la version 0.3.
- Compilation réussie sans avertissement ; cette itération n'a pas fait l'objet de tests de gameplay ou réseau.

# Version 0.3.0

- Salons coopératifs jusqu’à quatre joueurs : création, connexion par IPv4, pseudos, joueurs prêts et lancement par l’hôte.
- Simulation commune des joueurs, ennemis, soins, dégâts et score d’équipe.
- Prédiction du déplacement des invités, confirmation des commandes et interpolation des autres joueurs et ennemis.
- Alliés visibles avec casque et armures de couleurs distinctes.
- Retour après cinq secondes si un allié reste en vie ; fin de manche commune et relancement depuis le salon.
- Sockets TCP non bloquantes, messages encodés explicitement pour ARM et x86, limite des buffers et vérification des versions.
- Options `--host`, `--join`, `--port` et `--name`.
- Contrôles du protocole et de quatre joueurs avec transport en mémoire ; contrôle TCP réel disponible via `make test-tcp`.

# Version 0.2.0

- Trois ennemis : traqueur rapide, invocateur à distance et brute résistante dès la vague 3.
- Silhouettes et couleurs distinctes, poses articulées, préparation des attaques, réactions aux impacts et morts animées.
- Recul, mains, balancement, inertie de visée, réarmement du fusil et changement d’arme.
- Esquive au clic droit / Alt gauche, frappe avec F et soins sur élimination au corps à corps.
- Séries d’éliminations avec multiplicateur de score.
- Sons de pas, atterrissage, pompe et attaques ; placement stéréo des ennemis.
- Option F4 pour réduire les mouvements de caméra, compatible avec les anciens réglages sauvegardés.
- Budget de rendu conservé : cinq appels de dessin et tableaux à taille limitée.
