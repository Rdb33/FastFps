# Réglages GitHub du dépôt

Ces réglages se font dans l'interface GitHub avec un compte ayant les droits d'administration. Les fichiers de communauté ne modifient pas ces paramètres automatiquement.

## Description du dépôt

Sur la [page du dépôt](https://github.com/Rdb33/FastFps), ouvre la roue dentée à côté de **About**, puis colle cette description et enregistre :

> FPS 3D natif en C11, SDL2 et OpenGL ES 2, inspiré de Doom et conçu pour Raspberry Pi 4. Survie par vagues, progression et coopération jusqu'à 4 joueurs.

## Signalements de sécurité privés

1. Ouvre **Settings → Advanced Security** dans la section **Security and quality**.
2. Active **Private vulnerability reporting**.
3. L'onglet **Security → Advisories** doit alors proposer **Report a vulnerability**.
4. Choisis les notifications de sécurité souhaitées dans **Watch → Custom** et tes réglages de notifications personnels.

Cette méthode permet de recevoir les signalements via GitHub sans publier d'adresse personnelle. Les signalements de conduite utilisent leur propre procédure dans CODE_OF_CONDUCT.md ; les avis de sécurité sont réservés aux vulnérabilités.

Instructions officielles : [configurer les signalements privés](https://docs.github.com/en/code-security/how-tos/report-and-fix-vulnerabilities/configure-vulnerability-reporting/configure-for-a-repository).

## Contrôler les standards communautaires

Ouvre **Insights → Community Standards** une fois les fichiers intégrés sur la branche par défaut. GitHub peut mettre un moment à actualiser les indicateurs.

Le README, le code de conduite, le guide de contribution, la politique de sécurité, la licence choisie et les modèles d'issues / pull request ont chacun leur rôle. La licence du code ne couvre pas les morceaux de musique tiers : voir THIRD_PARTY_NOTICES.md.

## Adresse des commits

Dans [Settings → Emails](https://github.com/settings/emails) de ton compte personnel, active **Keep my email addresses private**. GitHub affiche une adresse `noreply` adaptée à ton compte. Pour tes commits depuis un terminal, configure aussi `user.email` dans Git avec cette adresse. L'option **Block command line pushes that expose my email** permet de bloquer certains envois qui révèlent l'adresse personnelle.

Ces réglages ne retirent pas une adresse déjà inscrite dans l'historique Git. Les outils API qui publient des commits doivent également utiliser une identité adaptée ; cacher une adresse dans le profil ou dans les documents ne masque pas les métadonnées des anciens commits.

Voir les [instructions GitHub sur l'adresse des commits](https://docs.github.com/en/account-and-profile/how-tos/email-preferences/setting-your-commit-email-address).
