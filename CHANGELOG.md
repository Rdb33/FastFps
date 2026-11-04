# Snapshot 1.3.3

- Escaliers : points d’accès adaptés au volume de chaque créature autour des marches étroites et rebords. Les ennemis peuvent contourner le socle central pour rejoindre son escalier, plutôt que pousser contre ses parois.

- Compilation Raspberry Pi OS : déclarations POSIX 2008 explicites, sans dépendre des options GNU fournies par SDL sur certaines distributions. Le journal de mise à jour utilise `fcntl(FD_CLOEXEC)` ; correction de l'erreur `O_CLOEXEC` non déclaré.
- Navigation A* à plusieurs hauteurs, passages classés selon le gabarit et la capacité à escalader, chemins mémorisés et tronçons marchables lissés. Coût des détours tenant compte des voisins et de leurs prochains déplacements réservés. Liaisons d'ascenseur avec paliers d'embarquement, attente, trajet et sortie ; attente estimée selon la position courante de la plateforme.
- Perception collective avec mémoire spatiale limitée : un joueur caché laisse sa dernière position observée, puis les créatures explorent des accès et croisements. Observation des vitesses, changements de direction, déplacements latéraux répétés et sauts ; anticipation des projectiles adaptée à ces mesures, sans modification d'une attaque déjà annoncée.
- Stratégies de groupe : pression de front, contournement, soutien à distance, couvert pendant recharge et recherche. Réévaluation en fonction du rôle, de la santé, des alliés présents, de la pression sur la cible et des positions déjà choisies par les autres créatures. Évitement local des roquettes visibles en approche, dans les limites du mouvement normal.
- Calcul sur l'hôte uniquement en coopération. Mémoire, graphes et chemins privés au monde, absents des snapshots ; mêmes états de créatures et protocole 12. Aucun réglage d'IA lié à la qualité graphique ; réinitialisation de la mémoire à la nouvelle partie et des positions observées au changement de map.

# Snapshot 1.3.2

- IA : choix de cible tenant compte de la hauteur et de la pression des autres ennemis, trajets vérifiés avec le gabarit réel après séparation, demi-tour possible dans une impasse et prudence au bord des plateformes. Les tireurs cherchent un angle découvert et privilégient un couvert latéral pendant leur recharge. Les invocateurs et lance-toiles vérifient obstacles et alliés avant de préparer puis libérer leur tir ; l'escalade ne contredit plus un recul à distance. Décisions autoritaires sur l'hôte, sans modification du protocole 12.

- Affinage des transitions : champ de vision de vitesse lissé et limité à cinq degrés supplémentaires, retour amorti de l'arme après atterrissage, impulsions de recharge alignées sur les clics existants et frappe à extension rapide / retour plus lent.
- Signal discret au retour de l'esquive et au changement d'arme, curseur de progression sur la recharge. Extinction progressive du flash de bouche, également sur les armes des alliés. Pas de son ajouté en doublon.

- Ressenti nerveux et lisible : retour amorti et légère torsion alternée de l'arme, réponse plus lourde du fusil à pompe et des roquettes, retour plus rapide de la mitrailleuse. Visée et simulation préservées.
- Flash de bouche qui se déploie puis se résorbe, noyau allongé et branches orientées selon le tir. Flash de touche progressif sur la peau et l'armure des ennemis vivants.
- HUD : marqueurs de touche convergents, accent supplémentaire pour une élimination, petit rebond des munitions et de la santé touchée, trace de santé perdue, signaux distincts de soin et de recharge terminée. Seuil de munitions basses proportionnel au chargeur.
- Éclats locaux sur les explosions proches dans le budget existant : 40 particules sur le profil Pi, 112 maximum ailleurs. Pas de nouvelle passe graphique, texture ou donnée réseau ; protocole 12 conservé. Réglages F4 et intensité pris en compte pour les nouveaux mouvements.

# Snapshot 1.3.1

- Notification : bouton Mettre à jour avec confirmation, téléchargement par `git pull --ff-only origin main`, compilation en arrière-plan puis relance via `lancer.sh` seulement après réussite. Boutons Copier commandes et Plus tard conservés, interactions adaptées au parallaxe.
- Refus sur une autre branche, en présence de modifications suivies ou avant d'avoir quitté le salon. Erreur affichée sans fermer le jeu si Git ou la compilation échoue ; journal local `build/mise-a-jour.log`, aucune réinitialisation des fichiers. Fermeture propre des commandes et sous-processus à l'arrêt du jeu.
- Protocole 12 et gameplay de la 1.3.0 conservés. La 1.3.1 doit être installée une première fois pour que le bouton soit disponible lors des futures publications.

