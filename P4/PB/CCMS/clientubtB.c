#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h> //Para manejo tiempo en log
#include <netdb.h> //Resolucion para que reconozca Alias

#define BUFFER_SIZE 1024 // Tamaño del buffer para recibir datos

//funcion para mandar la informacion del archivo de texto al servidor.
void send2Server(char *server_alias, int port, int shift, char *texto) {
    int client_sock;
    //Resolucion para contectarse usando el alias con gethostbyname().
    struct hostent *he = gethostbyname(server_alias);
    if (!he) {
        fprintf(stderr, "[-] No se pudo resolver alias %s\n", server_alias);
        return;
    }
    
    client_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (client_sock < 0) {
        perror("[-] Error creando socket");
        return;
    }
    
    //Cambio para la aceptacion por s01 y no por numero 192...
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    memcpy(&serv_addr.sin_addr, he->h_addr_list[0], he->h_length);

    if (connect(client_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("[-] Error conectando con el servidor");
        close(client_sock);
        return;
    }
    
    //Envia el formato al servidor con el puerto objetivo, el corrimiento y el texto a cifrar.
    char mensaje[BUFFER_SIZE];
    snprintf(mensaje, sizeof(mensaje), "%d %s", shift, texto);
    send(client_sock, mensaje, strlen(mensaje), 0);

    int bytes = recv(client_sock, mensaje, sizeof(mensaje)-1, 0);
    if (bytes > 0) {
        mensaje[bytes] = '\0';
        printf("[+] Respuesta de %s: %s\n", server_alias, mensaje);

        //LOG con fecha, hora y agregamos estado
        time_t now = time(NULL); //Tiempo unix (segundos desde 1970)
        struct tm *t = localtime(&now);  //Descompone en struct tm

        FILE *log = fopen("cliente_status.log", "a");  // Abre en append para añadir los documentos que lleguen.
        if (log) {
            fprintf(log, "%04d-%02d-%02d %02d:%02d:%02d - %s - %s:%d\n",
            t->tm_year + 1900,     // tm_year es años desde 1900 -> +1900
            t->tm_mon + 1,         // tm_mon es 0..11 -> +1 para 1..12
            t->tm_mday,            // día del mes 1..31
            t->tm_hour,            // hora 0..23
            t->tm_min,             // minuto 0..59
            t->tm_sec,             // segundos 0..60
            mensaje,               // estado o mensaje del servidor
            server_alias,          // alias del servidor
            port);
            fclose(log);
        }
    }

    close(client_sock);
}

//Leer contenido de archivo
int read_file(char *filename, char *buffer, int max_size) {
    FILE *fp = fopen(filename, "r");
    if (!fp) return -1;
    int bytes = fread(buffer, 1, max_size-1, fp);
    fclose(fp);
    buffer[bytes] = '\0';
    return bytes;
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        printf("Uso: %s <alias_cliente> <corrimiento> <archivo1> [archivo2 ...]\n", argv[0]);
        return 1;
    }
    
    //El primer argumento es el alias.
    char *my_alias = argv[1];
    //El segundo es el corrimiento.
    int shift = atoi(argv[2]);

    //Lista de todos los servidores
    char *all_alias[] = {"s01","s02","s03","s04"};
    int num_alias = 4;

    //Determina a cuáles servidores conectarse
    char *targets[num_alias-1];
    int idx = 0;
    for (int i=0;i<num_alias;i++) {
        if (strcmp(all_alias[i], my_alias) != 0) {
            targets[idx++] = all_alias[i];
        }
    }

    //Iterar sobre archivos
    for (int i=3; i<argc; i++) {
        char buffer[BUFFER_SIZE];
        if (read_file(argv[i], buffer, BUFFER_SIZE) < 0) {
            perror("[-] Error leyendo archivo");
            continue;
        }

        // Enviar a todos los demás servidores
        for (int j=0; j<idx; j++) {
            send2Server(targets[j], 49200, shift, buffer);
        }
    }

    return 0;
}