<p align="center">
  <img width="200" src="https://www.fciencias.unam.mx/sites/default/files/logoFC_2.png" alt="">
  <br><strong>Redes de Computadoras 2026-1</strong> <br>
  <em>Kevin Steve Quezada Ordoñez </em> <br>
</p>

# Práctica IV. Servidor con alias - Parte A

## Arquitectura

🔹 **VM Servidor → Debian**

🔹 **VM Cliente → Ubuntu** 

## Archivos

- `Server.c` - Servidor que escucha en alias s01 y s02
- `Client.c` - Cliente para enviar archivos a alias
- `sender.sh` - Script para envío aleatorio

## Funcionamiento

### VM Servidor (Debian):
1. **Configuración de red**: Se configuran alias s01 y s02
2. **Servidor**: Se escucha simultáneamente en ambos alias puerto 49200
3. **Estados**: Se registran ESPERANDO → RECIBIENDO → TRANSMITIENDO → ESPERANDO
4. **Almacenamiento**: Se guardan archivos en ~/s01/ y ~/s02/
5. **Logs**: Se generan s01_status.log y s02_status.log con timestamp

### VM Cliente (Ubuntu):
1. **Resolución**: Se convierten s01,s02 a IPs usando /etc/hosts
2. **Archivos**: Se preparan múltiples .txt para envío
3. **Envío**: Se conecta al servidor y se envía contenido
4. **Script**: sender.sh selecciona archivo aleatorio y servidor

## Ejecución

### 🔹 Servidor (Debian):
```bash
# 1 Compilación y ejecución
gcc server.c -o server
./server s01 s02

# 2. Monitoreo (después de ejecutar el Cliente)
cat s01_status.log
cat s02_status.log

# 3. Ver contenido de archivos recibidos (después de ejecutar el Cliente)
cat s01/archivo_*.txt
cat s02/archivo_*.txt
```

### 🔹 Cliente (Ubuntu):
```bash
# 1 Compilación y ejecución
gcc client.c -o client
chmod +x sender.sh

# 2. Envío de archivos
./client s01 <archivo.txt>
./sender.sh s02
```