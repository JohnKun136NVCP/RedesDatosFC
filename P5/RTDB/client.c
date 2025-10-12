#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BUFFER_SIZE 1024
#define BASE_PORT 49200
#define NUM_SERVERS 4

// Puertos de cada servidor
const int server_ports[] = {49200, 49201, 49202, 49203};
const char *server_names[] = {"s01", "s02", "s03", "s04"};

void logStatus(const char *server_ip, int port, const char *status, const char *filename) {
    FILE *log = fopen("client_log.txt", "a");
    if (log) {
        time_t now = time(NULL);
        char *ts = ctime(&now);
        ts[strcspn(ts, "\n")] = 0;
        if (filename)
            fprintf(log, "[%s] Servidor %s:%d Archivo: %s Estado: %s\n", ts, server_ip, port, filename, status);
        else
            fprintf(log, "[%s] Servidor %s:%d Estado: %s\n", ts, server_ip, port, status);
        fclose(log);
    }
}

int try_connect_to_server(const char *server_ip, int port, const char *filename) {
    int sock;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE];

    printf("Intentando conectar a %s:%d...\n", server_ip, port);
    
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { 
        perror("socket"); 
        return -1; 
    }

    // Configurar timeout de conexión
    struct timeval timeout;
    timeout.tv_sec = 2;
    timeout.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = inet_addr(server_ip);

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        close(sock);
        return -1; // Este servidor no está aceptando conexiones
    }

    // Recibir mensaje de bienvenida del servidor
    int recv_bytes = recv(sock, buffer, sizeof(buffer)-1, 0);
    if (recv_bytes <= 0) { 
        close(sock); 
        return -1; 
    }
    buffer[recv_bytes] = '\0';
    
    printf("Respuesta del servidor: %s", buffer);
    
    // Verificar si el servidor está listo (no está esperando turno)
    if (strstr(buffer, "Esperando") != NULL || strstr(buffer, "ERROR") != NULL) {
        close(sock);
        return -1;
    }

    if (filename) {
        // Enviar archivo
        FILE *fp = fopen(filename, "*.txt");
        if (!fp) { 
            perror("fopen"); 
            close(sock); 
            return -1; 
        }

        printf("Enviando archivo %s...\n", filename);
        
        int bytes;
        while ((bytes = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
            if (send(sock, buffer, bytes, 0) < 0) {
                perror("send");
                fclose(fp);
                close(sock);
                return -1;
            }
        }
        fclose(fp);

        // Cerrar envío para notificar fin de archivo
        shutdown(sock, SHUT_WR);

        // Recibir confirmación final
        recv_bytes = recv(sock, buffer, sizeof(buffer)-1, 0);
        if (recv_bytes > 0) {
            buffer[recv_bytes] = '\0';
            printf("Confirmación del servidor: %s", buffer);
        }
        
        logStatus(server_ip, port, "Archivo enviado exitosamente", filename);
    } else {
        // Consulta de estado
        printf("[Estado del servidor %s]: %s", server_ip, buffer);
        logStatus(server_ip, port, buffer, NULL);
    }

    close(sock);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage:\n");
        printf("  %s <SERVER_IP> [FILE]\n", argv[0]);
        printf("Si FILE no se pasa, se hará consulta de estado.\n");
        return 1;
    }

    const char *server_ip = argv[1];
    const char *filename = (argc >= 3) ? argv[2] : NULL;

    // Intentar conectar a cada servidor en orden hasta que uno acepte
    int success = -1;
    
    for (int attempt = 0; attempt < 2; attempt++) { // Intentar 2 ciclos completos
        for (int i = 0; i < NUM_SERVERS; i++) {
            printf("\n--- Intentando con %s (puerto %d) ---\n", 
                   server_names[i], server_ports[i]);
            
            if (try_connect_to_server(server_ip, server_ports[i], filename) == 0) {
                success = 0;
                break;
            }
            
            printf("Servidor %s no disponible, intentando siguiente...\n", server_names[i]);
            sleep(1); // Pequeña pausa entre intentos
        }
        
        if (success == 0) break;
        
        printf("\n--- Ningún servidor disponible, reintentando en 3 segundos ---\n");
        sleep(3);
    }

    if (success != 0) {
        printf("Error: No se pudo conectar a ningún servidor después de múltiples intentos\n");
        logStatus(server_ip, 0, "Fallo en todos los intentos de conexión", filename);
        return 1;
    }

    return 0;
}