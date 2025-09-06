#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>       // Entrada/salida est ́andar (printf, fopen, etc.)
#include <stdlib.h>      // Funciones generales (exit, malloc, etc.)
#include <string.h>      // Manejo de cadenas (strlen, strcpy, etc.)
#include <ctype.h>
#include <stdbool.h>     // Tipo booleano (true/false)
#define PORT 7006        // Puerto en el que el servidor escucha
#define BUFFER_SIZE 1024 // Tamaño del buffer para recibir datos


/* Función de cifrado César a la inversa */
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

/* Función de cifrado César
   Nos movemos del lado opuesto ahora*/
void cryptCaesar(char *text, int shift) {
        shift = shift % 26;
        for (int i = 0; text[i] != '\0'; i++) {
                char c = text[i];
                if (isupper(c)) {
                        text[i] = ((c - 'A' + shift + 26) % 26) + 'A';
                } else if (islower(c)) {
                        text[i] = ((c - 'a' + shift + 26) % 26) + 'a';
                } else {
                        text[i] = c;
                }
        }
}

int main() {
        char text[] = "Mensaje original";
        printf(text);
        printf("\n");
        cryptCaesar(text, 10);
        printf(text);
        printf("\n");
        decryptCaesar(text, 10);
        printf(text);
        printf("\n");
}