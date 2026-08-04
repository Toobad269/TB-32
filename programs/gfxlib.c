/* ==========================================================================
   gfxlib.c  --  Grafik fuer eigene Programme auf dem TB-32

   Es gibt zwei Wege auf den Bildschirm, und man braucht beide:

     * Der Blitter (Ports 0x44-0x4C). Ein Kommando malt eine ganze Flaeche
       oder ein Zeichen. Fuer alles Grosse der richtige Weg.
     * Der Bildspeicher liegt ab 0x02100000 im Adressraum -- ein Byte je
       Bildpunkt, das Byte ist die Farbnummer. Wer einzelne Punkte setzt,
       schreibt einfach dorthin und spart sich den Systemaufruf.

   Einbinden: zuerst proglib.c holen, dann diese Datei -- beide mit der
   include-Anweisung. Sie hier als Beispiel hinzuschreiben geht nicht: der
   Compiler sucht das Wort auch in Kommentaren und wuerde sich selbst
   einbinden.

   Achtung: Der Compiler auf dem Geraet (CC) kann nur bis zu 5 Argumente je
   Funktion. Alles hier bleibt darunter, damit sich Programme mit gfxlib
   auch auf dem TB-32 selbst uebersetzen lassen.
   ========================================================================== */

#define GX_VRAM    0x02100000
#define GX_BREIT   640
#define GX_HOCH    400

#define P_BLT_X    0x44
#define P_BLT_Y    0x45
#define P_BLT_W    0x46
#define P_BLT_H    0x47
#define P_BLT_COL  0x48
#define P_BLT_CMD  0x49
#define P_BLT_CHR  0x4A
#define P_BLT_SRC  0x4B
#define P_BLT_BG   0x4C
#define P_BLT_ZOOM 0x54
#define P_GFX_DOPPEL 0x52
#define P_GFX_TAUSCH 0x53
#define P_BLT_ZIEL   0x5B          /* der Blitter malt in den Speicher ... */
#define P_BLT_ZIELB  0x5C          /* ... so breit ... */
#define P_BLT_ZIELH  0x5D          /* ... und so hoch */
#define P_MCUR_X   0x4D
#define P_MCUR_Y   0x4E
#define P_MCUR_ON  0x4F
#define P_MAUS_X   0x60
#define P_MAUS_Y   0x61
#define P_MAUS_BTN 0x62
#define P_MAUS_RAD 0x63

#define BLT_FILL   1
#define BLT_FRAME  2
#define BLT_CHAR   3

/* Farben: 0-15 sind die klassischen PC-Farben, ab 16 ein Wuerfel aus
   6x6x6 Mischungen -- Nummer = 16 + rot*36 + gruen*6 + blau (je 0..5). */
#define GX_SCHWARZ  0
#define GX_BLAU     1
#define GX_ROT      4
#define GX_GRAU     7
#define GX_DUNKEL   8
#define GX_WEISS   15

int gx_font = 0;
int gx_mx = 0;                   /* zuletzt gelesene Mausposition */
int gx_my = 0;
int gx_btn = 0;
int gx_btn_alt = 0;

/* --- Ein- und Ausschalten ------------------------------------------------ */

void gx_start() {
    gx_font = fontaddr();            /* wo der Zeichensatz des Systems liegt */
    setmode(1 + 256);                /* Bit 8 = Bildspeicher gleich loeschen */
    portout(P_BLT_SRC, gx_font);
    portout(P_MCUR_ON, 1);
}

void gx_ende() {
    gx_doppelpuffer(0);
    portout(P_MCUR_ON, 0);
    setmode(0 + 256);
    cls();
}

/* --- Doppelpufferung -----------------------------------------------------

   Ohne sie liest der Bildschirm mit, waehrend gemalt wird: man sieht halb
   gezeichnete Ziffern flackern, und man muss sich mit Tricks behelfen --
   nur das Bewegte loeschen, die Reihenfolge der Malbefehle abzaehlen. Das
   ist genau der Grund, warum eine Anzeige oben im Bild nur mit einem
   eigenen Kaestchen ging: sie musste ihren alten Stand ueberdecken, und
   dieses Kaestchen stanzte ein Loch in alles, was dahinter lag.

   Mit zwei Bildseiten faellt das alles weg. Man malt in Ruhe die ganze
   Szene auf die unsichtbare Seite und macht sie dann in einem Schlag
   sichtbar. So arbeitet jede Grafikkarte:

       gx_doppelpuffer(1);
       while (1) {
           ... alles malen ...
           gx_zeigen();
       }
   ------------------------------------------------------------------------ */

