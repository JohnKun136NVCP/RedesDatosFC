#define _POSIX_C_SOURCE 200112L

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
#include <netdb.h>

// Ids
#include <stdint.h>

#define BUFFER_SIZE 1024 // Tama~no del buffer para recibir datos
#define PUERTOSERVER 49200
#define MAX_CONEXIONES 5 // clientes se van a conectar, todos menos el que tiene el mismo estatus
#define QUANTUM 4        // 4 segundos va a durar el quantum
#define MAX_ALIAS 4      // Número máximo de servidores con su alias que van a estar escuchando

// Candado de exclusión mutua y variable de condición
static pthread_mutex_t EXCLUSION = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t CONDICION = PTHREAD_COND_INITIALIZER;

// Variables globales para saber a quién le toca y si hay un hilo ocupado
static volatile int turno = 0;
static volatile int hiloOcupado = -1;
static int numPendientes[MAX_ALIAS] = {0}; // clientes pendientes del servidor

// Arreglos para guardar los alias y sus IPs
static char *aliasIP[MAX_ALIAS] = {0};
// static char *aliasIP[MAX_ALIAS] = {"s01", "s02", "s03", "s04"};

static inline int next_id(int id) { return (id + 1) % MAX_ALIAS; }

void *coordinadorRR(void *arg)
{
    (void)arg;
    while (1)
    {
        pthread_mutex_lock(&EXCLUSION);

        // si alguien recibe, el coordinador "pausa"
        while (hiloOcupado != -1)
        {
            pthread_cond_wait(&CONDICION, &EXCLUSION);
        }

        // nadie recibe: si el del turno no tiene pendientes, salta inmediato
        int guard = 0;
        while (numPendientes[turno] == 0 && guard < MAX_ALIAS)
        {
            turno = next_id(turno);
            guard++;
            pthread_cond_broadcast(&CONDICION);
        }

        pthread_mutex_unlock(&EXCLUSION);

        // espera el quantum; si alguien empieza a recibir, el condvar lo reactivará
        sleep(QUANTUM);

        pthread_mutex_lock(&EXCLUSION);
        if (hiloOcupado == -1)
        {                           // nadie empezó en este quantum
            turno = next_id(turno); // pasa turno
            pthread_cond_broadcast(&CONDICION);
        }
        pthread_mutex_unlock(&EXCLUSION);
    }
    return NULL;
}

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
        perror("[+] Server cannot open the file");
        return;
    }
    char buffer[BUFFER_SIZE];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer) - 1, fp)) > 0)
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
int configurarEnlazarSocket(struct sockaddr_in *server_addr, int puertoServer, int server_sock, char *ipChar)
{

    // Configuramos el socket
    server_addr->sin_family = AF_INET;
    server_addr->sin_port = htons(puertoServer);
    server_addr->sin_addr.s_addr = inet_addr(ipChar);

    // reuso del puerto
    int one = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    // Resolvemos alias
    // Convertir puerto a cadena
    char portstr[16];
    snprintf(portstr, sizeof(portstr), "%d", puertoServer);

    struct addrinfo hints, *res = NULL, *p = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int rc = getaddrinfo(ipChar, portstr, &hints, &res);
    if (rc != 0 || !res)
    {
        fprintf(stderr, "[*] COULDNT RESOLVE %s:%d\n", ipChar, puertoServer);
        return -1;
    }

    int status = 1;
    for (p = res; p != NULL; p = p->ai_next)
    {
        // para cada uno de los resultados vamos a intentar enlazar el socket
        if (bind(server_sock, p->ai_addr, p->ai_addrlen) == 0)
        {
            // guardar la sockaddr elegida en server_addr (que es sockaddr_in)
            memcpy(server_addr, p->ai_addr, sizeof(*server_addr));
            status = 0;
            break;
        }
    }
    if (status != 0)
    {
        perror("[-] Error binding");
        close(server_sock);
        return -1;
    }
    freeaddrinfo(res);
    return status;
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
    if (listen(server_sock, MAX_CONEXIONES) < 0)
    {
        perror("[-] Error on listen");
        return -1;
    }

    // printf("[+] Server listening port %d...\n", puertoServer);

    // Bloqueamos esperando a que un cliente se conecte
    addr_size = sizeof(*client_addr);
    *client_sock = accept(server_sock, (struct sockaddr *)client_addr, &addr_size);
    if (*client_sock < 0)
    {
        perror("[-] Error on accept");
        return -1;
    }
    return 0;
}

