#include <stdio.h>
#include <ctype.h>

/* 
 * Aplica el cifrado César a la cadena `texto` con desplazamiento `clave`.
 * Sólo cifra letras mayúsculas y minúsculas; deja intactos otros caracteres.
 */
void cifrar_cesar(char *texto, int clave) {
    clave = clave % 26;  // normalizar el desplazamiento
    for (int i = 0; texto[i] != '\0'; i++) {
        unsigned char c = texto[i];
        if (c >= 'A' && c <= 'Z') {
            texto[i] = (char)('A' + (c - 'A' + clave + 26) % 26);
        } else if (c >= 'a' && c <= 'z') {
            texto[i] = (char)('a' + (c - 'a' + clave + 26) % 26);
        }
        /* si no es letra, se deja tal cual */
    }
}

int main(void) {
    char texto[256];
    int clave;

    printf("Introduce una frase a cifrar: ");
    /* fgets lee la línea completa (hasta 255 caracteres) y la deja en `texto` */
    if (!fgets(texto, sizeof(texto), stdin)) {
        fprintf(stderr, "Error al leer la entrada.\n");
        return 1;
    }

    printf("Introduce el desplazamiento (clave): ");
    if (scanf("%d", &clave) != 1) {
        fprintf(stderr, "Error al leer la clave.\n");
        return 1;
    }

    cifrar_cesar(texto, clave);

    printf("Texto cifrado: %s\n", texto);
    return 0;
}
