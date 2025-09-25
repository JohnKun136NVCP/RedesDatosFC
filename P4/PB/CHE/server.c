#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define SERV_LOCAL "127.0.0.1"
#define SERV_S01 "192.168.1.101"
#define SERV_S02 "192.168.1.102"
#define SERV_S03 "192.168.1.103"
#define SERV_S04 "192.168.1.104"
#define SERV_PORT 49200

/*
 * Ejecutar con: ./server [servidor]
 *    - servidor: opcional, "s01" para 192.168.1.101, "s02" para 192.168.1.102, "s03" para 192.168.1.103, "s04" para 192.168.1.104, o nada para local (127.0.0.1)
 */

// Función para obtener la marca de tiempo actual
char *timetag() {
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  static char buf[20];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", t);
  return buf;
}

// Función principal del servidor
int main(int argc, char *argv[]) {
  // Determina la dirección IP de enlace según parámetro
  char *ip = SERV_LOCAL;
  if (argv[1] && strcmp(argv[1], "s01") == 0)
    ip = SERV_S01;
  else if (argv[1] && strcmp(argv[1], "s02") == 0)
    ip = SERV_S02;
  else if (argv[1] && strcmp(argv[1], "s03") == 0)
    ip = SERV_S03;
  else if (argv[1] && strcmp(argv[1], "s04") == 0)
    ip = SERV_S04;

  // Crea el socket de escucha principal
  int s = socket(AF_INET, SOCK_STREAM, 0);

  // Configura la estructura de dirección (familia, IP y puerto)
  struct sockaddr_in sa = {0};
  sa.sin_family = AF_INET;
  sa.sin_addr.s_addr = inet_addr(ip);
  sa.sin_port = htons(SERV_PORT);

  // Liga el socket y se inicia la escucha en el puerto configurado
  bind(s, (struct sockaddr *)&sa, sizeof(sa));
  listen(s, 5);
  printf("[%s] [SERVIDOR]: Escuchando en %s:%d\n", timetag(), ip, SERV_PORT);

  // Inicializa el conjunto de descriptores para multiplexar conexiones
  fd_set readfds;
  FD_ZERO(&readfds);
  FD_SET(s, &readfds);
  int max_fd = s;
  int client_sockets[10];
  int num_clients = 0;
  int listen_sockets[10];
  int num_listen = 0;
  int next_port = SERV_PORT + 1;

  // Bucle principal de manejo de conexiones
  while (1) {
    fd_set temp = readfds;
    // Espera actividad en los sockets
    select(max_fd + 1, &temp, NULL, NULL, NULL);

    // Acepta la conexión inicial del cliente en el puerto principal
    if (FD_ISSET(s, &temp)) {
      // Acepta nueva conexión inicial
      int c = accept(s, NULL, NULL);
      if (c >= 0) {
        printf("[%s] [SERVIDOR]: Cliente conectado\n", timetag());
        // Reasigna al cliente a un puerto dinámico y se crea un socket adicional
        int new_port = next_port++;
        // Crea socket de escucha para el nuevo puerto
        int ls = socket(AF_INET, SOCK_STREAM, 0);
        // Configura dirección para el nuevo puerto
        struct sockaddr_in lsa = {0};
        lsa.sin_family = AF_INET;
        lsa.sin_addr.s_addr = inet_addr(ip);
        lsa.sin_port = htons(new_port);
        // Liga el socket al nuevo puerto
        bind(ls, (struct sockaddr *)&lsa, sizeof(lsa));
        // Inicia escucha en el nuevo puerto
        listen(ls, 5);
        // Agrega el socket de escucha al conjunto
        FD_SET(ls, &readfds);
        listen_sockets[num_listen++] = ls;
        // Actualiza el descriptor máximo si es necesario
        if (ls > max_fd)
          max_fd = ls;
        // Prepara mensaje con el nuevo puerto
        char msg[100];
        sprintf(msg, "%d", new_port);
        // Envía el puerto reasignado al cliente
        write(c, msg, strlen(msg));
        // Cierra la conexión inicial
        close(c);
        printf("[%s] [SERVIDOR]: Reasignó cliente a puerto %d\n", timetag(), new_port);
      }
    }

    // Verifica conexiones en sockets de escucha adicionales
    for (int i = 0; i < num_listen; i++) {
      int ls = listen_sockets[i];
      if (FD_ISSET(ls, &temp)) {
        // Acepta conexión en puerto dinámico
        int c = accept(ls, NULL, NULL);
        // Si hay espacio para más clientes
        if (c >= 0 && num_clients < 10) {
          // Agrega socket del cliente al conjunto
          FD_SET(c, &readfds);
          client_sockets[num_clients++] = c;
          // Actualiza descriptor máximo
          if (c > max_fd)
            max_fd = c;
        }
      }
    }

    // Maneja comunicación con clientes conectados
    for (int i = 0; i < num_clients; i++) {
      int c = client_sockets[i];
      if (FD_ISSET(c, &temp)) {
        // Prepara mensaje de espera de archivo
        char msg[200];
        sprintf(msg, "[%s] [SERVIDOR]: Esperando archivo del cliente\n", timetag());
        printf("%s", msg);
        // Envía mensaje de espera de archivo al cliente
        write(c, msg, strlen(msg));
        // Lee el nombre del archivo del cliente
        char filename[100] = {0};
        int idx = 0;
        // Lee el nombre del archivo enviado por el cliente
        while (idx < sizeof(filename) - 1) {
          int n = read(c, &filename[idx], 1);
          if (n <= 0)
            break;
          if (filename[idx] == '\n') {
            filename[idx] = '\0';
            break;
          }
          idx++;
        }
        // Si se recibió un nombre de archivo válido
        if (filename[0] != '\0') {
          // Determina la carpeta según el servidor
          char folder[10];
          if (argv[1] && strcmp(argv[1], "s01") == 0)
            strcpy(folder, "s01");
          else if (argv[1] && strcmp(argv[1], "s02") == 0)
            strcpy(folder, "s02");
          else if (argv[1] && strcmp(argv[1], "s03") == 0)
            strcpy(folder, "s03");
          else if (argv[1] && strcmp(argv[1], "s04") == 0)
            strcpy(folder, "s04");
          else
            strcpy(folder, "local");
          // Crea la carpeta correspondiente al servidor
          mkdir(folder, 0755);
          // Construye la ruta completa del archivo
          char path[200];
          sprintf(path, "%s/%s", folder, filename);
          // Abre archivo para escritura
          FILE *fp = fopen(path, "wb");
          // Si el archivo se abrió correctamente
          if (fp) {
            char buf[1024];
            int n;
            // Escribe el archivo recibido en disco
            while ((n = read(c, buf, sizeof(buf))) > 0) {
              fwrite(buf, 1, n, fp);
            }
            fclose(fp);
            // Cierra el archivo
            sprintf(msg, "[%s] [SERVIDOR]: Recibió el archivo: %s\n", timetag(), filename);
            printf("%s", msg);
            // Notifica al cliente la recepción exitosa del archivo
            write(c, msg, strlen(msg));
          }
        }
        // Cierra la conexión con el cliente y se actualiza el conjunto de descriptores
        close(c);
        // Remueve el socket del conjunto
        FD_CLR(c, &readfds);
        // Elimina el socket de la lista de clientes
        client_sockets[i] = client_sockets[--num_clients];
        // Ajusta el índice del bucle
        i--;
        printf("[%s] [SERVIDOR]: Cliente desconectado\n", timetag());
      }
    }
  }
}