#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>

int main() {

    printf("¡Adivina el titulo del juego!\n");

    //Arreglo de titulos de juegos disponibles para adivinar.
    const char *juegos[] = {
        "hollowknight", "doom", "thebindingofisaac", "undertale", "deltarune"
        "halo", "thelastofus", "godofwar", "celste", "gearsofwar", "halflife"
        "rainworld", "teamfortress", "osu", "counterstrike","supersmashbros",
        "pokemon", "metalgear", "metroid","xenobladechronicles", "finalfantasy"
    };
    const int totaljuegos = 21;
    
    srand(time(NULL));
    int indice = rand() % totaljuegos;
    const char *juegosecreto = juegos[indice];
    
    //cadena de caracteres maxima para el juego adivinado.
    char juegoadivinado[50];
    int longitud = strlen(juegosecreto);
    
    //For para ocultar el titulo del juego con guiones.
    for (int i = 0; i < longitud; i++) {
        juegoadivinado[i] = '_';
    }
    juegoadivinado[longitud] = '\0'; //Caracter que marca el final de la cadena (null terminator).
    
    int intentos = 6;
    char letrasusadas[26] = {0}; //inicializamos el arreglo con valores siempre en 0.
    int letrasdescubiertas = 0;
    
    //Ciclo del juego.
    while ((intentos > 0) && letrasdescubiertas < longitud) {
        printf("Juego: %s\n", juegoadivinado);
        printf("Intentos restantes: %d\n", intentos);
        printf("Letras usadas: ");
        for (int i = 0; i < 26; i++) {
            if (letrasusadas[i]) {
                printf("%c ", 'a' + i);
            }
        }
        printf("\n");
        
        char letra;
        printf("Ingresa una letra: ");
        char input[10];
        fgets(input, sizeof(input), stdin); //Nos aseguramos de obtener toda la cadena con fgets para limpiar el buffer una vez se inserte la letra adivinada.
        letra = tolower(input[0]); //Pasamos a minuscula todas las letras.
        
        //Manejar si los caracteres escritos no son letras.
        if (letra < 'a' || letra > 'z') {
            printf("Por favor, ingresa una letra válida\n");
            continue;
        }
        //Notifica una letra ya utilizada
        if (letrasusadas[letra - 'a'] != 0) {
            printf("Ya usaste esa letra\n");
            continue;
        }
        
        letrasusadas[letra - 'a'] = 1; //Coloca 1 en el arreglo de 0 para indicar que la letra ya fue usada.
        int letraencontrada = 0;

        //for que actualiza las letras correctas del titulo de juego.
        for (int i = 0; i < longitud; i++) {
            if (juegosecreto[i] == letra) {
                juegoadivinado[i] = letra;
                letrasdescubiertas++;
                letraencontrada = 1;
            }
        }
        
        if (!letraencontrada) {
            intentos--;
            printf("Letra incorrecta!\n");
        }
    }
    
    printf("\n");
    if (letrasdescubiertas == longitud) {
        printf("¡Felicidades Adivinaste el juego!: %s", juegosecreto);
    } else {
        printf("¡Perdiste! El juego era: %s\n", juegosecreto);
    }
    
    return 0;
}