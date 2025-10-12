#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>

#define TAM_BUFFER (1<<20)
#define MAX_CLIENTES 1024

typedef struct {
    int fd;
    int puerto_servidor;          // puerto del listener que lo aceptó
    struct sockaddr_in addr_cli;
    char *buf;
    size_t recibido;
} Cliente;

static int puertos[] = {49200, 49201, 49202};
static const int NUM_PUERTOS = 3;

// Cifrado César
static void cifrado_cesar(char *texto, int desplazamiento) {
    int k = desplazamiento % 26;
    if (k < 0) k += 26;
    for (size_t i = 0; texto[i] != '\0'; i++) {
        unsigned char c = (unsigned char)texto[i];
        if (c >= 'A' && c <= 'Z') texto[i] = (char)('A' + (c - 'A' + k) % 26);
        else if (c >= 'a' && c <= 'z') texto[i] = (char)('a' + (c - 'a' + k) % 26);
    }
}

// Crear socket en modo escucha para un puerto dado
static int crear_listener(int puerto) {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) { perror("socket"); return -1; }

    int opt = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in dir;
    memset(&dir, 0, sizeof(dir));
    dir.sin_family = AF_INET;
    dir.sin_addr.s_addr = INADDR_ANY;
    dir.sin_port = htons((uint16_t)puerto);

    if (bind(s, (struct sockaddr*)&dir, sizeof(dir)) < 0) {
        perror("bind");
        close(s);
        return -1;
    }
    if (listen(s, 8) < 0) {
        perror("listen");
        close(s);
        return -1;
    }
    printf("[SERVIDOR %d] Escuchando...\n", puerto);
    return s;
}

// Procesar buffer completo según el protocolo y mostrar resultado
static void procesar_y_mostrar(int puerto_servidor, const char *buf) {
    // Protocolo:
    // PORT X\n
    // SHIFT Y\n
    // \n
    // contenido
    int puerto_objetivo = -1, desplazamiento = 0;
    char *copia = strdup(buf);
    if (!copia) return;

    char *p = copia;
    char *l1 = strsep(&p, "\n");
    char *l2 = strsep(&p, "\n");
    strsep(&p, "\n");
    char *contenido = p ? p : (char*)"";

    if (l1) sscanf(l1, "PORT %d", &puerto_objetivo);
    if (l2) sscanf(l2, "SHIFT %d", &desplazamiento);

    if (puerto_objetivo == puerto_servidor) {
        char *trabajo = strdup(contenido);
        if (trabajo) {
            cifrado_cesar(trabajo, desplazamiento);
            printf("[SERVIDOR %d] Solicitud aceptada. Texto cifrado:\n", puerto_servidor);
            puts(trabajo);
            free(trabajo);
        }
    } else {
        printf("[SERVIDOR %d] Solicitud rechazada (objetivo %d)\n",
               puerto_servidor, puerto_objetivo);
    }
    free(copia);
}

//Buscar un hueco para nuevo cliente
static int agregar_cliente(Cliente *cs, int fd, int puerto_srv, struct sockaddr_in *cliaddr) {
    for (int i = 0; i < MAX_CLIENTES; i++) {
        if (cs[i].fd == -1) {
            cs[i].fd = fd;
            cs[i].puerto_servidor = puerto_srv;
            cs[i].addr_cli = *cliaddr;
            cs[i].buf = (char*)malloc(TAM_BUFFER);
            cs[i].recibido = 0;
            if (!cs[i].buf) { cs[i].fd = -1; return -1; }
            return i;
        }
    }
    return -1;
}

// Quitar y limpiar cliente
static void quitar_cliente(Cliente *cs, int i) {
    if (cs[i].fd != -1) close(cs[i].fd);
    if (cs[i].buf) free(cs[i].buf);
    cs[i].fd = -1;
    cs[i].buf = NULL;
    cs[i].recibido = 0;
}

int main(void) {
    int listeners[3];
    for (int i = 0; i < NUM_PUERTOS; i++) {
        listeners[i] = crear_listener(puertos[i]);
        if (listeners[i] < 0) {
            // si uno falla, cerrar los ya abiertos y salir
            for (int j = 0; j < i; j++) close(listeners[j]);
            return 1;
        }
    }

    Cliente clientes[MAX_CLIENTES];
    for (int i = 0; i < MAX_CLIENTES; i++) {
        clientes[i].fd = -1;
        clientes[i].buf = NULL;
        clientes[i].recibido = 0;
        clientes[i].puerto_servidor = 0;
    }

    while (1) {
        fd_set rd;
        FD_ZERO(&rd);
        int maxfd = -1;

        // Monitorear listeners
        for (int i = 0; i < NUM_PUERTOS; i++) {
            FD_SET(listeners[i], &rd);
            if (listeners[i] > maxfd) maxfd = listeners[i];
        }

        // Monitorear clientes activos
        for (int i = 0; i < MAX_CLIENTES; i++) {
            if (clientes[i].fd != -1) {
                FD_SET(clientes[i].fd, &rd);
                if (clientes[i].fd > maxfd) maxfd = clientes[i].fd;
            }
        }

        int rsel = select(maxfd + 1, &rd, NULL, NULL, NULL);
        if (rsel < 0) {
            if (errno == EINTR) continue;
            perror("select");
            break;
        }

        // Aceptar nuevas conexiones en cualquier listener listo
        for (int li = 0; li < NUM_PUERTOS; li++) {
            if (FD_ISSET(listeners[li], &rd)) {
                struct sockaddr_in cli;
                socklen_t tcli = sizeof(cli);
                int cfd = accept(listeners[li], (struct sockaddr*)&cli, &tcli);
                if (cfd < 0) {
                    perror("accept");
                } else {
                    int idx = agregar_cliente(clientes, cfd, puertos[li], &cli);
                    if (idx < 0) {
                        printf("[SERVIDOR %d] Sin espacio para más clientes\n", puertos[li]);
                        close(cfd);
                    } else {
                        printf("[SERVIDOR %d] Cliente conectado desde %s:%d (slot %d)\n",
                               puertos[li], inet_ntoa(cli.sin_addr), ntohs(cli.sin_port), idx);
                    }
                }
            }
        }

        // Leer de clientes listos
        for (int i = 0; i < MAX_CLIENTES; i++) {
            if (clientes[i].fd != -1 && FD_ISSET(clientes[i].fd, &rd)) {
                // Leer un bloque
                size_t cap_disp = TAM_BUFFER - 1 - clientes[i].recibido;
                if (cap_disp == 0) {
                    // buffer lleno cerrar y procesar lo que haya
                    clientes[i].buf[clientes[i].recibido] = '\0';
                    procesar_y_mostrar(clientes[i].puerto_servidor, clientes[i].buf);
                    quitar_cliente(clientes, i);
                    continue;
                }
                ssize_t r = recv(clientes[i].fd, clientes[i].buf + clientes[i].recibido, cap_disp, 0);
                if (r > 0) {
                    clientes[i].recibido += (size_t)r;
                } else if (r == 0) {
                    // EOF: procesar
                    clientes[i].buf[clientes[i].recibido] = '\0';
                    procesar_y_mostrar(clientes[i].puerto_servidor, clientes[i].buf);
                    quitar_cliente(clientes, i);
                } else {
                    if (errno == EINTR) continue;
                    perror("recv");
                    quitar_cliente(clientes, i);
                }
            }
        }
    }

    // Limpieza final
    for (int i = 0; i < NUM_PUERTOS; i++) close(listeners[i]);
    for (int i = 0; i < MAX_CLIENTES; i++) quitar_cliente(clientes, i);
    return 0;
}