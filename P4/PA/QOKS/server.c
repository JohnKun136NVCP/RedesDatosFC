/*
 * Práctica IV. Servidor con alias.
 *
 * Servidor basado en serverOpt.c que escucha en múltiples alias de red.
 *
 * Compilación:
 *   gcc server.c -o server
 *
 * Uso:
 *   ./server <ALIAS1> <ALIAS2>
 *
 * Ejemplos:
 *   ./server s01 s02
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
#include <sys/select.h>
#include <time.h>

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

// Función para registrar el estado del servidor por alias
// Muestra el estado en terminal con colores ANSI y lo guarda en archivo log
void log_status(const char* alias, const char* status) {
    char filename[64];
    snprintf(filename, sizeof(filename), "%s_status.log", alias);
    
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    
    // Mostrar estado en terminal
    char color[16] = "";
    if (strcmp(status, "ESPERANDO") == 0) strcpy(color, GREEN);
    else if (strcmp(status, "RECIBIENDO") == 0) strcpy(color, YELLOW);
    else if (strcmp(status, "TRANSMITIENDO") == 0) strcpy(color, CYAN);
    
    printf("%s[%s] %02d/%02d/%04d %02d:%02d:%02d - %s%s\n",
           color, alias,
           tm_info->tm_mday, tm_info->tm_mon + 1, tm_info->tm_year + 1900,
           tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec,
           status, RESET);
    
    // Guardar en archivo log
    FILE *log = fopen(filename, "a");
    if (log) {
        fprintf(log, "%02d/%02d/%04d %02d:%02d:%02d %s\n",
                tm_info->tm_mday, tm_info->tm_mon + 1, tm_info->tm_year + 1900,
                tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec,
                status);
        fclose(log);
    }
}

// Función para resolver alias de red a dirección IP
int resolve_alias(const char* alias, char* ip) {
    struct hostent *host_entry = gethostbyname(alias);
    if (host_entry) {
        strcpy(ip, inet_ntoa(*((struct in_addr*)host_entry->h_addr_list[0])));
        return 1;
    }
    return 0;
}

// Función para manejar conexiones de clientes en un alias específico
void handle_client_connection(int server_sock, const char* alias) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    printf(BOLD "\n[+] Conexión en %s" RESET "\n", alias);
    log_status(alias, "RECIBIENDO");
    
    // Aceptar conexión de cliente
    int client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len);
    if (client_sock >= 0) {
        printf(CYAN "[%s]   Cliente conectado desde %s" RESET "\n", 
               alias, inet_ntoa(client_addr.sin_addr));
        
        // Recepción de datos del cliente
        char buffer[BUFFER_SIZE];
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(client_sock, buffer, BUFFER_SIZE - 1, 0);
        
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';
            
            // Crear directorio para almacenar archivos del alias
            char mkdir_cmd[64];
            snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p %s", alias);
            system(mkdir_cmd);
            
            log_status(alias, "TRANSMITIENDO");
            
            // VIII: Guardar archivo con timestamp único
            char filename[256];
            time_t now = time(NULL);
            snprintf(filename, sizeof(filename), "%s/archivo_%ld.txt", alias, now);
            
            FILE *file = fopen(filename, "w");
            if (file) {
                fprintf(file, "%s", buffer);
                fclose(file);
                printf(YELLOW "[%s]   Archivo guardado: %s" RESET "\n", alias, filename);
            }
            
            // Enviar confirmación al cliente
            send(client_sock, "ARCHIVO_RECIBIDO", 16, 0);
        }
        
        close(client_sock);
        log_status(alias, "ESPERANDO");
        printf(BLUE "[%s]   Conexión cerrada" RESET "\n", alias);
    }
}

int main(int argc, char *argv[]) {
    // Argumentos de línea de comandos: ./server <ALIAS1> <ALIAS2>
    if (argc != 3) {
        printf("Uso: %s <ALIAS1> <ALIAS2>\n", argv[0]);
        printf("Ejemplo: %s s01 s02\n", argv[0]);
        return 1;
    }
    
    // Parámetros del Servidor Multi-Alias
    char *alias1 = argv[1];   // Primer alias de red
    char *alias2 = argv[2];   // Segundo alias de red
    char ip1[16], ip2[16];    // Direcciones IP resueltas
    printf(BOLD CYAN "\n[+] Iniciando Servidor" RESET "\n");
    
    // I: Resolución de alias a direcciones IP
    if (!resolve_alias(alias1, ip1)) {
        printf(RED "[-] No se pudo resolver alias: %s" RESET "\n", alias1);
        return 1;
    }
    if (!resolve_alias(alias2, ip2)) {
        printf(RED "[-] No se pudo resolver alias: %s" RESET "\n", alias2);
        return 1;
    }
    
    // II: Creación de sockets TCP para cada alias
    int sock1 = socket(AF_INET, SOCK_STREAM, 0);
    int sock2 = socket(AF_INET, SOCK_STREAM, 0);
    
    if (sock1 < 0 || sock2 < 0) {
        perror("Error creando sockets");
        return 1;
    }
    
    // III: Configuración para reutilizar direcciones
    int opt = 1;
    setsockopt(sock1, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(sock2, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // IV: Configuración de direcciones de red para cada socket
    struct sockaddr_in addr1, addr2;
    
    // Configurar dirección para alias1
    memset(&addr1, 0, sizeof(addr1));           // Limpiar estructura
    addr1.sin_family = AF_INET;                 // Protocolo IPv4
    addr1.sin_port = htons(PORT);               // Puerto en formato de red
    inet_aton(ip1, &addr1.sin_addr);            // IP resuelta del alias1
    
    // Configurar dirección para alias2  
    memset(&addr2, 0, sizeof(addr2));           // Limpiar estructura
    addr2.sin_family = AF_INET;                 // Protocolo IPv4
    addr2.sin_port = htons(PORT);               // Puerto en formato de red
    inet_aton(ip2, &addr2.sin_addr);            // IP resuelta del alias2
    
    // V: Enlace de sockets a sus respectivas direcciones
    if (bind(sock1, (struct sockaddr*)&addr1, sizeof(addr1)) < 0) {
        printf(RED "Error en bind para %s (%s)" RESET "\n", alias1, ip1);
        return 1;
    }
    if (bind(sock2, (struct sockaddr*)&addr2, sizeof(addr2)) < 0) {
        printf(RED "Error en bind para %s (%s)" RESET "\n", alias2, ip2);
        return 1;
    }
    
    // VI: Configuración de sockets en modo escucha
    listen(sock1, 5);
    listen(sock2, 5);
    
    printf(GREEN "[+]   Escuchando en %s (%s):%d" RESET "\n", alias1, ip1, PORT);
    printf(GREEN "[+]   Escuchando en %s (%s):%d" RESET "\n", alias2, ip2, PORT);
    
    // VII: Inicialización de estados de logging
    log_status(alias1, "ESPERANDO");
    log_status(alias2, "ESPERANDO");
    
    // VIII: Preparación de select()
    fd_set read_fds, temp_fds;                     // Sets de descriptores para select()
    FD_ZERO(&read_fds);                            // Limpiar set principal
    FD_SET(sock1, &read_fds);                      // Agregar socket1 al monitoring
    FD_SET(sock2, &read_fds);                      // Agregar socket2 al monitoring
    int max_fd = (sock1 > sock2) ? sock1 : sock2;  // Descriptor máximo para select()
    
    printf(BOLD "\n[+] Servidor listo" RESET "\n\n");
    
    // IX: Bucle principal del servidor
    while (1) {
        temp_fds = read_fds;
        
        // X: Esperar actividad en cualquier socket
        int activity = select(max_fd + 1, &temp_fds, NULL, NULL, NULL);
        if (activity < 0) {
            perror("Error en select");
            break;
        }
        
        // XI: Verificar actividad en socket1 (alias1)
        if (FD_ISSET(sock1, &temp_fds)) {
            handle_client_connection(sock1, alias1);
        }
        
        // XII: Verificar actividad en socket2 (alias2)
        if (FD_ISSET(sock2, &temp_fds)) {
            handle_client_connection(sock2, alias2);
        }
    }
    
    // Limpiar recursos y terminar servidor
    close(sock1);
    close(sock2);
    printf(BOLD CYAN "[+] Servidor terminado" RESET "\n");
    return 0;
}