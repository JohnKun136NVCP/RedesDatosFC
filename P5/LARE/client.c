#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <time.h>


#define BUFFER_SIZE 2048
#define PORT 49200

// Crea sockets de acuerdo a la cantidad de puertos pedidos por terminal
int creaSockets(int *cliente, struct sockaddr_in *configuracion, char *alias) {
	// Como recibimos un alias, debemos obtener la ip asociada al alias.
	struct addrinfo hints, *res;
	char ip_str[INET_ADDRSTRLEN];

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	int status = getaddrinfo(alias, NULL, &hints, &res);
	if (status !=0) {
		perror("Error al obtener la ip del alias");
		return 1;
	}

	struct sockaddr_in *addr = (struct sockaddr_in*)res->ai_addr;
	inet_ntop(AF_INET, &(addr->sin_addr), ip_str, INET_ADDRSTRLEN);

	*cliente = socket(AF_INET, SOCK_STREAM, 0);
	if (*cliente < 0) {
		perror("Error al crear el socket");
		return 1;
	}
	configuracion->sin_family = AF_INET;
	configuracion->sin_addr = addr->sin_addr;
	configuracion->sin_port = htons(PORT);
	freeaddrinfo(res);
	if (connect(*cliente, (struct sockaddr *)configuracion, sizeof(*configuracion)) < 0) {
		perror("[-] Error con conexion\n");
		printf("[-][Client] Error al conectar al servidor\n");
		close(*cliente);
		return 1;
	}
	printf("[+][Client] Conectado");
	return 0;
}




// Se agrega la peticion del cliente
void guardaLog(const char *status) {
	FILE *output = fopen("log.txt", "a");
	if (output == NULL) {
		perror("Error al intentar abrir el log de cliente");
        	return;
    	}

	// Obtenemos la informacion de tiempo
    	time_t now = time(NULL);
    	struct tm *t = localtime(&now);
    	char time_b[128];
    	strftime(time_b, sizeof(time_b), "%Y-%m-%d %H:%M:%S", t);

    	fprintf(output, "%s - %s\n", time_b, status);
    	printf("[+][Client] %s - %s\n", time_b, status);

    	fclose(output);
}


// Notificamos al cliente de todo lo que esta ocurriendo entre el y el servidor
void logCliente(int client_sock, int bytes, char *buffer, size_t sizebuffer) {
	// Si no hubo errores en el mensaje
	if (bytes > 0) {
		buffer[bytes] = '\0';
		printf("%s", buffer);
	} else {
		printf("[-] Tiempo excedido\n"); // Si hubo un error de conexion o en el mensaje
	}
}


void procesoCliente_Servidor(int *client_sock, char *nombre, char *buffer) {
	// Primer log recibido
	char mensaje[BUFFER_SIZE];
	char *mensaje_log;
	
	int log;

	log = recv(*client_sock, mensaje, sizeof(mensaje) -1, 0);

	if (log < 0) {
		mensaje_log = "ERROR: Fallo con el envio de estado del servidor";
		guardaLog(mensaje_log);
		close(*client_sock);
		exit(1);
	}
	logCliente(*client_sock, log, mensaje, sizeof(mensaje));

	// Preparamos el mensaje y los enviamos al servidor
	
	// Primero enviamos el nombre del archivo
	send(*client_sock, nombre, sizeof(nombre), 0);
	send(*client_sock, buffer, strlen(buffer), 0);	
	printf("[+][Client] El mensaje se ha enviado al servidor\n");

	// Segundo log recibido
	
	memset(mensaje, 0, sizeof(mensaje));

	log = recv(*client_sock, mensaje, sizeof(mensaje) -1, 0);
	if (log < 0) {
		mensaje_log = "ERROR: fallo con el servidor al recibir el envio";
		guardaLog(mensaje_log);
		close(*client_sock);
		exit(1);
	}

	logCliente(*client_sock, log, mensaje, sizeof(mensaje));
	

	// Tercer log recibido
	memset(mensaje, 0, sizeof(mensaje));

	log = recv(*client_sock, mensaje, sizeof(mensaje) -1, 0);
	if (log < 0) {
		mensaje_log = "ERROR: No se pudo notificar al cliente";
		guardaLog(mensaje_log);
		close(*client_sock);
		exit(1);
	}
	logCliente(*client_sock, log, mensaje, sizeof(mensaje));
	
	// Terminamos el programa
	guardaLog("OK");
	close(*client_sock);
	return;
}


int main(int argc, char *argv[]) {
	// Verificamos que recibimos los parametros necesarios para comunicarse con el servidor
	// Se debe especificar el puerto, desplazamiento y archivo a enviar al servidor
	// La ip es unica, se debe separar cada puerto con la bandera -p
	//
	if (argc != 4) {
		printf("Type: %s <SERVIDOR_IP> 49200 <Archivo>\n", argv[0]);
		return 1;
	}
	// Asignamos los diferentes parametros para su posterior uso
	char *alias = argv[1];
	// Inicializamos los datos

	int sockets[3];
	struct sockaddr_in direccion[3];
	socklen_t size_cliente[3];

	char *aliasPosibles[] = {"s01","s02","s03","s04"};
	char *aliasUso[3];
	int acc = 0;

	for (int i = 0; i < 4; i++){
		if (strcmp(alias, aliasPosibles[i]) == 0) {
			continue;
		}
		aliasUso[acc++] = aliasPosibles[i];
	}
	// Preparamos el archivo de lectura
	FILE *input;
	char buffer[BUFFER_SIZE];
	char linea[100];
	input = fopen(argv[3], "r");

	if (input == NULL) {
		perror("Error al leer el archivo");
		return 1;
	}
	buffer[0] = '\0';
	// Leemos el archivo y lo concatenamos al buffer
	while (fgets(linea, sizeof(linea), input)) {
		strcat(buffer, linea);
	}
	fclose(input);
	// Para que el buffer sea una cadena valida
	buffer[BUFFER_SIZE-1] = '\0';

	// Intentamos crear sockets y tratamos de enviar los mensajes al servidor
	for (int i = 0; i < 3; i++) {
		if (creaSockets(&sockets[i], &direccion[i], aliasUso[i]) == 0){
			procesoCliente_Servidor(&sockets[i], argv[3], buffer);
			sleep(1);
		}
	}
}