# Version 1.3.0

- Confirmation avant de lancer ou recommencer en solo et avant le lancement du salon par l'hôte. Annuler sélectionné par défaut, Échap pour revenir, protection contre la touche maintenue et le double clic ; reprendre une partie ne demande pas de confirmation.
- Bombes du bombardier : chute de 3 à 2 s, recharge de 5 s conservée. La réanimation continue sous les dégâts tant que le soigneur reste debout, à portée et maintient E.
- Frappe fatale : onde de recul sur 4 m, bloquée par les murs, résistance accrue des gros ennemis, alliés préservés.
- Lance-toile dès la vague 21 : silhouette vert-gris / blanche, projectile préparé et évitable, immobilisation de 0,75 s laissant viser et tirer, immunité temporaire contre les blocages en chaîne. Autorité sur l'hôte et durée prédite chez les invités.
- Escalade de dalles basses et terrasses en 0,5 s pour traqueurs, invocateurs et lance-toiles ; animation de prise d'appui puis montée, cible ancrée aux dalles mobiles, contrôle de gabarit et plafond, interruption par les coups.
- Refonte des environnements : hall fermé cyan du Réacteur, couloirs de machines et fours de la Fonderie, ruines / falaises / terrasses du Bastion à ciel ouvert. Volumes, lignes de vue, accès aux hauteurs, matériaux, ciel, éclairage et coeur central distincts ; routes des plateformes conservées, barils dégagés des ascenseurs.
- Introductions de champions : première garantie, puis 20 % de chance lors d'une apparition éligible avec au moins quinze vagues d'intervalle. Présentation de 2,2 s, caméra dégagée des murs, nom, pose, bandes cinématiques et musique atténuée. Combat gelé pour l'équipe ; Espace pour passer après relâchement et délai de protection.
- Parallaxe par profondeur indépendant du réglage F4 : nouvel interrupteur du menu, activé par défaut à la lecture des anciens réglages. Douze lignes de réglages et zones de clic adaptées.
- Impacts avec traits convergents et éclair concentrés sur la cible, silhouette noir/blanc puis noir/rouge, restauration progressive. Durées nominales 110 / 170 ms ; délai partagé porté à 2 / 4 s pour réduire la répétition. Interface stable et réglage d'intensité conservés.
- Profil 1080P : rendu 1920 × 1080, objectif 60 FPS, sélection dans le menu / F2 ou --quality 1080p ; profils Pi conservés.
- Protocole 12 : positions horizontales et vitesses sur seize bits, hauteur des corps conservée en flottant complet, emplacements inactifs omis et seize particules cosmétiques maximum. 20 Hz conservés ; paquets réduits de 49 à 51 % dans les deux scénarios mesurés, sans compression lourde. État final prioritaire en cas de congestion ; message prêt tardif traité sans déconnexion. Mise à jour de toute l'équipe nécessaire.
- Compilation C11, suites gameplay / mécaniques / réseau virtuel réussies, connexion TCP réelle locale, captures OpenGL hors écran des maps / ennemi / cinématiques / impacts et rendu 1080p. Pas d'essai Pi ni de session réelle entre ordinateurs ; coût et équilibre restent à éprouver.

# Snapshot 1.2.3

- Impact frames composées en trois phases : négatif noir/blanc avec contours, positif noir/blanc pour les têtes ou noir/rouge pour les autres coups marquants, puis retour aux couleurs normales. Durées nominales de 160 / 220 ms, chaque phase affichée au moins une image ; intensité accrue et détails lisibles dans les ombres. Délai partagé 0,6 / 1 s ; interface conservée.
- Confirmations des têtes fatales fondées sur les compteurs de combat plutôt que sur la visée de l'image suivante. Interruptions fatales reconnues parmi les nouveaux corps. Délai cosmétique avant les modules porté de 0,22 à 0,3 s ; échéance autoritaire inchangée.
- Parallaxe de toute la composition du menu : fond, grille, carte d'informations, commandes, titre et réacteur déplacés selon leur profondeur ; décor 3D en arrière-plan, accueil / pause / fin de partie / réglages / salons / fenêtre de mise à jour. Zones de clic, saisies et sliders suivent les commandes ; conversion commune pour les fenêtres avec bandes noires.
- Visualiseur : filtres deux fois plus sélectifs, échelle logarithmique avec plancher et réponse plus rapide, segments lumineux et témoins de crête. Suppression des colonnes de fond qui semblaient pleines au silence. Volume et fondus conservés, aucune musique ajoutée au dépôt.
- F4 coupe la phase inversée et le parallaxe, conserve un contraste positif et réduit les barres ; intensité zéro coupe les impact frames. Protocole 11 et simulation conservés, compatibilité avec les versions 1.2.0 à 1.2.2. Aucune dépendance, texture ou passe de rendu supplémentaire.
- Compilation C11 sans avertissement, captures OpenGL hors écran, contrôles temporaires des déclenchements et phases, interactions avec parallaxe, analyse de signaux et extraits musicaux locaux, encodage / décodage réseau. Aucun essai Pi ni connexion réelle entre machines ; coût GPU et confort à confirmer en partie.
- À la publication, suite des mécaniques réussie ; deux échecs préexistants des suites réseau / gameplay, reproduits dans la 1.2.2, restent ouverts. Détails dans VALIDATION.md.

