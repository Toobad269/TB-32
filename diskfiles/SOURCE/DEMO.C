/* DEMO.C -- Beispielprogramm. Uebersetzen mit:  CC DEMO.C DEMO.TBX
   Dann starten mit:  DEMO                                          */

int fib(int n) {
    if (n < 2) return n;
    return fib(n - 1) + fib(n - 2);
}

int main() {
    int i;
    cls();
    printc("C compiler running on the TB-32\n\n", 11);

    print("Fibonacci: ");
    for (i = 0; i < 12; i++) {
        printn(fib(i));
        print(" ");
    }
    print("\n\nPress a key.\n");
    getkey();
    return 0;
}
