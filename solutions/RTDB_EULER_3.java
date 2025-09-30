public class RTDB_EULER_3{

    public static void main(String[] args) {
        long number = 600851475143L;
        long largestFactor = largestPrimeFactor(number);
        System.out.println("El factor primo más grande de " + number + " es " + largestFactor);
    }

    public static long largestPrimeFactor(long n) {
        long factor = 2;
        long largest = 1;
        while (n > 1) {
            if (n % factor == 0) {
                largest = factor;
                n /= factor;
                while (n % factor == 0) {
                    n /= factor;
                }
            }
            factor++;
        }
        return largest;
    }
}
