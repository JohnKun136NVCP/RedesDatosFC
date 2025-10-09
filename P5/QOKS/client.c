/*
 * Práctica V. Restricción de servidor.
 *
 * Cliente que implementa un sistema de envío de archivos a servidores usando alias de red
 * con sockets TCP.
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

#define BUFFER_SIZE 4096  // Tamaño máximo del buffer para contenido de archivos
#define PORT 49200        // Puerto TCP al cual se conectan los clientes
#define MAX_ALIAS 4       // Número máximo de alias de red soportados

// Códigos ANSI para formato de texto en consola
#define BOLD "\033[1m"
#define RESET "\033[0m"
#define GREEN "\033[32m"
#define RED "\033[31m"
#define YELLOW "\033[33m"
#define BLUE "\033[34m"
#define CYAN "\033[36m"
#define MAGENTA "\033[35m"
#define TEAL "\033[96m"

// Función para conectar y enviar archivo a un servidor específico
int connect_and_send(const char* target_alias, const char* file_content, size_t content_length) {
    // Resolución de alias a dirección IP
    struct hostent *host_entry = gethostbyname(target_alias);  // Entrada del host desde /etc/hosts o DNS
    if (!host_entry) {
        printf(RED "│   Error: No se pudo resolver alias %s" RESET "\n", target_alias);
        printf(RED "└─ CONEXIÓN FALLIDA\n" RESET);
        return 0;
    }
    
    // Extraer IP resuelta del alias
    char *server_ip = inet_ntoa(*((struct in_addr*)host_entry->h_addr_list[0]));  // IP en formato string
    printf(BLUE "│   Destino: %s (%s:%d)" RESET "\n", target_alias, server_ip, PORT);
    
    // Creación de socket TCP
    int sock = socket(AF_INET, SOCK_STREAM, 0);  // Descriptor del socket cliente
    if (sock < 0) {
        printf(RED "│   Error: No se pudo crear socket" RESET "\n");
        printf(RED "└─ CONEXIÓN FALLIDA\n" RESET);
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
        printf(RED "│   Error: No se pudo establecer conexión TCP" RESET "\n");
        printf(RED "└─ CONEXIÓN FALLIDA\n" RESET);
        close(sock);
        return 0;
    }
    
    printf(GREEN "│   Conexión TCP establecida" RESET "\n");
    
    // Transmisión de archivo al servidor
    if (send(sock, file_content, content_length, 0) < 0) {
        printf(RED "│   Error: Fallo en transmisión de datos" RESET "\n");
        printf(RED "└─ ENVÍO FALLIDO\n" RESET);
        close(sock);
        return 0;
    }
    
    printf(CYAN "│   Archivo transmitido (%zu bytes)" RESET "\n", content_length);
    
    // Recepción de respuesta del servidor
    char response[BUFFER_SIZE];  // Buffer para la respuesta del servidor
    memset(response, 0, BUFFER_SIZE);
    int bytes_received = recv(sock, response, BUFFER_SIZE - 1, 0);
    
    if (bytes_received > 0) {
        response[bytes_received] = '\0';
        printf(YELLOW "│   Respuesta: %s" RESET "\n", response);
        
        // Verificar si el archivo fue realmente aceptado
        if (strcmp(response, "ARCHIVO_RECIBIDO") == 0) {
            close(sock);
            printf(BLUE "│   Conexión cerrada" RESET "\n");
            printf(BOLD GREEN "└─ ARCHIVO ACEPTADO\n" RESET);
            return 1;  // Archivo aceptado
        } else {
            close(sock);
            printf(BLUE "│   Conexión cerrada" RESET "\n");
            printf(BOLD YELLOW "└─ SERVIDOR OCUPADO\n" RESET);
            return 0;  // Servidor ocupado
        }
    }
    
    close(sock);
    printf(BLUE "│   Conexión cerrada" RESET "\n");
    printf(RED "└─ SIN RESPUESTA\n" RESET);
    return 0;  // Sin respuesta del servidor
}

int main(int argc, char *argv[]) {
    // Argumentos de línea de comandos: ./client <ALIAS_ORIGEN> <ARCHIVO>
    if (argc != 3) {
        printf("Uso: %s <ALIAS_ORIGEN> <ARCHIVO>\n", argv[0]);
        printf("Ejemplo: %s s01 mensaje.txt\n", argv[0]);
        printf("El alias s01 enviará el archivo a s02, s03 y s04\n");
        printf("NOTA: Solo el servidor con turno activo aceptará el archivo\n");
        printf("      Los demás responderán 'SERVIDOR_OCUPADO'\n");
        return 1;
    }
    
    // Parámetros del Cliente
    char *source_alias = argv[1];   // Alias de origen desde línea de comandos
    char *filename = argv[2];       // Archivo a transmitir desde línea de comandos
    
    // Lista de todos los alias disponibles
    char all_aliases[MAX_ALIAS][8] = {"s01", "s02", "s03", "s04"};  // Array con todos los alias válidos
    
    // I: Lectura de archivo local
    FILE *file = fopen(filename, "r");  // Puntero al archivo a enviar
    if (!file) {
        perror("Error abriendo archivo");
        return 1;
    }
    
    // Buffer para contenido del archivo
    char file_content[BUFFER_SIZE];  // Buffer que contiene todo el contenido del archivo
    memset(file_content, 0, BUFFER_SIZE);
    size_t content_length = fread(file_content, 1, BUFFER_SIZE - 1, file); 
    file_content[content_length] = '\0';
    fclose(file);
    
    printf(BOLD TEAL "\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n" RESET);
    printf(BOLD CYAN "[+] Cliente de Envío de Archivos" RESET "\n");
    printf(BOLD MAGENTA "[+] Archivo: " RESET "%s (%zu bytes)\n", filename, content_length);
    printf(BOLD YELLOW "[+] Alias origen: %s" RESET "\n", source_alias);
    printf(BOLD TEAL "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n" RESET);
    
    // II: Determinar alias destino y enviar (con sistema de turnos)
    printf(BOLD GREEN "\n[+] Iniciando conexiones a servidores:" RESET "\n");
    printf(YELLOW "    (Solo el servidor con turno activo aceptará)" RESET "\n");
    
    int successful_connections = 0;  // Contador de conexiones exitosas (ARCHIVO_RECIBIDO)
    
    // III: Conectar a cada alias excepto el origen (solo uno debería aceptar)
    for (int i = 0; i < MAX_ALIAS; i++) {
        // Saltar el alias de origen
        if (strcmp(all_aliases[i], source_alias) == 0) {
            printf(CYAN "\n[*] Omitiendo %s (es el alias origen)" RESET "\n", all_aliases[i]);
            continue;
        }
        
        printf(BOLD GREEN "\n┌─ Conectando a %s" RESET "\n", all_aliases[i]);
        
        if (connect_and_send(all_aliases[i], file_content, content_length)) {
            successful_connections++;  // Incrementar solo si el servidor aceptó el archivo
        }
    }
    
    // IV: Resumen de conexiones (en sistema de turnos, solo 1 debería ser exitosa)
    printf(BOLD CYAN "\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n" RESET);
    printf(BOLD GREEN "[+] Resumen Final:" RESET "\n");
    printf(BOLD "    Conexiones exitosas: %d/%d" RESET "\n", successful_connections, MAX_ALIAS - 1);
    
    if (successful_connections == 1) {
        printf(BOLD GREEN "    Estado: ✓ Sistema de turnos funcionando correctamente" RESET "\n");
    } else if (successful_connections == 0) {
        printf(BOLD YELLOW "    Estado: Ningún servidor aceptó - verificar turnos" RESET "\n");
    } else {
        printf(BOLD RED "    Estado: ✗ Múltiples servidores aceptaron - posible problema" RESET "\n");
    }
    
    printf(BOLD CYAN "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n" RESET);
    printf(BOLD CYAN "[+] Cliente terminado\n\n" RESET);
    
    return 0;
}