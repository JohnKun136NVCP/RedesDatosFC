// serverOpt.c

#include <arpa/inet.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUFFER_SIZE 1024

void cifradoCesar(char *texto, int desplazamiento) {
  desplazamiento = desplazamiento % 26;
  for (int i = 0; texto[i] != '\0'; i++) {
    char c = texto[i];
    if (isupper(c)) {
      texto[i] = ((c - 'A' + desplazamiento) % 26) + 'A';
    } else if (islower(c)) {
      texto[i] = ((c - 'a' + desplazamiento) % 26) + 'a';
    }
  }
}

int servidor(int puerto) {
  int socket_servidor;
  struct sockaddr_in direccion_servidor;

  socket_servidor = socket(AF_INET, SOCK_STREAM, 0);
  if (socket_servidor == -1) {
    perror("Error creando socket");
    return -1;
  }

  int opt = 1;
  setsockopt(socket_servidor, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  direccion_servidor.sin_family = AF_INET;
  direccion_servidor.sin_port = htons(puerto);
  direccion_servidor.sin_addr.s_addr = INADDR_ANY;

  if (bind(socket_servidor, (struct sockaddr *)&direccion_servidor, sizeof(direccion_servidor)) < 0) {
    perror("Error en bind");
    close(socket_servidor);
    return -1;
  }

  if (listen(socket_servidor, 3) < 0) {
    perror("Error escuchando");
    close(socket_servidor);
    return -1;
  }

  return socket_servidor;
}

void mensaje(char *buffer, int mi_puerto, int socket_cliente) {
  int puerto_destino, desplazamiento;
  char contenido[BUFFER_SIZE];

  sscanf(buffer, "%d %d", &puerto_destino, &desplazamiento);
  char *inicio_contenido = strchr(buffer, ' ');
  if (inicio_contenido) {
    inicio_contenido = strchr(inicio_contenido + 1, ' ');
    if (inicio_contenido) {
      strcpy(contenido, inicio_contenido + 1);
    }
  }

  if (puerto_destino == mi_puerto) {
    printf("\nSolicitud aceptada: puerto destino = %d, mi puerto = %d\n", puerto_destino, mi_puerto);
    printf("Archivo recibido, cifrando...\n");
    cifradoCesar(contenido, desplazamiento);
    printf("Contenido cifrado: %s\n", contenido);
    char respuesta[BUFFER_SIZE + 20];
    snprintf(respuesta, sizeof(respuesta), "PROCESADO: %s", contenido);
    send(socket_cliente, respuesta, strlen(respuesta), 0);
  } else {
    char *respuesta = "RECHAZADO: Puerto incorrecto";
    send(socket_cliente, respuesta, strlen(respuesta), 0);
    printf("\nSolicitud rechazada: puerto destino = %d, mi puerto = %d\n", puerto_destino, mi_puerto);
  }
}

void clientes(int *sockets_servidor, int num_puertos, int *puertos) {
  fd_set descriptores_lectura;
  int descriptor_maximo;

  while (1) {
    FD_ZERO(&descriptores_lectura);
    descriptor_maximo = 0;

    for (int i = 0; i < num_puertos; i++) {
      int descriptor = sockets_servidor[i];
      if (descriptor > 0) {
        FD_SET(descriptor, &descriptores_lectura);
      }
      if (descriptor > descriptor_maximo) {
        descriptor_maximo = descriptor;
      }
    }

    int actividad = select(descriptor_maximo + 1, &descriptores_lectura, NULL, NULL, NULL);

    for (int i = 0; i < num_puertos; i++) {
      int descriptor = sockets_servidor[i];
      if (FD_ISSET(descriptor, &descriptores_lectura)) {
        struct sockaddr_in direccion_cliente;
        socklen_t tamano_direccion = sizeof(direccion_cliente);
        int socket_cliente = accept(descriptor, (struct sockaddr *)&direccion_cliente, &tamano_direccion);
        if (socket_cliente < 0) {
          perror("Error aceptando conexión");
          continue;
        }

        char buffer[BUFFER_SIZE] = {0};
        int bytes = recv(socket_cliente, buffer, sizeof(buffer) - 1, 0);
        if (bytes > 0) {
          buffer[bytes] = '\0';
          mensaje(buffer, puertos[i], socket_cliente);
        }

        close(socket_cliente);
      }
    }
  }
}

int main(int argc, char *argv[]) {
  int puertos[] = {49200, 49201, 49202};
  int num_puertos = 3;
  int sockets_servidor[num_puertos];

  for (int i = 0; i < num_puertos; i++) {
    sockets_servidor[i] = servidor(puertos[i]);
    if (sockets_servidor[i] == -1) {
      fprintf(stderr, "No se pudo crear el servidor en el puerto %d\n", puertos[i]);
      return 1;
    }
    printf("Servidor escuchando en el puerto %d\n", puertos[i]);
  }

  clientes(sockets_servidor, num_puertos, puertos);

  for (int i = 0; i < num_puertos; i++) {
    close(sockets_servidor[i]);
  }

  return 0;
}
