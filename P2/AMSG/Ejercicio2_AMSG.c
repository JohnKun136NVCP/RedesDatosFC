#include<stdio.h>
// referencia: https://www.digitalocean.com/community/tutorials/fgets-and-gets-in-c-programming#1-read-from-a-given-file-using-fgets

int main(){
    char string[50];
    printf("Dime tu nombre completo porfavor: ");
    fgets(string,50,stdin);
    printf("\nHola y buenos dias, tardes y noches %s",string);
    return 0;
}
