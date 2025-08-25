#include <stdio.h>
#include <string.h> 

int main() {
    char nombre[50]; 
    char edad[10];   

    // Pedimos que el usuario ingrese su nombre
    printf("¿Cómo te llamas? ");
    // Utilizamos fgets, como parámetros tiene: el buffer donde se guardará la entrada, el tamaño de la entrada y de donde sacará la entrada (teclado en este caso)
    fgets(nombre, sizeof(nombre), stdin);
    // fgets siempre mete el '\n' al final, lo quitamos para que quede bien con el mensaje final 
    nombre[strcspn(nombre, "\n")] = '\0';

    // Pedimos que el usuario ingrese su edad
    printf("¿Cuántos años tienes? ");
    fgets(edad, sizeof(edad), stdin);
    edad[strcspn(edad, "\n")] = '\0';

    // Invertimos nombre con edad como el meme 
    printf("Hola %s, tienes %s años.\n", edad, nombre);

    return 0;
}
