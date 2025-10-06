#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 2048

// Funcion para procesar los mensajes recibidos
void recibeMensaje(int client_sock ,int bytes, char *buffer, size_t sizebuffer, int puerto) {
	// Si no hubo errores en el mensaje
	if (bytes > 0) {
		buffer[bytes] = '\0';
		// Verificamos si el servidor nos dio acceso
		if (strstr(buffer, "ACCESS GRANTED") != NULL) {
			printf("[+][Client] Mensaje recibido del puerto %d\n", puerto);
			// Escribimos lo recibido y lo guardamos en un archivo
			char *archivo = "encrypted";
			char nombre_archivo[25];
			snprintf(nombre_archivo, sizeof(nombre_archivo), "%s_%d.txt", archivo, puerto);
			FILE *fp = fopen(nombre_archivo, "w");
			if (fp == NULL) {
				perror("[-] Error al abrir el archivo");
				close(client_sock);
				return;
			}
			while ((bytes = recv(client_sock, buffer, sizebuffer,0)) > 0) {
				fwrite(buffer, 1, bytes, fp);
			}
			printf("[+][Client] El archivo se guardo como '%s'\n", nombre_archivo);
			fclose(fp);
		} else {
			printf("[-] Acceso denegado al servidor %d...\n", puerto);
		}
	} else {
		printf("[-] Tiempo excedido\n"); // Si hubo un error de conexion o en el mensaje
	}
}

// Crea sockets de acuerdo a la cantidad de puertos pedidos por terminal
int creaSockets(int *cliente, struct sockaddr_in *configuracion, int puerto, char *server_ip) {
	*cliente = socket(AF_INET, SOCK_STREAM, 0);
	if (*cliente < 0) {
		perror("Error al crear el socket");
		return 1;
	}
	configuracion->sin_family = AF_INET;
	configuracion->sin_addr.s_addr = inet_addr(server_ip);
	configuracion->sin_port = htons(puerto);
	if (connect(*cliente, (struct sockaddr *)configuracion, sizeof(*configuracion)) < 0) {
		perror("[-] Error con conexion");
		printf("Error al conectar al servidor %d", puerto);
		close(*cliente);
		return 1;
	}
	printf("[+][Client] Conectado con puerto %d\n", puerto);
	return 0;
}

// Funcion que envia los mensajes al servidor y queda a la espera de su respuesta
void enviaMensajes(int *cliente, char *archivo, int shift, int puerto) {
	char buffer[BUFFER_SIZE];
	char linea[100];
	FILE *input;
	input = fopen(archivo, "r");

	if (input == NULL) {
		perror("Error al leer el archivo");
		exit(1);
	}
	buffer[0] = '\0';

	while (fgets(linea, sizeof(linea), input)) {
		strcat(buffer, linea);
	}
	fclose(input);
	buffer[BUFFER_SIZE-1] = '\0';

	char mensaje[BUFFER_SIZE];
	snprintf(mensaje, sizeof(mensaje), "%s\n%d\n%d", buffer, puerto, shift);
	send(*cliente, mensaje, strlen(mensaje), 0);
	printf("[+][Client] Archivo enviado a puerto %d, esperando respuesta...\n", puerto);
	char rcvbuffer[BUFFER_SIZE];
	int bytes = recv(*cliente, rcvbuffer, sizeof(rcvbuffer) - 1, 0);
	recibeMensaje(*cliente, bytes, rcvbuffer, sizeof(rcvbuffer), puerto);

	close(*cliente);
	return;
}

int main(int argc, char *argv[]) {
	// Verificamos que recibimos los parametros necesarios para comunicarse con el servidor
	// Se debe especificar el puerto, desplazamiento y archivo a enviar al servidor
	// La ip es unica, se debe separar cada puerto con la bandera -p
	//
	if (argc < 6 || (argc - 2) % 4) {
		printf("Type: %s <SERVIDOR_IP> -p <PUERTO> <DESPLAZAMIENTO> <Archivo> -p <PUERTO1> <DESP..1> <Archivo1> -p ...\nDebes separar cada puerto con la bandera -p", argv[0]);
		return 1;
	}
	// Asignamos los diferentes parametros para su posterior uso
	char *server_ip = argv[1];
	int socket_acc = (argc - 2) / 4;
	// Inicializamos los datos
	int puertos[socket_acc];
	int desplazamientos[socket_acc];
	char *archivos[socket_acc];

	int sockets[socket_acc];
	struct sockaddr_in direccion[socket_acc];
	socklen_t size_cliente[socket_acc];

	// Asignamos los valores de acuerdo a su posicion para su posterior uso
	for (int i = 2, j = 0; i < argc; i += 4, j++) {
		puertos[j] = atoi(argv[i+1]);
		desplazamientos[j] = atoi(argv[i+2]);
		if (puertos[j] == 0 || desplazamientos[j] == 0) {
			perror("Type: %s <SERVIDOR_IP> -p <PUERTO> <DESPLAZAMIENTO> <Archivo> -p ...\nEl puerto y desplazamiento deben ser numeros, sigue la estructura planteada");
			return 1;
		}
		archivos[j] = argv[i+3];
	}
	// Intentamos crear sockets y tratamos de enviar los mensajes al servidor
	for (int i = 0; i < socket_acc; i++) {
		if (creaSockets(&sockets[i], &direccion[i], puertos[i], server_ip) == 0){
			enviaMensajes(&sockets[i], archivos[i], desplazamientos[i], puertos[i]);
		}
	}
}
