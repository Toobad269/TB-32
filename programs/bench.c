/* BENCH -- Leistungsmessung des TB-32.
   Misst, wie viele Rechenschritte der Prozessor pro Sekunde schafft, und
   rechnet das in eine Taktrate um. Das Ergebnis haengt davon ab, was im
   BIOS-Setup unter "CPU Clock Speed" eingestellt ist. */
#include "proglib.c"
#include "gfxlib.c"

int main() {
    int start; int dauer; int i; int summe; int n; int prim; int j;
    int schleifen; int primzahlen; int adr;

    /* Frueher schrieb dieses Programm in die Textkonsole -- im
       Schreibtisch sah man davon nichts. Jetzt bekommt es ein
       Fenster mit Textausgabe. */
    if (tf_neu("BENCH") < 0) return 1;

    tf_leeren();
    tf_text("TOOBAD Performance Benchmark 1.0\n\n");

    /* --- 1. Ganzzahl-Rechenleistung --- */
    tf_text("Integer arithmetic ..... ");
    start = ticks();
    summe = 0;
    for (i = 0; i < 60000; i++) summe = summe + i * 3 - (i / 2);
    dauer = ticks() - start;
    if (dauer == 0) dauer = 1;
    schleifen = 60000 / dauer;
    tf_zahl(schleifen * 100);
    tf_text(" iterations/s\n");

    /* --- 2. Primzahlen (Sprungvorhersage, Division) --- */
    tf_text("Prime sieve ............ ");
    start = ticks();
    primzahlen = 0;
    for (n = 2; n < 4000; n++) {
        prim = 1;
        for (j = 2; j * j <= n; j++) if (n % j == 0) { prim = 0; break; }
        if (prim) primzahlen++;
    }
    dauer = ticks() - start;
    if (dauer == 0) dauer = 1;
    tf_zahl(primzahlen);
    tf_text(" primes in ");
    tf_zahl(dauer * 10);
    tf_text(" ms\n");

    /* --- 3. Speicherdurchsatz --- */
    tf_text("Memory throughput ...... ");
    start = ticks();
    adr = 0x00400000;
    for (i = 0; i < 40000; i++) mem_put(adr + (i & 4092), i);
    dauer = ticks() - start;
    if (dauer == 0) dauer = 1;
    tf_zahl((40000 * 4) / (dauer * 10));
    tf_text(" KB/s\n");

    /* --- Gesamtergebnis --- */
    tf_zeichen(10);
    tf_text("Estimated clock speed .. ");
    tf_zahl(schleifen * 100 / 90);
    tf_text(" kHz\n");
    tf_text("Configured in BIOS ..... ");
    portout(0x70, 0x13);
    i = portin(0x71);
    if (i == 0) tf_text("0.4 MHz\n");
    if (i == 1) tf_text("1 MHz\n");
    if (i == 2) tf_text("2 MHz\n");
    if (i == 3) tf_text("4 MHz\n");
    if (i == 4) tf_text("8 MHz\n");
    tf_text("\nTip: change the clock in BIOS setup (DEL at boot) and run again.\n");
    tf_text("\nPress any key to return to the command prompt.\n");
    getkey();
    tf_leeren();
    tf_warten();
    return 0;
}
