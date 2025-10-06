#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

//Agregamos nuevas librerias para obtener la info
#include <sys/utsname.h>
#include <sys/sysinfo.h>


#define PORT 7006 // Puerto en el que el servidor escucha
#define BUFFER_SIZE 1024 // Tamano del buffer para recibir datos

// Funcion nueva que recupera la info del servidor
void saveSystemInfo() {
    // Crear archivo desde cero
    system("echo '=== SISTEMA OPERATIVO ===' > sysinfo.txt");

    // Información del OS y Kernel
    system("echo 'Sistema: Linux' >> sysinfo.txt");
    system("echo 'Nodo: $(hostname)' >> sysinfo.txt");
    system("echo 'Release: $(uname -r)' >> sysinfo.txt");
    system("echo 'Versión: $(uname -v)' >> sysinfo.txt");
    system("echo 'Máquina: $(uname -m)' >> sysinfo.txt");
    system("echo '' >> sysinfo.txt");

    // Información de la CPU
    system("echo '=== CPU ===' >> sysinfo.txt");
    system("lscpu | grep 'Model name\\|Architecture\\|CPU(s)' >> sysinfo.txt");

    // Memoria
    system("echo '' >> sysinfo.txt");
    system("echo '=== MEMORIA ===' >> sysinfo.txt");
    system("free -h >> sysinfo.txt");

    // Discos
    system("echo '' >> sysinfo.txt");
    system("echo '=== DISCOS ===' >> sysinfo.txt");
    system("df -h >> sysinfo.txt");

    // Usuarios conectados
    system("echo '' >> sysinfo.txt");
    system("echo '=== USUARIOS CONECTADOS ===' >> sysinfo.txt");
    system("who >> sysinfo.txt");

    // Todos los usuarios
    system("echo '' >> sysinfo.txt");
    system("echo '=== TODOS LOS USUARIOS ===' >> sysinfo.txt");
    system("cut -d: -f1 /etc/passwd >> sysinfo.txt");

    // Uptime
    system("echo '' >> sysinfo.txt");
    system("echo '=== UPTIME ===' >> sysinfo.txt");
    system("uptime >> sysinfo.txt");

    // Procesos activos
    system("echo '' >> sysinfo.txt");
    system("echo '=== PROCESOS ACTIVOS ===' >> sysinfo.txt");
    system("echo 'Número de procesos: $(ps aux | wc -l)' >> sysinfo.txt");

    // Directorios montados
    system("echo '' >> sysinfo.txt");
    system("echo '=== DIRECTORIOS MONTADOS ===' >> sysinfo.txt");
    system("mount >> sysinfo.txt");
    printf("[+] Archivo sysinfo.txt creado\n");
}

void decryptCaesar(char *text, int shift) {
shift = shift % 26;
for (int i = 0; text[i] != '\0' ; i++) {
char c = text[i];
if (isupper(c)) {
text[i] = ((c - 'A' - shift + 26) % 26) + 'A' ;
} else if (islower(c)) {
text[i] = ((c - 'a' - shift + 26) % 26) + 'a' ;
} else {
text[i] = c;
}
}
}

void saveNetworkInfo(const char *outputFile) {
FILE *fpCommand;
FILE *fpOutput;
char buffer[512];

// Ejecutar comando para obtener informacion de red
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

// Leer linea por linea y escribir en el archivo
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


// Funcion para convertir a minusculas
void toLowerCase(char *str) {
for (int i = 0; str[i]; i++)
str[i] = tolower((unsigned char)str[i]);
}
// Funcion para eliminar espacios al inicio y final
void trim(char *str) {
char *end;
while (isspace((unsigned char)*str)) str++; // inicio
end = str + strlen(str) - 1;
while (end > str && isspace((unsigned char)*end)) end--; // final
*(end + 1) = '\0' ;
}

bool isOnFile(const char *bufferOriginal) {
FILE *fp;
char line[BUFFER_SIZE];
char buffer[BUFFER_SIZE];
bool foundWorld = false;
// Copiamos el buffer original para poder modificarlo
strncpy(buffer, bufferOriginal, BUFFER_SIZE);
buffer[BUFFER_SIZE - 1] = '\0' ; // aseguramos terminacion
trim(buffer);
toLowerCase(buffer);
fp = fopen("cipherworlds.txt", "r");
if (fp == NULL) {
printf("[-]Error opening file!\n");
return false;
}
while (fgets(line, sizeof(line), fp) != NULL) {
line[strcspn(line, "\n")] = '\0' ;
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
buffer[bytes] = '\0' ;
sscanf(buffer, "%s %d", clave, &shift); // extrae clave y desplazamiento
printf("[+][Server] Encrypted key obtained: %s\n", clave);
if (isOnFile(clave)) {
decryptCaesar(clave, shift);
printf("[+][Server] Key decrypted: %s\n", clave);
send(client_sock, "ACCESS GRANTED", strlen("ACCESS GRANTED"), 0);
sleep(1); // Pequena pausa para evitar colision de datos
saveNetworkInfo("network_info.txt");
saveSystemInfo();
sendFile("network_info.txt", client_sock);
saveSystemInfo();printf("[+] Sent file\n");
} else {
send(client_sock, "ACCESS DENIED", strlen("ACCESS DENIED"), 0);
printf("[-][Server] Wrong Key\n");
}
close(client_sock);
close(server_sock);
return 0;
}