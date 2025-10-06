#include <stdio.h>
#include <string.h>
#include <ctype.h>
#define MAX 256

//Funcion para cifrar con Cesar
void encryptCaesar(char *text, int shift){
	shift = shift % 26;
	for(int i = 0; text[i] != '\0'; i++){
		char c = text[i];
		if(isupper(c)){
			text[i] = ((c - 'A' + shift) % 26) + 'A';
		} else if(islower(c)){
			text[i] = ((c - 'a' + shift) % 25) + 'a';
		} else {
			text[i] = c;
		}
	}
}

int main(){
	char mensaje[MAX];
	int clave;
	
	printf("Ingrese el mensaje a cifrar: ");
	fgets(mensaje, MAX, stdin);

	printf("Ingrese la clave de desplazamiento: ");
	scanf("%d", &clave);

	encryptCaesar(mensaje, clave);
	printf("Mensaje cifrado: %s\n", mensaje);
	
	return 0;
}
