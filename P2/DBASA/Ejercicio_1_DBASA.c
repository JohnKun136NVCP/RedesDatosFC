#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

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

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Uso: %s <texto> <desplazamiento>\n", argv[0]);
        return 1;
    }

    char text[100];
    int shift = atoi(argv[2]);

    strncpy(text, argv[1], sizeof(text) - 1);
    text[sizeof(text) - 1] = '\0';

    char encrypted[100];
    strcpy(encrypted, text);
    encryptCaesar(encrypted, shift);
    printf("Texto encriptado: %s\n", encrypted);

    char decrypted[100];
    strcpy(decrypted, encrypted);
    decryptCaesar(decrypted, shift);
    printf("Texto desencriptado: %s\n", decrypted);

    return 0;
}

