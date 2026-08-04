/* ==========================================================================
   FLAPPY  --  fuer den TB-32

   Leertaste oder Mausklick laesst den Vogel steigen, sonst faellt er.
   Durch die Luecken fliegen, ESC beendet.

   Der TB-32 kann kein Fliesskomma. Physik in ganzen Zahlen sieht aber
   ruckelig aus, weil die kleinste Bewegung ein ganzer Bildpunkt waere.
   Deshalb rechnet dieses Programm in SECHZEHNTELN eines Punktes: die
   Hoehe steht als y16, gezeichnet wird bei y16/16. So kann der Vogel um
   ein Sechzehntel steigen, und die Bewegung wird weich.

   Genau dieser Trick steckt auch im Taschenrechner (dort Tausendstel) --
   Festkomma ist die uebliche Antwort, wenn Hardware kein Komma kennt.

   Uebersetzen auf dem Geraet:   CD SOURCE   und   CC FLAPPY.C FLAPPY.TBX
   ========================================================================== */

#include "proglib.c"
#include "gfxlib.c"

/* --- Spielfeld ----------------------------------------------------------- */

#define BODEN      360               /* Oberkante des Bodens */
#define VOGEL_X    140
#define VOGEL_B     24               /* Breite und Hoehe des Vogels */
#define VOGEL_H     18

#define ROHRE       3                /* so viele Rohre sind gleichzeitig da */
#define ROHR_B      52
#define LUECKE     120               /* Hoehe der Luecke */
#define ABSTAND    220               /* waagerechter Abstand der Rohre */
#define TEMPO        3               /* Punkte, die die Rohre je Bild wandern */

#define SCHWERKRAFT  3               /* Sechzehntel je Bild */
#define FLATTERN    46               /* Aufwaertsstoss in Sechzehnteln */

/* --- Farben (Wuerfel: 16 + rot*36 + gruen*6 + blau, je 0..5) -------------- */

#define F_HIMMEL   117               /* helles Blau   (2,4,5) */
#define F_ROHR      34               /* Gruen         (0,3,0) */
#define F_ROHRHELL  77               /* Glanzkante    (1,4,1) */
#define F_BODEN    179               /* Sand          (4,3,1) */
#define F_ERDE      94               /* Erde darunter (2,1,0) */
#define F_VOGEL     14               /* Gelb */
#define F_SCHNABEL  12               /* Hellrot */
#define F_SCHRIFT   15

/* --- Zustand ------------------------------------------------------------- */

int y16;                             /* Hoehe des Vogels, in Sechzehnteln */
int v16;                             /* Fallgeschwindigkeit, Sechzehntel/Bild */
int rohr_x[ROHRE];
int rohr_y[ROHRE];                   /* Oberkante der Luecke */
int punkte;
int bestwert = 0;
int lebt;
int alt_y;                           /* wo der Vogel im letzten Bild war */
int bilder;                          /* Bildzaehler fuer die Anzeige unten */
int bilder_zeit;
int bildrate;
char text[32];

/* ==========================================================================
   Spielstand
   ========================================================================== */

void neues_spiel() {
    int i;
    y16 = 160 * 16;
    v16 = 0;
    punkte = 0;
    lebt = 1;
    alt_y = 160;
    bilder = 0;
    bilder_zeit = ticks();
    for (i = 0; i < ROHRE; i++) {
        rohr_x[i] = GX_BREIT + i * ABSTAND;
        rohr_y[i] = 60 + rnd(BODEN - LUECKE - 100);
    }
}

/* Stossen sich Vogel und Rohr? Der Vogel ist ein Rechteck, das Rohr auch --
   also reicht der uebliche Rechteckvergleich. */
int stoss(int i) {
    int vy;
    vy = y16 / 16;
    if (VOGEL_X + VOGEL_B < rohr_x[i]) return 0;
    if (VOGEL_X > rohr_x[i] + ROHR_B) return 0;
    if (vy > rohr_y[i] && vy + VOGEL_H < rohr_y[i] + LUECKE) return 0;
    return 1;
}

void schritt() {
    int i; int vy;

    v16 = v16 + SCHWERKRAFT;
    y16 = y16 + v16;

    vy = y16 / 16;
    if (vy < 0) { y16 = 0; v16 = 0; }
    if (vy + VOGEL_H >= BODEN) lebt = 0;      /* aufgeschlagen */

    for (i = 0; i < ROHRE; i++) {
        rohr_x[i] = rohr_x[i] - TEMPO;
        /* Punkt, sobald der Vogel vorbei ist -- genau in dem Bild, in dem
           die rechte Rohrkante den Vogel passiert. */
        if (rohr_x[i] + ROHR_B < VOGEL_X && rohr_x[i] + ROHR_B >= VOGEL_X - TEMPO) {
            punkte++;
            beep(1200, 2);
        }
        if (rohr_x[i] + ROHR_B < 0) {         /* raus: hinten neu einsetzen */
            rohr_x[i] = rohr_x[i] + ROHRE * ABSTAND;
            rohr_y[i] = 60 + rnd(BODEN - LUECKE - 100);
        }
        if (stoss(i)) lebt = 0;
    }
    if (lebt == 0) {
        beep(200, 8);
        if (punkte > bestwert) bestwert = punkte;
    }
}

/* ==========================================================================
   Zeichnen
   ========================================================================== */

