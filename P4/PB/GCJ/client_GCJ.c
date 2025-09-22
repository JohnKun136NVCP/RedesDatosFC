#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <netdb.h>
#include <time.h>
#include <errno.h>
#define BUFFER_SIZE 512


// Estructura de datos para los argumentos a enviar.
struct argumentos_envio
{
    const char *alias;
    int puerto;
    const char *filename;
    char *shift;
};

// Revisamos si existe el archivo que deseamos enviar.
int check_file (const char *archivo_a_revisar){
    FILE *archivo = fopen(archivo_a_revisar, "r"); 
    if(archivo != NULL){
        fclose(archivo); 
        return 1;
    }
    return 0;
}

/** Función que nos permite hacer el log de los mensajes. */
void logger(char *mensaje, int longitud){
    time_t ahora;
    struct tm *local;
    char mensajeCompleto[BUFFER_SIZE];
    time(&ahora);
    local = localtime(&ahora);
    strftime(mensajeCompleto, sizeof(mensajeCompleto), "[%Y-%m-%d %H:%M:%S]: ", local);
    strncat(mensajeCompleto, mensaje, sizeof(mensajeCompleto) - longitud - 1); 
    mensajeCompleto[sizeof(mensajeCompleto) - 1] = '\0'; 
    FILE *archivo_log = fopen("clientLog.txt", "a"); 
    if (archivo_log != NULL){
        fprintf(archivo_log, "%s\n", mensajeCompleto);
        fclose(archivo_log);
    }else {
        perror("[-] Couldn't open log file");
    }
}

// Nos permite conectarnos a un servidor. Regresa el entero que representa al puerto.
int connect_to_server(int puerto, const char *server_alias){
    struct addrinfo hints, *res, *p; //Estructuras de datos para la información del alias.
    int client_sock_logistico;

    memset(&hints, 0, sizeof(hints));
    // Se configura el socket de la misma manera.
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    char puerto_str[6];
    snprintf(puerto_str, sizeof(puerto_str), "%d", puerto);

    int status = getaddrinfo(server_alias, puerto_str, &hints, &res); // Obtenemos la información del alias, se llenan las estructuras hints,res y el string del puerto.
    if (status != 0) {
        // En caso de resolver la información, se termina la ejecución.
        printf("[-] getaddrinfo error: %s\n", gai_strerror(status));
        return 1;
    }

    client_sock_logistico = -1;
    // Revisamos todos los resultados que se obtuvieron del alias.
    for (p = res; p != NULL; p = p->ai_next){
        client_sock_logistico = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if(client_sock_logistico == -1) {
            perror("[-] socket creation failed");
            continue;
        }

        // Nos conectamos a la primer dirección IP válida.
        if(connect(client_sock_logistico, p->ai_addr, p->ai_addrlen) == 0){
            printf("[+] Connected to %s:%d\n", server_alias, puerto);
            break;
        } else {
            perror("[-] connect failed");
            close(client_sock_logistico);
            client_sock_logistico = -1;
        }
    }

    // En caso de no encontrar ninguna conexión válida, terminamos la ejecución.
    if(client_sock_logistico == -1){
        printf("[*] SERVER RESPONSE %d : REJECTED \n", puerto);
        char mensaje_error[128]; 
        snprintf(mensaje_error, sizeof(mensaje_error), 
                 "Couldn't establish connection with %s in port %d\n", server_alias, puerto);
        logger(mensaje_error, strlen(mensaje_error));
        freeaddrinfo(res);
        return 1;
    }
    freeaddrinfo(res);

    char buffer_nuevo_puerto[BUFFER_SIZE] = {0};
    int bytes_np = recv(client_sock_logistico, buffer_nuevo_puerto, sizeof(buffer_nuevo_puerto) - 1, 0);// Recibimos el buffer que contiene la nueva dirección.
    if(bytes_np <= 0){
        perror("[-] recv failed for new port");
        char mensaje_error[] = "Couldn't get new communication port";
        logger(mensaje_error, strlen(mensaje_error));
        close(client_sock_logistico);
        return 1;
    }
    buffer_nuevo_puerto[bytes_np] = '\0';

    int puerto_nuevo;
    // Extraemos el puerto de los datos enviados por el servidor.
    if(sscanf(buffer_nuevo_puerto, "%d", &puerto_nuevo) != 1 || puerto_nuevo <= 0){
        printf("[-] Invalid port received: %s\n", buffer_nuevo_puerto);
        char mensaje_error[] = "Connection port couldn't be parsed correctly"; 
        logger(mensaje_error, strlen(mensaje_error));
        close(client_sock_logistico);
        return 1;
    }

    char mensaje_exito[100];  
    snprintf(mensaje_exito, sizeof(mensaje_exito), "Received communication port %d", puerto_nuevo);
    logger(mensaje_exito, strlen(mensaje_exito));
    close(client_sock_logistico); // Se cierra el socket actual.
    struct addrinfo hints_comunicacion, *res_comunicacion;
    memset(&hints_comunicacion, 0, sizeof(hints_comunicacion));
    // Configuramos el nuevo puerto.
    hints_comunicacion.ai_family = AF_INET;
    hints_comunicacion.ai_socktype = SOCK_STREAM;

    char puerto_nuevo_str[6];
    snprintf(puerto_nuevo_str, sizeof(puerto_nuevo_str), "%d", puerto_nuevo);

    // Obtenemos nuevamente la IP pero ahora utilizamos el nuevo puerto asignado.
    if(getaddrinfo(server_alias, puerto_nuevo_str, &hints_comunicacion, &res_comunicacion) != 0){
        // En caso de error terminamos la ejecución.
        perror("[-] getaddrinfo failed for new port");
        char mensaje_error[] = "Couldn't get new port information";
        logger(mensaje_error, strlen(mensaje_error));
        return 1;
    }

    int client_sock_comunication = -1;
    // Nuevamente intentamos conectarnos con todos los parámetros que se resolvieron de nuestro alias.
    for(p = res_comunicacion; p != NULL; p = p->ai_next){
        client_sock_comunication = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if(client_sock_comunication == -1){
            perror("[-] socket creation failed for new port");
            continue;
        }
        // Nos conectamos de nuevo, pero ahora en el puerto de datos.
        if(connect(client_sock_comunication, p->ai_addr, p->ai_addrlen) == 0){
            printf("[+] Reconnected to server on new port %d\n", puerto_nuevo);
            break;
        } else {
            perror("[-] connect failed for new port");
            close(client_sock_comunication);
            client_sock_comunication = -1;
        }
    }

    freeaddrinfo(res_comunicacion);
    
    // Nuevamente, en caso de encontrar una conexción válida, terminamos la ejecución.
    if(client_sock_comunication == -1){
        char mensaje_error[] = "Couldn't connect to new port";
        logger(mensaje_error, strlen(mensaje_error));
        return -1;
    }

    return client_sock_comunication; 
}

