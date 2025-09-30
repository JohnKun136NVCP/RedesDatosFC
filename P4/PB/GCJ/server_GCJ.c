#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <sys/utsname.h>
#include <sys/statvfs.h>
#include <utmp.h>
#include <pwd.h>
#include <sys/types.h>
#include <pthread.h>
#include <dirent.h>
#include <time.h>
#define BUFFER_SIZE 512

struct argumentos_servidor
{
    int puerto;
    const char *ip;
    const char *interfaz;
    int i;
};


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
    FILE *archivo_log = fopen("serverLog.txt", "a"); 
    if (archivo_log != NULL){
        fprintf(archivo_log, "%s\n", mensajeCompleto);
        fclose(archivo_log);
    }else {
        perror("[-] Couldn't open log file");
    }
}

// Nos permite realizar el cifrado César
void encryptCesar(char *text, int shift){
    shift = shift % 26;
    for (int i = 0; text[i] !='\0'; i++) {
        char c = text[i];
	if (isupper(c)){
           text[i] = ((c - 'A' + shift) % 26) + 'A'; 
        } else if(islower(c)) {
           text[i] = ((c - 'a' + shift) % 26) + 'a';
        } else {
            text[i] = c;
        }
    }
}

// Guardamos los contenidos del buffer en un archivo.
void save_file(char *buffer_encriptado, const char *filename){
    printf("[*] Saving file on %s\n", filename);
    FILE *archivo_encriptado;
    archivo_encriptado = fopen(filename, "w");
    if (archivo_encriptado == NULL)
    {
        perror("[-] Error trying to open the file\n");
        return;
    }
    fwrite(buffer_encriptado, 1, strlen(buffer_encriptado), archivo_encriptado); 
    fclose(archivo_encriptado);
}

// Recibimos el shift, mensaje y encripta los mensajes recibidos.
void recieve_encrypt_buffer(char *archivo, int client_sock, int server_sock, char *interfaz, int id_proceso){
    char buffer_recibido[BUFFER_SIZE] = {0};
    char wait_message[] = "Server waiting for file"; 
    logger(wait_message, strlen(wait_message));
    int bytes = recv(client_sock, buffer_recibido, BUFFER_SIZE, 0);
    if (bytes <= 0)
    {
        printf("[-] Error receiving message\n");
        char error_message[] = "Couldn't recieve client message";
        logger(error_message, strlen(error_message));
        close(client_sock);
        close(server_sock);
        return;
    }

    buffer_recibido[bytes] = '\0';
    char shift[256];
    char nombreArchivo[512];
    sscanf(buffer_recibido, "%s %s", shift, nombreArchivo);
    int shift_int = atoi(shift);
    send(client_sock, "Shift and filename received\n", 28, 0);
    char shift_recieved[] = "Shift and filename recieved";
    logger(shift_recieved, strlen(shift_recieved));
    char *dot = strrchr(nombreArchivo, '.');  
    if (dot && strcmp(dot, ".txt") == 0) {
        *dot = '\0';
    }
    archivo[0] = '\0';
    while ((bytes = recv(client_sock, buffer_recibido, BUFFER_SIZE, 0)) > 0)
    {
        buffer_recibido[bytes] = '\0';
        strcat(archivo, buffer_recibido);
    }

    encryptCesar(archivo, shift_int);
    char *respuesta = "File recieved and encrypted\n";
    char file_recieved[] = "File recieved successfully";
    logger(file_recieved, strlen(file_recieved));
    if (send(client_sock, respuesta, strlen(respuesta), 0) == -1)
    {
        perror("[-] Error sending recieved confirmation\n");
        char error_confirmation[] = "Couldn't sent recieved confirmation"; 
        logger(error_confirmation, strlen(error_confirmation)); 
        close(client_sock);
        close(server_sock);
        return;
    }
    printf("[*]Response sent successfully\n");
    strncat(nombreArchivo, interfaz, strlen(interfaz));
    char numero_proceso[2]; 
    snprintf(numero_proceso, 2, "%d", id_proceso);
    strncat(nombreArchivo, numero_proceso, strlen(numero_proceso)); 
    char *terminacion = ".txt";
    strncat(nombreArchivo, terminacion, strlen(terminacion));
    save_file(archivo, nombreArchivo);
    printf("[*] File saved in location %s\n", nombreArchivo);
    char file_saved[100];
    snprintf(file_saved, sizeof(file_saved), "File saved on location %s", nombreArchivo);
    logger(file_saved, strlen(file_saved));
}

// Hacemos la creación del socket para escuchar en un puerto, junto con el binding y el escucha.
int create_sock_server(const char *ip, int puerto){
    int server_sock;
    struct sockaddr_in server_addr;
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if(server_sock == -1){
        perror("[-] Couldn't create server socket");
        return 1;
    }
    server_addr.sin_family = AF_INET; 
    server_addr.sin_port = htons(puerto); 

    // Escuchamos en la IP pasada como parámetro.
    if (inet_pton(AF_INET, ip, &server_addr.sin_addr) <= 0) {
        perror("[-] Invalid IP address");
        close(server_sock);
        return 1;
    }

    if (bind(server_sock, (struct sockaddr *)&server_addr,sizeof(server_addr)) < 0) {
        perror("[-] Error binding");
        close(server_sock);
        return 1;
    }
    if (listen(server_sock, 1) < 0) {
        perror("[-] Error on listen");
        close(server_sock);
        return 1;
    }
    printf("[+] Server listening port %d...\n", puerto);
    char mensaje_escucha[100];
    snprintf(mensaje_escucha, sizeof(mensaje_escucha), "Server listening on port %d", puerto);
    logger(mensaje_escucha, strlen(mensaje_escucha));
    return server_sock;
}

