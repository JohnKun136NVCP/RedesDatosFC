#include <stdio.h>
#include <ctype.h>

// En lugar de restar el shift, solo sumamos para encriptar
void encryptCaesar(char *text, int shift) {
    shift = shift % 26;
    for (int i = 0; text[i] != '\0'; i++) {
	char c = text[i];
	if (isupper(c)) {
	    text[i] = ((c - 'A' + shift) % 26) + 'A';
	} else if (islower(c)) {
	    text[i] = ((c - 'a' + shift) % 26) + 'a';
	} else {
	    text[i] = c;
	}
    }
}

int main() {
    char text[] = "texto";
    int shift = 24;
    encryptCaesar(text, shift);
    printf("%s", text);
    return 0;
}
