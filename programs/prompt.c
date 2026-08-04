/* ==========================================================================
   COMMAND PROMPT  --  das Fenster zur Schale, als eigenes Programm

   Aufgeteilt, und zwar an der richtigen Naht: die **Schale** -- also der
   Befehlsinterpreter mit DIR, COPY, CD und allem -- bleibt im Kernel. Sie
   ist Teil des Betriebssystems. Das **Fenster** ist ein Programm: es malt
   den Bildspeicher der Schale und reicht Tasten hinein.

   Genau so ist es bei den Grossen auch: die Shell und das Terminalfenster
   sind zwei verschiedene Dinge.

   Uebersetzen auf dem Geraet selbst:  CC PROMPT.C
   ========================================================================== */

#include "proglib.c"
#include "gfxlib.c"

#define C_BLACK    0
#define C_WHITE   15
#define C_TEXT     0
#define C_WINDARK  8
#define C_ACCENT   9
#define C_TITLEBAR 1

#define TERM_W  70
#define TERM_H  22

int term_view = 0;

int laeuft = 1;

void app_term(int w) {
    int x; int y; int zx; int zy; int c; int a; int adr; int mx; int my;
    int zeilenadr;

    x = 4;
    y = 4;

    gx_fill(0, 0, fn_breite, fn_hoehe, C_BLACK);

    /* Der Bildspeicher der Shell ist 70x22 gross und aendert sich nicht.
       Ist das Fenster kleiner, malen wir nur so viel, wie hineinpasst --
       sonst schriebe der Text ueber den Fensterrand hinaus. */
    mx = (fn_breite - 8) / 8;
    my = (fn_hoehe - 8) / 9;
    if (mx > TERM_W) mx = TERM_W;
    if (my > TERM_H) my = TERM_H;

    for (zy = 0; zy < my; zy++) {
        zeilenadr = schale_zeile(zy, term_view);
        if (zeilenadr < 0) continue;
        for (zx = 0; zx < mx; zx++) {
            adr = zeilenadr + zx * 2;
            c = byte_get(adr);
            if (c == 32 || c == 0) continue;
            a = byte_get(adr + 1) & 15;
            if (a == 7) a = C_WHITE;
            else if (a == 8) a = C_WINDARK;
            /* Blockzeichen kennt der 8x8-Zeichensatz nicht -- die malen wir
               als Rechtecke, damit Fortschrittsbalken sichtbar werden. */
            if (c == 219) { gx_fill(x + zx * 8, y + zy * 9, 8, 8, a); continue; }
            if (c == 176) { gx_fill(x + zx * 8 + 2, y + zy * 9 + 2, 4, 4, a); continue; }
            gx_char(x + zx * 8, y + zy * 9, c, a, 256);
        }
    }

    /* Schreibmarke -- nur wenn man live zusieht */
    if (term_view) {
        gx_fill(x, y + (my - 1) * 9, mx * 8, 9, C_TITLEBAR);
        gx_text(x + 4, y + (my - 1) * 9, "-- scrolled back, press a key --",
               C_WHITE, 256);
    } else if (laeuft) {
        if (schale_x() < mx && schale_y() < my)
            gx_fill(x + schale_x() * 8, y + schale_y() * 9 + 7, 7, 2, 10);
    } else {
        gx_text(x + 8, y + 8, "The command prompt has closed.", C_WINDARK, 256);
    }
}

int main() {
    int e[4];
    int art;

    if (fenster_neu("Command Prompt", 580, 230) < 0) {
        print("Braucht den Schreibtisch -- erst WIN eingeben.\n");
        return 1;
    }
    laeuft = schale_start();
    fenster_malziel();
    app_term(0);
    fenster_fertig();

    while (1) {
        art = fenster_ereignis(e);
        if (art == FE_SCHLIESS) break;
        if (art == FE_TASTE) {
            /* Die Schale erwartet dasselbe Format wie von der Tastatur:
               unten das Zeichen, oben der Tastencode. */
            term_view = 0;
            schale_taste((e[2] << 8) | e[1]);
        }
        /* Hat die Schale etwas geschrieben? Dann neu malen. */
        if (schale_neu() || art != FE_NICHTS) {
            fenster_malziel();
            app_term(0);
            fenster_fertig();
        }
        if (art == FE_NICHTS) sleep(2);
    }

    schale_ende();
    fenster_zu();
    return 0;
}
