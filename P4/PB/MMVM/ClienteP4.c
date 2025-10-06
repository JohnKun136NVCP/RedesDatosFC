// Cliente P4.c (Parte B)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>

#define SERVER_PORT 49200

// ----- Función para conectar con un servidor -----
void contact_server(const char* my_name, const char* target_hostname) {

    struct hostent *host;
    struct sockaddr_in serv_addr;

    // Primero, traducimos de hostname a una IP
    host = gethostbyname(target_hostname);
    if (host == NULL) {
        fprintf(stderr, "[%s] -> No se pudo resolver el hostname '%s'\n", my_name, target_hostname);
        return;
    }

    // Crear socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        fprintf(stderr, "[%s] -> No se pudo crear socket para %s\n", my_name, target_hostname);
        return;
    }

    // Preparar la dirección del server a la que nos vamos a conectar
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);
    // Usamos la ip resuktante
    memcpy(&serv_addr.sin_addr, host->h_addr_list[0], host->h_length);

    // Intentamos establecer la conexión con el servidor
    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        fprintf(stderr, "[%s] -> Falló la conexión a %s (%s)\n", my_name, target_hostname, inet_ntoa(serv_addr.sin_addr));
        close(sock);
        return;
    }

    printf("[%s] -> Conexión exitosa con %s\n", my_name, target_hostname);
    close(sock);
}


// ----- main -----
int main(int argc, char *argv[]) {

    // Solo un alias
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <mi_alias>\nEjemplo: %s s01\n", argv[0], argv[0]);
        return 1;
    }

    // Guardar alias
    const char *my_alias = argv[1];
    // Lista de todos los servidores 
    const char *all_servers[] = {"s01", "s02", "s03", "s04"};
    int num_all_servers = sizeof(all_servers) / sizeof(all_servers[0]);

    printf("Cliente de estado '%s' contactando a otros servidores...\n", my_alias);

    // Vamos a recorrer la lista de los servidores
    for (int i = 0; i < num_all_servers; i++) {
        if (strcmp(my_alias, all_servers[i]) != 0) {
	    // Nos conectamos si todo chido
            contact_server(my_alias, all_servers[i]);
        }
    }

    return 0;
}
// El fin :)
mmvm@mmvm-servid
