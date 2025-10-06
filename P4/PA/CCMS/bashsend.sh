#!/bin/bash

SERVER=$1            # Alias del servidor
PORT=49200           # Puerto de conexion
FILES=(*.txt)        # Todos los archivos de texto en la carpeta ac>NUM_FILES=${#FILES[@]}

while true; do
    # Elegir un archivo al azar
    INDEX=$((RANDOM % NUM_FILES))
    FILE=${FILES[$INDEX]}

    echo "Enviando $FILE a $SERVER:$PORT"
    ./clienub2 "$SERVER" "$PORT" "$FILE" 3  # Cambiar el ./clienub2 por el ejecutable que genero al compilar clientubt.c
    # Esperar tiempo aleatorio entre 1 y 5 segundos
    SLEEP_TIME=$((RANDOM % 5 + 1))
    sleep $SLEEP_TIME
done