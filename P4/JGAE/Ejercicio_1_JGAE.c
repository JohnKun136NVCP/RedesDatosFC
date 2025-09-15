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
        perror("connect");
        // Si el puerto está cerrado o no hay servidor avisamos
        printf("[*] CONNECTION TO SERVER %d failed\n", puerto);
        return 1;
    }
    return 0;
}

/* Función para autorizar el envío del archivo al servidor y enviarlo si es autorizado respecto a la ruta y el desplazamiento
 */
void autorizarEnviarArchivo(int client_sock, int puerto, char *rutaArchivo)
{
    char mensaje[BUFFER_SIZE];
    char buffer[BUFFER_SIZE] = {0};

    // Formateamos y enviamos nombre del archivo
    snprintf(mensaje, sizeof(mensaje), "%s", rutaArchivo);
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

/**
 * Devolver el puerto al que el servidor nos manda a conectarnos para envío de archivos
 */
int obtenerPuertoServidor(int client_sock)
{
    // Buffer para transferencia de datos
    char buffer[BUFFER_SIZE] = {0};

    // recibimos el puerto al que el servidor nos autoriza a conectarnos
    int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes <= 0)
    {
        printf("[-] Missed server port\n");
        close(client_sock);
        return -1;
    }
    buffer[bytes] = '\0';

    // Extraemos el puerto autorizado y lo devolvemos como entero
    int puertoServer;
    sscanf(buffer, "%d", &puertoServer);
    return puertoServer;
}

/*
 * Función que imprime y guarda un mensaje al final del serchivo serverMessages.txt
 */
void guardarLog(char *mensaje, int longitud)
{
    char mensajeCompleto[BUFFER_SIZE];

    // Al mensaje le ponemos fecha y hora antes
    time_t now = time(NULL);
    struct tm t;
    localtime_r(&now, &t);
    strftime(mensajeCompleto, sizeof(mensajeCompleto) - 1, "[%Y-%m-%d %H:%M:%S] ", &t);
    strncat(mensajeCompleto, mensaje, sizeof(mensajeCompleto) - longitud - 1);
    mensajeCompleto[sizeof(mensajeCompleto) - 1] = '\0';
    // Abrimos el archivo en modo append para hasta el final poner el nuevo mensaje recibido
    FILE *file = fopen("serverMessages.txt", "a");
    if (file != NULL)
    {
        printf("%s\n", mensajeCompleto);
        fprintf(file, "%s\n", mensajeCompleto);
        fclose(file);
    }
    else
    {
        perror("[-] Cannot open log file");
    }
}

int manejadorServidor(int *client_sock, struct sockaddr_in *serv_addr, const char *server_ip, int puertoServer, char *rutaArchivo)
{
    char buffer[BUFFER_SIZE] = {0};
    // Nos deconectamos del primer socket del servidor
    close(*client_sock);
    // Ahora el socket será conectando al puerto obtenido
    *client_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (*client_sock == -1)
    {
        perror("[-] Error to create the connection socket provided by server");
        return -1;
    }
    if (conectarServidor(serv_addr, *client_sock, server_ip, puertoServer) != 1)
    {
        // Si la conexión es exitosa recibimos el estatus del servidor y lo guardamos en un archivo
        // Esperamos la respuesta del servidor con recv que recibe datos de un socket ya conectado
        int bytes = recv(*client_sock, buffer, sizeof(buffer) - 1, 0);
        if (bytes > 0)
        {
            buffer[bytes] = '\0';

            // Lo que nos mande el servidor lo guardamos en un archivo
            guardarLog(buffer, bytes);

            // Hacemos la espera aleatoria 1 a 3 segundosr
            srand(time(NULL) ^ getpid());
            int delay = 1 + rand() % 3;
            sleep(delay);

            // Enviamos el archivo con lo de la práctica pasada
            autorizarEnviarArchivo(*client_sock, puertoServer, rutaArchivo);
        }
        else
        {
            printf("[-] Server connection timeout\n");
        }
    }
    else
    {
        close(*client_sock);
        return -1;
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
    // char *shift = datos->shift;

    // Creamos el socket de tipo TCP ipv4
    *client_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (*client_sock == -1)
    {
        perror("[-] Error to create the socket");
        return NULL;
    }
    if (conectarServidor(serv_addr, *client_sock, server_ip, puerto) != 1)
    {
        // Obtenemos el puerto al que el servidor nos manda a conectarnos:
        int puertoServer = obtenerPuertoServidor(*client_sock);
        if (puertoServer == -1)
        {
            perror("[-] Error to get the server port");
            close(*client_sock);
            return NULL;
        }
        else
        {
            // Nos conectamos  al puerto obtenido por el server y manejamos el resto del programa
            if (manejadorServidor(client_sock, serv_addr, server_ip, puertoServer, rutaArchivo) == -1)
            {
                return NULL;
            }
        }
        close(*client_sock);
    }
}

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        printf("Type: %s <server_ip> <port1> <file_path>\n", argv[0]);
        return 1;
    }

    // Obtenemos los argumentos que van a necesitar todos los hilos
    char *server_ip = argv[1];
    int puerto1 = atoi(argv[2]);
    char *rutaArchivo1 = argv[3];
    int client_sock1;
    struct sockaddr_in serv_addr1;

    // Creamos los 3 struct para datos del programa que pasaremos como argumento
    struct datos_programa datos1;
    datos1.client_sock = &client_sock1;
    datos1.serv_addr = &serv_addr1;
    datos1.server_ip = server_ip;
    datos1.puerto = puerto1;
    datos1.rutaArchivo = rutaArchivo1;

    // Funcionalidad para el envío de 3 archivos a 3 puertos diferentes
    pthread_t t1;

    // Creamos los hilos con la función del programa y el puerto correspondiente
    pthread_create(&t1, NULL, programaCliente, &datos1);
    // Hacemos que el hilo main espere a que terminen los demás hilos
    pthread_join(t1, NULL);

    return 1;
}
