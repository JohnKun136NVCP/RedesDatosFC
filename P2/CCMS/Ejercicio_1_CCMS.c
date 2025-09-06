#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

void caesarCrypt(char *text, int shift) {
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

    printf(text);
}

int main(int argc, char *argv[]){
    printf("Programa de encriptado caesar. \n");
    printf("Ingrese el texto que desea encriptar seguido del corrimiento que desea utilizar. \n");

    if (argc < 3) {
        printf("Uso: <texto> <corrimiento>\n", argv[0]);
        return 1;
    }
    
    char *word = argv[1];
    int corrimiento = atoi(argv[2]);

    printf("Su palabra cifrada es:");
    caesarCrypt(word, corrimiento);


}