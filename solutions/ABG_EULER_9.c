#include <stdio.h>

int main() {
    int a, b, c;
    int found = 0;

    for (a = 1; a < 1000/3; a++) {
        for (b = a+1; b < 1000/2; b++) {
            c = 1000 - a - b;
            if (a*a + b*b == c*c) {
                printf("a=%d, b=%d, c=%d\n", a, b, c);
                printf("Producto abc = %d\n", a*b*c);
                found = 1;
                break;
            }
        }
        if (found) break;
    }

    if (!found) {
        printf("No se encontro el triplete.\n");
    }

    return 0;
}
