#!/bin/bash

# Script para envío aleatorio de archivos a servidores con alias
# Uso: ./sender.sh <ALIAS>

ALIAS=$1

# Archivos disponibles
FILES=(*.txt)
NUM_FILES=${#FILES[@]}

# Seleccionar archivo aleatorio
INDEX=$((RANDOM % NUM_FILES))
FILE=${FILES[$INDEX]}

echo "Enviando $FILE a $ALIAS"
./client $ALIAS "$FILE"