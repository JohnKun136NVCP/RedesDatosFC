#!/bin/bash

SERVER=$1
PORT=$2
FILES=(*.txt) # Archivos a enviar
NUM_FILES=${#FILES[@]}
INDEX=$((RANDOM % NUM_FILES))
FILE=${FILES[$INDEX]}
echo "Sending $FILE to $SERVER:$PORT"
=======

SERVER=$1
PORT=$2

FILES=(*.txt)     # lista de archivos del directorio actual
NUM_FILES=${#FILES[@]}   # cuántos archivos hay

INDEX=$((RANDOM % NUM_FILES))   # índice aleatorio
FILE=${FILES[$INDEX]}

echo "Enviando $FILE a $SERVER:$PORT"

