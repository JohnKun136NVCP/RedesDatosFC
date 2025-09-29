#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>



int main() {
    uint64_t max = 0;
    uint64_t starting;
    for(int i = 1; i<1000000;i++){
        uint64_t n=i;
        uint64_t count =0;
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
