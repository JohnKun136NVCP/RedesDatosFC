#!/bin/bash

SERVER=$1
PORT=$2
STAMP() { date '+%Y-%m-%d %H:%M:%S'; }

shopt -s nullglob 
FILES=(*.txt)
FILE=${FILES[$((RANDOM % ${#FILES[@]}))]}

echo "[$(STAMP)] Intentando enviar $FILE a $SERVER:$PORT" | tee -a log_cliente.txt
if ./client "$SERVER" "$PORT" "$FILE"; then
  echo "[$(STAMP)] OK: $FILE enviado a $SERVER:$PORT" | tee -a log_cliente.txt
else
  echo "[$(STAMP)] ERROR: fallo al enviar $FILE a $SERVER:$PORT" | tee -a log_cliente.txt
fi
