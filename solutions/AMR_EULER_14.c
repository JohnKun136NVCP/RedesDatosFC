#include <stdio.h>

int main() {
    u_int64_t max = 0;
    u_int64_t starting;
    for(int i = 1; i<1000000;i++){
        u_int64_t n=i;
        u_int64_t count =0;
        while(n != 1){
            if (n % 2 == 0)
                n = n/2;
            else 
                n = (3*n+1);
            count++;
        }
        if (count > max){
            max = count;
            starting = i;
        }
   }
   printf("El número que produce la mayor cadena es: %lld\n",starting);
}
