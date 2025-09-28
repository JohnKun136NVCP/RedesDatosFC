#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define SERV_LOCAL "127.0.0.1"
#define SERV_S01 "192.168.1.101"
#define SERV_S02 "192.168.1.102"
#define SERV_S03 "192.168.1.103"
#define SERV_S04 "192.168.1.104"
#define SERV_PORT 49200

/*
 * Ejecutar con: ./client [servidor] archivo
 *    - servidor: opcional, "s01" para 192.168.1.101, "s02" para 192.168.1.102, "s03" para 192.168.1.103, "s04" para 192.168.1.104, o nada para local (127.0.0.1)
 *    - archivo: nombre del archivo a enviar al servidor
 */

// Función para obtener la marca de tiempo actual
char *timetag() {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  static char buf[20];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", t);
  return buf;
}

// Función para registrar la actividad del servidor
void serverlog(int sockfd, FILE *log_file) {
  if (!log_file) return;
  char bufmsg[500];
  int n = read(sockfd, bufmsg, sizeof(bufmsg) - 1);
  if (n > 0) {
    bufmsg[n] = '\0';
    fprintf(log_file, "%s", bufmsg);
    fflush(log_file);
  }
}

// Función principal del cliente
int main(int argc, char *argv[]) {
  // Determina la dirección IP del servidor según parámetro
  char *ip = SERV_LOCAL;
  if (argv[1] && strcmp(argv[1], "s01") == 0)
    ip = SERV_S01;
  else if (argv[1] && strcmp(argv[1], "s02") == 0)
    ip = SERV_S02;
  else if (argv[1] && strcmp(argv[1], "s03") == 0)
    ip = SERV_S03;
  else if (argv[1] && strcmp(argv[1], "s04") == 0)
    ip = SERV_S04;

  // Crea el socket del cliente
  int s = socket(AF_INET, SOCK_STREAM, 0);

  // Configura la estructura de dirección del servidor
  struct sockaddr_in sa = {0};
  sa.sin_family = AF_INET;
  inet_pton(AF_INET, ip, &sa.sin_addr);
  sa.sin_port = htons(SERV_PORT);

  // Conecta al servidor en el puerto principal
  connect(s, (void *)&sa, sizeof(sa));
  printf("[%s] [CLIENTE]: Conectado a %s:%d\n", timetag(), ip, SERV_PORT);

  // Lee el nuevo puerto asignado por el servidor
  char buf[100] = {0};
  read(s, buf, sizeof(buf) - 1);
  int new_port = atoi(buf);
  // Cierra la conexión inicial
  close(s);

  // Crea nuevo socket para el puerto reasignado
  s = socket(AF_INET, SOCK_STREAM, 0);
  // Actualiza el puerto en la estructura de dirección
  sa.sin_port = htons(new_port);
  // Conecta al nuevo puerto
  connect(s, (void *)&sa, sizeof(sa));
  printf("[%s] [CLIENTE]: Reasignado al puerto %d\n", timetag(), new_port);

  // Determina el nombre del archivo desde los argumentos
  char *filename = NULL;
  int arg_index = 1;
  if (argv[1] && (strcmp(argv[1], "s01") == 0 || strcmp(argv[1], "s02") == 0 || strcmp(argv[1], "s03") == 0 || strcmp(argv[1], "s04") == 0)) {
    arg_index = 2;
  }
  if (argv[arg_index]) {
    filename = argv[arg_index];
  }
  // Abre el archivo para lectura
  FILE *fp = fopen(filename, "rb");
  // Si el archivo se abrió correctamente
  if (fp) {
    // Define el nombre del archivo de log
    const char *log_filename = "server.log";
    // Abre el archivo de log para escritura
    FILE *log_file = fopen(log_filename, "a");

    // Envía el nombre del archivo al servidor
    write(s, filename, strlen(filename));
    // Envía el delimitador de fin de nombre
    write(s, "\n", 1);
    // Registra la respuesta inicial del servidor
    serverlog(s, log_file);

    // Obtiene el tamaño del archivo
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    // Reserva memoria para el contenido del archivo
    char *file_buffer = malloc(size);
    // Lee el contenido del archivo en memoria
    fread(file_buffer, 1, size, fp);
    // Cierra el archivo local
    fclose(fp);
    // Envía el contenido del archivo al servidor
    write(s, file_buffer, size);
    // Indica fin de envío de datos
    shutdown(s, SHUT_WR);
    printf("[%s] [CLIENTE]: Envió el archivo: %s\n", timetag(), filename);

    // Registra la confirmación del servidor
    serverlog(s, log_file);

    // Si el log se abrió
    if (log_file) {
      // Cierra el archivo de log
      fclose(log_file);
      printf("[%s] [CLIENTE]: Actividad del servidor guardada en %s\n", timetag(), log_filename);
    }
  }

  // Cierra la conexión con el servidor
  close(s);
  printf("[%s] [CLIENTE]: Desconectado\n", timetag());
}