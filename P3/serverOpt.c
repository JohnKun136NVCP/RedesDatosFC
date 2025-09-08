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
#define BUFFER_SIZE 512

struct argumentos_servidor
{
    const char *filename; 
    int puerto;
};


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
    printf("[*] Saving file...");
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
void recieve_encrypt_buffer(char *archivo, int client_sock, int server_sock){
    char shift_buffer[32] = {0};
    int bytes_shift = recv(client_sock, shift_buffer, sizeof(shift_buffer) - 1, 0);
    if (bytes_shift <= 0) {
        perror("[-] Failed to receive shift");
        close(client_sock);
        close(server_sock);
        return;
    }
    shift_buffer[bytes_shift] = '\0';
    int shift_int = atoi(strtok(shift_buffer, "\n"));
    printf("[*] Received shift: %d\n", shift_int);

    // Clear the archivo buffer
    archivo[0] = '\0';

    char buffer[BUFFER_SIZE];
    int bytes_received;
    while ((bytes_received = recv(client_sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes_received] = '\0';  // ensure null-terminated
        strcat(archivo, buffer);        // append received data
    }

    if (bytes_received < 0) {
        perror("[-] Error receiving file data");
    }

    // Encrypt the collected data
    encryptCesar(archivo, shift_int);

    const char *respuesta = "File received and encrypted\n";
    if (send(client_sock, respuesta, strlen(respuesta), 0) == -1) {
        perror("[-] Error sending confirmation");
    } else {
        printf("[*] Response sent successfully\n");
    }
}

// Hacemos la creación del socket para escuchar en un puerto, junto con el binding y el escucha.
int create_sock_server(int puerto){
    int server_sock;
    struct sockaddr_in server_addr;
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if(server_sock == -1){
        perror("[-] Couldn't create server socket");
        return 1;
    }
    server_addr.sin_family = AF_INET; 
    server_addr.sin_port = htons(puerto); 
    server_addr.sin_addr.s_addr = INADDR_ANY;
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
    return server_sock;
}

// Nos permite hacer manejo del cliente.
void *recieve_file_thread(void *arg){
    struct argumentos_servidor *argumentos = (struct argumentos_servidor *) arg; 
    int server_sock = create_sock_server(argumentos->puerto);
    if (server_sock == 1){
        pthread_exit(NULL);
    }
    int client_sock;
    struct sockaddr_in client_addr;
    socklen_t addr_size;
    addr_size = sizeof(client_addr); 
    client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_size); 
    if(client_sock < 0){
        perror("[-] Error accepting connection");
        close(server_sock);
        pthread_exit(NULL);
    }
    char buffer_prueba[2];
    int bytes_prueba = recv(client_sock, buffer_prueba, sizeof(buffer_prueba) - 1, 0); 
    if (bytes_prueba <= 0) {
        close(server_sock);
        pthread_exit(NULL);
    }
    buffer_prueba[bytes_prueba] = '\0'; 
    char archivo_a_recibir[BUFFER_SIZE] = {0};
    char *shift;
    recieve_encrypt_buffer(archivo_a_recibir, client_sock, server_sock);
    save_file(archivo_a_recibir, argumentos->filename);
    close(client_sock);
    close(server_sock);
    pthread_exit(NULL);
}


int main(int argc, char *argv[])
{
    if (argc < 2){
        printf("Type: <PUERTOS>\n");
        return 1;
    }
    int puertos[argc-1];
    for (int i = 1; i < argc ; i++){
        puertos[i] = atoi(argv[i]);
    }

    int puerto = atoi(argv[1]);
    pthread_t hilos[argc-1]; 
    struct argumentos_servidor argumentos[argc - 1];
    char buffer[50];
    for (int i = 1; i < argc; i++){
        char buffer[50];
        snprintf(buffer, sizeof(buffer), "mensaje_%d.txt", i);

        argumentos[i-1].filename = strdup(buffer); 
        argumentos[i-1].puerto   = atoi(argv[i]);
    }

    for (int i = 0; i < argc - 1; i++){
        pthread_create(&hilos[i], NULL, recieve_file_thread, &argumentos[i]);
    }

    for (int i = 0; i < 3; i++){
        pthread_join(hilos[i], NULL);
    }
    return 0;
}
