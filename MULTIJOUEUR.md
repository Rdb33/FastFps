# Salons coopératifs — version 0.3

Un joueur héberge un salon sur son ordinateur ; jusqu’à trois amis le rejoignent. L’hôte lance la partie quand les joueurs sont prêts. Le jeu utilise une **adresse IPv4 et un port TCP**, sans service central ni frais d’hébergement.

Les joueurs doivent utiliser la même version du jeu et la même carte.

## Depuis le menu

1. Lance `./lancer.sh` et sélectionne **Multijoueur / salons**.
2. Renseigne ton **pseudo**. Clique sur le champ puis saisis le texte ; la première saisie remplace la valeur existante. `Entrée` termine la saisie.
3. L’hôte choisit **Créer un salon**. Le port par défaut est **27015**.
4. Les invités renseignent l’**adresse de l’hôte** et le même port, puis choisissent **Rejoindre un salon**.
5. Chaque invité sélectionne **Je suis prêt**.
6. L’hôte sélectionne **Lancer la partie**.

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

Ces commandes ouvrent directement le salon ; il faut ensuite se marquer prêt et lancer depuis les boutons.

Pour choisir un autre port, ajoute `--port 27016` **sur les deux ordinateurs**.

## Sur le même réseau

L’hôte peut consulter ses adresses avec :

```bash
hostname -I
```

Partage l’adresse du réseau utilisé, généralement `192.168.x.x` ou `10.x.x.x`. Un pare-feu actif doit autoriser le port TCP choisi sur l’hôte.

`127.0.0.1` sert uniquement à rejoindre un autre lancement du jeu sur **le même ordinateur**. Pour jouer entre deux ordinateurs, utilise l’adresse du réseau ou du VPN.

## Sur Internet

Deux possibilités pour cette première version :

- **Connexion directe** : l’hôte partage son adresse IPv4 publique et configure sa box pour rediriger le **port TCP 27015** vers l’adresse locale de son ordinateur. Le port choisi doit aussi être autorisé par son pare-feu.
- **VPN privé reliant les joueurs** : les invités utilisent l’adresse IPv4 du VPN de l’hôte. Le VPN peut lui-même utiliser des services de connexion ou de relais ; le jeu ne gère pas ces services.

Si la connexion de l’hôte est derrière un NAT d’opérateur (CGNAT), une redirection dans sa propre box peut ne pas suffire. Il faut alors une adresse publique accessible, un VPN approprié ou, plus tard, un relais intégré. La version 0.3 n’effectue pas de traversée automatique des box et ne fournit pas de code de salon.

## Jouer ensemble

- Les joueurs combattent les mêmes ennemis, avec les mêmes vagues, soins et **score d’équipe**.
- Les alliés ont des silhouettes avec casque et armure colorée, différentes des créatures. Leurs noms et leur statut apparaissent dans l’interface.
- Pas de dégâts entre alliés dans cette base.
- Si un joueur tombe, il revient après **5 secondes** tant qu’un allié reste en vie, puis bénéficie de **2 secondes de protection**.
- Si toute l’équipe tombe, la manche s’arrête et le salon affiche le résultat. L’hôte peut relancer une partie.
- `Échap` libère ta souris et ouvre ton menu : **la partie des autres continue**. Sélectionne **Reprendre** pour revenir.
- Quitter le salon en tant qu’invité retire uniquement cet invité. Fermer le salon de l’hôte déconnecte tout le monde. Il n’y a pas encore de transfert automatique du rôle d’hôte.

## Architecture et limites

L’hôte simule les joueurs, les ennemis, les collisions et les dégâts à 60 Hz. Les invités transmettent leurs commandes ; ils prédisent leur déplacement puis se corrigent à partir de l’état confirmé par l’hôte. Les positions des autres joueurs et des ennemis sont interpolées à l’affichage.

Les états du monde sont transmis environ **20 fois par seconde**. L’encodage utilise des champs explicites pour permettre à un Raspberry Pi ARM et à un PC x86 d’échanger les mêmes messages. Chaque nouvelle manche possède un identifiant pour ignorer des actions de l’ancienne.

Cette première version utilise **TCP**, avec des sockets non bloquantes et `TCP_NODELAY`, pour garder un transport simple et fiable. Les pertes de paquets peuvent toutefois provoquer des retards de livraison : le passage à UDP avec des messages fiables ciblés sera à étudier si les parties réelles le demandent. Voir les références [TCP](https://man7.org/linux/man-pages/man7/tcp.7.html) et [connexion non bloquante](https://man7.org/linux/man-pages/man2/connect.2.html).

Il reste à mesurer la latence réelle, le débit et la charge sur le Pi 4, surtout lorsqu’il héberge et affiche le jeu simultanément. Les effets de particules des tirs ne sont pas tous répliqués chez les invités dans cette base ; les dégâts et éliminations sont décidés par l’hôte.

## Contrôles disponibles

```bash
make test
make test-tcp
```

`make test` contrôle le protocole, les messages fragmentés, quatre joueurs, les changements de manche, la prédiction et la coopération avec un transport en mémoire.

`make test-tcp` ouvre réellement un hôte et un client sur `127.0.0.1:37991`, sans fenêtre, et vérifie la connexion et les échanges. Le port doit être libre. Ce contrôle doit être lancé dans un environnement qui autorise les sockets.

L’environnement de développement de cette session bloque la création de sockets : les contrôles en mémoire ont pu être exécutés, **pas une connexion TCP réelle ni une partie entre deux machines**. Le résultat détaillé est dans [VALIDATION.md](VALIDATION.md).
