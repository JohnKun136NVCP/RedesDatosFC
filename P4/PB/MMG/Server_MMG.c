#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <sys/stat.h>

#define BUFFER_SIZE 8192

// ======= Mapeo de alias a IPs =======
typedef struct {
    char *alias;
    char *ip;
} alias_ip_t;

alias_ip_t mapeo[] = {
    // (Actualización, se agregan s03 y s04)
    {"s01", "192.168.0.101"},
    {"s02", "192.168.0.102"},
    {"s03", "192.168.0.103"},
    {"s04", "192.168.0.104"},
    {NULL, NULL}  // Terminador
};

// ======= Función para obtener IP desde alias =======
char* obtener_ip(const char *alias) {
    for (int i = 0; mapeo[i].alias != NULL; i++) {
        if (strcmp(mapeo[i].alias, alias) == 0) {
            return mapeo[i].ip;
        }
    }
    return NULL; // No encontrado
}

// ======= Función de Cifrado César de práctica 2 =======
void encryptCesar(char *text, int shift) {
    shift = ((shift % 26) + 26) % 26;

    for (int i = 0; text[i] != '\0'; i++) {
        unsigned char c = (unsigned char)text[i];

        if (isupper(c)) {
            // Mayúsculas
            text[i] = (char)(((c - 'A' + shift) % 26) + 'A');
        } else if (islower(c)) {
            // Minúsculas
            text[i] = (char)(((c - 'a' + shift) % 26) + 'a');
        } else {
            // Caracteres no alfabéticos quedan igual
            text[i] = (char)c;
        }
    }
}

// ======= Helper para escribir logs con timestamp =======
void log_event(const char *alias, const char *mensaje) {
    // Crear directorio si no existe
    struct stat st = {0};
    if (stat(alias, &st) == -1) {
        mkdir(alias, 0755);
    }

    char fname[128];
    snprintf(fname, sizeof(fname), "%s/status.log", alias);

    FILE *f = fopen(fname, "a");
    if (!f) return;

    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%F %T", tm_info);

    fprintf(f, "[%s] %s\n", timestamp, mensaje);
    fclose(f);
}

// Implementación del servidor para UN alias
static int run_server_for_alias(const char *alias) {
    char *ip_servidor = obtener_ip(alias);
    int puerto = 49200;  // Puerto fijo para todos los alias
    
    if (ip_servidor == NULL) {
        fprintf(stderr, "Alias inválido: %s\n", alias);
        fprintf(stderr, "Aliases disponibles: s01, s02, s03, s04\n");
        return 1;
    }

    int sockfd = -1, newsockfd = -1;
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t clilen;
    char buffer[BUFFER_SIZE];

    // ======= Crear socket TCP =======
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) { perror("socket"); return 1; }

    // ======= Reusar puerto si se reinicia rápido =======
    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // ======= Configuración del servidor =======
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(ip_servidor);  // Bind a la IP del alias
    serv_addr.sin_port = htons(puerto);

    // ======= Bind =======
    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("bind");
        close(sockfd);
        return 1;
    }

    // ======= Listen =======
    if (listen(sockfd, 5) < 0) {
        perror("listen");
        close(sockfd);
        return 1;
    }

    printf("[*]SERVIDOR %s escuchando en puerto %d ...\n", alias, puerto);
    log_event(alias, "Servidor iniciado y en escucha");

    // ======= Loop principal para aceptar conexiones =======
    clilen = sizeof(cli_addr);
    while (1) {
        newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
        if (newsockfd < 0) { perror("accept"); continue; }

        // ======= Leer petición: "<PUERTO_OBJ> <SHIFT> <TEXTO>" =======
        memset(buffer, 0, sizeof(buffer));
        ssize_t n = read(newsockfd, buffer, sizeof(buffer) - 1);
        if (n <= 0) { close(newsockfd); continue; }

        int puerto_obj = 0, shift = 0;
        char texto[BUFFER_SIZE];
        texto[0] = '\0';

        if (sscanf(buffer, "%d %d %[^\n]", &puerto_obj, &shift, texto) < 2) {
            const char *errorFormato = "Formato inválido (esperado: <PUERTO> <DESPLAZAMIENTO> <TEXTO>)\n";
            write(newsockfd, errorFormato, strlen(errorFormato));
            log_event(alias, "Solicitud rechazada: formato inválido");
            close(newsockfd);
            continue;
        }

        if (puerto_obj != puerto) {
            // Rechazo
            char msg[128];
            snprintf(msg, sizeof(msg), "Solicitud rechazada (cliente pidió puerto %d)", puerto_obj);
            printf("[*]SERVIDOR %s → %s\n", alias, msg);
            log_event(alias, msg);

            char rechazo[128];
            snprintf(rechazo, sizeof(rechazo), "Rechazado por servidor %s en puerto %d\n", alias, puerto);
            write(newsockfd, rechazo, strlen(rechazo));
            close(newsockfd);
            continue;
        }

        // Aceptado
        printf("[*]SERVIDOR %s → Solicitud aceptada\n", alias);
        log_event(alias, "Solicitud aceptada");

        encryptCesar(texto, shift);

        char msg2[256];
        snprintf(msg2, sizeof(msg2), "Archivo recibido y cifrado: %.200s", texto);
        printf("[*]SERVIDOR %s → %s\n", alias, msg2);
        log_event(alias, msg2);

        // ======= Responder al cliente =======
        char respuesta[BUFFER_SIZE];
        // Limito el %s a 4000 caracteres para evitar warning de truncamiento
        snprintf(respuesta, sizeof(respuesta),
                 "Procesado por servidor %s en puerto %d: %.4000s\n", alias, puerto, texto);
        write(newsockfd, respuesta, strlen(respuesta));

        close(newsockfd);
    }

    close(sockfd);
    return 0;
}

// ======= Servidor TCP =======
int main(int argc, char *argv[]) {
    // Acepta uno o varios alias (fork por alias).
    if (argc < 2) {
        fprintf(stderr, "USO: %s <ALIAS> [<ALIAS> ...]\n", argv[0]);
        fprintf(stderr, "Aliases disponibles: s01, s02, s03, s04\n");
        return 1;
    }

    // Un alias: comportamiento original
    if (argc == 2) {
        return run_server_for_alias(argv[1]);
    }

    // Varios alias: un proceso por alias
    int children = 0;
    for (int i = 1; i < argc; i++) {
        const char *alias = argv[i];
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            continue;
        }
        if (pid == 0) {
            int rc = run_server_for_alias(alias);
            _exit(rc == 0 ? 0 : 2);
        }
        children++;
    }

    // Espera a los hijos
    while (children > 0) {
        int status = 0;
        pid_t p = wait(&status);
        if (p < 0) {
            if (errno == EINTR) continue;
            break;
        }
        children--;
    }
    return 0;
}
