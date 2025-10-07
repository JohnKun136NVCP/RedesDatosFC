#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>

#define TAM_BUFFER 1024
#define PUERTO_BASE 49200
#define PUERTO_CONTROL 49300
#define MAX_CLIENTES 10
#define TIEMPO_TURNO 15
#define TIMEOUT_ESPERA 45
#define NUM_SERVIDORES 4

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
int turno_actual = 0;
int mi_id = -1;
int recibiendo = 0;
int pendientes[NUM_SERVIDORES] = {0};
int running = 1;

// Cifrado César
void cifrar_cesar(char *texto, int desplazamiento) {
    desplazamiento = desplazamiento % 26;
    for (int i = 0; texto[i] != '\0'; i++) {
        char c = texto[i];
        if (isupper(c)) {
            texto[i] = ((c - 'A' + desplazamiento) % 26) + 'A';
        } else if (islower(c)) {
            texto[i] = ((c - 'a' + desplazamiento) % 26) + 'a';
        } else {
            texto[i] = c;
        }
    }
}

// Obtener id de servidor
int get_id(const char *nombre) {
    if (strcmp(nombre, "s01") == 0) return 0;
    if (strcmp(nombre, "s02") == 0) return 1;
    if (strcmp(nombre, "s03") == 0) return 2;
    if (strcmp(nombre, "s04") == 0) return 3;
    return -1;
}

// Notificar a otros servidores con UDP
void notificar(const char *servidor, const char *accion) {
    char msg[200];
    sprintf(msg, "%s:%s", servidor, accion);
    
    for (int i = 0; i < NUM_SERVIDORES; i++) {
        if (i == mi_id) continue;
        
        int sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock < 0) continue;
        
        struct sockaddr_in addr = {
            .sin_family = AF_INET,
            .sin_port = htons(PUERTO_CONTROL + i),
            .sin_addr.s_addr = inet_addr("127.0.0.1")
        };
        
        sendto(sock, msg, strlen(msg), 0, (struct sockaddr*)&addr, sizeof(addr));
        close(sock);
    }
}

// Verificar turno
int tengo_turno() {
    pthread_mutex_lock(&mutex);
    int es_mi_turno = (turno_actual == mi_id);
    pthread_mutex_unlock(&mutex);
    return es_mi_turno;
}

// Esperar turno con timeout
int esperar_turno(const char *servidor, int timeout) {
    pthread_mutex_lock(&mutex);
    
    struct timespec ts = {.tv_sec = time(NULL) + timeout, .tv_nsec = 0};
    pendientes[mi_id] = 1;
    
    printf("s0%d esperando turno (actual: s0%d)\n", mi_id + 1, turno_actual + 1);
    
    while (running && (turno_actual != mi_id || recibiendo)) {
        if (pthread_cond_timedwait(&cond, &mutex, &ts) == ETIMEDOUT) {
            pendientes[mi_id] = 0;
            pthread_mutex_unlock(&mutex);
            return 0;
        }
    }
    
    if (!running) {
        pendientes[mi_id] = 0;
        pthread_mutex_unlock(&mutex);
        return 0;
    }
    
    pendientes[mi_id] = 0;
    pthread_mutex_unlock(&mutex);
    
    printf("s0%d obtuvo turno\n", mi_id + 1);
    return 1;
}

// Hilo de control: recibe notificaciones 
void* hilo_control(void* arg) {
    char *servidor = (char*)arg;
    
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return NULL;
    
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(PUERTO_CONTROL + mi_id),
        .sin_addr.s_addr = INADDR_ANY
    };
    
    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        return NULL;
    }
    
    printf("Escuchando puerto %d\n", PUERTO_CONTROL + mi_id);
    
    char buffer[256];
    struct sockaddr_in emisor;
    socklen_t tam = sizeof(emisor);
    
    while (running) {
        int bytes = recvfrom(sock, buffer, sizeof(buffer)-1, 0, (struct sockaddr*)&emisor, &tam);
        if (bytes > 0) {
            buffer[bytes] = '\0';
            
            char srv[10], accion[50];
            if (sscanf(buffer, "%[^:]:%s", srv, accion) == 2) {
                if (strcmp(accion, "RECIBIENDO") == 0) {
                    printf("%s recibiendo\n", srv);
                } else if (strcmp(accion, "FINALIZADO") == 0) {
                    printf("%s finalizó\n", srv);
                } else if (strstr(accion, "SOLICITUD_PENDIENTE")) {
                    int id = atoi(&srv[2]) - 1;
                    if (id >= 0 && id < NUM_SERVIDORES) {
                        pthread_mutex_lock(&mutex);
                        pendientes[id] = 1;
                        pthread_mutex_unlock(&mutex);
                    }
                }
            }
        }
    }
    
    close(sock);
    return NULL;
}

