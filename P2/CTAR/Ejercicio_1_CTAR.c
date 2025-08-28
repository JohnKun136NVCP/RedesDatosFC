#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#define PORT 7006        // Puerto en el que el servidor escucha
#define BUFFER_SIZE 1024 // Tamaño del buffer para recibir datos

//server.c

/*
    Función para desencriptar texto usando el cifrado César
*/
void decryptCaesar(char *text, int shift){
    // Ajustamos el desplazamiento y verificamos si las letras son mayúsculas o minúsculas
    shift = shift % 26;
    for (int i = 0; text[i] != '\0'; i++){
        char c = text[i];
        if (isupper(c)){
            text[i] = ((c - 'A' - shift + 26) % 26) + 'A';
        }
        else if (islower(c)){
            text[i] = ((c - 'a' - shift + 26) % 26) + 'a';
        }
        else{
            text[i] = c;
        }
    }
}

/*
    Función para guardar la información de red del equipo en un archivo
*/
void saveNetworkInfo(const char *outputFile){
    FILE *fpCommand;
    FILE *fpOutput;
    char buffer[512];

    // Ejecutar comando para obtener información de red
    fpCommand = popen("ip addr show", "r");
    if (fpCommand == NULL)
    {
        perror("Error!");
        return;
    }
    // Abrir archivo para guardar la salida
    fpOutput = fopen(outputFile, "w");
    if (fpOutput == NULL)
    {
        perror("[-] Error to open the file");
        pclose(fpCommand);

        return;
    }

    // Leer línea por línea y escribir en el archivo
    while (fgets(buffer, sizeof(buffer), fpCommand) != NULL)
    {
        fputs(buffer, fpOutput);
    }
    // Cerrar ambos archivos
    fclose(fpOutput);
    pclose(fpCommand);
}

/*
    Función para enviar un archivo a través del socket
*/
void sendFile(const char *filename, int sockfd){
    FILE *fp = fopen(filename, "r");
    if (fp == NULL){
        perror("[-] Cannot open the file");
        return;
    }
    char buffer[BUFFER_SIZE];
    size_t bytes;

    // Leemos el archivo y enviamos su contenido a través del socket
    while ((bytes = fread(buffer, 1, sizeof(buffer), fp)) > 0){
        if (send(sockfd, buffer, bytes, 0) == -1){
            perror("[-] Error to send the file");
            break;
        }
    }
    fclose(fp);
}

/*
    Función para convertir a minúsculas
*/
void toLowerCase(char *str){
    for (int i = 0; str[i]; i++)
        str[i] = tolower((unsigned char)str[i]);
}

/*
    Función para eliminar espacios al inicio y final
*/
void trim(char *str){
    char *end;
    while (isspace((unsigned char)*str))
        str++; // inicio
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end))
        end--; // final

    *(end + 1) = '\0';
}

/*
    Función para verificar si una palabra está en el archivo cipherworlds.txt
*/
bool isOnFile(const char *bufferOriginal){
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
    if (fp == NULL){
        printf("[-]Error opening file!\n");
        return false;
    }
    while (fgets(line, sizeof(line), fp) != NULL){
        line[strcspn(line, "\n")] = '\0';
        trim(line);
        toLowerCase(line);
        if (strcmp(line, buffer) == 0)
        {
            foundWorld = true;
            break;
        }
    }
    fclose(fp);
    return foundWorld;
}

