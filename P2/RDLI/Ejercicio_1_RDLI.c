//Usando el c ́odigo de descifrado C ́esar, realiza un c ́odigo en C para cifrar.

void encryptCaesar(char *text, int shift) {
        shift = shift % 26;
        for (int i = 0; text[i] != '\0'; i++) {
                char c = text[i];
                // Si la letra es mayuscula la desplazamos sobre las mayusculas 
                if (isupper(c)) {
                        text[i] = ((c - 'A' + shift) % 26) + 'A';
                // si la letra es miniscula la desplazamos sobre las minusculas
                } else if (islower(c)) {
                        text[i] = ((c - 'a' + shift) % 26) + 'a';
                } else {
                        text[i] = c;
                }
        }
}