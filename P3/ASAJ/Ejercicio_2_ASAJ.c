// server.c 
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define FILE_BUF 1024
#define CABECERA_MAX 128

static void cesar(char *s, int k)
{
    k %= 26;
    if (k < 0)
        k += 26;
    for (int i = 0; s[i] != '\0'; i++)
    {
        unsigned char c = (unsigned char)s[i];
        if (c >= 'a' && c <= 'z')
        {
            s[i] = (char)('a' + ((c - 'a' + k) % 26));
        }
        else if (c >= 'A' && c <= 'Z')
        {
            s[i] = (char)('A' + ((c - 'A' + k) % 26));
        }
    }
}

// leemos del socket hasta encontrar '\n' y guardamos la línea
static void leer_linea(int cfd, char *hdr, int maxhdr)
{
    int pos = 0;
    while (pos < maxhdr - 1)
    {
        char ch;
        int n = recv(cfd, &ch, 1, 0);
        if (n <= 0)
            break;
        if (ch == '\n')
            break;
        if (ch != '\r')
        {
            hdr[pos++] = ch;
        }
    }
    hdr[pos] = '\0';
}

int main(int argc, char *argv[])
{

    const int PUERTO = atoi(argv[1]);
    // creamos socket TCP IPv4
    int srv = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in dir;
    memset(&dir, 0, sizeof(dir));
    dir.sin_family = AF_INET;
    dir.sin_addr.s_addr = htonl(INADDR_ANY);
    dir.sin_port = htons((uint16_t)PUERTO);

    bind(srv, (struct sockaddr *)&dir, sizeof(dir));
    listen(srv, 8);

    printf("[Servidor] Escuchando en el puerto %d...\n", PUERTO);

    // aceptamos y ponemos null ip y puerto porque nosotros ya sabemos esa info
    int cfd = accept(srv, NULL, NULL);

    // leemos cabecera
    char header[CABECERA_MAX];
    leer_linea(cfd, header, sizeof(header));

    // pasamos la info leída a variables para usarlas posteriormente
    int puerto_obj = 0, shift = 0;
    sscanf(header, "%d %d", &puerto_obj, &shift);

    // leemos hasta que el cliente cierre su lado
    static char texto[FILE_BUF];
    int total = 0;
    for (;;)
    {
        int n = recv(cfd, texto + total, (int)(FILE_BUF - 1 - total), 0);
        if (n <= 0)
            break;
        total += n;
        if (total >= FILE_BUF - 1)
            break;
    }
    texto[total] = '\0';

    if (puerto_obj == PUERTO)
    {
        cesar(texto, shift);

        printf("Petición aceptada: Archivo recibido y encriptado\n");
        printf("- %s -\n", texto);

        const char *ok = "Archivo recibido y encriptado";
        send(cfd, ok, strlen(ok), 0);
    }
    else
    {
        printf("Petición rechazada: El cliente solicitó el puerto %d\n", puerto_obj);

        const char *no = "Rechazado";
        send(cfd, no, strlen(no), 0);
    }

    close(cfd);

    close(srv);
    return 0;
}
