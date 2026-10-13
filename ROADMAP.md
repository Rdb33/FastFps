# Évolution de FASTFPS

Direction choisie : **FPS style Doom, application native, cible Raspberry Pi 4**.
Préférences : style géométrique expressif, ennemis différents, multijoueur envisageable ensuite.

## Base 0.1

- [x] Application C / SDL2 / OpenGL ES 2, compilation locale.
- [x] Arène industrielle, plateforme, sprint, saut et contrôle en l’air.
- [x] Blaster et fusil à pompe.
- [x] Créatures au contact et à distance, vagues, soins et score.
- [x] Pause, réglages, plein écran et compteur FPS.
- [x] Profil Pi 4 : 640 × 360, limite 30 FPS, effets limités.
- [ ] Mesures sur un vrai Pi 4 : FPS, temps d’image, température et mémoire.

## Version 0.2

- [x] Traqueur rouge, invocateur violet et brute verte, avec silhouettes différentes.
- [x] Marche articulée, respiration, attaques annoncées et animations de mort.
- [x] Mains visibles, recul, réarmement du fusil et changement d’arme.
- [x] Esquive, frappe au corps à corps et bonus de série.
- [x] Pas et sons d’ennemis placés en stéréo.
- [x] Réglage de réduction des mouvements de caméra.

## Prochaine itération

- Ajuster le mouvement et les dégâts après une première partie.
- Affiner les modèles géométriques et leurs animations.
- Ajouter une difficulté réglable.
- Choisir entre survie en arène et niveaux avec progression.

## Contenu

- Niveaux avec portes, clés et sortie.
- Lance-roquettes et dégâts de zone.
- Mini boss et nouveaux comportements d’ennemis.
- Munitions et armure si elles renforcent le rythme souhaité.
- Musique originale.

## Architecture selon les besoins

- Chargement des cartes depuis des fichiers.
- Configuration des touches et manette.
- Mesures détaillées et résolution adaptative.
- Packaging pour Raspberry Pi OS.
- Étudier le lancement sans bureau après validation sur Pi.

## Version 0.3 : coopération

- [x] Salons créés par un joueur, jusqu’à quatre joueurs.
- [x] Connexion par IPv4 et port TCP, joueurs prêts et lancement commun.
- [x] Hôte faisant autorité, prédiction des invités et interpolation.
- [x] Ennemis et score partagés, alliés visibles et retour après élimination.
- [x] Contrôles du protocole, des fragments et des manches en mémoire.
- [ ] Connexion TCP réelle, partie entre deux machines et validation Internet.
- [ ] Mesurer latence, débit et charge sur le Pi 4.

## Suite du multijoueur

- Améliorer la prédiction selon les mesures sur Internet.
- Évaluer UDP pour réduire les retards lors de pertes de paquets.
- Répliquer davantage d’effets de combat et ajouter un indicateur de latence.
- Connexion par code de salon et relais, si un service central est souhaité.
- Arrivée en cours de manche et transfert du rôle d’hôte.
- Étudier un mode d’affrontement entre joueurs après la coopération.

Les personnages jouables restent identiques pour cette version ; la variété demandée concerne les ennemis.

## Version 0.4 : combat et connexion simplifiée

- [x] Salons LAN automatiques, invitations à copier/coller et aide VPN.
- [x] Prêt automatique, latence, préférences réseau et particules répliquées.
- [x] Barils explosifs, tirs à la tête, direction des dégâts et HUD enrichi.
- [x] Trois difficultés et vagues adaptées à la coopération.
- [x] Repères visuels dans l'arène et recharge visible des soins.
- [ ] Parties réelles entre deux machines, avec et sans VPN.
- [ ] Ajuster l'équilibrage et mesurer les performances sur Pi.

## Version 0.5 : progression pendant la partie

- [x] Six modules à cinq rangs, choix entre les vagues et panneau de configuration.
- [x] Choix individuel en coopération et validation par l’hôte.
- [x] Champions toutes les cinq vagues et changement d’ambiance du secteur.
- [x] Réacteur animé, ombres stylisées, lumières de combat et armes évolutives.
- [x] Statistiques et points affichés pendant la partie.
- [ ] Jouer plusieurs configurations pour ajuster les modules et les champions.
- [ ] Mesurer la 0.5 sur Pi et observer les choix simultanés en coopération.
