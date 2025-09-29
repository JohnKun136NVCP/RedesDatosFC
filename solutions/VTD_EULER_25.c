#include <stdio.h>
#include <string.h>

#define MAX_DIGITS 1000  //Cantidad maxima de digitos que debe contener

//Suma dos numeros grandes representados como arrays
void bigAdd(int a[], int b[], int result[]) {
    int carry = 0;
    for (int i = 0; i < MAX_DIGITS; i++) {
        int sum = a[i] + b[i] + carry;
        result[i] = sum % 10;
        carry = sum / 10;
    }
}

//Imprime un numero grande
void printBigint(int num[]) {
    int i = MAX_DIGITS - 1;
    while (i > 0 && num[i] == 0) i--;  //Ignora ceros iniciales
    for (i; i >= 0; i--) {
        printf("%d", num[i]);
    }
}

//Cuenta los digitos
int countDigits(int num[]) {
    int i = MAX_DIGITS - 1;
    while (i > 0 && num[i] == 0) i--;
    return i + 1;
}

int main() {
    int f1[MAX_DIGITS] = {0};
    int f2[MAX_DIGITS] = {0};
    int f3[MAX_DIGITS] = {0};

    f1[0] = 1;
    f2[0] = 1;

    int index = 2;

    while (1) {
        index++;
        bigAdd(f1, f2, f3);  //f3 = f1 + f2

        int length = countDigits(f3);

        if (length >= 1000) {
            printf("El indice del primer termino de Fibonacci con 1000 digitos es: %d\n", index);
            printf("No me crees? Confirmalo por ti mismo:\n");
            printBigint(f3);
            printf(" contiene %d digitos.\n", length);
            break;
        }

        memcpy(f1, f2, sizeof(f1));
        memcpy(f2, f3, sizeof(f2));
    }

    return 0;
}

