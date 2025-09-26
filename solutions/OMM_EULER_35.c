#include <stdio.h>
#include <stdbool.h> 
#include <string.h>  
#include <math.h>   

#define LIMITE 1000000

void criba_de_eratostenes(bool es_primo[], int limite) {
    memset(es_primo, true, limite);

    es_primo[0] = false;
    es_primo[1] = false;

    for (int p = 2; p * p < limite; p++) {
        if (es_primo[p]) {
            for (int i = p * p; i < limite; i += p) {
                es_primo[i] = false;
            }
        }
    }
}

bool es_circular(int n, const bool es_primo[]) {
    int num_digitos = floor(log10(n)) + 1;
    int potencia_diez = pow(10, num_digitos - 1);
    int rotacion = n;

    for (int i = 0; i < num_digitos; i++) {
        if (!es_primo[rotacion]) {
            return false;
        }

        int primer_digito = rotacion / potencia_diez;
        int resto = rotacion % potencia_diez;
        rotacion = resto * 10 + primer_digito;
    }

    return true;
}

int main() {
    bool es_primo[LIMITE];
    criba_de_eratostenes(es_primo, LIMITE);

    int contador_circulares = 0;
    for (int i = 2; i < LIMITE; i++) {
        if (es_primo[i]) {
            if (es_circular(i, es_primo)) {
                contador_circulares++;
            }
        }
    }

    printf("Hay %d primos circulares por debajo de %d.\n", contador_circulares, LIMITE);

    return 0;
}
