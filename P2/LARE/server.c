#include <arpa/inet.h>
#include <ctype.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/utsname.h>
#include <unistd.h>

#define PORT 7006        // Puerto en el que el servidor escucha
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
  // Leer l ́ınea por l ́ınea y escribir en el archivo
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

// Funci ́on para convertir a min ́usculas
void toLowerCase(char *str) {
  for (int i = 0; str[i]; i++)
    str[i] = tolower((unsigned char)str[i]);
}
// Funci ́on para eliminar espacios al inicio y final
void trim(char *str) {
  char *end;
  while (isspace((unsigned char)*str))
    str++; // inicio
  end = str + strlen(str) - 1;
  while (end > str && isspace((unsigned char)*end))
    end--; // final
  *(end + 1) = '\0';
}

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
// Ejercicio 3
void system_info(void) {
  // Inicializamos el archivo
  char *file = "sysinfo.txt";
  FILE *out;
  FILE *command;
  char buffer[128];

  // Borramos todo su contenido o en su defecto, creamos el archivo
  out = fopen(file, "w");
  if (file == NULL) {
    perror("Error al abrir el archivo");
    return;
  }

  // utsname para obtener informacion basica del servidor
  struct utsname uts;

  if (uname(&uts) < 0) {
    perror("uname() error");
    return;
  }

  fprintf(out, "OS: %s\nKernel:\t%s\nArquitectura:\t%s\nDistribucion:\t",
          uts.sysname, uts.release, uts.machine);
  fclose(out);
  // Obtenemos la distribucion del sistema
  system(
      "grep ^NAME= /etc/os-release | cut -d= -f2- | tr -d '\"' >> sysinfo.txt");

  // Volvemos a abrir el archivo para dar formato
  out = fopen(file, "a");
  if (file == NULL) {
    perror("Error al abrir el archivo");
    return;
  }
  fprintf(out, "Ip:\t");
  // Obtenemos la Ip
  command = popen("hostname -I", "r");
  if (command == NULL) {
    perror("Error en el comando");
    return;
  }
  while (fgets(buffer, sizeof(buffer), command) != NULL) {
    fputs(buffer, out);
  }
  fprintf(out, "CPU:\t");
  pclose(command);
  fclose(out);
  // Obtenemos el modelo de CPU
  system("grep 'model name' /proc/cpuinfo | head -1 | cut -d: -f2- >> "
         "sysinfo.txt");
  // Numero de nucleos
  out = fopen(file, "a");
  fprintf(out, "Nucleos:\t");
  fclose(out);
  system("nproc >> sysinfo.txt");
  // Obtener informacion de memoria, la memoria se dice en Kb
  system("grep MemTotal /proc/meminfo >> sysinfo.txt");
  // Obtener informacion del disco.
  out = fopen(file, "a");
  fprintf(out, "Disco:\n");
  fclose(out);
  system("df -h | sed 's/^/\t/'>> sysinfo.txt");
  // Usuarios conectados
  out = fopen(file, "a");
  fprintf(out, "Usuarios conectados:\n");
  fclose(out);
  system("who | sed 's/^/\t/' >> sysinfo.txt");
  // Todos los usuarios del sistema
  out = fopen(file, "a");
  fprintf(out, "Usuarios del sistema:\n");
  fclose(out);
  system("cut -d: -f1 /etc/passwd | sed 's/^/\t/'>> sysinfo.txt");
  // Obtener el tiempo de encendido
  out = fopen(file, "a");
  fprintf(out, "Uptime:\t");
  fclose(out);
  system("uptime -p >> sysinfo.txt");
  // Procesos activos
  out = fopen(file, "a");
  fprintf(out, "Procesos activos:\n");
  fclose(out);
  system("ps -e >> sysinfo.txt");
  // Directorios montados
  out = fopen(file, "a");
  fprintf(out, "Directorios montados:\n");
  fclose(out);
  system("findmnt >> sysinfo.txt");

  return;
}

int main() {
  int server_sock, client_sock;
  struct sockaddr_in server_addr, client_addr;
  socklen_t addr_size;
  char buffer[BUFFER_SIZE] = {0};
  char clave[BUFFER_SIZE];
  int shift;
  system_info();
  server_sock = socket(AF_INET, SOCK_STREAM, 0);
  if (server_sock == -1) {
    perror("[-] Error to create the socket");
    return 1;
  }
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(PORT);
  server_addr.sin_addr.s_addr = INADDR_ANY;
  if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) <
      0) {
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
  client_sock =
      accept(server_sock, (struct sockaddr *)&client_addr, &addr_size);
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
