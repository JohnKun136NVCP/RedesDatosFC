// fgets_demo.c
#include <stdio.h>
#include <string.h>

int main(void) {
    char buffer[256]; // Creamos un arreglo para guardar lo que escriba el usuario

    printf("Escribe una línea (máx 255 chars): ");
     //fgets sirve para leer cadenas de forma segura, eentonces guarda lo que se escriba en el buffer
    // Si fgets devuelve NULL entonces mandamos  el mensaje de error
    if (!fgets(buffer, sizeof(buffer), stdin)) {
        fprintf(stderr, "No se pudo leer la línea\n");
        return 1;
    }
    // fgets normalemente guarda el \n al final, entonces lo quitamos para que la cadena se vea limpia
    size_t len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n') {
	//cambiamos el \n por el fin de la cadena
        buffer[len - 1] = '\0';
        len--;
    }

    // Por ultimo mostramos lo que se lee para comprobar que era lo que queriamos
    // y de igual forma mostramos la longitud
    printf("Leí: \"%s\"\n", buffer);
    printf("Longitud: %zu caracteres\n", len);

    return 0;
}
