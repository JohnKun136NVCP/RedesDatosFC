#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

// Biblioteca para trabajar con hilos
#include <pthread.h>

#define BUFFER_SIZE 1024 // Tama~no del buffer para recibir datos

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

/*
Imprime el contenido de un archivo  línea a línea
*/
void imprimirArchivo(const char *filename)
{
    FILE *fp = fopen(filename, "r");
    if (fp == NULL)
    {
        perror("[+] Server cannot open the encrypted file");
        return;
    }
    char buffer[BUFFER_SIZE];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), fp)) > 0)
    {
        // hacemos que el buffer se detenga hasta donde se llena por cada línea
        buffer[bytes] = '\0';
        printf("%s", buffer);
    }
    // cerramos el archivo
    fclose(fp);
}

/*
 * Función para crear un socket IPv4 TCP
 */
int crearSocketIPv4TCP()
{
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1)
    {
        perror("[-] Error to create the socket");
        return -1;
    }
    return server_sock;
}

/*
Función para configurar el socket servidor, debe recibir un puntero a la estrcutura para que sí se modifique y no sea paso por valor
*/
int configurarEnlazarSocket(struct sockaddr_in *server_addr, int puertoServer, int server_sock)
{
    // Configuramos el socket
    server_addr->sin_family = AF_INET;
    server_addr->sin_port = htons(puertoServer);
    server_addr->sin_addr.s_addr = INADDR_ANY;

    // reuso del puerto
    int one = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    // Enlazamos el socket y hacemos que escuche en el puerto deseado
    if (bind(server_sock, (struct sockaddr *)server_addr, sizeof(*server_addr)) < 0)
    {
        perror("[-] Error binding");
        close(server_sock);
        return -1;
    }
    return 0;
}

/*
 * Función para escuchar y aceptar conexiones de clientes
 Es importante que client_sock y client_addr sean paso por referencia porque queremos que en el código original sí se modifiquen con
 el entero representación del socket cliente y su dirección respectivamente
 */
int escucharAceptarServidor(int server_sock, int puertoServer, int *client_sock, struct sockaddr_in *client_addr)
{
    // Estructura para el tamaño de la dirección
    socklen_t addr_size;

    // Ponemos al socket a escuchar en el puerto deseado
    if (listen(server_sock, 1) < 0)
    {
        perror("[-] Error on listen");
        close(server_sock);
        return -1;
    }
    printf("[+] Server listening port %d...\n", puertoServer);

    // Bloqueamos esperando a que un cliente se conecte
    addr_size = sizeof(*client_addr);
    *client_sock = accept(server_sock, (struct sockaddr *)client_addr, &addr_size);
    if (*client_sock < 0)
    {
        perror("[-] Error on accept");
        close(server_sock);
        return -1;
    }
    // printf("[+] Client connected\n"); Comento lo de client connected ahorita que ya tenemos 3 servidores
}

/**
 * Función que maneja la conexión inicial con el cliente y la asignación de puerto para transferencia
 */
int manejadorCliente(int client_sock, int server_sock, int puertoServer)
{
    // Buffer para transferencia de datos
    char buffer[BUFFER_SIZE] = {0};
    // Puerto que le vamos a dar al cliente par que se conecte
    int puertoClient = ++puertoServer;

    // Formateamos a cadena el puerto y se lo enviamos
    char mensaje[BUFFER_SIZE];
    sprintf(mensaje, "%d", puertoClient);
    send(client_sock, mensaje, strlen(mensaje), 0);

    close(client_sock);
    // close(server_sock);
    return 0;
}

/**
 * Función que contiene todo el programa del servidor.
 */
void *

