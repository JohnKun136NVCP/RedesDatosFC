#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// Biblioteca para trabajar con hilos
#include <pthread.h>

#define BUFFER_SIZE 1024

struct datos_programa
{
    int *client_sock;
    struct sockaddr_in *serv_addr;
    const char *server_ip;
    int puerto;
    char *rutaArchivo;
    char *shift;
};

void sendFile(const char *filename, int sockfd)
{
    FILE *fp = fopen(filename, "r");
    if (fp == NULL)
    {
        perror("[-] Cannot open the file");
        return;
    }
    char buffer[BUFFER_SIZE];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), fp)) > 0)
    {
        if (send(sockfd, buffer, bytes, 0) == -1)
        {
            perror("[-] Error to send the file");
            break;
        }
    }
    fclose(fp);
}

/* Función para configuración del socket y conexión al servidor
 */
int conectarServidor(struct sockaddr_in *serv_addr, int client_sock, const char *server_ip, int puerto)
{
    // Flechas porque son apuntadores
    serv_addr->sin_family = AF_INET;
    serv_addr->sin_port = htons(puerto);
    serv_addr->sin_addr.s_addr = inet_addr(server_ip);
    // intentamos conectarnos al socket de servidor usando nuestro socket de cliente con nuestro socket configurado para conexiones
    if (connect(client_sock, (struct sockaddr *)serv_addr, sizeof(*serv_addr)) < 0)
    {
        // Si el puerto está cerrado o no hay servidor avisamos
        printf("[*] CONNECTION TO SERVER %d failed\n", puerto);
        return 1;
    }
    return 0;
}

/* Función para autorizar el envío del archivo al servidor y enviarlo si es autorizado respecto a la ruta y el desplazamiento
 */
void autorizarEnviarArchivo(int client_sock, int puerto, char *shift, char *rutaArchivo)
{
    char mensaje[BUFFER_SIZE];
    char buffer[BUFFER_SIZE] = {0};

    // Formateamos y enviamos puerto objetivo y desplazamiento
    snprintf(mensaje, sizeof(mensaje), "%s %s", rutaArchivo, shift);
    send(client_sock, mensaje, strlen(mensaje), 0);

    // Esperamos la respuesta del servidor con recv que recibe datos de un socket ya conectado
    int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes > 0)
    {
        buffer[bytes] = '\0';
        // Si la respuesta es positiva
        if (strstr(buffer, "REQUEST ACCEPTED") != NULL)
        {

            // enviamos el archivo
            sendFile(rutaArchivo, client_sock);

            // indicamos que ya no enviaremos más datos
            shutdown(client_sock, SHUT_WR);

            // Esperamos confiramación de recibido y cifrado
            bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
            if (bytes > 0)
            {
                buffer[bytes] = '\0';
                printf("[*] SERVER RESPONSE %d: %s\n", puerto, buffer);
            }
            close(client_sock);
        }
        else
        {
            printf("[*] SERVER RESPONSE %d: %s\n", puerto, buffer);
            close(client_sock);
        }
    }
    else
    {
        printf("[-] Server connection timeout\n");
    }
}

/*
Función principal que ejecutará cada hilo del cliente para conectarse a un servidor con un puerto específico y enviar un archivo.
*/
void *programaCliente(void *arg)
{
    struct datos_programa *datos = (struct datos_programa *)arg;
    int *client_sock = datos->client_sock;
    struct sockaddr_in *serv_addr = datos->serv_addr;
    const char *server_ip = datos->server_ip;
    int puerto = datos->puerto;
    char *rutaArchivo = datos->rutaArchivo;
    char *shift = datos->shift;

    // Creamos el socket de tipo TCP ipv4
    *client_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (*client_sock == -1)
    {
        perror("[-] Error to create the socket");
        return NULL;
    }
    if (conectarServidor(serv_addr, *client_sock, server_ip, puerto) != 1)
    {
        autorizarEnviarArchivo(*client_sock, puerto, shift, rutaArchivo);
    }
    close(*client_sock);
}

int main(int argc, char *argv[])
{
    if (argc != 9)
    {
        printf("Type: %s <server_ip> <port1> <port2> <port3> <file_path1> <file_path2> <file_path3> <shift>\n", argv[0]);
        return 1;
    }

    // Obtenemos los argumentos que van a necesitar todos los hilos
    char *server_ip = argv[1];
    int puerto1 = atoi(argv[2]);
    int puerto2 = atoi(argv[3]);
    int puerto3 = atoi(argv[4]);
    char *rutaArchivo1 = argv[5];
    char *rutaArchivo2 = argv[6];
    char *rutaArchivo3 = argv[7];
    char *shift = argv[8];
    int client_sock1;
    struct sockaddr_in serv_addr1;
    int client_sock2;
    struct sockaddr_in serv_addr2;
    int client_sock3;
    struct sockaddr_in serv_addr3;

    // Creamos los 3 struct para datos del programa que pasaremos como argumento
    struct datos_programa datos1;
    datos1.client_sock = &client_sock1;
    datos1.serv_addr = &serv_addr1;
    datos1.server_ip = server_ip;
    datos1.puerto = puerto1;
    datos1.rutaArchivo = rutaArchivo1;
    datos1.shift = shift;

    struct datos_programa datos2;
    datos2.client_sock = &client_sock2;
    datos2.serv_addr = &serv_addr2;
    datos2.server_ip = server_ip;
    datos2.puerto = puerto2;
    datos2.rutaArchivo = rutaArchivo2;
    datos2.shift = shift;

    struct datos_programa datos3;
    datos3.client_sock = &client_sock3;
    datos3.serv_addr = &serv_addr3;
    datos3.server_ip = server_ip;
    datos3.puerto = puerto3;
    datos3.rutaArchivo = rutaArchivo3;
    datos3.shift = shift;

    // Funcionalidad para el envío de 3 archivos a 3 puertos diferentes
    pthread_t t1, t2, t3;

    // Creamos los hilos con la función del programa y el puerto correspondiente
    pthread_create(&t1, NULL, programaCliente, &datos1);
    pthread_create(&t2, NULL, programaCliente, &datos2);
    pthread_create(&t3, NULL, programaCliente, &datos3);

    // Hacemos que el hilo main espere a que terminen los demás hilos
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    pthread_join(t3, NULL);

    return 1;
}
