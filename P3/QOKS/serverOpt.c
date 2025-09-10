/*
 * Práctica III. Cliente - Servidor.
 *
 * Servidor que escucha en múltiples puertos simultáneamente
 * y guarda archivos cifrados en disco.
 *
 * Compilación:
 *   gcc serverOpt.c -o serveropt
 *
 * Uso:
 *   ./serveropt
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
#include <ctype.h>
#include <sys/select.h>

#define BUFFER_SIZE 4096

// Códigos ANSI para formato de texto en consola
#define BOLD "\033[1m"
#define RESET "\033[0m"
#define GREEN "\033[32m"
#define RED "\033[31m"
#define YELLOW "\033[33m"
#define BLUE "\033[34m"
#define CYAN "\033[36m"
#define TEAL "\033[96m"
#define MAGENTA "\033[35m"

// Función para cifrar texto usando cifrado César
// Referencia: server.c (función decryptCaesar)
// La diferencia principal es que sumamos el shift en lugar de restarlo
void Ejercicio_1_QOKS(char *text, int shift)
{
    shift = shift % 26;
    for (int i = 0; text[i] != '\0'; i++)
    {
        char c = text[i];
        if (isupper(c))
        {
            // Cifrar letras mayúsculas manteniendo el rango A-Z
            text[i] = ((c - 'A' + shift) % 26) + 'A';
        }
        else if (islower(c))
        {
            // Cifrar letras minúsculas manteniendo el rango a-z
            text[i] = ((c - 'a' + shift) % 26) + 'a';
        }
        // Otros caracteres se mantienen igual
    }
}

int main()
{
    // Parámetros servidor
    int ports[] = {49200, 49201, 49202}; // Puertos de escucha múltiples
    int server_socks[3];                 // Descriptores de socket
    struct sockaddr_in server_addrs[3];  // Estructuras de direcciones
    fd_set read_fds, temp_fds;           // Conjuntos de descriptores para select()
    int max_fd = 0;                      // Descriptor máximo para select()

    FD_ZERO(&read_fds);

    printf(BOLD TEAL "\n[+] Iniciando servidor" RESET "\n");

    // I: Creación de sockets TCP para múltiples puertos
    for (int i = 0; i < 3; i++)
    {
        server_socks[i] = socket(AF_INET, SOCK_STREAM, 0);
        if (server_socks[i] < 0)
        {
            perror("Error creando socket");
            exit(1);
        }

        // Permitir reutilizar dirección del socket
        int opt = 1;
        setsockopt(server_socks[i], SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        // II: Configuración de la dirección del servidor
        memset(&server_addrs[i], 0, sizeof(server_addrs[i])); // Limpiar estructura
        server_addrs[i].sin_family = AF_INET;                 // Protocolo IPv4
        server_addrs[i].sin_addr.s_addr = INADDR_ANY;         // Escuchar en todas las interfaces
        server_addrs[i].sin_port = htons(ports[i]);           // Puerto en formato de red

        // III: Enlace del socket al puerto
        if (bind(server_socks[i], (struct sockaddr *)&server_addrs[i], sizeof(server_addrs[i])) < 0)
        {
            printf(RED "Error en bind puerto %d" RESET "\n", ports[i]);
            exit(1);
        }

        // IV: Configuración del servidor en modo escucha
        if (listen(server_socks[i], 5) < 0)
        {
            printf(RED "Error en listen puerto %d" RESET "\n", ports[i]);
            exit(1);
        }

        printf(BOLD GREEN "[+]   Escuchando puerto %d" RESET "\n", ports[i]);

        // V: Preparar descriptores para select()
        FD_SET(server_socks[i], &read_fds);
        if (server_socks[i] > max_fd)
        {
            max_fd = server_socks[i];
        }
    }

    // VI: Bucle principal con multiplexación
    while (1)
    {
        temp_fds = read_fds;

        // Esperar actividad en cualquiera de los sockets
        int activity = select(max_fd + 1, &temp_fds, NULL, NULL, NULL);
        if (activity < 0)
        {
            perror("Error en select");
            break;
        }

        // VII: Verificar actividad en cada socket
        for (int i = 0; i < 3; i++)
        {
            if (FD_ISSET(server_socks[i], &temp_fds))
            {
                struct sockaddr_in client_addr;             // Dirección del cliente
                socklen_t client_len = sizeof(client_addr); // Tamaño de dirección cliente
                char buffer[BUFFER_SIZE];                   // Buffer para recepción de datos

                printf(BOLD "\n[+] Conexión en puerto %d" RESET "\n", ports[i]);

                // VIII: Aceptar conexión de cliente en puerto específico
                int client_sock = accept(server_socks[i], (struct sockaddr *)&client_addr, &client_len);
                if (client_sock >= 0)
                {
                    printf(BOLD GREEN "[+] Cliente conectado desde: " RESET "%s\n", inet_ntoa(client_addr.sin_addr));

                    // IX: Recepción de datos del cliente
                    memset(buffer, 0, BUFFER_SIZE);
                    int total_received = 0;
                    int bytes_received;
                    
                    // Recibir todos los datos del cliente
                    while (total_received < BUFFER_SIZE - 1) {
                        bytes_received = recv(client_sock, buffer + total_received, BUFFER_SIZE - 1 - total_received, 0);
                        if (bytes_received <= 0) break;
                        total_received += bytes_received;
                        // Si ya tenemos al menos el header y algo de contenido, intentamos procesar
                        if (total_received > 10 && strchr(buffer + 5, '\n') && strchr(strchr(buffer + 5, '\n') + 1, '\n')) {
                            break;
                        }
                    }

                    if (total_received > 0)
                    {
                        buffer[total_received] = '\0';

                        // X: Análisis del mensaje del cliente
                        int target_port, shift;         // Puerto objetivo y desplazamiento César
                        char file_content[BUFFER_SIZE]; // Contenido del archivo a cifrar

                        // Extraer puerto objetivo de la primera línea
                        char *first_newline = strchr(buffer, '\n');
                        if (first_newline)
                        {
                            *first_newline = '\0';
                            target_port = atoi(buffer);

                            // Extraer desplazamiento César de la segunda línea
                            char *second_line = first_newline + 1;
                            char *second_newline = strchr(second_line, '\n');
                            if (second_newline)
                            {
                                *second_newline = '\0';
                                shift = atoi(second_line);

                                // El resto es el contenido del archivo
                                strcpy(file_content, second_newline + 1);

                                printf(CYAN "[+]   Puerto solicitado: " RESET BOLD "%d" RESET "\n", target_port);
                                printf(TEAL "[+]   Puerto del servidor: " RESET BOLD "%d" RESET "\n", ports[i]);
                                printf(YELLOW "[+]   Desplazamiento César: " RESET BOLD "%d" RESET "\n", shift);
                                printf("[+]   Tamaño: %zu bytes\n", strlen(file_content));

                                // XI: Aplicar cifrado César
                                Ejercicio_1_QOKS(file_content, shift);

                                // XII: Guardar archivo cifrado
                                char filename[256];
                                snprintf(filename, sizeof(filename), "file_%d_cesar.txt", ports[i]);
                                FILE *file = fopen(filename, "w");
                                if (file)
                                {
                                    fprintf(file, "%s", file_content);
                                    fclose(file);
                                    printf(BOLD GREEN "[+] Archivo recibido y cifrado\n" RESET);
                                }

                                // XIII: Envío de confirmación al cliente (en un solo mensaje)
                                char complete_response[BUFFER_SIZE * 2];
                                snprintf(complete_response, sizeof(complete_response), "ARCHIVO CIFRADO RECIBIDO\n%s", file_content);
                                send(client_sock, complete_response, strlen(complete_response), 0);
                            }
                        }
                    }
                    else
                    {
                        printf(RED "[-] Error: No se recibieron datos del cliente" RESET "\n");
                    }

                    // XIV: Cerrar conexión con cliente
                    close(client_sock);
                    printf("[+] Conexión con cliente cerrada\n\n");
                    printf(BOLD CYAN "[+] === Listo para nueva conexión ===" RESET "\n");
                }
            }
        }
    }

    // XV: Limpieza final del servidor
    for (int i = 0; i < 3; i++)
    {
        close(server_socks[i]);
    }

    printf(BOLD TEAL "[+] servidor terminado" RESET "\n");
    return 0;
}
