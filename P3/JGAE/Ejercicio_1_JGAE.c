#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

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

// Función para convertir a minúsculas
void toLowerCase(char *str)
{
    for (int i = 0; str[i]; i++)
        str[i] = tolower((unsigned char)str[i]);
}

// Función para eliminar espacios al inicio y final
void trim(char *str)
{
    char *end;
    while (isspace((unsigned char)*str))
        str++; // inicio
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end))
        end--; // final
    *(end + 1) = '\0';
}

bool isOnFile(const char *bufferOriginal)
{
    FILE *fp;
    char line[BUFFER_SIZE];
    char buffer[BUFFER_SIZE];
    bool foundWorld = false;
    // Copiamos el buffer original para poder modificarlo
    strncpy(buffer, bufferOriginal, BUFFER_SIZE);
    buffer[BUFFER_SIZE - 1] = '\0'; // aseguramos terminación
    trim(buffer);
    toLowerCase(buffer);
    fp = fopen("cipherworlds.txt", "r");
    if (fp == NULL)
    {
        printf("[-]Error opening file!\n");
        return false;
    }
    while (fgets(line, sizeof(line), fp) != NULL)
    {
        // Obtener el tamaño de la cadena hasta antes del salto de línea
        // Luego con ese número sabe dónde está el salto de línea y reemplaza el salto de línea por
        // caracter del final de una cadena
        line[strcspn(line, "\n")] = '\0';
        // Elimina espacios en blanco al inicio y al final
        trim(line);
        // Convierte a minúsculas
        toLowerCase(line);
        // Compara la línea del buffer con la línea que está leyendo del archivo
        if (strcmp(line, buffer) == 0)
        {
            // Si encuentra la palabra anota que la encontró
            foundWorld = true;
            break;
        }
    }
    // cierra el archivo
    fclose(fp);
    // Devuelve si la encontró o no
    return foundWorld;
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

int main(int argc, char *argv[])
{
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size;
    char buffer[BUFFER_SIZE] = {0};
    int puertoClient;
    int shift;
    int puertoServer;
    char fileName[BUFFER_SIZE / 2];
    char fileComplete[BUFFER_SIZE];

    if (argc != 2)
    {
        printf("Type: %s <port> \n", argv[0]);
        return 1;
    }

    puertoServer = atoi(argv[1]);

    // Crear el socket:
    // AF_INET indica que se utilizará IPv4
    // SOCK_STREAM indica que se utilizará TCP
    // 0 indica que se utilizará el protocolo predeterminado (TCP en este caso)
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1)
    {
        perror("[-] Error to create the socket");
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(puertoServer);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("[-] Error binding");
        close(server_sock);
        return 1;
    }

    if (listen(server_sock, 1) < 0)
    {
        perror("[-] Error on listen");
        close(server_sock);
        return 1;
    }

    printf("[+] Server listening port %d...\n", puertoServer);

    addr_size = sizeof(client_addr);
    client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_size);
    if (client_sock < 0)
    {
        perror("[-] Error on accept");
        close(server_sock);
        return 1;
    }
    printf("[+] Client connected\n");

    int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes <= 0)
    {
        printf("[-] Missed port or shift\n");
        close(client_sock);
        close(server_sock);
        return 1;
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
            return 1;
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
            return 1;
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
    return 0;
}
