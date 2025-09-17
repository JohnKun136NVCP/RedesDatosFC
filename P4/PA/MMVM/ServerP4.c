// ServerP4
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

//Puerto que va a escuchar
#define LISTEN_PORT 49200
#define BUFSZ 4096

// ----- Función para crear un socket que escucha en una ip y puerto -----
int crear_socket(const char* ip) {

    // HAcemos el socket
    int s = socket(AF_INET, SOCK_STREAM, 0);

    if (s < 0) { perror("socket"); return -1; }

    // Por si las flys, vamos a dejar que se pueda reutilizar
    int opt = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Preparamos ip y puerto
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(LISTEN_PORT);
    addr.sin_addr.s_addr = inet_addr(ip);

    // bind entre socket y puerto
    if (bind(s, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "Error en bind para IP %s\n", ip);
        perror("bind");
        close(s);
        return -1;
    }

    // socket con el oido al tiro
    if (listen(s, 10) < 0) { perror("listen"); close(s); return -1; }
    printf("[SERVIDOR] Escuchando en %s:%d\n", ip, LISTEN_PORT);

    // Regreasmos el socket
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

// ----- main -----
int main(void) {

    // Hacemos los dos sockets requeridos
    int socket1 = crear_socket("192.168.1.101");
    int socket2 = crear_socket("192.168.1.102");

    if (socket1 < 0 || socket2 < 0) {
        printf("Error al crear los sockets de escucha. Saliendo.\n");
        return 1;
    }

    fd_set read_fds;
    // socket mayor. Esto para el select()
    int max_sd = (socket1 > socket2) ? socket1 : socket2;

    // Ciclo infinito para atender a los clientes 
    while(1) {
        // Añadir los dos sockets
        FD_ZERO(&read_fds);
        FD_SET(socket1, &read_fds);
        FD_SET(socket2, &read_fds);
	// select(). Esperamos conexión
        int activity = select(max_sd + 1, &read_fds, NULL, NULL, NULL);

        if ((activity < 0) && (errno != EINTR)) {
            perror("select");
        }

	// Ahora vemos que socket rrecibio la conexión
        if (FD_ISSET(socket1, &read_fds)) {
            int new_socket = accept(socket1, NULL, NULL);
            printf("\n[SERVIDOR] Conexión aceptada en s01 (192.168.1.101)\n");
            manejar_cliente(new_socket, "s01");
        }

        if (FD_ISSET(socket2, &read_fds)) {
            int new_socket = accept(socket2, NULL, NULL);
            printf("\n[SERVIDOR] Conexión aceptada en s02 (192.168.1.102)\n");
            manejar_cliente(new_socket, "s02");
        }

    }
    return 0;
}
// El fin.

