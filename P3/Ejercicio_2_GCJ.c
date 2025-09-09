#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#define BUFFER_SIZE 512


// Estructura de datos para los argumentos a enviar.
struct argumentos_envio
{
    const char *ip;
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

// Nos permite conectarnos a un servidor. Regresa el entero que representa al puerto.
int connect_to_server(int puerto, const char *server_ip){
    struct sockaddr_in serv_addr;
    int client_sock;
    client_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (client_sock == -1)
    {
        perror("[-] Error trying to create socket\n");
        return 1;
    }
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(puerto);
    serv_addr.sin_addr.s_addr = inet_addr(server_ip);
    if (connect(client_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        printf("[*] SERVER RESPONSE %d : REJECTED \n", puerto);
        close(client_sock);
        return 1;
    }
    char mensaje_prueba = 0;
    int envio = send(client_sock, &mensaje_prueba, 1, 0);
    if(envio <= 0){
        printf("[*] SERVER RESPONSE %d : REJECTED\n", puerto);
        close(client_sock);
        return 1;
    }
    printf("[+] Connected to server\n");
    return client_sock;
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
    int conexion = connect_to_server(argumentos_struct->puerto, argumentos_struct->ip);
    send_file_shift(argumentos_struct->filename, conexion, argumentos_struct->shift, argumentos_struct->puerto);
    close(conexion);
    pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
    if (argc != 5)
    {
        printf("Type: <SERVIDOR_IP> <PUERTO> <DESPLAZAMIENTO> <ARCHIVO>\n");
        return 1;
    }
    const char *archivo = argv[4];
    if (check_file(archivo) == 0){
        perror("[-] Couldn't open specified file.\n"); 
        return 1;
    }
    const char *ip = argv[1];
    int puerto = atoi(argv[2]);
    char *shift = argv[3];
    
    pthread_t hilos[3]; 
    struct argumentos_envio argumentos[3] = {
        {ip, puerto + 2, archivo, shift},  
        {ip, puerto + 1, archivo, shift},
        {ip, puerto, archivo, shift},
    };
    for (int i = 0; i < 3; i++){
        pthread_create(&hilos[i], NULL, send_file_thread, &argumentos[i]);
    }

    for (int i = 0; i < 3; i++){
        pthread_join(hilos[i], NULL);
    }

    return 0;
}