void rohr_malen(int i) {
    int x; int oben; int unten;
    x = rohr_x[i];
    oben = rohr_y[i];
    unten = oben + LUECKE;

    /* Sparsam malen: jeder Malbefehl kostet einen Systemaufruf, und der
       schlaegt bei einem Spiel mit 30 Bildern je Sekunde voll durch.
       Deshalb je Rohr nur drei Flaechen statt acht -- die Glanzkante steckt
       in der Muffe, der schwarze Rahmen faellt weg. */
    gx_fill(x, 0, ROHR_B, oben, F_ROHR);                    /* oberes Rohr */
    gx_fill(x - 4, oben - 16, ROHR_B + 8, 16, F_ROHRHELL);  /* Muffe */

    gx_fill(x, unten, ROHR_B, BODEN - unten, F_ROHR);       /* unteres Rohr */
    gx_fill(x - 4, unten, ROHR_B + 8, 16, F_ROHRHELL);
}

void vogel_malen() {
    int y;
    y = y16 / 16;
    gx_fill(VOGEL_X, y, VOGEL_B, VOGEL_H, F_VOGEL);
    gx_fill(VOGEL_X + 15, y + 4, 5, 5, GX_WEISS);     /* Auge */
    gx_fill(VOGEL_X + 17, y + 5, 3, 3, GX_SCHWARZ);
    gx_fill(VOGEL_X + VOGEL_B, y + 8, 7, 5, F_SCHNABEL);
    /* Fluegel: oben, wenn er steigt -- unten, wenn er faellt */
    if (v16 < 0) gx_fill(VOGEL_X + 3, y - 3, 12, 6, F_SCHNABEL);
    else         gx_fill(VOGEL_X + 3, y + VOGEL_H - 3, 12, 6, F_SCHNABEL);
}

/* Frueher wurde der Bildschirm nur einmal gemalt und danach je Bild nur das
   Bewegte weggewischt -- ein Vollbild flackerte, weil der Bildschirm beim
   Malen schon mitgelesen wurde. Mit der zweiten Bildseite (gx_doppelpuffer)
   faellt dieser ganze Eiertanz weg: wir malen jedes Bild komplett neu auf
   die unsichtbare Seite und zeigen es erst, wenn es fertig ist. Deshalb
   darf die Punkteanzeige jetzt auch einfach obendrauf liegen, ohne sich ein
   Kaestchen freizuraeumen. */
void hintergrund_malen() {
    gx_fill(0, 0, GX_BREIT, BODEN, F_HIMMEL);
    gx_fill(0, BODEN, GX_BREIT, 6, F_BODEN);
    gx_fill(0, BODEN + 6, GX_BREIT, GX_HOCH - BODEN - 6, F_ERDE);
}

/* Kein Freiraeumen mehr -- die Ziffern legen sich einfach ueber das Bild.
   Ein Schatten dahinter haelt sie auch vor einem gruenen Rohr lesbar. */
void punkte_malen() {
    itoa(punkte, text);
    gx_text_gross(23, 19, text, GX_SCHWARZ, 3);
    gx_text_gross(20, 16, text, F_SCHRIFT, 3);
}

void bild_malen() {
    int i; int y;

    /* Von hinten nach vorn, wie ein Maler: Himmel, Rohre, Vogel, Anzeige.
       Keine Wischtricks, keine Reihenfolge zum Abzaehlen. */
    hintergrund_malen();
    for (i = 0; i < ROHRE; i++) rohr_malen(i);

    vogel_malen();
    y = y16 / 16;
    alt_y = y;

    punkte_malen();

    /* Bildrate: einmal je Sekunde nachrechnen. Der Tickzaehler laeuft mit
       100 Hz, also ist die Differenz direkt in Hundertstelsekunden. */
    bilder++;
    if (ticks() - bilder_zeit >= 100) {
        bildrate = bilder;
        bilder = 0;
        bilder_zeit = ticks();
    }
    itoa(bildrate, text);
    gx_text(6, GX_HOCH - 13, text, F_SCHRIFT);
    gx_text(6 + gx_breite(text) + 8, GX_HOCH - 13, "fps", F_SCHRIFT);

    if (lebt == 0) {
        gx_panel(180, 150, 280, 90, 0);
        gx_text_mitte(180, 168, 280, "GAME OVER", GX_SCHWARZ);
        gx_text_mitte(180, 190, 280, "SPACE plays again", GX_SCHWARZ);
        gx_text_mitte(180, 206, 280, "ESC leaves", GX_SCHWARZ);
    }
}

/* ==========================================================================
   Hauptschleife
   ========================================================================== */

int main() {
    int k; int c; int code; int druck; int alt;

    gx_start();
    gx_doppelpuffer(1);              /* zweite Bildseite: kein Flackern mehr */
    portout(P_MCUR_ON, 0);           /* kein Mauszeiger im Spiel */
    neues_spiel();
    alt = 0;

    while (1) {
        druck = 0;

        /* --- Eingabe: Leertaste oder Maustaste --- */
        if (haskey()) {
            k = getkey();
            c = keychar(k);
            code = keycode(k);
            if (code == K_ESC) break;
            if (c == 32) druck = 1;
        }
        gx_maus_lesen();
        if (gx_klick()) druck = 1;

        if (druck) {
            if (lebt) {
                v16 = 0 - FLATTERN;
            } else {
                neues_spiel();
            }
        }

        if (lebt) schritt();
        bild_malen();
        gx_zeigen();                     /* fertiges Bild sichtbar machen */
        gx_takt(2);                      /* 50 Bilder je Sekunde, gleichmaessig */
    }

    gx_ende();
    return 0;
}
