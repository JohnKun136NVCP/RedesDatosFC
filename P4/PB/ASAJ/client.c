// cliente.c
#include <arpa/inet.h>
#include <netdb.h> // lo usamos para que podamos obtener la IP correspondiente a s01/2/3/4
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define TAM_BUFFER                                                             \
  65536 // tamaño del buffer para comunicación, lo agrandamos por si toca un
        // .txt largo en el randomized del script

// función para registrar eventos en la terminal y en archivo log
void registrar_evento(const char *estado) {
  FILE *f =
      fopen("cliente.log", "a"); // crea un archivo cliente.log en modo append
  if (!f)
    return;

  time_t ahora = time(NULL); // obtenemos la hora actual para llevar registro
  char marca_tiempo[64];
  strftime(marca_tiempo, sizeof(marca_tiempo), "%Y-%m-%d %H:%M:%S",
           localtime(&ahora));

  fprintf(f, "[%s] %s\n", marca_tiempo, estado); // escribimos en el archivo
  fclose(f);

  printf("[%s] %s\n", marca_tiempo,
         estado); // también lo mostramos en la terminal
}

// función que se encarga de la creación de un socket y la conexión al servidor
int conectar_a(const char *host, int puerto) {
  int socket_cliente;
  struct sockaddr_in dir_servidor;
  struct hostent *servidor;

  // crear socket
  if ((socket_cliente = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("Error al crear socket del cliente");
    exit(1);
  }

  // resolver nombre de host (sirve para alias tipo s01/2/3/4 o IP directa)
  servidor = gethostbyname(host);
  if (servidor == NULL) {
    fprintf(stderr, "Error en el alias de red %s\n", host);
    exit(1);
  }

  memset(&dir_servidor, 0, sizeof(dir_servidor));
  dir_servidor.sin_family = AF_INET;
  dir_servidor.sin_port = htons(puerto);
  memcpy(&dir_servidor.sin_addr, servidor->h_addr, servidor->h_length);

  // ahora sí conectar
  if (connect(socket_cliente, (struct sockaddr *)&dir_servidor,
              sizeof(dir_servidor)) < 0) {
    perror("Error al conectar con el servidor");
    exit(1);
  }

  return socket_cliente;
}

// función auxiliar que lee archivo para enviar
int leer_archivo(const char *nombre, char *buffer, int cap) {
  FILE *f = fopen(nombre, "r");
  if (!f)
    return 0;
  int len = fread(buffer, 1, cap - 1, f);
  buffer[len] = '\0';
  fclose(f);
  return len;
}

int main(int argc, char *argv[]) {

  // veificamos que se pasen los argumentos correctos aunque esto será para el
  // sender.sh
  if (argc < 4) {
    fprintf(stderr, "Uso: %s <ALIAS_PROPIO> <PUERTO_BASE> <ARCHIVO_A_ENVIAR>\n",
            argv[0]);
    exit(1);
  }

  const char *ALIAS_PROPIO = argv[1];
  int PUERTO_BASE = atoi(argv[2]);
  const char *ARCHIVO = argv[3];
  srand(time(NULL)); //  semilla para rand()

  // agregamos todos los alias que hemos establecido
  const char *alias_servidores[] = {"s01", "s02", "s03", "s04"};
  int num_servidores = 4;

  registrar_evento("Cliente de estado y envío iniciado.");

  // recorremos toda la lista de servidores para conectarnos a cada uno
  for (int i = 0; i < num_servidores; i++) {
    // menos a nosotros mismos
    if (strcmp(alias_servidores[i], ALIAS_PROPIO) == 0) {
      continue;
    }

    char mensaje[256];
    snprintf(mensaje, sizeof(mensaje), "Iniciando proceso para servidor %s",
             alias_servidores[i]);
    registrar_evento(mensaje);

    // nos conectamos al puerto base para que nos asignen un puerto de datos
    int sock_base = conectar_a(alias_servidores[i], PUERTO_BASE);
    if (sock_base < 0) {
      snprintf(mensaje, sizeof(mensaje),
               "Error al conectar con %s en puerto base. Me lo saltaré.",
               alias_servidores[i]);
      registrar_evento(mensaje);
      continue; // si no podemos conectarnos, pasamos al siguiente servidor
    }

    // recibir el nuevo puerto que nos asignó el servidor padre
    char buffer_puerto[32];
    int n = recv(sock_base, buffer_puerto, sizeof(buffer_puerto) - 1, 0);
    if (n <= 0) {
      registrar_evento("Error al recibir el puerto asignado del servidor.");
      close(sock_base);
      continue;
    }
    buffer_puerto[n] = '\0';
    int puerto_nuevo = atoi(buffer_puerto);
    close(sock_base); // cerramos la conexión inicial

    snprintf(mensaje, sizeof(mensaje), "Servidor %s asignó puerto de datos %d",
             alias_servidores[i], puerto_nuevo);
    registrar_evento(mensaje);

    // pequeña pausa para darle chance  al servidor de preparar el nuevo socket
    sleep(1);

    // ahora sí nos conectamos al puerto para enviar el archivo
    int sock_datos = conectar_a(alias_servidores[i], puerto_nuevo);
    if (sock_datos < 0) {
      snprintf(mensaje, sizeof(mensaje),
               "Error al conectar con %s en el puerto de datos %d.",
               alias_servidores[i], puerto_nuevo);
      registrar_evento(mensaje);
      continue;
    }

    // esperamos un tiempo aleatorio antes de mandar archivo
    int espera = (rand() % 5) + 1; // entre 1 y 5 segundos
    char mensaje_espera[128];
    snprintf(mensaje_espera, sizeof(mensaje_espera),
             "Esperando %d segundos antes de enviar a %s", espera,
             alias_servidores[i]);
    registrar_evento(mensaje_espera);
    sleep(espera);

    // leemos el archivo y se lo vamos enviando
    char buffer_archivo[TAM_BUFFER];
    int long_archivo =
        leer_archivo(ARCHIVO, buffer_archivo, sizeof(buffer_archivo));

    if (long_archivo >= 0) {
      send(sock_datos, buffer_archivo, long_archivo, 0);
      snprintf(mensaje, sizeof(mensaje), "Enviando archivo '%s' a %s:%d",
               ARCHIVO, alias_servidores[i], puerto_nuevo);
      registrar_evento(mensaje);
    } else {
      snprintf(mensaje, sizeof(mensaje),
               "No se pudo leer el archivo '%s'. Saltando envío a %s.", ARCHIVO,
               alias_servidores[i]);
      registrar_evento(mensaje);
    }

    // cerramos la escritura para que el servidor sepa que terminamos de enviar
    // y no se quede esperando eternamente
    shutdown(sock_datos, SHUT_WR);

    // esperamos y mostramos en terminal la confirmación del servidor
    char buffer_respuesta[1024];
    n = recv(sock_datos, buffer_respuesta, sizeof(buffer_respuesta) - 1, 0);
    if (n > 0) {
      buffer_respuesta[n] = '\0';
      printf("Respuesta de %s: %s\n", alias_servidores[i], buffer_respuesta);
    }

    close(sock_datos); // cerramos la conexión de datos
    registrar_evento("---------------------------------");
  }

  registrar_evento("Cliente terminó todas las conexiones.");
  return 0;
}
