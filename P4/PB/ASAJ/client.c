// cliente.c
#include <stdio.h>     
#include <stdlib.h>   
#include <string.h>     
#include <unistd.h>    
#include <arpa/inet.h> 
#include <time.h>  
#include <netdb.h> // lo usamos para que podamos obtener la IP correspondiente a s01/2/3/4     

#define TAM_BUFFER 65536         // tamaño del buffer para comunicación, lo agrandamos por si toca un .txt largo en el randomized del script

// función para registrar eventos en la terminal y en archivo log
void registrar_evento(const char *estado) {
    FILE *f = fopen("cliente.log", "a");    // crea un archivo cliente.log en modo append
    if (!f) return;

    time_t ahora = time(NULL);              // obtenemos la hora actual para llevar registro
    char marca_tiempo[64];
    strftime(marca_tiempo, sizeof(marca_tiempo), "%Y-%m-%d %H:%M:%S", localtime(&ahora));
    
    fprintf(f, "[%s] %s\n", marca_tiempo, estado);   // escribimos en el archivo
    fclose(f);

    printf("[%s] %s\n", marca_tiempo, estado);      // también lo mostramos en la terminal
}

// función que se encarga de la creación de un socket y la conexión al servidor
int conectar_a(const char *host, int puerto) {
    int socket_cliente;
    struct sockaddr_in dir_servidor;
    struct hostent *servidor;

    // crear socket
    if ((socket_cliente = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Error al crear socket del cliente");
        exit(1);
    }

    // resolver nombre de host (sirve para alias tipo s01/2/3/4 o IP directa)
    servidor = gethostbyname(host);
    if (servidor == NULL) {
        fprintf(stderr, "Error: no se pudo resolver el host %s\n", host);
        exit(1);
    }

    memset(&dir_servidor, 0, sizeof(dir_servidor));
    dir_servidor.sin_family = AF_INET;
    dir_servidor.sin_port = htons(puerto);
    memcpy(&dir_servidor.sin_addr, servidor->h_addr, servidor->h_length);

    // ahora sí conectar
    if (connect(socket_cliente, (struct sockaddr*)&dir_servidor, sizeof(dir_servidor)) < 0) {
        perror("Error al conectar con el servidor");
        exit(1);
    }

    return socket_cliente;
}

// función auxiliar que lee archivo para enviar 
int leer_archivo(const char *nombre, char *buffer, int cap) {
    FILE *f = fopen(nombre, "r");
    if (!f) return 0;
    int len = fread(buffer, 1, cap - 1, f);
    buffer[len] = '\0';
    fclose(f);
    return len;
}

int main(int argc, char *argv[]) {

	if(argc < 4){
		fprintf(stderr, "Argumentos: %s <IP_SERVIDOR>, <PUERTO_BASE)>, <ARCHIVO>\n", argv[0]);
		exit(1);
	}
	// De los argumentos chupamos la IP, puerto y nombre de archivo
	const char *IP_SERVIDOR = argv[1];	
	int PUERTO_BASE = atoi(argv[2]);	 
	const char *ARCHIVO = argv[3];		

    char buffer[TAM_BUFFER];   

    registrar_evento("En espera - conectando al servidor en puerto base");

    // conectamos a lo que nos pasaron
    int socket_actual = conectar_a(IP_SERVIDOR, PUERTO_BASE);

    // recibimos el puerto que el servidor nos asignó
    int n = recv(socket_actual, buffer, sizeof(buffer)-1, 0);
    if (n <= 0) {
        perror("No se recibió puerto asignado");
        exit(1);
    }
    
    buffer[n] = '\0';
    int puerto_nuevo = atoi(buffer);  // convertimos el puerto a número
    close(socket_actual);             // cerramos el socket con el puerto base

    sleep(2);  // pausa para que al servidor le de chance de preparar el nuevo socket

    char mensaje_log[128];
    snprintf(mensaje_log, sizeof(mensaje_log), "Recibido nuevo puerto: %d", puerto_nuevo);
    registrar_evento(mensaje_log);

    // nos conectamos al puerto asignado
    registrar_evento("Conectando al puerto asignado");
    socket_actual = conectar_a(IP_SERVIDOR, puerto_nuevo);
	
    // enviamos archivo
    static char buffer_archivo[TAM_BUFFER];
    int len = leer_archivo(ARCHIVO, buffer_archivo, sizeof(buffer_archivo));
    send(socket_actual, buffer_archivo, len, 0);

	// cerramos conexión para que el servidor sepa que terminamos y guarde lo que le mandamos
	shutdown(socket_actual, SHUT_WR);
	
	snprintf(mensaje_log, sizeof(mensaje_log), "Archivo '%s' enviado al servidor", ARCHIVO);
    registrar_evento(mensaje_log);

    // esperamos respuesta del servidor
    n = recv(socket_actual, buffer, sizeof(buffer)-1, 0);
    if (n > 0) {
        buffer[n] = '\0';
        registrar_evento("Recibiendo respuesta del servidor");
        printf("Respuesta: %s\n", buffer);
    }

    // concluímos la conexión
    close(socket_actual);
    registrar_evento("Conexión cerrada");

    return 0;
}
