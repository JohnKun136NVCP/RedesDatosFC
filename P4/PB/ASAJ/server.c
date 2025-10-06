// servidor.c
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define PUERTO_BASE 49200
#define TAM_BUFFER                                                             \
  65536 // tamaño del buffer para comunicación, le subimos por si toca un
        // archivo grande en el randomized, igual que en el cliente

// función que se encarga de todas las conexiones para un solo alias
// antes todo estaba amontonado en el main, ahora como lo vamos a usar para
// varios lo separamos
void ejecutar_servidor(const char *alias) {
  int socket_escucha;
  struct sockaddr_in dir_servidor, dir_cliente;
  socklen_t tam_cliente = sizeof(dir_cliente);
  char buffer[TAM_BUFFER];

  // a cada alias le damos un offset a su contador de puertos para que podamos
  // identificar a ojo quien asignó puerto
  int siguiente_puerto = PUERTO_BASE + 1;
  if (strcmp(alias, "s02") == 0)
    siguiente_puerto += 100;
  if (strcmp(alias, "s03") == 0)
    siguiente_puerto += 200;
  if (strcmp(alias, "s04") == 0)
    siguiente_puerto += 300;

  // configuramos el socket principal que escuchará siempre

  // ahora, en lugar de escuchar en todos lados con INADDR_ANY, buscamos la IP
  // específica del alias
  struct hostent *host_info = gethostbyname(alias);
  if (host_info == NULL) {
    fprintf(stderr, "[%s] Error en el alias de red\n", alias);
    exit(1);
  }
  struct in_addr *direccion_ip = (struct in_addr *)host_info->h_addr_list[0];

  // creamos el socket, igual que antes
  if ((socket_escucha = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("No se pudo crear el socket de escucha");
    exit(1);
  }

  // preparamos la dirección del server
  memset(&dir_servidor, 0, sizeof(dir_servidor));
  dir_servidor.sin_family = AF_INET;
  dir_servidor.sin_port = htons(PUERTO_BASE);

  // le decimos que se vincule solamente a la IP del alias
  memcpy(&dir_servidor.sin_addr, direccion_ip, sizeof(struct in_addr));

  // nos vinculamos con bind
  if (bind(socket_escucha, (struct sockaddr *)&dir_servidor,
           sizeof(dir_servidor)) < 0) {
    fprintf(stderr, "Error al vincularse con bind para %s (%s)\n", alias,
            inet_ntoa(*direccion_ip));
    exit(1);
  }

  // ahora escuchamos a varios clientes pero formados solo puede haber cinco
  listen(socket_escucha, 5);
  printf("Servidor '%s' listo y escuchando en %s:%d\n", alias,
         inet_ntoa(*direccion_ip), PUERTO_BASE);

  // bucle infinito para despachar clientes
  while (1) {
    // aceptamos al cliente que llega al puerto base
    int socket_cliente_base =
        accept(socket_escucha, (struct sockaddr *)&dir_cliente, &tam_cliente);
    if (socket_cliente_base < 0) {
      perror("Falló el accept inicial");
      continue;
    }
    printf("[%s] Cliente nuevo desde %s. Asignandole un puerto...\n", alias,
           inet_ntoa(dir_cliente.sin_addr));

    int puerto_asignado = siguiente_puerto++;

    // ahora preparamos el nuevo socket (bind y listen) antes de avisarle al
    // cliente para asegurarnos
    int socket_datos = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in dir_datos;
    memset(&dir_datos, 0, sizeof(dir_datos));
    dir_datos.sin_family = AF_INET;
    dir_datos.sin_port = htons(puerto_asignado);
    memcpy(&dir_datos.sin_addr, direccion_ip, sizeof(struct in_addr));

    if (bind(socket_datos, (struct sockaddr *)&dir_datos, sizeof(dir_datos)) <
            0 ||
        listen(socket_datos, 1) < 0) {
      perror("No pude preparar el socket en el nuevo puerto");
      close(socket_cliente_base);
      close(socket_datos);
      continue;
    }

    // ahora que el nuevo socket ya está listo y escuchando, le mandamos el
    // número de puerto al cliente.
    snprintf(buffer, sizeof(buffer), "%d", puerto_asignado);
    send(socket_cliente_base, buffer, strlen(buffer), 0);
    close(socket_cliente_base); // cerramos la conexión inicial

    printf("[%s] Puerto %d asignado. Esperando que el cliente se conecte...\n",
           alias, puerto_asignado);

    // aceptamos al mismo cliente, pero ahora en su puerto privado.
    int conexion_datos =
        accept(socket_datos, (struct sockaddr *)&dir_cliente, &tam_cliente);
    close(
        socket_datos); // cerramos este socket de escucha, ya no lo necesitamos.

    if (conexion_datos < 0) {
      perror("El cliente no se conectó al puerto nuevo");
      continue;
    }
    printf("[%s] El cliente llegó al puerto %d.\n", alias, puerto_asignado);

    // Obtenemos la ruta del home del usuario
    const char *home_dir = getenv("HOME");
    if (home_dir == NULL) {
      fprintf(stderr, "[%s] No se pudo obtener el directorio HOME.\n", alias);
      continue; // Si no encuentra el home, no puede continuar con este cliente.
    }

    // Creamos la ruta completa: /home/usuario/s01 (o el alias que corresponda)
    char ruta_completa_dir[256];
    snprintf(ruta_completa_dir, sizeof(ruta_completa_dir), "%s/%s", home_dir,
             alias);

    char comando_mkdir[300];
    snprintf(comando_mkdir, sizeof(comando_mkdir), "mkdir -p %s",
             ruta_completa_dir);
    system(comando_mkdir);

    // para no sobreescribir, le ponemos la hora al nombre del archivo.
    char ruta_archivo[512];
    snprintf(ruta_archivo, sizeof(ruta_archivo), "%s/archivo_recibido_%ld.txt",
             ruta_completa_dir, time(NULL));

    FILE *f = fopen(ruta_archivo,
                    "wb"); // "wb" por si nos mandan algo que no sea texto
                           // plano, por la pregunta teórica de la práctica
    if (!f) {
      perror("No pude crear el archivo para guardar");
      close(conexion_datos);
      continue;
    }

    // leemos todo lo que el cliente mande hasta que cierre la conexión
    int bytes_leidos;
    while ((bytes_leidos = recv(conexion_datos, buffer, TAM_BUFFER, 0)) > 0) {
      fwrite(buffer, 1, bytes_leidos, f);
    }
    fclose(f);

    printf("[%s] Archivo guardado. Se encuentra en '%s'\n", alias,
           ruta_archivo);

    // le decimos ok
    send(conexion_datos, "OK: Archivo recibido\n", 21, 0);
    close(conexion_datos);
  }

  close(socket_escucha);
}

int main(int argc, char *argv[]) {
  // checa si hay al menos un alias (
  if (argc < 2) {
    fprintf(stderr, "Uso: %s <alias1> <alias2> ...\n", argv[0]);
    exit(1);
  }

  // creamos un proceso hijo por cada alias
  for (int i = 1; i < argc; i++) {
    pid_t pid = fork();
    if (pid == 0) {               // si soy el hijo
      ejecutar_servidor(argv[i]); // trabajo
      exit(0);                    // ya no entro al for
    }
  }

  // el proceso padre se queda esperando a los hijos.
  for (int i = 1; i < argc; i++) {
    wait(NULL);
  }

  return 0;
}
