/*
 * Práctica III. Cliente - Servidor.
 *
 * Servidor que implementa un sistema de procesamiento de archivos a múltiples clientes
 * usando sockets TCP. Cada servidor decide si procesa el archivo según el puerto solicitado.
 *
 * Compilación:
 *   gcc Server.c -o server
 *
 * Uso:
 *   ./Server <PUERTO>
 *
 * Ejemplos (ejecutar en 3 terminales):
 *   ./Server 49200
 *   ./Server 49201
 *   ./Server 49202
 * 
 * @author steve-quezada
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>

#define BUFFER_SIZE 4096

// Códigos ANSI para formato de texto en consola
#define BOLD "\033[1m"
#define RESET "\033[0m"
#define GREEN "\033[32m"
#define RED "\033[31m"
#define YELLOW "\033[33m"
#define BLUE "\033[34m"
#define CYAN "\033[36m"
#define TEAL "\033[96m"
#define MAGENTA "\033[35m"

// Función para cifrar texto usando cifrado César
// Referencia: server.c (función decryptCaesar)
// La diferencia principal es que sumamos el shift en lugar de restarlo
void Ejercicio_1_QOKS(char *text, int shift) {
    shift = shift % 26;
    for (int i = 0; text[i] != '\0'; i++) {
        char c = text[i];
        if (isupper(c)) {
            // Cifrar letras mayúsculas manteniendo el rango A-Z
            text[i] = ((c - 'A' + shift) % 26) + 'A';
        }
        else if (islower(c)) {
            // Cifrar letras minúsculas manteniendo el rango a-z
            text[i] = ((c - 'a' + shift) % 26) + 'a';
        }
        // Otros caracteres se mantienen igual
    }
}

int main(int argc, char *argv[]) {
    // Argumentos de línea de comandos: ./server <PUERTO>
    if (argc != 2) {
        printf("Uso: %s <PUERTO>\n", argv[0]);
        printf("Puertos válidos: " BOLD "49200, 49201, 49202" RESET "\n");
        return 1;
    }

    // Parámetros Servidor
    int server_port = atoi(argv[1]);             // Puerto de escucha
    int server_sock, client_sock;                // Descriptores de socket
    struct sockaddr_in server_addr, client_addr; // Estructuras de direcciones
    socklen_t client_len = sizeof(client_addr);  // Tamaño de dirección cliente
    char buffer[BUFFER_SIZE];                    // Buffer para recepción de datos
    
    printf(BOLD TEAL "\n[+] Iniciando servidor" RESET "\n");
    printf(BOLD CYAN "[+]   Puerto: " RESET "%d\n", server_port);
    
    // I: Creación socket TCP
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("Error creando socket");
        return 1;
    }

    // II: Configuración de la dirección del servidor
    memset(&server_addr, 0, sizeof(server_addr));  // Limpiar estructura
    server_addr.sin_family = AF_INET;              // Protocolo IPv4
    server_addr.sin_addr.s_addr = INADDR_ANY;      // Escuchar en todas las interfaces
    server_addr.sin_port = htons(server_port);     // Puerto en formato de red

    // III: Enlace del socket al puerto
    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error en bind - Puerto en uso");
        close(server_sock);
        return 1;
    }

    // IV: Configuración del servidor en modo escucha
    if (listen(server_sock, 5) < 0) {
        perror("Error en listen");
        close(server_sock);
        return 1;
    }

    printf(BOLD GREEN "[+] Servidor activo y escuchando\n" RESET);

    // Bucle principal
    while (1) {
        printf(BOLD "\n[+] Esperando nueva conexión" RESET "\n\n");

        // V: Aceptar conexión de cliente
        client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len);
        if (client_sock < 0) {
            perror("Error aceptando conexión");
            continue;
        }

        printf(BOLD GREEN "[+] Cliente conectado desde: " RESET "%s\n", inet_ntoa(client_addr.sin_addr));

        // VI: Recepción de datos del cliente
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(client_sock, buffer, BUFFER_SIZE - 1, 0);
        
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';

            // VII: Análisis del mensaje del cliente
            int target_port, shift;
            char file_content[BUFFER_SIZE];
            
            // Extraer puerto objetivo
            char *first_newline = strchr(buffer, '\n');
            if (!first_newline) {
                printf(RED "[-] Formato de mensaje inválido" RESET "\n");
                close(client_sock);
                continue;
            }
            
            *first_newline = '\0';
            target_port = atoi(buffer);
            
            // Extraer desplazamiento César
            char *second_line = first_newline + 1;
            char *second_newline = strchr(second_line, '\n');
            if (!second_newline) {
                printf(RED "[-] Formato de desplazamiento inválido" RESET "\n");
                close(client_sock);
                continue;
            }
            
            *second_newline = '\0';
            shift = atoi(second_line);
            
            // Extraer contenido del archivo
            char *content_start = second_newline + 1;
            strcpy(file_content, content_start);
            
            printf(CYAN "[+]   Puerto solicitado: " RESET BOLD "%d" RESET "\n", target_port);
            printf(TEAL "[+]   Puerto del servidor: " RESET BOLD "%d" RESET "\n", server_port);
            printf(YELLOW "[+]   Desplazamiento César: " RESET BOLD "%d" RESET "\n", shift);
            printf("[+]   Tamaño: %zu bytes\n\n", strlen(file_content));
                
            // VIII: Decisión de procesamiento según puerto
            if (target_port == server_port) {
                printf(BOLD GREEN "[+] Puerto coincide" RESET "\n");
                
                // Aplicar cifrado César
                Ejercicio_1_QOKS(file_content, shift);

                // Envío de respuesta positiva
                send(client_sock, "PROCESADO\n", 10, 0);
                send(client_sock, file_content, strlen(file_content), 0);
                
                printf(BOLD GREEN "[+]   Archivo procesado y cifrado enviado" RESET "\n");
            } else {
                printf(BOLD RED "[+] Puerto no coincide" RESET "\n");

                // Envío de respuesta negativa
                send(client_sock, "RECHAZADO", 9, 0);
                printf(RED "[+]   Solicitud rechazada (puerto %d ≠ %d)" RESET "\n", target_port, server_port);
            }
        } else {
            printf(RED "[-] Error: No se recibieron datos del cliente" RESET "\n");
        }

        // Cerrar conexión con cliente
        close(client_sock);
        printf("\n[+] Conexión con cliente cerrada\n");
        printf(BOLD CYAN "\n[+] Listo para nueva conexión" RESET "\n");
    }

    // Limpiar y terminar servidor
    close(server_sock);
    printf(BOLD TEAL "[+] Servidor terminado" RESET "\n");
    return 0;
}
