#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
//aumentamos el tamaño del buffer debido a que la nueva cadena es más larga
#define BUFFER_SIZE 2048
#define MAX_PUERTOS 3 //los puertos maximos que podemos meter
#define MAX_TXT 3 //el núm meximo de mensajes


void print_usage(char *program_name) {
    printf("Uso: %s <IP_SERVIDOR> <PUERTO1,PUERTO2,PUERTO3> <DESPLAZAMIENTO1,DESPLAZAMIENTO2,DESPLAZAMIENTO3> <ARCHIVO1,ARCHIVO2,ARCHIVO3>\n", program_name);
}


//separa los puertos con comas
int parse_ports(char *port_str, int ports[]) {
    char *token = strtok(port_str, ",");
    int count = 0;
    
    while (token != NULL && count < MAX_PUERTOS) {
        ports[count++] = atoi(token);
        token = strtok(NULL, ",");
    }
    return count;
}

//separa el desplazamiento con comas
int parse_shifts(char *shift_str, int shifts[]) {
    char *token = strtok(shift_str, ",");
    int count = 0;
    
    while (token != NULL && count < MAX_PUERTOS) {
        shifts[count++] = atoi(token);
        token = strtok(NULL, ",");
    }
    return count;
}

//separa los txt con comas
int parse_filenames(char *file_str, char filenames[][BUFFER_SIZE]) {
    char *token = strtok(file_str, ",");
    int count = 0;
    
    while (token != NULL && count < MAX_TXT) {
        strncpy(filenames[count], token, BUFFER_SIZE - 1);
        filenames[count][BUFFER_SIZE - 1] = '\0';
        count++;
        token = strtok(NULL, ",");
    }
    return count;
}

//lee el txt que le pasamos con el mensaje 
char* read_file(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("[-] fopen");
        return NULL;
    }
    
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    char *content = (char *)malloc(file_size + 1);
    if (!content) {
        perror("[-] malloc");
        fclose(fp);
        return NULL;
    }
    
    fread(content, 1, file_size, fp);
    content[file_size] = '\0';
    fclose(fp);
    
    return content;
}

//verifica que se le pasen los argumentos (IP, puerto,desplazamiento
// mensaje)
int main(int argc, char *argv[]) {
    if (argc != 5) {
        print_usage(argv[0]);
        return 1;
    }

    char *server_ip = argv[1];
    char *port_str = argv[2];
    char *shift_str = argv[3];
    char *file_str = argv[4];

    int ports[MAX_PUERTOS], shifts[MAX_PUERTOS];
    char filenames[MAX_TXT][BUFFER_SIZE];
    
    int num_ports = parse_ports(strdup(port_str), ports);
    int num_shifts = parse_shifts(strdup(shift_str), shifts);
    int num_files = parse_filenames(strdup(file_str), filenames);

    // Validar que los números coincidan
    if (num_ports != num_shifts || num_ports != num_files) {
        printf("[-] Error: El número de puertos, desplazamiento y archivos debe coincidir\n");
        printf("   Puertos: %d, Desplazamiento: %d, Archivos: %d\n", num_ports, num_shifts, num_files);
        print_usage(argv[0]);
        return 1;
    }

    printf("[Cliente] Enviando %d archivos a %s\n", num_ports, server_ip);

    for (int i = 0; i < num_ports; i++) {
        printf("\n[Cliente] Procesando archivo %d/%d:\n", i + 1, num_ports);
        printf("  Puerto: %d\n", ports[i]);
        printf("  Desplazamiento: %d\n", shifts[i]);
        printf("  Archivo: %s\n", filenames[i]);

        // Leer el archivo
        char *file_content = read_file(filenames[i]);
        if (!file_content) {
            printf("[-] Error leyendo archivo %s\n", filenames[i]);
            continue;
        }

        // Limpiar saltos de línea del contenido
        file_content[strcspn(file_content, "\n\r")] = '\0';

        // Conectar y enviar a TODOS los puertos de servidor (49200, 49201, 49202)
        int server_ports[] = {49200, 49201, 49202};
        
        for (int j = 0; j < 3; j++) {
            int sock = socket(AF_INET, SOCK_STREAM, 0);
            if (sock < 0) {
                perror("[-] socket");
                continue;
            }

            struct sockaddr_in serv_addr;
            serv_addr.sin_family = AF_INET;
            serv_addr.sin_port = htons(server_ports[j]);
            serv_addr.sin_addr.s_addr = inet_addr(server_ip);

            if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
                fprintf(stderr, "[-] No se pudo conectar a %s:%d (%s)\n", 
                        server_ip, server_ports[j], strerror(errno));
                close(sock);
                continue;
            }

            // Manda el mensaje de en qué pueto está
            char mensaje[BUFFER_SIZE];
            snprintf(mensaje, sizeof(mensaje), "%d %d %s", ports[i], shifts[i], file_content);
            
            if (send(sock, mensaje, strlen(mensaje), 0) < 0) {
                perror("[-] send");
                close(sock);
                continue;
            }

            char buffer[BUFFER_SIZE];
            int bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);
            if (bytes > 0) {
                buffer[bytes] = '\0';
                printf("  Respuesta de servidor %d: %s\n", server_ports[j], buffer);
            } else if (bytes == 0) {
                printf("  Servidor %d cerró la conexión\n", server_ports[j]);
            } else {
                perror("[-] recv");
            }

            close(sock);
        }

        free(file_content);
    }

    printf("\n[Cliente] Todos los archivos procesados\n");
    return 0;
}