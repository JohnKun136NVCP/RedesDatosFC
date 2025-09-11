#include <arpa/inet.h>   
#include <netinet/in.h> 
#include <sys/socket.h>
#include <unistd.h>      
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>       
#include <errno.h>       

#define BUFSZ 4096       

// Colores para que se vea mas coqueto.
#define C_RESET "\x1b[0m"
#define C_RED   "\x1b[31m"
#define C_GRN   "\x1b[32m"
#define C_CYN   "\x1b[36m"

// Cifrado cesar
static void caesar_inplace(char *s, int k){

    if (k < 0) k = (26 - ((-k)%26)) % 26; else k %= 26;

    for (char *p = s; *p; ++p){
        if ('a' <= *p && *p <= 'z') *p = 'a' + ((*p - 'a' + k) % 26);
        else if ('A' <= *p && *p <= 'Z') *p = 'A' + ((*p - 'A' + k) % 26);
    }
    
}

// Lee n bytes en buf, devolviendo n o error. No depende del EOF del cliente, y asi el servidor sabe cuando parar.
static ssize_t read_full(int fd, char *buf, size_t n){

    size_t got = 0;
    
    while (got < n) {
        ssize_t r = recv(fd, buf + got, n - got, 0);
        // Cierre anticipao del cliente
        if (r == 0) return 0;
        
        if (r < 0) {
            if (errno == EINTR) continue;  
            return -1;                 
        }
        
        got += (size_t)r;
    }
    
    return (ssize_t)got;
}

int main(int argc, char *argv[]) {

    if (argc != 2) {
        fprintf(stderr, "Uso: %s <PUERTO>\n", argv[0]);
        return 1;
    }
    
    // Puerto en el que este servidor escucha
    int myport = atoi(argv[1]);           
    // Crea socket 
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) { perror("socket"); return 1; }

    int opt = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    // Llena estructura de dirección
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(myport);
    addr.sin_addr.s_addr = INADDR_ANY;

    // Asociamos socket a un puerto
    if (bind(s, (struct sockaddr*)&addr, sizeof(addr)) < 0) { perror("bind"); return 1; }

    // Ponemos el socket en modo escucha
    if (listen(s, 16) < 0) { perror("listen"); return 1; }

    // Quiero poner los mesajes asi como en los ejemplos de la practica
    printf(C_CYN "[*][CLIENT %d] LISTENING..." C_RESET "\n", myport);
    fflush(stdout);

    // for para aceptar y atender conexiones una por una
    for(;;){
        // Aceptamos un cliente
        int c = accept(s, NULL, NULL);
        if (c < 0) { perror("accept"); continue; }

        char acc[BUFSZ]; size_t used = 0;
        int target = -1, k = 0;
        size_t need = 0;

        // Lee hasta encontrar '\n' del header
        for(;;){
            ssize_t n = recv(c, acc + used, sizeof(acc) - used - 1, 0);
            
            if (n <= 0) { close(c); goto NEXT_CONN; }
            
            used += (size_t)n; acc[used] = '\0';
            char *nl = memchr(acc, '\n', used);
            
            if (nl){
                size_t header_len = (size_t)(nl - acc);
                char header[256];
                
                if (header_len >= sizeof(header)) header_len = sizeof(header) - 1;
                
                memcpy(header, acc, header_len);
                header[header_len] = '\0';

                // Esperamos target, k y len
                if (sscanf(header, "%d %d %zu", &target, &k, &need) != 3){
                    const char *bad = "BAD REQUEST\n";
                    send(c, bad, strlen(bad), 0);
                    close(c);
                    goto NEXT_CONN;
                }

                size_t rest = used - (header_len + 1); 
                char *body = (char*)malloc(need + 1);   
                size_t blen = 0;
                
                if (rest > 0) {
                    size_t take = (rest > need ? need : rest);
                    memcpy(body, nl + 1, take);
                    blen = take;
                }

                if (blen < need) {
                    ssize_t r = read_full(c, body + blen, need - blen);
                    if (r <= 0) { free(body); close(c); goto NEXT_CONN; }
                    blen += (size_t)r;
                }
                
                body[blen] = '\0';

                // si target == mi puerto lo procesa
                // si no, lo rechaza
                if (target == myport){
                
                    // Cesar sobre el cuerpo
                    caesar_inplace(body, k);

                    // Enviamos respuesta al cliente
                    const char *ok = "PROCESADO\n";
                    send(c, ok, strlen(ok), 0);
                    if (blen) send(c, body, blen, 0);

                    printf(C_GRN "[*][SERVER %d] Request accepted..." C_RESET "\n", myport);
                    printf(C_GRN "[*][SERVER %d] File received and encrypted:" C_RESET "\n", myport);
                    
                    if (blen) {
                        fwrite(body, 1, blen, stdout);
                        if (body[blen-1] != '\n') putchar('\n');
                    } else {
                        printf("(empty)\n");
                    }
                    fflush(stdout);
                } else {
                    // Enviar RECHAZADO si el puerto no coincide
                    const char *no = "RECHAZADO\n";
                    send(c, no, strlen(no), 0);

                    printf(C_RED "[*][SERVER %d] Request rejected (client requested port %d)"
                           C_RESET "\n", myport, target);
                    fflush(stdout);
                }

                // Limpieza y cerrar conexión
                free(body);
                close(c);
                goto NEXT_CONN;
            }

            if (used >= sizeof(acc) - 1) {
                const char *bad = "BAD HEADER\n";
                send(c, bad, strlen(bad), 0);
                close(c);
                goto NEXT_CONN;
            }
        }

NEXT_CONN:
        ; // continuar con la siguiente conexión
    }
}

