// ServerP4 (Parte B)
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

//Puerto que va a escuchar
#define LISTEN_PORT 49200
#define BUFSZ 4096
// Podemos manejar hasta 4 alias
#define MAX 4

// ----- Función para crear un socket que escucha en una ip y puerto -----
// Lo modificamos para la parte B
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
// Esta función se queda igual
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

// ----- main -----
// Para la parte B ahora aceptamos argumentos.
int main(int argc, char *argv[]) {
   // Ver que todo  cool
   if (argc < 2) {
        fprintf(stderr, "Uso: %s <hostname1> <hostname2> ...\n", argv[0]);
        return 1;
    }

    // numero de alias 
    int num_servers = argc - 1;
    if (num_servers > MAX) {
        fprintf(stderr, "Error: Demasiados servidores, máximo %d\n", MAX);
        return 1;
    }

    // Para guardar los sockets
    int server_sockets[MAX];
    // Para guardar el socket mas alto
    int max_sd = 0;

    // Bucle para crear un socket por cada alias que pusimos como argumento
    for (int i = 0; i < num_servers; i++) {
        server_sockets[i] = crear_socket(argv[i + 1]);
        if (server_sockets[i] < 0) {
            fprintf(stderr, "No se pudo crear el socket para %s. Saliendo...\n", argv[i + 1]);
            return 1;
        }
        if (server_sockets[i] > max_sd) {
            max_sd = server_sockets[i];
        }
    }

    fd_set read_fds;
    // While para el servidor
    while(1) {
        FD_ZERO(&read_fds);
        // añadimos todos los sockets al conjunto
        for (int i = 0; i < num_servers; i++) {
            FD_SET(server_sockets[i], &read_fds);
        }

	// wsperar a que haya actividad de algun socktet
        select(max_sd + 1, &read_fds, NULL, NULL, NULL);

        // for para comprobar que socket recibio una conexión
        for (int i = 0; i < num_servers; i++) {
            if (FD_ISSET(server_sockets[i], &read_fds)) {
                int new_socket = accept(server_sockets[i], NULL, NULL);
                printf("\n[SERVIDOR] Conexión aceptada en %s\n", argv[i + 1]);
                // Para el directorio usamos el mismo nombre que el alias
                manejar_cliente(new_socket, argv[i + 1]);
            }
        }
    }
    return 0;
}
// El fin :)


