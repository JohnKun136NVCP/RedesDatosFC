
// Ubicacion en el servidor: amsg@amsg:~/sockets$ cat serverOpt.c

// REFRENCIA PRINCIPAL DE DONDE ME BASE para crear multiples puertos: https://www.geeksforgeeks.org/cpp/socket-programming-in-cc-handling-multiple-clients-on-server-without-multi-threading/

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
#include <unistd.h> //close
#include <arpa/inet.h> //close
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/time.h> //FD_SET, FD_ISSET, FD_ZERO macros
#define TRUE 1
#define FALSE 0
#define PORT1 49200
#define PORT2 49201
#define PORT3 49202

//#define PORT 7006 // Puerto en el que el servidor escucha
#define BUFFER_SIZE 1024 // Tama~no del buffer para recibir datos

void decryptCaesar(char *text, int shift) {
	shift = shift % 26;
	for (int i = 0; text[i] != '\0'; i++) {
		char c = text[i];
		if (isupper(c)) {
			text[i] = ((c - 'A' - shift + 26) % 26) + 'A';
		} else if (islower(c)) {
			text[i] = ((c - 'a' - shift + 26) % 26) + 'a';
		} else {
			text[i] = c;
		}
	}
}

// solo se modifica la funcion del codigo original
// se cambia que se sume el shift en vez de que se reste y ya
void ecryptCaesar(char *text, int shift) {
	shift = shift % 26; // alfabeto de 26 letras
	for (int i = 0; text[i] != '\0'; i++) { 
		char c = text[i]; // los string son arreglos de char
		if (isupper(c)) {
			text[i] = ((c - 'A' + shift + 26) % 26) + 'A';
		} else if (islower(c)) {
			text[i] = ((c - 'a' + shift + 26) % 26) + 'a';
		} else {
			text[i] = c;
		}
	}
}


void procesar_por_cliente(int client_socket, int mi_puerto) {
    char buffer[1025];
    int puerto_objetivo, desplazamiento;
    char contenido_archivo[1025];
    
    // se reciben datos del cliente por el socket
    int bytes = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes <= 0) {
        close(client_socket);
        return;
    }
    
    buffer[bytes] = '\0';
    sscanf(buffer, "%d %d %s", &puerto_objetivo, &desplazamiento, contenido_archivo);
    
    printf("[Servidor puerto %d] Recibido: puerto_objetivo=%d, desplazamiento=%d\n", 
           mi_puerto, puerto_objetivo, desplazamiento);
    
    // verificar si el puerto coincide y si lo hace se encripta con Cesar
    if (puerto_objetivo == mi_puerto) {
        ecryptCaesar(contenido_archivo, desplazamiento);
        printf("[Servidor puerto %d] Archivo PROCESADO y cifrado\n", mi_puerto);
        send(client_socket, "PROCESADO", strlen("PROCESADO"), 0);
    } else {
        printf("[Servidor puerto %d] Archivo RECHAZADO (puerto no coincide)\n", mi_puerto);
        send(client_socket, "RECHAZADO", strlen("RECHAZADO"), 0);
    }
    
    close(client_socket);
}


