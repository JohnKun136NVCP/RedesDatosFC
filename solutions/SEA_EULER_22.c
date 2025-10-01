#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_NOMBRES 10000
#define MAX_LONG 100

void quitar_comillas(char *str) {
    size_t len = strlen(str);
    if (len >= 2 && str[0] == '"' && str[len - 1] == '"') {
        memmove(str, str + 1, len - 2);
        str[len - 2] = '\0';
    }
}

int cmp(const void *a, const void *b) {
    char *s1 = *(char **)a;
    char *s2 = *(char **)b;
    return strcmp(s1, s2);
}

int calcular_valor_alfabetico(const char *nombre) {
    int valor = 0;
    for (int i = 0; nombre[i] != '\0'; i++) {
        if (isalpha(nombre[i])) {
            valor += toupper(nombre[i]) - 'A' + 1;
        }
    }
    return valor;
}

int main() {
    char *nombres[MAX_NOMBRES];
    int count = 0;
    FILE *file = fopen("../0022_names.txt", "r");
    
    if (!file) {
        printf("Error: No se pudo abrir el archivo\n");
        return 1;
    }

    // Leer el archivo completo
    fseek(file, 0, SEEK_END);
    long filesize = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char *buffer = malloc(filesize + 1);
    fread(buffer, 1, filesize, file);
    buffer[filesize] = '\0';
    fclose(file);

    char *token = strtok(buffer, ",");
    while (token != NULL && count < MAX_NOMBRES) {
        while (*token == ' ') token++;
        
        char *nombre_copia = strdup(token);
        quitar_comillas(nombre_copia);
        nombres[count] = nombre_copia;
        count++;
        
        token = strtok(NULL, ",");
    }

    printf("Se leyeron %d nombres.\n", count);

    // Ordenar alfabéticamente
    qsort(nombres, count, sizeof(char *), cmp);

    long long total_puntuacion = 0;
    
    for (int i = 0; i < count; i++) {
        int valor_alfabetico = calcular_valor_alfabetico(nombres[i]);
        long long puntuacion = (long long)(i + 1) * valor_alfabetico;
        total_puntuacion += puntuacion;
        
        // COLIN
        if (strcmp(nombres[i], "COLIN") == 0) {
            printf("COLIN está en el lugar %d, valor alfabético: %d, puntuación: %lld\n", 
                   i + 1, valor_alfabetico, puntuacion);
        }
    }

    printf("Total de puntuaciones (nombres): %lld\n", total_puntuacion);

    for (int i = 0; i < count; i++) {
        free(nombres[i]);
    }
    free(buffer);

    return 0;
}