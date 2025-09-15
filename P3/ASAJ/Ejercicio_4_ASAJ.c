// serverOpt.c
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

static void corre_servidor(int PUERTO)
{
    int srv = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in dir;
    memset(&dir, 0, sizeof(dir));
    dir.sin_family = AF_INET;
    dir.sin_addr.s_addr = htonl(INADDR_ANY);
    dir.sin_port = htons((uint16_t)PUERTO);

    bind(srv, (struct sockaddr *)&dir, sizeof(dir));
    listen(srv, 8);

    printf("[Servidor] Esuchando puerto %d...\n", PUERTO);

    // aceptamos muchas conexiones
    for (;;)
    {
        int cfd = accept(srv, NULL, NULL);
        if (cfd < 0)
            continue;

        char header[CABECERA_MAX];
        leer_linea(cfd, header, sizeof(header));

        int puerto_obj = 0, shift = 0;
        char nombreArchivo[128] = {0};

        sscanf(header, "%d %d %127s", &puerto_obj, &shift, nombreArchivo);

        char salida[256];
        snprintf(salida, sizeof(salida), "cesar_%s", nombreArchivo);

        static char texto[FILE_BUF];
        int total = 0, n;
        while ((n = recv(cfd, texto + total, (int)(FILE_BUF - 1 - total), 0)) > 0)
        {
            total += n;
            if (total >= FILE_BUF - 1)
            {
                break;
            }
        }
        texto[total] = '\0';

        if (puerto_obj == PUERTO)
        {
            cesar(texto, shift);

            FILE *f = fopen(salida, "wb");

            if (f)
            {
                fwrite(texto, 1, (size_t)strlen(texto), f);
                fclose(f);
            }

            printf("Petición aceptada: Archivo recibido y encriptado\n");
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
    }

    close(srv);
}

int main(void)
{
    int puertos[3] = {49200, 49201, 49202};

    for (int i = 0; i < 3; ++i)
    {
        // creamos hijos para realizar las tareas individuales
        pid_t pid = fork();
        if (pid == 0)
        {
            corre_servidor(puertos[i]);
            _exit(0);
        }
    }

    for (;;)
        pause();

    return 0;
}
