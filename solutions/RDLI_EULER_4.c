# include <stdio.h>

// A palindromic number reads the same both ways. The largest palindrome made from the product of two 
//-digit numbers is 9009 = 91 × 99.
//Find the largest palindrome made from the product of two 
//-digit numbers.

int main() {
    int max_palindrome = 0;
    for (int i = 100; i < 1000; i++) {
        for (int j = 100; j < 1000; j++) {
            int product = i * j;
            int reversed = 0, original = product;
            while (original > 0) {
                reversed = reversed * 10 + original % 10;
                original /= 10;
            }
            if (product == reversed && product > max_palindrome) {
                max_palindrome = product;
            }
        }
    }
    printf("The largest palindrome made from the product of two 3-digit numbers is: %d\n", max_palindrome);
    return 0;
}