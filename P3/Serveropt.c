//Serveropt.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/wait.h>

#define BUFFER_SIZE 2048

// Función para cifrar el texto con el cifrado César
void encryptCaesar(char *text, int shift) {
    shift = shift % 26;
    for (int i = 0; text[i] != '\0'; i++) {
        char c = text[i];
        if (isupper(c)) {
            text[i] = ((c - 'A' + shift) % 26) + 'A';
        } else if (islower(c)) {
            text[i] = ((c - 'a' + shift) % 26) + 'a';
        }
    }
}

// Manejador de señal para liberar recursos de procesos hijos terminados
void sigchld_handler(int s) {
    // waitpid con WNOHANG para no bloquear y limpiar procesos hijos finalizados
    while(waitpid(-1, NULL, WNOHANG) > 0);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <puerto>\n", argv[0]);
        return 1;
    }

    int puerto_escucha = atoi(argv[1]);
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];

    // Crear socket TCP
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("[-] Error al crear el socket");
        exit(1);
    }

    // Permitir reutilizar el puerto inmediatamente después de cerrar el servidor
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Configurar dirección y puerto del servidor
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(puerto_escucha);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // Asociar socket con la dirección configurada
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("[-] Error al hacer bind");
        close(server_fd);
        exit(1);
    }

    // Poner el socket en modo escucha con cola para hasta 10 conexiones
    if (listen(server_fd, 10) < 0) {
        perror("[-] Error al escuchar");
        close(server_fd);
        exit(1);
    }

    printf("[+] Servidor escuchando en puerto %d...\n", puerto_escucha);

    // Configurar el manejador de señal SIGCHLD para limpiar procesos hijos terminados
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART; // Reinicia llamadas interrumpidas
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    // Bucle principal para aceptar conexiones entrantes
    while (1) {
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_fd < 0) {
            perror("[-] Error al aceptar conexión");
            continue;
        }

        printf("[+] Cliente conectado\n");

        // Crear un proceso hijo para atender al cliente
        pid_t pid = fork();
        if (pid < 0) {
            perror("[-] Error en fork");
            close(client_fd);
            continue;
        }

        if (pid == 0) {  // Proceso hijo
            close(server_fd); // El hijo no necesita el socket de escucha

            // Recibir datos del cliente
            int bytes_received = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
            if (bytes_received <= 0) {
                perror("[-] Error al recibir datos");
                close(client_fd);
                exit(1);
            }

            buffer[bytes_received] = '\0';

            int puerto_objetivo, shift;
            char texto[BUFFER_SIZE];

            // Extraer los datos recibidos: puerto, shift y texto
            sscanf(buffer, "%d %d %[^\t\n]", &puerto_objetivo, &shift, texto);

            printf("[+] Puerto objetivo recibido: %d\n", puerto_objetivo);
            printf("[+] Shift: %d\n", shift);
            printf("[+] Texto recibido: %s\n", texto);

            // Verificar que el puerto objetivo coincide con el puerto de escucha
            if (puerto_objetivo == puerto_escucha) {
                encryptCaesar(texto, shift);  // Cifrar el texto
                char respuesta[BUFFER_SIZE];
                snprintf(respuesta, sizeof(respuesta), "Procesado: %s", texto);
                send(client_fd, respuesta, strlen(respuesta), 0);
                printf("[+] Texto cifrado enviado\n");
            } else {
                char *rechazo = "Rechazado";
                send(client_fd, rechazo, strlen(rechazo), 0);
                printf("[-] Solicitud rechazada (puerto no coincide)\n");
            }

            close(client_fd);
            exit(0);  // Terminar proceso hijo
        } else {
            // Proceso padre cierra el socket del cliente y continúa aceptando conexiones
            close(client_fd);
        }
    }

    close(server_fd);
    return 0;
}
