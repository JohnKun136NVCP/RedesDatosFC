// ===== Programa cliente TCP =====
// Este programa se conecta a un servidor TCP y envía dos valores
// (clave y shift). Para leer entradas de usuario de forma segura,
// se utiliza fgets() en lugar de gets() o scanf sin límites.

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Puerto del servidor y tamaño de buffers de E/S
#define PORT 7006
#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    // Buffers para las credenciales y comunicación
    char clave[BUFFER_SIZE];        // Almacena la clave a enviar
    char shift_str[BUFFER_SIZE];    // Almacena el desplazamiento (como texto)
    char buffer[BUFFER_SIZE] = {0}; // Buffer de recepción
    char mensaje[BUFFER_SIZE];      // Mensaje de salida al servidor

    // Dirección IP del servidor (sustituir por la IP real)
    char *server_ip = "XXX.XXX.XXX.XXX";

    // Si se proporcionan parámetros por línea de comandos, úsalos; de lo contrario, pide entrada con fgets()
    if (argc == 3) {
        // Copia segura de los argumentos a los buffers locales
        strncpy(clave, argv[1], sizeof(clave) - 1);
        clave[sizeof(clave) - 1] = '\0';
        strncpy(shift_str, argv[2], sizeof(shift_str) - 1);
        shift_str[sizeof(shift_str) - 1] = '\0';
    } else {
        // Solicita la clave al usuario usando fgets() y elimina el salto de línea
        printf("Ingrese la clave: ");
        if (fgets(clave, sizeof(clave), stdin) == NULL) {
            fprintf(stderr, "Error al leer la clave.\n");
            return 1;
        }
        size_t len = strlen(clave);
        if (len > 0 && clave[len - 1] == '\n') {
            clave[len - 1] = '\0';
        }

        // Solicita el shift al usuario usando fgets() y elimina el salto de línea
        printf("Ingrese el shift: ");
        if (fgets(shift_str, sizeof(shift_str), stdin) == NULL) {
            fprintf(stderr, "Error al leer el shift.\n");
            return 1;
        }
        len = strlen(shift_str);
        if (len > 0 && shift_str[len - 1] == '\n') {
            shift_str[len - 1] = '\0';
        }
    }

    // Crea el socket TCP (AF_INET + SOCK_STREAM)
    int client_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (client_sock == -1) {
        perror("[-] Error to create the socket");
        return 1;
    }

    // Rellena la estructura con la dirección del servidor
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    serv_addr.sin_addr.s_addr = inet_addr(server_ip);

    // Intenta conectar con el servidor
    if (connect(client_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("[-] Error to connect");
        close(client_sock);
        return 1;
    }
    printf("[+] Connected to server\n");

    // Construye el mensaje de salida "<clave> <shift>" y lo envía
    snprintf(mensaje, sizeof(mensaje), "%s %s", clave, shift_str);
    send(client_sock, mensaje, strlen(mensaje), 0);
    printf("[+][Client] Key and shift was sent: %s\n", mensaje);

    // Recibe la respuesta inicial del servidor (por ejemplo, estado de acceso)
    int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes > 0) {
        buffer[bytes] = '\0';
        printf("[+][Client] Server message: %s\n", buffer);

        // Si el acceso fue concedido, el servidor enviará un archivo; guárdalo como info.txt
        if (strstr(buffer, "ACCESS GRANTED") != NULL) {
            FILE *fp = fopen("info.txt", "w");
            if (fp == NULL) {
                perror("[-] Error to open the file");
                close(client_sock);
                return 1;
            }

            // Lee en bloques hasta que el servidor cierre la conexión
            while ((bytes = recv(client_sock, buffer, sizeof(buffer), 0)) > 0) {
                fwrite(buffer, 1, bytes, fp);
            }
            printf("[+][Client] The file was save like 'info.txt'\n");
            fclose(fp);
        }
    } else {
        printf("[-] Server connection tiemeout\n");
    }

    // Cierra el socket antes de salir
    close(client_sock);
    return 0;
}
