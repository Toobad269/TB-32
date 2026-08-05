/* MEMTEST -- Speichertest fuer den TB-32.
   Schreibt Muster in den freien Arbeitsspeicher und liest sie zurueck.
   Genau wie ein echtes Speichertestprogramm findet es damit defekte
   Speicherzellen und Adressleitungsfehler. */
#include "proglib.c"
#include "gfxlib.c"

/* Der Bereich muss FREI sein. Frueher stand hier 3 bis 9 MB -- da liegen
   inzwischen die Programmplaetze (ab 2,5 MB) und die Puffer von Paint und
   Word. Der Speichertest schrieb ihnen ihre Daten mit Mustern voll: Paint
   zeigte wirre Striche, und wer danach etwas anklickte, sass vor einer
   eingefrorenen Maschine. Jetzt oberhalb von allem. */
#define TEST_START 0x00A00000        /* 10 MB */
#define TEST_END   0x00E00000        /* 14 MB -- darunter der Ladeplatz */
#define SCHRITT    4096

int lauf(char* name, int muster, int adressmuster) {
    int addr; int fehler; int erwartet; int gelesen; int gezeigt;
    tf_text("  ");
    tf_text(name);
    tf_text(" ");
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
            tf_zeichen('.');
        }
    }
    if (fehler == 0) tf_text("  ok\n");
    else {
        tf_text("  FAILED, errors: ");
        tf_zahl(fehler);
        tf_zeichen(10);
    }
    return fehler;
}

int main() {
    int fehler; int start; int dauer; int kb;

    /* Frueher schrieb dieses Programm in die Textkonsole -- im
       Schreibtisch sah man davon nichts. Jetzt bekommt es ein
       Fenster mit Textausgabe. */
    if (tf_neu("MEMTEST") < 0) return 1;

    tf_leeren();
    tf_text("TOOBAD Memory Test 1.0\n\n");
    kb = (TEST_END - TEST_START) / 1024;
    tf_text("Testing ");
    tf_zahl(kb);
    tf_text(" KB from 0x00300000 to 0x00900000\n\n");

    start = ticks();
    fehler = 0;
    fehler = fehler + lauf("Pass 1  zero pattern    ", 0, 0);
    fehler = fehler + lauf("Pass 2  ones pattern    ", 0 - 1, 0);
    fehler = fehler + lauf("Pass 3  0x55555555      ", 1431655765, 0);
    fehler = fehler + lauf("Pass 4  0xAAAAAAAA      ", 0 - 1431655766, 0);
    fehler = fehler + lauf("Pass 5  address pattern ", 0, 1);
    dauer = ticks() - start;

    tf_zeichen(10);
    tf_text("Elapsed time: ");
    tf_zahl(dauer / 100);
    tf_text(".");
    tf_zahl((dauer % 100) / 10);
    tf_text(" seconds\n");
    if (fehler == 0) tf_text("\nResult: PASS -- no memory errors detected.\n");
    else {
        tf_text("\nResult: FAIL -- total errors: ");
        tf_zahl(fehler);
        tf_zeichen(10);
    }
    tf_text("\nPress any key to return to the command prompt.\n");
    getkey();
    tf_leeren();
    tf_warten();
    return 0;
}
