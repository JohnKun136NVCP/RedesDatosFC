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

void decryptCaesar(char *text, int shift)
{
    shift = shift % 26;
    for (int i = 0; text[i] != '\0'; i++)
    {
        char c = text[i];
        if (isupper(c))
        {
            text[i] = ((c - 'A' - shift + 26) % 26) + 'A';
        }
        else if (islower(c))
        {
            text[i] = ((c - 'a' - shift + 26) % 26) + 'a';
        }
        else
        {
            text[i] = c;
        }
    }
}

void saveNetworkInfo(const char *outputFile)
{
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

// NUEVA FUNCIÓN: Obtener información completa del sistema
void saveSystemInfo(const char *outputFile)
{
    FILE *fp = fopen(outputFile, "w");
    if (fp == NULL)
    {
        perror("[-] Error creating system info file");
        return;
    }
    
    // Comandos para obtener información del sistema
    char *commands[] = {
        "echo '=== OS y Kernel ==='",
        "uname -srm",
        "echo '\n=== Distribución ==='", 
        "lsb_release -d 2>/dev/null || cat /etc/*release 2>/dev/null | head -n1 || echo 'No disponible'",
        "echo '\n=== Direcciones IP ==='",
        "ip addr show | grep 'inet ' | grep -v '127.0.0.1'",
        "echo '\n=== Información de CPU ==='",
        "lscpu | grep -E '(Model name|Architecture|CPU(s))'",
        "echo '\n=== Memoria ==='",
        "free -h",
        "echo '\n=== Espacio en disco ==='",
        "df -h",
        "echo '\n=== Usuarios conectados ==='",
        "who",
        "echo '\n=== Todos los usuarios ==='",
        "cat /etc/passwd | cut -d: -f1",
        "echo '\n=== Tiempo de actividad (Uptime) ==='",
        "uptime",
        "echo '\n=== Procesos activos ==='",
        "ps aux | wc -l",
        "echo '\n=== Directorios montados ==='",
        "mount"
    };
    
    int num_commands = sizeof(commands) / sizeof(commands[0]);
    char buffer[512];
    
    for (int i = 0; i < num_commands; i++)
    {
        FILE *cmd = popen(commands[i], "r");
        if (cmd)
        {
            fprintf(fp, "Ejecutando: %s\n", commands[i]);
            while (fgets(buffer, sizeof(buffer), cmd) != NULL)
            {
                fputs(buffer, fp);
            }
            fprintf(fp, "\n");
            pclose(cmd);
        }
    }
    
    fclose(fp);
    printf("[+] Información del sistema guardada en %s\n", outputFile);
}

void sendFile(const char *filename, int sockfd)
{
    FILE *fp = fopen(filename, "r");
    if (fp == NULL)
    {
        perror("[-] Cannot open the file");
        return;
    }
    char buffer[BUFFER_SIZE];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), fp)) > 0)
    {
        if (send(sockfd, buffer, bytes, 0) == -1)
        {
            perror("[-] Error to send the file");
            break;
        }
    }
    fclose(fp);
}

// Función para convertir a minúsculas
void toLowerCase(char *str)
{
    for (int i = 0; str[i]; i++)
        str[i] = tolower((unsigned char)str[i]);
}

// Función para eliminar espacios al inicio y final
void trim(char *str)
{
    char *end;
    while (isspace((unsigned char)*str))
        str++; // inicio
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end))
        end--; // final
    *(end + 1) = '\0';
}

bool isOnFile(const char *bufferOriginal)
{
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
    if (fp == NULL)
    {
        printf("[-]Error opening file!\n");
        return false;
    }
    while (fgets(line, sizeof(line), fp) != NULL)
    {
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

int main()
{
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size;
    char buffer[BUFFER_SIZE] = {0};
    char clave[BUFFER_SIZE];
    int shift;
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock == -1)
    {
        perror("[-] Error to create the socket");
        return 1;
    }
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("[-] Error binding");
        close(server_sock);
        return 1;
    }
    if (listen(server_sock, 1) < 0)
    {
        perror("[-] Error on listen");
        close(server_sock);
        return 1;
    }
    printf("[+] Server listening port %d...\n", PORT);
    addr_size = sizeof(client_addr);
    client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &addr_size);
    if (client_sock < 0)
    {
        perror("[-] Error on accept");
        close(server_sock);
        return 1;
    }
    printf("[+] Client connected\n"); // Corregido: "connected"
    int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes <= 0)
    {
        printf("[-] Missed key\n");
        close(client_sock);
        close(server_sock);
        return 1;
    }
    buffer[bytes] = '\0';
    sscanf(buffer, "%s %d", clave, &shift); 
    printf("[+][Server] Encrypted key obtained: %s\n", clave);

    if (isOnFile(clave))
    {
        decryptCaesar(clave, shift);
        printf("[+][Server] Key decrypted: %s\n", clave);
        send(client_sock, "ACCESS GRANTED", strlen("ACCESS GRANTED"), 0);
        sleep(1); // Pequeña pausa para evitar colisión de datos
        
        saveNetworkInfo("network_info.txt");
        saveSystemInfo("sysinfo.txt"); 

        sendFile("network_info.txt", client_sock);
        sendFile("sysinfo.txt", client_sock); // ENVIAR NUEVO ARCHIVO
        
        printf("[+] Sent files\n");
    }
    else
    {
        send(client_sock, "ACCESS DENIED", strlen("ACCESS DENIED"), 0);
        printf("[-][Server] Wrong Key\n");
    }
    close(client_sock);
    close(server_sock);
    return 0;
}