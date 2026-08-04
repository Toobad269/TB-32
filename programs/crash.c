/* ==========================================================================
   CRASH  --  Stresstest und Fehlerinjektor fuer den TB-32

   Zwei Sorten von Bosheit:

   HARMLOS -- die Maschine ueberlebt es, man sieht ihr nur beim Leiden zu:

     HEAT    Rechnet ohne Pause. Das Waermemodell heizt mit Takt mal
             Auslastung; ab 85 Grad drosselt der Chipsatz von selbst, ab
             105 Grad schaltet der Rechner zum Selbstschutz ab. Genau so
             verhaelt sich jeder heutige Prozessor.
     COLORS  Schreibt die Farbtabelle der Grafikkarte mit Zufall voll. Die
             Bilddaten bleiben unveraendert -- es ist nur die Zuordnung
             "Zahl im Bildspeicher -> Farbe auf dem Schirm", die verrueckt
             spielt. Der Schreibtisch laeuft normal weiter, sieht aber aus
             wie ein kaputtes Netzteil.
     FLICKER Setzt die Farbtabelle im Wechsel auf Schwarz und zurueck --
             das Bild flackert.
     FIX     Stellt die normale Farbtabelle wieder her.

   ECHTE ABSTUERZE -- danach haelt die CPU an, Strg+R startet neu:

     Division durch Null, Pufferueberlauf (die Ruecksprungadresse wird
     ueberschrieben), Ruecksprung nach Adresse 0, endlose Rekursion und
     Ueberschreiben des Kernels.

   Aufruf ohne Argument: Menue. Mit Argument gleich losgelegt --
   und das Spannende daran:

       START CRASH.TBX HEAT /B
       START CRASH.TBX COLORS /B

   Mit /B laeuft es im HINTERGRUND. Der Schreibtisch bleibt bedienbar,
   waehrend die Temperatur steigt oder die Farben durchdrehen. Beenden mit
   TASKLIST und TASKKILL.

   Es geht nichts kaputt: die Festplatte wird nicht angefasst.

   Uebersetzen auf dem Geraet:   CD SOURCE   und   CC CRASH.C CRASH.TBX
   ========================================================================== */

#include "proglib.c"

#define P_PAL_IDX   0x42
#define P_PAL_VAL   0x43
#define P_TEMP      0xA0
#define P_FAN       0xA1
#define P_THROTTLE  0xA2
#define P_TEMPLIMIT 0xA3
#define P_TEMPMAX   0xA5

#define PROG_ARGS   0x00008200

int tiefe = 0;

/* ==========================================================================
   Farbtabelle
   ========================================================================== */

void pal_set(int nr, int rot, int gruen, int blau) {
    portout(P_PAL_IDX, nr);
    portout(P_PAL_VAL, (rot << 16) | (gruen << 8) | blau);
}

/* Die normale Tabelle: 16 klassische PC-Farben, dann ein Wuerfel aus
   6*6*6 Mischungen, zum Schluss Graustufen. Genau die legt auch die
   Grafikkarte beim Einschalten an. */
int pal_norm16(int i) {
    if (i ==  0) return 0x000000;
    if (i ==  1) return 0x0000AA;
    if (i ==  2) return 0x00AA00;
    if (i ==  3) return 0x00AAAA;
    if (i ==  4) return 0xAA0000;
    if (i ==  5) return 0xAA00AA;
    if (i ==  6) return 0xAA5500;
    if (i ==  7) return 0xAAAAAA;
    if (i ==  8) return 0x555555;
    if (i ==  9) return 0x5555FF;
    if (i == 10) return 0x55FF55;
    if (i == 11) return 0x55FFFF;
    if (i == 12) return 0xFF5555;
    if (i == 13) return 0xFF55FF;
    if (i == 14) return 0xFFFF55;
    return 0xFFFFFF;
}

void pal_restore() {
    int i; int r; int g; int b; int n; int v;
    for (i = 0; i < 16; i++) {
        portout(P_PAL_IDX, i);
        portout(P_PAL_VAL, pal_norm16(i));
    }
    n = 16;
    for (r = 0; r < 6; r++) {
        for (g = 0; g < 6; g++) {
            for (b = 0; b < 6; b++) {
                pal_set(n, r * 51, g * 51, b * 51);
                n++;
            }
        }
    }
    while (n < 256) {
        v = (n - 232) * 10 + 8;
        if (v > 255) v = 255;
        pal_set(n, v, v, v);
        n++;
    }
}

void pal_schwarz() {
    int i;
    for (i = 0; i < 256; i++) pal_set(i, 0, 0, 0);
}

/* ==========================================================================
   HARMLOS 1: heiss rechnen
   ========================================================================== */