// Hilo que maneja rotación de turnos 
void* hilo_gestor(void* arg) {
    char *servidor = (char*)arg;
    sleep(2);
    
    if (mi_id != 0) {
        // Otros servidores solo escuchan
        while (running) {
            pthread_mutex_lock(&mutex);
            pthread_cond_wait(&cond, &mutex);
            pthread_mutex_unlock(&mutex);
        }
        return NULL;
    }
    
    printf("Round Robin iniciado\n");
    
    while (running) {
        sleep(TIEMPO_TURNO);
        
        pthread_mutex_lock(&mutex);
        
        if (!recibiendo && pendientes[turno_actual] == 0) {
            int anterior = turno_actual;
            
            // Buscar servidor con pendientes
            int encontrado = 0;
            for (int i = 1; i <= NUM_SERVIDORES; i++) {
                int candidato = (turno_actual + i) % NUM_SERVIDORES;
                if (pendientes[candidato] > 0) {
                    turno_actual = candidato;
                    encontrado = 1;
                    printf("Turno SALTO a s0%d (pendientes)\n", turno_actual + 1);
                    break;
                }
            }
            
            if (!encontrado) {
                turno_actual = (turno_actual + 1) % NUM_SERVIDORES;
                printf("Turno s0%d -> s0%d\n", anterior + 1, turno_actual + 1);
            }
            
            char msg[100];
            sprintf(msg, "TURNO:s0%d", turno_actual + 1);
            notificar(servidor, msg);
            pthread_cond_broadcast(&cond);
        } else {
            printf("s0%d ocupado, manteniendo\n", turno_actual + 1);
        }
        
        pthread_mutex_unlock(&mutex);
    }
    
    return NULL;
}

// Manejador de señales
void signal_handler(int sig) {
    (void)sig;
    printf("\n Deteniendo...\n");
    running = 0;
    pthread_mutex_lock(&mutex);
    pthread_cond_broadcast(&cond);
    pthread_mutex_unlock(&mutex);
}

// Status check
void status_check(int sock, const char *servidor) {
    char resp[200];
    
    pthread_mutex_lock(&mutex);
    int turno = (turno_actual == mi_id);
    int pend = pendientes[mi_id];
    int recv = recibiendo;
    pthread_mutex_unlock(&mutex);
    
    sprintf(resp, "STATUS_OK %s [Turno:%s Recibiendo:%s Pendientes:%d]\n", 
            servidor, turno ? "SI" : "NO", recv ? "SI" : "NO", pend);
    send(sock, resp, strlen(resp), 0);
    
    printf("Turno=%s Recv=%s Pend=%d\n", 
           turno ? "SI" : "NO", recv ? "SI" : "NO", pend);
}

