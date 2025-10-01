#include <stdio.h>
#include <string.h>

#define MAX_DIGITOS 200

int main() {
    int digitos[MAX_DIGITOS];
    int cantidad = 1;  // Número de dígitos actuales
    int i, j, acarreo, suma = 0;

    // Inicializar con 1
    memset(digitos, 0, sizeof(digitos));
    digitos[0] = 1;

    // Calcular 100! multiplicando
    for (i = 2; i <= 100; i++) {
        acarreo = 0;
        for (j = 0; j < cantidad; j++) {
            int producto = digitos[j] * i + acarreo;
            digitos[j] = producto % 10;
            acarreo = producto / 10;
        }
        while (acarreo > 0) {
            digitos[cantidad] = acarreo % 10;
            acarreo /= 10;
            cantidad++;
        }
    }

    // Sumar los dígitos
    for (i = 0; i < cantidad; i++) {
        suma += digitos[i];
    }

    // Mostrar resultado
    printf("La suma de los dígitos de 100! es: %d\n", suma);
    
    return 0;
}