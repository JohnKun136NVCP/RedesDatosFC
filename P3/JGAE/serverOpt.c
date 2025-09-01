#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#define PORT 7006 // Puerto en el que el servidor escucha
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

void saveNetworkInfo(const char *outputFile) {
    FILE *fpCommand;
    FILE *fpOutput;
    char buffer[512];
    // Ejecutar comando para obtener información de red
    fpCommand = popen("ip addr show", "r");
    if (fpCommand == NULL) {
        perror("Error!");
        return;
    }
    // Abrir archivo para guardar la salida
    fpOutput = fopen(outputFile, "w");
    if (fpOutput == NULL) {
        perror("[-] Error to open the file");
        pclose(fpCommand);
        return;
    }
    // Leer línea por línea y escribir en el archivo
    while (fgets(buffer, sizeof(buffer), fpCommand) != NULL) {
        fputs(buffer, fpOutput);
    }
    // Cerrar ambos archivos
    fclose(fpOutput);
    pclose(fpCommand);
}

void sendFile(const char *filename, int sockfd) {
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        perror("[-] Cannot open the file");
        return;
    }
    char buffer[BUFFER_SIZE];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        if (send(sockfd, buffer, bytes, 0) == -1) {
            perror("[-] Error to send the file");
            break;
        }
    }
    fclose(fp);
}

// Función para convertir a minúsculas
void toLowerCase(char *str) {
    for (int i = 0; str[i]; i++)
        str[i] = tolower((unsigned char)str[i]);
}

// Función para eliminar espacios al inicio y final
void trim(char *str) {
    char *end;
    while (isspace((unsigned char)*str)) str++; // inicio
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--; // final
    *(end + 1) = '\0';
}

bool isOnFile(const char *bufferOriginal) {
    FILE *fp;
    char line[BUFFER_SIZE];
    char buffer[BUFFER_SIZE];
    bool foundWorld = false;
    // Copiamos el buffer original para poder modificarlo
    strncpy(buffer, bufferOriginal, BUFFER_SIZE);
    buffer[BUFFER_SIZE - 1] = '\0'; // aseguramos terminación
    trim(buffer);
    toLowerCase(buffer);
    fp = fopen("cipherworlds.txt", "r");
    if (fp == NULL) {
        printf("[-]Error opening file!\n");
        return false;
    }
    while (fgets(line, sizeof(line), fp) != NULL) {
        // Obtener el tamaño de la cadena hasta antes del salto de línea
        // Luego con ese número sabe dónde está el salto de línea y reemplaza el salto de línea por 
        // caracter del final de una cadena
        line[strcspn(line, "\n")] = '\0';
        // Elimina espacios en blanco al inicio y al final
        trim(line);
        //Convierte a minúsculas
        toLowerCase(line);
        // Compara la línea del buffer con la línea que está leyendo del archivo
        if (strcmp(line, buffer) == 0) {
            // Si encuentra la palabra anota que la encontró
            foundWorld = true;
            break;
        }
    }
    // cierra el archivo
    fclose(fp);
    // Devuelve si la encontró o no
    return foundWorld;
}

/**
 * Función para ejecutar un comando de la terminal
 * Sacado de https://www.baeldung.com/linux/c-code-execute-system-command
 */
static void ejecutarComando(FILE *out, const char *titulo, const char *cmd) {
    // Primero escribimos el título de la consulta
    fprintf(out, "%s:\n", titulo);

    // Luego intentamos ejecutar el comando por una tubería que nos conecta a su ejecución, donde podremos leer su salida por el modificador "r" de read
    // Curioso porque la ejecución del comando se guarda como archivo
    FILE *fp = popen(cmd, "r");
    if (!fp) {
        //si falla el comando entonces lo vamos a guardar en flujo que después escribiremos en el archivo
        fprintf(out, "No se pudo ejecutar el comando. %s\n", cmd);
        return;
    }
    // Sacamos línea por línea del flujo para escribirlo en el archivo
    char line[1024];
    // con fgets obtenemos la siguiente línea (si existe)
    while (fgets(line, sizeof(line), fp)) {
        // Con fputs escribimos una línea en el archivo de salida
        fputs(line, out);
    }

    // Salto de línea para dar formato
    fputc('\n', out); 
    pclose(fp);
}