/** Función que nos permite hacer el cambio de puerto para recibir la información del cliente. */
int switch_port(int puerto_actual, int client_sock, const char *ip){
    struct sockaddr_in puerto_actual_sock;
    socklen_t tamano_puerto = sizeof(puerto_actual_sock); 
    if (getsockname(puerto_actual, (struct sockaddr*)&puerto_actual_sock, &tamano_puerto)){
        perror("[-] Couldn't get current socket name"); 
        return 1; 
    }
    int puerto_actual_nombre = ntohs(puerto_actual_sock.sin_port); 
    int puerto_siguiente_nombre = puerto_actual_nombre + 1; 

    // Se crea el socket
    int puerto_siguiente = create_sock_server(ip, puerto_siguiente_nombre);
    if (puerto_siguiente == 1) {
        return 1;
    }

    //Se empieza a escuchar una vez que se ha creado el socket de manera exitosa.
    char puerto_siguiente_str[42]; 
    snprintf(puerto_siguiente_str, sizeof(puerto_siguiente_str), "%d", puerto_siguiente_nombre);
    if (send(client_sock, puerto_siguiente_str, strlen(puerto_siguiente_str), 0) == -1) {
        perror("[-] Error sending next port");
    }

    //Enviamos un mensaje indicando que ya no se puede escribir al puerto.
    shutdown(client_sock, SHUT_WR);

    close(puerto_actual); 
    close(client_sock);
    char mensaje_cerrado[100]; 
    snprintf(mensaje_cerrado, sizeof(mensaje_cerrado), 
             "Port %d closed for comunication", puerto_actual_nombre); 
    logger(mensaje_cerrado, strlen(mensaje_cerrado));

    char mensaje_abierto[100]; 
    snprintf(mensaje_abierto, sizeof(mensaje_abierto),
             "Port %d created for file exchange", puerto_siguiente_nombre);
    logger(mensaje_abierto, strlen(mensaje_abierto));

    return puerto_siguiente;
}

// Nos permite hacer manejo del cliente.
void *recieve_file_thread(void *arg){
    struct argumentos_servidor *argumentos = (struct argumentos_servidor *) arg; //Obtenemos los argumentos del hilo.
    int server_sock_logistico = create_sock_server(argumentos->ip, argumentos->puerto); //Creamos el nuevo socket en base a los argumentos.
    if (server_sock_logistico == 1){
        pthread_exit(NULL);
    }
    int client_sock_logistico; 
    struct sockaddr_in client_addr_logistico;
    socklen_t addr_size;
    addr_size = sizeof(client_addr_logistico); 
    client_sock_logistico = accept(server_sock_logistico, (struct sockaddr *)&client_addr_logistico, &addr_size); //Aceptamos la conexión del cliente.
    if(client_sock_logistico < 0){
        perror("[-] Error accepting connection");
        close(server_sock_logistico);
        pthread_exit(NULL);
    }
    int server_sock_comunicacion = switch_port(server_sock_logistico, client_sock_logistico, argumentos->ip); //Realizamos el cambio de puerto al puerto de datos.
    if (server_sock_comunicacion == 1){
        pthread_exit(NULL); 
    }
    int client_sock_comunicacion;
    struct sockaddr_in client_addr_comunicacion;
    addr_size = sizeof(client_addr_comunicacion); 
    client_sock_comunicacion = accept(server_sock_comunicacion, (struct sockaddr *)&client_addr_comunicacion, &addr_size); //Aceptamos la conexión del cliente por el puerto de datos.
    if(client_sock_comunicacion < 0){
        perror("[-] Error accepting connection");
        close(client_sock_logistico);
        pthread_exit(NULL);
    }
    // El resto se ejecuta de la misma manera que en la práctica anterior.
    char archivo_a_recibir[BUFFER_SIZE] = {0};
    recieve_encrypt_buffer(archivo_a_recibir, client_sock_comunicacion, server_sock_comunicacion, argumentos->interfaz, argumentos->i);
    close(client_sock_comunicacion);
    close(server_sock_comunicacion);
    pthread_exit(NULL);
}


int main(int argc, char *argv[])
{
    if (argc != 2){
        printf("Type: <INTERFAZ_IP>\n");
        return 1;
    }
    char *interfaz = argv[1];
    char *ip;
    int puerto = 4200;
    // Configuración de IPs, se puede modificar.
    if (strcmp(interfaz, "s01") == 0) {
        ip = "192.168.201.101"; 
    } else if (strcmp(interfaz, "s02") == 0) {
        ip = "192.168.201.102";
    } else if (strcmp(interfaz, "s03") == 0) {
        ip = "192.168.201.104";
    } else if (strcmp(interfaz, "s04") == 0){
        ip = "192.168.201.105";
    }
    else {
        printf("Type: <INTERFAZ_IP>\n");
        return 1;
    }
    pthread_t hilos[1]; 
    struct argumentos_servidor argumentos[1] = {
        {puerto, ip, interfaz, 1} 
    };
    for (int i = 0; i < 1; i++){
        pthread_create(&hilos[i], NULL, recieve_file_thread, &argumentos[i]);
    }

    for (int i = 0; i < 1; i++){
        pthread_join(hilos[i], NULL);
    }
    return 0;
}
