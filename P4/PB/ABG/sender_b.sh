#!/bin/bash
# Uso: ./sender_b.sh

SERVERS=( "192.168.0.200" "192.168.0.201" "192.168.0.202" )   
PORT=49200
ROUNDS=6                         
MIN_SLEEP=2                     
MAX_SLEEP=8                     

STAMP() { date '+%Y-%m-%d %H:%M:%S'; }
pick_server() { echo "${SERVERS[$RANDOM % ${#SERVERS[@]}]}"; }

shopt -s nullglob
FILES=(*.txt)
if [ ${#FILES[@]} -eq 0 ]; then
  echo "[$(STAMP)] ERROR: no hay .txt en $(pwd)" | tee -a log_cliente.txt
  exit 1
fi

for ((i=1;i<=ROUNDS;i++)); do
  SERVER=$(pick_server)
  FILE="${FILES[$RANDOM % ${#FILES[@]}]}"
  echo "[$(STAMP)] CONECTANDO: destino=$SERVER:$PORT archivo=$FILE (intento $i/$ROUNDS)" | tee -a log_cliente.txt

  if ./client "$SERVER" "$PORT" "$FILE"; then
    echo "[$(STAMP)] OK: ENVIADO archivo=$FILE a $SERVER:$PORT" | tee -a log_cliente.txt
  else
    echo "[$(STAMP)] ERROR: FALLO_EN_ENVIO archivo=$FILE a $SERVER:$PORT" | tee -a log_cliente.txt
  fi

  # espera aleatoria entre envios
  SLEEP=$(( MIN_SLEEP + (RANDOM % (MAX_SLEEP - MIN_SLEEP + 1)) ))
  echo "[$(STAMP)] ESPERA: ${SLEEP}s antes del próximo envío" | tee -a log_cliente.txt
  sleep "$SLEEP"
done
