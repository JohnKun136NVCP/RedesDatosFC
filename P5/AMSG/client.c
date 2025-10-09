#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h> // para el log de tiempo
#include <netdb.h>  // para gethostbyname()

#define PUERTO 49200
#define BUFFER_SIZE 1024
const int NUM_MENSAJES = 3;

// mini diccionario de mensajes -> El corazón que ríe - Charles Bukowski
// https://www.culturagenial.com/es/poemas-cortos-y-bonitos-para-dedicar/
const char* MENSAJES[] = {
    "tu vida es tu vida\n"
    "no dejes que sea golpeada contra la húmeda sumisión\n"
    "mantente alerta\n"
    "hay salidas",

    "hay una luz en algún lugar\n"
    "puede que no sea mucha luz pero\n"
    "vence a la oscuridad",

    "mantente alerta\n"
    "los dioses te ofrecerán oportunidades\n"
    "conócelas\n"
    "tómalas",

    "no puedes vencer a la muerte pero\n"
    "puedes vencer a la muerte en la vida, a veces\n"
    "y mientras más a menudo aprendas a hacerlo\n"
    "más luz habrá",

    "tu vida es tu vida\n"
    "conócela mientras la tengas\n"
    "tú eres maravilloso\n"
    "los dioses esperan para deleitarse\n"
    "en tí."
};


// referencia principal: https://stackoverflow.com/questions/29539671/correcting-timestamp-on-c-code-log-file
//se almacena en un archivo que incluye en cada lınea: fecha, hora, estado.
void guardar_estado(char* servidor, char* estado) {
    char dt[20]; 
    struct tm tm;
    time_t current_time;
    FILE *logfd;
    
    current_time = time(NULL);
    tm = *localtime(&current_time);
    strftime(dt, sizeof dt, "%Y-%m-%d %H:%M:%S", &tm);
    
    logfd = fopen("estatus.log", "a");
    if (logfd != NULL) {
        fprintf(logfd, "%s, %s, %s\n", dt, servidor, estado);
        fclose(logfd);
    }
}

int conectar_a_servidor(char *server_ip, char *servidor_alias) {
    int client_sock;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};
    
    client_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (client_sock == -1) {
        perror("[-] Error to create the socket");
        return -1;
    }
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(49200);
    serv_addr.sin_addr.s_addr = inet_addr(server_ip);
    
    if (connect(client_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("[-] Error to connect");
        close(client_sock);
        return -1;
    }
    
    printf("[+] Conectado al servidor\n");
    
    send(client_sock, "ESTATUS", 7, 0);
    
    int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes > 0) {
        buffer[bytes] = '\0';
        printf("[+] Server response: %s\n", buffer);
        guardar_estado(servidor_alias, buffer);
    } else {
        printf("[-] Server connection timeout\n");
        guardar_estado(servidor_alias, "timeout");
    }
    
    close(client_sock);
    return 0;
}

//REFERENCIA: https://www.binarytides.com/hostname-to-ip-address-c-sockets-linux/
int hostname_to_ip(char *hostname, char *ip) {
    struct hostent *he;
    struct in_addr **addr_list;
    
    if ((he = gethostbyname(hostname)) == NULL) {
        herror("gethostbyname");
        return 1;
    }
    
    addr_list = (struct in_addr **)he->h_addr_list;
    strcpy(ip, inet_ntoa(*addr_list[0]));
    return 0;
}

int enviar_mensaje(char *server_ip, char *servidor_alias, char *mensaje) {
    int client_sock;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};
    
    client_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (client_sock == -1) {
        perror("[-] Error to create the socket");
        return -1;
    }
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PUERTO);
    serv_addr.sin_addr.s_addr = inet_addr(server_ip);
    
    if (connect(client_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("[-] Error to connect");
        close(client_sock);
        return -1;
    }
    
    printf("[+] Conectado al servidor %s\n", servidor_alias);

    send(client_sock, mensaje, strlen(mensaje), 0);
    printf("[+] Mensaje enviado: %s\n", mensaje);
    
    // se recibe la confirmacion del servidor
    int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes > 0) {
        buffer[bytes] = '\0';
        printf("[+] Respuesta del servidor: %s\n", buffer);
        
        if (strcmp(buffer, "OK") == 0) {
            printf("[+] Mensaje recibido exitosamente por el servidor\n");
            guardar_estado(servidor_alias, "mensaje_enviado_OK");
        } else if (strcmp(buffer, "ERROR") == 0) {
            printf("[-] Error al procesar el mensaje en el servidor\n");
            guardar_estado(servidor_alias, "mensaje_enviado_ERROR");
        }
    } else {
        printf("[-] Server connection timeout\n");
        guardar_estado(servidor_alias, "timeout");
    }
    
    close(client_sock);
    return 0;
}


int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <ALIAS>\n", argv[0]);
        printf("Ejemplo: %s s01\n", argv[0]);
        return 1;
    }
    
    char *servidor_alias = argv[1];
    char ip[255];
    
    if (hostname_to_ip(servidor_alias, ip) != 0) {
        printf("[-] Error resolviendo alias\n");
        return 1;
    }
    
    printf("[+] Servidor %s resuelto a IP: %s\n", servidor_alias, ip);
    
    // se manda un mensaje aleatorio
    srand(time(NULL));
    int indice_aleatorio = rand() % NUM_MENSAJES;
    char *mensaje_elegido = (char*)MENSAJES[indice_aleatorio];
    
    printf("[+] Mensaje seleccionado aleatoriamente: #%d\n", indice_aleatorio + 1);
    
    enviar_mensaje(ip, servidor_alias, mensaje_elegido);
    
    return 0;
}