/**
 * Función que maneja la conexión para transferencia de información.
 */
int manejadorClienteTrans(int client_sock, int server_sock, int puertoServer, char *ipChar, int id)
{

    // Esperamos nuestro turno por medio de intentar obtener un candado
    pthread_mutex_lock(&EXCLUSION);
    // Dormimos al hilo si no nos toca o si hay otro hilo recibiendo
    while (!(hiloOcupado == -1 && turno == id))
    {
        pthread_cond_wait(&CONDICION, &EXCLUSION);
    }
    // Si logra salir es porque ya es su turno y nadie está recibiendo
    // avisa que está recibiendo y suelta el candado porque ya marcó que va a recibir
    hiloOcupado = id;
    pthread_mutex_unlock(&EXCLUSION);

    // para guardar el directorio home del usuario
    const char *homedir;
    if ((homedir = getenv("HOME")) == NULL)
    {
        homedir = ".";
    }

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

    // Aquí no estaba verificando que sí hubiéramos recibidos bytes
    if (bytes <= 0)
    {
        printf("[-] Missed file name\n");
        close(client_sock);
        return -1;
    }

    buffer[bytes] = '\0';
    sscanf(buffer, "%s", fileName); // extrae nombre del archivo

    send(client_sock, "REQUEST ACCEPTED", strlen("REQUEST ACCEPTED"), 0);

    // Eliminamos saltos de línea y retornos de carro del nombre del archivo, reemplazándolos por el caracter nulo
    fileName[strcspn(fileName, "\r\n")] = '\0';

    // Construimos la ruta completa con el home
    // Construir la ruta completa: $HOME/ipChar/fileName
    char nameComplete[BUFFER_SIZE];
    snprintf(nameComplete, sizeof(nameComplete), "%s/%s/%s", homedir, ipChar, fileName);

    // Abrimos el archivo que vamos a guardar
    FILE *fp = fopen(nameComplete, "w");
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

    fclose(fp);

    // Avisamos que el archivo fue guardado
    send(client_sock, "FILE RECEIVED", strlen("FILE RECEIVED"), 0);
    printf("[+][Server %d] File received:\n", puertoServer);

    // Imprimir el archivo guardado
    imprimirArchivo(nameComplete);

    // Dejamos de cerrar el socket del servidor
    close(client_sock);

    // ya que acabamos de recibir, tomamos el candado para actualizar que ya no estamos recibiendo
    pthread_mutex_lock(&EXCLUSION);
    // Se baja uno de los pendientes a recibir
    if (numPendientes[id] > 0)
        numPendientes[id]--;
    // reiniciamos qué hilo está ocupado para decir que ya nadie
    hiloOcupado = -1;
    // actualizamos el turno
    turno = next_id(id);
    // se debe usar broadcast porque signal despierta a un solo hilo y no está fácil controlar cuál será
    // Broadcast despierta a todos los hilos que están en espera del candado
    pthread_cond_broadcast(&CONDICION);
    pthread_mutex_unlock(&EXCLUSION);

    return 0;
}

void *
programaServidorLogistico(void *arg)
{
    // Casteamos los argumentos del tipo genérico al array de argumentos
    int id = (int)(intptr_t)arg;
    char *ipChar = aliasIP[id];
    int puertoNuevo = PUERTOSERVER + 1;

    // enteros representación de los sockets
    int server_sock, client_sock;
    // Estructura definición de la dirección de los sockets
    struct sockaddr_in server_addr, client_addr;
    // Tamaño de la dirección
    socklen_t addr_size = sizeof(client_addr);

    // Crear el socket IPv4 TCP:
    if ((server_sock = crearSocketIPv4TCP()) < 0)
        return NULL;

    // Reuso del servidor
    int one = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    // Configuramos el socket pasando la dirección de memoria donde guardamos la configuración
    if (configurarEnlazarSocket(&server_addr, puertoNuevo, server_sock, ipChar) < 0)
        return NULL;

    /*
     * Cambios necesario para un ciclo de atención a varios clientess:
     * - Socket servidor se pone en modo escuchar con max conexiones
     * - Con las conexiones pendientes va a ir aceptando cada una y luego reinicial el ciclo
     */

    // Ponemos al socket en modo escuchar
    if (listen(server_sock, MAX_CONEXIONES) < 0)
    {
        perror("[-] Error on listen");
        close(server_sock);
        return NULL;
    }

    while (1)
    {
        // Bloqueamos esperando a que un cliente se conecte

        client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_size);
        if (client_sock < 0)
        {
            perror("[-] Error on accept");
            // en lugar de cerrarlo pasamos a la siguiente
            continue;
        }

        // Manejador de transferencia
        // Nuevamente en lugar de cerrar el socket del servidor, pasamos a la siguiente conexión
        if (manejadorClienteTrans(client_sock, server_sock, puertoNuevo, ipChar, id) < 0)
            continue;
    }

    // Cerramos socket servidor y finalizamos el programa
    // Este caso ya no se va a dar
    close(server_sock);
    return NULL;
}

