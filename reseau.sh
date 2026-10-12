#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"

show_help() {
  cat <<'HELP'
FASTFPS — connexion entre amis

Sur le même Wi-Fi : crée un salon, puis ton ami clique sur le salon détecté.

Sur Internet, sur chaque machine Linux / Raspberry Pi :
  ./reseau.sh vpn             Installer Tailscale si nécessaire et se connecter
  ./reseau.sh etat            Afficher l'état du VPN et ton invitation

Invite ensuite ton ami dans ton réseau privé Tailscale, avec son propre compte :
  https://tailscale.com/docs/features/sharing/how-to/invite-any-user

Puis, dans FastFPS : l'hôte crée le salon et copie l'invitation.
L'invité ouvre Multijoueur, colle avec Ctrl+V, puis appuie sur Entrée.

Raccourcis :
  ./reseau.sh heberger --name Alice
  ./reseau.sh rejoindre fastfps://100.101.102.103:27015 --name Bob

La commande vpn installe un logiciel système et peut demander sudo.
La commande etat ne modifie pas la configuration réseau.
HELP
}

show_status() {
  if ! command -v tailscale >/dev/null; then
    echo "Tailscale n'est pas installé. Lance ./reseau.sh vpn pour jouer à distance."
    return 0
  fi
  if ! tailscale status; then
    echo "VPN non connecté. Lance ./reseau.sh vpn et ouvre le lien de connexion."
    return 1
  fi
  local vpn_address
  vpn_address=$(tailscale ip -4 2>/dev/null || true)
  if [[ -n "$vpn_address" ]]; then
    printf '\nInvitation pour le port par défaut : fastfps://%s:27015\n' "$vpn_address"
    echo "Crée un salon dans le jeu avant que ton ami utilise cette invitation."
  fi
}

case "${1:-aide}" in
  aide|help|--help|-h) show_help ;;
  etat|status) show_status ;;
  vpn)
    if ! command -v tailscale >/dev/null; then
      if ! command -v curl >/dev/null; then
        echo "curl est nécessaire pour l'installation : sudo apt install curl"
        exit 1
      fi
      echo "Installation de Tailscale depuis l'installateur officiel."
      vpn_installer=$(mktemp "${TMPDIR:-/tmp}/fastfps-tailscale.XXXXXX")
      trap 'rm -f -- "$vpn_installer"' EXIT
      curl --fail --silent --show-error --location --proto '=https' --tlsv1.2 \
        https://tailscale.com/install.sh -o "$vpn_installer"
      sh "$vpn_installer"
    fi
    echo "Connexion au VPN : ouvre le lien affiché et utilise ton propre compte."
    if (( EUID == 0 )); then tailscale up; else sudo tailscale up; fi
    show_status
    echo "Invite ton ami : https://tailscale.com/docs/features/sharing/how-to/invite-any-user"
    ;;
  heberger|host)
    shift
    exec ./lancer.sh --host "$@"
    ;;
  rejoindre|join)
    if (( $# < 2 )); then echo "Usage : ./reseau.sh rejoindre INVITATION [options]"; exit 1; fi
    invitation=$2
    shift 2
    exec ./lancer.sh --join "$invitation" "$@"
    ;;
  *) show_help; exit 1 ;;
esac
