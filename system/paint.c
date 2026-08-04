/* ==========================================================================
   PAINT -- Zeichenprogramm als Fenster im Schreibtisch

   Die Leinwand liegt NICHT im Bildspeicher, sondern in einem eigenen Bereich
   im RAM. Das hat zwei Gruende:

     * Ein Fenster kann verschoben, ueberdeckt oder zugeklappt werden. Waere
       das Bild direkt auf dem Schirm, waere es danach kaputt.
     * Rueckgaengig braucht eine zweite Kopie -- und die kann nur im RAM
       liegen.

   Auf den Schirm kommt sie mit einem einzigen Blitterbefehl (Kommando 4,
   "Bild aus dem RAM"). Der braucht dafuer 0,06 ms, also praktisch nichts.
   Kopiert wird mit dem Blockkopierer (DMA): 125 KB in 0,03 ms, waehrend der
   Prozessor sie Byte fuer Byte eine Zehntelsekunde lang umschaufeln wuerde.
   ========================================================================== */

#define PAINT_W      480             /* Leinwand in Bildpunkten */
#define PAINT_H      260

#define PAINT_KOPF   0x00600000      /* 8 Byte Kopf, dann die Bildpunkte */
#define PAINT_BUF    0x00600008      /* die Leinwand selbst */
#define PAINT_UNDO   0x00640000      /* Kopie fuer Rueckgaengig */
#define PAINT_KELLER 0x00680000      /* Arbeitsspeicher des Fuellwerkzeugs */
#define PAINT_KELLER_MAX 60000

#define P_DMA_SRC    0x56
#define P_DMA_DST    0x57
#define P_DMA_LEN    0x58
#define P_DMA_VAL    0x59
#define P_DMA_CMD    0x5A

/* Werkzeuge */
#define W_STIFT      0
#define W_RADIER     1
#define W_LINIE      2
#define W_RECHTECK   3
#define W_GEFUELLT   4
#define W_KREIS      5
#define W_FUELLEN    6
#define W_PIPETTE    7
#define W_ANZ        8

/* Aufteilung im Fenster, alles relativ zur linken oberen Ecke des Inhalts */
#define PT_LEISTE_B  52              /* Werkzeugleiste links */
#define PT_KNOPF     24
#define PT_LEIN_X    (PT_LEISTE_B + 4)
#define PT_LEIN_Y    4
#define PT_PAL_Y     (PT_LEIN_Y + PAINT_H + 6)
#define PT_PAL_K     14              /* Kantenlaenge eines Farbfeldes */

int pt_werkzeug = W_STIFT;
int pt_farbe = C_BLACK;
int pt_staerke = 1;
int pt_zieht = 0;                    /* 1 = Maustaste haelt gerade */
int pt_x0 = 0;                       /* Startpunkt der laufenden Figur */
int pt_y0 = 0;
int pt_x1 = 0;                       /* aktueller Punkt */
int pt_y1 = 0;
int pt_undo_da = 0;
int pt_win = 0 - 1;                  /* Fenster, in dem gerade gezogen wird */
int pt_namemode = 0;                 /* Dateiname wird gerade getippt */
char pt_name[24];
int pt_meldung = 0;                  /* 0 nichts, 1 gespeichert, 2 geladen, 3 Fehler */

/* --- Blockkopierer ------------------------------------------------------- */

void pt_kopieren(int ziel, int quelle, int anzahl) {
    sys_out(P_DMA_SRC, quelle);
    sys_out(P_DMA_DST, ziel);
    sys_out(P_DMA_LEN, anzahl);
    sys_out(P_DMA_CMD, 1);
}

/* Suchbefehle des Blockkopierers.
     3 = wie viele Bytes ab adr sind gleich wert
     4 = an welcher Stelle ab adr steht das erste gleiche (oder -1)
     5 = wie viele Bytes VOR adr (einschliesslich) sind gleich wert */
