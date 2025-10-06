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
#define BUFFER_SIZE 1024 // Tamaño del buffer para recibir datos

//Funcion para obtener la informacion del sistema.
void systeminfo(){
    //creamos un archivo nuevo o reescribimos el existente con fopen.
    FILE *file = fopen("sysinfo.txt", "w");
    if (!file) {
        perror("Error al abrir sysinfo.txt");
        return;
    }

    //fprintf para escribir en nuestro archivo nuevo.
    fprintf(file, "Información del Sistema \n");
    fprintf(file, "Sistema Operativo y Kernel:\n");
    //usamos system para utilizar comandos de linux que nos den la informacion requerida.
    //ademas usamos el operador >> para indicarle a system que escriba al final del texto sin borrar nada.
    fclose(file);
    system("uname -a >> sysinfo.txt");
    file = fopen("sysinfo.txt", "a"); //reabrir archivo para escribir informacion del comando
    fprintf(file, "\nDistribución:\n");
    fclose(file);
    system("lsb_release -d >> sysinfo.txt");
    file = fopen("sysinfo.txt", "a");
    fprintf(file, "\nIp's:\n");
    fclose(file);
    system("hostname -I >> sysinfo.txt");
    file = fopen("sysinfo.txt", "a");
    fprintf(file, "\nInformacion de CPU:\n");
    fclose(file);
    system("lscpu | grep 'Model name' >> sysinfo.txt");
    file = fopen("sysinfo.txt", "a");
    fprintf(file, "\nNúcleos:\n");
    fclose(file);
    system("nproc >> sysinfo.txt");
    file = fopen("sysinfo.txt", "a");
    fprintf(file, "\nArquitectura:\n");
    fclose(file);
    system("uname -m >> sysinfo.txt");
    file = fopen("sysinfo.txt", "a");
    fprintf(file, "\nMemoria:\n");
    fclose(file);
    system("free -h >> sysinfo.txt");
    file = fopen("sysinfo.txt", "a");
    fprintf(file, "\nDisco:\n");
    fclose(file);
    system("df -h >> sysinfo.txt");
    file = fopen("sysinfo.txt", "a");
    fprintf(file, "\nUsuarios conectados:\n");
    fclose(file);
    system("who >> sysinfo.txt");
    file = fopen("sysinfo.txt", "a");
    fprintf(file, "\nTodos los usuarios del sistema:\n");
    fclose(file);
    system("cut -d: -f1 /etc/passwd >> sysinfo.txt");
    file = fopen("sysinfo.txt", "a");
    fprintf(file, "\nUptime:\n");
    fclose(file);
    system("uptime -p >> sysinfo.txt");
    file = fopen("sysinfo.txt", "a");
    fprintf(file, "\nProcesos activos:\n");
    fclose(file);
    system("ps -e | wc -l >> sysinfo.txt");
    file = fopen("sysinfo.txt", "a");
    fprintf(file, "\nDirectorios montados:\n");
    fclose(file);
    system("mount | grep '^/' >> sysinfo.txt");
    file = fopen("sysinfo.txt", "a");
    //terminamos de poner la informacion y cerramos el archivo.
    fclose(file);
}
//Funcion de encriptado caesar.
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

//Funcion para envio de info de red a cliente.
void saveNetworkInfo(const char *outputFile) {
    FILE *fpCommand;
    FILE *fpOutput;
    char buffer[512];
    // Ejecutar comando para obtener informaci ́on de red
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

//Funcion para mandar archivos
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

// Función para convertir a min ́usculas
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

//Funcion que verifica la llave del usuario.
bool isOnFile(const char *bufferOriginal) {
    FILE *fp;
    char line[BUFFER_SIZE];
    char buffer[BUFFER_SIZE];
    bool foundWorld = false;
    // Copiamos el buffer original para poder modificarlo
    strncpy(buffer, bufferOriginal, BUFFER_SIZE);
    buffer[BUFFER_SIZE - 1] = '\0'; // aseguramos terminaci ́on
    trim(buffer);
    toLowerCase(buffer);
    fp = fopen("cipherworlds.txt", "r");
    if (fp == NULL) {
        printf("[-]Error opening file!\n");
        return false;
    }
    while (fgets(line, sizeof(line), fp) != NULL) {
        line[strcspn(line, "\n")] = '\0';
        trim(line);
        toLowerCase(line);
        if (strcmp(line, buffer) == 0) {
            foundWorld = true;
            break;
        }
    }
    fclose(fp);
    return foundWorld;
}

//Funcion main.
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
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(server_sock, (struct sockaddr *)&server_addr,sizeof(server_addr)) < 0) {
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
    sscanf(buffer, "%s %d", clave, &shift); // extrae clave y desplazamiento
    systeminfo(); //llamamos a la funcion para crear el archivo de informacion de sistema.
    printf("[+][Server] Encrypted key obtained: %s\n", clave);

    if (isOnFile(clave)) {
        decryptCaesar(clave, shift);
        printf("[+][Server] Key decrypted: %s\n", clave);
        send(client_sock, "ACCESS GRANTED", strlen("ACCESS GRANTED"), 0);
        sleep(1); // Peque~na pausa para evitar colisi ́on de datos
        saveNetworkInfo("network_info.txt");
        sendFile("network_info.txt", client_sock);
        printf("[+] Sent file\n");
    } else {
        send(client_sock, "ACCESS DENIED", strlen("ACCESS DENIED"), 0);
        printf("[-][Server] Wrong Key\n");
    }
    close(client_sock);
    close(server_sock);
    return 0;
}