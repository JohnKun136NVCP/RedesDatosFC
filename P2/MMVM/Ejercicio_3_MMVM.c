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

#define PORT 7006
#define BUFFER_SIZE 1024 

static void append_cmd(FILE *out, const char *titulo, const char *cmd) {
  fprintf(out, "==== %s ====\n", titulo);

  FILE *p = popen(cmd, "r");
  if (!p) {
    fprintf(out, "(error al ejecutar '%s': %s)\n\n", cmd, strerror(errno));
    return;
  }
  char buf[4096];
  size_t n;
  while ((n = fread(buf, 1, sizeof buf, p)) > 0) {
    fwrite(buf, 1, n, out);
  }
  pclose(p);
  fputc('\n', out);
}
static void writeSysInfo(const char *path) {
  FILE *out = fopen(path, "w");
  if (!out) {
    perror("[-] No se pudo crear sysinfo.txt");
    return;
  }

  // OS y Kernel
  append_cmd(out, "OS y Kernel", "uname -srm");

  // Distribución
  append_cmd(out, "Distribucion",
	     "grep -m1 '^PRETTY_NAME=' /etc/os-release | cut -d= -f2- | tr -d '\"' || lsb_release -ds");

  // IPs
  append_cmd(out, "IPs", "ip -4 -o addr show | awk '{print $2\":\"$4}'");

  // CPU Info
  append_cmd(out, "CPU Info", "lscpu || cat /proc/cpuinfo");

  //NUcleos
  append_cmd(out, "Nucleos", "nproc");

  //Arquitectura
  append_cmd(out, "Arquitectura", "uname -m");

  // Memoria
  append_cmd(out, "Memoria", "free -h");

  // Disco
  append_cmd(out, "Disco (df -h)", "df -h");

  // Usuarios conectados ahora
  append_cmd(out, "Usuarios conectados", "who || echo 'ninguno'");

  // Todos los usuarios del sistema
  append_cmd(out, "Todos los usuarios del sistema", "cut -d: -f1 /etc/passwd");

  // Uptime
  append_cmd(out, "Uptime", "uptime -p || cat /proc/uptime");

  // Procesos activos
  append_cmd(out, "Procesos activos (conteo)", "ps -e --no-headers | wc -l");

  // Directorios montados
  append_cmd(out, "Directorios montados",
	     "findmnt -rno TARGET,SOURCE,FSTYPE,SIZE,AVAIL 2>/dev/null || mount");

  fclose(out);
  printf("[+] sysinfo.txt generado\n");
}

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


void toLowerCase(char *str) {
  for (int i = 0; str[i]; i++)
    str[i] = tolower((unsigned char)str[i]);
}


void trim(char *str) {
  char *end;
  while (isspace((unsigned char)*str)) str++;
  end = str + strlen(str) - 1;
  while (end > str && isspace((unsigned char)*end)) end--; 
  *(end + 1) = '\0';
}

bool isOnFile(const char *bufferOriginal) {
  FILE *fp;
  char line[BUFFER_SIZE];
  char buffer[BUFFER_SIZE];
  bool foundWorld = false;

  strncpy(buffer, bufferOriginal, BUFFER_SIZE);
  buffer[BUFFER_SIZE - 1] = '\0'; 
    
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
  buffer[bytes] = '\0';
  sscanf(buffer, "%s %d", clave, &shift);
  printf("[+][Server] Encrypted key obtained: %s\n", clave);

  if (isOnFile(clave)) {
    decryptCaesar(clave, shift);
    printf("[+][Server] Key decrypted: %s\n", clave);
    send(client_sock, "ACCESS GRANTED", strlen("ACCESS GRANTED"), 0);
    sleep(1);
    writeSysInfo("sysinfo.txt");
    sendFile("sysinfo.txt", client_sock);
    printf("[+] Enviado sysinfo.txt\n");
  } else {
    send(client_sock, "ACCESS DENIED", strlen("ACCESS DENIED"), 0);
    printf("[-][Server] Wrong Key\n");
  }
  close(client_sock);
  close(server_sock);
  return 0;
}
