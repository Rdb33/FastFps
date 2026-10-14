# Salons coopératifs — version 0.9.2

Un joueur héberge un salon sur son ordinateur ; jusqu’à trois amis le rejoignent. L’hôte lance la partie quand les joueurs sont prêts. Le jeu utilise une **adresse IPv4 et un port TCP**, sans service central ni frais d’hébergement.

Les joueurs doivent utiliser la même version du jeu et la même carte. **Tous les ordinateurs doivent être en 0.9.2** pour les nouveaux balcons, ascenseurs et navettes. Le mouvement des plaques et l’élan au saut restent simulés par l’hôte puis prédits localement. Les couleurs, numéros, armes, poses et étiquettes des alliés sont dérivés des états répliqués.

## Depuis le menu

1. Lance `./lancer.sh` et sélectionne **Multijoueur / salons**.
2. Renseigne ton **pseudo**. Clique sur le champ puis saisis le texte ; la première saisie remplace la valeur existante. `Entrée` termine la saisie.
3. L’hôte choisit **Créer un salon**. Le port par défaut est **27015**.
4. Sur le même réseau, les invités cliquent sur le salon détecté automatiquement. Sinon, l’hôte clique sur **Copier l’invitation** (ou `Ctrl+C`) et la transmet à ses amis.
5. Les invités ouvrent les salons, collent avec `Ctrl+V`, puis appuient sur `Entrée`. L’adresse et le port sont récupérés automatiquement. Ils sont prêts dès leur arrivée ; le bouton permet de se remettre en attente.
6. L’hôte sélectionne **Lancer la partie**. La difficulté choisie par l’hôte s’applique à toute l’équipe.

Le salon affiche les joueurs, les places libres et leur statut. Une tentative d’arrivée pendant une manche est refusée ; rejoins avant le lancement ou entre deux manches.

## Depuis le terminal

Sur l’ordinateur de l’hôte :

```bash
cd ~/Projets/fastfps
./lancer.sh --host --name Alice
```

Chez un invité, en remplaçant l’adresse de l’exemple :

```bash
cd ~/Projets/fastfps
./lancer.sh --join 192.168.1.20 --name Bob
```

Ces commandes ouvrent directement le salon ; les invités sont prêts automatiquement et l’hôte lance depuis le bouton. `--join` accepte aussi `IP:port` ou une invitation `fastfps://IP:port`. Le pseudo, l’adresse et le port sont mémorisés à la fermeture normale.

Pour choisir un autre port, ajoute `--port 27016` **sur les deux ordinateurs**.

## Sur le même réseau

La liste des salons se remplit automatiquement et se rafraîchit toutes les deux secondes. La découverte utilise **UDP 27014**, tandis que la partie reste sur **TCP 27015** (ou le port choisi). Un pare-feu, un Wi-Fi invité isolant les appareils ou un réseau bloquant les diffusions peuvent empêcher la détection ; l’invitation IPv4 reste utilisable si la connexion TCP est autorisée. Trois salons peuvent être affichés.


L’hôte peut consulter ses adresses avec :

```bash
hostname -I
```

Partage l’adresse du réseau utilisé, généralement `192.168.x.x` ou `10.x.x.x`. Un pare-feu actif doit autoriser le port TCP choisi sur l’hôte.

`127.0.0.1` sert uniquement à rejoindre un autre lancement du jeu sur **le même ordinateur**. Pour jouer entre deux ordinateurs, utilise l’adresse du réseau ou du VPN.

## Sur Internet : parcours conseillé avec Tailscale

Sur chaque ordinateur Linux ou Raspberry Pi :

```bash
./reseau.sh vpn
```

