// Función para cifrar utilizando César

// text: la cadena de texto a cifrar
// shift: número corrimientos en el alfabeto
void encryptCaesar(char *text, int shift) {

    // Nos aseguramos que el corrimiento quede dentro del rango del alfabeto, en este caso es 26 pero depende de cada alfabeto.
    shift = shift % 26;

    // Recorremos los caracteres hasta llegar a '\0'
    for (int i = 0; text[i] != '\0'; i++) {
        char c = text[i]; // Guardamos el carácter actual en una variable auxiliar

        // Verificamos si el carácter es mayúscula 
        if (isupper(c)) {
            // Aplicamos el desplazamiento, ahora SUMAMOS en vez de restar. 
            text[i] = ((c - 'A' + shift) % 26) + 'A';
        } 
        // Verificamos si el carácter es minúscula 
        else if (islower(c)) {
            // Aplicamos el desplazamiento, ahora SUMAMOS en vez de restar. 
            text[i] = ((c - 'a' + shift) % 26) + 'a';
        } 
        // Si no es una letra lo dejamos así porque no está en nuestro alfabeto 
        else {
            text[i] = c; 
        }
    }
}