/*
    Función para guardar la información del sistema en el archivo sysinfo.txt
*/
void saveSystemInfo(const char *outputFile){
    FILE *fp = fopen(outputFile, "w");
    if (!fp) {
        perror("[-] Error opening file");
        return;
    }

    char buffer[512];
    FILE *fpCommand;

    // Sistema y Kernel
    fprintf(fp, "\n*** Sistema y Kernel ***\n");
    fpCommand = popen("uname -s -r", "r");
    if (fpCommand) {
        while (fgets(buffer, sizeof(buffer), fpCommand) != NULL)
            fputs(buffer, fp);
        pclose(fpCommand);
    }
    fprintf(fp, "\n");

    // Distribución
    fprintf(fp, "*** Distribución ***\n");
    fpCommand = popen("cat /etc/os-release | grep PRETTY_NAME", "r");
    if (fpCommand) {
        while (fgets(buffer, sizeof(buffer), fpCommand) != NULL)
            fputs(buffer, fp);
        pclose(fpCommand);
    }
    fprintf(fp, "\n");

    // Direcciones IP
    fprintf(fp, "*** Direcciones IP ***\n");
    fpCommand = popen("ip -o addr show | awk '{print $2, $3, $4}'", "r");
    if (fpCommand) {
        while (fgets(buffer, sizeof(buffer), fpCommand) != NULL)
            fputs(buffer, fp);
        pclose(fpCommand);
    }
    fprintf(fp, "\n");

    // CPU y Núcleos
    fprintf(fp, "*** CPU y Núcleos ***\n");
    fpCommand = popen("lscpu", "r");
    if (fpCommand) {
        while (fgets(buffer, sizeof(buffer), fpCommand) != NULL)
            fputs(buffer, fp);
        pclose(fpCommand);
    }
    fprintf(fp, "\n");

    // Memoria
    fprintf(fp, "*** Memoria ***\n");
    fpCommand = popen("free -h", "r");
    if (fpCommand) {
        while (fgets(buffer, sizeof(buffer), fpCommand) != NULL)
            fputs(buffer, fp);
        pclose(fpCommand);
    }
    fprintf(fp, "\n");

    // Disco
    fprintf(fp, "*** Disco ***\n");
    fpCommand = popen("df -h", "r");
    if (fpCommand) {
        while (fgets(buffer, sizeof(buffer), fpCommand) != NULL)
            fputs(buffer, fp);
        pclose(fpCommand);
    }
    fprintf(fp, "\n");

    // Usuarios conectados
    fprintf(fp, "*** Usuarios conectados ***\n");
    fpCommand = popen("who", "r");
    if (fpCommand) {
        while (fgets(buffer, sizeof(buffer), fpCommand) != NULL)
            fputs(buffer, fp);
        pclose(fpCommand);
    }
    fprintf(fp, "\n");

    // Todos los usuarios del sistema
    fprintf(fp, "*** Todos los usuarios del sistema ***\n");
    fpCommand = popen("cut -d: -f1 /etc/passwd", "r");
    if (fpCommand) {
        while (fgets(buffer, sizeof(buffer), fpCommand) != NULL)
            fputs(buffer, fp);
        pclose(fpCommand);
    }
    fprintf(fp, "\n");

    // Uptime
    fprintf(fp, "*** Uptime ***\n");
    fpCommand = popen("uptime -p", "r");
    if (fpCommand) {
        while (fgets(buffer, sizeof(buffer), fpCommand) != NULL)
            fputs(buffer, fp);
        pclose(fpCommand);
    }
    fprintf(fp, "\n");

    // Procesos activos
    fprintf(fp, "*** Procesos activos ***\n");
    fpCommand = popen("ps -e", "r");
    if (fpCommand) {
        while (fgets(buffer, sizeof(buffer), fpCommand) != NULL)
            fputs(buffer, fp);
        pclose(fpCommand);
    }
    fprintf(fp, "\n");

    // Directorios montados
    fprintf(fp, "*** Directorios montados ***\n");
    fpCommand = popen("mount | column -t", "r");
    if (fpCommand) {
        while (fgets(buffer, sizeof(buffer), fpCommand) != NULL)
            fputs(buffer, fp);
        pclose(fpCommand);
    }
    fprintf(fp, "\n");

    fclose(fp);
}


/*
    Función principal con la configuración del socket
*/
int main()
{
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size;
    char buffer[BUFFER_SIZE] = {0};
    char clave[BUFFER_SIZE];
    int shift;

    // Creamos el socket del servidor para la comunicación
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1){
        perror("[-] Error to create the socket");
        return 1;
    }

    //Configuramos la dirección del servidor (IPv4, puerto, cualquier IP local).
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    //Asignamos el socket a la dirección y puerto especificados
    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0){
        perror("[-] Error binding");
        close(server_sock);
        return 1;
    }

    // Escuchamos conexiones entrantes
    if (listen(server_sock, 1) < 0){
        perror("[-] Error on listen");
        close(server_sock);
        return 1;
    }
    printf("[+] Server listening port %d...\n", PORT);

    //Esperamos y aceptamos una conexión entrante
    addr_size = sizeof(client_addr);
    client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_size);
    if (client_sock < 0){
        perror("[-] Error on accept");
        close(server_sock);
        return 1;
    }
    printf("[+] Client conneted\n");

    //Recibimos la clave encriptada + desplazamiento
    int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes <= 0){
        printf("[-] Missed key\n");
        close(client_sock);
        close(server_sock);
        return 1;
    }
    buffer[bytes] = '\0';

    // Extraemos la clave y desplazamiento del mensaje recibido
    sscanf(buffer, "%s %d", clave, &shift); 
    printf("[+][Server] Encrypted key obtained: %s\n", clave);


    //Verificamos si la clave está en el archivo
    if (isOnFile(clave)){
        decryptCaesar(clave, shift);
        printf("[+][Server] Key decrypted: %s\n", clave);
        send(client_sock, "ACCESS GRANTED", strlen("ACCESS GRANTED"), 0);
        
        sleep(1); // Pequeña pausa para evitar colisión de datos
        saveNetworkInfo("network_info.txt");
        sendFile("network_info.txt", client_sock);
        printf("[+] Sent file network_info.txt\n");
        
        sleep(1); // Pequeña pausa para evitar colisión de datos con network_info
        saveSystemInfo("sysinfo.txt");
        sendFile("sysinfo.txt", client_sock);
        printf("[+] Sent file sysinfo.txt\n");
    }
    else{
        send(client_sock, "ACCESS DENIED", strlen("ACCESS DENIED"), 0);
        printf("[-][Server] Wrong Key\n");
    }
    close(client_sock);
    close(server_sock);
    return 0;
}
