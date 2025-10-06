#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#define PUERTO 49200
#define BUFFER_SIZE 1024

char servidor_estado[20] = "esperando";

char* obtener_estado_servidor() {
    return servidor_estado;
}

char* identificar_alias_cliente(struct sockaddr_in client_addr) {
    char* client_ip = inet_ntoa(client_addr.sin_addr);
    
    // IMPORTANTE: tuve que cambiar las alias IPs 92.168.0.10X a 92.168.0.10X
    // porque no me hacia el ping con la que tiene 0
    if(strcmp(client_ip, "192.168.1.101") == 0){
        return "s01";
    }else if (strcmp(client_ip, "192.168.1.102") == 0) {
        return "s02";
    }else if (strcmp(client_ip, "127.0.0.1") == 0) {
        return "localhost";
    }else {
        return "desconocido";
    }
}

void obtener_info_cliente(int client_sock, char* client_info) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    
    // https://pubs.opengroup.org/onlinepubs/007904875/functions/getpeername.html
    
    if (getpeername(client_sock, (struct sockaddr*)&client_addr, &addr_len) == 0) {
        char* client_ip = inet_ntoa(client_addr.sin_addr);
        int client_port = ntohs(client_addr.sin_port);
        char* alias = identificar_alias_cliente(client_addr);
        
        snprintf(client_info, BUFFER_SIZE, "IP: %s, Puerto: %d, Alias: %s", 
                 client_ip, client_port, alias);
    } else {
        strcpy(client_info, "Cliente desconocido");
    }
}

// FUNCION para procesar peticion de un cliente
void procesar_cliente(int client_sock) {
    char buffer[BUFFER_SIZE] = {0};
    char client_info[BUFFER_SIZE];
    
    obtener_info_cliente(client_sock, client_info);
    printf("[+] Cliente conectado desde: %s\n", client_info);
    
    int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes <= 0) {
        printf("[-] NO hay datos recibiendose del cliente\n");
        close(client_sock);
        return;
    }
    
    buffer[bytes] = '\0';
    printf("[+] Peticion del cliente: %s\n", buffer);
    
    strcpy(servidor_estado, "recibiendo"); // en lo que se procesa la peticion
    
    char* estado_actual = obtener_estado_servidor();
    send(client_sock, estado_actual, strlen(estado_actual), 0);
    printf("[+] Estado actual: %s\n", estado_actual);
    
    strcpy(servidor_estado, "esperando"); // se regresa al estadod espera tras procesar la peticion
    
    close(client_sock);
}



int main() {
	int server_sock, client_sock;
	struct sockaddr_in server_addr, client_addr;
	socklen_t addr_size;
	char buffer[BUFFER_SIZE] = {0};
	
	server_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (server_sock == -1) {
		perror("[-] Error to create the socket");
		return 1;
	}
	
	//referencia para usar SO_REASADOR: https://stackoverflow.com/questions/24194961/how-do-i-use-setsockoptso-reuseaddr
	// QUE ES PARA usar un puerto que ya estaba en uso
        int opt = 1;
	if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
		perror("[-] Error setsockopt SO_REUSEADDR");
		close(server_sock);
		return 1;
	}
	
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PUERTO);
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
	
	printf("[+] Server listening port %d...\n", PUERTO);
	addr_size = sizeof(client_addr);
	
	while(1){
		// se asigna el puerto de froma automatica
		client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_size);
		if(client_sock < 0){
			perror("[-] Error en accept");
			continue;
		}
		
		printf("[+] Client connected\n");
		
		procesar_cliente(client_sock);
	}
		
	/*
	//client_sock = accept(server_sock, (struct sockaddr *)&client_addr,&addr_size);
	//if (client_sock < 0) {
	//	perror("[-] Error on accept");
	//	close(server_sock);
	//	return 1;
	//}
	//printf("[+] Client conneted\n");
	int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
	if (bytes <= 0) {
		printf("[-] No data received\n");
		close(client_sock);
		close(server_sock);
		return 1;
	}
	buffer[bytes] = '\0';*/
	

	close(server_sock);
	return 0;
}
