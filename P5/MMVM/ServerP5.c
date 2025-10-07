// ServerP5
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <netdb.h>
#include <pthread.h>

// Puerto que va a escuchar
#define LISTEN_PORT 49200
#define BUFSZ 4096
// Podemos manejar hasta 4 alias
#define MAX 4
// Tiempo que cada servidor espera por una conexión
#define TIME 5

// Token para tener un acceso
// Es mutex para tener MUTual EXclusion
pthread_mutex_t token;
// Indice del servidor que tiene el token
int turno = 0;
// Numero de servidores activos
int servidoresAct = 0;

// Estructura para pasar argumentos a cada hilo
typedef struct {
    // id del hilo que corresponde al turno
    int thread_id;
    // socket de escucha de este servidor
    int server_socket;
    // host
    char* hostname;
} thread_args_t;


// ----- Función para crear un socket que escucha en una ip y puerto -----
int crear_socket(const char* hostname) {
    // Almacenar info del host y definir la dirección del socket
    struct hostent *host;
    struct sockaddr_in addr;

    //  Tracucimos el host a una ip
    host = gethostbyname(hostname);
    if (host == NULL) {
        fprintf(stderr, "Error: No se pudo resolver el hostname '%s'\n", hostname);
        return -1;
    }

    // Crear el socket
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) { perror("socket"); return -1; }

    // reutilizarlo para que ya no haya bronca si reinicio el servidor
    int opt = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Preparar la estructura de dirección
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(LISTEN_PORT);
    // Copiamos la dirección ip resultante del host
    memcpy(&addr.sin_addr, host->h_addr_list[0], host->h_length);

    // No le movere a esto
    if (bind(s, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "Error en bind para IP %s\n", hostname);
        perror("bind");
        close(s);
        return -1;
    }
    if (listen(s, 10) < 0) { perror("listen"); close(s); return -1; }

    // Exito
    printf("[SERVIDOR] Escuchando en %s (%s):%d\n", hostname, inet_ntoa(addr.sin_addr), LISTEN_PORT);
    return s;
}

// ----- Función para manejar al cliente y guardar el archivo -----
void manejar_cliente(int client_socket, const char* save_dir) {

    char buffer[BUFSZ];
    char filepath[256];
    // Nombre del archivo
    snprintf(filepath, sizeof(filepath), "%s/recibido_%ld.txt", save_dir, time(NULL));

    // Abrir el archivo
    FILE *fp = fopen(filepath, "wb");

    // Just in case...
    if (fp == NULL) {
        perror("fopen");
        close(client_socket);
        return;
    }

    ssize_t bytes_read;
    // Mientras haya  datos que leer, se escriben en el archivo
    while ((bytes_read = recv(client_socket, buffer, BUFSZ, 0)) > 0) {
        fwrite(buffer, 1, bytes_read, fp);
    }

    // Cerrar archivo y socket
    fclose(fp);
    printf("[SERVIDOR] Archivo guardado en: %s\n", filepath);
    close(client_socket);
}

// ----- Nueva lógica con hilos -----
void* serverConHilos(void* args_ptr) {

    thread_args_t *args = (thread_args_t*)args_ptr;
    fd_set read_fds;
    struct timeval timeout;

    printf("[HILO %d] Servidor para '%s' iniciado.\n", args->thread_id, args->hostname);

    // Ciclo infinito para cada hilo
    while(1) {
        // Esperar el turno
        int turnito = 0;
        while (!turnito) {
            // Cerramos el candado para leer el 'turno' de forma segura.
            pthread_mutex_lock(&token);

            if (turno == args->thread_id) {
                // Si es su turno, sale del ciclo
                turnito = 1;
            }
            // Abrimos el candado
            pthread_mutex_unlock(&token);
            // Si no su turno, esperar un poco
            if (!turnito) {
                sleep(1);
            }
        }

        printf("\n[TURNO %d] Servidor '%s' tiene el token. Escuchando por %d segundos...\n", args->thread_id, args->hostname, TIME);

        FD_ZERO(&read_fds);
        FD_SET(args->server_socket, &read_fds);
        timeout.tv_sec = TIME;
        timeout.tv_usec = 0;
        // Una vez que tiene el token, usamos select con un timeout para esperar una conexión
        int activity = select(args->server_socket + 1, &read_fds, NULL, NULL, &timeout);

        if (activity < 0) {
            perror("select error");
        } else if (activity > 0) {
            // Si hubo actividad, entonces llego un cliente. Se acepta y lo maneja.
            printf("--> [TURNO %d] '%s' acepta una conexión.\n", args->thread_id, args->hostname);
            int new_socket = accept(args->server_socket, NULL, NULL);
            // Como tenemos que utilizar el turno para recibir todo el archivo sin importar el tiempo,
            // ejecutamos "manejar_cliente"
            manejar_cliente(new_socket, args->hostname);
        } else {
            // Se acabó el tiempo y no llego nadie :(
            printf("--> [TURNO %d] '%s' no recibió conexiones (timeout).\n", args->thread_id, args->hostname);
        }

        // Ya sea que haya recibido un archivo o no, cedemos el turno al siguiente
        pthread_mutex_lock(&token);
        turno = (turno + 1) % servidoresAct;
        printf("[TOKEN] Turno pasado al servidor #%d\n", turno);
        pthread_mutex_unlock(&token);
    }

    free(args);
    return NULL;
}

// ----- main -----
int main(int argc, char *argv[]) {
    // Ver que todo cool
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <hostname1> <hostname2> ...\n", argv[0]);
        return 1;
    }

    servidoresAct = argc - 1;

    if (servidoresAct > MAX) {
        fprintf(stderr, "Error: Demasiados servidores, máximo %d\n", MAX);
        return 1;
    }

    // Arreglo para guardar los id de los hilos.
    pthread_t threads[MAX];

    // Iniciar el mutex que protegera la variable del turno
    if (pthread_mutex_init(&token, NULL) != 0) {
        perror("Mutex init ha fallado");
        return 1;
    }

    // Crear un socket y un hilo para cada alias
    for (int i = 0; i < servidoresAct; i++) {
        int sock = crear_socket(argv[i + 1]);
        if (sock < 0) {
            fprintf(stderr, "No se pudo crear el socket para %s. Saliendo...\n", argv[i + 1]);
            return 1;
        }

        // Preparar argumentos para el hilo
        thread_args_t *args = malloc(sizeof(thread_args_t));
        args->thread_id = i;
        args->server_socket = sock;
        args->hostname = argv[i + 1];

        // Crear y lanzar el hilo
        if (pthread_create(&threads[i], NULL, serverConHilos, args) != 0) {
            perror("pthread_create falló");
            return 1;
        }
    }

    // Esperar a que todos los hilos terminen. Pero en este caso nunca pasará
    for (int i = 0; i < servidoresAct; i++) {
        pthread_join(threads[i], NULL);
    }

    // Libera los recursos del candado
    pthread_mutex_destroy(&token);

    return 0;
}
// El fin :)