/**
 * Función para obtener información del servidor y guardarla en sysinfo.txt
 */
void saveSysInfo(const char *outputFile) {
    FILE *out = fopen(outputFile, "w");
    if (!out) {
        perror("Error al crear el archivo de información del sistema");
        return;
    }

    ejecutarComando(out, "Fecha y hora", "date");
    ejecutarComando(out, "OS y Kernel", "uname -s -r");
    ejecutarComando(out, "Distribución", "grep PRETTY_NAME /etc/os-release | cut -d= -f2 | tr -d '\"'");
    ejecutarComando(out, "IPs", "hostname -I");
    ejecutarComando(out, "CPU", "grep -m1 'model name' /proc/cpuinfo");
    ejecutarComando(out, "Info. Núcleos", "nproc");
    ejecutarComando(out, "Arquitectura", "uname -m");
    ejecutarComando(out, "Memoria", "free -h");
    ejecutarComando(out, "Disco", "df -h");
    ejecutarComando(out, "Usuarios conectados", "who");
    ejecutarComando(out, "Todos los usuarios", "cut -d: -f1 /etc/passwd");
    ejecutarComando(out, "Uptime", "uptime -p");
    ejecutarComando(out, "Procesos activos", "ps -e --no-headers | wc -l");
    ejecutarComando(out, "Directorios montados", "mount");

    fclose(out);
}