int pt_suchen(int adr, int wert, int max, int cmd) {
    sys_out(P_DMA_SRC, adr);
    sys_out(P_DMA_VAL, wert);
    sys_out(P_DMA_LEN, max);
    sys_out(P_DMA_CMD, cmd);
    return sys_in(P_DMA_LEN);
}

void pt_fuellen_roh(int ziel, int wert, int anzahl) {
    sys_out(P_DMA_DST, ziel);
    sys_out(P_DMA_VAL, wert);
    sys_out(P_DMA_LEN, anzahl);
    sys_out(P_DMA_CMD, 2);
}

/* --- Leinwand ------------------------------------------------------------ */

int pt_lesen(int x, int y) {
    if (x < 0 || y < 0 || x >= PAINT_W || y >= PAINT_H) return 0 - 1;
    return byte_get(PAINT_BUF + y * PAINT_W + x);
}

void pt_setzen(int x, int y, int farbe) {
    if (x < 0 || y < 0 || x >= PAINT_W || y >= PAINT_H) return;
    byte_put(PAINT_BUF + y * PAINT_W + x, farbe);
}

/* Ein Punkt in der eingestellten Strichstaerke */
void pt_tupfen(int x, int y, int farbe) {
    int dx; int dy; int h;
    if (pt_staerke <= 1) { pt_setzen(x, y, farbe); return; }
    h = pt_staerke / 2;
    for (dy = 0 - h; dy < pt_staerke - h; dy++)
        for (dx = 0 - h; dx < pt_staerke - h; dx++)
            pt_setzen(x + dx, y + dy, farbe);
}

/* Bresenham -- Linien ohne Kommazahlen, so wie es sein muss */
void pt_linie(int x0, int y0, int x1, int y1, int farbe) {
    int dx; int dy; int sx; int sy; int fehler; int e2;
    dx = x1 - x0; if (dx < 0) dx = 0 - dx;
    dy = y1 - y0; if (dy < 0) dy = 0 - dy;
    sx = 1; if (x0 > x1) sx = 0 - 1;
    sy = 1; if (y0 > y1) sy = 0 - 1;
    fehler = dx - dy;
    while (1) {
        pt_tupfen(x0, y0, farbe);
        if (x0 == x1 && y0 == y1) return;
        e2 = fehler * 2;
        if (e2 > 0 - dy) { fehler = fehler - dy; x0 = x0 + sx; }
        if (e2 < dx)     { fehler = fehler + dx; y0 = y0 + sy; }
    }
}