/* Etwas, das der Compiler nicht wegoptimieren kann und das nur den
   Prozessor beschaeftigt -- keine Systemaufrufe, kein Warten. */
int malochen(int runden) {
    int i; int x;
    x = 12345;
    for (i = 0; i < runden; i++) {
        x = x * 1103515245 + 12345;
        x = x ^ (x >> 7);
        x = x + i;
    }
    return x;
}

void zeige_temperatur(int start) {
    int t; int dr; int farbe;
    t = portin(P_TEMP);
    setcursor(6, 10); print("Temperature .... ");
    printn(t / 10); putch('.'); printn(t % 10); print(" C   ");
    setcursor(6, 11); print("Fan ............ ");
    printn(portin(P_FAN)); print(" %    ");
    setcursor(6, 12); print("Throttling ..... ");
    dr = portin(P_THROTTLE);
    farbe = GREEN;                       /* CC auf dem Geraet kennt kein ?: */
    if (dr) farbe = RED;
    printnc(dr, farbe); print(" %    ");
    setcursor(6, 13); print("Limit .......... ");
    printn(portin(P_TEMPLIMIT)); print(" C    ");
    setcursor(6, 14); print("Highest ........ ");
    t = portin(P_TEMPMAX);
    printn(t / 10); putch('.'); printn(t % 10); print(" C   ");
    setcursor(6, 16); print("Seconds running  ");
    printn((ticks() - start) / 100); print("    ");
}

void heat(int mit_anzeige) {
    int start;
    start = ticks();
    if (mit_anzeige) {
        cls();
        setcursor(6, 2);
        printc("TB-32 BURN-IN", BRIGHT);
        setcursor(6, 4);
        print("Der Prozessor rechnet ohne Pause. Ab der Drosselgrenze");
        setcursor(6, 5);
        print("nimmt der Chipsatz den Takt zurueck -- man sieht es unten.");
        setcursor(6, 18);
        printc("ESC beendet.", CYAN);
    }
    while (1) {
        malochen(60000);
        if (mit_anzeige) {
            zeige_temperatur(start);
            if (haskey()) { getkey(); break; }
        }
    }
    if (mit_anzeige) { cls(); }
}

/* ==========================================================================
   HARMLOS 2 und 3: Farben
   ========================================================================== */

void colors(int mit_anzeige) {
    int i; int n;
    if (mit_anzeige) {
        cls();
        setcursor(6, 2); printc("COLOUR CHAOS", BRIGHT);
        setcursor(6, 4); print("Die Bilddaten bleiben unveraendert -- nur die Farbtabelle");
        setcursor(6, 5); print("wird verdreht. ESC beendet und stellt sie wieder her.");
    }
    n = 0;
    while (1) {
        for (i = 0; i < 12; i++)
            pal_set(rnd(256), rnd(256), rnd(256), rnd(256));
        n++;
        sleep(6);
        if (mit_anzeige && haskey()) { getkey(); break; }
    }
    pal_restore();
    if (mit_anzeige) cls();
}

void flicker(int mit_anzeige) {
    if (mit_anzeige) {
        cls();
        setcursor(6, 2); printc("FLICKER", BRIGHT);
        setcursor(6, 4); print("ESC beendet.");
    }
    while (1) {
        pal_schwarz();
        sleep(4);
        pal_restore();
        sleep(10);
        if (mit_anzeige && haskey()) { getkey(); break; }
    }
    pal_restore();
    if (mit_anzeige) cls();
}

/* ==========================================================================
   ECHTE ABSTUERZE
   ========================================================================== */

void crash_div0() {
    int a; int b;
    a = 42;
    b = ticks() * 0;        /* ueber ticks(), damit der Compiler die Null
                               nicht schon beim Uebersetzen wegrechnet */
    printn(a / b);
}

/* puffer hat 4 Plaetze. Alles ab puffer[4] liegt schon nicht mehr im Array,
   sondern im uebrigen Rahmen: gesicherte Register, alter Framepointer und
   die Ruecksprungadresse. Genau die faelschen wir hier. */
void ueberlauf(int wert) {
    int puffer[4];
    int i;
    for (i = 0; i < 40; i++) puffer[i] = wert;
}

void crash_badop() { ueberlauf(0 - 1); }     /* Ruecksprung nach 0xFFFFFFFF */
void crash_null()  { ueberlauf(0); }         /* Ruecksprung nach 0x00000000 */

int fresser(int n) {
    int puffer[16];         /* damit jeder Aufruf ordentlich Stack frisst */
    puffer[0] = n;
    tiefe++;
    if ((tiefe & 1023) == 0) { setcursor(2, 21); print("Tiefe: "); printn(tiefe); }
    return fresser(n + 1) + puffer[0];
}

