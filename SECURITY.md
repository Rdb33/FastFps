# Politique de sécurité

## Versions prises en charge

Les corrections de sécurité visent la dernière version publiée sur `main` ; les anciennes versions ne disposent pas d'une branche de maintenance distincte.

| Version | Corrections de sécurité |
| --- | --- |
| Dernière version publiée sur `main` | Oui |
| Versions précédentes | Mise à jour vers la dernière version recommandée |

Consulte `GAME_VERSION` dans `src/config.h` et [CHANGELOG.md](CHANGELOG.md) pour identifier la version publiée.

## Signaler une vulnérabilité

Utilise le [signalement privé de vulnérabilité sur GitHub](https://github.com/Rdb33/FastFps/security/advisories/new) lorsque le bouton **Report a vulnerability** est disponible dans l'onglet **Security → Advisories** du dépôt. Le rapport est destiné aux responsables de la sécurité du dépôt et n'est pas une issue publique.

Si cette option n'est pas disponible, ouvre une [issue](https://github.com/Rdb33/FastFps/issues/new) intitulée **« Demande de contact sécurité »**, contenant uniquement une demande de canal privé. Attends qu'un canal soit convenu avant d'envoyer des détails techniques ou une preuve de concept. Aucune adresse personnelle n'est publiée dans ce dépôt.

Garde les étapes d'exploitation, charges réseau, données personnelles et informations sensibles hors des issues et pull requests publiques jusqu'à la coordination d'une correction.

## Informations utiles dans le rapport privé

- Version ou commit concerné, système et architecture : Raspberry Pi ARM ou PC.
- Description du problème et de son impact possible.
- Rôle hôte ou invité et conditions nécessaires pour le déclencher, si le réseau est concerné.
- Étapes minimales de reproduction ou preuve de concept non destructive.
- Journaux et proposition de correction, si disponibles, en retirant les secrets et informations d'autres personnes.

Les erreurs de traitement des paquets, corruptions mémoire, accès non autorisés, exécutions de commandes ou problèmes liés au vérificateur de mise à jour peuvent relever de cette procédure. Les problèmes ordinaires de gameplay, de rendu ou de performance utilisent le formulaire de bug public.

## Traitement

Le mainteneur examinera le signalement, cherchera à reproduire le problème et coordonnera une correction et sa publication. Il s'agit d'un projet bénévole : aucun délai fixe de réponse ou de correction, ni récompense financière, n'est promis. Une attribution au chercheur sera discutée avant publication ; l'anonymat peut être demandé.

Effectue tes recherches sur des instances et réseaux que tu contrôles ou pour lesquels tu as une autorisation. Cette politique ne donne pas d'autorisation pour tester les ordinateurs, salons ou services d'autres personnes.
