# Contribuer à FastFps

FastFps est un FPS 3D natif en C11, inspiré de Doom, conçu pour rester jouable sur Raspberry Pi 4. Le projet propose du solo et de la coopération jusqu'à quatre joueurs. Les contributions au gameplay, aux graphismes géométriques, à l'accessibilité, au réseau, à la documentation et aux essais sur matériel réel sont bienvenues.

## Avant de commencer

- Lis le [README](README.md), le [guide multijoueur](MULTIJOUEUR.md) et l'[historique des vérifications](VALIDATION.md).
- Respecte le [code de conduite](CODE_OF_CONDUCT.md). Les échanges peuvent être en français ou en anglais ; les textes visibles dans le jeu restent en français.
- Cherche une issue existante avant d'en ouvrir une. Pour un changement important de gameplay, une nouvelle dépendance ou une modification du protocole réseau, présente d'abord la proposition dans une issue.
- Pour une faille de sécurité, suis [SECURITY.md](SECURITY.md) ; garde les détails d'exploitation dans un canal privé.

## Signaler un problème ou proposer une idée

Utilise les [formulaires d'issues](https://github.com/Rdb33/FastFps/issues/new/choose). Un rapport utile indique la version ou le commit, le système, le modèle de Raspberry Pi ou le PC, le profil graphique, le mode de jeu, les étapes de reproduction et le résultat attendu / obtenu.

Pour un problème réseau, précise le rôle hôte ou invité, la version de chaque joueur, le nombre de joueurs et le type de connexion : LAN, Tailscale ou redirection de port. Pour la fluidité, indique la résolution, les FPS observés, le nombre de joueurs et la vague. Distingue les observations sur Pi réel des mesures faites sur PC ou en rendu logiciel.

## Installer un environnement de développement

Sur Linux ou Raspberry Pi OS avec une session graphique :

```bash
sudo apt update
sudo apt install git build-essential pkg-config libsdl2-dev libgles-dev libmpg123-dev
# Facultatif : vérification des mises à jour au lancement
sudo apt install curl ca-certificates
```

Crée un fork sur GitHub, puis clone ton fork et crée une branche :

```bash
git clone https://github.com/VOTRE_PSEUDO/FastFps.git
cd FastFps
git switch -c amelioration/description-courte
make -j2
./lancer.sh --no-update-check
```

Le jeu cherche les musiques dans `assets/music/`. Leur provenance et leurs droits sont décrits dans [les notices tierces](THIRD_PARTY_NOTICES.md) ; elles ne sont pas une condition pour compiler le jeu. Ne joins pas de nouveau morceau ou autre contenu tiers sans autorisation de redistribution vérifiable.

## Règles de développement

- Utilise le C11 et les conventions des fichiers modifiés ; évite les reformatages sans rapport avec le changement.
- Conserve la compatibilité OpenGL ES 2 et les profils Raspberry Pi. Privilégie les géométries simples, les allocations limitées et les buffers existants.
- L'hôte décide des apparitions, dégâts, éliminations et récompenses. La prédiction d'un invité ne doit pas créer un deuxième événement autoritaire.
- Tout changement de format ou de règles de prédiction incompatible doit incrémenter `NET_PROTOCOL` dans `src/network.h` et être documenté dans `MULTIJOUEUR.md`.
- Lors d'une version publiée, mets à jour `GAME_VERSION` dans `src/config.h`, `CHANGELOG.md` et les documents concernés. Le vérificateur de mise à jour lit une version stable sous la forme `majeure.mineure.correctif` sur `main`.
- Garde les scripts personnels, secrets, sauvegardes, fichiers compilés et captures hors des commits. `/sleep.sh` est volontairement ignoré.

## Vérifier une contribution

Pour une modification du code, compile avec les avertissements du Makefile et joue le scénario concerné :

```bash
make -j2
```

La suite existante peut être exécutée explicitement pour contrôler le gameplay et le transport réseau en mémoire :

```bash
make test
```

Un essai TCP distinct est disponible si l'environnement autorise l'ouverture de sockets :

```bash
make test-tcp
```

Pour le rendu automatique, hors vérification de mise à jour :

```bash
./build/fastfps --demo --mute --frames 120 --screenshot capture.bmp
```

Une capture ou un compteur FPS sur PC ne remplace pas un essai sur Raspberry Pi 4. Pour le multijoueur, vérifie également avec plusieurs instances ou machines et indique ce que tu as réellement essayé. Si un contrôle n'a pas été effectué, dis-le dans la pull request ; n'annonce pas une validation non réalisée.

## Envoyer une pull request

1. Garde une modification principale par pull request et des commits compréhensibles.
2. Relie l'issue concernée et explique le problème, la solution et les effets attendus pour les joueurs.
3. Utilise le modèle de pull request et liste les compilations, essais, mesures et limites réelles.
4. Ajoute des captures pour un changement visuel et les étapes de reproduction pour une correction.
5. Mets à jour la documentation concernée et indique les conséquences pour les invités / l'hôte et pour le Raspberry Pi.

Le mainteneur peut demander des ajustements avant intégration. Aucun délai de revue n'est garanti.

## Licence des contributions

Les contributions originales proposées au code et à la documentation sont destinées à être distribuées sous la [licence MIT](LICENSE) du projet. Conserve les mentions d'auteur et les notices requises. Pour un contenu tiers, indique sa provenance, son auteur et sa licence ou son autorisation de redistribution ; son ajout doit être compatible avec l'usage prévu du projet. Les pistes de Buckshot Roulette ne sont pas couvertes par cette licence et ne doivent pas être réintroduites dans un commit.
