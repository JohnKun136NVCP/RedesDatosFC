#!/bin/bash

PORT=49200
ALIAS=(s01 s02 s03 s04)

FILES=( *.txt )
NUM_FILES=${#FILES[@]}


for SERVER in "${ALIAS[@]}"; do
  INDEX=$((RANDOM % NUM_FILES))
  FILE=${FILES[$INDEX]}

  echo "Enviando '$FILE' a $SERVER:$PORT"
  ./client "$SERVER" "$PORT" "$FILE"
done
