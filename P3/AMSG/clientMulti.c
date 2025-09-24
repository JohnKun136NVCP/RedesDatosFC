// Ubicacion en el cliente: amsg@AMSG:~/sockets$ cat clientMulti.c 

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define PORT 7006
#define BUFFER_SIZE 1024

// PARA ESTA FUNCION TOME  LO QUE ESTABA EN EL main orignal, pero convienen en funcion para crar avrios sockets
int conectar_a_servidor(char *server_ip, int puerto, int desplazamiento, char *contenido_archivo) {
	// 1 -> SE CONECTA AL SERVIDOR DADO UN PUERTO
    int client_sock;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};
    char mensaje[BUFFER_SIZE];
    client_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (client_sock == -1) {
        perror("[-] Error to create the socket");
        return -1;
    }
    serv_addr.sin_family = AF_INET;
    //serv_addr.sin_port = htons(PORT);
    serv_addr.sin_port = htons(puerto);
    serv_addr.sin_addr.s_addr = inet_addr(server_ip);
    
    if (connect(client_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("[-] Error to connect");
        close(client_sock);
        return 1;
    }
    printf("[+] COnectado al servidor en el puerto %d\n", puerto);
    
    //snprintf(mensaje, sizeof(mensaje), "%d %d %s", puerto, desplazamiento, contenido_archivo);
    //send(client_sock, mensaje, strlen(mensaje), 0);
    //printf("[+][Client] Data sent to port %d: puerto=%d, shift=%d\n", puerto, puerto, desplazamiento);
    
    snprintf(mensaje, sizeof(mensaje), "%d %d %s", puerto, desplazamiento, contenido_archivo);
    send(client_sock, mensaje, strlen(mensaje), 0);
    printf("[+][Client] Data sent to port %d: CON desplazamiento=%d\n", puerto, desplazamiento);
    
    int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes > 0) {
        buffer[bytes] = '\0';
        printf("[+][Client] Server response from port %d: %s\n", puerto, buffer);
        
	if (strstr(buffer, "PROCESADO") != NULL) {
            printf("[+] Archivo PROCESADO exitosamente por servidor en puerto %d\n", puerto);
        } else if (strstr(buffer, "RECHAZADO") != NULL) {
            printf("[-] Archivo RECHAZADO!! -> por servidor en el puerto %d\n", puerto);
        }
        /*
        if (strstr(buffer, "PROCESADO") != NULL) {
            char nombre_archivo[100];
            snprintf(nombre_archivo, sizeof(nombre_archivo), "resultado_puerto_%d.txt", puerto);
            
            FILE *fp = fopen(nombre_archivo, "w");
            if (fp == NULL) {
                perror("[-] Error to open the file");
                close(client_sock);
                return -1;
            }
            
            while ((bytes = recv(client_sock, buffer, sizeof(buffer), 0)) > 0) {
                fwrite(buffer, 1, bytes, fp);
            }
            printf("[+][Client] File saved as '%s'\n", nombre_archivo);
            fclose(fp); */
    } else {
        printf("[-] Server connection timeout on port %d\n", puerto);
    }
    
    close(client_sock);
    return 0;
}

int main(int argc, char *argv[]) {
	if (argc < 5) {
		printf("Type: %s <IP> <PUERTO1> <PUERTO2> ... <PUERTOn> <DESPLAZAMIENTO> <archivo.txt>\n", argv[0]);
		printf("EJEMPLO: %s 192.168.1.197 49200 49201 49202 46 evento.txt\n", argv[0]);
		//printf("Type: %s <key> <shift>\n", argv[0]);
		return 1;
	}
	//char *clave = argv[1];
	//char *shift = argv[2];
	
	char *servidor_ip = argv[1];
	
	int num_puertos = argc - 4; // total de puertos - IP, desplazamiento y archivo
	int puertos[num_puertos];
	
	for (int i = 0; i < num_puertos; i++) {
		puertos[i] = atoi(argv[2 + i]);
		printf("PUERTO %d: %d\n", i+1, puertos[i]);
	}
	
	// Los ULTIMOS dos son desplazamineto y el archivo!
	int desplazamiento = atoi(argv[argc-2]);
    	char *archivo = argv[argc-1];
    	
    	FILE *fp = fopen(archivo, "r");
    	if (fp == NULL) {
    	    perror("[-] Error al abrir el archivo");
    	    return 1;
    	}
    	
    	fseek(fp, 0, SEEK_END);
    	long tamaño = ftell(fp);
    	fseek(fp, 0, SEEK_SET);
    	
    	char *contenido_archivo = malloc(tamaño + 1);
    	fread(contenido_archivo, 1, tamaño, fp);
    	contenido_archivo[tamaño] = '\0';
    	fclose(fp);
    	
    	for (int i = 0; i < num_puertos; i++) {
    	    conectar_a_servidor(servidor_ip, puertos[i], desplazamiento, contenido_archivo);
    	}
    	
    	free(contenido_archivo);
	return 0;
}


