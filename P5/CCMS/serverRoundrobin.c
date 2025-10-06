#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <time.h> //Manejo de tiempo para timestamps
#include <sys/stat.h>//Para hacer directorio
#include <sys/types.h>
#include <netdb.h>//Resolucion para que reconozca Alias
#include <pthread.h>//Hilos en c.

#define PORT 49200 // Puerto de conexion.
#define BUFFER_SIZE 1024 // Tamaño del buffer para recibir datos
#define TIEMPO_TURNO 8   // segundos por turno

// Turno mutex
int turno = 0;                 
pthread_mutex_t lock;          
pthread_cond_t cond;           

typedef struct {
    int id;           
    char *alias;      
} servidor_t;

int server_sock;  // Socket unico de escucha

//Funcion de encriptado cesar.
void caesarCrypt(char *text, int shift) {
    shift = shift % 26;
    for (int i = 0; text[i] != '\0'; i++) {
        char c = text[i];
        if (isupper(c)) {
            text[i] = ((c - 'A' + shift) % 26) + 'A';
        } else if (islower(c)) {
            text[i] = ((c - 'a' + shift) % 26) + 'a';
        } else {
            text[i] = c;
        }
    }
}

// Funcion de recepcion por servidor.
void *servidor_thread(void *arg) {
    servidor_t *srv = (servidor_t *)arg;
    char buffer[BUFFER_SIZE];

    while (1) {
        // Esperar turno
        pthread_mutex_lock(&lock);
        while (turno != srv->id) {
            pthread_cond_wait(&cond, &lock);
        }
        pthread_mutex_unlock(&lock);

        printf("[SERVIDOR %s] Ocupando turno\n", srv->alias);

        // Aceptar una conexión en este turno (bloquea hasta que llegue cliente)
        struct sockaddr_in client_addr;
        socklen_t addr_size = sizeof(client_addr);
        int client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &addr_size);

        if (client_sock >= 0) {
            int bytes = recv(client_sock, buffer, BUFFER_SIZE-1, 0);
            if (bytes > 0) {
                buffer[bytes] = '\0';
                int shift; char texto[BUFFER_SIZE];
                sscanf(buffer, "%d %[^\n]", &shift, texto);
                caesarCrypt(texto, shift);

                // Guardar archivo en carpeta del alias (srv->alias)
                char dirpath[256], filename[512];
                char *home = getenv("HOME");
                if (!home) home = "/tmp";
                snprintf(dirpath, sizeof(dirpath), "%s/%s", home, srv->alias);
                mkdir(dirpath, 0755);

                // archivo con timestamp
                time_t now = time(NULL);
                struct tm *t = localtime(&now);
                snprintf(filename, sizeof(filename),
                         "%s/file_%04d%02d%02d_%02d%02d%02d.txt",
                         dirpath, t->tm_year+1900, t->tm_mon+1, t->tm_mday,
                         t->tm_hour, t->tm_min, t->tm_sec);

                FILE *fp = fopen(filename, "w");
                if (fp) {
                    fputs(texto, fp);
                    fclose(fp);
                }
                send(client_sock, "ARCHIVO CIFRADO RECIBIDO", 26, 0);
                printf("[SERVIDOR %s] Archivo guardado como %s\n", srv->alias, filename);
            }
            close(client_sock);
        }
    }
    return NULL;
}

// Planificador round robin
void *planificador_thread(void *arg) {
    int num_alias = *(int*)arg;
    while (1) {
        sleep(TIEMPO_TURNO);
        pthread_mutex_lock(&lock);
        turno = (turno + 1) % num_alias;
        pthread_cond_broadcast(&cond);
        pthread_mutex_unlock(&lock);
    }
    return NULL;
}



//Funcion main.
int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <alias1> <alias2> ...\n", argv[0]);
        exit(1);
    }
    
    //Deteccion de alias
    int num_alias = argc - 1;
    pthread_t hilos[num_alias], planificador;
    servidor_t servidores[num_alias];

    pthread_mutex_init(&lock, NULL);
    pthread_cond_init(&cond, NULL);

    int sockets[num_alias];

    // Crear socket unico de escucha
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            perror("bind");
            exit(1);
    }
    listen(server_sock, 5);
    printf("[+] Servidor escuchando en puerto %d\n", PORT);

    // Crear hilos
    for (int i = 0; i < num_alias; i++) {
        servidores[i].id = i;
        servidores[i].alias = argv[i+1];
        pthread_create(&hilos[i], NULL, servidor_thread, &servidores[i]);
    }

    // Crear planificador
    pthread_create(&planificador, NULL, planificador_thread, &num_alias);

    // Cerrar hilos con join
    for (int i = 0; i < num_alias; i++) 
        pthread_join(hilos[i], NULL);
    pthread_join(planificador, NULL);

    return 0;
}
