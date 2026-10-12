#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "${BASH_SOURCE[0]}")"
if ! command -v make >/dev/null || ! command -v pkg-config >/dev/null; then
  echo "Il faut make, un compilateur C et pkg-config. Voir README.md."
  exit 1
fi
make -s
exec ./build/fastfps "$@"
