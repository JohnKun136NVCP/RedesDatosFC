/*
 * Práctica III. Cliente - Servidor.
 *
 * Cliente distribuido que implementa un sistema de envío de archivos a múltiples servidores
 * usando sockets TCP. Cada servidor decide si procesa el archivo según el puerto solicitado.
 *
 * Compilación:
 *   gcc Client.c -o client
 *
 * Uso:
 *   ./Client <SERVIDOR_IP> <PUERTO> <DESPLAZAMIENTO> <Nombre_del_archivo>
 *
 * Ejemplo:
 *   ./Client 192.168.1.100 49201 10 mensaje.txt
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

#define BUFFER_SIZE 4096
#define NUM_SERVERS 3

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

// Puertos específicos de las 3 instancias del servidor Ubuntu
int server_ports[NUM_SERVERS] = {49200, 49201, 49202};

int main(int argc, char *argv[])
{
    // Argumentos de línea de comandos: ./client <IP> <PUERTO> <DESPLAZAMIENTO> <ARCHIVO>
    if (argc != 5)
    {
        printf("Uso: %s <SERVIDOR_IP> <PUERTO> <DESPLAZAMIENTO> <Nombre_del_archivo>\n", argv[0]);
        printf("Ejemplo: %s 192.168.1.100 49201 10 mensaje.txt\n", argv[0]);
        return 1;
    }

    // Parámetros Cliente
    char *server_ip = argv[1];       // IP servidor Ubuntu
    int target_port = atoi(argv[2]); // Puerto objetivo (49200, 49201 o 49202)
    int shift = atoi(argv[3]);       // Desplazamiento para cifrado César
    char *filename = argv[4];        // Archivo con texto histórico

    // I: Lectura de archivo
    FILE *file = fopen(filename, "r");
    if (!file)
    {
        perror("Error abriendo archivo");
        return 1;
    }

    char file_content[BUFFER_SIZE];
    memset(file_content, 0, BUFFER_SIZE);

    // Leer los caracteres alfabéticos del archivo
    size_t content_length = fread(file_content, 1, BUFFER_SIZE - 1, file);
    file_content[content_length] = '\0';
    fclose(file);

    printf(BOLD MAGENTA "\n[+] Archivo: " RESET "%s (%zu bytes)\n", filename, content_length);
    printf(BOLD CYAN "[+] Puerto: " RESET "%d\n", target_port);
    printf(BOLD YELLOW "[+] Desplazamiento César: " RESET "%d\n", shift);

    // II: Conexión a múltiples servidores (3 servidores Ubuntu)
    for (int i = 0; i < NUM_SERVERS; i++)
    {
        int sock;
        struct sockaddr_in server_addr;
        char response[BUFFER_SIZE];

        printf(BOLD "\n[+] Conectando al servidor %d en puerto %d" RESET "\n", i + 1, server_ports[i]);

        // Socket TCP (Protocolo de comunicación cliente-servidor)
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0)
        {
            perror("Error creando socket");
            continue;
        }

        // Configuración de dirección servidor
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(server_ports[i]);

        if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0)
        {
            printf(RED "[-] IP inválida: %s" RESET "\n", server_ip);
            close(sock);
            continue;
        }

        // Conexión TCP con servidor
        if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
        {
            printf(RED "[-] No se pudo conectar al puerto %d" RESET "\n", server_ports[i]);
            close(sock);
            continue;
        }

        printf(GREEN "[+]   Conexión establecida" RESET "\n");

        // III: Envío de datos al Servidor
        char header[64];
        snprintf(header, sizeof(header), "%d\n%d\n", target_port, shift);

        // Enviar header con puerto objetivo y desplazamiento César
        if (send(sock, header, strlen(header), 0) < 0)
        {
            perror("Error enviando header");
            close(sock);
            continue;
        }

        // Enviar contenido del archivo con acontecimiento histórico
        if (send(sock, file_content, strlen(file_content), 0) < 0)
        {
            perror("Error enviando contenido");
            close(sock);
            continue;
        }

        printf(GREEN "[+]   Datos Enviados\n" RESET);

        // IV: Respuesta del servidor
        memset(response, 0, BUFFER_SIZE);
        int bytes_received = recv(sock, response, BUFFER_SIZE - 1, 0);

        if (bytes_received > 0)
        {
            response[bytes_received] = '\0';
            
            printf(BLUE "[+] Respuesta del Servidor %d\n" RESET, i + 1);

            if (strncmp(response, "PROCESADO", 9) == 0)
            {
                printf(GREEN "[+]   Procesado (puerto coincide)" RESET "\n");
                printf(BOLD "[+]   Contenido cifrado con desplazamiento %d:\n\n%s\n\n" RESET, shift, response + 10);
            }
            else if (strncmp(response, "RECHAZADO", 9) == 0)
            {
                printf(RED "[-]   Rechazado (puerto no coincide)" RESET "\n");
            }
            else
            {
                printf("   Estado desconocido: %s\n", response);
            }
        }
        else
        {
            printf(RED "[-] Error: No se recibió respuesta del servidor puerto %d" RESET "\n", server_ports[i]);
        }

        // Cerrar conexión con este servidor
        close(sock);
        printf("[+] Conexión cerrada [puerto %d]\n", server_ports[i]);
    }
    
    printf(BOLD TEAL "\n    El servidor en el puerto %d procesó el archivo\n\n" RESET, target_port);

    return 0;
}