# Snapshot 1.2.2

- Parallaxe à la souris : titre, réacteur et panneau décoratif à plusieurs profondeurs, déplacement léger du décor 3D dans l'accueil et les salons, interpolation et retour au centre à la perte du survol. Zones de clic et commandes fixes.
- Secousses locales amorties de 0,12 à 0,28 s pour les coups marquants, dégâts, explosions proches, frappes au sol des champions, réacteur, vagues, déblocages et événements de roquettes. Amplitude plafonnée, atténuation par distance et priorité à la secousse la plus forte ; visée de gameplay et interface conservées.
- Visualiseur musical à douze bandes dans le menu, les salons et le HUD : analyse stéréo après fondus / volume / filtre / écho, avant les bruitages, sans animation simulée au silence. Filtrage évité au silence ; copie bornée des niveaux sous verrou SDL hors du callback.
- F4 désactive parallaxe et secousses, ralentit et réduit les barres. Intensité des impacts pilote aussi les secousses ; réglage par flèches gauche / droite corrigé.
- Protocole 11 conservé, compatibilité de gameplay avec 1.2.0 / 1.2.1 ; aucun changement des paquets, des statistiques ou du RNG de gameplay. Pas de nouvelle dépendance, texture ou passe de rendu.
- Compilation C11 sans avertissement et revue de code. Aucun test ni essai de rendu, audio réel, coopération ou Pi exécuté ; coût du visualiseur et confort des mouvements à mesurer en partie.

# Snapshot 1.2.1

- Impact frames locales : cyan pour une tête fatale, orange pour une interruption, violet pour des éliminations alignées avec Perforation, contraste noir et blanc pour le champion. Durées de 45 / 60 ms, délai partagé et priorité aux événements majeurs ; interface composée après les effets.
- Arrêt visuel jusqu'à 40 ms sur la pose de l'arme et les particules locales ; simulation et réseau continuent. Recharge et changement d'arme prioritaires ; F4 supprime cet arrêt.
- Armures à trois éléments, plaques du torse perdues à 70 / 35 % de santé, épaulières des brutes et fissures lumineuses des champions ; débris colorés et onde dorée à la mort d'un champion. Collisions et statistiques conservées.
- Poussière sur le sol, étincelles et copeaux sur les structures ; marques temporaires suivant les plateformes mobiles. Traînées d'esquive aux couleurs des joueurs, poussière de réception et impulsion verte du réacteur sur l'arène.
- Effets d'armes en première et troisième personne : Assaut orange, Alternance dorée, Perforation violette, Surtension rose et témoins verts du réacteur.
- Réglage « Intensité des impacts », slider et saisie 0–100 %, défaut 50 %, persistance compatible avec les anciens réglages. Menu adapté à onze lignes ; coordonnée d'une fixture de saisie existante ajustée au nouvel espacement.
- Présentation des modules différée de 0,22 s pour laisser apparaître la dernière confirmation ; délai autoritaire de dix secondes et protection contre les clics conservés.
- Protocole 11 conservé et états sérialisés inchangés : compatibilité de gameplay avec 1.2.0. Effets déduits localement des états reçus, RNG cosmétique séparé ; limites propres au Pi, aucune allocation par image, texture ou passe de rendu supplémentaire.
- Compilation C11 et revue de code. Aucun test ni essai en partie ou sur Pi exécuté pour ce snapshot ; coût réel, lisibilité et effets en coopération restent à observer.

# Version 1.2.0