void pt_rechteck(int x0, int y0, int x1, int y1, int farbe, int voll) {
    int x; int y; int t;
    if (x0 > x1) { t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { t = y0; y0 = y1; y1 = t; }
    if (voll) {
        for (y = y0; y <= y1; y++)
            for (x = x0; x <= x1; x++) pt_setzen(x, y, farbe);
        return;
    }
    pt_linie(x0, y0, x1, y0, farbe);
    pt_linie(x0, y1, x1, y1, farbe);
    pt_linie(x0, y0, x0, y1, farbe);
    pt_linie(x1, y0, x1, y1, farbe);
}

/* Kreis nach dem Mittelpunktverfahren -- auch das ganz ohne Wurzel */
void pt_kreis(int mx, int my, int r, int farbe) {
    int x; int y; int f;
    x = r; y = 0; f = 1 - r;
    while (x >= y) {
        pt_tupfen(mx + x, my + y, farbe);
        pt_tupfen(mx + y, my + x, farbe);
        pt_tupfen(mx - y, my + x, farbe);
        pt_tupfen(mx - x, my + y, farbe);
        pt_tupfen(mx - x, my - y, farbe);
        pt_tupfen(mx - y, my - x, farbe);
        pt_tupfen(mx + y, my - x, farbe);
        pt_tupfen(mx + x, my - y, farbe);
        y++;
        if (f < 0) { f = f + 2 * y + 1; }
        else { x--; f = f + 2 * (y - x) + 1; }
    }
}

/* Flaeche fuellen -- zeilenweise, mit einer Warteschlange im RAM.
   Rekursiv ginge nicht: der Stack eines Prozesses ist 8 KB gross, eine volle
   Leinwand haette 125.000 verschachtelte Aufrufe.

   Kopf und Ende der Warteschlange stehen bewusst in festen Speicherzellen und
   nicht in lokalen Variablen: die Schleife ruft zwischendurch Dutzende anderer
   Funktionen auf, und ein einziger verlorener Zaehler laesst das Fuellen nach
   drei Zeilen aufhoeren -- genau das ist beim ersten Versuch passiert. */
#define PT_Q_KOPF   (PAINT_KELLER)          /* Leseposition */
#define PT_Q_ENDE   (PAINT_KELLER + 4)      /* Schreibposition */
#define PT_Q_DATEN  (PAINT_KELLER + 8)
#define PT_Q_MAX    30000                   /* Eintraege zu je zwei Worten */

void pt_q_rein(int x, int y) {
    int e;
    e = mem_get(PT_Q_ENDE);
    if (e >= PT_Q_MAX) return;              /* voll -- Rest bleibt ungefuellt */
    mem_put(PT_Q_DATEN + e * 8, x);
    mem_put(PT_Q_DATEN + e * 8 + 4, y);
    mem_put(PT_Q_ENDE, e + 1);
}

/* Die Nachbarzeilen einer fertig gefuellten Zeile einreihen.

   Hier zaehlt jeder Befehl: bei einer Flaeche von 220 mal 120 Punkten wird
   diese Schleife 26.000-mal durchlaufen. Der erste Versuch rief dafuer
   pt_lesen auf -- eine Funktion mit vier Bereichspruefungen -- und brauchte
   fuer die Flaeche eine halbe Minute. Jetzt steht die Zeilenadresse fest
   und es wird direkt gelesen. */
void pt_nachbarn(int links, int rechts, int y, int alt) {
    int x; int d; int zn; int ny; int seite;
    for (seite = 0; seite < 2; seite++) {
        ny = y - 1;
        if (seite) ny = y + 1;
        if (ny < 0 || ny >= PAINT_H) continue;
        zn = PAINT_BUF + ny * PAINT_W;
        x = links;
        while (x <= rechts) {
            d = pt_suchen(zn + x, alt, rechts - x + 1, 4);   /* naechster Lauf */
            if (d < 0) break;
            x = x + d;
            pt_q_rein(x, ny);
            d = pt_suchen(zn + x, alt, rechts - x + 1, 3);   /* wie lang ist er */
            if (d <= 0) break;
            x = x + d;
        }
    }
}

void pt_flut(int sx, int sy, int neu) {
    int alt; int y; int links; int rechts; int k; int zeile; int d;
    alt = pt_lesen(sx, sy);
    if (alt < 0 || alt == neu) return;

    mem_put(PT_Q_KOPF, 0);
    mem_put(PT_Q_ENDE, 0);
    pt_q_rein(sx, sy);

    while (mem_get(PT_Q_KOPF) < mem_get(PT_Q_ENDE)) {
        k = mem_get(PT_Q_KOPF);
        links = mem_get(PT_Q_DATEN + k * 8);
        y = mem_get(PT_Q_DATEN + k * 8 + 4);
        mem_put(PT_Q_KOPF, k + 1);
        zeile = PAINT_BUF + y * PAINT_W;
        if (byte_get(zeile + links) != alt) continue;

        /* Die Grenzen des Laufs holt die Hardware in zwei Befehlen statt in
           zweihundert Leseschritten. */
        d = pt_suchen(zeile + links, alt, PAINT_W - links, 3);
        rechts = links + d - 1;
        d = pt_suchen(zeile + links, alt, links + 1, 5);
        links = links - d + 1;

        pt_fuellen_roh(zeile + links, neu, rechts - links + 1);
        pt_nachbarn(links, rechts, y, alt);
    }
}

void pt_undo_sichern() {
    pt_kopieren(PAINT_UNDO, PAINT_BUF, PAINT_W * PAINT_H);
    pt_undo_da = 1;
}

void pt_undo() {
    if (pt_undo_da == 0) return;
    pt_kopieren(PAINT_BUF, PAINT_UNDO, PAINT_W * PAINT_H);
}

/* Ein neues Bild fragt sofort, wohin es gehoert. Danach weiss es seinen
   Platz, und "Save" speichert einfach -- statt jedes Mal wieder zu fragen.
   Genau so macht es jedes Programm, das man kennt. */
int pt_ort = 0;                  /* 1 = Name und Ordner stehen fest */

void pt_neu_anlegen() {
    pt_undo_sichern();
    pt_fuellen_roh(PAINT_BUF, C_WHITE, PAINT_W * PAINT_H);
}

void pt_neu() {
    pt_ort = 0;
    dlg_oeffne(APP_PAINT, DLG_SPEICHERN, ".TBI",
               pt_name[0] ? pt_name : "PICTURE.TBI");
    dlg_neu = 1;
}

/* --- Datei --------------------------------------------------------------- */

/* Eigenes Format TBI: Breite und Hoehe als Wort, dann ein Byte je Punkt. */
void pt_speichern() {
    if (pt_name[0] == 0) { pt_meldung = 3; return; }
    mem_put(PAINT_KOPF, PAINT_W);
    mem_put(PAINT_KOPF + 4, PAINT_H);
    if (fs_write(pt_name, PAINT_KOPF, 8 + PAINT_W * PAINT_H) < 0) pt_meldung = 3;
    else pt_meldung = 1;
}

void pt_laden() {
    int n;
    if (pt_name[0] == 0) { pt_meldung = 3; return; }
    n = fs_read(pt_name, PAINT_KOPF, 8 + PAINT_W * PAINT_H);
    if (n < 8) { pt_meldung = 3; return; }
    if (mem_get(PAINT_KOPF) != PAINT_W || mem_get(PAINT_KOPF + 4) != PAINT_H) {
        pt_meldung = 3;                  /* andere Groesse -- passt nicht */
        return;
    }
    pt_undo_da = 0;
    pt_meldung = 2;
}

/* --- Malen des Fensters --------------------------------------------------- */

char* pt_wz_name(int i) {
    if (i == W_STIFT)    return "Pen";
    if (i == W_RADIER)   return "Era";
    if (i == W_LINIE)    return "Lin";
    if (i == W_RECHTECK) return "Box";
    if (i == W_GEFUELLT) return "Bx*";
    if (i == W_KREIS)    return "Cir";
    if (i == W_FUELLEN)  return "Fil";
    /* "Get" ist die Pipette: Farbe aus dem Bild aufnehmen und damit
       weitermalen. Hiess frueher "Pic" -- das las sich wie "Picture". */
    return "Get";
}

/* Die 32 Felder der Farbleiste: erst die 16 klassischen Farben, dann 16
   kraeftige aus dem Farbwuerfel (Nummer = 16 + rot*36 + gruen*6 + blau). */
int pt_pal_farbe(int i) {
    if (i < 16) return i;
    if (i == 16) return 16;                 /* tiefes Schwarz  */
    if (i == 17) return 16 + 5 * 36;        /* Rot             */
    if (i == 18) return 16 + 5 * 36 + 3 * 6;/* Orange          */
    if (i == 19) return 16 + 5 * 36 + 5 * 6;/* Gelb            */
    if (i == 20) return 16 + 3 * 36 + 5 * 6;/* Hellgruen       */
    if (i == 21) return 16 + 5 * 6;         /* Gruen           */
    if (i == 22) return 16 + 5 * 6 + 5;     /* Tuerkis         */
    if (i == 23) return 16 + 3 * 6 + 5;     /* Himmelblau      */
    if (i == 24) return 16 + 5;             /* Blau            */
    if (i == 25) return 16 + 3 * 36 + 5;    /* Violett         */
    if (i == 26) return 16 + 5 * 36 + 5;    /* Magenta         */
    if (i == 27) return 16 + 5 * 36 + 3 * 6 + 3 * 6; /* Rosa   */
    if (i == 28) return 16 + 2 * 36 + 1 * 6;/* Braun           */
    if (i == 29) return 16 + 3 * 36 + 3 * 6 + 3; /* Grau       */
    if (i == 30) return 16 + 4 * 36 + 4 * 6 + 4; /* Hellgrau   */
    return 16 + 5 * 36 + 5 * 6 + 5;         /* Weiss           */
}

void app_paint(int i) {
    int x; int y; int k; int j; int px; int py;
    x = win_x[i];
    y = win_y[i] + TITLE_H;

    /* Werkzeuge links */
    for (k = 0; k < W_ANZ; k++) {
        g_button(x + 2 + (k % 2) * (PT_KNOPF + 1),
                 y + 4 + (k / 2) * (PT_KNOPF + 1),
                 PT_KNOPF, 20, pt_wz_name(k), pt_werkzeug == k);
    }
    /* Strichstaerke */
    j = y + 4 + 4 * (PT_KNOPF + 1) + 6;
    g_text(x + 4, j, "Size", C_TEXT, 256);
    for (k = 0; k < 3; k++) {
        int s;
        g_button(x + 3 + k * 14, j + 12, 12, 14, "", 0);
    }
    g_text(x + 6, j + 15, "1", C_TEXT, 256);
    g_text(x + 20, j + 15, "2", C_TEXT, 256);
    g_text(x + 34, j + 15, "4", C_TEXT, 256);
    g_frame(x + 2 + (pt_staerke / 2) * 14, j + 11, 14, 16, C_ACCENT);

    /* Aktionen */
    j = j + 32;
    g_button(x + 2, j, 47, 14, "New", 0);
    g_button(x + 2, j + 16, 47, 14, "Undo", 0);
    g_button(x + 2, j + 32, 47, 14, "Save", 0);
    g_button(x + 2, j + 48, 47, 14, "Open", 0);

    /* Leinwand: ein einziger Blitterbefehl.
       Wichtig: Kommando 4 liest die Quelle aus demselben Register wie der
       Zeichensatz. Also vorher auf die Leinwand zeigen -- und hinterher
       zurueckstellen, sonst malt der ganze Schreibtisch seine Schrift aus
       unserem Bild. */
    px = x + PT_LEIN_X;
    py = y + PT_LEIN_Y;
    g_frame(px - 1, py - 1, PAINT_W + 2, PAINT_H + 2, C_WINDARK);
    sys_out(P_BLT_SRC, PAINT_BUF);
    sys_blit((px & 65535) | ((py & 65535) << 16),
             (PAINT_W & 65535) | ((PAINT_H & 65535) << 16), 0, 4);
    sys_out(P_BLT_SRC, (int)font8);

    /* Farbleiste unter der Leinwand */
    for (k = 0; k < 32; k++) {
        int fx; int fy;
        fx = px + (k % 16) * PT_PAL_K;
        fy = y + PT_PAL_Y + (k / 16) * PT_PAL_K;
        g_fill(fx, fy, PT_PAL_K - 1, PT_PAL_K - 1, pt_pal_farbe(k));
        if (pt_pal_farbe(k) == pt_farbe)
            g_frame(fx - 1, fy - 1, PT_PAL_K + 1, PT_PAL_K + 1, C_ACCENT);
    }

    /* Dateiname und Meldung */
    j = y + PT_PAL_Y + 2 * PT_PAL_K + 4;
    g_text(x + 4, j, "File:", C_TEXT, 256);
    g_fill(x + 44, j - 2, 150, 12, C_WHITE);
    g_text(x + 46, j, pt_name, C_TEXT, 256);
    if (pt_namemode) g_fill(x + 46 + strlen(pt_name) * 8, j, 7, 8, C_ACCENT);
    if (pt_meldung == 1) g_text(x + 200, j, "saved", C_GOOD, 256);
    if (pt_meldung == 2) g_text(x + 200, j, "loaded", C_GOOD, 256);
    if (pt_meldung == 3) g_text(x + 200, j, "no such file / name missing",
                                C_WARN, 256);
}

/* Nur die Leinwand neu auf den Schirm bringen -- ein einziger Blitterbefehl.
   Waehrend man mit der Maus zieht, wurde vorher das ganze Fenster neu gemalt:
   Werkzeugleiste, Knoepfe, Palette, Dateiname. Das flackerte sichtbar, weil
   der Schreibtisch direkt auf den angezeigten Bildspeicher malt. Jetzt geht
   nur noch das, was sich wirklich aendert. */
void pt_leinwand_malen(int i) {
    int px; int py;
    px = win_x[i] + PT_LEIN_X;
    py = win_y[i] + TITLE_H + PT_LEIN_Y;
    sys_out(P_BLT_SRC, PAINT_BUF);
    sys_blit((px & 65535) | ((py & 65535) << 16),
             (PAINT_W & 65535) | ((PAINT_H & 65535) << 16), 0, 4);
    sys_out(P_BLT_SRC, (int)font8);
}

/* Zeichnet die Figur, die man gerade aufzieht, direkt auf den Schirm --
   die Leinwand bleibt unberuehrt, bis man die Taste loslaesst. */
/* Rahmen, der die Leinwand nicht verlaesst. Ohne diese Begrenzung ragte
   die Vorschau eines grossen Kreises ueber den Fensterrand hinaus auf den
   Schreibtisch -- die Figur selbst war korrekt beschnitten, nur ihre
   Vorschau nicht. */
void pt_rahmen_begrenzt(int px, int py, int x0, int y0, int x1, int y1, int farbe) {
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > PAINT_W - 1) x1 = PAINT_W - 1;
    if (y1 > PAINT_H - 1) y1 = PAINT_H - 1;
    if (x1 < x0 || y1 < y0) return;
    g_fill(px + x0, py + y0, x1 - x0 + 1, 1, farbe);
    g_fill(px + x0, py + y1, x1 - x0 + 1, 1, farbe);
    g_fill(px + x0, py + y0, 1, y1 - y0 + 1, farbe);
    g_fill(px + x1, py + y0, 1, y1 - y0 + 1, farbe);
}

