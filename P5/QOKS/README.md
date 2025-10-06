<p align="center">
  <img width="200" src="https://www.fciencias.unam.mx/sites/default/files/logoFC_2.png" alt="">
  <br><strong>Redes de Computadoras 2026-1</strong> <br>
  <em>Kevin Steve Quezada Ordoñez </em> <br>
</p>

# Práctica V. Restricción de servidor.

## Arquitectura

**VM Servidor → Debian**

**VM Cliente → Ubuntu**

## Archivos

- `Server.c` - Servidor que escucha en 4 alias (s01, s02, s03, s04) con sistema de turnos
- `Client.c` - Cliente para enviar archivos a alias

## Funcionamiento

### VM Servidor (Debian):
1. **Configuración de red**: Se configuran alias s01, s02, s03 y s04
2. **Servidor con turnos**: Escucha simultáneamente en los alias puerto 49200 con sistema round robin
3. **Control de acceso**: Solo un servidor puede recibir archivos a la vez
4. **Estados**: Se registran ESPERANDO → RECIBIENDO → TRANSMITIENDO → ESPERANDO
5. **Almacenamiento**: Se guardan archivos en ~/s01/, ~/s02/, ~/s03/, ~/s04/
6. **Logs**: Se generan archivos de log individuales para cada alias
7. **Hilos**: Implementa hilos para manejo concurrente y control de turnos

### VM Cliente (Ubuntu):
1. **Resolución**: Se convierten los alias a IPs usando /etc/hosts
2. **Multi-conexión**: Cada alias se conecta a los otros 3
3. **Envío**: Se conecta y envía el mismo archivo a múltiples servidores
4. **Respuestas**: Recibe "ARCHIVO_RECIBIDO" o "SERVIDOR_OCUPADO"
5. **Script**: sender.sh selecciona archivo aleatorio y alias destino

## Ejecución

### Servidor (Debian):
```bash
# 1. Compilación y ejecución
gcc Server.c -o server -lpthread
./server s01 s02 s03 s04

# 2. Monitoreo de logs (después de ejecutar el Cliente)
cat s01_status.log
cat s02_status.log
cat s03_status.log
cat s04_status.log

# 3. Ver archivos recibidos (después de ejecutar el Cliente)
ls s01/ s02/ s03/ s04/
cat s01/archivo_*.txt
cat s02/archivo_*.txt
cat s03/archivo_*.txt
cat s04/archivo_*.txt
```

### Cliente (Ubuntu):
```bash
# 1. Compilación
gcc Client.c -o client
chmod +x sender.sh

# 2. Envío de archivos
./client s01 archivo1.txt  # s01 envía a s02, s03, s04
./client s02 archivo2.txt  # s02 envía a s01, s03, s04
./client s03 archivo3.txt  # s03 envía a s01, s02, s04
./client s04 archivo4.txt  # s04 envía a s01, s02, s03

# 4. Envío aleatorio con script
./sender.sh s01
./sender.sh s02
./sender.sh s03
./sender.sh s04
```
