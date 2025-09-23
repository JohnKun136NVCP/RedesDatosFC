#include <stdio.h>
#include <ctype.h>
#include <string.h>

//Funcion de desifrado de Cesar dada en el PDF de la practica
void decryptCaesar(char *text, int shift) {
    shift = shift % 26;
    for (int i = 0; text[i] != '\0'; i++) {
        char c = text[i];

        if (isupper(c)) {
            text[i] = ((c - 'A' - shift + 26) % 26) + 'A';
        } else if (islower(c)) {
            text[i] = ((c - 'a' - shift + 26) % 26) + 'a';
        } else {
            text[i] = c;
        }
    }
}

int main() {
    char text[1000];
    int shift;
    
    printf("Texto a cifrar: ");
    fgets(text, sizeof(text), stdin);
    text[strcspn(text, "\n")] = '\0'; 
    
    // Solicitar shift
    printf("Shift: ");
    scanf("%d", &shift);
    
    // Cifrar el texto
    decryptCaesar(text, shift);
    
    //Resultado
    printf("Texto cifrado: %s\n", text);
    
    return 0;
}