void pt_vorschau(int i) {
    int px; int py; int r; int dx; int dy;
    if (pt_zieht == 0) return;
    if (pt_werkzeug != W_LINIE && pt_werkzeug != W_RECHTECK
        && pt_werkzeug != W_GEFUELLT && pt_werkzeug != W_KREIS) return;
    px = win_x[i] + PT_LEIN_X;
    py = win_y[i] + TITLE_H + PT_LEIN_Y;
    if (pt_werkzeug == W_RECHTECK || pt_werkzeug == W_GEFUELLT) {
        int x0; int y0; int x1; int y1; int t;
        x0 = pt_x0; x1 = pt_x1; y0 = pt_y0; y1 = pt_y1;
        if (x0 > x1) { t = x0; x0 = x1; x1 = t; }
        if (y0 > y1) { t = y0; y0 = y1; y1 = t; }
        pt_rahmen_begrenzt(px, py, x0, y0, x1, y1, pt_farbe);
        return;
    }
    if (pt_werkzeug == W_KREIS) {
        dx = pt_x1 - pt_x0; if (dx < 0) dx = 0 - dx;
        dy = pt_y1 - pt_y0; if (dy < 0) dy = 0 - dy;
        r = dx; if (dy > r) r = dy;
        pt_rahmen_begrenzt(px, py, pt_x0 - r, pt_y0 - r, pt_x0 + r, pt_y0 + r,
                           pt_farbe);
        return;
    }
    /* Linie: als duenner Rahmen um die Strecke andeuten waere irrefuehrend,
       also malen wir sie wirklich -- direkt auf den Schirm. */
    {
        int ax; int ay; int bx; int by; int sx; int sy; int f; int e2;
        ax = pt_x0; ay = pt_y0; bx = pt_x1; by = pt_y1;
        dx = bx - ax; if (dx < 0) dx = 0 - dx;
        dy = by - ay; if (dy < 0) dy = 0 - dy;
        sx = 1; if (ax > bx) sx = 0 - 1;
        sy = 1; if (ay > by) sy = 0 - 1;
        f = dx - dy;
        while (1) {
            g_fill(px + ax, py + ay, 1, 1, pt_farbe);
            if (ax == bx && ay == by) return;
            e2 = f * 2;
            if (e2 > 0 - dy) { f = f - dy; ax = ax + sx; }
            if (e2 < dx)     { f = f + dx; ay = ay + sy; }
        }
    }
}

