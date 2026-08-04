/* MEMTEST -- Speichertest fuer den TB-32.
   Schreibt Muster in den freien Arbeitsspeicher und liest sie zurueck.
   Genau wie ein echtes Speichertestprogramm findet es damit defekte
   Speicherzellen und Adressleitungsfehler. */
#include "proglib.c"

#define TEST_START 0x00300000        /* 3 MB -- oberhalb aller Systempuffer */
#define TEST_END   0x00900000        /* 9 MB */
#define SCHRITT    4096

int lauf(char* name, int muster, int adressmuster) {
    int addr; int fehler; int erwartet; int gelesen; int gezeigt;
    print("  ");
    print(name);
    print(" ");
    fehler = 0;
    gezeigt = 0;

    addr = TEST_START;
    while (addr < TEST_END) {
        erwartet = muster;
        if (adressmuster) erwartet = addr;
        mem_put(addr, erwartet);
        addr = addr + SCHRITT;
    }
    addr = TEST_START;
    while (addr < TEST_END) {
        erwartet = muster;
        if (adressmuster) erwartet = addr;
        gelesen = mem_get(addr);
        if (gelesen != erwartet) fehler++;
        addr = addr + SCHRITT;
        if ((addr - TEST_START) / SCHRITT > gezeigt * 128) {
            gezeigt++;
            putch('.');
        }
    }
    if (fehler == 0) printc("  ok\n", GREEN);
    else {
        printc("  FAILED, errors: ", RED);
        printn(fehler);
        nl();
    }
    return fehler;
}

int main() {
    int fehler; int start; int dauer; int kb;

    cls();
    printc("TOOBAD Memory Test 1.0\n\n", CYAN);
    kb = (TEST_END - TEST_START) / 1024;
    print("Testing ");
    printnc(kb, BRIGHT);
    print(" KB from 0x00300000 to 0x00900000\n\n");

    start = ticks();
    fehler = 0;
    fehler = fehler + lauf("Pass 1  zero pattern    ", 0, 0);
    fehler = fehler + lauf("Pass 2  ones pattern    ", 0 - 1, 0);
    fehler = fehler + lauf("Pass 3  0x55555555      ", 1431655765, 0);
    fehler = fehler + lauf("Pass 4  0xAAAAAAAA      ", 0 - 1431655766, 0);
    fehler = fehler + lauf("Pass 5  address pattern ", 0, 1);
    dauer = ticks() - start;

    nl();
    print("Elapsed time: ");
    printnc(dauer / 100, BRIGHT);
    print(".");
    printn((dauer % 100) / 10);
    print(" seconds\n");
    if (fehler == 0) printc("\nResult: PASS -- no memory errors detected.\n", GREEN);
    else {
        printc("\nResult: FAIL -- total errors: ", RED);
        printn(fehler);
        nl();
    }
    print("\nPress any key to return to the command prompt.\n");
    getkey();
    cls();
    return 0;
}
