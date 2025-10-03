#include <stdio.h>
#include <string.h>

/*
  Levi Iparrea Granados — IGL
  Project Euler 16: Suma de dígitos de 2^1000

  Cada iteración multiplica el número completo por 2 y administra el acarreo.
*/

int main(void) {
    /* 2^1000 tiene 302 dígitos aprox. (1000*log10(2) ≈ 301.03), dejo margen. */
    enum { MAXD = 400 };
    unsigned char d[MAXD];        /* d[0] es la unidad, d[1] decenas, etc. */
    int nd = 1;                   /* cantidad de dígitos usados */
    int i, k, carry, sum = 0;

    memset(d, 0, sizeof(d));
    d[0] = 1;  /* empezamos con el número 1 */

    /* Multiplicar por 2, 1000 veces -> 2^1000 */
    for (k = 0; k < 1000; ++k) {
        carry = 0;
        for (i = 0; i < nd; ++i) {
            int v = d[i] * 2 + carry;
            d[i] = (unsigned char)(v % 10);
            carry = v / 10;
        }
        while (carry) {
            d[nd++] = (unsigned char)(carry % 10);
            carry /= 10;
        }
    }

    /* Sumar los dígitos de 2^1000 */
    for (i = 0; i < nd; ++i) sum += d[i];

    /* Si quieres ver el número completo (opcional):
    for (i = nd - 1; i >= 0; --i) putchar('0' + d[i]);
    putchar('\n');
    */

    printf("%d\n", sum);
    return 0;
}
