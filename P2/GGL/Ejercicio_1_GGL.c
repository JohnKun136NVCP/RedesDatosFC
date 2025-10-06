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
        }
    }
}

int main() {
    char text[100];
    int shift;
    
    printf("Texto a cifrar: ");
    fgets(text, sizeof(text), stdin);
    text[strcspn(text, "\n")] = '\0';
    
    printf("Desplazamiento: ");
    scanf("%d", &shift);
    
    encryptCaesar(text, shift);
    printf("Texto cifrado: %s\n", text);
    
    return 0;
}