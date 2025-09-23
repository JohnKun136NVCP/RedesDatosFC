#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#include <sys/utsname.h>
#include <time.h>
#include <sys/sysinfo.h>

#define PORT 7006 // Puerto en el que el servidor escucha
#define BUFFER_SIZE 1024  //Tamaño del buffer para recibir datos
void decryptCaesar(char *text, int shift) {
        shift = shift % 26;
        for (int i = 0; text[i] != '\0'; i++){
                char c = text[i];
                if (isupper(c)) {
                        text[i] = ((c - 'A' - shift + 26) % 26 ) + 'A';
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

	fpCommand = popen("ip addr show", "r");	

        if (fpCommand == NULL) {
                perror("Error!");
                return;
        }

        fpOutput = fopen(outputFile, "w");
        if (fpOutput == NULL) {
                perror("[-] Error to open the file");
                pclose(fpCommand);
                return;
        }

        while (fgets(buffer, sizeof(buffer), fpCommand) != NULL) {
                fputs(buffer, fpOutput);
        }

        fclose(fpOutput);
        pclose(fpCommand);
}

void sendFile (const char *filename, int sockfd) {
        FILE *fp = fopen(filename, "r");
        if (fp == NULL) {
                perror("[-] Cannot open the file");
                return;
        }
  	char buffer[BUFFER_SIZE];	
        size_t bytes;
        while ((bytes = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
                if (send (sockfd, buffer, bytes, 0) == -1) {
                        perror("[-] Error to send the file");
                        break;
                }
        }
        fclose(fp);
}

void toLowerCase (char *str) {
        for (int i = 0; str[i]; i++)
                str[i] = tolower((unsigned char) str[i]);
}

void trim (char *str) {
        char *end;
        while (isspace ((unsigned char) *str)) str++;
        end = str + strlen(str) - 1;
        while (end > str && isspace ((unsigned char)*end)) end--;
        *(end + 1) = '\0';
}

bool isOnFile (const char *bufferOriginal) {
        FILE *fp;
        char line[BUFFER_SIZE];
        char buffer[BUFFER_SIZE];
        bool foundWorld = false;

        strncpy (buffer, bufferOriginal, BUFFER_SIZE);
	buffer[BUFFER_SIZE - 1] = '\0';

        trim(buffer);
        toLowerCase(buffer);

        fp = fopen("cipherworlds.txt", "r");
        if (fp == NULL) {
                printf("[-] Error opening file!\n");
                return false;
        }

        while (fgets (line, sizeof(line), fp) != NULL) {
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

void ejecutarComando(const char *comando, FILE *archivo, const char *descripcion){
  fprintf(archivo,"\n=== %s ===\n",descripcion);
  fflush(archivo);

  char comandoC[512];
  snprintf(comandoC, sizeof(comandoC), "%s >> temp_info.txt 2>&1", comando);
  system(comandoC);

  FILE *temp = fopen("temp_info.txt", "r");
  if (temp) {
    char buffer[256];
    while ( fgets(buffer, sizeof(buffer), temp)){
      fputs(buffer, archivo);
    }
    fclose(temp);
    remove("temp_info.txt");
  }
}

void getInfoSistema(){
  FILE *archivo = fopen("sysinfo.txt", "w");
  if (!archivo) {
    perror("Error al abrir el archivo");
    return;
  }

  time_t t = time(NULL);
  struct tm *tmI = localtime(&t);
  char fechaHora[80];
  strftime(fechaHora,sizeof(fechaHora),"%Y-%m-%d %H:%M:%S",tmI);

  fprintf(archivo,"Información del servidor - %s\n",fechaHora);

  struct utsname infoSistema;
  struct sysinfo infoMemoria;

  if (uname(&infoSistema) == 0) {
    fprintf(archivo,"Sistema operativo: %s\n",infoSistema.sysname);
    fprintf(archivo,"Nombre del host: %s\n",infoSistema.nodename);
    fprintf(archivo,"Version del kernel: %s\n",infoSistema.release);
    fprintf(archivo,"Version del sistema: %s\n",infoSistema.version);
    fprintf(archivo,"Arquitectura: %s\n",infoSistema.machine);
  }

  // Info Distro

  ejecutarComando("cat /etc/os-release | grep PRETTY_NAME | head -1",archivo,"Distribucion");
  ejecutarComando("hostname -I",archivo,"Direcciones IP");
  ejecutarComando("ip addr show | grep 'inet ' | grep -v '127.0.0.1'",archivo,"Interfaces de red");

  ejecutarComando("lscpu | grep 'Model name'",archivo,"Modelo de CPU");
  ejecutarComando("lscpu | grep 'CPU(s):' | head -1",archivo,"Número de núcleos");
  ejecutarComando("lscpu | grep 'Architecture'",archivo,"Arquitectura de CPU");

  if ( sysinfo(&infoMemoria) == 0) {
    fprintf(archivo,"Memoria total: %lu MB\n",infoMemoria.totalram /1024 / 1024);
    fprintf(archivo,"Memoria libre: %lu MB\n", infoMemoria.freeram / 1024 / 1024);
    fprintf(archivo,"Memoria en uso: %lu MB\n",(infoMemoria.totalram - infoMemoria.freeram) / 1024 / 1024);
  }

  ejecutarComando("df -h --total | grep total",archivo,"Uso de disco (total)");
  ejecutarComando("df -h /",archivo,"Partición raiz");

  ejecutarComando("who",archivo,"Usuarios conectados");

  ejecutarComando("getent passwd | cut -d: -f1 | sort",archivo,"Todos los usuarios del sistema");

  ejecutarComando("uptime",archivo,"Tiempo de actividad");

  ejecutarComando("ps aux --sort=-%cpu | head -5",archivo,"Top 5 procesos por CPU");
  ejecutarComando("ps aux --sort=-%mem | head -5",archivo,"Top 5 procesos por memoria");

  ejecutarComando("mount",archivo,"Sistemas de archivos montados");
  
  fclose(archivo);
}

int main() {

  getInfoSistema();
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
        if (bind (server_sock, (struct sockaddr *)&server_addr, sizeof (server_addr)) < 0) {
                perror("[-] Error binding");
                close(server_sock);
                return 1;
        }

        if (listen (server_sock, 1) < 0 ) {
                perror("[-] Error on listen");
                close(server_sock);
                return 1;
        }

        printf("[+] Server listening port %d...\n", PORT);

        addr_size = sizeof(client_addr);
        client_sock = accept (server_sock, (struct sockaddr *)&client_addr, &addr_size);
        if (client_sock < 0) {
                perror("[-] Error on accept");
                close(server_sock);
                return 1;
        }

        printf("[+] Client conneted\n");
        int bytes = recv (client_sock, buffer, sizeof(buffer) - 1, 0 );
        if (bytes <= 0) {
                printf("[-] Missed key\n");
                close(client_sock);
                close(server_sock);
                return 1;
        }

        buffer[bytes] = '\0';
        sscanf (buffer, "%s %d", clave, &shift);
        printf("[+][Server] Encrypted key obtained: %s\n", clave);

        if (isOnFile (clave)) {
                decryptCaesar (clave, shift);
                printf ("[+] [Server] Key decrypted: %s\n", clave);
                send(client_sock, "ACCESS GRANTED", strlen("ACCESS GRANTED"),0);
                sleep(1);
                saveNetworkInfo("network_info.txt");
                sendFile("network_info.txt", client_sock);
                printf("[+] Sent file\n");
        } else {
                send (client_sock, "ACCESS DENIED", strlen("ACCESS DENIED"), 0);
                printf("[-] [Server] Wrong Key\n");
        }

        close(client_sock);
        close(server_sock);
        return 0;
}