// Procesar archivo
void procesar_archivo(int sock, const char *servidor, const char *buffer) {
    // Esperar turno si no lo tengo
    if (!tengo_turno()) {
        printf("No es mi turno (s0%d), esperando...\n", turno_actual + 1);
        send(sock, "EN_ESPERA\n", 10, 0);
        
        if (!esperar_turno(servidor, TIMEOUT_ESPERA)) {
            send(sock, "ERROR_TIMEOUT\n", 14, 0);
            return;
        }
    }
    
    // Tomar posesión
    pthread_mutex_lock(&mutex);
    recibiendo = 1;
    pthread_mutex_unlock(&mutex);
    
    notificar(servidor, "RECIBIENDO");
    send(sock, "RECIBIENDO\n", 11, 0);
    
    // Extraer datos
    int shift;
    char contenido[TAM_BUFFER];
    
    if (sscanf(buffer, "%d %[^\n]", &shift, contenido) != 2) {
        send(sock, "ERROR_FORMATO\n", 14, 0);
        pthread_mutex_lock(&mutex);
        recibiendo = 0;
        pthread_mutex_unlock(&mutex);
        notificar(servidor, "FINALIZADO");
        return;
    }
    
    // Cifrar
    cifrar_cesar(contenido, shift);
    
    // Guardar
    char dir[100], archivo[256];
    sprintf(dir, "mkdir -p %s", servidor);
    system(dir);
    
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    sprintf(archivo, "%s/archivo_%04d%02d%02d_%02d%02d%02d.txt", 
            servidor, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
            tm.tm_hour, tm.tm_min, tm.tm_sec);
    
    FILE *f = fopen(archivo, "w");
    if (f) {
        fprintf(f, "%s", contenido);
        fclose(f);
        
        printf("Archivo guardado: %s, Shift: %d\n", archivo, shift);
        
        send(sock, "TRANSMITIENDO\n", 14, 0);
        send(sock, "OK Archivo recibido\n", 20, 0);
    } else {
        send(sock, "ERROR_GUARDAR\n", 14, 0);
    }
    
    // Liberar y rotar turno
    pthread_mutex_lock(&mutex);
    recibiendo = 0;
    int anterior = turno_actual;
    turno_actual = (turno_actual + 1) % NUM_SERVIDORES;
    pthread_mutex_unlock(&mutex);
    
    notificar(servidor, "FINALIZADO");
    printf("Turno s0%d -> s0%d\n", anterior + 1, turno_actual + 1);
    pthread_cond_broadcast(&cond);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Uso: %s <s01|s02|s03|s04>\n", argv[0]);
        return 1;
    }
    
    char *servidor = argv[1];
    mi_id = get_id(servidor);
    
    if (mi_id < 0) {
        printf("Error: Servidor inválido\n");
        return 1;
    }
    
    // Crear directorio
    char cmd[100];
    sprintf(cmd, "mkdir -p %s", servidor);
    system(cmd);
    
    printf("=== Servidor %s ===\n", servidor);
    printf("Puerto: %d\n", PUERTO_BASE);
    printf("ID: %d\n", mi_id);
    printf("Control: %d\n", PUERTO_CONTROL + mi_id);
    printf("Turno: %ds\n", TIEMPO_TURNO);
    printf("Timeout: %ds\n\n", TIMEOUT_ESPERA);
    
    // Señales
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Socket TCP
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }
    
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(PUERTO_BASE)
    };
    
    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server_fd);
        return 1;
    }
    
    if (listen(server_fd, MAX_CLIENTES) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }
    
    printf("Escuchando en %d...\n", PUERTO_BASE);
    
    // Crear hilos
    pthread_t th_control, th_gestor;
    pthread_create(&th_control, NULL, hilo_control, servidor);
    pthread_create(&th_gestor, NULL, hilo_gestor, servidor);
    
    while (running) {
        struct sockaddr_in client;
        socklen_t len = sizeof(client);
        
        int client_fd = accept(server_fd, (struct sockaddr*)&client, &len);
        if (client_fd < 0) {
            if (!running) break;
            continue;
        }
        
        pthread_mutex_lock(&mutex);
        pendientes[mi_id] = 1;
        pthread_mutex_unlock(&mutex);
        
        char msg[100];
        sprintf(msg, "s0%d:SOLICITUD_PENDIENTE", mi_id + 1);
        notificar("", msg);
        
        char buffer[TAM_BUFFER] = {0};
        int bytes = recv(client_fd, buffer, TAM_BUFFER - 1, 0);
        
        if (bytes > 0) {
            buffer[bytes] = '\0';
            
            if (strstr(buffer, "STATUS_CHECK")) {
                status_check(client_fd, servidor);
            } else {
                procesar_archivo(client_fd, servidor, buffer);
            }
        }
        
        pthread_mutex_lock(&mutex);
        pendientes[mi_id] = 0;
        pthread_mutex_unlock(&mutex);
        
        close(client_fd);
    }
    
    printf("\nCerrando %s\n", servidor);
    close(server_fd);
    
    return 0;
}