void gx_doppelpuffer(int an) { portout(P_GFX_DOPPEL, an); }
void gx_zeigen()             { portout(P_GFX_TAUSCH, 1); }

/* --- Bildtakt -------------------------------------------------------------

   Ohne Bremse laeuft eine Spielschleife so schnell, wie der Rechner kann --
   und dann haengt die Geschwindigkeit des Spiels davon ab, wie schnell er
   gerade ist. Mit gx_takt(2) kommt alle zwei Hundertstel ein Bild, also 50
   je Sekunde, egal wie viel Luft die Maschine hat. Die uebrige Zeit gibt es
   an die anderen Prozesse zurueck. */

int gx_takt_letzte = 0;

void gx_takt(int hundertstel) {
    int ziel;
    /* Der naechste Zeitpunkt wird vom vorigen aus gerechnet, nicht von
       "jetzt". Sonst geht bei jedem Bild der Rest der angebrochenen
       Hundertstelsekunde verloren, und aus 50 Bildern werden 40.

       Gewartet wird mit sleep, nicht mit einer Warteschleife: sleep legt den
       Prozess schlafen, der Prozessor darf anhalten und bleibt kalt. Eine
       Schleife, die staendig die Uhr abfragt, sieht fuer die Hardware aus wie
       Volllast -- der TB-32 wurde damit 65 Grad heiss und hat sich auf 40
       Prozent Takt gedrosselt. Das ganze System wurde dadurch zaeh. */
    ziel = gx_takt_letzte + hundertstel;
    while (ziel - ticks() >= 1) sleep(1);
    gx_takt_letzte = ziel;
    /* Wer laenger als ein Bild gebraucht hat, faengt neu an statt
       aufzuholen -- sonst kaeme nach einem Ruckler ein Sprint. */
    if (ticks() - gx_takt_letzte > hundertstel) gx_takt_letzte = ticks();
}

/* --- Flaechen ueber den Blitter ------------------------------------------ */

/* Ein Malbefehl geht jetzt ohne den Kernel: die Blitter-Ports gehoeren dem
   Programm genauso wie dem Betriebssystem. Vorher kostete jede Flaeche einen
   Sprung nach int 0x40 -- mit Sichern von 15 Registern und einer langen
   Fallunterscheidung. Sechs Portschreibvorgaenge sind zusammen billiger als
   dieser eine Sprung. */
void gx_kommando(int x, int y, int w, int h, int cmd) {
    portout(P_BLT_X, x);
    portout(P_BLT_Y, y);
    portout(P_BLT_W, w);
    portout(P_BLT_H, h);
    portout(P_BLT_CMD, cmd);
}

void gx_fill(int x, int y, int w, int h, int farbe) {
    portout(P_BLT_COL, farbe);
    gx_kommando(x, y, w, h, BLT_FILL);
}

void gx_frame(int x, int y, int w, int h, int farbe) {
    portout(P_BLT_COL, farbe);
    gx_kommando(x, y, w, h, BLT_FRAME);
}

/* --- Schrift ------------------------------------------------------------- */

/* bg = 256 heisst durchsichtig */
void gx_char(int x, int y, int c, int farbe, int bg) {
    portout(P_BLT_ZOOM, 1);
    portout(P_BLT_COL, farbe);
    portout(P_BLT_BG, bg);
    portout(P_BLT_X, x);
    portout(P_BLT_Y, y);
    portout(P_BLT_CHR, c);
    portout(P_BLT_CMD, BLT_CHAR);
}

/* Eine ganze Zeile: Farbe, Hintergrund und die Zeichensatzadresse aendern
   sich dabei nicht, also werden sie nur einmal geschrieben. Bei zwanzig
   Buchstaben spart das vierzig Schreibvorgaenge. */
void gx_text(int x, int y, char* s, int farbe) {
    portout(P_BLT_ZOOM, 1);
    portout(P_BLT_COL, farbe);
    portout(P_BLT_BG, 256);
    portout(P_BLT_Y, y);
    while (*s) {
        portout(P_BLT_X, x);
        portout(P_BLT_CHR, *s);
        portout(P_BLT_CMD, BLT_CHAR);
        x = x + 8;
        s++;
    }
}

