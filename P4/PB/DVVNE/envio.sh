#!/bin/bash
SERVER=$1
#Buscamos todos los archivos .txt solo dentro del directorio correspondiente al puerto
FILES=($(find "$HOME/$SERVER" -maxdepth 1 -name "*.txt" -type f)) 
#Si no existen los ficheros para cada alias en home usar:
#FILES=(*.txt)
NUM_FILES=${#FILES[@]}

#Si no hay archivos .txt decimos que el directorio esta vacio y salimos
if [ "$NUM_FILES" -eq 0 ]
then 
        echo "Directorio Vacio"
        exit 1
fi

INDEX=$((RANDOM % NUM_FILES))
FILE=${FILES[$INDEX]}
echo "Sending $FILE to $SERVER:$PORT"
./c "$FILE" $SERVER
