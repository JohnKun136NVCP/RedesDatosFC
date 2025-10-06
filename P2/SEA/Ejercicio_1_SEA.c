#include <stdio.h>
#include <ctype.h>
#include <string.h>

void encryptCaesar(char *text, int shift) {
    shift = shift % 26;
    for (int i = 0; text[i] != '\0'; i++) {
        char c = text[i];
        if (isupper(c)) {
            text[i] = ((c - 'A' + shift) % 26) + 'A';
        } else if (islower(c)) {
            text[i] = ((c - 'a' + shift) % 26) + 'a';
        } else {
            text[i] = c;
        }
    }
}

int main() {
    char text[100];
    int shift;
    FILE *output_file;

    printf("Ingrese el texto a cifrar: ");
    fgets(text, sizeof(text), stdin);
    text[strcspn(text, "\n")] = '\0'; // Eliminar salto de línea
    
    printf("Ingrese el desplazamiento: ");
    scanf("%d", &shift);
    
    encryptCaesar(text, shift);
    printf("Texto cifrado: %s\n", text);

    output_file = fopen("texto_cifrado.txt", "w");
    if (output_file == NULL) {
        printf("Error: No se pudo crear el archivo de salida.\n");
        return 1;
    }

    // Escribir el texto cifrado a un archivo de texto
    fprintf(output_file, "%s\n", text);
    fclose(output_file);
    
    printf("Texto cifrado guardado en 'texto_cifrado.txt'\n");
    

    return 0;
}