/* --- Bedienung ------------------------------------------------------------ */

/* Rueckgabe: 1 = neu zeichnen */
int pt_klick(int i, int mx, int my) {
    int x; int y; int k; int j; int px; int py;
    x = win_x[i];
    y = win_y[i] + TITLE_H;
    pt_meldung = 0;

    for (k = 0; k < W_ANZ; k++) {
        if (treffer(mx, my, x + 2 + (k % 2) * (PT_KNOPF + 1),
                    y + 4 + (k / 2) * (PT_KNOPF + 1), PT_KNOPF, 20)) {
            pt_werkzeug = k;
            pt_namemode = 0;
            return 1;
        }
    }

    j = y + 4 + 4 * (PT_KNOPF + 1) + 6;
    for (k = 0; k < 3; k++) {
        if (treffer(mx, my, x + 3 + k * 14, j + 12, 12, 14)) {
            pt_staerke = 1;
            if (k == 1) pt_staerke = 2;
            if (k == 2) pt_staerke = 4;
            return 1;
        }
    }

    j = j + 32;
    if (treffer(mx, my, x + 2, j, 47, 14))      { pt_neu();  return 1; }
    if (treffer(mx, my, x + 2, j + 16, 47, 14)) { pt_undo(); return 1; }
    if (treffer(mx, my, x + 2, j + 32, 47, 14)) {
        /* Steht der Platz schon fest, wird ohne Nachfrage gespeichert. */
        if (pt_ort && pt_name[0]) pt_speichern();
        else dlg_oeffne(APP_PAINT, DLG_SPEICHERN, ".TBI", pt_name);
        return 1;
    }
    if (treffer(mx, my, x + 2, j + 48, 47, 14)) {
        dlg_oeffne(APP_PAINT, DLG_OEFFNEN, ".TBI", pt_name);
        return 1;
    }

    px = x + PT_LEIN_X;
    py = y + PT_LEIN_Y;

    for (k = 0; k < 32; k++) {
        if (treffer(mx, my, px + (k % 16) * PT_PAL_K,
                    y + PT_PAL_Y + (k / 16) * PT_PAL_K,
                    PT_PAL_K - 1, PT_PAL_K - 1)) {
            pt_farbe = pt_pal_farbe(k);
            pt_namemode = 0;
            return 1;
        }
    }

    j = y + PT_PAL_Y + 2 * PT_PAL_K + 4;
    if (treffer(mx, my, x + 44, j - 2, 150, 12)) { pt_namemode = 1; return 1; }

    /* In der Leinwand: Zeichnen beginnt */
    if (treffer(mx, my, px, py, PAINT_W, PAINT_H)) {
        pt_namemode = 0;
        pt_x0 = mx - px;
        pt_y0 = my - py;
        pt_x1 = pt_x0;
        pt_y1 = pt_y0;
        pt_win = i;
        if (pt_werkzeug == W_PIPETTE) {
            k = pt_lesen(pt_x0, pt_y0);
            if (k >= 0) pt_farbe = k;
            return 1;
        }
        pt_undo_sichern();
        pt_zieht = 1;
        if (pt_werkzeug == W_STIFT)  pt_tupfen(pt_x0, pt_y0, pt_farbe);
        if (pt_werkzeug == W_RADIER) pt_tupfen(pt_x0, pt_y0, C_WHITE);
        if (pt_werkzeug == W_FUELLEN) {
            pt_flut(pt_x0, pt_y0, pt_farbe);
            pt_zieht = 0;
        }
        return 1;
    }
    return 0;
}

