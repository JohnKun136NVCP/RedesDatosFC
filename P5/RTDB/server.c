#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BUFFER_SIZE 1024
#define BASE_PORT 49200
#define NUM_SERVERS 4

pthread_mutex_t turn_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t turn_cond = PTHREAD_COND_INITIALIZER;

int my_index = -1;
int current_turn = 0; // 0 = s01, 1 = s02, etc.

const char *servers[] = {"s01", "s02", "s03", "s04"};

void logStatus(const char *alias, const char *msg) {
    char logfile[64];
    snprintf(logfile, sizeof(logfile), "%s/server_log.txt", alias);
    FILE *f = fopen(logfile, "a");
    if (!f) return;
    time_t now = time(NULL);
    fprintf(f, "[%s] %s\n", ctime(&now), msg);
    fclose(f);
}

// Hilo que espera turno
void *turn_listener(void *arg) {
    int sock;
    struct sockaddr_in addr;
    char buffer[BUFFER_SIZE];

    sock = socket(AF_INET, SOCK_STREAM, 0);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(BASE_PORT + 100 + my_index); // canal interno
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(sock, (struct sockaddr*)&addr, sizeof(addr));
    listen(sock, 5);

    while (1) {
        int client = accept(sock, NULL, NULL);
        if (client < 0) continue;
        int bytes = recv(client, buffer, sizeof(buffer)-1, 0);
        if (bytes > 0) {
            buffer[bytes] = 0;
            if (strcmp(buffer, "TURN_GRANTED") == 0) {
                pthread_mutex_lock(&turn_mutex);
                current_turn = my_index;
                pthread_cond_signal(&turn_cond);
                pthread_mutex_unlock(&turn_mutex);
            }
        }
        close(client);
    }
}

// Pasar el turno al siguiente servidor
void pass_turn() {
    int next = (my_index + 1) % NUM_SERVERS;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(BASE_PORT + 100 + next);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
        send(sock, "TURN_GRANTED", strlen("TURN_GRANTED"), 0);
        printf("[TURN] Pasando turno a %s\n", servers[next]);
    }
    close(sock);
    pthread_mutex_lock(&turn_mutex);
    current_turn = -1;  // Nadie tiene el turno temporalmente
    pthread_mutex_unlock(&turn_mutex);
}

// Hilo principal de recepción de clientes
void *server_thread(void *arg) {
    const char *alias = (const char *)arg;
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size;
    char buffer[BUFFER_SIZE];

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(BASE_PORT + my_index);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        exit(1);
    }

    if (listen(server_sock, 5) < 0) {
        perror("listen");
        exit(1);
    }

    while (1) {
        pthread_mutex_lock(&turn_mutex);
        while (current_turn != my_index) {
            printf("[%s] Esperando turno...\n", alias);
            pthread_cond_wait(&turn_cond, &turn_mutex);
        }
        pthread_mutex_unlock(&turn_mutex);

        printf("[%s] Es mi turno. Esperando cliente...\n", alias);
        addr_size = sizeof(client_addr);
        client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &addr_size);
        if (client_sock < 0) continue;

        printf("[+] Cliente conectado desde %s:%d\n",
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        // Enviar mensaje de bienvenida
        char *welcome = "Servidor listo para recibir archivo.\n";
        send(client_sock, welcome, strlen(welcome), 0);

        char outFile[128];
        snprintf(outFile, sizeof(outFile), "%s/received_file.txt", alias);
        FILE *fp = fopen(outFile, "wb");
        if (!fp) {
            perror("fopen");
            close(client_sock);
            continue;
        }

        int bytes;
        while ((bytes = recv(client_sock, buffer, sizeof(buffer), 0)) > 0) {
            fwrite(buffer, 1, bytes, fp);
        }
        fclose(fp);

        // Confirmar recepción al cliente
        char *done_msg = "Archivo recibido correctamente.\n";
        send(client_sock, done_msg, strlen(done_msg), 0);

        close(client_sock);
        logStatus(alias, "Archivo recibido");
        printf("[%s] Archivo recibido y confirmado al cliente.\n", alias);

        pass_turn(); // Pasar turno al siguiente
    }

    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Uso: %s <alias> (s01, s02, s03, s04)\n", argv[0]);
        return 1;
    }

    const char *alias = argv[1];

    for (int i = 0; i < NUM_SERVERS; i++) {
        if (strcmp(alias, servers[i]) == 0) my_index = i;
    }

    if (my_index == -1) {
        printf("Alias no válido.\n");
        return 1;
    }

    pthread_t t1, t2;
    pthread_create(&t1, NULL, turn_listener, NULL);
    pthread_create(&t2, NULL, server_thread, (void*)alias);

    // Solo s01 inicia con el turno
    if (my_index == 0) {
        sleep(2);
        current_turn = 0;
        pthread_cond_signal(&turn_cond);
    }

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    return 0;
}
