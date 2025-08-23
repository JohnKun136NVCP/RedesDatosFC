/*
*   Se importan las bibliotecas usadas. Como la biblioteca estándar y funciones de uso de cadenas.
*/
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>


/*
 * Se hace el algortimo de encriptaado César. 
 * Resulta ser práctimanete al de desincriptado, pues se cambia un signo. 
 * Pues se tienen las siguientes fórmulas:
 * En(x) = (x + n) mód 26
 * En(x) = (x - n) mód 26  
 */
void encryptCaesar(char *text, int shift) {
    shift = shift % 26;
    for (int i = 0; text[i] != '\0'; i++) {
        char c = text[i];
        if (isupper(c)) {
            // Se cambia el signo aquí, en lugar de tener "-shift", se cambia a "+ shift"
            text[i] = ((c - 'A' + shift + 26) % 26) + 'A';
        } else if (islower(c)) {
            // Se cambia el signo aquí, en lugar de tener "-shift", se cambia a "+ shift"
            text[i] = ((c - 'a' + shift + 26) % 26) + 'a';
        } else {
            text[i] = c;
        }
    }
}

/**
 * Método principal, se utiliza la misma palabra que resultó en el ejercicio de la práctica.
 * Así pues,el cifrado debería ser el mismo que en el arhivo.
 */
int main(){
    // Palabra a cifrar
    char secret[] = "cherfrench";
    // Copia de la palabra
    char secret_copy[strlen(secret)+1];
    strcpy(secret_copy,secret); 
    // Shift usado en la práctica
    int shift = 42;
    encryptCaesar(secret,shift);
    printf("La palabra %s cifrada con un desplazamiento de %d es: %s \n",secret_copy,shift,secret);
    return 0;
}
