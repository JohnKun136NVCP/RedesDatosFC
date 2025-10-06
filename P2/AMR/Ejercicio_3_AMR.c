#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#define PORT 7006
#define BUFFER_SIZE 1024

void decryptCaesar(char *text, int shift){
  shift = shift % 26;
  for (int i = 0; text[i] != '\0';i++){
    char c = text[i];
    if (isupper(c)){
      text[i] = ((c - 'A' - shift + 26) % 26) + 'A';
    } else if (islower(c)){
      text[i] = ((c - 'a' - shift + 26) % 26) + 'a';
    } else {
      text[i] = c;
    }
  }
}

void saveNetworkInfo(const char *outputFile){
  FILE *fpCommand;
  FILE *fpOutput;
  char buffer[512];

  fpCommand = popen("ip addr show", "r");
  if (fpCommand == NULL){
    perror("Error!");
    return;
  }

  fpOutput = fopen(outputFile, "w");
  if (fpOutput == NULL){
    perror("[-] Error to open the file");
    pclose(fpCommand);
    return;
  }

  while(fgets(buffer,sizeof(buffer),fpCommand) != NULL){
    fputs(buffer,fpOutput);
  }

  fclose(fpOutput);
  pclose(fpCommand);
}

void saveSystemInfo(){
    char outputFile[] = "sysinfo.txt";
    FILE *fpCommand;
    FILE *fpOutput;
    char buffer[7196]; //512*14. 512 por cada commando.
    fpOutput = fopen(outputFile,"w");
    if (fpOutput == NULL){
        perror("[-] Error to open the file");
        pclose(fpCommand);
    }

    // Información del kernel
    fpCommand = popen("uname -a","r");
    if(fpCommand == NULL){
        perror("Error");
        return;
    }
    //Comando para escribir en el archivo, se esrcibe un header indicando la información
    fprintf(fpOutput, "------KERNEL------\n\n");
    while(fgets(buffer,sizeof(buffer),fpCommand) != NULL){
        fputs(buffer,fpOutput);
    }
    pclose(fpCommand);


    // Información de la Distribución
    fpCommand = popen("lsb_release -a","r");
    if(fpCommand == NULL){
        perror("Error");
        return;
    }
    //Comando para escribir en el archivo, se esrcibe un header indicando la información
    fprintf(fpOutput, "------Distribución------\n\n");
    while(fgets(buffer,sizeof(buffer),fpCommand) != NULL){
        fputs(buffer,fpOutput);
    }
    pclose(fpCommand);


    // Información de las IPS
    fpCommand = popen("hostname -I","r");
    if(fpCommand == NULL){
        perror("Error");
        return;
    }
    //Comando para escribir en el archivo, se esrcibe un header indicando la información
    fprintf(fpOutput, "------IPS------\n\n");
    while(fgets(buffer,sizeof(buffer),fpCommand) != NULL){
        fputs(buffer,fpOutput);
    }
    pclose(fpCommand);


    // Información del cpu
    fpCommand = popen("lscpu","r");
    if(fpCommand == NULL){
        perror("Error");
        return;
    }
    //Comando para escribir en el archivo, se esrcibe un header indicando la información
    fprintf(fpOutput, "------InfoCpu------\n\n");
    while(fgets(buffer,sizeof(buffer),fpCommand) != NULL){
        fputs(buffer,fpOutput);
    }
    pclose(fpCommand);


    // Información de los núcleos
    fpCommand = popen("cat /proc/cpuinfo","r");
    if(fpCommand == NULL){
        perror("Error");
        return;
    }
    //Comando para escribir en el archivo, se esrcibe un header indicando la información
    fprintf(fpOutput, "------Núcleos------\n\n");
    while(fgets(buffer,sizeof(buffer),fpCommand) != NULL){
        fputs(buffer,fpOutput);
    }
    pclose(fpCommand);


    // Información de la arquitectura
    fpCommand = popen("uname -m","r");
    if(fpCommand == NULL){
        perror("Error");
        return;
    }
    //Comando para escribir en el archivo, se esrcibe un header indicando la información
    fprintf(fpOutput, "------Arquitectura------\n\n");
    while(fgets(buffer,sizeof(buffer),fpCommand) != NULL){
        fputs(buffer,fpOutput);
    }
    pclose(fpCommand);


    // Información de la memoria
    fpCommand = popen("df -h","r");
    if(fpCommand == NULL){
        perror("Error");
        return;
    }
    //Comando para escribir en el archivo, se esrcibe un header indicando la información
    fprintf(fpOutput, "------Memoria------\n\n");
    while(fgets(buffer,sizeof(buffer),fpCommand) != NULL){
        fputs(buffer,fpOutput);
    }
    pclose(fpCommand);


    // Información del disco
    fpCommand = popen("lsblk","r");
    if(fpCommand == NULL){
        perror("Error");
        return;
    }
    //Comando para escribir en el archivo, se esrcibe un header indicando la información
    fprintf(fpOutput, "------Disco------\n\n");
    while(fgets(buffer,sizeof(buffer),fpCommand) != NULL){
        fputs(buffer,fpOutput);
    }
    pclose(fpCommand);

    // Información del usuarios conectados
    fpCommand = popen("who","r");
    if(fpCommand == NULL){
        perror("Error");
        return;
    }
    //Comando para escribir en el archivo, se esrcibe un header indicando la información
    fprintf(fpOutput, "------UsuariosConectados------\n\n");
    while(fgets(buffer,sizeof(buffer),fpCommand) != NULL){
        fputs(buffer,fpOutput);
    }
    pclose(fpCommand);


    // Información del usuarios del sistema
    fpCommand = popen("cat /etc/passwd","r");
    if(fpCommand == NULL){
        perror("Error");
        return;
    }
    //Comando para escribir en el archivo, se esrcibe un header indicando la información
    fprintf(fpOutput, "------UsuariosDelSistema------\n\n");
    while(fgets(buffer,sizeof(buffer),fpCommand) != NULL){
        fputs(buffer,fpOutput);
    }
    pclose(fpCommand);


    // Información del uptime
    fpCommand = popen("uptime","r");
    if(fpCommand == NULL){
        perror("Error");
        return;
    }
    //Comando para escribir en el archivo, se esrcibe un header indicando la información
    fprintf(fpOutput, "------UPTIME------\n\n");
    while(fgets(buffer,sizeof(buffer),fpCommand) != NULL){
        fputs(buffer,fpOutput);
    }
    pclose(fpCommand);


    // Información del procesosActivos
    fpCommand = popen("ps aux","r");
    if(fpCommand == NULL){
        perror("Error");
        return;
    }
    //Comando para escribir en el archivo, se esrcibe un header indicando la información
    fprintf(fpOutput, "------ProcesosActivos------\n\n");
    while(fgets(buffer,sizeof(buffer),fpCommand) != NULL){
        fputs(buffer,fpOutput);
    }
    pclose(fpCommand);


    // Información del DirectoriosMontadps
    fpCommand = popen("mount","r");
    if(fpCommand == NULL){
        perror("Error");
        return;
    }
    //Comando para escribir en el archivo, se esrcibe un header indicando la información
    fprintf(fpOutput, "------DirectoriosMontados------\n\n");
    while(fgets(buffer,sizeof(buffer),fpCommand) != NULL){
        fputs(buffer,fpOutput);
    }
    pclose(fpCommand);
    fclose(fpOutput);
}

