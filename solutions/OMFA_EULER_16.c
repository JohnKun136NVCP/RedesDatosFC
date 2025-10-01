#include <stdio.h>
#include <string.h>

#define MAX_DIGITS 310  // 2^1000 tiene 302 dígitos, dejamos margen

int main() {
    int digits[MAX_DIGITS];
    memset(digits, 0, sizeof(digits));

    digits[0] = 1;  // empezamos con 2^0 = 1
    int size = 1;   // número actual tiene 1 dígito

    // Calculamos 2^1000
    for (int i = 0; i < 1000; i++) {
        int carry = 0;
        for (int j = 0; j < size; j++) {
            int prod = digits[j] * 2 + carry;
            digits[j] = prod % 10;
            carry = prod / 10;
        }
        while (carry > 0) {
            digits[size] = carry % 10;
            carry /= 10;
            size++;
        }
    }

    // Suma de dígitos
    int sum = 0;
    for (int i = 0; i < size; i++) {
        sum += digits[i];
    }

    printf("La suma de los digitos de 2^1000 es: %d\n", sum);
    return 0;
}
