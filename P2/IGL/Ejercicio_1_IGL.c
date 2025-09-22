/* P2/IGL/Ejercicio 1 IGL.c
   Cifra con César: lee una palabra y un shift, e imprime el resultado.
*/
#include <stdio.h>
#include <ctype.h>

static void caesar_encrypt(char *s, int k) {
    k %= 26;
    for (int i = 0; s[i]; ++i) {
        unsigned char c = (unsigned char)s[i];
        if (c >= 'A' && c <= 'Z')      s[i] = (char)('A' + (c - 'A' + k) % 26);
        else if (c >= 'a' && c <= 'z') s[i] = (char)('a' + (c - 'a' + k) % 26);
    }
}

int main(void) {
    char text[256]; int k;
    printf("Texto a cifrar (una palabra): ");
    if (scanf("%255s", text) != 1) return 0;
    printf("Shift: ");
    if (scanf("%d", &k) != 1) return 0;
    caesar_encrypt(text, k);
    printf("Cifrado: %s\n", text);
    return 0;
}
