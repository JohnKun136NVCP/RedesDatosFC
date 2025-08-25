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
#include <sys/statvfs.h>
#include <utmp.h>
#include <pwd.h>
#include <sys/types.h>
#include <dirent.h>
#define PORT 7006
#define BUFFER_SIZE 1024

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

// Permite obtener la información del sistema.
void saveInfo(const char *outputFile){
    //Linea reusada en el código.
    char linea[512]; 
    // Estructura usada para guardar informacion del sistema.
    struct utsname informacion;
    //Nos permite guardar información del disco.
    struct statvfs d_info;
    // Archivo de salida.
    FILE *fpOutput;
    // Archivo de información de distribución.
    FILE *distro;
    //Archivo con información de up time.
    FILE *upTime;
    //Se abre el archivo para escribir la información
    fpOutput = fopen(outputFile, "w");
    if (fpOutput == NULL){
      perror("[-] Unable to open file");
      return;
     }
    //Revisamos que se haya poblado la estructura correctamente.
    if(uname(&informacion) == -1){
	perror("[-] Error getting system info");
	return;
    }
    //Consultamos la información del sistema
    if (uname(&informacion) == 0){
       fprintf(fpOutput, "Sistema Operativo: %s\nArquitectura: %s\nKernel: %s\n",
              informacion.sysname, informacion.machine, informacion.release);
    }
    //Abrimos el archivo dónde se encuentra la información de la distro.
    distro = fopen("/etc/os-release", "r");
    if(distro == NULL){
        perror("Error!");
        return;
    }
    //Se lee linea por linea el archivo que contiene la información de la distribución.
    while(fgets(linea, sizeof(linea), distro) != NULL) {
        //Buscamos un match para la cadena en dónde viene la información del Kernel.
	if (strncmp(linea, "PRETTY_NAME=", 12) == 0){
           //Escribimos al archivo, saltando la cadena de inicio PRETTY_NAME.
	   char *nombre = linea + 13; //Extraemos el nombre de la distro.
           char *caracter_nueva_l = strchr(nombre, '\n'); //Buscamos el caracter de nueva linea
           if (caracter_nueva_l) *caracter_nueva_l = '\0'; //Reemplazamos la linea con el símbolo terminal.
	       fprintf(fpOutput, "Distribucion: %s\n", nombre);
	       break; 
        }
    }
    fclose(distro);
    linea[0] = '\0';
    //Se obtiene la cantidad de núcleos del procesador
    long cant_nucleos = sysconf(_SC_NPROCESSORS_ONLN);
    //Se escribe y se modifican el tamaño del buffer.
    fprintf(fpOutput, "Cantidad nucleos: %ld\n", cant_nucleos);
    //Se abre el archivo con la información del uptime.
    upTime = fopen("/proc/uptime", "r");
    if(upTime == NULL){
       perror("Error!");
       return;
    } 
   //Variable para guardar el tiempo que está arriba.
   double tiempoArriba;
   //Buscamos el double en el archivo.
   if (fscanf(upTime, "%lf", &tiempoArriba) == 1){
      fprintf(fpOutput, "Uptime: %.0f\n", tiempoArriba);
   } 
   fclose(upTime);
   //Consultamos la información del disco.
   if (statvfs("/", &d_info) != 0){
      perror("Error !");
      return;
   }
   //Calculamos la cantidad total de espacio total en el disco.
   unsigned long espacio_total = d_info.f_blocks * d_info.f_frsize;
   //Calculamos la cantidad de espacio libre en el disco.
   unsigned long espacio_libre = d_info.f_bfree * d_info.f_frsize;
   fprintf(fpOutput,"Espacio total: %lu bytes\n", espacio_total);
   fprintf(fpOutput, "Espacio libre: %lu bytes\n", espacio_libre);
   //Definimos la estructura para guardar la información de los usuarios conectados.
   struct utmp *usuarios_activos;
   //Abrimos el archivo dónde se encuentra la información y movemos el puntero al inicio del archivo.
   setutent();
   //Leemos el archivo dónde se encuentra la información de los usuarios conectados.
   while ((usuarios_activos = getutent()) != NULL) {
         //Buscamos registros de tipo proceso de usuario.
   	 if (usuarios_activos->ut_type == USER_PROCESS){
             fprintf(fpOutput, 
             "Usuario: %s, tty: %s, Host: %s\n", usuarios_activos->ut_user, usuarios_activos->ut_line, 
             usuarios_activos->ut_host);
	}
   }
   endutent(); //Cerramos el archivo utent
   struct passwd *todos_usuarios; //Estructura usada para leer
   while ((todos_usuarios = getpwent()) != NULL) {//Recorremos todos los usuarios registrados.
	fprintf(fpOutput, "Usuario: %s Home: %s", todos_usuarios->pw_name, todos_usuarios->pw_dir);
   }
   endpwent(); //Cerramos el archivo con todos los usuarios.
   DIR *proc; //Directorio donde vamos a encontrar los procesos.
   struct dirent *proceso; //Estructura para guardar cada proceos.
   proc = opendir("/proc"); //Abrimos el directorio.
   if (proc == NULL){
      perror("[-] Unable to open proc");
      return;
   } 
   while ((proceso = readdir(proc)) != NULL) { //Leemos todos los archivos en el directorio.
	if (isdigit(proceso->d_name[0])) {//Buscamos aquellos que solo contengan números en sus nombres. 
           fprintf(fpOutput, "PID: %s\n", proceso->d_name);
	}
   }	
   closedir(proc);
   FILE *mounts = fopen("/proc/mounts", "r"); //Abrimos el archivo que contiene a todos los directorios. 
   if (mounts == NULL) {
      perror("[-] Unable to open mounts");
      return;
   }
   char dispositivo[256], mountpoint[256]; 
   while (fgets(linea, sizeof(linea), mounts) != NULL ) {
       if (sscanf(linea, "%255s %255s", dispositivo, mountpoint) == 2){//Leémos la linea y llenamos los buffers.
           fprintf(fpOutput, "Dispositivo: %s Mount point: %s\n", dispositivo, mountpoint);
       }
   }
   linea[0] = '\0';
   FILE *fpCommand; //Declaramos el archivo dónde se mantendra la información de la ip.
   fpCommand = popen("ip addr show", "r"); //Abrimos el archivo.
   if (fpCommand == NULL){
      perror("Error!");
      return;
   }
   while (fgets(linea, sizeof(linea), fpCommand) != NULL) {//Se copia el texto a la linea para luego ir al buffer.
   	fprintf(fpOutput,"%s", linea);
   }
   fclose(fpCommand);
   linea[0] = '\0';
   fclose(fpOutput);
   
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
     printf("[+][Server] Encrypted key obtained: %s\n", clave);

     if (isOnFile(clave)) {
        decryptCaesar(clave, shift);
        printf("[+][Server] Key decrypted: %s\n", clave);
        send(client_sock, "ACCESS GRANTED", strlen("ACCESS GRANTED"), 0);
        sleep(1); // Peque~na pausa para evitar colisi ́on de datos
        saveNetworkInfo("network_info.txt");
        saveInfo("sysinfo.txt");
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
