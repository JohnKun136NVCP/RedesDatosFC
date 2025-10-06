#include <stdio.h>
#define MAX 100

int main(){
	char texto[MAX];

	printf("Escriba un texto: ");
	fgets(texto, MAX, stdin); //fgets() lee una cadena desde stdin, que sera lo ingresado por el teclado 
	
	printf("Texto ingresado: %s\n", texto);
	return 0;
}
