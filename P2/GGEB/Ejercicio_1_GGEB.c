#include <stdio.h>
#include <string.h>

int main() {
    char text[100];
    int shift;

    // pedimos el texto a cifrar
    printf("Introduce el texto a cifrar: ");
    fgets(text, sizeof(text), stdin);

    // se elimina el salto de línea que fgets agrega
    text[strcspn(text, "\n")] = 0;

    printf("Introduce el desplazamiento (número): ");
    scanf("%d", &shift); 

    // Cifrado César
    for (int i = 0; text[i] != '\0'; i++) {
        if (text[i] >= 'a' && text[i] <= 'z') {
            text[i] = ((text[i] - 'a' + shift) % 26) + 'a';  // Cifrar en minúsculas
        }
        else if (text[i] >= 'A' && text[i] <= 'Z') {
            text[i] = ((text[i] - 'A' + shift) % 26) + 'A';  // Cifrar en mayúsculas
        }
    }

    // Mostrar el texto cifrado
    printf("Texto cifrado: %s\n", text);

    return 0;
}

