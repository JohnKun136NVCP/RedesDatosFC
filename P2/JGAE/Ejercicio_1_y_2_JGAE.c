#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Colores ANSI que ocupé para embellecer texto en terminal
#define BLANCO "\033[0m"
#define MORADO "\033[0;35m"
#define ROJO   "\033[0;31m"

/**
 * Función de cifrado y descifrado de César
 * @param text EL texto a cifrar o descifrar
 * @param shift el desplazamiento que se hará
 * @param cifrar 1 para cifrar y 0 para descifrar
 * 
 * Esta función mezcla el cifrado y descifrado para evitar código redundante, lo único que hace es cambiar el signo del desplazamiento
 */
void caesar(char *text, int shift, int cifrar) {
    // Si vamos a cifrar el desplazamiento es positivo, en caso contrario es negativo, 
    // entonces uso el operador ternario ? que hace justamente eso 
    int desplazamiento = cifrar ? shift : -shift; 
    desplazamiento = desplazamiento % 26; 
    for (int i = 0; text[i] != '\0'; i++) {
        char c = text[i]; 
        if (isupper(c)) { 
            // Al restar la 'A' obtenemos el índice en el alfabeto 
            // Luego aplicamos el desplazamiento (ya sea positivo o negativo) con el módulo 26 
            // para saber el caracter que le toca aún cuando da una vuelta completa o más 
            text[i] = ((c - 'A' + desplazamiento + 26) % 26) + 'A'; 
        } else if (islower(c)) { 
            // Lo mismo que el caso anterior pero ahora con minúsculas 
            text[i] = ((c - 'a' + desplazamiento + 26) % 26) + 'a'; 
        }
         // Todos los demás los dejamos igual 
    } 
        
} 

/**
 * 
 * 
 * Esta función muestra el menú del programa, y devuelve la opción seleccionada para cifrar, descifrar o cortar el flujo del programa
 */
int Menu(){
    int opcion;
    printf(MORADO "Cifrado y descifrado\nSeleccione una opción:\n" BLANCO);
    printf("1. Cifrar texto\n");
    printf("2. Descifrar texto\n");
    printf("3. Salir\n");

    // Leemos la opción; si el usuario mete letras, scanf no asigna y devuelve 0
    if (scanf("%d", &opcion) != 1) {
        // Entrada inválida: vaciamos lo que haya quedado en la línea
        while (getchar() != '\n' && !feof(stdin));
        return -1; // opción inválida para el flujo principal
    }

    // Limpiamos el buffer de entrada (comes el '\n' que dejó Enter)
    while (getchar() != '\n' && !feof(stdin));
    return opcion;
}

/**
 * Función para ejercicio 2, uso de fgets()
 * 
 * Esta función usa la función auxiliar fgets() para obtener un texto de la terminal.
 * Cambié la firma para **recibir** el buffer desde afuera (el main lo declara).
 * Así evitamos usar `static` y no devolvemos un puntero a memoria que ya no existe.
 * 
 * @param buff  arreglo donde vamos a guardar el texto
 * @param tam   tamaño de ese arreglo (para no pasarnos)
 * @return      regresa el mismo puntero buff si salió bien, o NULL si hubo error
 */
char* obtenTexto(char *buff, size_t tam){
    printf("Introduzca el texto: ");

    // Usamos fgets para leer hasta tam-1 caracteres (reserva 1 para el '\0')
    if (fgets(buff, tam, stdin) == NULL) {
        return NULL; // EOF o error
    }

    // fgets deja el '\n' si cupo; lo corto para que no estorbe al cifrar ni al imprimir
    buff[strcspn(buff, "\n")] = '\0';

    return buff;
}


/**
 * Función para obtener el desplazamiento del usuario
 * (uso scanf para enteros, y limpio el buffer para no pelearme con fgets)
 */
int obtenDesplazamiento(){
    printf("Introduzca el desplazamiento: ");
    int desplazamiento;

    if (scanf("%d", &desplazamiento) != 1) {
        // Si el usuario escribió algo que no es número
        while (getchar() != '\n' && !feof(stdin)); // limpio la línea mala
        printf(ROJO "Entrada inválida, debe ser un entero.\n" BLANCO);
        return __INT_MAX__; //para cachar los errores
    }

    // Limpio el '\n' para que la siguiente fgets no lea una cadena vacía
    while (getchar() != '\n' && !feof(stdin));

    return desplazamiento;
}

/**
 * Flujo principal del programa para cifrar y descifrar texto con cifrado César
 */
int main() {
    // Variables y selección del cifrado o descifrado César
    int opcion = -1;
    int desplazamiento;
    char texto[256]; //buffer del texto

    // Flujo continúa hasta que el usuario quiera salir, mientras puede seguir cifrando o descifrando
    while (opcion != 3) {
        opcion = Menu();

        if (opcion == 1 || opcion == 2) {
            desplazamiento = obtenDesplazamiento();
            if (desplazamiento == __INT_MAX__)
            {
               continue;
            }
            
            // Si quisieras validar rango, aquí podrías limitarlo, pero con módulo da igual

            // Solicitamos el texto guardándolo en el buffer declarado
            if (obtenTexto(texto, sizeof(texto)) == NULL) {
                printf(ROJO "No se pudo leer el texto \n" BLANCO);
                continue;
            }

            if (opcion == 1) {
                caesar(texto, desplazamiento, 1);
                printf("Texto cifrado: %s\n", texto);
            } else { // opcion == 2
                caesar(texto, desplazamiento, 0);
                printf("Texto descifrado: %s\n", texto);
            }

        } else if (opcion == 3) {
            printf("Saliendo del programa...\n");
            return 0;

        } else {
            // Solo caemos aquí cuando la opción no es 1, 2 ni 3 (o cuando scanf falló)
            printf(ROJO "Opción no válida\n" BLANCO);
        }
    }

    return 0;
}
