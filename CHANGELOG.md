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
