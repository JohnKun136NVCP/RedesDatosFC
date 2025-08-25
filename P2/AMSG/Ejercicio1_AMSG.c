#include <stdio.h>
#include <ctype.h>

// funcion orignal
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


// solo se modifica la funcion del codigo original
// se cambia que se sume el shift en vez de que se reste y ya
void ecryptCaesar(char *text, int shift) {
	shift = shift % 26; // alfabeto de 26 letras
	for (int i = 0; text[i] != '\0'; i++) { 
		char c = text[i]; // los string son arreglos de char
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
	char texto[50] = "hola mundo";	
	int shift = 10;

	// PRUEBA
	ecryptCaesar(texto, shift);
    	printf("Texto cifrado: %s\n", texto);

        decryptCaesar(texto, shift);
    	printf("Texto DECIFRADO: %s\n", texto);

}