int main(int argc, char *argv[]) {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size;
    char buffer[BUFFER_SIZE] = {0};
    int puertoClient;
    int shift;
    int puertoServer;
    char fileName[BUFFER_SIZE];
    char fileExt[BUFFER_SIZE];

    if (argc != 2) {
        printf("Type: %s <port> \n", argv[0]);
        return 1;
    }

    puertoServer = atoi(argv[1]);

    // Crear el socket:
    // AF_INET indica que se utilizará IPv4
    // SOCK_STREAM indica que se utilizará TCP
    // 0 indica que se utilizará el protocolo predeterminado (TCP en este caso)
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1) {
        perror("[-] Error to create the socket");
        return 1;
    }

    // Configurar dirección del servidor
    // Indicar que se utlizará IPv4
    server_addr.sin_family = AF_INET;
    // Asignar el puerto- htons() convierte el número de puerto a formato de red
    // htons() ajusta la representación del puerto, porque algunos sistemas utilizan un orden de bytes diferente
    // entonces la función convierte a Big Endian que es como poner el 8080 que en hexa es 0x1F90, entonces lo acomoda como 1F 90,
    // pero si nuestro sistema utilizara Little Endian, entonces lo acomodaría como 90 1F y eso no es compatible con la red, entonces tenemos que convertir a Big Endian
    server_addr.sin_port = htons(puertoServer);
    // definición de dirección de un socket -> def dirección IP ->  IP en formato binario = 000000... (escuchar todas las ip)
    /*
     INADDR_ANY permite que el servidor escuche en todas las interfaces de red disponibles, las interfaces de red son las que aparecen cuando haces ip a
     por ejemplo cuando tienes dos adaptadores de red
    ensp08: Podría tener la IP 192.168.1.10.
    ensp03: Podría tener la IP 10.0.0.5.
    ambas de la misma máquina, entonces el servidor va a escuchar con ambas
    Para esta práctica podríamos poner solo la IP que usamos para que el cliente se conecte
    */
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // Enlazamos el socket con la configuración que le dimos antes (puerto e ips por las que va a escuchar, además de la familia)
    // socket creado anteriormente // casteamos de sockaddr_in a sockaddr (sockaddr es más general) // calculamos el tamaño de server_addr (suma del tamaño de todos sus campos)
    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("[-] Error binding");
        close(server_sock);
        return 1;
    }
    // socket se prepara para escuchar solo a una conexión antes de que se acepten.
    // con backlog = 1 permite solo 1 conexión pendiente en la cola, si hay más de una las adicionales se rechazarán
    if (listen(server_sock, 1) < 0) {
        perror("[-] Error on listen");
        close(server_sock);
        return 1;
    }

    printf("[+] Server listening port %d...\n", puertoServer);
    // tamaño de la estructura sockaddr_in, es la suma del tamaño fijo de sus campos
    addr_size = sizeof(client_addr);
    // accept debe saber qué estructura va a llenar (nuevamente casteada porque trabaja con sockaddr) y el tamaño de dicha estructura
    // accept es una función bloqueante, se va a quedar ahí parada hasta que llegue algo
    client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_size);
    if (client_sock < 0) {
        perror("[-] Error on accept");
        close(server_sock);
        return 1;
    }
    printf("[+] Client connected\n");

    /*
     Handshake de aplicación
    - recv es para recibir datos de un socket ya conectado
       buffer es el punto del buffer donde se podrán almacenar datos
       size es el tamaño del buffer y hacemos - 1 porque reservamos el espacio extra para el caracter terminador nulo '\0'
       0 es porque no estamos usando banderas, queremos comportamiento estandar TCP
    */
    int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes <= 0) {
        printf("[-] Missed port or shift\n");
        close(client_sock);
        close(server_sock);
        return 1;
    }


    buffer[bytes] = '\0';
    // Realizamos un escaneo formateado del buffer al que recibimos el mensaje
    // Recibimos el puerto y el deplazamiento ambos como enteros
    sscanf(buffer, "%d %d", &puertoClient, &shift); // extrae puerto y desplazamiento
    printf("[+][Server] Port received: %d\n", puertoClient);
    printf("[+][Server] Shift received: %d\n", shift);


    /*
    Aquí dejo mi TODO por el día de hoy.
    Lo que falta:
    */

    // Validamos que el puerto recibido sea igual al puerto de mi socket
    if (puertoClient == puertoServer)
    {
        // Si el puerto es válido avisamos que está autorizado
        printf("[+][Server %d] Request accepted... %d\n", puertoServer, puertoClient);
        send(client_sock, "REQUEST ACCEPTED", strlen("REQUEST ACCEPTED"), 0);
        // Pausa para evitar colisión
        sleep(1);

        // recibimos el nombre del archivo
        bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) {
            printf("[-] Missed port or shift\n");
            close(client_sock);
            close(server_sock);
            return 1;
        }
        // caracter nulo
        buffer[bytes] = '\0';
        // Formateamos el nombre del archivo
        sscanf(buffer, "%s.%s", fileName, fileExt);



        // ARREGLAR ESTO
        // recibimos un archivo
        FILE *fp = fopen(strcat(fileName,fileExt), "w");
                if (fp == NULL) {
                    perror("[-] Error to open the file");
                    close(client_sock);
                    return 1;
                }
                while ((bytes = recv(client_sock, buffer, sizeof(buffer), 0)) > 0) {
                    fwrite(buffer, 1, bytes, fp);
                }

            

        // Ciframos el archivo y lo guardamos

        // Avisamos que el archivo fue cifrado
        printf("[+][Server %d] File received and encrypted:\n", puertoServer);
        fclose(fp);

    }else{
        // si el puerto no es entonces mandamos REJECTED
        send(client_sock, "REJECTED", strlen("REJECTED"), 0);
        printf("[+][Server %d] Request rejected (Client requested port %d)\n", puertoServer, puertoClient);
    }

    close(client_sock);
    close(server_sock);
    return 0;
}