// Nos permite hacer el envío del archivo y procesar la respuesta del servidor.
void send_file_shift(const char *filename, int sockfd, char *shift, int puerto){
    char buffer[BUFFER_SIZE];
    char mensaje[BUFFER_SIZE];
    char shift_buffer[BUFFER_SIZE];
    FILE *fp = fopen(filename, "r");
    if (fp == NULL)
    {
        perror("[-] Cannot open the file\n");
        return;
    }

    snprintf(mensaje, BUFFER_SIZE, "%s %s", shift, filename);
    send(sockfd, mensaje, strlen(mensaje), 0);

    char buffer_conf[BUFFER_SIZE] = {0};
    int bytes_conf = recv(sockfd, buffer_conf, sizeof(buffer_conf) - 1, 0);
    if (bytes_conf > 0)
    {
        buffer_conf[bytes_conf] = '\0';
        printf("[*] SERVER RESPONSE %d : %s\n", puerto, buffer_conf);
    }

    size_t bytes;

    while ((bytes = fread(buffer, 1, sizeof(buffer), fp)) > 0)
    {
        if (send(sockfd, buffer, bytes, 0) == -1)
        {
            perror("[-] Error sending the file");
            fclose(fp);
            return;
        }
    }
    fclose(fp);


    shutdown(sockfd, SHUT_WR);

    char buffer_respuesta[BUFFER_SIZE] = {0};
    int bytes_recv = recv(sockfd, buffer_respuesta, sizeof(buffer_respuesta) - 1, 0);
    if (bytes_recv > 0)
    {
        buffer_respuesta[bytes_recv] = '\0';
        if (strstr(buffer_respuesta, "File recieved and encrypted\n") != NULL)
        {
            printf("[*] SERVER RESPONSE %d : File recived and encrypted\n", puerto);
        }
        else
        {
            perror("[-] Couldn't recieve server response \n");
        }
    }
    else
    {
        printf("[-] Connection timeout\n");
    }
}

// Wrapper de función para hacer el envío de manera paralela.
void *send_file_thread(void *argumentos){
    struct argumentos_envio* argumentos_struct = (struct argumentos_envio *) argumentos;
    if (check_file(argumentos_struct->filename) == 0){
        pthread_exit(NULL);
    }
    int conexion = connect_to_server(argumentos_struct->puerto, argumentos_struct->alias);
    if (conexion == 1){
        pthread_exit(NULL);
    }
    send_file_shift(argumentos_struct->filename, conexion, argumentos_struct->shift, argumentos_struct->puerto);
    close(conexion);
    pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        printf("Type: <ALIAS_IP> <PUERTO> <ARCHIVO>\n");
        return 1;
    }
    const char *archivo = argv[argc - 1];
    int puerto = atoi(argv[argc - 2]);
    char *interfaz = argv[1]; 
    if (check_file(archivo) == 0){
        perror("[-] Couldn't open specified file.\n"); 
        return 1;
    }
    int n = 3; //Tamaño máximo de alias
    char **alias = malloc(n * sizeof(char*)); //Lista de alias.
    //Alias a donde se enviará la información.
    if (strcmp(interfaz, "s01") == 0) {
        alias[0] = "s02"; 
        alias[1] = "s03"; 
        alias[2] = "s04"; 
    } else if (strcmp(interfaz, "s02") == 0) {
        alias[0] = "s01"; 
        alias[1] = "s03"; 
        alias[2] = "s04"; 
    } else if (strcmp(interfaz, "s03") == 0) {
        alias[0] = "s02"; 
        alias[1] = "s01"; 
        alias[2] = "s04"; 
    } else if (strcmp(interfaz, "s04") == 0){
        alias[0] = "s02"; 
        alias[1] = "s03"; 
        alias[2] = "s01"; 
    }
    else {
        printf("Type: <INTERFAZ_IP>\n");
        return 1;
    }

    char *shift = "20";
    pthread_t hilos[3]; 
    struct argumentos_envio argumentos[3];
    for (int i = 0; i < 3; i++){
        argumentos[i].alias = alias[i];
        argumentos[i].puerto = puerto;
        argumentos[i].filename = archivo;
        argumentos[i].shift = shift;
    }

    for (int i = 0; i < 3; i++){
        pthread_create(&hilos[i], NULL, send_file_thread, &argumentos[i]);
    }

    for (int i = 0; i < 3; i++){
        pthread_join(hilos[i], NULL);
    }

    // Liberamos el espacio de los alias.
    free(alias); 

    return 0;
}
