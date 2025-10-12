#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>

char* leerArchivo(const char* path, size_t* len_out) {
    FILE* f = fopen(path, "rb");
    if (!f) {
        perror("fopen");
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);

    char* buf = malloc(sz + 1);
    if (!buf) {
        perror("malloc");
        fclose(f);
        return NULL;
    }

    size_t leidos = fread(buf, 1, sz, f);
    fclose(f);

    if (leidos != (size_t)sz) {
        fprintf(stderr, "Error al leer el archivo\n");
        free(buf);
        return NULL;
    }

    buf[leidos] = '\0';
    *len_out = leidos;
    return buf;
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Uso: %s <ARCHIVO> <PUERTO> <alias_servidor1> [alias_servidor2] ...\n", argv[0]);
        return 1;
    }

    const char* archivo_path = argv[1];
    int puerto = atoi(argv[2]);
    // El resto de argumentos (desde el índice 3) son los alias de los servidores
    const char** servidores = (const char**)&argv[3];
    int num_servidores = argc - 3;

    size_t file_content_len;
    char* file_content = leerArchivo(archivo_path, &file_content_len);
    if (!file_content) {
        return 1;
    }

    const char* filename = strrchr(archivo_path, '/');
    if (filename == NULL) {
        filename = archivo_path;
    } else {
        filename++;
    }

    int enviado = 0;
    // Bucle para reintentar hasta que un servidor acepte el archivo
    while (!enviado) {
        // Iteramos sobre la lista de servidores que nos pasaron
        for (int i = 0; i < num_servidores; i++) {
            const char* alias_actual = servidores[i];
            printf("--- Intentando conectar con: %s ---\n", alias_actual);

            int cliente_fd;
            struct sockaddr_in direccion;

            cliente_fd = socket(AF_INET, SOCK_STREAM, 0);
            if (cliente_fd < 0) {
                perror("Error al crear socket");
                continue; // Intentar con el siguiente servidor
            }

            struct hostent* host_info = gethostbyname(alias_actual);
            if (host_info == NULL) {
                fprintf(stderr, "Error: No se pudo resolver el host '%s'\n", alias_actual);
                close(cliente_fd);
                continue;
            }

            direccion.sin_family = AF_INET;
            direccion.sin_port = htons(puerto);
            direccion.sin_addr = *((struct in_addr*)host_info->h_addr_list[0]);

            // Intentamos conectar. Si falla, es probable que no sea el turno de este servidor.
            if (connect(cliente_fd, (struct sockaddr*)&direccion, sizeof(direccion)) == 0) {
                // Encontramos al servidor que tenía el turno.
                printf("¡Conexión exitosa con %s! Enviando archivo...\n", alias_actual);

                // 1. Enviar nombre del archivo
                if (send(cliente_fd, filename, strlen(filename), 0) < 0) {
                    perror("Error al enviar nombre del archivo");
                    close(cliente_fd);
                    continue;
                }
                char name_ack[32] = {0};
    		recv(cliente_fd, name_ack, sizeof(name_ack) - 1, 0);
    		if (strcmp(name_ack, "OK_NAME") != 0) {
        		fprintf(stderr, "Error: El servidor no confirmó el nombre del archivo.\n");
        		close(cliente_fd);
        		continue; // Intentar con el siguiente servidor
    		}
    		printf("Servidor confirmó el nombre. Enviando contenido...\n");
                 

                // 2. Enviar contenido del archivo
                if (send(cliente_fd, file_content, file_content_len, 0) < 0) {
                     perror("Error al enviar contenido del archivo");
                     close(cliente_fd);
                     continue;
                }

                printf("Archivo enviado. Esperando confirmación...\n");
                
                char buffer_respuesta[1024] = {0};
                recv(cliente_fd, buffer_respuesta, sizeof(buffer_respuesta) - 1, 0);
                printf("Respuesta del servidor: %s\n", buffer_respuesta);

                close(cliente_fd);
                enviado = 1; // Marcamos que ya lo enviamos
                break;       // Salimos del bucle for
            } else {
                // Si la conexión falla, Simplemente no era su turno.
                printf("Servidor %s no disponible, intentando con el siguiente.\n", alias_actual);
                close(cliente_fd);
            }
        }

        if (!enviado) {
            printf("\nNingún servidor estaba disponible. Esperando 2 segundos para reintentar...\n");
            sleep(2); // Esperar un poco antes de volver a escanear la lista
        }
    }

    free(file_content);
    printf("\nProceso completado.\n");

    return 0;
}
