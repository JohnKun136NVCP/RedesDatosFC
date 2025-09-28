#!/bin/bash
LOCAL_ALIAS=$1 # Alias del servidor
PORT=49200 # Puerto de conexion
FILES=(*.txt)  # Todos los archivos de texto en la carpeta
NUM_FILES=${#FILES[@]} #Numero de archivos

while true; do
    # Elegir un archivo al azar
    INDEX=$((RANDOM % NUM_FILES))
    FILE=${FILES[$INDEX]}

    echo "Enviando $FILE desde $LOCAL_ALIAS"
    ./clientB "$LOCAL_ALIAS" 3 "$FILE" # Cambiar el ./clientB por el ejecutable que genero al compilar clientubtB.c
    # Esperar tiempo aleatorio entre 1 y 5 segundos
    SLEEP=$((RANDOM % 5 +1))
    sleep $SLEEP
done