Cette commande installe Tailscale si nécessaire avec son installateur officiel, puis lance la connexion au compte ; elle peut demander `sudo`. Elle ne redirige aucun port de la box. Suis le lien affiché pour te connecter, puis [invite ton ami dans ton réseau privé](https://tailscale.com/docs/features/sharing/how-to/invite-any-user), avec son propre compte. Voir l’[installation officielle Linux / Raspberry Pi OS](https://tailscale.com/docs/install/linux). Sur un autre système, utilise le [client adapté](https://tailscale.com/download).

Activez le VPN **avant** de créer le salon. Le jeu détecte les adresses IPv4 locales et donne priorité aux interfaces VPN, notamment Tailscale. L’hôte copie ensuite l’invitation depuis le salon ; l’ami la colle dans FastFPS. `H` ouvre l’aide dans le jeu. `./reseau.sh etat` affiche le statut et l’adresse Tailscale sans modifier la configuration. Si plusieurs VPN sont présents, utilise explicitement l’IPv4 du VPN partagé par les joueurs.

Les invitations sont du texte à coller dans le jeu, pas des liens enregistrés dans le système. Elles contiennent une adresse et un port ; elles ne fournissent pas de traversée automatique des box. La recherche LAN ne traverse généralement pas Tailscale : utilise l’invitation à distance. Aucune IP publique n’est recherchée automatiquement.

Tailscale utilise son service de coordination et éventuellement des relais ; la latence peut alors augmenter. Un pare-feu sur l’hôte doit autoriser le port TCP du jeu sur le VPN. [Fonctionnement des connexions](https://tailscale.com/docs/reference/connection-types).

## Autres possibilités sur Internet

Deux possibilités pour cette première version :

- **Connexion directe** : l’hôte partage son adresse IPv4 publique et configure sa box pour rediriger le **port TCP 27015** vers l’adresse locale de son ordinateur. Le port choisi doit aussi être autorisé par son pare-feu.
- **VPN privé reliant les joueurs** : les invités utilisent l’adresse IPv4 du VPN de l’hôte. Le VPN peut lui-même utiliser des services de connexion ou de relais ; le jeu ne gère pas ces services.

Si la connexion de l’hôte est derrière un NAT d’opérateur (CGNAT), une redirection dans sa propre box peut ne pas suffire. Il faut alors une adresse publique accessible, un VPN approprié ou, plus tard, un relais intégré. Le jeu n’effectue pas de traversée automatique des box. La version 0.8 fournit une invitation contenant l’IPv4 et le port, sans code de salon ni relais intégré.

## Jouer ensemble

- Les joueurs combattent les mêmes ennemis, avec les mêmes vagues, soins et **score d’équipe**.
- Les alliés ont des silhouettes avec casque et armure colorée, différentes des créatures. Leurs noms et leur statut apparaissent dans l’interface.
- Pas de dégâts directs entre alliés. Les explosions des barils sont un danger pour toute l’équipe, y compris le tireur.
- Si un joueur tombe, il reste à terre jusqu’à sa réanimation. Un allié doit être à moins de 2,8 mètres, avec une ligne de vue libre, et maintenir **E pendant 2,5 secondes**. Interrompre l’aide, sortir de portée ou subir des dégâts annule la progression. La santé revient à 50 % du maximum, avec **2 secondes de protection**, en conservant modules et chargeurs.
- Si toute l’équipe tombe simultanément, la partie s’arrête sur un écran de game over. Choisis Retour au salon ; l’hôte peut relancer une partie. Aucune réanimation ne peut se terminer lorsque toute l’équipe est à terre.
- `Échap` libère ta souris et ouvre ton menu : **la partie des autres continue**. Sélectionne **Reprendre** pour revenir.
- Quitter le salon en tant qu’invité retire uniquement cet invité. Fermer le salon de l’hôte déconnecte tout le monde. Il n’y a pas encore de transfert automatique du rôle d’hôte.

## Modules et progression en coopération

Chaque joueur reçoit ses propres propositions après une vague terminée et sélectionne son module avec les touches 1 / 2 / 3 ou un clic, après une seconde de blocage et le relâchement du tir. Entrée ou le bouton Valider confirme le choix. La sélection est envoyée à l’hôte, qui applique le bonus et confirme le nouvel état. Les autres joueurs continuent à voir la même arène pendant cette intermission.

Le choix dure au maximum 15 secondes ; la première proposition est sélectionnée automatiquement si nécessaire. Quand toute l’équipe a choisi, il reste trois secondes avant la vague suivante. Les modules restent installés après une réanimation. Relancer une partie depuis le salon remet toutes les améliorations à zéro.

Les champions apparaissent toutes les cinq vagues pour toute l’équipe. La map change pour tous au début des rounds 6, 11, 16, etc., avec reconstruction locale des collisions et du décor. Les armes se débloquent aux rounds 10, 20 et 30 ; chargeurs et rechargements sont suivis séparément par joueur et confirmés par l’hôte. Les points restent partagés, tandis que la santé, les modules, les éliminations personnelles et les impacts à la tête sont suivis par joueur.

## Champions, plateformes et événement rare

L’hôte choisit la cible du saut, simule le déplacement du champion et applique le choc au sol. Chaque invité reçoit ses phases, sa cible annoncée et son impact. Les ennemis visent uniquement les joueurs debout et disposent d’un itinéraire vers chaque cible.

Les trois plaques mobiles existent dans chaque map. Leur phase est calculée à partir du temps de simulation partagé ; l’invité reconstruit leurs collisions avant de rejouer ses déplacements. Les personnages à terre peuvent aussi être transportés, avec leur marqueur de réanimation.

L’événement **Roquettes !** est tiré uniquement par l’hôte à partir du round 3 : 8 % de chance par round éligible et cinq rounds minimum entre événements. Tous les joueurs reçoivent le lanceur pour ce round, y compris les joueurs à terre. Le lanceur temporaire s’utilise avec **5**, possède trois munitions par chargeur et disparaît à la fin du round.

Les roquettes, explosions, propriétaires des tirs, dégâts de zone, impulsions de rocket jump et remise à l’arme précédente sont confirmés par l’hôte. Pas de dégâts de roquette aux alliés ; le tireur peut être blessé et les barils conservent leurs dégâts à toute l’équipe. Les cartes de déblocage d’armes et d’événement apparaissent sur chaque ordinateur.

## Architecture et limites

L’hôte simule les joueurs, les ennemis, les collisions et les dégâts à 60 Hz. Les invités transmettent leurs commandes ; ils prédisent leur déplacement puis se corrigent à partir de l’état confirmé par l’hôte. Les positions des autres joueurs et des ennemis sont interpolées à l’affichage.

Les états du monde sont transmis environ **20 fois par seconde**. L’encodage utilise des champs explicites pour permettre à un Raspberry Pi ARM et à un PC x86 d’échanger les mêmes messages. Chaque nouvelle manche possède un identifiant pour ignorer des actions de l’ancienne.

Cette première version utilise **TCP**, avec des sockets non bloquantes et `TCP_NODELAY`, pour garder un transport simple et fiable. Les pertes de paquets peuvent toutefois provoquer des retards de livraison : le passage à UDP avec des messages fiables ciblés sera à étudier si les parties réelles le demandent. Voir les références [TCP](https://man7.org/linux/man-pages/man7/tcp.7.html) et [connexion non bloquante](https://man7.org/linux/man-pages/man2/connect.2.html).

Il reste à mesurer la latence réelle, le débit et la charge sur le Pi 4, surtout lorsqu’il héberge et affiche le jeu simultanément. Les effets de particules des tirs et explosions sont répliqués, en respectant la limite du profil graphique de chaque invité. Les dégâts et éliminations sont décidés par l’hôte. Le ping affiché mesure l’aller-retour TCP, avec un léger lissage. La version 0.9.2 utilise le protocole 8, incompatible avec toutes les versions précédentes : mets à jour chaque ordinateur. Les profils graphiques peuvent différer entre les joueurs.

## Contrôles disponibles

```bash
make test
make test-tcp
```

`make test` contrôle le protocole, les messages fragmentés, quatre joueurs, les transitions de map et de manche, la prédiction, les chargeurs, la transmission du champion / de l’événement et une réanimation effectuée par un invité sur un autre invité, avec un transport en mémoire. Il contrôle aussi les vies solo, déblocages, modules, sliders, saisie numérique, interruptions de réanimation, dégâts de zone bloqués par les murs, rocket jump, phases du champion, IA, plateformes et leurs collisions. Les contrôles de saisie utilisent une fenêtre SDL fictive.

`make test-tcp` ouvre réellement un hôte et un client sur `127.0.0.1:37991`, sans fenêtre, et vérifie la connexion et les échanges. Le port doit être libre. Ce contrôle doit être lancé dans un environnement qui autorise les sockets.

Les contrôles en mémoire et les tests de gameplay ont été exécutés pour la version 0.8, qui compile sans avertissement. L’environnement de développement bloque la création de sockets : **une connexion réelle ou une partie entre deux machines reste à valider**. Le résultat détaillé est dans [VALIDATION.md](VALIDATION.md).
