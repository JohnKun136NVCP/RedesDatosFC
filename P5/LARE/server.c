#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <sys/utsname.h>
#include <netdb.h>
#include <pthread.h>

#define BUFFER_SIZE 2048 // Tama~no del buffer para recibir datos
#define PORT 49200

#define QUANTUM 8 // Lapso de tiempo o quantum otorgado a cada hilo


int turno = 0; // Turnos del hilo actual
int fin_turno = 0;

pthread_mutex_t lock;
pthread_cond_t cond;

struct Server {
	char *alias;
	int servidor;
	int id;
};

void encryptCaesar(char *text, int shift) {
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

// Crea sockets que seran nuestros distintos puertos en el servidor
int creaSockets(int *server_sock, struct sockaddr_in *configuracion, char *alias, int desplazamiento) {
	// Como recibimos un alias, debemos obtener la ip asociado al alias
	//
	struct addrinfo hints, *res;
	char ip_str[INET_ADDRSTRLEN];

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	int status = getaddrinfo(alias, NULL, &hints, &res);
	if (status !=0) {
		perror("Error al obtener la ip del alias");
		exit(1);
	}

	struct sockaddr_in *addr = (struct sockaddr_in*)res->ai_addr;
	inet_ntop(AF_INET, &(addr->sin_addr), ip_str, INET_ADDRSTRLEN);	

	// Configuramos el socket

	*server_sock = socket(AF_INET, SOCK_STREAM, 0);

	int opt = 1;
	setsockopt(*server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	configuracion->sin_family = AF_INET;
	configuracion->sin_addr = addr->sin_addr;
	configuracion->sin_port = htons(PORT);
	freeaddrinfo(res);
	if (bind(*server_sock, (struct sockaddr *)configuracion, sizeof(*configuracion)) < 0) {
		perror("Error al enlazar los sockets del servidor");
		close(*server_sock);
		exit(1);	
	}
	if (listen(*server_sock, 3) <0) {
		perror("Ocurrio un error en la espera de mensajes");
		close(*server_sock);
		exit(1);
	}
}

char* recuperaAlias(char* ip) {
	if (strcmp(ip, "192.168.1.101") == 0){
		return "s01";
	} else if (strcmp(ip, "192.168.1.102") == 0){
		return "s02";
	} else if (strcmp(ip, "192.168.1.103") == 0){
		return "s03";
	} else if (strcmp(ip, "192.168.1.104") == 0){
		return "s04";
	} else {
		return NULL;
	}
}

void* rotaTurno(void* arg){
	int total = *(int*)arg;

	while(1){
		sleep(QUANTUM);
		pthread_mutex_lock(&lock);
		turno = (turno+1) % 4;
		printf("Cambio de turno: %d\n", turno);
		pthread_cond_broadcast(&cond);
		pthread_mutex_unlock(&lock);
	}
	return NULL;
}

// Procesa el mensaje recibido por el cliente, se espera un archivo y un desplazamiento
void* interaccionCliente_Servidor (void *info_server) {
	// Parseamos la estructura
	struct Server *s = (struct Server *)info_server;
	struct sockaddr_in direccion_cliente;
	socklen_t size_cliente = sizeof(direccion_cliente);
	// Comenzamos el algoritmo de Round Robin, lo que nos interesa es saber si es su turno o no, y ver
	// si reciben mensajes o no, de ahi en fuera la ejecucion es la misma.
	while (1) {
		pthread_mutex_lock(&lock);
		while(turno != s->id) {
			pthread_cond_wait(&cond, &lock);
		}
		pthread_mutex_unlock(&lock);

		printf("[Hilo %d - %s] Procesando conexión...\n", s->id, s->alias);
		int client_sock = accept(s->servidor,(struct sockaddr*)&direccion_cliente, &size_cliente );
		if (client_sock < 0) continue;

		char *alias = s->alias;

		char buffer[BUFFER_SIZE];
		send(client_sock, "[Server:] En espera de mensajes...\n", strlen("[Server:] En espera de mensajes...\n"), 0);

		char filename[100];
		// Recibimos el nombre del archivo
		int name = recv(client_sock, filename, sizeof(filename),0);
		if (name < 0) {
			perror("[-] Error al recibir el nombre del archivo");
			close(client_sock);
			exit(1);
		}
		// Recibimos el contenido del archivo
		int mensaje = recv(client_sock, buffer, sizeof(buffer), 0);
		if (mensaje < 0) {
			perror("[-] Error al recibir el mensaje");
			close(client_sock);
			exit(1);
		}
		send(client_sock, "[Server:]Recibiendo mensaje\n", strlen("[Server:]Recibiendo mensaje\n"), 0);
		printf("[+][IP:%s] Mensaje recibido\n", alias);
		char texto[BUFFER_SIZE];
		// Mensaje recibido: Escribimos en el directorio correspondiente:
		char *home = getenv("HOME");
		if (home == NULL) {
			perror("[-] Error con PATH, cerrando servidor");
			exit(1);
		}
		// Checamos el mensaje recibido
		char ruta[1024];
		// Creamos la ruta
		snprintf(ruta, sizeof(ruta), "%s/%s/%s", home,alias, filename);
		// Guardamos el contenido en un archivo
		FILE *output;
		output = fopen(ruta, "w");
		printf("[+][IP:%s]Escribiendo en el archivo\n", alias);

		if (output) {
			fprintf(output, "%s", buffer);
			fclose(output);
		} else {
			printf(ruta);
			perror("Error al escribir en el archivo\n");
			exit(1);
		}
		printf("[+][IP:%s] Archivo guardado\n",alias);
		sleep(1);
		// Enviamos una notificacion al cliente que el mensaje fue recibido
		if (send(client_sock, "[Server:] Archivo guardado\n", strlen("[Server:] Archivo guardado\n"), 0) < 0) {
			perror("[-] Error al enviar el archivo\n");
			close(client_sock);
			exit(1);
		}

		//Terminamos turno
		close(client_sock);

		pthread_mutex_lock(&lock);
		turno = (turno+1) % 4;
		pthread_cond_broadcast(&cond);
		pthread_mutex_unlock(&lock);
	}
	return NULL;
}

// Funcion principal
int main(int argc, char *argv[]) {

	if (argc < 2) {
		perror("Uso: ./server <ALIAS>\nDebe de haber al menos un alias, con un maximo de 4 alias entre s01, s02, s03 y s04");
		exit(1);
	}
	int sockets[argc-1];
	// Inicializamos los hilos
	pthread_t hilos[argc-1];
	int id[argc-1]; // Id de cada proceso
	pthread_mutex_init(&lock, NULL);
	pthread_cond_init(&cond, NULL);
	struct Server *info_server;

	struct sockaddr_in direccion[argc-1];
	
	// Configuramos el socket
	for (int i = 1; i < argc; i++) {
		creaSockets(&sockets[i-1], &direccion[i-1], argv[i], i-1); // Puertos dinamicos
		info_server = malloc(sizeof(struct Server));
		(*info_server).alias = argv[i];
		(*info_server).servidor = sockets[i-1];
		(*info_server).id = i-1;
		pthread_create(&hilos[i-1], NULL,interaccionCliente_Servidor, (void *)info_server);
	}
	int total = argc - 1;
	pthread_t hilo_rotador;
	pthread_create(&hilo_rotador, NULL, rotaTurno, &total);
	
	printf("[-][Server] Servidor escuchando puertos...\n");
	for (int i = 0; i < argc-1; i++) {
		pthread_join(hilos[i], NULL);
	}

	// Terminamos el programa
	pthread_mutex_destroy(&lock);
	pthread_cond_destroy(&cond);

	return 0;
}