int gx_breite(char* s) { return strlen(s) * 8; }

/* Eine Zahl malen. Ohne die muesste jedes Programm sich selbst eine
   Umwandlung schreiben -- und das haben schon drei getan. */
void gx_zahl(int x, int y, int n, int farbe) {
    char puffer[14];
    int i; int stelle; int minus;
    minus = 0;
    if (n < 0) { minus = 1; n = 0 - n; }
    i = 0;
    stelle = 1000000000;
    while (stelle > 1 && n < stelle) stelle = stelle / 10;
    if (minus) { puffer[i] = '-'; i++; }
    while (stelle > 0) {
        puffer[i] = '0' + (n / stelle) % 10;
        i++;
        stelle = stelle / 10;
    }
    puffer[i] = 0;
    gx_text(x, y, puffer, farbe);
}

/* Text mittig in einem Feld der Breite w */
void gx_text_mitte(int x, int y, int w, char* s, int farbe) {
    gx_text(x + (w - gx_breite(s)) / 2, y, s, farbe);
}

/* --- Einzelne Punkte, direkt in den Bildspeicher -------------------------- */

void gx_punkt(int x, int y, int farbe) {
    char* p;
    if (x < 0 || y < 0 || x >= GX_BREIT || y >= GX_HOCH) return;
    p = (char*)(GX_VRAM + y * GX_BREIT + x);
    *p = farbe;
}

/* Ein Zeichen vergroessert -- das macht jetzt die Karte selbst (Port 0x54).
   Vorher schrieb das Programm dafuer jeden Punkt einzeln: bei Zoom 3 sind
   das 576 Schreibvorgaenge fuer eine einzige Ziffer, und der Bildschirm las
   waehrenddessen schon mit. */
void gx_gross(int x, int y, int c, int farbe, int zoom) {
    portout(P_BLT_ZOOM, zoom);
    portout(P_BLT_COL, farbe);
    portout(P_BLT_BG, 256);
    portout(P_BLT_X, x);
    portout(P_BLT_Y, y);
    portout(P_BLT_CHR, c);
    portout(P_BLT_CMD, BLT_CHAR);
    portout(P_BLT_ZOOM, 1);
}

void gx_text_gross(int x, int y, char* s, int farbe, int zoom) {
    portout(P_BLT_ZOOM, zoom);
    portout(P_BLT_COL, farbe);
    portout(P_BLT_BG, 256);
    portout(P_BLT_Y, y);
    while (*s) {
        portout(P_BLT_X, x);
        portout(P_BLT_CHR, *s);
        portout(P_BLT_CMD, BLT_CHAR);
        x = x + 8 * zoom;
        s++;
    }
    portout(P_BLT_ZOOM, 1);
}

/* --- Knoepfe -------------------------------------------------------------- */

/* Ein Feld mit Licht- und Schattenkante. gedrueckt dreht die Kanten um --
   derselbe Trick, mit dem alle Fenstersysteme ihre Knoepfe plastisch machen. */
void gx_panel(int x, int y, int w, int h, int gedrueckt) {
    int hell; int dunkel;
    hell = GX_WEISS;
    dunkel = GX_DUNKEL;
    if (gedrueckt) { hell = GX_DUNKEL; dunkel = GX_WEISS; }
    gx_fill(x, y, w, h, GX_GRAU);
    gx_fill(x, y, w, 1, hell);
    gx_fill(x, y, 1, h, hell);
    gx_fill(x, y + h - 1, w, 1, dunkel);
    gx_fill(x + w - 1, y, 1, h, dunkel);
}

void gx_knopf(int x, int y, int w, int h, char* text) {
    gx_panel(x, y, w, h, 0);
    gx_text_mitte(x, y + (h - 8) / 2, w, text, GX_SCHWARZ);
}

/* --- Maus ---------------------------------------------------------------- */

/* Einmal je Durchlauf aufrufen: liest die Maus, zieht den Hardware-Zeiger
   nach und merkt sich den vorigen Tastenzustand. gx_klick() ist danach
   genau in dem Durchlauf 1, in dem die Taste neu gedrueckt wurde. */
void gx_maus_lesen() {
    gx_btn_alt = gx_btn;
    gx_mx = portin(P_MAUS_X);
    gx_my = portin(P_MAUS_Y);
    gx_btn = portin(P_MAUS_BTN);
    portout(P_MCUR_X, gx_mx);
    portout(P_MCUR_Y, gx_my);
}