void crash_stack() { tiefe = 0; fresser(1); }

void crash_kernel() {
    char* p; int i;
    p = (char*)0x00011000;              /* mitten im laufenden Kernel */
    for (i = 0; i < 0x20000; i++) { *p = 0 - 1; p++; }
    /* Jetzt steht dort ueberall 0xFF, und 0xFF ist kein gueltiger Opcode.
       Das Programm selbst laeuft noch -- es liegt ja bei 0x200000. Erst der
       naechste Systemaufruf springt in den zerstoerten Kernel. */
    print("Dieser Text kommt nicht mehr an.");
}

/* ==========================================================================
   Menue
   ========================================================================== */

void zeile(int y, int n, char* text) {
    setcursor(6, y);
    putcolor('[', YELLOW);
    putcolor('0' + n, BRIGHT);
    putcolor(']', YELLOW);
    print("  ");
    print(text);
}

void menue() {
    cls();
    setcursor(6, 1);
    printc("TB-32 STRESS TEST & FAULT INJECTOR", BRIGHT);

    setcursor(6, 3);
    printc("Harmlos -- die Maschine ueberlebt es:", GREEN);
    zeile(4, 1, "Burn-in       rechnen, bis der Chipsatz drosselt");
    zeile(5, 2, "Colour chaos  Farbtabelle verdrehen");
    zeile(6, 3, "Flicker       Bild flackern lassen");
    zeile(7, 4, "Restore       Farben wieder normal");

    setcursor(6, 9);
    printc("Echte Abstuerze -- danach haelt die CPU an:", RED);
    zeile(10, 5, "Division durch Null      INT 0x00");
    zeile(11, 6, "Pufferueberlauf          Ruecksprung nach 0xFFFFFFFF");
    zeile(12, 7, "Ruecksprung nach 0       Datenmuell wird ausgefuehrt");
    zeile(13, 8, "Endlose Rekursion        Stack frisst die Vektoren");
    zeile(14, 9, "Kernel ueberschreiben    kein Speicherschutz");

    setcursor(6, 16);
    print("[ESC]  beenden");
    setcursor(6, 18);
    printc("Im Hintergrund ist es am schoensten -- der Schreibtisch", CYAN);
    setcursor(6, 19);
    printc("laeuft weiter:  START CRASH.TBX COLORS /B", CYAN);
    setcursor(6, 20);
    printc("Beenden dann mit TASKLIST und TASKKILL.", CYAN);
}

/* Argument lesen: HEAT, COLORS, FLICKER oder FIX */
int argument() {
    char* a;
    char wort[16];
    int i; int n;
    a = (char*)PROG_ARGS;
    i = 0;
    while (a[i] == ' ') i++;
    n = 0;
    while (a[i] && a[i] != ' ' && n < 14) { wort[n] = toupper(a[i]); n++; i++; }
    wort[n] = 0;
    if (n == 0) return 0;
    if (strcmp(wort, "HEAT") == 0) return 1;
    if (strcmp(wort, "COLORS") == 0) return 2;
    if (strcmp(wort, "COLOURS") == 0) return 2;
    if (strcmp(wort, "FLICKER") == 0) return 3;
    if (strcmp(wort, "FIX") == 0) return 4;
    return 0 - 1;
}

int main() {
    int k; int c; int a;

    a = argument();
    if (a == 1) { heat(0); return 0; }        /* laeuft, bis man es abschiesst */
    if (a == 2) { colors(0); return 0; }
    if (a == 3) { flicker(0); return 0; }
    if (a == 4) { pal_restore(); return 0; }
    if (a < 0) {
        print("Unknown option. Use HEAT, COLORS, FLICKER or FIX.\n");
        return 1;
    }

    menue();
    while (1) {
        k = getkey();
        c = keychar(k);
        if (keycode(k) == K_ESC) break;
        if (c < '1' || c > '9') continue;

        if (c == '1') { heat(1); menue(); continue; }
        if (c == '2') { colors(1); menue(); continue; }
        if (c == '3') { flicker(1); menue(); continue; }
        if (c == '4') { pal_restore(); menue(); continue; }

        setcursor(6, 22);
        printc("Ausloesen ...", RED);

        if (c == '5') crash_div0();
        if (c == '6') crash_badop();
        if (c == '7') crash_null();
        if (c == '8') crash_stack();
        if (c == '9') crash_kernel();

        /* Hierhin kommt man nur, wenn der Fehler NICHT ausgeloest hat. */
        setcursor(6, 23);
        printc("Kein Absturz -- die Maschine hat es abgefangen.", GREEN);
    }
    cls();
    return 0;
}
