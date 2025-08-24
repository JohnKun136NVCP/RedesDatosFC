#include "Ejercicio_1_LARE.h"
#include <stdio.h>
#include <stdlib.h>

#define MAX_LEN 100

int main(int argc, char *argv[]) {
  if (argc != 3) {
    printf("Type: <archive> <number> \n The number must be between 1 and 26");
    return 1;
  }

  FILE *stream;
  FILE *exit;
  int shift = atoi(argv[2]) % 26;
  char line[MAX_LEN];

  stream = fopen(argv[1], "r");
  exit = fopen("temp.txt", "w");

  while (fgets(line, MAX_LEN, stream) != NULL) {
    encryptCaesar(line, shift);
    fprintf(exit, "%s", line);
  }

  fclose(stream);
  fclose(exit);

  remove(argv[1]);
  rename("temp.txt", argv[1]);

  return 0;
}
