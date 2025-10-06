#include <stdio.h>
#include <ctype.h>

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

int main(void) {
    char txt[256];
    int k;

    printf("Texto: ");
    if (!fgets(txt, sizeof(txt), stdin)) return 1;

    printf("Desplazamiento: ");
    if (scanf("%d", &k) != 1) return 1;

    encryptCaesar(txt, k);
    printf("Cifrado: %s", txt);
    return 0;
}