/* Wird jede Runde gerufen, solange die Maustaste haelt */
void pt_ziehen(int mx, int my) {
    int px; int py; int nx; int ny;
    if (pt_zieht == 0 || pt_win < 0) return;
    px = win_x[pt_win] + PT_LEIN_X;
    py = win_y[pt_win] + TITLE_H + PT_LEIN_Y;
    nx = mx - px;
    ny = my - py;
    if (nx < 0) nx = 0;
    if (ny < 0) ny = 0;
    if (nx >= PAINT_W) nx = PAINT_W - 1;
    if (ny >= PAINT_H) ny = PAINT_H - 1;

    if (pt_werkzeug == W_STIFT) {
        pt_linie(pt_x1, pt_y1, nx, ny, pt_farbe);
        pt_x1 = nx; pt_y1 = ny;
        return;
    }
    if (pt_werkzeug == W_RADIER) {
        pt_linie(pt_x1, pt_y1, nx, ny, C_WHITE);
        pt_x1 = nx; pt_y1 = ny;
        return;
    }
    pt_x1 = nx;                      /* Figuren: nur den Endpunkt merken */
    pt_y1 = ny;
}

/* Maustaste losgelassen -- jetzt wird die Figur wirklich in die Leinwand
   gemalt. Vorher war sie nur auf dem Schirm zu sehen. */
