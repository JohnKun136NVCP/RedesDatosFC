#!/bin/bash

# Alias configurados en Debian según práctica
SERVERS=("s01" "s02")
BASE_PORT=49200
FILES=("/home/daiki"/*.txt)

# Mapear alias a la IP real del servidor Ubuntu
declare -A SERVER_IP
SERVER_IP["s01"]="192.168.1.166"
SERVER_IP["s02"]="192.168.1.166"

# Revisar que haya archivos .txt
if [ ${#FILES[@]} -eq 0 ]; then
    echo "[-] No hay archivos .txt en el directorio actual"
    exit 1
fi

while true; do
    # Elegir alias y archivo aleatorio
    SERVER=${SERVERS[$RANDOM % ${#SERVERS[@]}]}
    FILE=${FILES[$RANDOM % ${#FILES[@]}]}
    IP=${SERVER_IP[$SERVER]}

    echo "$(date '+%Y-%m-%d %H:%M:%S') Enviando $FILE a $SERVER ($IP):$BASE_PORT"

    # Llamar al cliente con IP real del servidor
    ./clientAlias $IP $BASE_PORT "$FILE"

    # Espera aleatoria entre 5 y 15 segundos
    SLEEP_TIME=$((RANDOM % 11 + 5))
    echo "Esperando $SLEEP_TIME segundos antes del próximo envío..."
    sleep $SLEEP_TIME
done



