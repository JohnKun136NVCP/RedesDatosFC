#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <sys/stat.h>

#define BUFFER_SIZE 1024
#define server_port 49200  // Puerto base 


// Función para guardar archivo en el directorio del servidor
void saveFile(const char *server_name, const char *filename, const char *content) {
    char file_path[256];
    char *home_dir = getenv("HOME");
    if (home_dir == NULL) {
        home_dir = "/home";
        printf("Warning: HOME environment variable not set, using %s\n", home_dir);
    }
    
    snprintf(file_path, sizeof(file_path), "%s/%s/%s", home_dir, server_name, filename);
    FILE *file = fopen(file_path, "w");
    if (file) {
        fprintf(file, "%s", content);
        fclose(file);
    }
}

/*
    Función principal para conectar el servidor en el puerto 49200 y asignar puertos dinámicos a los clientes
    para recibir archivos y guardarlos en el directorio correspondiente (s01 o s02)
*/
int main(int argc, char *argv[]) {

    int port_s;
    int client_port; 
    struct sockaddr_in server_addr;
    int port_counter = 1;

    if (argc < 5) { 
        printf("Use: %s <s01> <s02> <s03> <s04>\n", argv[0]);
        return 1;
    }

    // Creamos el socket principal para el puerto base
    port_s = socket(AF_INET, SOCK_STREAM, 0);
    if (port_s < 0) {
        perror("[-] Error creating socket");
        return 1;
    }

    int opt = 1;
    // Permitimos que se vuelva a usar el puerto después de termianr la ejecución del programa
    if (setsockopt(port_s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt SO_REUSEADDR failed");
        return 1;
    }
    
    // Configuramos la dirección del servidor
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // Asignamos el socket a la dirección y puerto especificados
    if (bind(port_s, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("[-] Error binding");
        close(port_s);
        return 1;
    }

    // Escuchamos conexiones entrantes
    if (listen(port_s, 10) < 0) {
        perror("[-] Error on listen");
        close(port_s);
        return 1;
    }

    printf("[*] LISTENING on port %d...\n", server_port);
    printf("[*] Server started. Waiting for connections...\n");

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addr_size = sizeof(client_addr);
        client_port = accept(port_s, (struct sockaddr*)&client_addr, &addr_size);
        
        if (client_port < 0) {
            perror("Accept error");
            continue;
        }

        // Asignamos un puerto dinámico al cliente mayor al puerto base
        int dynamic_port = server_port + port_counter;
        port_counter++;

        // Enviamos el puerto dinámico al cliente
        char port_msg[64];
        snprintf(port_msg, sizeof(port_msg), "DYNAMIC_PORT|%d", dynamic_port);
        send(client_port, port_msg, strlen(port_msg), 0);
        close(client_port);

        // Creamos el socket para el puerto dinámico
        int dynamic_sock = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in dynamic_addr;
        
        dynamic_addr.sin_family = AF_INET;
        dynamic_addr.sin_port = htons(dynamic_port);
        dynamic_addr.sin_addr.s_addr = INADDR_ANY;
        
        // Permitimos que se vuelva a usar el puerto después de termianr la ejecución del programa
        int dyn_opt = 1;
        setsockopt(dynamic_sock, SOL_SOCKET, SO_REUSEADDR, &dyn_opt, sizeof(dyn_opt));
        
        // Asignamos el socket a la dirección y puerto especificados
        if (bind(dynamic_sock, (struct sockaddr*)&dynamic_addr, sizeof(dynamic_addr)) < 0) {
            perror("Bind error on dynamic port");
            close(dynamic_sock);
            continue;
        }
        
        // Escuchamos conexiones entrantes
        if (listen(dynamic_sock, 1) < 0) {
            perror("Listen error on dynamic port");
            close(dynamic_sock);
            continue;
        }
        
        printf("[*] Assigned dynamic port %d to client\n", dynamic_port);

        // Aceptamos la conexión del cliente en el puerto dinámico
        int dynamic_client = accept(dynamic_sock, NULL, NULL);
        if (dynamic_client < 0) {
            perror("Accept error on dynamic port");
            close(dynamic_sock);
            continue;
        }
        
        char buffer[BUFFER_SIZE] = {0};
        char file_content[BUFFER_SIZE] = {0};
        int requested_port;
        char filename[256];

        int bytes = recv(dynamic_client, buffer, sizeof(buffer) - 1, 0);
        if (bytes > 0) {
            buffer[bytes] = '\0';
            
            char alias[32];
            if (sscanf(buffer, "%31[^|]|%255[^|]|%[^\n]", alias, filename, file_content) == 3) {
                saveFile(alias, filename, file_content);
                
                char *msg = "File received successfully";
                send(dynamic_client, msg, strlen(msg), 0);
                printf("[SERVER %s] File %s saved\n", alias, filename);
            } else {
                char *msg = "REJECTED";
                send(dynamic_client, msg, strlen(msg), 0);
                printf("[SERVER] Request rejected - invalid format\n");
            }
        }
        
        close(dynamic_client);
        close(dynamic_sock);
    }
    
    close(port_s);
    return 0;
}
