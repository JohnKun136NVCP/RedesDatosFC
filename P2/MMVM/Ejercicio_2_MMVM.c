#include <stdio.h>

// Programa para leer un archivo completo linea por linea
int main() {

  // Almacenar cada linea y un puntero al archivo
  char buf[256];
  FILE *fp;

  //Abrir archivo "entrada.txt"
  fp = fopen("entrada.txt", "r");
  
  // Verificar que exista un archivo llamado "entrada.txt"
  if (fp == NULL) {
    printf("No se pudo abrir el archivo.\n");
    return 1;
  }
  
  printf("Contenido del archivo:\n");
  
  // fgets() lee linea por linea hasta llegar al final del archivo
  while (fgets(buf, sizeof(buf), fp) != NULL) {
    // fgets incluye el salto de linea al final, por lo que solo mostramos lo que trae
    printf("%s", buf);
  }
  
  // Cerramos el archivo despues de leerlo
  fclose(fp);
  
  return 0;
  // El fin :)
}
