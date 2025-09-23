#!/bin/bash
SERVER=$1
PORT=$2
FILES=(*.txt) # Archivos a enviar
NUM_FILES=${#FILES[@]}
INDEX=$((RANDOM % NUM_FILES))
FILE=${FILES[$INDEX]}
echo "Sending $FILE to $SERVER:$PORT"
./client $SERVER $PORT "$FILE"
