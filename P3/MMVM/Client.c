#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BUFSZ 4096

// Lee a memoria el archivo completo. Regresamos buffer + longitud.
static char* read_file(const char* path, size_t* out_len) {

    FILE* fp = fopen(path, "rb");
    if (!fp) { perror("fopen"); return NULL; }
    
    char *data = NULL;
    size_t cap = 0, len = 0;
    char buf[BUFSZ];
    size_t n;

    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        if (len + n + 1 > cap) {
            cap = (cap ? cap*2 : 8192);           
            while (cap < len + n + 1) cap *= 2; 
            data = (char*)realloc(data, cap);
            if (!data) { perror("realloc"); fclose(fp); return NULL; }
        }
        memcpy(data + len, buf, n);
        len += n;
    }
    
    if (ferror(fp)) { perror("fread"); free(data); fclose(fp); return NULL; }
    
    fclose(fp);
    
    if (!data) { data = (char*)malloc(1); data[0] = '\0'; }
    
    data[len] = '\0';
    if (out_len) *out_len = len;
    return data;
}

// Conecta a un puerto y envia el paquete con target k len y el cuerpo
// Imprimimos la respuesta del servidor
static int talk_one(const char* ip, int listen_port, int target_port, int k,
                    const char* body, size_t body_len) {
    // Creamos socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return -1; }

    // Llenamos sockaddr_in con ip y puerto de ese servidor
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port   = htons(listen_port);
    sa.sin_addr.s_addr = inet_addr(ip);

    // Conectamos
    if (connect(sock, (struct sockaddr*)&sa, sizeof(sa)) < 0) {
        perror("connect");
        close(sock);
        return -1;
    }

    // Mensaje como en la practica 
    printf("\n[CLIENT] ----> Conectado a %s con el servidor %d (target = %d, k = %d) <----\n",
           ip, listen_port, target_port, k);

    // Enviar header 
    char header[96];
    int hlen = snprintf(header, sizeof(header), "%d %d %zu\n", target_port, k, body_len);
    if (send(sock, header, hlen, 0) < 0) perror("send header");

    // Enviamos el cuerpo en trozos de tamaño BUFSZ
    size_t sent = 0;
    while (sent < body_len) {
        size_t chunk = body_len - sent;
        if (chunk > BUFSZ) chunk = BUFSZ;
        int n = send(sock, body + sent, (int)chunk, 0);
        if (n <= 0) { perror("send body"); break; }
        sent += (size_t)n;
    }

    // Cerramos el lado de escritura
    shutdown(sock, SHUT_WR);

    // Recibir respuesta del servidor hasta que cierre
    char resp[BUFSZ + 1];
    int r;
    
    while ((r = recv(sock, resp, BUFSZ, 0)) > 0) {
        resp[r] = '\0';
        fputs(resp, stdout);
    }
    
    if (r < 0) perror("recv");

    close(sock);
    return 0;
}

int main(int argc, char *argv[]) {
    // La poderosa jaja
    if (argc != 5) {
        fprintf(stderr, "Uso: %s <IP_SERVIDOR> <PUERTO> <DESPLAZAMIENTO> <Archivo>\n", argv[0]);
        return 1;
    }

    const char *server_ip = argv[1];
    // Puerto que deberia procesar
    int target_port = atoi(argv[2]);
    // Desplazamiento cesar
    int k = atoi(argv[3]);
    const char *file_path = argv[4];

    // Cargar el archivo completo
    size_t body_len = 0;
    char *body = read_file(file_path, &body_len);
    if (!body) return 1;

    // Lista fija de servidores validos
    const int valid_ports[] = {49200, 49201, 49202};
    const int N = (int)(sizeof(valid_ports)/sizeof(valid_ports[0]));

    // Vamos a conectarnos a cada servidor y enviar el mismo paquete
    for (int i = 0; i < N; ++i) {
        (void)talk_one(server_ip, valid_ports[i], target_port, k, body, body_len);
    }

    free(body);
    return 0;
}

