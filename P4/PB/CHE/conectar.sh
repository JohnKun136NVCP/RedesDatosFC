#!/bin/bash

# Este script simula el "cliente status" de un alias.
# Y se conecta a todos los demás servidores para enviar un archivo.

# 1. Verifica que se pasó el alias del cliente como argumento
if [ -z "$1" ]; then
  echo "Error: Debes especificar el alias del cliente."
  echo "Uso: ./conectar.sh [alias]"
  echo "Ejemplo: ./conectar.sh s01"
  exit 1
fi

# 2. Define el cliente actual y la lista de todos los servidores
CLIENTE_ACTUAL=$1
SERVIDORES=("s01" "s02" "s03" "s04")

# 3. Selecciona un archivo .txt al azar para enviar
FILES=(*.txt)
if [ ${#FILES[@]} -eq 0 ]; then
    echo "No se encontraron archivos .txt en este directorio."
    exit 1
fi
NUM_FILES=${#FILES[@]}
INDEX=$((RANDOM % NUM_FILES))
FILE_TO_SEND=${FILES[$INDEX]}


echo "--- Iniciando proceso para el cliente '$CLIENTE_ACTUAL' ---"
echo "Archivo a enviar: $FILE_TO_SEND"
echo ""

# 4. Itera sobre la lista de servidores para conectarse
for SERVIDOR_DESTINO in "${SERVIDORES[@]}"; do
  
  # 5. El cliente no debe conectarse a su propio servidor
  if [ "$SERVIDOR_DESTINO" != "$CLIENTE_ACTUAL" ]; then
    
    echo "($CLIENTE_ACTUAL) -> Conectando con el servidor '$SERVIDOR_DESTINO'..."
    
    # Llama al programa cliente compilado
    ./client "$SERVIDOR_DESTINO" "$FILE_TO_SEND"
    
    echo "($CLIENTE_ACTUAL) -> Conexión con '$SERVIDOR_DESTINO' finalizada."
    echo "----------------------------------------------------"
    sleep 1 # Pequeña pausa para que la salida sea más clara
  fi
done

echo "--- Proceso completado para el cliente '$CLIENTE_ACTUAL' ---"