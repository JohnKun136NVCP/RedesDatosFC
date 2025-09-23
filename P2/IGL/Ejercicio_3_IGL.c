#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define BUFSIZE 1024

/* 
 * Ejecuta un comando de shell y escribe su salida en un archivo FILE*.
 * `titulo` se imprime antes de la salida para contextualizar la sección.
 */
void volcar_comando(FILE *fo, const char *titulo, const char *cmd) {
    fprintf(fo, "===== %s =====\n", titulo);
    FILE *pp = popen(cmd, "r");
    if (!pp) {
        fprintf(fo, "[Error al ejecutar: %s]\n\n", cmd);
        return;
    }
    char buffer[BUFSIZE];
    while (fgets(buffer, sizeof(buffer), pp)) {
        fputs(buffer, fo);
    }
    fputc('\n', fo);
    pclose(pp);
}

/* 
 * Genera sysinfo.txt con OS y kernel, distribución, IPs, información de CPU,
 * arquitectura, memoria, disco, usuarios conectados, todos los usuarios,
 * uptime, procesos activos y montajes.
 */
void generar_sysinfo(const char *ruta) {
    FILE *fo = fopen(ruta, "w");
    if (!fo) {
        perror("fopen sysinfo");
        return;
    }
    /* OS y kernel */
    volcar_comando(fo, "OS y kernel", "uname -o && uname -r");

    /* Distribución (intenta leer /etc/os-release si existe) */
    volcar_comando(fo, "Distribucion",
        "if [ -f /etc/os-release ]; then . /etc/os-release && echo \"$PRETTY_NAME\"; "
        "else lsb_release -d 2>/dev/null; fi");

    /* Interfaces y direcciones IP */
    volcar_comando(fo, "IPs (IPv4/IPv6)", "ip -o addr show");

    /* Información de CPU y arquitectura */
    volcar_comando(fo, "CPU y arquitectura",
        "lscpu 2>/dev/null | egrep 'Model name|Architecture|CPU\\(s\\)|Thread|Core|Socket' "
        "|| uname -m");

    /* Memoria */
    volcar_comando(fo, "Memoria (free -h)", "free -h");

    /* Disco */
    volcar_comando(fo, "Disco (df -h)", "df -hT --total");

    /* Usuarios conectados */
    volcar_comando(fo, "Usuarios conectados", "who");

    /* Todos los usuarios del sistema */
    volcar_comando(fo, "Usuarios del sistema", "cut -d: -f1 /etc/passwd");

    /* Uptime */
    volcar_comando(fo, "Uptime", "uptime -p; uptime");

    /* Procesos activos */
    volcar_comando(fo, "Procesos activos (top 10 por uso CPU)",
        "ps -eo pid,comm,pcpu,pmem --sort=-pcpu | head -n 11");

    /* Directorios montados */
    volcar_comando(fo, "Montajes", "mount | column -t");

    fclose(fo);
}

int main(void) {
    /* Ejemplo de cómo llamar la función en tu server.c */
    generar_sysinfo("sysinfo.txt");
    printf("Se generó sysinfo.txt con la información del sistema.\n");
    return 0;
}