/**
 * Función que maneja la conexión inicial con el cliente y la asignación de puerto para transferencia
 */
int manejadorCliente(int client_sock, int server_sock, int puertoServer, char *ipChar, int id)
{
    // Puerto nuevo para conexión
    int puertoClient = ++puertoServer;

    // candado de exclusión
    pthread_mutex_lock(&EXCLUSION);
    numPendientes[id]++; // Hay un cliente pendiente de transferir
    pthread_cond_broadcast(&CONDICION);
    pthread_mutex_unlock(&EXCLUSION);

    // Enviamos el puerto al que el cliente debe conectarse
    char mensaje[BUFFER_SIZE];
    sprintf(mensaje, "%d", puertoClient);
    send(client_sock, mensaje, strlen(mensaje), 0);

    // Cerramos la conexión con el cliente
    close(client_sock);
    return 0;
}

/**
 * Función que contiene todo el programa del servidor.
 */
void *
programaServidor(void *arg)
{
    int id = (int)(intptr_t)arg;
    char *ipChar = aliasIP[id];

    // enteros representación de los sockets
    int server_sock, client_sock;
    // Estructura definición de la dirección de los sockets
    struct sockaddr_in server_addr, client_addr;

    // Crear el socket IPv4 TCP:
    if ((server_sock = crearSocketIPv4TCP()) < 0)
        return NULL;

    // Configuramos el socket pasando la dirección de memoria donde guardamos la configuración
    if (configurarEnlazarSocket(&server_addr, PUERTOSERVER, server_sock, ipChar) < 0)
    {
        close(server_sock);
        return NULL;
    }

    // Creamos servidor transferencia y lo hacemos independiente
    pthread_t t;
    pthread_create(&t, NULL, programaServidorLogistico, (void *)(intptr_t)id);
    pthread_detach(t);

    // Ciclo infinito de aceptar conexiones y asignarles puerto para transferencia
    while (1)
    {
        // Ponemos al socket a escuchar en el puerto deseado
        if (escucharAceptarServidor(server_sock, PUERTOSERVER, &client_sock, &client_addr) < 0)
        {
            close(server_sock);
            return NULL;
        }

        // Manejador del cliente
        if (manejadorCliente(client_sock, server_sock, PUERTOSERVER, ipChar, id) < 0)
        {
            close(server_sock);
            return NULL;
        }
    }

    // A este tampoco va a llegar nunca por el ciclo
    // Si todo sale normal cerramos el socket y finalizamos el programa
    close(server_sock);
    return NULL;
}

int main(int argc, char *argv[])
{
    if (argc != 5)
    {
        printf("Type: %s <IP_alias_1> <IP_alias_2> <IP_alias_3> <IP_alias_4>\n", argv[0]);
        return 1;
    }

    for (int i = 0; i < MAX_ALIAS; i++)
        aliasIP[i] = argv[i + 1];

    // coordinador RR
    pthread_t tcoord;
    pthread_create(&tcoord, NULL, coordinadorRR, NULL);
    pthread_detach(tcoord);

    // hilos de servidores (pasamos id como (void*))
    pthread_t t[MAX_ALIAS];
    for (int i = 0; i < MAX_ALIAS; i++)
    {
        pthread_create(&t[i], NULL, programaServidor, (void *)(intptr_t)i);
    }
    for (int i = 0; i < MAX_ALIAS; i++)
        pthread_join(t[i], NULL);
    return 0;
}
