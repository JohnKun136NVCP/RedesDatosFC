// Para poder incluir las funciones de entrada y salida
#include <stdio.h>

// Program simple que usa la función fgets para almacenar en un buffer declarado
// la entrada dada por el usuario/programa y está declarado de manera que solo
// el primer caracter se imprima a la salida estandar.
int main() {
    // Se crea un buffer en donde se guardará el input
    char buffer[2];
    printf("Escribe cualquier cosa: ");
    // Dado que fgets puede regresar NULL si algo falla o termina de leer una linea
    // entonces agregamos la condición del if
    // Se le tiene que pasar el buffer, el tamaño del buffer y la entrada estandar
    // que se le pide al usuario cuando ejecuta el programa.
    if (fgets(buffer, sizeof(buffer), stdin) != NULL)
	// Simplemente imprime la primera letra o carácter del input pues
	// el buffer que declaro es uno de tamaño 2.
	printf("El primer caracter de lo que escribiste es: %s", buffer);
    else
	//En caso de que fgets falle o termine de leer
	printf("Esto no debería en principio pasar");
}
