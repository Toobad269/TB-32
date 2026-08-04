/* ==========================================================================
   SYSTEM MONITOR  --  was der Rechner gerade tut, als eigenes Programm

   Prozesse, Plattenbelegung, Temperatur und Luefter. Er liest nur; aendern
   kann er nichts. Frueher stand er im Kernel und griff direkt auf p_state
   und Freunde zu -- jetzt fragt er ueber Systemaufrufe.

   Uebersetzen auf dem Geraet selbst:  CC MONITOR.C
   ========================================================================== */

#include "proglib.c"
#include "gfxlib.c"

#define C_BLACK    0
#define C_WHITE   15
#define C_TEXT     0
#define C_WINDARK  8
#define C_ACCENT   9
#define C_WARN     4
#define C_GOOD     2
#define C_WINBG    7

#define P_TEMP      0xA0
#define P_FAN       0xA1
#define P_THROTTLE  0xA2

#define PS_FREI     0
#define PS_BEREIT   1
#define PS_LAEUFT   2
#define PS_SCHLAEFT 3

#define FS_DATA   576

void gx_num(int x, int y, int n, int farbe, int bg) { gx_zahl(x, y, n, farbe); }

void app_monitor(int w) {
    int x; int y; int i; int zeile; int breite; int belegt; int gesamt;
    int temp; int farbe;
    x = 6;
    y = 6;
    gx_fill(0, 0, fn_breite, fn_hoehe, C_WINBG);

    gx_text(x, y, "Process", C_ACCENT, 256);
    gx_text(x + 120, y, "PID", C_ACCENT, 256);
    gx_text(x + 160, y, "Status", C_ACCENT, 256);
    gx_text(x + 232, y, "CPU ms", C_ACCENT, 256);
    gx_fill(x, y + 10, fn_breite - 12, 1, C_WINDARK);

    zeile = 0;
    for (i = 0; i < MAXPROC; i++) {
        if (proz_zustand(i) == PS_FREI) continue;
        gx_text(x, y + 16 + zeile * 10, proz_name(i), C_TEXT, 256);
        gx_num(x + 120, y + 16 + zeile * 10, i, C_TEXT, 256);
        if (proz_zustand(i) == PS_LAEUFT)
            gx_text(x + 160, y + 16 + zeile * 10, "Running", C_GOOD, 256);
        if (proz_zustand(i) == PS_BEREIT)
            gx_text(x + 160, y + 16 + zeile * 10, "Ready", C_TEXT, 256);
        if (proz_zustand(i) == PS_SCHLAEFT)
            gx_text(x + 160, y + 16 + zeile * 10, "Sleeping", C_WINDARK, 256);
        gx_num(x + 232, y + 16 + zeile * 10, proz_ticks(i) * 10, C_TEXT, 256);
        zeile++;
    }
    if (zeile == 0) gx_text(x, y + 16, "Multitasking is disabled.", C_WINDARK, 256);

    y = y + 16 + zeile * 10 + 8;
    gx_text(x, y, "Disk usage", C_ACCENT, 256);
    gesamt = (platte_groesse() - FS_DATA) / 64;
    belegt = platte_belegt() / 64;
    breite = fn_breite - 24;
    gx_fill(x, y + 12, breite, 12, C_WHITE);
    gx_frame(x, y + 12, breite, 12, C_BLACK);
    if (gesamt > 0)
        gx_fill(x + 1, y + 13, (belegt * (breite - 2)) / gesamt, 10, C_ACCENT);
    gx_num(x, y + 30, platte_belegt() / 2, C_TEXT, 256);
    gx_text(x + 40, y + 30, "KB used of", C_TEXT, 256);
    gx_num(x + 128, y + 30, (platte_groesse() - FS_DATA) / 2, C_TEXT, 256);
    gx_text(x + 190, y + 30, "KB", C_TEXT, 256);

    gx_text(x, y + 44, "Context switches:", C_TEXT, 256);
    gx_num(x + 144, y + 44, proz_wechsel(), C_ACCENT, 256);
    gx_text(x, y + 54, "System up time:", C_TEXT, 256);
    gx_num(x + 144, y + 54, ticks() / 100, C_ACCENT, 256);
    gx_text(x + 190, y + 54, "s", C_TEXT, 256);

    /* Temperatur, Lüfter und Drosselung */
    y = y + 70;
    temp = portin(P_TEMP);
    gx_text(x, y, "Temperature", C_ACCENT, 256);
    gx_num(x + 100, y, temp / 10, C_TEXT, 256);
    gx_text(x + 128, y, "C", C_TEXT, 256);
    farbe = C_GOOD;
    if (temp > 700) farbe = 6;
    if (temp > 850) farbe = C_WARN;
    gx_fill(x + 150, y, breite - 150, 8, C_WHITE);
    gx_frame(x + 150, y, breite - 150, 8, C_BLACK);
    gx_fill(x + 151, y + 1, (temp / 10) * (breite - 152) / 110, 6, farbe);

    gx_text(x, y + 12, "Fan", C_ACCENT, 256);
    gx_num(x + 100, y + 12, portin(P_FAN), C_TEXT, 256);
    gx_text(x + 128, y + 12, "%", C_TEXT, 256);
    gx_fill(x + 150, y + 12, breite - 150, 8, C_WHITE);
    gx_frame(x + 150, y + 12, breite - 150, 8, C_BLACK);
    gx_fill(x + 151, y + 13, portin(P_FAN) * (breite - 152) / 100, 6, C_ACCENT);

    if (portin(P_THROTTLE)) {
        gx_text(x, y + 24, "THROTTLING", C_WARN, 256);
        gx_num(x + 100, y + 24, portin(P_THROTTLE), C_WARN, 256);
        gx_text(x + 128, y + 24, "% - CPU slowed down to cool", C_WARN, 256);
    }
}

int main() {
    int e[4];
    int art; int laufen;

    if (fenster_neu("System Monitor", 340, 250) < 0) {
        print("Der Monitor braucht den Schreibtisch -- erst WIN eingeben.\n");
        return 1;
    }
    laufen = 1;
    while (laufen) {
        art = fenster_ereignis(e);
        if (art == FE_SCHLIESS) laufen = 0;
        else if (art == FE_TASTE && e[2] == K_ESC) laufen = 0;
        /* Einmal je Sekunde auffrischen -- die Werte aendern sich staendig,
           und oefter waere nur Rechenzeit fuer nichts. */
        fenster_malziel();
        app_monitor(0);
        fenster_fertig();
        sleep(100);
    }
    fenster_zu();
    return 0;
}