programaServidor(void *arg)
{
    // Casteamos los argumentos del tipo genérico al array de argumentos
    char *puertoChar = (char *)arg;
    int puertoServer = atoi(puertoChar);

    // enteros representación de los sockets
    int server_sock, client_sock;
    // Estructura definición de la dirección de los sockets
    struct sockaddr_in server_addr, client_addr;

    // Crear el socket IPv4 TCP:
    if ((server_sock = crearSocketIPv4TCP()) < 0)
        return NULL;

    // Configuramos el socket pasando la dirección de memoria donde guardamos la configuración
    if (configurarEnlazarSocket(&server_addr, puertoServer, server_sock) < 0)
        return NULL;

    // Ponemos al socket a escuchar en el puerto deseado
    if (escucharAceptarServidor(server_sock, puertoServer, &client_sock, &client_addr) < 0)
        return NULL;

    // Manejador del cliente
    if (manejadorCliente(client_sock, server_sock, puertoServer) < 0)
        return NULL;
}

/**
 * Función que maneja la conexión para transferencia de información.
 */
int manejadorClienteTrans(int client_sock, int server_sock, int puertoServer)
{

    // Le enviiamos nuestro estado al cliente
    char *mensaje = "SERVER WAITING";
    send(client_sock, mensaje, strlen(mensaje), 0);

    // Buffer para transferencia de datos
    char buffer[BUFFER_SIZE] = {0};
    // Puerto al que quiere conectarse el cliente
    int puertoClient;
    // Nombre del archivo que se recibirá
    char fileName[BUFFER_SIZE / 2];

    // recibimos nombre del archivo
    int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);

    buffer[bytes] = '\0';
    sscanf(buffer, "%s", fileName); // extrae nombre del archivo

    send(client_sock, "REQUEST ACCEPTED", strlen("REQUEST ACCEPTED"), 0);

    // Eliminamos saltos de línea y retornos de carro del nombre del archivo, reemplazándolos por el caracter nulo
    fileName[strcspn(fileName, "\r\n")] = '\0';

    // Abrimos el archivo que vamos a guardar encriptado
    FILE *fp = fopen(fileName, "w");
    if (fp == NULL)
    {
        perror("[-] Error to open the file");
        close(client_sock);
        return -1;
    }

    // Recibimos el archivo en bloques por medio del buffer
    while ((bytes = recv(client_sock, buffer, sizeof(buffer), 0)) > 0)
    {

        fwrite(buffer, 1, bytes, fp);
    }

    // Avisamos que el archivo fue cifrado
    send(client_sock, "FILE RECEIVED", strlen("FILE RECEIVED"), 0);
    printf("[+][Server %d] File received:\n", puertoServer);

    fclose(fp);
    // Imprimir el archivo cifrado
    imprimirArchivo(fileName);

    close(client_sock);
    close(server_sock);
    return 0;
}

void *
programaServidorLogistico(void *arg)
{
    // Casteamos los argumentos del tipo genérico al array de argumentos
    int puertoServer = *((int *)arg);

    // enteros representación de los sockets
    int server_sock, client_sock;
    // Estructura definición de la dirección de los sockets
    struct sockaddr_in server_addr, client_addr;

    // Crear el socket IPv4 TCP:
    if ((server_sock = crearSocketIPv4TCP()) < 0)
        return NULL;

    // Configuramos el socket pasando la dirección de memoria donde guardamos la configuración
    if (configurarEnlazarSocket(&server_addr, puertoServer, server_sock) < 0)
        return NULL;

    // Ponemos al socket a escuchar en el puerto deseado
    if (escucharAceptarServidor(server_sock, puertoServer, &client_sock, &client_addr) < 0)
        return NULL;

    // Manejador del cliente
    if (manejadorClienteTrans(client_sock, server_sock, puertoServer) < 0)
        return NULL;
}

int main(int argc, char *argv[])
{

    /*
        if (argc != 2)
        {
            printf("Type: %s <port1> \n", argv[0]);
            return 1;
        }
    */

    // Funcionalidad para el manejo de 3 clientes
    pthread_t t1, t2;
    // pthread_t t3;

    // El argumento del programa es un puerto para el servidor
    char *puerto1 = argv[1];
    int puerto2 = atoi(argv[1]);
    puerto2++; // puerto del servidor logístico

    // Creamos los hilos con la función del programa y el puerto correspondiente
    pthread_create(&t1, NULL, programaServidor, puerto1);

    pthread_create(&t2, NULL, programaServidorLogistico, &puerto2);
    // Hacemos que el hilo main espere a que terminen los demás hilos
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    return 0;
}
