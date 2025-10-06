/*
 * Práctica IV. Servidor con alias.
 *
 * Cliente que implementa un sistema de envío de archivos a servidores usando alias de red
 * con sockets TCP. 
 *
 * Compilación:
 *   gcc client.c -o client
 *
 * Uso:
 *   ./client <SERVIDOR_ALIAS> <ARCHIVO>
 *
 * Ejemplos:
 *   ./client s01 archivo1.txt
 *   ./client s02 archivo2.txt
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
#include <netdb.h>

#define BUFFER_SIZE 4096
#define PORT 49200

// Códigos ANSI para formato de texto en consola
#define BOLD "\033[1m"
#define RESET "\033[0m"
#define GREEN "\033[32m"
#define RED "\033[31m"
#define YELLOW "\033[33m"
#define BLUE "\033[34m"
#define CYAN "\033[36m"

int main(int argc, char *argv[]) {
    // Argumentos de línea de comandos: ./client <SERVIDOR_ALIAS> <ARCHIVO>
    if (argc != 3) {
        printf("Uso: %s <SERVIDOR_ALIAS> <ARCHIVO>\n", argv[0]);
        printf("Ejemplo: %s s01 mensaje.txt\n", argv[0]);
        return 1;
    }
    
    // Parámetros del Cliente
    char *server_alias = argv[1];   // Alias del servidor
    char *filename = argv[2];       // Archivo a transmitir
    
    // I: Lectura de archivo local
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error abriendo archivo");
        return 1;
    }
    
    // Buffer para contenido del archivo
    char file_content[BUFFER_SIZE];
    memset(file_content, 0, BUFFER_SIZE);
    size_t content_length = fread(file_content, 1, BUFFER_SIZE - 1, file);
    file_content[content_length] = '\0';
    fclose(file);
    
    printf(BOLD CYAN "\n[+] Archivo: " RESET "%s (%zu bytes)\n", filename, content_length);
    printf(YELLOW "[+] Servidor: %s" RESET "\n", server_alias);
    
    // II: Resolución de alias a dirección IP
    struct hostent *host_entry = gethostbyname(server_alias);
    if (!host_entry) {
        printf(RED "[-] No se pudo resolver alias: %s" RESET "\n\n", server_alias);
        return 1;
    }
    
    // Extraer IP resuelta del alias
    char *server_ip = inet_ntoa(*((struct in_addr*)host_entry->h_addr_list[0]));
    printf(BLUE "[+] IP: %s:%d" RESET "\n", server_ip, PORT);
    
    // III: Creación de socket TCP
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Error creando socket");
        return 1;
    }
    
    // IV: Configuración de dirección del servidor
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));    // Limpiar estructura
    server_addr.sin_family = AF_INET;                // Protocolo IPv4
    server_addr.sin_port = htons(PORT);              // Puerto en formato de red
    inet_aton(server_ip, &server_addr.sin_addr);     // IP resuelta del alias
    
    // V: Establecimiento de conexión TCP
    printf(BOLD YELLOW "\n[+] Conectando al servidor" RESET "\n");
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        printf(RED "[-] No se pudo conectar al servidor %s" RESET "\n", server_alias);
        close(sock);
        return 1;
    }
    
    printf(GREEN "[+]   Conexión establecida" RESET "\n");
    
    // VI: Transmisión de archivo al servidor
    if (send(sock, file_content, content_length, 0) < 0) {
        perror("Error enviando datos");
        close(sock);
        return 1;
    }
    
    printf(CYAN "[+]   Archivo enviado" RESET "\n");
    
    // VII: Recepción de respuesta del servidor
    char response[BUFFER_SIZE];
    memset(response, 0, BUFFER_SIZE);
    int bytes_received = recv(sock, response, BUFFER_SIZE - 1, 0);
    
    // VIII: Procesamiento de respuesta del servidor
    if (bytes_received > 0) {
        response[bytes_received] = '\0';
        printf(BOLD GREEN "[+] Respuesta del servidor: %s" RESET "\n", response);
    } else {
        printf(RED "[-] Error: No se recibió respuesta del servidor" RESET "\n");
    }
    
    // IX: Finalización de conexión y limpieza de recursos
    close(sock);
    printf(BLUE "[+]   Conexión cerrada" RESET "\n");
    printf(BOLD GREEN "\n[+] Archivo enviado a %s\n\n" RESET, server_alias);
    
    return 0;
}