/*
 * Práctica V. Restricción de servidor.
 *
 * Servidor que escucha en 4 alias de red (s01, s02, s03, s04) con sistema
 * de turnos round robin para recepción de archivos. 
 * Solo un servidor puede recibir a la vez.
 *
 * Compilación:
 *   gcc server.c -o server -lpthread
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
#include <pthread.h>

#define BUFFER_SIZE 4096  // Tamaño máximo del buffer para recepción de archivos
#define PORT 49200        // Puerto TCP en el que escuchan todos los servidores
#define MAX_ALIAS 4       // Número máximo de alias de red soportados
#define TURN_DURATION 10  // Duración del turno en segundos para el sistema round robin

// Variables para control de turnos
int current_turn = 0;       // Índice del servidor que tiene el turno actual
int receiving_server = -1;  // Índice del servidor que está recibiendo

pthread_mutex_t turn_mutex = PTHREAD_MUTEX_INITIALIZER;  // Mutex para exclusión mutua
pthread_cond_t turn_cond = PTHREAD_COND_INITIALIZER;     // Variable para sincronización de hilos

// Estructura para datos del servidor
typedef struct {
    int server_index;         // Índice único del servidor (0-3)
    int socket;               // Descriptor del socket TCP del servidor
    char *alias;              // Nombre del alias de red (s0x)
    char ip[16];              // Dirección IP resuelta del alias
    struct sockaddr_in addr;  // Estructura de dirección de socket para bind/listen
} server_data_t;

server_data_t servers[MAX_ALIAS];  // Array de estructuras para los 4 servidores

// Función para resolver alias de red a dirección IP
int resolve_alias(const char* alias, char* ip) {
    struct hostent *host_entry = gethostbyname(alias);
    if (host_entry) {
        strcpy(ip, inet_ntoa(*((struct in_addr*)host_entry->h_addr_list[0])));
        return 1;
    }
    return 0;
}

// Códigos ANSI para formato de texto en consola
#define BOLD "\033[1m"
#define RESET "\033[0m"
#define GREEN "\033[32m"
#define RED "\033[31m"
#define YELLOW "\033[33m"
#define BLUE "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN "\033[36m"
#define TEAL "\033[96m"

// Función para notificar a otros servidores sobre el estado actual
void notify_other_servers(int sender_index, const char* message) {
    if (strcmp(message, "RECIBIENDO") == 0) {
        printf(BLUE "│        Notificando estado OCUPADO a otros servidores" RESET "\n");
    } else if (strcmp(message, "LIBRE") == 0) {
        printf(BLUE "│        Notificando estado LIBRE a otros servidores" RESET "\n");
    }
}

// Función para verificar si es el turno del servidor
int is_my_turn(int server_index) {
    pthread_mutex_lock(&turn_mutex);
    int result = (current_turn == server_index && receiving_server == -1);
    pthread_mutex_unlock(&turn_mutex);
    return result;
}

// Función para iniciar recepción
int start_receiving(int server_index) {
    pthread_mutex_lock(&turn_mutex);
    if (current_turn == server_index && receiving_server == -1) {
        receiving_server = server_index;  // Marcar este servidor como el que está recibiendo
        notify_other_servers(server_index, "RECIBIENDO");
        pthread_mutex_unlock(&turn_mutex);
        return 1; // Puede recibir
    }
    pthread_mutex_unlock(&turn_mutex);
    return 0; // No es su turno o alguien más está recibiendo
}

// Función para finalizar recepción
void finish_receiving(int server_index) {
    pthread_mutex_lock(&turn_mutex);
    if (receiving_server == server_index) {
        receiving_server = -1;  // Ningún servidor está recibiendo
        notify_other_servers(server_index, "LIBRE");
        pthread_cond_broadcast(&turn_cond);  // Despertar hilos esperando
    }
    pthread_mutex_unlock(&turn_mutex);
}

// Función para registrar el estado del servidor por alias
// Muestra el estado en terminal con colores ANSI y lo guarda en archivo log
void log_status(const char* alias, const char* status) {
    char filename[64];  // Buffer para el nombre del archivo de log
    snprintf(filename, sizeof(filename), "%s_status.log", alias);
    
    time_t now = time(NULL);  // Timestamp actual
    struct tm *tm_info = localtime(&now);
    
    // Mostrar estado en terminal con formato completo
    char color[16] = "";  // Buffer para código de color ANSI
    if (strcmp(status, "ESPERANDO") == 0) strcpy(color, GREEN);
    else if (strcmp(status, "RECIBIENDO") == 0) strcpy(color, YELLOW);
    else if (strcmp(status, "TRANSMITIENDO") == 0) strcpy(color, CYAN);
    
    printf("%s│  [%s] %02d/%02d/%04d %02d:%02d:%02d - %s%s\n",
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

// Función para manejar conexiones de clientes en un alias específico
void handle_client_connection(int server_sock, int server_index) {
    const char* alias = servers[server_index].alias;  // Alias del servidor actual
    struct sockaddr_in client_addr;                   // Dirección del cliente que se conecta
    socklen_t client_len = sizeof(client_addr);       // Tamaño de la estructura de dirección
    
    // Verificar si es su turno antes de mostrar conexión
    if (!is_my_turn(server_index)) {
        // Aceptar la conexión pero rechazar silenciosamente
        int client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len);
        if (client_sock >= 0) {
            send(client_sock, "SERVIDOR_OCUPADO", 16, 0);
            close(client_sock);
        }
        printf(YELLOW "    [%s] Conexión rechazada (no es su turno)" RESET "\n", alias);
        return;
    }
    
    // Mostrar conexión aceptada
    printf(BOLD GREEN "\n┌─ [%s] CONEXIÓN ACEPTADA" RESET "\n", alias);
    
    // Intentar iniciar recepción
    if (!start_receiving(server_index)) {
        printf(YELLOW "└─ [%s] Error: No se pudo iniciar recepción" RESET "\n", alias);
        
        int client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len);
        if (client_sock >= 0) {
            send(client_sock, "SERVIDOR_OCUPADO", 16, 0);
            close(client_sock);
        }
        return;
    }
    
    log_status(alias, "RECIBIENDO");
    
    // Aceptar conexión de cliente
    int client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len);
    if (client_sock >= 0) {
        printf(CYAN "│        Cliente: %s" RESET "\n", inet_ntoa(client_addr.sin_addr));
        
        // Recepción de datos del cliente
        char buffer[BUFFER_SIZE];  // Buffer para recibir datos del archivo
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(client_sock, buffer, BUFFER_SIZE - 1, 0);
        
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';
            
            // Crear directorio para almacenar archivos del alias
            char mkdir_cmd[64];
            snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p %s", alias);
            system(mkdir_cmd);
            
            log_status(alias, "TRANSMITIENDO");
            
            // Guardar archivo con timestamp
            char filename[256];       // Ruta completa del archivo
            time_t now = time(NULL);  // Timestamp
            snprintf(filename, sizeof(filename), "%s/archivo_%ld.txt", alias, now);
            
            FILE *file = fopen(filename, "w");
            if (file) {
                fprintf(file, "%s", buffer);
                fclose(file);
                printf(YELLOW "│        Archivo: %s (%d bytes)" RESET "\n", filename, bytes_received);
            }
            
            // Enviar confirmación al cliente
            send(client_sock, "ARCHIVO_RECIBIDO", 16, 0);
        }
        
        close(client_sock);
        printf(BLUE "│        Conexión cerrada" RESET "\n");
    }
    
    // Finalizar recepción
    finish_receiving(server_index);
    log_status(alias, "ESPERANDO");
    printf(BOLD GREEN "└─ [%s] TRANSFERENCIA COMPLETADA" RESET "\n\n", alias);
}

// Hilo para controlar los turnos round robin
void* turn_controller(void* arg) {
    while (1) {
        sleep(TURN_DURATION);  // Esperar duración del turno (10 segundos)
        
        pthread_mutex_lock(&turn_mutex);
        
        // Solo cambiar turno si nadie está recibiendo
        if (receiving_server == -1) {
            int old_turn = current_turn;                    // Guardar turno anterior
            current_turn = (current_turn + 1) % MAX_ALIAS;  // Rotar al siguiente servidor
            
            printf(MAGENTA "\n[TURNO] %s -> %s" RESET "\n", 
                   servers[old_turn].alias, servers[current_turn].alias);
            
            // Despertar hilos que esperan su turno
            pthread_cond_broadcast(&turn_cond);
        } else {
            printf(YELLOW "[TURNO] %s sigue recibiendo, esperando..." RESET "\n", 
                   servers[receiving_server].alias);
        }
        
        pthread_mutex_unlock(&turn_mutex);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    // Argumentos de línea de comandos: ./server <ALIAS1> <ALIAS2> <ALIAS3> <ALIAS4>
    if (argc != 5) {
        printf("Uso: %s <ALIAS1> <ALIAS2> <ALIAS3> <ALIAS4>\n", argv[0]);
        printf("Ejemplo: %s s01 s02 s03 s04\n", argv[0]);
        printf("Compilación: gcc server.c -o server -lpthread\n");
        return 1;
    }
    printf(BOLD TEAL "\n━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n" RESET);
    printf(BOLD CYAN "[+] Iniciando Servidor con Restricciones de Monitoreo" RESET "\n");
    
    // I: Inicializar estructuras de servidores
    printf(BLUE "\n[+] Configurando servidores: " RESET);
    for (int i = 0; i < MAX_ALIAS; i++) {
        servers[i].server_index = i;
        servers[i].alias = argv[i + 1];
        printf(CYAN "%s" RESET, servers[i].alias);
        if (i < MAX_ALIAS - 1) printf(", ");
    }
    printf("\n");
    
    // II: Resolución de alias a direcciones IP
    printf(GREEN "\n[+] Resolución de alias:" RESET "\n");
    for (int i = 0; i < MAX_ALIAS; i++) {
        if (!resolve_alias(servers[i].alias, servers[i].ip)) {
            printf(RED "[-] No se pudo resolver alias: %s" RESET "\n", servers[i].alias);
            return 1;
        }
        printf(YELLOW "    %s -> %s" RESET "\n", servers[i].alias, servers[i].ip);
    }
    
    // III: Creación de sockets TCP para cada alias
    printf(CYAN "\n[+] Creando sockets TCP..." RESET "\n");
    for (int i = 0; i < MAX_ALIAS; i++) {
        servers[i].socket = socket(AF_INET, SOCK_STREAM, 0);
        if (servers[i].socket < 0) {
            perror("Error creando socket");
            return 1;
        }
        
        // Configuración para reutilizar direcciones
        int opt = 1;
        setsockopt(servers[i].socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    }
    printf(GREEN "    ✓ %d sockets creados" RESET "\n", MAX_ALIAS);
    
    // IV: Configuración de direcciones de red para cada socket
    for (int i = 0; i < MAX_ALIAS; i++) {
        memset(&servers[i].addr, 0, sizeof(servers[i].addr));
        servers[i].addr.sin_family = AF_INET;
        servers[i].addr.sin_port = htons(PORT);
        inet_aton(servers[i].ip, &servers[i].addr.sin_addr);
    }
    
    // V: Enlace de sockets a sus respectivas direcciones
    printf(CYAN "\n[+] Enlazando sockets..." RESET "\n");
    for (int i = 0; i < MAX_ALIAS; i++) {
        if (bind(servers[i].socket, (struct sockaddr*)&servers[i].addr, sizeof(servers[i].addr)) < 0) {
            printf(RED "Error en bind para %s (%s)" RESET "\n", servers[i].alias, servers[i].ip);
            perror("Bind error details");
            return 1;
        }
    }
    printf(GREEN "    ✓ %d sockets enlazados" RESET "\n", MAX_ALIAS);
    
    // VI: Configuración de sockets en modo escucha
    printf(CYAN "\n[+] Configurando modo escucha en puerto %d:" RESET "\n", PORT);
    for (int i = 0; i < MAX_ALIAS; i++) {
        listen(servers[i].socket, 5);
        printf(GREEN "    ✓ %s (%s)" RESET "\n", servers[i].alias, servers[i].ip);
    }
    
    // VII: Inicialización de estados de logging
    printf(CYAN "\n[+] Estados iniciales:" RESET "\n");
    for (int i = 0; i < MAX_ALIAS; i++) {
        log_status(servers[i].alias, "ESPERANDO");
    }
    
    printf(BOLD MAGENTA "\n[+] Sistema de turnos: %s inicia" RESET "\n", 
           servers[current_turn].alias);
    
    // VIII: Crear hilo para control de turnos
    pthread_t turn_thread;
    if (pthread_create(&turn_thread, NULL, turn_controller, NULL) != 0) {
        perror("Error creando hilo de turnos");
        return 1;
    }
    printf(CYAN "[+] Hilo de control iniciado" RESET "\n", TURN_DURATION);
    
    // IX: Bucle principal con select()
    fd_set read_fds, temp_fds;  // Conjuntos de descriptores para select()
    FD_ZERO(&read_fds);
    int max_fd = 0;             // Descriptor de archivo más alto para select()
    
    for (int i = 0; i < MAX_ALIAS; i++) {
        FD_SET(servers[i].socket, &read_fds);
        if (servers[i].socket > max_fd) {
            max_fd = servers[i].socket;
        }
    }
    
    printf(BOLD GREEN "\n[✓] Servidor listo - Esperando conexiones...\n" RESET);
    printf(YELLOW "    Puerto: %d | Aliases: %d | Turnos: %ds\n" RESET, 
           PORT, MAX_ALIAS, TURN_DURATION);
    printf(BOLD TEAL "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n" RESET);
    
    // X: Bucle principal de servidor
    while (1) {
        temp_fds = read_fds;
        
        int activity = select(max_fd + 1, &temp_fds, NULL, NULL, NULL);
        if (activity < 0) {
            perror("Error en select");
            break;
        }
        
        for (int i = 0; i < MAX_ALIAS; i++) {
            if (FD_ISSET(servers[i].socket, &temp_fds)) {
                handle_client_connection(servers[i].socket, i);
            }
        }
    }
    
    // Limpiar recursos
    for (int i = 0; i < MAX_ALIAS; i++) {
        close(servers[i].socket);
    }
    printf(BOLD CYAN "[+] Servidor terminado" RESET "\n");
    return 0;
}