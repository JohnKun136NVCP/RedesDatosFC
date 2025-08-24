#include <stdio.h>

int main(void) {
    char buf[128];  // Buffer para guardar hasta 128 caracteres

    printf("Ingrese un texto: ");

   // Lee el texto y revisa si hay algun error
    if (!fgets(buf, sizeof(buf), stdin)) { 
        perror("fgets"); return 1;  // Si hay error, lo muestro y termina
    }
    printf("Texto: %s", buf);  // Muestra el texto ingresado

    return 0;
}
