// clientMulti.c

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUFFER_SIZE 1024

void conexion(int puerto, int puerto_destino, int desplazamiento, const char *contenido, const char *ip_servidor) {
  int sock;
  struct sockaddr_in direccion_servidor;
  char buffer[BUFFER_SIZE] = {0};
  char mensaje[BUFFER_SIZE];

  sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock == -1) {
    printf("Error al crear el socket para el puerto %d\n", puerto);
    return;
  }

  direccion_servidor.sin_family = AF_INET;
  direccion_servidor.sin_port = htons(puerto);
  direccion_servidor.sin_addr.s_addr = inet_addr(ip_servidor);

  if (connect(sock, (struct sockaddr *)&direccion_servidor, sizeof(direccion_servidor)) < 0) {
    printf("Conexión fallida al puerto %d\n", puerto);
    close(sock);
    return;
  }

  snprintf(mensaje, sizeof(mensaje), "%d %d %s", puerto_destino, desplazamiento, contenido);
  send(sock, mensaje, strlen(mensaje), 0);

  int bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);
  if (bytes > 0) {
    buffer[bytes] = '\0';
    if (strncmp(buffer, "PROCESADO:", 10) == 0) {
      printf("Servidor %d: ACEPTADO: archivo procesado y cifrado\n", puerto);
    } else {
      printf("Servidor %d: %s\n", puerto, buffer);
    }
  }

  close(sock);
}

int main(int argc, char *argv[]) {
  if (argc < 5 || (argc - 2) % 3 != 0) {
    printf("Uso: %s <ip_servidor> <puerto_destino1> <desplazamiento1> <ruta_archivo1> [<puerto_destino2> <desplazamiento2> <ruta_archivo2> ...]\n", argv[0]);
    return 1;
  }

  const char *ip_servidor = argv[1];
  int puertos[] = {49200, 49201, 49202};

  for (int i = 2; i < argc; i += 3) {
    int puerto_destino = atoi(argv[i]);
    int desplazamiento = atoi(argv[i + 1]);
    const char *ruta_archivo = argv[i + 2];

    FILE *fp = fopen(ruta_archivo, "r");
    if (fp == NULL) {
      fprintf(stderr, "Error al abrir el archivo %s.\n", ruta_archivo);
      perror("Detalle del error");
      continue;
    }

    char contenido[BUFFER_SIZE] = {0};
    fread(contenido, 1, sizeof(contenido) - 1, fp);
    fclose(fp);

    printf("Enviando al servidor: puerto destino = %d, desplazamiento = %d, archivo = %s\n", puerto_destino, desplazamiento, ruta_archivo);

    for (int j = 0; j < 3; j++) {
      conexion(puertos[j], puerto_destino, desplazamiento, contenido, ip_servidor);
    }
    printf("\n");
  }

  return 0;
}
