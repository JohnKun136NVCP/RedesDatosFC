#include "Ejercicio_1_LARE.h"
#include <stdio.h>
#include <stdlib.h>
// Nuestro Buffer para leer
#define MAX_LEN 100

int main(int argc, char *argv[]) {
  if (argc != 3) {
    printf("Type: %s <archive> <number> \n The number must be between 1 "
           "and 26\n",
           argv[0]);
    return 1;
  }

  FILE *stream;
  FILE *exit;
  int shift = atoi(argv[2]) % 26;
  char line[MAX_LEN];

  // Generamos un archivo temporal donde guardaremos el contenido de la entrada
  // ya encriptado
  stream = fopen(argv[1], "r");
  exit = fopen("temp.txt", "w"); // El archivo temporal
  // Leemos de 100 en 100 caracteres del archivo para su encriptacion
  while (fgets(line, MAX_LEN, stream) != NULL) {
    encryptCaesar(line, shift); // Encriptamos...
    fprintf(exit, "%s",
            line); // Una vez encriptado, escribimos en nuestra salida tempora;
  }

  // Terminando de leer y escribir, cerramos el flujo en ambos archivos de texto
  fclose(stream);
  fclose(exit);

  // Borramos el archivo original ya que no esta encriptado y renombramos al
  // temporal como el original
  remove(argv[1]);
  rename("temp.txt", argv[1]); // El archivo ya esta encriptado

  return 0;
}
