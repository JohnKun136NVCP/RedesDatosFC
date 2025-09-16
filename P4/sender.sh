#!/bin/bash

SERVER=$1
PORT=$2

FILES=(*.txt)     # lista de archivos del directorio actual
NUM_FILES=${#FILES[@]}   # cuántos archivos hay

INDEX=$((RANDOM % NUM_FILES))   # índice aleatorio
FILE=${FILES[$INDEX]}

echo "Enviando $FILE a $SERVER:$PORT"
./client $SERVER $PORT "$FILE"
