#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <time.h>
#include <unistd.h>

#define SERV_LOCAL "127.0.0.1"
#define SERV_S01 "192.168.1.101"
#define SERV_S02 "192.168.1.102"
#define SERV_S03 "192.168.1.103"
#define SERV_S04 "192.168.1.104"
#define SERV_PORT 49200
#define TURN_FILE "turno.txt"
#define TURN_DURATION 10
#define NUM_SERVERS 4

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

// Función para obtener el ID del servidor actual
int get_server_id(char *arg) {
  if (!arg) return 0;
  if (strcmp(arg, "s01") == 0) return 1;
  if (strcmp(arg, "s02") == 0) return 2;
  if (strcmp(arg, "s03") == 0) return 3;
  if (strcmp(arg, "s04") == 0) return 4;
  return 0;
}

// Función para verificar si es el turno del servidor
int is_my_turn(int my_id) {
  FILE *f = fopen(TURN_FILE, "r+");
  if (!f) {
    f = fopen(TURN_FILE, "w");
    if (f) {
      flock(fileno(f), LOCK_EX);
      fprintf(f, "%d %ld", 1, time(NULL));
      fflush(f);
      flock(fileno(f), LOCK_UN);
      fclose(f);
    }
    return my_id == 1;
  }
  
  flock(fileno(f), LOCK_EX);
  int current_turn;
  time_t start_time;
  fscanf(f, "%d %ld", &current_turn, &start_time);
  
  time_t now = time(NULL);
  int result;
  
  if (now - start_time >= TURN_DURATION) {
    int next_turn = (current_turn % NUM_SERVERS) + 1;
    fseek(f, 0, SEEK_SET);
    fprintf(f, "%d %ld", next_turn, now);
    fflush(f);
    result = (my_id == next_turn);
  } else {
    result = (my_id == current_turn);
  }
  
  flock(fileno(f), LOCK_UN);
  fclose(f);
  return result;
}

// Función para pasar al siguiente turno
void pass_turn(int my_id) {
  FILE *f = fopen(TURN_FILE, "r+");
  if (!f) return;
  
  flock(fileno(f), LOCK_EX);
  int current_turn;
  time_t start_time;
  fscanf(f, "%d %ld", &current_turn, &start_time);
  
  if (current_turn == my_id) {
    int next_turn = (my_id % NUM_SERVERS) + 1;
    fseek(f, 0, SEEK_SET);
    fprintf(f, "%d %ld", next_turn, time(NULL));
    fflush(f);
  }
  
  flock(fileno(f), LOCK_UN);
  fclose(f);
}

// Función principal del servidor
int main(int argc, char *argv[]) {
  // Determina la dirección IP de enlace según parámetro
  char *ip = SERV_LOCAL;
  int server_id = get_server_id(argv[1]);
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
  printf("[%s] [SERVIDOR]: Escuchando en %s:%d\n\n", timetag(), ip, SERV_PORT);

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

  // Variables para control de turno
  int my_turn_active = 0;
  time_t turn_start = 0;

  // Bucle principal de manejo de conexiones
  while (1) {
    fd_set temp = readfds;
    struct timeval tv = {0, 100000};
    // Espera actividad en los sockets con timeout de 100ms
    select(max_fd + 1, &temp, NULL, NULL, &tv);

    // Verifica si es mi turno
    int current_is_my_turn = is_my_turn(server_id);
    
    // Detecta inicio de turno
    if (current_is_my_turn && !my_turn_active) {
      my_turn_active = 1;
      turn_start = time(NULL);
      printf("[%s] [SERVIDOR]: Inicio de turno\n", timetag());
    }
    
    // Detecta fin de turno por timeout
    if (my_turn_active && !current_is_my_turn) {
      my_turn_active = 0;
      printf("[%s] [SERVIDOR]: Fin de turno\n\n", timetag());
    }

    // Acepta la conexión inicial del cliente en el puerto principal solo si es mi turno
    if (FD_ISSET(s, &temp) && my_turn_active) {
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

    // Verifica conexiones en sockets de escucha adicionales solo si es mi turno
    if (my_turn_active) {
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
    }

    // Maneja comunicación con clientes conectados solo si es mi turno
    if (my_turn_active) {
      int has_reception = 0;
      for (int i = 0; i < num_clients; i++) {
        int c = client_sockets[i];
        if (FD_ISSET(c, &temp)) {
          has_reception = 1;
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
      
      // Si no hay recepción en espera, pasa el turno
      if (!has_reception && num_clients == 0) {
        time_t now = time(NULL);
        if (now - turn_start >= 5) {
          printf("[%s] [SERVIDOR]: Sin recepción en espera, cediendo turno\n", timetag());
          pass_turn(server_id);
          my_turn_active = 0;
          printf("[%s] [SERVIDOR]: Fin de turno\n\n", timetag());
        }
      }
    }
  }
}