int gx_klick()    { return gx_btn && gx_btn_alt == 0; }
int gx_maus_rad() { return portin(P_MAUS_RAD); }

/* Liegt die Maus im Rechteck? Nutzt gx_mx/gx_my, damit es mit vier
   Argumenten auskommt -- CC auf dem Geraet kann nicht mehr als fuenf. */
int gx_treffer(int x, int y, int w, int h) {
    return gx_mx >= x && gx_mx < x + w && gx_my >= y && gx_my < y + h;
}


/* ===========================================================================
   Ein Fenster auf dem Schreibtisch.

   Bis hierher hatte ein Programm den ganzen Bildschirm oder gar nichts. Mit
   diesen Funktionen bekommt es ein Fenster wie Word oder der Coder -- man
   kann es verschieben, es hat eine Titelleiste, und daneben laeuft alles
   andere weiter.

   Der Trick: das Programm malt NICHT auf den Bildschirm, sondern in seinen
   eigenen Puffer. Der Blitter kann das (Ports 0x5B..0x5D), und der
   Schreibtisch setzt die Puffer der Fenster zusammen. Deshalb kann kein
   Programm ueber ein fremdes Fenster malen, und ein verdecktes Fenster darf
   trotzdem weiterzeichnen, ohne dass man etwas davon sieht.

   Ablauf:
       nr = fenster_neu("Titel", 300, 200);
       while (1) {
           art = fenster_ereignis(e);
           if (art == FE_SCHLIESS) break;
           ... malen ...
           fenster_fertig();
       }
       fenster_zu();
   =========================================================================== */

#define FE_NICHTS    0
#define FE_TASTE     1             /* e[1] = Zeichen, e[2] = Tastennummer */
#define FE_KLICK     2             /* e[1] = x, e[2] = y im Fenster */
#define FE_SCHLIESS  3             /* der Benutzer hat auf das Kreuz geklickt */
#define FE_MALEN     4             /* bitte neu zeichnen */

int fn_nr = 0 - 1;                 /* unsere Fensternummer */
int fn_puffer = 0;                 /* wohin wir malen */
int fn_breite = 0;
int fn_hoehe = 0;

/* Den Blitter auf unseren Puffer richten. Muss vor jedem Malen stehen --
   dazwischen malt der Schreibtisch auf den Bildschirm und stellt ihn zurueck. */
void fenster_malziel() {
    portout(P_BLT_ZIEL, fn_puffer);
    portout(P_BLT_ZIELB, fn_breite);
    portout(P_BLT_ZIELH, fn_hoehe);
    portout(P_BLT_SRC, gx_font);
}

int fenster_neu(char* titel, int breite, int hoehe) {
    int daten[4];
    /* KEIN gx_start() hier -- das schaltet den Bildschirmmodus um und
       loescht das Bild. Fuer ein Vollbildprogramm ist das richtig, fuer ein
       Fenster waere es das Ende des Schreibtischs: schwarz, und der
       Schreibtisch weiss nichts davon. Ein Fenster braucht nur den
       Zeichensatz. */
    gx_font = fontaddr();
    fn_nr = sc(40, (int)titel, breite, hoehe, 0);
    if (fn_nr < 0) return 0 - 1;
    sc(42, fn_nr, (int)daten, 0, 0);
    fn_puffer = daten[0];
    fn_breite = daten[1];
    fn_hoehe = daten[2];
    return fn_nr;
}

/* Holt das naechste Ereignis. <e> muss Platz fuer drei Zahlen haben. */
int fenster_ereignis(int* e) {
    int daten[4];
    int art;
    art = sc(41, fn_nr, (int)e, 0, 0);
    if (art == FE_MALEN) {                 /* Groesse koennte sich geaendert haben */
        sc(42, fn_nr, (int)daten, 0, 0);
        fn_puffer = daten[0];
        fn_breite = daten[1];
        fn_hoehe = daten[2];
    }
    return art;
}

/* Fertig gemalt -- der Schreibtisch darf es zeigen. Danach gehoert der
   Blitter wieder dem Bildschirm. */
void fenster_fertig() {
    portout(P_BLT_ZIEL, 0);
    sc(43, fn_nr, 0, 0, 0);
}

void fenster_zu() {
    portout(P_BLT_ZIEL, 0);
    sc(44, fn_nr, 0, 0, 0);
    fn_nr = 0 - 1;
}
