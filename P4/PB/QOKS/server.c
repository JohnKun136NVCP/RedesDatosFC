/*
 * Práctica IV. Servidor con alias.
 *
 * Servidor que escucha en 4 alias de red (s01, s02, s03, s04) con comunicación
 * entre servidores para consulta de estados.
 *
 * Compilación:
 *   gcc server.c -o server
 *
 * Uso:
 *   ./server <ALIAS1> <ALIAS2> <ALIAS3> <ALIAS4>
 *
 * Ejemplo:
 *   ./server s01 s02 s03 s04
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
#define MAX_ALIAS 4

// Códigos ANSI para formato de texto en consola
#define BOLD "\033[1m"
#define RESET "\033[0m"
#define GREEN "\033[32m"
#define RED "\033[31m"
#define YELLOW "\033[33m"
#define BLUE "\033[34m"
#define MAGENTA "\033[35m"
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
            
            // Guardar archivo con timestamp único
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
    // Argumentos de línea de comandos: ./server <ALIAS1> <ALIAS2> <ALIAS3> <ALIAS4>
    if (argc != 5) {
        printf("Uso: %s <ALIAS1> <ALIAS2> <ALIAS3> <ALIAS4>\n", argv[0]);
        printf("Ejemplo: %s s01 s02 s03 s04\n", argv[0]);
        return 1;
    }
    
    // Parámetros del Servidor
    char *aliases[MAX_ALIAS];            // Array de nombres de alias
    char ips[MAX_ALIAS][16];             // Array de direcciones IP resueltas
    int sockets[MAX_ALIAS];              // Array de sockets TCP
    struct sockaddr_in addrs[MAX_ALIAS]; // Array de estructuras de dirección
    
    // Inicializar alias
    for (int i = 0; i < MAX_ALIAS; i++) {
        aliases[i] = argv[i + 1];
    }
    
    printf(BOLD CYAN "\n[+] Iniciando Servidor" RESET "\n");
    
    // I: Resolución de alias a direcciones IP
    for (int i = 0; i < MAX_ALIAS; i++) {
        if (!resolve_alias(aliases[i], ips[i])) {
            printf(RED "[-] No se pudo resolver alias: %s" RESET "\n", aliases[i]);
            return 1;
        }
    }
    
    // II: Creación de sockets TCP para cada alias
    for (int i = 0; i < MAX_ALIAS; i++) {
        sockets[i] = socket(AF_INET, SOCK_STREAM, 0);
        if (sockets[i] < 0) {
            perror("Error creando socket");
            return 1;
        }
        
        // Configuración para reutilizar direcciones
        int opt = 1;
        setsockopt(sockets[i], SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    }
    
    // III: Configuración de direcciones de red para cada socket
    for (int i = 0; i < MAX_ALIAS; i++) {
        memset(&addrs[i], 0, sizeof(addrs[i]));
        addrs[i].sin_family = AF_INET;
        addrs[i].sin_port = htons(PORT);
        inet_aton(ips[i], &addrs[i].sin_addr);
    }
    
    // IV: Enlace de sockets a sus respectivas direcciones
    for (int i = 0; i < MAX_ALIAS; i++) {
        if (bind(sockets[i], (struct sockaddr*)&addrs[i], sizeof(addrs[i])) < 0) {
            printf(RED "Error en bind para %s (%s)" RESET "\n", aliases[i], ips[i]);
            return 1;
        }
    }
    
    // V: Configuración de sockets en modo escucha
    for (int i = 0; i < MAX_ALIAS; i++) {
        listen(sockets[i], 5);
        printf(GREEN "[+]   Escuchando en %s (%s):%d" RESET "\n", aliases[i], ips[i], PORT);
    }
    
    // VI: Inicialización de estados de logging
    for (int i = 0; i < MAX_ALIAS; i++) {
        log_status(aliases[i], "ESPERANDO");
    }
    
    // VII: Preparación de select()
    fd_set read_fds, temp_fds;
    FD_ZERO(&read_fds);
    int max_fd = 0;
    
    for (int i = 0; i < MAX_ALIAS; i++) {
        FD_SET(sockets[i], &read_fds);
        if (sockets[i] > max_fd) {
            max_fd = sockets[i];
        }
    }
    
    printf(BOLD "\n[+] Servidor listo" RESET "\n\n");
    
    // VIII: Bucle principal del servidor
    while (1) {
        temp_fds = read_fds;
        
        // Esperar actividad en cualquier socket
        int activity = select(max_fd + 1, &temp_fds, NULL, NULL, NULL);
        if (activity < 0) {
            perror("Error en select");
            break;
        }
        
        // Verificar actividad en cada socket
        for (int i = 0; i < MAX_ALIAS; i++) {
            if (FD_ISSET(sockets[i], &temp_fds)) {
                handle_client_connection(sockets[i], aliases[i]);
            }
        }
    }
    
    // Limpiar recursos y terminar servidor
    for (int i = 0; i < MAX_ALIAS; i++) {
        close(sockets[i]);
    }
    printf(BOLD CYAN "[+] Servidor terminado" RESET "\n");
    return 0;
}