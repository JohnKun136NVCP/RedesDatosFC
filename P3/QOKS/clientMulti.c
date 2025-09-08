/*
 * Práctica III. Cliente Multi-archivo.
 *
 * Cliente que envía múltiples archivos a múltiples puertos para trabajar con el servidor.
 *
 * Compilación:
 *   gcc clientMulti.c -o clientmulti
 *
 * Uso:
 *   ./clientmulti <IP> <PUERTO1> <PUERTO2> <PUERTO3> <ARCHIVO1> <ARCHIVO2> <ARCHIVO3> <DESPLAZAMIENTO>
 *
 * Ejemplo:
 *   ./clientmulti 192.168.0.193 49200 49201 49202 file.txt file2.txt file3.txt 49
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

#define BUFFER_SIZE 4096
#define NUM_SERVERS 3

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

// Puertos específicos de las 3 instancias del servidor Ubuntu
int server_ports[NUM_SERVERS] = {49200, 49201, 49202};

int main(int argc, char *argv[])
{
    // Argumentos de línea de comandos: ./clientmulti <IP> <PUERTO1> <PUERTO2> <PUERTO3> <ARCHIVO1> <ARCHIVO2> <ARCHIVO3> <DESPLAZAMIENTO>
    if (argc != 9)
    {
        printf("Uso: %s <IP> <PUERTO1> <PUERTO2> <PUERTO3> <ARCHIVO1> <ARCHIVO2> <ARCHIVO3> <DESPLAZAMIENTO>\n", argv[0]);
        printf("Ejemplo: %s 192.168.0.193 49200 49201 49202 file.txt file2.txt file3.txt 49\n", argv[0]);
        return 1;
    }

    // Parámetros Cliente
    char *server_ip = argv[1];                                    // Dirección IP del servidor
    int ports[3] = {atoi(argv[2]), atoi(argv[3]), atoi(argv[4])}; // Puertos de destino
    char *filenames[3] = {argv[5], argv[6], argv[7]};             // Nombres de archivos
    int shift = atoi(argv[8]);                                    // Desplazamiento César para cifrado

    printf(BOLD TEAL "\n[+] Iniciando cliente" RESET "\n");
    printf(BOLD CYAN "[+]   IP Servidor: " RESET "%s\n", server_ip);
    printf(BOLD CYAN "[+]   Puertos: " RESET "%d, %d, %d\n", ports[0], ports[1], ports[2]);
    printf(BOLD CYAN "[+]   Archivos: " RESET "%s, %s, %s\n", filenames[0], filenames[1], filenames[2]);
    printf(BOLD CYAN "[+]   Desplazamiento César: " RESET "%d\n", shift);

    // I: Procesamiento secuencial de archivos múltiples
    for (int i = 0; i < 3; i++)
    {
        printf(BOLD "\n[+] Procesando archivo %d/3" RESET "\n", i + 1);
        printf(CYAN "[+]   Archivo: " RESET BOLD "%s" RESET "\n", filenames[i]);
        printf(TEAL "[+]   Puerto destino: " RESET BOLD "%d" RESET "\n", ports[i]);

        // II: Lectura del archivo histórico desde disco
        FILE *file = fopen(filenames[i], "r");
        if (!file)
        {
            printf(RED "[Puerto %d] ERROR: No se pudo leer el archivo %s" RESET "\n", ports[i], filenames[i]);
            continue;
        }

        char file_content[BUFFER_SIZE];                                    // Buffer para contenido del archivo
        memset(file_content, 0, BUFFER_SIZE);                              // Limpiar buffer
        size_t bytes_read = fread(file_content, 1, BUFFER_SIZE - 1, file); // Leer contenido
        fclose(file);

        printf(YELLOW "[+]   Tamaño leído: " RESET "%zu bytes\n", bytes_read);

        // III: Creación de socket TCP para conexión
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0)
        {
            printf(RED "[Puerto %d] ERROR: No se pudo crear socket" RESET "\n", ports[i]);
            continue;
        }

        // IV: Configuración de dirección del servidor
        struct sockaddr_in server_addr;                       // Estructura de dirección servidor
        memset(&server_addr, 0, sizeof(server_addr));         // Limpiar estructura
        server_addr.sin_family = AF_INET;                     // Protocolo IPv4
        server_addr.sin_port = htons(ports[i]);               // Puerto en formato de red
        inet_pton(AF_INET, server_ip, &server_addr.sin_addr); // Convertir IP

        // V: Establecimiento de conexión con servidor
        if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == 0)
        {
            printf(BOLD GREEN "[+] Conectado al servidor puerto %d" RESET "\n", ports[i]);

            // VI: Preparación del mensaje para envío
            char message[BUFFER_SIZE + 64]; // Buffer para mensaje (incluye puerto + shift)
            int result = snprintf(message, sizeof(message), "%d\n%d\n%s", ports[i], shift, file_content);

            // Verificar que el mensaje
            if (result >= (int)sizeof(message))
            {
                printf(RED "[Puerto %d] ERROR: Mensaje demasiado largo" RESET "\n", ports[i]);
                close(sock);
                continue;
            }

            // VII: Envío de datos al servidor
            ssize_t bytes_sent = send(sock, message, strlen(message), 0);
            if (bytes_sent > 0)
            {
                printf(BLUE "[+]   Bytes enviados: " RESET "%zd\n", bytes_sent);

                // VIII: Recepción de respuesta del servidor
                char response[BUFFER_SIZE];       // Buffer para respuesta
                memset(response, 0, BUFFER_SIZE); // Limpiar buffer
                int bytes_received = recv(sock, response, BUFFER_SIZE - 1, 0);

                if (bytes_received > 0)
                {
                    response[bytes_received] = '\0'; // Terminar cadena

                    // IX: Análisis de respuesta del servidor
                    if (strstr(response, "ARCHIVO CIFRADO RECIBIDO"))
                    {
                        printf(BOLD GREEN "[+] Archivo Cifrado" RESET "\n");
                    }
                    else if (strstr(response, "PROCESADO"))
                    {
                        printf(BOLD GREEN "[Puerto %d] ARCHIVO PROCESADO" RESET "\n", ports[i]);
                        // Mostrar texto cifrado si está disponible
                        char *newline = strchr(response, '\n');
                        if (newline)
                        {
                            printf(MAGENTA "[+] Texto cifrado:\n" RESET "%s\n", newline + 1);
                        }
                    }
                    else if (strstr(response, "RECHAZADO"))
                    {
                        printf(BOLD RED "[Puerto %d] SOLICITUD RECHAZADA" RESET "\n", ports[i]);
                    }
                    else
                    {
                        printf(YELLOW "[Puerto %d] ? Respuesta desconocida: %s" RESET "\n", ports[i], response);
                    }
                }
                else
                {
                    printf(RED "[Puerto %d] ERROR: No se recibió respuesta del servidor" RESET "\n", ports[i]);
                }
            }
            else
            {
                printf(RED "[Puerto %d] ERROR: No se pudo enviar datos" RESET "\n", ports[i]);
            }
        }
        else
        {
            printf(RED "[Puerto %d] ERROR: No se pudo conectar al servidor" RESET "\n", ports[i]);
        }

        // X: Cierre de conexión
        close(sock);
        printf("[+] Conexión cerrada\n");
    }
    printf(BOLD CYAN "\n[+] Todos los archivos han sido procesados" RESET "\n\n");

    return 0;
}