void sendFile(const char *filename, int sockfd){
  FILE *fp = fopen(filename, "r");
  if (fp == NULL){
    perror("[-] Cannot open the file");
    return;
  }
  char buffer[BUFFER_SIZE];
  size_t bytes;

  while ((bytes = fread(buffer,1,sizeof(buffer),fp)) >0){
    if (send(sockfd,buffer,bytes,0) == -1){
      perror("[-] Error to send the file");
      break;
    }
  }
  fclose(fp);
}

void toLowerCase(char *str){
  for (int i = 0; str[i]; i++)
    str[i] = tolower((unsigned char)str[i]);
}

void trim(char *str){
  char *end;
  while (isspace((unsigned char)*str)) str++;
  end = str + strlen(str) -1;
  while (end > str && isspace((unsigned char)*end)) end --;
  *(end +1) = '\0';
}

bool isOnFile(const char *bufferOriginal){
  FILE *fp;
  char line[BUFFER_SIZE];
  char buffer[BUFFER_SIZE];
  bool foundWorld = false;

  strncpy(buffer,bufferOriginal,BUFFER_SIZE);
  buffer[BUFFER_SIZE-1]= '\0';

  trim(buffer);
  toLowerCase(buffer);

  fp = fopen("cipherworlds.txt","r");
  if (fp == NULL){
    printf("[-] Error opening file!\n");
    return false;
  }

  while (fgets(line,sizeof(line),fp) != NULL){
    line[strcspn(line,"\n")] = '\0';
    trim(line);
    toLowerCase(line);
    if (strcmp(line,buffer) == 0){
      foundWorld = true;
      break;
    }
  }

  fclose(fp);
  return foundWorld;
}

int main() {
  saveSystemInfo();  
  int server_sock,client_sock;
  struct sockaddr_in server_addr, client_addr;
  socklen_t addr_size;
  char buffer[BUFFER_SIZE] = {0};
  char clave[BUFFER_SIZE];
  int shift;

  server_sock = socket(AF_INET,SOCK_STREAM,0);
  if (server_sock == -1){
    perror("[-] Error to create the socket");
    return 1;
  }

  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(PORT);
  server_addr.sin_addr.s_addr = INADDR_ANY;

  if (bind(server_sock, (struct sockaddr *)&server_addr,sizeof(server_addr))<0){
    perror("[-] Error binding");
    close(server_sock);
    return 1;
  }

  if (listen(server_sock, 1)<0){
    perror("[-] Error onn listen");
    close(server_sock);
    return 1;
  }

  printf("[+] Server listening port %d... \n",PORT);

  addr_size = sizeof(client_addr);
  client_sock = accept(server_sock,(struct sockaddr *)&client_addr,&addr_size);
  if (client_sock < 0){
    perror("[-] Error on accept");
    close(server_sock);
    return 1;
  }

  printf("[+] Client connected\n");

  int bytes = recv(client_sock,buffer,sizeof(buffer)-1,0);
  if (bytes <= 0){
    printf("[-] Missed key\n");
    close(client_sock);
    close(server_sock);
    return 1;
  }

  buffer[bytes] = '\0';
  sscanf(buffer, "%s %d",clave,&shift);
  printf("[+][Server] Encrypted key obtained: %s\n",clave);

  if (isOnFile(clave)) {
    decryptCaesar(clave,shift);
    printf("[+][Server] Key decrypted: %s\n",clave);
    send(client_sock,"ACCESS GRANTED",strlen("ACCESS GRANTED"),0);
    sleep(1);
    saveNetworkInfo("network_info.txt");
    sendFile("network_info.txt",client_sock);
    printf("[+] Sent file\n");
  } else {
    send(client_sock, "ACCESS DENIED",strlen("ACCESS DENIED"),0);
    printf("[-][SERVER] Wrong Key\n");
  }

  close(client_sock);
  close(server_sock);
  return 0;
}