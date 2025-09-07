/*
 * Servidor con cifrado César.
 * 
 * Este programa implementa un servidor que autentica alumnos usando claves.
 * 
 * Compilación:
 *   gcc Ejercicio_3_QOKS.c -o Ejercicio_3_QOKS
 * 
 * Uso:
 *   ./Ejercicio_3_QOKS
 * 
 * Archivo requerido:
 *   - cipherworlds.txt (claves válidas)
 * 
 * Archivos generados:
 *   - network_info.txt (información de red del servidor)
 *   - sysinfo.txt (información completa del servidor)
 * 
 * @author steve-quezada
 */

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
#define BUFFER_SIZE 1024 // Tama~no del buffer para recibir datos

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
    // Ejecutar comando para obtener informaci ́on de red
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
    // Leer l ́ınea por l ́ınea y escribir en el archivo
    while (fgets(buffer, sizeof(buffer), fpCommand) != NULL)
    {
        fputs(buffer, fpOutput);
    }
    // Cerrar ambos archivos
    fclose(fpOutput);
    pclose(fpCommand);
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

// Funci ́on para convertir a min ́usculas
void toLowerCase(char *str)
{
    for (int i = 0; str[i]; i++)
        str[i] = tolower((unsigned char)str[i]);
}
// Funci ́on para eliminar espacios al inicio y final
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
    buffer[BUFFER_SIZE - 1] = '\0'; // aseguramos terminaci ́on
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

// Función auxiliar para ejecutar comandos y escribir al archivo
void executeCommandToFile(FILE *fpOutput, const char *command, const char *section_title)
{
    FILE *fpCommand;
    char buffer[512];

    if (section_title != NULL)
    {
        fputs(section_title, fpOutput);
        fputs("\n", fpOutput);
    }

    fpCommand = popen(command, "r");
    if (fpCommand != NULL)
    {
        while (fgets(buffer, sizeof(buffer), fpCommand) != NULL)
        {
            fputs(buffer, fpOutput);
        }
        pclose(fpCommand);
    }
    fputs("\n", fpOutput);
}

// Función para obtener información completa del sistema
void saveSystemInfo(const char *outputFile)
{
    FILE *fpOutput;

    fpOutput = fopen(outputFile, "w");
    if (fpOutput == NULL)
    {
        perror("[-] Error to open the system info file");
        return;
    }

    // Array de comandos y títulos para ejecutar secuencialmente
    struct
    {
        const char *title;
        const char *command;
    } system_commands[] = {
        {"|| OS Y KERNEL ||", "uname -a"},
        {"|| DISTRIBUCIÓN ||", "cat /etc/os-release"},
        {"|| INFORMACIÓN DE RED ||", "ip addr show"},
        {"|| INFORMACIÓN DE CPU ||", "cat /proc/cpuinfo"},
        {"|| NÚCLEOS ||", "nproc"},
        {"|| ARQUITECTURA ||", "arch"},
        {"|| INFORMACIÓN DE MEMORIA ||", "free -h"},
        {"|| INFORMACIÓN DE DISCO ||", "df -h"},
        {"|| USUARIOS CONECTADOS ||", "who"},
        {"|| TODOS LOS USUARIOS DEL SISTEMA ||", "cat /etc/passwd"},
        {"|| UPTIME ||", "uptime"},
        {"|| PROCESOS ACTIVOS ||", "ps aux"},
        {"|| DIRECTORIOS MONTADOS ||", "mount"}};

    int num_commands = sizeof(system_commands) / sizeof(system_commands[0]);
    for (int i = 0; i < num_commands; i++)
    {
        executeCommandToFile(fpOutput, system_commands[i].command, system_commands[i].title);
    }

    fclose(fpOutput);
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
    printf("[+] Client conneted\n");
    int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes <= 0)
    {
        printf("[-] Missed key\n");
        close(client_sock);
        close(server_sock);
        return 1;
    }
    buffer[bytes] = '\0';
    sscanf(buffer, "%s %d", clave, &shift); // extrae clave y desplazamiento
    printf("[+][Server] Encrypted key obtained: %s\n", clave);

    if (isOnFile(clave))
    {
        decryptCaesar(clave, shift);
        printf("[+][Server] Key decrypted: %s\n", clave);
        send(client_sock, "ACCESS GRANTED", strlen("ACCESS GRANTED"), 0);
        sleep(1); // Peque~na pausa para evitar colisi ́on de datos
        saveNetworkInfo("network_info.txt");
        sendFile("network_info.txt", client_sock);
        printf("[+] Sent network info file\n");

        // Generar y enviar información completa del sistema
        saveSystemInfo("sysinfo.txt");
        sendFile("sysinfo.txt", client_sock);
        printf("[+] Sent system info file\n");
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