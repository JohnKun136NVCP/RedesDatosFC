#include <stdio.h>

/*
    Le pregunta al usuario cuantos caracteres leer de monologue.txt
    e imprime el resultado.
*/
int main() {
    // Se recibe la cantidad de caracteres a leer, debe ser entre 0 y 976
    int cantidad = -1;
    while(cantidad < 0) {
        printf("Escribe la cantidad de caracteres a leer (maximo 976): \n");
        scanf("%d", &cantidad);
        cantidad = (cantidad > 976) ? -1 : cantidad + 1;
    }

    // Se abre el archivo monologue.txt para leer con fopen
    FILE *archivo = fopen("monologue.txt", "r");
    // Lee la cantidad deseada de caracteres
    char buff[cantidad];
    fgets(buff, sizeof(buff), archivo);
    // Imprime lo leído y cierra el buffer
    printf("%s", buff);
    fclose(archivo);
    return 0;
}