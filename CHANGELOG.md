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
