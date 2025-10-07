#!/bin/bash

# Script para enviar archivos aleatorios a servidores
# Uso: ./sender.sh [server] [port]

# Función para resolver alias a IP
get_ip_for_server() {
    case $1 in
        "s01") echo "192.168.0.101" ;;
        "s02") echo "192.168.0.102" ;;
        "s03") echo "192.168.0.103" ;;
        "s04") echo "192.168.0.104" ;;
        *) echo $1 ;;  # Si no es alias, devolver tal como está
    esac
}

# Si se proporcionan argumentos, usarlos
if [ $# -eq 2 ]; then
    SERVER_ALIAS=$1
    PORT=$2
    SERVER=$(get_ip_for_server $SERVER_ALIAS)
else
    # Si no se proporcionan, seleccionar aleatoriamente
    SERVERS=("s01" "s02" "s03" "s04")
    SERVER_INDEX=$((RANDOM % ${#SERVERS[@]}))
    SERVER_ALIAS=${SERVERS[$SERVER_INDEX]}
    SERVER=$(get_ip_for_server $SERVER_ALIAS)
    PORT=49200
fi

echo "=== Random File Sender ==="
echo "Target alias: $SERVER_ALIAS -> $SERVER:$PORT"
echo "Time: $(date)"

# Buscar archivos .txt
FILES=(*.txt)
NUM_FILES=${#FILES[@]}

# Verificar si hay archivos disponibles. Si no, crear algunos de ejemplo
if [ $NUM_FILES -eq 0 ] || [ "${FILES[0]}" = "*.txt" ]; then
    echo "No .txt files found. Creating sample files..."
    echo "Contenido aleatorio 1 - $(date)" > random1.txt
    echo "Contenido aleatorio 2 - $(date)" > random2.txt
    echo "Contenido aleatorio 3 - $(date)" > random3.txt

    # Actualizar lista de archivos
    FILES=(*.txt)
    NUM_FILES=${#FILES[@]}
fi

# Seleccionar archivo aleatorio
INDEX=$((RANDOM % NUM_FILES))
FILE=${FILES[$INDEX]}

# Generar shift aleatorio
SHIFT=$((RANDOM % 25 + 1))

echo "Selected file: $FILE"
echo "Using shift: $SHIFT"
echo "Sending $FILE to $SERVER_ALIAS ($SERVER:$PORT)..."

# Enviar archivo usando tu clientFile - usar la IP real, no el alias
./client $SERVER $PORT $SHIFT "$FILE"

if [ $? -eq 0 ]; then
    echo "✓ File sent successfully!"
else
    echo "✗ Failed to send file"
fi

echo "==========================="