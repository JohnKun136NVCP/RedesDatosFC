#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h> 

#define DIGITOS 10		 // Vamos a trabajar con los números del 0 al 9
#define PROCESADO -1	// Una marca para saber que un número ya lo usamos y no repetirlo.

int main() {

    // abrimos el archivo keylog.txt para leerlo
    FILE *f = fopen("ASAJ_EULER_79_keylog.txt", "r");
    // si por alguna razón no se puede, avisamos y salimos.
    if (!f) {
        perror("No se pudo abrir el archivo");
        return 1;
    }

    // la idea es pensarlo como una gráfica dirigida. 
    int grafica[DIGITOS][DIGITOS] = {0};   // matriz de adyacencia. Si grafica[3][1] = 1, el 3 va antes que el 1.
    int grado_entrada[DIGITOS] = {0};     // un contador para cada número. Nos dice cuántos otros números deben ir antes que él.
    bool visto[DIGITOS] = {false};        // nos dice cuáles números forman parte de la contraseña y cuáles no.


	// leemos el archivo línea por línea hasta que se acabe
    char linea[5];
    while (fgets(linea, sizeof(linea), f)) {
        // convertimos los caracteres '3' en los números de verdad 3.
        int a = linea[0] - '0';
        int b = linea[1] - '0';
        int c = linea[2] - '0';

        // anotamos que los leídos sí existen en la contraseña
        visto[a] = visto[b] = visto[c] = true;

        // si leímos "319", sacamos que el 3 va antes que el 1.
        if (!grafica[a][b]) { 		// si no lo sabíamos 
            grafica[a][b] = 1;     // lo anotamos en la tabla
            grado_entrada[b]++;    // le sumamos uno al contador de b, porque ahora tiene un número más antes que él.
        }
        // y también sacamos que el 1 va antes que el 9
        if (!grafica[b][c]) {
            grafica[b][c] = 1;
            grado_entrada[c]++;
        }
    }
    // ya terminamos de leer los números
    fclose(f); 

    // ahora usamos la info adquirida para armar la contraseña
    char resultado[DIGITOS + 1]; // espacio para ir construyendo la respuesta final.
    int len = 0;
    bool se_encontro_digito;

    // vamos a repetir este proceso una y otra vez hasta que hayamos colocado todos los números.
    do {
        se_encontro_digito = false;
        // buscamos en todos los números, del 0 al 9
        for (int i = 0; i < DIGITOS; i++) {
            // un número que sí exista y que no tenga a nadie antes que él
            if (visto[i] && grado_entrada[i] == 0) {
                // lo ponemos en el siguiente hueco de nuestra respuesta.
                resultado[len++] = '0' + i;

                // lo marcamos como usado  para no volver a agarrarlo.
                grado_entrada[i] = PROCESADO;
                se_encontro_digito = true;

                // a todos los números que iban después de él, les restamos uno en su contador
                for (int j = 0; j < DIGITOS; j++) {
                    if (grafica[i][j]) {
                        grado_entrada[j]--;
                    }
                }

                // salimos y empezamos la búsqueda de nuevo desde el 0, si hay varios números libres, siempre escogemos el más pequeño primero.
                break;
            }
        }
    } while (se_encontro_digito);


    resultado[len] = '\0'; 

    printf("El passcode es: %s\n", resultado);
    return 0;
}
