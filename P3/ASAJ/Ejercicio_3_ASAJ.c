// clientMulti.c
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FILE_BUF 65536

/*
    lo hacemos función porque se ocupará más veces
*/
static int leer_archivo(const char *nombre_archivo, char *buffer_archivo, int cap)
{
    FILE *fp = fopen(nombre_archivo, "r");
    int longitud = 0;
    if (fp)
    {
        longitud = (int)fread(buffer_archivo, 1, cap - 1, fp);
        buffer_archivo[longitud] = '\0';
        fclose(fp);
    }
    return longitud;
}

/*
    enviar_a_puerto(ip del servidor, puerto al que se conectará, puerto objetivo y desplazamiento, contenido del archivo, tamaño del contenido del archivo)
*/
static void enviar_a_puerto(const char *ip, int puerto, const char *header, const char *data, int len)
{
    int s = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in destino;
    destino.sin_family = AF_INET;
    destino.sin_port = htons(puerto);
    destino.sin_addr.s_addr = inet_addr(ip);

    connect(s, (struct sockaddr *)&destino, sizeof(destino));

    send(s, header, (int)strlen(header), 0);
    send(s, data, len, 0);
    shutdown(s, SHUT_WR);

    char resp[256];
    int n = recv(s, resp, sizeof(resp) - 1, 0);
    if (n > 0)
    {
        resp[n] = '\0';

        while (n > 0 && (resp[n - 1] == '\n' || resp[n - 1] == '\r' || resp[n - 1] == ' '))
            resp[--n] = '\0';

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
    // ./clientemulti IP P1 P2 P3 A1 A2 A3 SHIFT
    const char *IP = argv[1];

    int P1 = atoi(argv[2]);
    int P2 = atoi(argv[3]);
    int P3 = atoi(argv[4]);

    const char *A1 = argv[5];
    const char *A2 = argv[6];
    const char *A3 = argv[7];

    const char *SHIFT = argv[8];

    static char B1[FILE_BUF], B2[FILE_BUF], B3[FILE_BUF];
    int L1 = leer_archivo(A1, B1, FILE_BUF);
    int L2 = leer_archivo(A2, B2, FILE_BUF);
    int L3 = leer_archivo(A3, B3, FILE_BUF);

    char H1[64], H2[64], H3[64];
    snprintf(H1, sizeof(H1), "%d %s %s\n", P1, SHIFT, A1);
    snprintf(H2, sizeof(H2), "%d %s %s\n", P2, SHIFT, A2);
    snprintf(H3, sizeof(H3), "%d %s %s\n", P3, SHIFT, A3);

    enviar_a_puerto(IP, P1, H1, B1, L1);
    enviar_a_puerto(IP, P2, H2, B2, L2);
    enviar_a_puerto(IP, P3, H3, B3, L3);

    return 0;
}
