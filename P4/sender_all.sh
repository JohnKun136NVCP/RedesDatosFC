#!/bin/bash
PORT=49200

# Lista de alias
ALIAS=(s01 s02 s03 s04)

# Carpeta donde están los archivos a enviar
FILES_DIR=~/files

# Función para enviar archivos de un alias a los demás
send_from_alias() {
    local SRC=$1
    for DST in "${ALIAS[@]}"; do
        if [ "$DST" != "$SRC" ]; then
            # Elegir archivo aleatorio
            FILES=($FILES_DIR/*.txt)
            NUM_FILES=${#FILES[@]}
            INDEX=$((RANDOM % NUM_FILES))
            FILE=${FILES[$INDEX]}
            echo "[$SRC] Sending $FILE to $DST:$PORT"
            ./client $DST $PORT "$FILE"
            sleep 1
        fi
    done
}

# Ejecutar para cada alias
for AL in "${ALIAS[@]}"; do
    send_from_alias $AL
done
