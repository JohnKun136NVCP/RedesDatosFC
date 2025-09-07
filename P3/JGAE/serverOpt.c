#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#include <pthread.h>

#define BUFFER_SIZE 1024 // Tama~no del buffer para recibir datos

/**
 * Función de cifrado y descifrado de César
 * @param text EL texto a cifrar o descifrar
 * @param shift el desplazamiento que se hará
 * @param cifrar 1 para cifrar y 0 para descifrar
 *
 * Esta función mezcla el cifrado y descifrado para evitar código redundante, lo único que hace es cambiar el signo del desplazamiento
 */
void caesar(char *text, int bytes, int shift, int cifrar)
{
    // Si vamos a cifrar el desplazamiento es positivo, en caso contrario es negativo,
    // entonces uso el operador ternario ? que hace justamente eso
    int desplazamiento = cifrar ? shift : -shift;
    desplazamiento = desplazamiento % 26;
    for (int i = 0; i < bytes; i++)
    {
        char c = text[i];
        if (isupper(c))
        {
            // Al restar la 'A' obtenemos el índice en el alfabeto
            // Luego aplicamos el desplazamiento (ya sea positivo o negativo) con el módulo 26
            // para saber el caracter que le toca aún cuando da una vuelta completa o más
            text[i] = ((c - 'A' + desplazamiento + 26) % 26) + 'A';
        }
        else if (islower(c))
        {
            // Lo mismo que el caso anterior pero ahora con minúsculas
            text[i] = ((c - 'a' + desplazamiento + 26) % 26) + 'a';
        }
        // Todos los demás los dejamos igual
    }
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

/**
 * Función que contiene todo el programa del servidor.
 */
void *manejadorCliente(void *arg)
{
    // Casteamos los argumentos del tipo genérico al array de argumentos
    char *puertoChar = (char *)arg;
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size;
    char buffer[BUFFER_SIZE] = {0};
    int puertoClient;
    int shift;
    int puertoServer;
    char fileName[BUFFER_SIZE / 2];
    char fileComplete[BUFFER_SIZE];

    puertoServer = atoi(puertoChar);

    // Crear el socket IPv4 TCP:
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1)
    {
        perror("[-] Error to create the socket");
        return NULL;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(puertoServer);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("[-] Error binding");
        close(server_sock);
        return NULL;
    }

    if (listen(server_sock, 1) < 0)
    {
        perror("[-] Error on listen");
        close(server_sock);
        return NULL;
    }

    printf("[+] Server listening port %d...\n", puertoServer);

    addr_size = sizeof(client_addr);
    client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_size);
    if (client_sock < 0)
    {
        perror("[-] Error on accept");
        close(server_sock);
        return NULL;
    }
    printf("[+] Client connected\n");

    int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes <= 0)
    {
        printf("[-] Missed port or shift\n");
        close(client_sock);
        close(server_sock);
        return NULL;
    }
    buffer[bytes] = '\0';

    sscanf(buffer, "%d %d", &puertoClient, &shift); // extrae puerto y desplazamiento

    // Validamos que el puerto recibido sea igual al puerto de mi socket
    if (puertoClient == puertoServer)
    {
        // Si el puerto es válido avisamos que está autorizado
        printf("[+][Server %d] Request accepted... %d\n", puertoServer, puertoClient);
        send(client_sock, "REQUEST ACCEPTED", strlen("REQUEST ACCEPTED"), 0);

        // recibimos el nombre del archivo
        bytes = recv(client_sock, fileName, sizeof(fileName) - 1, 0);
        if (bytes <= 0)
        {
            printf("[-] Error receiving file name\n");
            close(client_sock);
            close(server_sock);
            return NULL;
        }
        // caracter nulo para que el final del nombre del archivo sea correcto
        fileName[bytes] = '\0';

        // Enviamos confirmación de recibido del nombre del archivo
        send(client_sock, "FILENAME RECEIVED", strlen("FILENAME RECEIVED"), 0);

        // Formateamos el nombre del archivo agregando la leyenda encrypted_
        snprintf(fileComplete, BUFFER_SIZE, "encrypted_%s", fileName);
        // Eliminamos saltos de línea y retornos de carro del nombre del archivo, reemplazándolos por el caracter nulo
        fileName[strcspn(fileName, "\r\n")] = '\0';

        // Abrimos el archivo que vamos a guardar encriptado
        FILE *fp = fopen(fileComplete, "w");
        if (fp == NULL)
        {
            perror("[-] Error to open the file");
            close(client_sock);
            return NULL;
        }

        // Recibimos el archivo en bloques por medio del buffer
        while ((bytes = recv(client_sock, buffer, sizeof(buffer), 0)) > 0)
        {
            // cada bloque se cifra y se escribe en el archivo
            caesar(buffer, bytes, shift, 1);
            fwrite(buffer, 1, bytes, fp);
        }

        // Avisamos que el archivo fue cifrado
        send(client_sock, "FILE RECEIVED AND ENCRYPTED", strlen("FILE RECEIVED AND ENCRYPTED"), 0);
        printf("[+][Server %d] File received and encrypted:\n", puertoServer);

        fclose(fp);
        // Imprimir el archivo cifrado
        imprimirArchivo(fileComplete);
    }
    else
    {
        // si el puerto no es entonces mandamos REJECTED
        send(client_sock, "REJECTED", strlen("REJECTED"), 0);
        printf("[+][Server %d] Request rejected (Client requested port %d)\n", puertoServer, puertoClient);
    }

    close(client_sock);
    close(server_sock);
    return NULL;
}

int main(int argc, char *argv[])
{

    if (argc != 2)
    {
        printf("Type: %s <port> \n", argv[0]);
        return 1;
    }

    // Función para manejar múltiples clientes
    pthread_t t1;

    char *puerto1 = argv[1];

    pthread_create(&t1, NULL, manejadorCliente, puerto1);

    pthread_join(t1, NULL);
    return 0;
}
