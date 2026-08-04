/* BENCH -- Leistungsmessung des TB-32.
   Misst, wie viele Rechenschritte der Prozessor pro Sekunde schafft, und
   rechnet das in eine Taktrate um. Das Ergebnis haengt davon ab, was im
   BIOS-Setup unter "CPU Clock Speed" eingestellt ist. */
#include "proglib.c"

int main() {
    int start; int dauer; int i; int summe; int n; int prim; int j;
    int schleifen; int primzahlen; int adr;

    cls();
    printc("TOOBAD Performance Benchmark 1.0\n\n", CYAN);

    /* --- 1. Ganzzahl-Rechenleistung --- */
    print("Integer arithmetic ..... ");
    start = ticks();
    summe = 0;
    for (i = 0; i < 60000; i++) summe = summe + i * 3 - (i / 2);
    dauer = ticks() - start;
    if (dauer == 0) dauer = 1;
    schleifen = 60000 / dauer;
    printnc(schleifen * 100, BRIGHT);
    print(" iterations/s\n");

    /* --- 2. Primzahlen (Sprungvorhersage, Division) --- */
    print("Prime sieve ............ ");
    start = ticks();
    primzahlen = 0;
    for (n = 2; n < 4000; n++) {
        prim = 1;
        for (j = 2; j * j <= n; j++) if (n % j == 0) { prim = 0; break; }
        if (prim) primzahlen++;
    }
    dauer = ticks() - start;
    if (dauer == 0) dauer = 1;
    printnc(primzahlen, BRIGHT);
    print(" primes in ");
    printn(dauer * 10);
    print(" ms\n");

    /* --- 3. Speicherdurchsatz --- */
    print("Memory throughput ...... ");
    start = ticks();
    adr = 0x00400000;
    for (i = 0; i < 40000; i++) mem_put(adr + (i & 4092), i);
    dauer = ticks() - start;
    if (dauer == 0) dauer = 1;
    printnc((40000 * 4) / (dauer * 10), BRIGHT);
    print(" KB/s\n");

    /* --- Gesamtergebnis --- */
    nl();
    print("Estimated clock speed .. ");
    printnc(schleifen * 100 / 90, BRIGHT);
    print(" kHz\n");
    print("Configured in BIOS ..... ");
    portout(0x70, 0x13);
    i = portin(0x71);
    if (i == 0) print("0.4 MHz\n");
    if (i == 1) print("1 MHz\n");
    if (i == 2) print("2 MHz\n");
    if (i == 3) print("4 MHz\n");
    if (i == 4) print("8 MHz\n");
    print("\nTip: change the clock in BIOS setup (DEL at boot) and run again.\n");
    print("\nPress any key to return to the command prompt.\n");
    getkey();
    cls();
    return 0;
}
