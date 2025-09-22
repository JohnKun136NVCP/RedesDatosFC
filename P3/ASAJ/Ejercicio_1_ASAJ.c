// client.c
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#define FILE_BUF 1024

/*
    enviar_a_puerto(ip del servidor, puerto al que se conectará, puerto objetivo y desplazamiento, contenido del archivo, tamaño del contenido del archivo)
*/
void enviar_a_puerto(const char *ip, int puerto, const char *header, const char *data, int len)
{
    // creamos un socket IPv4 con AF_INET y especificamos tipo TCP con SOCK_STREAM
    int s = socket(AF_INET, SOCK_STREAM, 0);

    // preparamos la estructura con la dirección del servidor
    struct sockaddr_in destino;
    destino.sin_family = AF_INET;            // IPv4
    destino.sin_port = htons(puerto);        // puerto
    destino.sin_addr.s_addr = inet_addr(ip); // dirección IP del servidor

    // nos conectamos con el servidor de ese puerto
    connect(s, (struct sockaddr *)&destino, sizeof(destino));

    // les mandamos quién es el puerto objetivo y el desplazamiento para césar
    send(s, header, strlen(header), 0);
    send(s, data, len, 0);

    // avisamos que ya acabamos para que el servidor deje de esperar y responda
    shutdown(s, SHUT_WR);

    char resp[256];
    int n = recv(s, resp, sizeof(resp) - 1, 0);
    if (n > 0)
    {
        resp[n] = '\0';

        while (n > 0 && (resp[n - 1] == '\n' || resp[n - 1] == '\r' || resp[n - 1] == ' '))
        {
            resp[--n] = '\0';
        }

        if (strstr(resp, "encriptado") != NULL)
        {
            printf("Respuesta del puerto %d : Archivo recibido y encriptado\n", puerto);
        }
        else if (strstr(resp, "Rechazado") != NULL || strstr(resp, "rechazado") != NULL)
        {
            printf("Respuesta del puerto %d: Rechazado\n", puerto);
        }
        else
        {
            printf("Respuesta del puerto %d: %s\n", puerto, resp);
        }
    }
    else
    {
        printf("Respuesta del puerto %d: (sin respuesta)\n", puerto);
    }

    close(s);
}

int main(int argc, char *argv[])
{
    // ./client <IP_SERVIDOR> <PUERTO_OBJETIVO> <SHIFT> <archivo>

    const char *IP_SERVIDOR = argv[1];     // IP de la MV Ubuntu server
    const char *PUERTO_OBJETIVO = argv[2]; // Puerto que debe cifrar el archivo ("49200"/"49201"/"49202")
    const char *SHIFT = argv[3];           // Desplazamiento para cifrado César (la suma de mi num de cuenta: 30)
    const char *nombre_archivo = argv[4];  // Nombre del archivo a enviar (texto.txt)

    // leemos el archivo completo en memoria
    static char buffer_archivo[FILE_BUF];
    FILE *fp = fopen(nombre_archivo, "r");
    int longitud = fread(buffer_archivo, 1, FILE_BUF - 1, fp);
    buffer_archivo[longitud] = '\0';
    fclose(fp);

    // cabecera con <PUERTO_OBJETIVO> <SHIFT> \n"
    char cabecera[64];
    snprintf(cabecera, sizeof(cabecera), "%s %s\n", PUERTO_OBJETIVO, SHIFT);

    // nos conectamos a tres servidores en diferentes puertos
    enviar_a_puerto(IP_SERVIDOR, 49200, cabecera, buffer_archivo, longitud);
    enviar_a_puerto(IP_SERVIDOR, 49201, cabecera, buffer_archivo, longitud);
    enviar_a_puerto(IP_SERVIDOR, 49202, cabecera, buffer_archivo, longitud);

    return 0;
}
