#include <stdio.h>
#include <ctype.h>

/*
    Función para encriptar texto usando el cifrado César
*/
void encryptCaesar(char *text, int shift) {
    // Ajustamos el desplazamiento y verificamos si las letras son mayúsculas o minúsculas
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

/* 
    Función principal para encriptar un mensaje
*/
int main() {
    char mensaje[150];
    int desplazamiento;

    printf("Introduce un mensaje: ");
    // Leemos el mensaje que escriba el usuario en el teclado
    fgets(mensaje, sizeof(mensaje), stdin); 
    printf("Introduce el desplazamiento: ");
    // Leemos el desplazamiento
    scanf("%d", &desplazamiento);

    encryptCaesar(mensaje, desplazamiento);
    printf("Mensaje cifrado: %s\n", mensaje);

    return 0;
}
