# Salons coopératifs — version 0.4

Un joueur héberge un salon sur son ordinateur ; jusqu’à trois amis le rejoignent. L’hôte lance la partie quand les joueurs sont prêts. Le jeu utilise une **adresse IPv4 et un port TCP**, sans service central ni frais d’hébergement.

Les joueurs doivent utiliser la même version du jeu et la même carte.

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

Si la connexion de l’hôte est derrière un NAT d’opérateur (CGNAT), une redirection dans sa propre box peut ne pas suffire. Il faut alors une adresse publique accessible, un VPN approprié ou, plus tard, un relais intégré. Le jeu n’effectue pas de traversée automatique des box. La version 0.4 fournit une invitation contenant l’IPv4 et le port, sans code de salon ni relais intégré.

## Jouer ensemble

- Les joueurs combattent les mêmes ennemis, avec les mêmes vagues, soins et **score d’équipe**.
- Les alliés ont des silhouettes avec casque et armure colorée, différentes des créatures. Leurs noms et leur statut apparaissent dans l’interface.
- Pas de dégâts directs entre alliés. Les explosions des barils sont un danger pour toute l’équipe, y compris le tireur.
- Si un joueur tombe, il revient après **5 secondes** tant qu’un allié reste en vie, puis bénéficie de **2 secondes de protection**.
- Si toute l’équipe tombe, la manche s’arrête et le salon affiche le résultat. L’hôte peut relancer une partie.
- `Échap` libère ta souris et ouvre ton menu : **la partie des autres continue**. Sélectionne **Reprendre** pour revenir.
- Quitter le salon en tant qu’invité retire uniquement cet invité. Fermer le salon de l’hôte déconnecte tout le monde. Il n’y a pas encore de transfert automatique du rôle d’hôte.

## Architecture et limites

L’hôte simule les joueurs, les ennemis, les collisions et les dégâts à 60 Hz. Les invités transmettent leurs commandes ; ils prédisent leur déplacement puis se corrigent à partir de l’état confirmé par l’hôte. Les positions des autres joueurs et des ennemis sont interpolées à l’affichage.

Les états du monde sont transmis environ **20 fois par seconde**. L’encodage utilise des champs explicites pour permettre à un Raspberry Pi ARM et à un PC x86 d’échanger les mêmes messages. Chaque nouvelle manche possède un identifiant pour ignorer des actions de l’ancienne.

Cette première version utilise **TCP**, avec des sockets non bloquantes et `TCP_NODELAY`, pour garder un transport simple et fiable. Les pertes de paquets peuvent toutefois provoquer des retards de livraison : le passage à UDP avec des messages fiables ciblés sera à étudier si les parties réelles le demandent. Voir les références [TCP](https://man7.org/linux/man-pages/man7/tcp.7.html) et [connexion non bloquante](https://man7.org/linux/man-pages/man2/connect.2.html).

Il reste à mesurer la latence réelle, le débit et la charge sur le Pi 4, surtout lorsqu’il héberge et affiche le jeu simultanément. Les effets de particules des tirs et explosions sont répliqués, en respectant la limite du profil graphique de chaque invité. Les dégâts et éliminations sont décidés par l’hôte. Le ping affiché mesure l’aller-retour TCP, avec un léger lissage. Tous les joueurs doivent utiliser la 0.4 : le protocole 4 est incompatible avec la 0.3.

## Contrôles disponibles

```bash
make test
make test-tcp
```

`make test` contrôle le protocole, les messages fragmentés, quatre joueurs, les changements de manche, la prédiction et la coopération avec un transport en mémoire.

`make test-tcp` ouvre réellement un hôte et un client sur `127.0.0.1:37991`, sans fenêtre, et vérifie la connexion et les échanges. Le port doit être libre. Ce contrôle doit être lancé dans un environnement qui autorise les sockets.

Les contrôles en mémoire ont été exécutés pour la version 0.3. La version 0.4 a été compilée sans avertissement, mais ses contrôles n’ont pas été exécutés. L’environnement de développement bloque la création de sockets : **une connexion réelle ou une partie entre deux machines reste à valider**. Le résultat détaillé est dans [VALIDATION.md](VALIDATION.md).
