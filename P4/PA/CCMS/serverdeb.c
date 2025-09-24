#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h> //Manejo de tiempo para timestamps
#include <sys/stat.h> //Para hacer directorio
#include <sys/types.h>

#define PORT1 49200 
#define PORT2 49201
#define PORT3 49202 //Puertos en los que el servidor va a escuchar

#define BUFFER_SIZE 1024 // Tamaño del buffer para recibir datos

//Funcion para obtener la informacion del sistema.

//Funcion de encriptado cesar.
void caesarCrypt(char *text, int shift) {
    shift = shift % 26;
    for (int i = 0; text[i] != '\0'; i++) {
        char c = text[i];
        if (isupper(c)) {
            text[i] = ((c - 'A' + shift) % 26) + 'A';
        } else if (islower(c)) {
            text[i] = ((c - 'a' + shift) % 26) + 'a';
        } else {
            text[i] = c;
        }
    }
}

//Servidor de sockets
int create_server_socket(int port) {
    //descriptor de socket
    int sockfd;
    struct sockaddr_in server_addr;
    
    //TCP sockets
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("[-] Error creando socket");
        exit(1);
    }
    
    //Limpieza del buffer.
    memset(&server_addr, 0, sizeof(server_addr));
    //Configuracion inicial de familia, port y ips.
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("[-] Error en bind");
        exit(1);
    }

    if (listen(sockfd, 5) < 0) {
        perror("[-] Error en listen");
        exit(1);
    }

    printf("[+] Escuchando en puerto %d...\n", port);
    return sockfd;
}