- Attaques lourdes coordonnées par cible, délai entre menaces et vérification des zones voisines pour espacer les bombes et sauts de champion.
- Réserves coopératives et arrivées progressives : jusqu'à 54 ennemis par vague, 18 actifs maximum, groupes de trois espacés de deux secondes et positions libres à distance des joueurs. Bonus de PV coopératif réduit de 15 à 10 % par joueur supplémentaire.
- Difficulté progressive : réserve solo / coopérative dès la vague 9, jusqu’à 12 ennemis de plus ; PV +5,5 % de base par vague, plafond ×4,25 ; dégâts jusqu’à +40 % à la vague 31 (+25 % en Découverte), projectiles des invocateurs de 14 à 16 m/s. Délais des bombes et préparations d’attaque conservés.
- Directions, distances et étages des deux derniers ennemis lorsque la réserve est épuisée. Indicateur de dégâts à quatre directions, arcs et flèches de 1,15 s, distinct du flash de dégâts.
- Intermission de 10 secondes, protection contre les clics accidentels et confirmation conservées.
- Réacteur central : 12 éliminations pour charger, E sur le socle, +15 PV à l'équipe debout, vitesse +15 % et vitesse de recharge +25 % pendant 15 s ; recharge de 45 s. Réanimation prioritaire.
- Quais : bonus individuels alternant bouclier 30 / 12 s, prochain tir +65 % / 12 s, vitesse +20 % / 8 s. Retour en 30 s, emplacement adapté à chaque map, modèles géométriques animés, état et délais affichés.
- Dix modules : Précision (munitions sur tête fatale), Assaut (dégâts après esquive), Alternance (premier tir après changement) et Perforation (une seconde cible, 45 à 85 % des dégâts) ajoutés aux six existants ; cinq rangs.
- Défis facultatifs : aucune mise à terre, trois attaques légères interrompues, champion en moins de 25 s. Récompense commune de 400 + 25 × vague points, bouclier 15 / 12 s et célérité 8 s ; pas de pénalité d'échec.
- Bonus temporaires suspendus entre les vagues pour conserver les récompenses pendant le choix des modules ; résultat des défis affiché dans ce choix.
- Protocole 11 : états explicitement encodés et bornés. Résolution des objectifs, réserves et bonus sur l'hôte, prédiction des déplacements et représentation côté invité. Tous les joueurs doivent mettre à jour le jeu.
- Compilation C11 et revue de code ; aucun test ni essai en partie exécuté pour cette version. Équilibrage, affichage, connexion entre machines et performances Pi restent à essayer. Pas de dépendance ou passe de rendu ajoutée.

# Version 1.1.1

- Vérification de version une fois au démarrage, sur un thread dédié : lecture HTTPS de la version publiée sur GitHub, avec curl facultatif, réponse limitée et délais courts. Démarrage et jeu hors ligne conservés.
- Fenêtre en français uniquement si une version stable plus récente est disponible ; versions installée / proposée, explications, copie d'une commande de mise à jour et bouton Plus tard.
- Notification différée jusqu'à un menu si le joueur est déjà en combat ; délai et relâchement des boutons contre les clics accidentels. Aucun téléchargement ou lancement automatique d'une commande de mise à jour.
- Commande copiée adaptée au chemin de l'installation, avec protection des espaces et apostrophes, sans retour à la ligne final. Consigne de coller sans valider, fermer le jeu puis appuyer sur Entrée.
- Option `--no-update-check` ; aucune vérification en démonstration ou lancement limité par `--frames`. Annulation du téléchargement et attente du thread à la fermeture.
- Protocole 10 et gameplay 1.1.0 conservés ; compatibilité coopérative entre 1.1.0 et 1.1.1. Pas de nouvelle bibliothèque à la compilation ni de passe de rendu supplémentaire.
- Compilation C11 et revue du diff ; tests, requête HTTPS réelle, affichage et essais Raspberry Pi non exécutés pour cette version.

# Version 1.1.0

- Fusil à pompe à la vague 11, mitrailleuse à la 21 et lance rail à la 31 ; notifications et disponibilité partagées avec la coopération.
- Bombardier dès la vague 11 : trois par vague contre deux brutes, 220 PV de base contre 260, silhouette ambrée avec visière et générateurs lumineux.
- Bombes à portée de 48 m, cible visible et position fixe ; chute de 3 s, puis recharge de 5 s. Cercle de danger de 3,5 m, compte à rebours, alerte en français, explosion avec dégâts de zone diminuant selon la distance et bloqués par les murs.
- Vampirisme réduit de +2 à +1 PV par élimination et par rang, en solo comme en coopération.
- Étiquettes des alliés plus contrastées, numéro J1–J4, pseudo, couleur et pointe au-dessus de la tête ; portée de visibilité augmentée à 48 m, occlusion conservée.
- Marche fondée sur le déplacement réel, foulées par silhouette, coudes / genoux articulés et mouvement du torse. Préparation / frappe / récupération calées sur le moment des dégâts ; charge spécifique du bombardier.
- Protocole 10 : bombes, positions, cibles et délais répliqués. Décisions et dégâts uniquement sur l'hôte ; mise à jour de tous les joueurs nécessaire. Aucune nouvelle dépendance ou passe de rendu.
- Compilation C11 et revue de code. Fixture de snapshot des armes adaptée ; tests, partie réelle et mesures Raspberry Pi non exécutés pour cette version.

