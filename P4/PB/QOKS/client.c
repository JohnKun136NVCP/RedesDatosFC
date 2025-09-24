/*
 * Práctica IV. Servidor con alias.
 *
 * Cliente que implementa un sistema de envío de archivos a servidores usando alias de red
 * con sockets TCP, cada alias se conecta a los otros 3 para consultar estados.
 *
 * Compilación:
 *   gcc client.c -o client
 *
 * Uso:
 *   ./client <ALIAS_ORIGEN> <ARCHIVO>
 *
 * Ejemplo:
 *   ./client s01 archivo1.txt
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
#define MAX_ALIAS 4

// Códigos ANSI para formato de texto en consola
#define BOLD "\033[1m"
#define RESET "\033[0m"
#define GREEN "\033[32m"
#define RED "\033[31m"
#define YELLOW "\033[33m"
#define BLUE "\033[34m"
#define CYAN "\033[36m"
#define MAGENTA "\033[35m"

// Función para conectar y enviar archivo a un servidor específico
int connect_and_send(const char* target_alias, const char* file_content, size_t content_length) {
    // Resolución de alias a dirección IP
    struct hostent *host_entry = gethostbyname(target_alias);
    if (!host_entry) {
        printf(RED "[-] No se pudo resolver alias: %s" RESET "\n", target_alias);
        return 0;
    }
    
    // Extraer IP resuelta del alias
    char *server_ip = inet_ntoa(*((struct in_addr*)host_entry->h_addr_list[0]));
    printf(BLUE "[+] Conectando a %s (%s:%d)" RESET "\n", target_alias, server_ip, PORT);
    
    // Creación de socket TCP
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Error creando socket");
        return 0;
    }
    
    // Configuración de dirección del servidor
    struct sockaddr_in server_addr;               // Estructura para la dirección del servidor destino
    memset(&server_addr, 0, sizeof(server_addr)); // Limpiar la estructura antes de configurar
    server_addr.sin_family = AF_INET;             // Usar protocolo IPv4
    server_addr.sin_port = htons(PORT);           // Puerto de destino en formato de red
    inet_aton(server_ip, &server_addr.sin_addr);  // Asignar la IP resuelta del alias destino
    
    // Establecimiento de conexión TCP
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        printf(RED "[-] No se pudo conectar al servidor %s" RESET "\n", target_alias);
        close(sock);
        return 0;
    }
    
    printf(GREEN "[+]   Conexión establecida" RESET "\n");
    
    // Transmisión de archivo al servidor
    if (send(sock, file_content, content_length, 0) < 0) {
        perror("Error enviando datos");
        close(sock);
        return 0;
    }
    
    printf(CYAN "[+]   Archivo enviado" RESET "\n");
    
    // Recepción de respuesta del servidor
    char response[BUFFER_SIZE];
    memset(response, 0, BUFFER_SIZE);
    int bytes_received = recv(sock, response, BUFFER_SIZE - 1, 0);
    
    if (bytes_received > 0) {
        response[bytes_received] = '\0';
        printf(BOLD GREEN "[+] Respuesta: %s" RESET "\n", response);
    }
    
    close(sock);
    printf(BLUE "[+]   Conexión cerrada" RESET "\n");
    return 1;
}

int main(int argc, char *argv[]) {
    // Argumentos de línea de comandos: ./client <ALIAS_ORIGEN> <ARCHIVO>
    if (argc != 3) {
        printf("Uso: %s <ALIAS_ORIGEN> <ARCHIVO>\n", argv[0]);
        printf("Ejemplo: %s s01 mensaje.txt\n", argv[0]);
        printf("El alias s01 enviará el archivo a s02, s03 y s04\n");
        return 1;
    }
    
    // Parámetros del Cliente
    char *source_alias = argv[1];   // Alias de origen
    char *filename = argv[2];       // Archivo a transmitir
    
    // Lista de todos los alias disponibles
    char all_aliases[MAX_ALIAS][8] = {"s01", "s02", "s03", "s04"};
    
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
    
    printf(BOLD CYAN "\n[+] Cliente" RESET "\n");
    printf(BOLD MAGENTA "[+] Archivo: " RESET "%s (%zu bytes)\n", filename, content_length);
    printf(BOLD YELLOW "[+] Alias origen: %s" RESET "\n", source_alias);
    
    // II: Determinar alias destino
    printf(BOLD "\n[+] Conectando desde %s a los demás servidores:" RESET "\n", source_alias);
    
    int successful_connections = 0;
    
    // III: Conectar a cada alias excepto el origen
    for (int i = 0; i < MAX_ALIAS; i++) {
        // Saltar el alias de origen
        if (strcmp(all_aliases[i], source_alias) == 0) {
            printf(YELLOW "[*] Saltando %s" RESET "\n", all_aliases[i]);
            continue;
        }
        
        printf(BOLD "\n[+] Enviando a %s" RESET "\n", all_aliases[i]);
        
        if (connect_and_send(all_aliases[i], file_content, content_length)) {
            successful_connections++;
        } else {
            printf(RED "[-] Falló conexión con %s" RESET "\n", all_aliases[i]);
        }
    }
    
    // IV: Resumen de conexiones
    printf(BOLD GREEN "\n[+] Resumen: %d/%d conexiones exitosas" RESET "\n", 
           successful_connections, MAX_ALIAS - 1);
    printf(BOLD CYAN "[+] Cliente terminado\n\n" RESET);
    
    return 0;
}