//Funcion main.
int main() {
    //Declaracion de socket
    int sock1, sock2, sock3;
    //Valor maximo
    int max_fd;
    //Lista de descriptores.
    fd_set readfds;
    struct sockaddr_in client_addr;
    socklen_t addr_size;
    char buffer[BUFFER_SIZE];
    int client_sock;

    //Crea sockets para cada puerto
    sock1 = create_server_socket(PORT1);
    sock2 = create_server_socket(PORT2);
    sock3 = create_server_socket(PORT3);

    //Establecer el valor maximo de descriptores a vigilar con select()
    max_fd = sock1;
    if (sock2 > max_fd) max_fd = sock2;
    if (sock3 > max_fd) max_fd = sock3;

    while (1) {
        //Limpiamos lista con FD_ZERO
        FD_ZERO(&readfds);
        //Añadimos sockets a la lista con FD_SET
        FD_SET(sock1, &readfds);
        FD_SET(sock2, &readfds);
        FD_SET(sock3, &readfds);

        printf("Esperando conexiones...\n");

        //select() espera que alguno de los sockets tenga actividad
        if (select(max_fd + 1, &readfds, NULL, NULL, NULL) < 0) {
            perror("[-] Error en select");
            exit(1);
        }

        //Checa si sock1 esta activo y hace la encriptacion de la informacion
        if (FD_ISSET(sock1, &readfds)) {
            addr_size = sizeof(client_addr);
            client_sock = accept(sock1, (struct sockaddr*)&client_addr, &addr_size);
            //Convertir el IP del cliente
            char client_ip_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, client_ip_str, INET_ADDRSTRLEN);
            printf("[PUERTO %d] Conexión desde %s\n", PORT1, client_ip_str);

            int bytes = recv(client_sock, buffer, BUFFER_SIZE - 1, 0);
            if (bytes > 0) {
                buffer[bytes] = '\0';

                int shift;
                char texto[BUFFER_SIZE];
                sscanf(buffer, "%d %[^\n]", &shift, texto);

                caesarCrypt(texto, shift);
                //Ahora guardamos los datos recibidos en la carpeta correspodiente al servidor junto con su nombre.
                // Define el nombre de este servidor
               char *server_name = "s01"; // Cambia según sea s01, s02, etc.

                // Carpeta donde se guardarán los archivos
               char dirpath[256];
               char *home = getenv("HOME"); 
                if (!home) home = "/tmp";
                snprintf(dirpath, sizeof(dirpath), "%s/%s", home, server_name);

                mkdir(dirpath, 0755); //Crear directorio si no existe

                // archivo con timestamp
                time_t now = time(NULL);
                struct tm *t = localtime(&now);
                char filename[512];
                snprintf(filename, sizeof(filename), "%s/file_%04d%02d%02d_%02d%02d%02d.txt", dirpath, t->tm_year+1900, t->tm_mon+1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);

                FILE *fp = fopen(filename, "w");
                    if (fp) {
                        fputs(texto, fp);
                        fclose(fp);
                    }

                send(client_sock, "ARCHIVO CIFRADO RECIBIDO", strlen("ARCHIVO CIFRADO RECIBIDO") + 1, 0);
                printf("[PUERTO %d] Conexión desde %s\n", PORT1, client_ip_str);
                printf("[PUERTO %d] Archivo cifrado y guardado como %s\n", PORT1, filename);
                
            }
            close(client_sock);
        }
        
        //Checa si sock2 esta activo y hace la encriptacion de la informacion
        if (FD_ISSET(sock2, &readfds)) {
            addr_size = sizeof(client_addr);
            client_sock = accept(sock2, (struct sockaddr*)&client_addr, &addr_size);
            //Convertir el IP del cliente
            char client_ip_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, client_ip_str, INET_ADDRSTRLEN);
            printf("[PUERTO %d] Conexión desde %s\n", PORT2, client_ip_str);

            int bytes = recv(client_sock, buffer, BUFFER_SIZE - 1, 0);
            if (bytes > 0) {
                buffer[bytes] = '\0';

                int shift;
                char texto[BUFFER_SIZE];
                sscanf(buffer, "%d %[^\n]", &shift, texto);

                caesarCrypt(texto, shift);
                //Bloque para definir el nombre y carpeta de guardado como en el anterior.
                char *server_name = "s01"; 

                char dirpath[256];
                char *home = getenv("HOME"); 
                if (!home) home = "/tmp";
                snprintf(dirpath, sizeof(dirpath), "%s/%s", home, server_name);

                mkdir(dirpath, 0755); 

                time_t now = time(NULL);
                struct tm *t = localtime(&now);
                char filename[512];
                snprintf(filename, sizeof(filename), "%s/file_%04d%02d%02d_%02d%02d%02d.txt", dirpath, t->tm_year+1900, t->tm_mon+1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
                FILE *fp = fopen(filename, "w");
                    if (fp) {
                        fputs(texto, fp);
                        fclose(fp);
                    }

                send(client_sock, "ARCHIVO CIFRADO RECIBIDO", strlen("ARCHIVO CIFRADO RECIBIDO") + 1, 0);
                printf("[PUERTO %d] Archivo cifrado y guardado como %s\n", PORT2, filename);
                
            }
            close(client_sock);
        }
        
        //Checa si sock3 esta activo y hace la encriptacion de la informacion
        if (FD_ISSET(sock3, &readfds)) {
            addr_size = sizeof(client_addr);
            client_sock = accept(sock3, (struct sockaddr*)&client_addr, &addr_size);
            //Convertir el IP del cliente
            char client_ip_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, client_ip_str, INET_ADDRSTRLEN);
            printf("[PUERTO %d] Conexión desde %s\n", PORT3, client_ip_str);

            int bytes = recv(client_sock, buffer, BUFFER_SIZE - 1, 0);
            if (bytes > 0) {
                buffer[bytes] = '\0';

                int shift;
                char texto[BUFFER_SIZE];
                sscanf(buffer, "%d %[^\n]", &shift, texto);

                caesarCrypt(texto, shift);
                //Bloque para definir el nombre y carpeta de guardado como en el anterior.
               char *server_name = "s01"; 

               char dirpath[256];
               char *home = getenv("HOME"); 
                if (!home) home = "/tmp";
                snprintf(dirpath, sizeof(dirpath), "%s/%s", home, server_name);

                mkdir(dirpath, 0755); 

                time_t now = time(NULL);
                struct tm *t = localtime(&now);
                char filename[512];
                //Guarda el archivo con la timestamp
                snprintf(filename, sizeof(filename), "%s/file_%04d%02d%02d_%02d%02d%02d.txt", dirpath, t->tm_year+1900, t->tm_mon+1, t->tm_mday, t->tm_hour, t->tm_min, t->tm_sec);
                FILE *fp = fopen(filename, "w");
                    if (fp) {
                        fputs(texto, fp);
                        fclose(fp);
                    }

                send(client_sock, "ARCHIVO CIFRADO RECIBIDO", strlen("ARCHIVO CIFRADO RECIBIDO") + 1, 0);
                printf("[PUERTO %d] Archivo cifrado y guardado como %s\n", PORT3, filename);
                
            }
            close(client_sock);
        }
    }

    close(sock1);
    close(sock2);
    close(sock3);

    return 0;
}