# Version 0.9.3

- Trajets et quais spécifiques : traversée latérale du Réacteur ; ascenseurs nord / sud et convoyeur longitudinal de la Fonderie ; accès inclinés et navette en arc du Bastion. Hauteurs, pauses, phases de départ, repères et soins adaptés à chaque map.
- Définition partagée des trajectoires entre collisions, prédiction et affichage. Coéquipiers embarqués interpolés par rapport à la plateforme pour rester visuellement dessus.
- Menu : entrée progressive, sélection fluide, éclairage des cartes et réacteur animé ; indicateur de connexion. Zones d’interaction fixes.
- Recharge par étapes et selon l’arme : chargeur / cellule, paires de cartouches, roquette, mains et réarmement. Clics mécaniques synchronisés, durées et munitions inchangées.
- Mouvement d’arme adapté à la course / au saut, changement adouci, roulis de caméra léger désactivable avec F4.
- Genoux des ennemis articulés, apparitions adoucies, pieds au niveau du sol ; chute et relèvement progressifs des alliés et phases de recharge communes.
- Notifications glissées, cartes de modules stables avec barre de déverrouillage et anneaux de soins pulsants.
- Protocole 9 : mise à jour de tous les joueurs requise. Format de gameplay conservé ; buffers, cinq appels de dessin et dépendances existants conservés.
- Compilation C11 et revue de code ; tests, rendu en partie et mesures Pi non exécutés pour cette version.

# Version 0.9.2

- Deux balcons avec couverts sur chaque map, desservis par les ascenseurs ; navette élevée reliant les balcons sur environ 31 mètres. Arrêts aux paliers, mouvement adouci, repères bleus / orange et sens de circulation.
- Deux postes de soins en hauteur, 25 PV / retour après 30 s ; collecte selon la hauteur du joueur et rendu placé sur le bon étage.
- Conservation de l’élan au saut depuis la navette et impulsion supplémentaire au saut d’un ascenseur montant, partagées par l’hôte et la prédiction.
- Modèles des alliés : armures et casques détaillés, couleurs et numéros, cinq silhouettes d’armes, marche / saut / visée / recul / recharge / changement / frappe / aide ; posture à terre à taille normale.
- Noms et santé à l’écran au-dessus des alliés visibles, avec état à terre et progression de réanimation ; couleurs communes au modèle et à la liste de l’équipe. Lissage de la visée et du mouvement des alliés sur les invités.
- Protocole 8 : mise à jour de tous les joueurs requise pour les nouvelles collisions et trajectoires ; format de snapshot conservé. Pas de nouvelle dépendance ou d’appel de dessin.
- Fixtures des contrôles de plateformes existants adaptées aux nouvelles coordonnées et durées. Compilation C11 ; tests et mesures Pi non exécutés pour cette version.

# Version 0.9.1

- Petit rappel non bloquant de huit secondes quand on joue avec le volume musical à zéro : « Tu joues sans musique / Échap > Réglages > Musique ». Une seule fois par partie, en solo comme en coopération ; masqué pendant la pause, le choix de modules et les écrans de progression, retiré dès que la musique est activée. Pas de rappel en démo ou avec `--mute`.
- Volume musical inchangé ; aucun changement du protocole réseau 7.

# Version 0.9.0

- Blank Shell dans le menu / les salons et General Release en jeu, copiées depuis le dossier Musique du PC.
- Volume musical indépendant de 0 à 100 %, slider, saisie numérique et clavier ; zéro par défaut et persistance compatible avec les anciens réglages.
- Fondus, transition entre morceaux, boucles adoucies, filtre et atténuation en pause / à terre, léger écho et baisse sous les bruitages.
- Lecture MP3 en continu avec mpg123, buffers fixes et décodage hors du callback SDL ; nouvelle dépendance `libmpg123-dev`.
- Aucun changement de gameplay ou de protocole réseau (7).

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