int main(int argc, char* argv[]) {

// https://www.geeksforgeeks.org/cpp/socket-programming-in-cc-handling-multiple-clients-on-server-without-multi-threading/
// Example code: A simple server side code, which echos back
// the received message. Handle multiple socket connections
// with select and fd_set on Linux

    int opt = TRUE;
    int master_socket1, master_socket2, master_socket3, addrlen, new_socket,
        activity, i, valread;

    int max_sd;
    struct sockaddr_in address1, address2, address3, client_addr;

    char buffer[1025]; // data buffer of 1K
    // set of socket descriptors
    fd_set readfds;

    //*char* message = "ECHO Daemon v1.0 \r\n";*/

    /*for (i = 0; i < max_clients; i++) {
        client_socket[i] = 0;
    }*/

    // SE NECESITAN 3 sockets maestros (uno por puerto)
    if ((master_socket1 = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed -> PUERTO 49200");
        exit(EXIT_FAILURE);
    }
    
    if ((master_socket2 = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed -> PUERTO 49201");
        exit(EXIT_FAILURE);
    }
    
    if ((master_socket3 = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed -> PUERTO 49202");
        exit(EXIT_FAILURE);
    }

    // set master socket to allow multiple connections ,
    // this is just a good habit, it will work without this
    if (setsockopt(master_socket1, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt)) < 0) {
        perror("setsockopt -> PUERTO 49200");
        exit(EXIT_FAILURE);
    }
    if (setsockopt(master_socket2, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt)) < 0) {
        perror("setsockopt -> PUERTO 49201");
        exit(EXIT_FAILURE);
    }
    if (setsockopt(master_socket3, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt)) < 0) {
        perror("setsockopt -> PUERTO 49202");
        exit(EXIT_FAILURE);
    }

    // PARA CADA PUERTO: type of socket created
    address1.sin_family = AF_INET;
    address1.sin_addr.s_addr = INADDR_ANY;
    address1.sin_port = htons(49200);
    
    address2.sin_family = AF_INET;
    address2.sin_addr.s_addr = INADDR_ANY;
    address2.sin_port = htons(49201);
    
    address3.sin_family = AF_INET;
    address3.sin_addr.s_addr = INADDR_ANY;
    address3.sin_port = htons(49202);

    // PARA CADA PUERTO: bind the socket to localhost port
    if (bind(master_socket1, (struct sockaddr*)&address1, sizeof(address1)) < 0) {
        perror("bind failed -> PUERTO 49200");
        exit(EXIT_FAILURE);
    }
    if (bind(master_socket2, (struct sockaddr*)&address2, sizeof(address2)) < 0) {
        perror("bind failed -> PUERTO 49201");
        exit(EXIT_FAILURE);
    }
    if (bind(master_socket3, (struct sockaddr*)&address3, sizeof(address3)) < 0) {
        perror("bind failed -> PUERTO 49202");
        exit(EXIT_FAILURE);
    }


	// listen para cada puerto
    if (listen(master_socket1, 3) < 0) {
        perror("listen puerto 49200");
        exit(EXIT_FAILURE);
    }
    if (listen(master_socket2, 3) < 0) {
        perror("listen puerto 49201");
        exit(EXIT_FAILURE);
    }
    if (listen(master_socket3, 3) < 0) {
        perror("listen puerto 49202");
        exit(EXIT_FAILURE);
    }

    // accept the incoming connection
    addrlen = sizeof(client_addr);
    puts("Waiting for connections ...");

    while (TRUE) {
        // clear the socket set
        FD_ZERO(&readfds);

        FD_SET(master_socket1, &readfds);
        FD_SET(master_socket2, &readfds);
        FD_SET(master_socket3, &readfds);
        
        max_sd = master_socket1;
        
        if (master_socket2 > max_sd){
         	max_sd = master_socket2;
        }
        if (master_socket3 > max_sd){
         	max_sd = master_socket3;
        }

        // wait for an activity on one of the sockets
        activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
        
                // add child sockets to set (no clientes persistentes)
        /*for (i = 0; i < max_clients; i++) {
            sd = client_socket[i];
            if (sd > 0)
                FD_SET(sd, &readfds);
            if (sd > max_sd)
                max_sd = sd;
        }*/

        if ((activity < 0) && (errno != EINTR)) {
            printf("select error");
        }

        // verificar actividad en los 3 sockets maestros
        
        if (FD_ISSET(master_socket1, &readfds)) {
            if ((new_socket = accept(master_socket1, (struct sockaddr*)&client_addr, (socklen_t*)&addrlen)) < 0) {
                perror("accept puerto 49200");
                exit(EXIT_FAILURE);
            }
            printf("Nueva conexion en puerto 49200, socket fd: %d\n", new_socket);
            procesar_por_cliente(new_socket, 49200);
        }
        
        if (FD_ISSET(master_socket2, &readfds)) {
            if ((new_socket = accept(master_socket2, (struct sockaddr*)&client_addr, (socklen_t*)&addrlen)) < 0) {
                perror("accept puerto 49201");
                exit(EXIT_FAILURE);
            }
            printf("Nueva conexion en puerto 49201, socket fd: %d\n", new_socket);
            procesar_por_cliente(new_socket, 49201);
        }
        
        if (FD_ISSET(master_socket3, &readfds)) {
            if ((new_socket = accept(master_socket3, (struct sockaddr*)&client_addr, (socklen_t*)&addrlen)) < 0) {
                perror("accept puerto 49202");
                exit(EXIT_FAILURE);
            }
            printf("Nueva conexion en puerto 49202, socket fd: %d\n", new_socket);
            procesar_por_cliente(new_socket, 49202);
        }



        }

	return 0;
}




/*
int main() {
	int server_sock, client_sock;
	struct sockaddr_in server_addr, client_addr;
	socklen_t addr_size;
	char buffer[BUFFER_SIZE] = {0};
	char clave[BUFFER_SIZE];
	int shift;
	server_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (server_sock == -1) {
		perror("[-] Error to create the socket");
		return 1;
	}
	server_addr.sin_family = AF_INET;
	//server_addr.sin_port = htons(PORT);
	server_addr.sin_addr.s_addr = INADDR_ANY;
	if (bind(server_sock, (struct sockaddr *)&server_addr,sizeof(server_addr))
	< 0) {
		perror("[-] Error binding");
		close(server_sock);
		return 1;
	}
	if (listen(server_sock, 1) < 0) {
		perror("[-] Error on listen");
		close(server_sock);
		return 1;
	}
	printf("[+] Server listening port %d...\n", PORT);
	addr_size = sizeof(client_addr);
	client_sock = accept(server_sock, (struct sockaddr *)&client_addr,&addr_size);
	if (client_sock < 0) {
		perror("[-] Error on accept");
		close(server_sock);
		return 1;
	}
	printf("[+] Client conneted\n");
	int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
	if (bytes <= 0) {
		printf("[-] Missed key\n");
		close(client_sock);
		close(server_sock);
		return 1;
	}
	buffer[bytes] = '\0';
	//sscanf(buffer, "%s %d", clave, &shift); // extrae clave y desplazamiento
	printf("[+][Server] Encrypted key obtained: %s\n", clave);
	if (isOnFile(clave)) {
		decryptCaesar(clave, shift);
		printf("[+][Server] Key decrypted: %s\n", clave);
		send(client_sock, "ACCESS GRANTED", strlen("ACCESS GRANTED"), 0);
		sleep(1); // Peque~na pausa para evitar colisi´on de datos
		saveNetworkInfo("network_info.txt");

		saveInfo("sysinfo.txt");
	
		sendFile("network_info.txt", client_sock);
		printf("[+] Sent file\n");
	} else {
		send(client_sock, "ACCESS DENIED", strlen("ACCESS DENIED"), 0);
		printf("[-][Server] Wrong Key\n");
	}
	close(client_sock);
	close(server_sock);
	return 0;
}*/






