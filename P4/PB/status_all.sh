#!/usr/bin/env bash
# status_all.sh — Parte B
# Ejecuta consultas de "status" cruzadas entre s01..s04 usando el binario ./client
# Ajusta PORT si decides usar un puerto distinto a 49200.

set -euo pipefail

PORT=49200
ALIASES=(s01 s02 s03 s04)

for SRC in "${ALIASES[@]}"; do
  echo "==== Cliente $SRC consulta a los demás ===="

  for DST in "${ALIASES[@]}"; do
    if [[ "$SRC" != "$DST" ]]; then
      # Requiere que ./client esté compilado en la misma carpeta (PB)
      ./client "$DST" "$PORT" status
    fi
  done
done
