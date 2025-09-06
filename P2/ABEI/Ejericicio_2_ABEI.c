// Es el cifrado pero ocupe fgets para leer la entrada
#include <stdio.h>
#include <ctype.h>

void encryptCaesar(char *text, int shift) {
    // Dezplazamiento
    shift = shift % 26; 
    for (int i = 0; text[i] != '\0'; i++) {
        char c = text[i];
        if (isupper(c)) {
            text[i] = ((c - 'A' + shift) % 26) + 'A';
        } else if (islower(c)) {
            text[i] = ((c - 'a' + shift) % 26) + 'a';
        } 
    }
}

int main() {
    char mensaje[1024];
    int shift;

    printf("Ingrese la palabra a cifrar: ");
    // Leemos la palabra con fgets
    fgets(mensaje, sizeof(mensaje), stdin); 
    printf("Ingrese el desplazamiento: ");
    scanf("%d", &shift);

    encryptCaesar(mensaje, shift);

    printf("Mensaje cifrado: %s\n", mensaje);
    return 0;
}
