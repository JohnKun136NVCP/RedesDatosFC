
#define _POSIX_C_SOURCE 200112L
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <netdb.h>   // Biblioteca para llamada al sistema para resolver alias
#include <pthread.h> // Biblioteca para trabajar con hilos
#include <time.h>    // Bibiloteca para el tiempo
#include <errno.h>

#define BUFFER_SIZE 1024

struct datos_programa
{
    int *client_sock;
    struct sockaddr_in *serv_addr;
    const char *server_ip;
    int puerto;
    char *rutaArchivo;
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
int conectarServidor(struct sockaddr_in *serv_addr, int client_sock,
                     const char *server_ip, int puerto)
{
    // Inicializa estructura destino
    memset(serv_addr, 0, sizeof(*serv_addr));
    serv_addr->sin_family = AF_INET;
    serv_addr->sin_port = htons(puerto);
    serv_addr->sin_addr.s_addr = inet_addr(server_ip);

    /* Parte comentada para conexión por ip numérica
    // conexión por ip XXX.XXX.XXX.XXX
    if (connect(client_sock, (struct sockaddr *)serv_addr, sizeof(*serv_addr)) == 0)
        return 0; // éxito
    */

    // Convertir puerto a cadena
    char portstr[16];
    snprintf(portstr, sizeof(portstr), "%d", puerto);

    // La función que quiero usar para los alias es getaddrinfo para resolver el alias
    // esta estructura es para definir las preferencias de la búsqueda que haremos

    // Con hints vamos a poner las preferencias, res se van a guardar los resultados y p lo usaremos para iterar los resultados
    struct addrinfo hints, *res = NULL, *p = NULL;
    memset(&hints, 0, sizeof(hints));
    // Aunque no es necesario especificar esta configuración, es mejor porque ya automáticamente todos los resultados van a estar ipv4 y tcp, además de la dirección que habrá conseguido (si logra resolverla)
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    // obtenemos los resultados
    int rc = getaddrinfo(server_ip, portstr, &hints, &res);
    if (rc != 0 || !res)
    {
        fprintf(stderr, "[*] No se pudo resolver el alias %s:%d\n", server_ip, puerto);
        return 1;
    }

    // por defecto devolvemos un error
    int status = 1;
    for (p = res; p != NULL; p = p->ai_next)
    {
        // para cada uno de los resultados vamos a intentar conectarnos con la sockaddr
        if (connect(client_sock, p->ai_addr, p->ai_addrlen) == 0)
        {
            // Copiamos porque esta función de conexión es de paso por valor
            memcpy(serv_addr, p->ai_addr, sizeof(*serv_addr));
            status = 0; // éxito
            break;
        }
    }
    if (status != 0)
    {
        perror("connection failed");
        fprintf(stderr, "[*] CONNECTION TO SERVER %d failed\n", puerto);
    }
    freeaddrinfo(res);
    return status; // 0 = ok, 1 = error
}

/* Función para autorizar el envío del archivo al servidor y enviarlo si es autorizado respecto a la ruta y el desplazamiento
 */
int autorizarEnviarArchivo(int client_sock, int puerto, char *rutaArchivo)
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
            return 0;
        }
        else
        {
            printf("[*] SERVER RESPONSE %d: %s\n", puerto, buffer);
            return -1;
        }
    }
    else
    {
        printf("[-] Server connection timeout\n");
        return -1;
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
void guardarLog(char *mensaje)
{
    // Validación de mensaje
    if (mensaje == NULL)
    {
        printf("[-] No server message\n");
        return;
    }

    char mensajeCompleto[BUFFER_SIZE];
    time_t now = time(NULL);
    struct tm t;
    localtime_r(&now, &t);

    // strftime ya deja el caracter nulo al final
    strftime(mensajeCompleto, sizeof(mensajeCompleto), "[%Y-%m-%d %H:%M:%S] ", &t);

    // uso de size_t porque es lo que espera strncat
    // Longitud  es el tamaño que dejó poner la fecha y hora
    size_t longitud = strlen(mensajeCompleto);
    // espacio disponible es cuántos caracteres se podrán pegar después de la fecha y hora
    size_t espacioDisponible = sizeof(mensajeCompleto) - longitud - 1;

    strncat(mensajeCompleto, mensaje, espacioDisponible);
    mensajeCompleto[sizeof(mensajeCompleto) - 1] = '\0'; // asegurar final de la cadena

    // Abrimos el archivo en modo append para hasta el final poner el nuevo mensaje recibido
    FILE *file = fopen("serverMessages.txt", "a");
    if (file != NULL)
    {
        printf("[Server] %s\n", mensajeCompleto);
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
            guardarLog(buffer);

            // Hacemos la espera aleatoria 1 a 3 segundosr
            srand(time(NULL) ^ getpid());
            int delay = 1 + rand() % 3;
            sleep(delay);

            // Enviamos el archivo con lo de la práctica pasada
            return autorizarEnviarArchivo(*client_sock, puertoServer, rutaArchivo);
        }
        else
        {
            printf("[-] Server connection timeout\n");
            return -1;
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
    // Validación de argumentos
    if (datos == NULL)
    {
        printf("[-] No program arguments\n");
        return NULL;
    }
    int *client_sock = datos->client_sock;
    struct sockaddr_in *serv_addr = datos->serv_addr;
    const char *server_ip = datos->server_ip;
    int puerto = datos->puerto;
    char *rutaArchivo = datos->rutaArchivo;

    // Creamos el socket de tipo TCP ipv4
    *client_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (*client_sock == -1)
    {
        perror("[-] Error to create the socket");
        return NULL;
    }
    printf("[*] Connecting to server %s:%d\n", server_ip, puerto);
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
                close(*client_sock);
                return NULL;
            }
        }
        close(*client_sock);
        return NULL;
    }
    else
    {
        // Manejamos el error de conexión
        printf("[-] Conexion failed\n");
        close(*client_sock);
        return NULL;
    }
}

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        printf("Type: %s <server_alias> <port> <file_path>\n", argv[0]);
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

    // Por el momento solo habrá un cliente pero dejo la estructura para agregar más en el mismo programa
    pthread_t t1;

    // Creamos los hilos con la función del programa y el puerto correspondiente
    pthread_create(&t1, NULL, programaCliente, &datos1);
    // Hacemos que el hilo main espere a que terminen los demás hilos
    pthread_join(t1, NULL);

    return 1;
}
