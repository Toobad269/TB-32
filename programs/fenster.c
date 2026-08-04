/* ==========================================================================
   FENSTER  --  das erste Programm mit einem eigenen Fenster

   Bisher galt: entweder ein Programm hat den ganzen Bildschirm (Flappy,
   Calc), oder es ist als Fenster in den Kernel einkompiliert (Word, Paint,
   der Coder). Dieses Programm ist beides nicht: es liegt als .TBX auf der
   Platte, laeuft als eigener Prozess -- und hat trotzdem ein Fenster, das
   man verschieben kann, waehrend daneben alles weiterlaeuft.

   Moeglich macht das der Fenster-Server im Schreibtisch. Das Programm malt
   nicht auf den Bildschirm, sondern in seinen eigenen Puffer; der
   Schreibtisch setzt die Fenster daraus zusammen. Deshalb kann es nicht
   ueber fremde Fenster malen, und wenn es verdeckt ist, malt es trotzdem
   weiter -- man sieht es nur nicht.

   Starten: aus dem Dateimanager oder mit  START FENSTER.TBX /B
   Uebersetzen auf dem Geraet selbst:      CC FENSTER.C
   ========================================================================== */

#include "proglib.c"
#include "gfxlib.c"

int zaehler = 0;
int letzte_x = 0 - 1;
int letzte_y = 0 - 1;
int letzte_taste = 0;

void malen() {
    int i;
    fenster_malziel();

    gx_fill(0, 0, fn_breite, fn_hoehe, 7);            /* Hintergrund */
    gx_text(8, 8, "Ein Fenster aus einem eigenen Programm", 0);
    gx_text(8, 22, "Dieses Programm liegt als .TBX auf der Platte.", 8);

    gx_frame(8, 40, fn_breite - 16, 46, 8);
    gx_text(16, 48, "Bilder gemalt:", 0);
    gx_zahl(140, 48, zaehler, 9);
    gx_text(16, 62, "Fenstergroesse:", 0);
    gx_zahl(140, 62, fn_breite, 0);
    gx_text(172, 62, "x", 0);
    gx_zahl(188, 62, fn_hoehe, 0);

    gx_text(8, 96, "Klick ins Fenster:", 8);
    if (letzte_x >= 0) {
        gx_zahl(160, 96, letzte_x, 9);
        gx_text(190, 96, "/", 8);
        gx_zahl(200, 96, letzte_y, 9);
    } else {
        gx_text(160, 96, "noch keiner", 8);
    }

    gx_text(8, 110, "Letzte Taste:", 8);
    if (letzte_taste > 0) gx_zahl(160, 110, letzte_taste, 9);
    else gx_text(160, 110, "keine", 8);

    /* Ein bisschen Bewegung, damit man sieht, dass es wirklich laeuft. */
    for (i = 0; i < 8; i++)
        gx_fill(8 + i * 14, fn_hoehe - 24, 10, 10,
                ((zaehler / 10 + i) % 8) + 1);

    gx_text(8, fn_hoehe - 40, "ESC oder das Kreuz schliesst.", 8);
    fenster_fertig();
}

int main() {
    int e[4];
    int art;
    int laufen;

    /* Ausdruecklich kein gx_start(): das gehoert Vollbildprogrammen. Ein
       Fenster bekommt seinen Platz vom Schreibtisch. */
    if (fenster_neu("Beispielfenster", 340, 200) < 0) {
        print("Dafuer braucht es den Schreibtisch -- erst WIN eingeben.\n");
        return 1;
    }

    laufen = 1;
    while (laufen) {
        art = fenster_ereignis(e);
        if (art == FE_SCHLIESS) {
            laufen = 0;
        } else if (art == FE_KLICK) {
            letzte_x = e[1];
            letzte_y = e[2];
        } else if (art == FE_TASTE) {
            letzte_taste = e[2];
            if (e[2] == K_ESC) laufen = 0;
        }
        zaehler = zaehler + 1;
        malen();
        sleep(5);                    /* dem Rest des Systems Luft lassen */
    }

    fenster_zu();
    return 0;
}