void pt_loslassen() {
    int dx; int dy; int r;
    if (pt_zieht == 0) return;
    pt_zieht = 0;
    if (pt_werkzeug == W_LINIE)
        pt_linie(pt_x0, pt_y0, pt_x1, pt_y1, pt_farbe);
    if (pt_werkzeug == W_RECHTECK)
        pt_rechteck(pt_x0, pt_y0, pt_x1, pt_y1, pt_farbe, 0);
    if (pt_werkzeug == W_GEFUELLT)
        pt_rechteck(pt_x0, pt_y0, pt_x1, pt_y1, pt_farbe, 1);
    if (pt_werkzeug == W_KREIS) {
        dx = pt_x1 - pt_x0; if (dx < 0) dx = 0 - dx;
        dy = pt_y1 - pt_y0; if (dy < 0) dy = 0 - dy;
        r = dx; if (dy > r) r = dy;
        pt_kreis(pt_x0, pt_y0, r, pt_farbe);
    }
}

/* Tasten -- nur fuer das Namensfeld und ein paar Abkuerzungen */
void pt_taste(int k) {
    int c; int code; int n;
    c = keychar(k);
    code = keycode(k);
    if (pt_namemode) {
        n = strlen(pt_name);
        if (code == K_ENTER) { pt_namemode = 0; return; }
        if (code == K_ESC)   { pt_namemode = 0; return; }
        if (c == 8) { if (n > 0) pt_name[n - 1] = 0; return; }
        if (c >= 32 && c < 127 && n < 20) {
            pt_name[n] = toupper(c);
            pt_name[n + 1] = 0;
        }
        return;
    }
    if (c == 'u' || c == 'U') { pt_undo(); return; }
    if (c >= '1' && c <= '8') { pt_werkzeug = c - '1'; return; }
}

void pt_init() {
    if (pt_name[0] == 0) strncpy(pt_name, "BILD.TBI", 20);
    pt_fuellen_roh(PAINT_BUF, C_WHITE, PAINT_W * PAINT_H);
    pt_undo_da = 0;
}
