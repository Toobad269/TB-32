/* ==========================================================================
   WORD  --  Textverarbeitung fuer TOOBAD-OS, jetzt als eigenes Programm

   Frueher im Kernel, jetzt eine Datei auf der Platte. Es malt in seinen
   eigenen Fensterpuffer; der Schreibtisch setzt ihn an die richtige Stelle.
   Der Umbau folgt demselben Muster wie bei Paint: aus win_x[i] wird 0, aus
   den g_-Funktionen die gx_-Funktionen, aus fs_read/fs_write und der
   Zwischenablage Systemaufrufe.

   Uebersetzen auf dem Geraet selbst:  CC WORD.C
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
#define C_WIN      7
#define C_TITLEBAR 1

#define K_BACKSPACE 14
#define K_TAB       15
#define K_HOME      71
#define K_END       79
#define K_PGUP      73
#define K_PGDN      81
#define K_DEL       83

#define P_DMA_SRC  0x56
#define P_DMA_DST  0x57
#define P_DMA_LEN  0x58
#define P_DMA_CMD  0x5A

/* Endet der Name auf <endung>? (ohne Gross-/Kleinschreibung) */
int endet_auf(char* name, char* endung) {
    int n; int e; int i;
    n = strlen(name);
    e = strlen(endung);
    if (n < e) return 0;
    for (i = 0; i < e; i++)
        if (toupper(name[n - e + i]) != toupper(endung[i])) return 0;
    return 1;
}

int treffer(int mx, int my, int x, int y, int w, int h) {
    return mx >= x && mx < x + w && my >= y && my < y + h;
}

void p_knopf(int x, int y, int w, int h, char* text, int gedrueckt) {
    gx_panel(x, y, w, h, gedrueckt);
    if (text[0])
        gx_text_mitte(x + gedrueckt, y + (h - 8) / 2 + gedrueckt, w, text,
                      C_TEXT);
}

void gx_str(int x, int y, int adr, int n, int farbe, int bg) {
    int i;
    for (i = 0; i < n; i++)
        gx_char(x + i * 8, y, byte_get(adr + i), farbe, bg);
}

void gx_num(int x, int y, int n, int farbe, int bg) { gx_zahl(x, y, n, farbe); }

void gx_bild(int x, int y, int w, int h, int quelle) {
    portout(P_BLT_SRC, quelle);
    portout(P_BLT_X, x);
    portout(P_BLT_Y, y);
    portout(P_BLT_W, w);
    portout(P_BLT_H, h);
    portout(P_BLT_CMD, 4);
    portout(P_BLT_SRC, gx_font);
}

/* Die Laenge der Zwischenablage steht im Kernel; hier eine Kopie, die vor
   jedem Zugriff geholt und danach zurueckgeschrieben wird. */
int clip_len = 0;

#define WD_TEXT      0x00720000      /* der Text selbst, ein Byte je Zeichen */
#define WD_MAX       30000
#define WD_FARBE     0x00728000      /* je Zeichen eine Farbe */
#define WD_ABS       0x00730000      /* je Absatz ein Byte Form */
#define WD_ABS_MAX   1000
#define WD_ABS2      0x00730400      /* je Absatz ein zweites Byte: Listen */
#define WD_BW        0x00730800      /* je Bildabsatz die Breite */
#define WD_BH        0x00731800      /* ... und die Hoehe */
#define WD_UMBRUCH   0x00733000      /* je Bildschirmzeile VIER Worte */
#define WD_UMBR_MAX  1500
#define WD_DATEI     0x00739000      /* Puffer zum Speichern und Laden */
#define WD_BILD      0x00750000      /* geladenes Bild, Punkte */
#define WD_BILD_MAX  200000

/* Formbyte: Bit 0-1 Groesse (1..3), Bit 2 fett, Bit 3 unterstrichen,
   Bit 4-5 Ausrichtung (0 links, 1 mittig, 2 rechts), Bit 6 = Bild */
#define WF_GROESSE   3
#define WF_FETT      4
#define WF_UNTER     8
#define WF_MITTE     16
#define WF_RECHTS    32
#define WF_BILD      64

#define WD_SEITE_B   470             /* Breite der Seite in Punkten */
#define WD_SEITE_H   620             /* Hoehe einer Seite in Punkten */
#define WD_RAND      18              /* Luecke zwischen zwei Seiten */

/* Zweites Formbyte: Listen. Ein Absatz ist entweder Aufzaehlung oder
   nummeriert, nie beides. */
#define WL_PUNKT     1
#define WL_ZAHL      2

int wd_len = 0;                      /* Zeichen im Text */
int wd_pos = 0;                      /* Schreibmarke */
int wd_top = 0;                      /* erste sichtbare Bildschirmzeile */
int wd_zeilen = 0;                   /* so viele stehen im Umbruch */
int wd_umbr_gueltig = 0;             /* Umbruch passt zum aktuellen Text? */
int wd_form = 1;                     /* Form fuer den naechsten neuen Absatz */
int wd_stift = C_BLACK;              /* Farbe fuer neu getippten Text */
int wd_namemode = 0;
int wd_namemode2 = 0;
/* Worauf wir warten, waehrend der Dateidialog offen ist:
   1 = neues Dokument, 2 = speichern unter, 3 = oeffnen, 4 = Bild. */
int wd_warte = 0;

/* Der Dialog gehoert dem Kernel; wir fragen in der Hauptschleife nach, ob
   der Benutzer etwas ausgewaehlt hat. */
int wd_dialog_pruefen() {
    char name[24];
    int r;
    if (wd_warte == 0) return 0;
    r = datei_gewaehlt(name);
    if (r == 0) return 0;
    if (r == 2) { wd_warte = 0; return 1; }
    memset(wd_name, 0, 20);
    strncpy(wd_name, name, 20);
    wd_ort = 1;
    if (wd_warte == 1) { wd_neu_anlegen(); wd_speichern(); }
    else if (wd_warte == 2) wd_speichern();
    else if (wd_warte == 3) wd_laden();
    else if (wd_warte == 4) wd_bild_einfuegen_name(wd_name);
    wd_warte = 0;
    return 1;
}
char wd_name[24];
/* 1 = Name und Ordner stehen fest, "Save" speichert ohne Nachfrage.
   Siehe pt_ort in paint.c -- dieselbe Regel. */
int  wd_ort = 0;
int wd_meldung = 0;                  /* 0 nichts, 1 gespeichert, 2 geladen, 3 Fehler */
int wd_sicht = 18;
int wd_hoehe = 260;                  /* Hoehe des Seitenbereichs im Fenster */

int wd_sel_von = 0 - 1;              /* Markierung */
int wd_sel_bis = 0 - 1;
int wd_zieht = 0;                    /* Maustaste haelt und zieht */
int wd_win = 0 - 1;
int wd_menue = 0;                    /* Rechtsklick-Menue offen */
int wd_menue_x = 0;
int wd_menue_y = 0;
int wd_bild_sel = 0 - 1;             /* markierter Bildabsatz */
int wd_griff = 0;                    /* Groessenanfasser gepackt */
int wd_griff_x = 0;
int wd_griff_y = 0;

char* wd_t() { return (char*)WD_TEXT; }

/* --- Absaetze ------------------------------------------------------------ */

int wd_absatz_bei(int pos) {
    char* t; int i; int n;
    t = wd_t();
    n = 0;
    for (i = 0; i < pos; i++) if (t[i] == 10) n++;
    return n;
}

int wd_absatz_anzahl() { return wd_absatz_bei(wd_len) + 1; }

int wd_formbyte(int absatz) {
    int f;
    if (absatz < 0 || absatz >= WD_ABS_MAX) return 1;
    f = byte_get(WD_ABS + absatz);
    if ((f & WF_GROESSE) == 0) f = f | 1;
    return f;
}

void wd_formbyte_setzen(int absatz, int wert) {
    if (absatz < 0 || absatz >= WD_ABS_MAX) return;
    byte_put(WD_ABS + absatz, wert);
}

int wd_groesse(int f) { return f & WF_GROESSE; }

int wd_liste(int absatz) {
    if (absatz < 0 || absatz >= WD_ABS_MAX) return 0;
    return byte_get(WD_ABS2 + absatz);
}

void wd_liste_setzen(int absatz, int wert) {
    if (absatz < 0 || absatz >= WD_ABS_MAX) return;
    byte_put(WD_ABS2 + absatz, wert);
}

/* Der wievielte Punkt einer nummerierten Liste ist dieser Absatz?
   Gezaehlt wird zurueck bis zum ersten Absatz, der keine Nummer traegt --
   so faengt jede neue Liste wieder bei eins an. */
int wd_listennummer(int absatz) {
    int i; int n;
    n = 1;
    for (i = absatz - 1; i >= 0; i--) {
        if ((wd_liste(i) & WL_ZAHL) == 0) break;
        n++;
    }
    return n;
}

/* Breite der Listenmarke in Punkten -- um so viel rueckt der Text ein. */
int wd_marke_breite(int absatz, int zoom) {
    int l;
    l = wd_liste(absatz);
    if (l & WL_PUNKT) return 2 * 8 * zoom;
    if (l & WL_ZAHL)  return 4 * 8 * zoom;
    return 0;
}

int  wd_bw(int a)          { return mem_get(WD_BW + a * 4); }
int  wd_bh(int a)          { return mem_get(WD_BH + a * 4); }
void wd_bw_setzen(int a, int v) { mem_put(WD_BW + a * 4, v); }
void wd_bh_setzen(int a, int v) { mem_put(WD_BH + a * 4, v); }

/* --- Umbruch -------------------------------------------------------------- */

int wd_u_start(int i)  { return mem_get(WD_UMBRUCH + i * 16); }
int wd_u_laenge(int i) { return mem_get(WD_UMBRUCH + i * 16 + 4); }
int wd_u_absatz(int i) { return mem_get(WD_UMBRUCH + i * 16 + 8); }
int wd_u_seite(int i)  { return mem_get(WD_UMBRUCH + i * 16 + 12); }

void wd_u_setzen(int i, int start, int laenge, int absatz) {
    if (i >= WD_UMBR_MAX) return;
    mem_put(WD_UMBRUCH + i * 16, start);
    mem_put(WD_UMBRUCH + i * 16 + 4, laenge);
    mem_put(WD_UMBRUCH + i * 16 + 8, absatz);
}

void wd_u_seite_setzen(int i, int seite) {
    if (i >= WD_UMBR_MAX) return;
    mem_put(WD_UMBRUCH + i * 16 + 12, seite);
}

/* Hoehe einer Bildschirmzeile in Punkten */
int wd_zeilenhoehe(int z) {
    int f; int a;
    a = wd_u_absatz(z);
    f = wd_formbyte(a);
    if (f & WF_BILD) return wd_bh(a) + 6;
    return 9 * wd_groesse(f) + 2;
}

void wd_umbrechen() {
    char* t; int p; int absatz; int zeile; int max_z; int anfang;
    int letzte_luecke; int i; int f; int hoehe; int seite; int y;

    t = wd_t();
    p = 0;
    absatz = 0;
    zeile = 0;

    while (p <= wd_len && zeile < WD_UMBR_MAX) {
        f = wd_formbyte(absatz);
        /* Eine Liste rueckt ein -- entsprechend weniger Zeichen passen hin. */
        max_z = (WD_SEITE_B - wd_marke_breite(absatz, wd_groesse(f)))
                / (8 * wd_groesse(f));
        if (max_z < 4) max_z = 4;

        anfang = p;
        letzte_luecke = 0 - 1;
        i = 0;
        while (p < wd_len && t[p] != 10 && i < max_z) {
            if (t[p] == 32) letzte_luecke = p;
            p++;
            i++;
        }
        if (p < wd_len && t[p] != 10 && t[p] != 32 && letzte_luecke > anfang)
            p = letzte_luecke + 1;
        wd_u_setzen(zeile, anfang, p - anfang, absatz);
        zeile++;

        if (p < wd_len && t[p] == 10) { p++; absatz++; }
        else if (p >= wd_len) break;
    }
    wd_zeilen = zeile;
    if (wd_zeilen == 0) { wd_u_setzen(0, 0, 0, 0); wd_zeilen = 1; }

    /* Zweiter Durchgang: Seiten einteilen. Eine Zeile, die nicht mehr auf
       die Seite passt, faengt die naechste an -- genau wie auf Papier. */
    seite = 1;
    y = 0;
    for (i = 0; i < wd_zeilen; i++) {
        hoehe = wd_zeilenhoehe(i);
        if (y + hoehe > WD_SEITE_H && y > 0) {
            seite++;
            y = 0;
        }
        wd_u_seite_setzen(i, seite);
        y = y + hoehe;
    }
    wd_umbr_gueltig = 1;
}

void wd_pruefen() { if (wd_umbr_gueltig == 0) wd_umbrechen(); }

int wd_zeile_von_pos() {
    int i;
    wd_pruefen();
    for (i = wd_zeilen - 1; i >= 0; i--)
        if (wd_pos >= wd_u_start(i)) return i;
    return 0;
}

/* --- Text aendern --------------------------------------------------------- */

void wd_einfuegen(int c) {
    char* t; int i; int a; int n;
    if (wd_len >= WD_MAX - 1) return;
    t = wd_t();
    for (i = wd_len; i > wd_pos; i--) {
        t[i] = t[i - 1];
        byte_put(WD_FARBE + i, byte_get(WD_FARBE + i - 1));
    }
    t[wd_pos] = c;
    byte_put(WD_FARBE + wd_pos, wd_stift);
    wd_len++;
    wd_pos++;
    if (c == 10) {
        a = wd_absatz_bei(wd_pos);
        n = wd_absatz_anzahl();
        for (i = n; i > a; i--) {
            wd_formbyte_setzen(i, wd_formbyte(i - 1));
            wd_liste_setzen(i, wd_liste(i - 1));
            wd_bw_setzen(i, wd_bw(i - 1));
            wd_bh_setzen(i, wd_bh(i - 1));
        }
        wd_formbyte_setzen(a, wd_formbyte(a - 1) & ~WF_BILD);
    }
    wd_umbr_gueltig = 0;
    wd_meldung = 0;
}

void wd_loeschen() {
    char* t; int i;
    if (wd_pos <= 0) return;
    t = wd_t();
    for (i = wd_pos - 1; i < wd_len - 1; i++) {
        t[i] = t[i + 1];
        byte_put(WD_FARBE + i, byte_get(WD_FARBE + i + 1));
    }
    wd_len--;
    wd_pos--;
    wd_umbr_gueltig = 0;
    wd_meldung = 0;
}

/* --- Zwischenablage -------------------------------------------------------
   Word benutzt dieselbe Ablage wie der Editor (CLIP_BUF). Damit wandert
   Text zwischen beiden Programmen -- und weil pc.py die Mac-Zwischenablage
   dorthin spiegelt, auch zwischen dem TB-32 und dem Mac.
   Die Farben kommen in eine eigene kleine Ablage daneben: bleibt die Laenge
   gleich, faerbt sich der eingefuegte Text wieder wie vorher. */

#define WD_CLIPF   0x00760000
int wd_clipf_len = 0;

void wd_kopieren() {
    int i; int n;
    if (wd_sel_von < 0 || wd_sel_bis <= wd_sel_von) return;
    n = wd_sel_bis - wd_sel_von;
    if (n > CLIP_MAX) n = CLIP_MAX;
    for (i = 0; i < n; i++) {
        byte_put(CLIP_BUF + i, byte_get(WD_TEXT + wd_sel_von + i));
        byte_put(WD_CLIPF + i, byte_get(WD_FARBE + wd_sel_von + i));
    }
    clip_len = n;
    wd_clipf_len = n;
}

void wd_auswahl_loeschen() {
    int i; int n;
    if (wd_sel_von < 0 || wd_sel_bis <= wd_sel_von) return;
    n = wd_sel_bis - wd_sel_von;
    wd_pos = wd_sel_bis;
    for (i = 0; i < n; i++) wd_loeschen();
    wd_sel_von = 0 - 1;
    wd_sel_bis = 0 - 1;
}

void wd_ausschneiden() {
    wd_kopieren();
    wd_auswahl_loeschen();
}

void wd_clip_einfuegen() {
    int i; int mit_farbe; int start;
    if (clip_len <= 0) return;
    wd_auswahl_loeschen();
    mit_farbe = 0;
    if (wd_clipf_len == clip_len) mit_farbe = 1;
    start = wd_pos;
    for (i = 0; i < clip_len; i++) wd_einfuegen(byte_get(CLIP_BUF + i));
    if (mit_farbe)
        for (i = 0; i < clip_len; i++)
            byte_put(WD_FARBE + start + i, byte_get(WD_CLIPF + i));
    wd_meldung = 0;
}

void wd_auswahl_weg() {
    wd_sel_von = 0 - 1;
    wd_sel_bis = 0 - 1;
}

void wd_neu_anlegen() {
    wd_len = 0; wd_pos = 0; wd_top = 0;
    wd_bild_sel = 0 - 1;
    wd_auswahl_weg();
    wd_umbr_gueltig = 0;
}

/* Die Markierung einfaerben -- das ist der Grund fuer den zweiten Puffer. */
void wd_faerben(int farbe) {
    int i;
    wd_stift = farbe;
    if (wd_sel_von < 0 || wd_sel_bis <= wd_sel_von) return;
    for (i = wd_sel_von; i < wd_sel_bis; i++) byte_put(WD_FARBE + i, farbe);
}

/* --- Bilder ---------------------------------------------------------------
   Ein Bild ist ein eigener Absatz. Sein Text ist der Dateiname, sein
   Formbyte traegt die Marke WF_BILD, und seine Groesse steht in zwei
   eigenen Feldern. Der Umbruch behandelt ihn wie eine sehr hohe Zeile. */

int wd_bild_geladen = 0 - 1;         /* welcher Absatz liegt im Puffer? */
int wd_bild_qb = 0;                  /* Groesse des geladenen Bildes */
int wd_bild_qh = 0;

/* Laedt das Bild eines Absatzes, wenn es nicht schon im Puffer liegt. */
int wd_bild_holen(int absatz, int start, int laenge) {
    char nm[24]; int i; int n;
    if (wd_bild_geladen == absatz) return wd_bild_qb > 0;
    if (laenge <= 0 || laenge > 20) return 0;
    for (i = 0; i < laenge; i++) nm[i] = byte_get(WD_TEXT + start + i);
    nm[laenge] = 0;
    n = fileread(nm, WD_DATEI, WD_BILD_MAX);
    if (n < 8) { wd_bild_geladen = absatz; wd_bild_qb = 0; return 0; }
    wd_bild_qb = mem_get(WD_DATEI);
    wd_bild_qh = mem_get(WD_DATEI + 4);
    if (wd_bild_qb <= 0 || wd_bild_qh <= 0
        || wd_bild_qb * wd_bild_qh > WD_BILD_MAX) {
        wd_bild_geladen = absatz;
        wd_bild_qb = 0;
        return 0;
    }
    /* Punkte hinter dem Kopf an ihren Platz schaufeln -- der Blockkopierer
       macht das in einem Rutsch. */
    /* Paint ist ausgezogen -- der Blockkopierer bleibt aber Hardware und
       steht jedem offen. */
#define P_DMA_SRC  0x56
#define P_DMA_DST  0x57
#define P_DMA_LEN  0x58
#define P_DMA_CMD  0x5A
    portout(P_DMA_SRC, WD_DATEI + 8);
    portout(P_DMA_DST, WD_BILD);
    portout(P_DMA_LEN, wd_bild_qb * wd_bild_qh);
    portout(P_DMA_CMD, 1);
    wd_bild_geladen = absatz;
    return 1;
}

void wd_bild_einfuegen_name(char* dateiname) {
    int a; int i; int n;
    char nm[24];
    strncpy(nm, dateiname, 20);
    if (endet_auf(nm, ".TBI") == 0) strncpy(nm, "BILD.TBI", 20);
    /* Eigener Absatz: davor und dahinter ein Umbruch. */
    if (wd_pos > 0 && byte_get(WD_TEXT + wd_pos - 1) != 10) wd_einfuegen(10);
    a = wd_absatz_bei(wd_pos);
    n = strlen(nm);
    for (i = 0; i < n; i++) wd_einfuegen(nm[i]);
    wd_formbyte_setzen(a, 1 | WF_BILD);
    wd_bw_setzen(a, 200);
    wd_bh_setzen(a, 130);
    wd_einfuegen(10);
    wd_bild_geladen = 0 - 2;
    wd_bild_sel = a;
    wd_umbr_gueltig = 0;
}

/* Ein Bild wieder herausnehmen. Ein Bild ist ein ganzer Absatz -- also
   muessen sein Dateiname UND der Absatzumbruch dahinter weg, sonst bleibt
   eine leere Zeile stehen. Ueber die Ruecktaste einzeln zu loeschen waere
   sinnlos: man wuerde Buchstaben des Dateinamens abknabbern. */
void wd_bild_loeschen(int absatz) {
    int i; int start; int ende; int n;
    if (absatz < 0) return;
    /* Anfang und Ende des Absatzes suchen */
    start = 0;
    n = 0;
    for (i = 0; i < wd_len; i++) {
        if (n == absatz) break;
        if (byte_get(WD_TEXT + i) == 10) n++;
        start = i + 1;
    }
    if (n != absatz) return;
    ende = start;
    while (ende < wd_len && byte_get(WD_TEXT + ende) != 10) ende++;
    if (ende < wd_len) ende++;               /* den Umbruch mitnehmen */

    wd_pos = ende;
    for (i = 0; i < ende - start; i++) wd_loeschen();

    /* Die Formen der Absaetze dahinter ruecken auf */
    n = wd_absatz_anzahl();
    for (i = absatz; i < n; i++) {
        wd_formbyte_setzen(i, wd_formbyte(i + 1));
        wd_liste_setzen(i, wd_liste(i + 1));
        wd_bw_setzen(i, wd_bw(i + 1));
        wd_bh_setzen(i, wd_bh(i + 1));
    }
    wd_bild_sel = 0 - 1;
    wd_bild_geladen = 0 - 2;
    wd_umbr_gueltig = 0;
}

/* --- Datei ---------------------------------------------------------------
   Format TBW: Laenge, Anzahl Absaetze, dann die Formbytes, dann Breite und
   Hoehe je Absatz, dann die Farben, dann der Text. */

void wd_speichern() {
    int i; int a; int p;
    if (wd_name[0] == 0) { wd_meldung = 3; return; }
    a = wd_absatz_anzahl();
    mem_put(WD_DATEI, wd_len);
    mem_put(WD_DATEI + 4, a);
    p = 8;
    for (i = 0; i < a; i++) { byte_put(WD_DATEI + p, wd_formbyte(i)); p++; }
    for (i = 0; i < a; i++) { mem_put(WD_DATEI + p, wd_bw(i)); p = p + 4; }
    for (i = 0; i < a; i++) { mem_put(WD_DATEI + p, wd_bh(i)); p = p + 4; }
    for (i = 0; i < wd_len; i++) { byte_put(WD_DATEI + p, byte_get(WD_FARBE + i)); p++; }
    for (i = 0; i < wd_len; i++) { byte_put(WD_DATEI + p, byte_get(WD_TEXT + i)); p++; }
    /* Die Listen kommen ganz ans Ende. Aeltere Dateien haben dort nichts --
       die werden dann einfach ohne Listen geladen, statt kaputtzugehen. */
    for (i = 0; i < a; i++) { byte_put(WD_DATEI + p, wd_liste(i)); p++; }
    if (filewrite(wd_name, WD_DATEI, p) < 0) wd_meldung = 3;
    else wd_meldung = 1;
}

void wd_laden() {
    int n; int i; int a; int p;
    if (wd_name[0] == 0) { wd_meldung = 3; return; }
    n = fileread(wd_name, WD_DATEI, WD_MAX * 2 + 12000);
    if (n < 8) { wd_meldung = 3; return; }
    wd_len = mem_get(WD_DATEI);
    a = mem_get(WD_DATEI + 4);
    if (wd_len < 0 || wd_len > WD_MAX || a < 1 || a > WD_ABS_MAX) {
        wd_len = 0;
        wd_meldung = 3;
        return;
    }
    p = 8;
    for (i = 0; i < a; i++) { wd_formbyte_setzen(i, byte_get(WD_DATEI + p)); p++; }
    for (i = 0; i < a; i++) { wd_bw_setzen(i, mem_get(WD_DATEI + p)); p = p + 4; }
    for (i = 0; i < a; i++) { wd_bh_setzen(i, mem_get(WD_DATEI + p)); p = p + 4; }
    for (i = 0; i < wd_len; i++) { byte_put(WD_FARBE + i, byte_get(WD_DATEI + p)); p++; }
    for (i = 0; i < wd_len; i++) { byte_put(WD_TEXT + i, byte_get(WD_DATEI + p)); p++; }
    for (i = 0; i < a; i++) wd_liste_setzen(i, 0);
    if (n >= p + a)                          /* neuere Datei mit Listen */
        for (i = 0; i < a; i++) { wd_liste_setzen(i, byte_get(WD_DATEI + p)); p++; }
    wd_pos = 0;
    wd_top = 0;
    wd_bild_geladen = 0 - 2;
    wd_auswahl_weg();
    wd_umbr_gueltig = 0;
    wd_meldung = 2;
}

/* Der Rechner hat keinen Drucker -- also gibt es die Datei. Geschrieben
   wird reiner Text: Formen fallen weg, Listenmarken werden ausgeschrieben,
   Bilder erscheinen als Hinweis in eckigen Klammern. So laesst sich ein
   Dokument mit TYPE anzeigen oder im Coder oeffnen. */
void wd_als_text() {
    char nm[24]; char zahl[12];
    int i; int n; int a; int p; int l; int j;
    strncpy(nm, wd_name, 20);
    n = strlen(nm);
    i = n;
    while (i > 0 && nm[i - 1] != '.') i--;
    if (i == 0) i = n + 1;
    nm[i - 1] = 0;
    strcat(nm, ".TXT");

    p = 0;
    a = 0;
    i = 0;
    while (i <= wd_len) {
        if (i == 0 || byte_get(WD_TEXT + i - 1) == 10) {
            /* Anfang eines Absatzes: Marke ausschreiben */
            if (wd_formbyte(a) & WF_BILD) {
                byte_put(WD_DATEI + p, '['); p++;
                byte_put(WD_DATEI + p, 'B'); p++;
                byte_put(WD_DATEI + p, 'i'); p++;
                byte_put(WD_DATEI + p, 'l'); p++;
                byte_put(WD_DATEI + p, 'd'); p++;
                byte_put(WD_DATEI + p, ':'); p++;
                byte_put(WD_DATEI + p, ' '); p++;
            } else {
                l = wd_liste(a);
                if (l & WL_PUNKT) {
                    byte_put(WD_DATEI + p, '-'); p++;
                    byte_put(WD_DATEI + p, ' '); p++;
                } else if (l & WL_ZAHL) {
                    itoa(wd_listennummer(a), zahl);
                    for (j = 0; zahl[j]; j++) { byte_put(WD_DATEI + p, zahl[j]); p++; }
                    byte_put(WD_DATEI + p, '.'); p++;
                    byte_put(WD_DATEI + p, ' '); p++;
                }
            }
        }
        if (i == wd_len) break;
        if (byte_get(WD_TEXT + i) == 10 && (wd_formbyte(a) & WF_BILD)) {
            byte_put(WD_DATEI + p, ']'); p++;
        }
        byte_put(WD_DATEI + p, byte_get(WD_TEXT + i));
        p++;
        if (byte_get(WD_TEXT + i) == 10) a++;
        i++;
    }
    if (filewrite(nm, WD_DATEI, p) < 0) wd_meldung = 3;
    else wd_meldung = 4;
}

/* --- Malen ---------------------------------------------------------------- */

/* Wo faengt die Zeile an? Haengt an der Ausrichtung. */
int wd_zeilen_x(int px, int zeile, int f) {
    int breite; int x; int rand;
    breite = wd_u_laenge(zeile) * 8 * wd_groesse(f);
    rand = wd_marke_breite(wd_u_absatz(zeile), wd_groesse(f));
    x = px + rand;
    if (f & WF_MITTE)  x = px + rand + (WD_SEITE_B - rand - breite) / 2;
    if (f & WF_RECHTS) x = px + WD_SEITE_B - breite;
    if (x < px + rand) x = px + rand;
    return x;
}

/* Die Marke einer Liste: ein Kaestchen bei Aufzaehlungen, sonst die Nummer.
   Ein Punkt aus dem Zeichensatz waere schoener, aber der hat nur ASCII --
   ein gemaltes Kaestchen sieht in jeder Groesse gleich gut aus. */
void wd_marke_malen(int px, int py, int absatz, int zoom) {
    int l; int n; char txt[8];
    l = wd_liste(absatz);
    if (l & WL_PUNKT) {
        gx_fill(px + 2 * zoom, py + 3 * zoom, 3 * zoom, 3 * zoom, C_BLACK);
        return;
    }
    if (l & WL_ZAHL) {
        n = wd_listennummer(absatz);
        itoa(n, txt);
        strcat(txt, ".");
        portout(P_BLT_ZOOM, zoom);
        gx_str(px, py, (int)txt, strlen(txt), C_BLACK, 256);
        portout(P_BLT_ZOOM, 1);
    }
}

/* Eine Textzeile mit Farben. Gleichfarbige Stuecke gehen in einem
   Malbefehl an den Blitter -- dieselbe Technik wie im Coder. */
void wd_zeile_malen(int px, int py, int zeile, int f) {
    int n; int start; int zoom; int x; int i; int c;
    int lauf; int lauf_farbe; int lauf_x; int markiert;

    n = wd_u_laenge(zeile);
    if (n <= 0) return;
    start = wd_u_start(zeile);
    zoom = wd_groesse(f);
    x = wd_zeilen_x(px, zeile, f);
    portout(P_BLT_ZOOM, zoom);

    lauf = 0;
    lauf_farbe = C_BLACK;
    lauf_x = x;
    for (i = 0; i < n; i++) {
        c = byte_get(WD_FARBE + start + i);
        markiert = 0;
        if (wd_sel_von >= 0 && start + i >= wd_sel_von && start + i < wd_sel_bis)
            markiert = 1;
        if (markiert) {
            if (lauf > 0) {
                gx_str(lauf_x, py, WD_TEXT + start + i - lauf, lauf, lauf_farbe, 256);
                lauf = 0;
            }
            gx_fill(x + i * 8 * zoom, py, 8 * zoom, 9 * zoom, C_TITLEBAR);
            gx_str(x + i * 8 * zoom, py, WD_TEXT + start + i, 1, C_WHITE, 256);
            lauf_x = x + (i + 1) * 8 * zoom;
        } else if (lauf > 0 && c == lauf_farbe) {
            lauf++;
        } else {
            if (lauf > 0)
                gx_str(lauf_x, py, WD_TEXT + start + i - lauf, lauf, lauf_farbe, 256);
            lauf_x = x + i * 8 * zoom;
            lauf_farbe = c;
            lauf = 1;
        }
    }
    if (lauf > 0)
        gx_str(lauf_x, py, WD_TEXT + start + n - lauf, lauf, lauf_farbe, 256);

    if (f & WF_FETT) {
        /* Fett: dasselbe noch einmal einen Punkt versetzt. Genau so haben es
           Nadeldrucker gemacht. */
        gx_str(x + 1, py, WD_TEXT + start, n, lauf_farbe, 256);
    }
    portout(P_BLT_ZOOM, 1);
    if (f & WF_UNTER)
        gx_fill(x, py + 8 * zoom, n * 8 * zoom, 1, C_BLACK);
}

void wd_bild_malen(int px, int py, int zeile, int a) {
    int bw; int bh; int x;
    bw = wd_bw(a);
    bh = wd_bh(a);
    x = px;
    if (wd_formbyte(a) & WF_MITTE)  x = px + (WD_SEITE_B - bw) / 2;
    if (wd_formbyte(a) & WF_RECHTS) x = px + WD_SEITE_B - bw;
    if (x < px) x = px;

    if (wd_bild_holen(a, wd_u_start(zeile), wd_u_laenge(zeile))) {
        portout(P_BLT_SRC, WD_BILD);
        portout(P_BLT_CHR, (wd_bild_qb & 65535) | ((wd_bild_qh & 65535) << 16));
        portout(P_BLT_X, x & 65535);
        portout(P_BLT_Y, py & 65535);
        portout(P_BLT_W, bw);
        portout(P_BLT_H, bh);
        portout(P_BLT_CMD, 7);
        portout(P_BLT_SRC, gx_font);      /* Zeichensatz zurueckstellen */
        portout(P_BLT_CHR, 32);
    } else {
        gx_fill(x, py, bw, bh, C_WIN);
        gx_text(x + 4, py + 4, "Picture missing:", C_WARN, 256);
        gx_str(x + 4, py + 16, WD_TEXT + wd_u_start(zeile), wd_u_laenge(zeile),
              C_TEXT, 256);
    }
    gx_frame(x, py, bw, bh, C_WINDARK);
    if (wd_bild_sel == a) {
        gx_frame(x - 1, py - 1, bw + 2, bh + 2, C_ACCENT);
        gx_fill(x + bw - 6, py + bh - 6, 8, 8, C_ACCENT);   /* Anfasser */
    }
}

/* --- Rechtsklick-Menue ---------------------------------------------------- */

#define WDM_ANZ  14
#define WDM_B    166
#define WDM_ZH   14

char* wdm_text(int i) {
    if (i == 0) return "Black";
    if (i == 1) return "Red";
    if (i == 2) return "Green";
    if (i == 3) return "Blue";
    if (i == 4) return "Orange";
    if (i == 5) return "Grey";
    if (i == 6) return "Copy           ^C";
    if (i == 7) return "Cut            ^X";
    if (i == 8) return "Paste          ^V";
    if (i == 9) return "Select all";
    if (i == 10) return "Deselect";
    if (i == 11) return "Insert picture";
    if (i == 12) return "Delete picture";
    return "Save as text";
}

int wdm_farbe(int i) {
    if (i == 0) return C_BLACK;
    if (i == 1) return 16 + 5 * 36;
    if (i == 2) return 16 + 3 * 6;
    if (i == 3) return 16 + 4;
    if (i == 4) return 16 + 5 * 36 + 3 * 6;
    if (i == 5) return 16 + 2 * 36 + 2 * 6 + 2;
    return C_BLACK;
}

void wd_menue_malen() {
    int i; int hoehe;
    hoehe = WDM_ANZ * WDM_ZH + 6;
    gx_panel(wd_menue_x, wd_menue_y, WDM_B, hoehe, 0);
    for (i = 0; i < WDM_ANZ; i++) {
        if (i < 6) gx_fill(wd_menue_x + 5, wd_menue_y + 5 + i * WDM_ZH, 9, 9,
                          wdm_farbe(i));
        if (i == 6 || i == 9) gx_fill(wd_menue_x + 4, wd_menue_y + 3 + i * WDM_ZH - 1,
                                     WDM_B - 8, 1, C_WINDARK);
        gx_text(wd_menue_x + 18, wd_menue_y + 5 + i * WDM_ZH, wdm_text(i),
               C_TEXT, 256);
    }
}

int wd_menue_klick(int mx, int my) {
    int i;
    if (mx < wd_menue_x || mx > wd_menue_x + WDM_B) { wd_menue = 0; return 1; }
    i = (my - wd_menue_y - 4) / WDM_ZH;
    if (i < 0 || i >= WDM_ANZ) { wd_menue = 0; return 1; }
    if (i < 6) wd_faerben(wdm_farbe(i));
    if (i == 6) wd_kopieren();
    if (i == 7) wd_ausschneiden();
    if (i == 8) wd_clip_einfuegen();
    if (i == 9) { wd_sel_von = 0; wd_sel_bis = wd_len; }
    if (i == 10) wd_auswahl_weg();
    if (i == 11) { wd_warte = 4; datei_dialog(DLG_BILD, ".TBI", "BILD.TBI"); }
    if (i == 12) wd_bild_loeschen(wd_bild_sel);
    if (i == 13) wd_als_text();
    wd_menue = 0;
    return 1;
}

/* --- Das Fenster ---------------------------------------------------------- */

int wd_seite_x(int i) { return (fn_breite - WD_SEITE_B) / 2; }
int wd_seite_y(int i) { return 24; }

void app_word(int i) {
    int x; int y; int px; int py; int z; int f; int hoehe; int k;
    int cz; int breite; int a;

    x = 0;
    y = 0;
    breite = fn_breite;
    hoehe = fn_hoehe;

    wd_pruefen();

    p_knopf(x + 4, y + 3, 22, 16, "A", wd_groesse(wd_form) == 1);
    p_knopf(x + 28, y + 3, 22, 16, "A+", wd_groesse(wd_form) == 2);
    p_knopf(x + 52, y + 3, 22, 16, "A*", wd_groesse(wd_form) == 3);
    p_knopf(x + 82, y + 3, 22, 16, "B", wd_form & WF_FETT);
    p_knopf(x + 106, y + 3, 22, 16, "U", wd_form & WF_UNTER);
    p_knopf(x + 136, y + 3, 26, 16, "|<", (wd_form & (WF_MITTE | WF_RECHTS)) == 0);
    p_knopf(x + 164, y + 3, 26, 16, "><", wd_form & WF_MITTE);
    p_knopf(x + 192, y + 3, 26, 16, ">|", wd_form & WF_RECHTS);
    p_knopf(x + 222, y + 3, 24, 16, "*", wd_liste(wd_absatz_bei(wd_pos)) & WL_PUNKT);
    p_knopf(x + 248, y + 3, 24, 16, "1.", wd_liste(wd_absatz_bei(wd_pos)) & WL_ZAHL);
    gx_fill(x + 278, y + 6, 12, 10, wd_stift);
    gx_frame(x + 278, y + 6, 12, 10, C_WINDARK);
    p_knopf(x + 294, y + 3, 40, 16, "New", 0);
    p_knopf(x + 336, y + 3, 46, 16, "Save", 0);
    p_knopf(x + 384, y + 3, 46, 16, "Open", 0);
    gx_fill(x + 432, y + 5, 92, 12, C_WHITE);
    gx_frame(x + 432, y + 5, 92, 12, C_WINDARK);
    gx_text(x + 435, y + 7, wd_name, C_TEXT, 256);
    if (wd_namemode) gx_fill(x + 435 + strlen(wd_name) * 8, y + 7, 7, 8, C_ACCENT);
    if (wd_meldung == 1) gx_text(x + 530, y + 7, "saved", C_GOOD, 256);
    if (wd_meldung == 2) gx_text(x + 530, y + 7, "loaded", C_GOOD, 256);
    if (wd_meldung == 3) gx_text(x + 530, y + 7, "no file", C_WARN, 256);
    if (wd_meldung == 4) gx_text(x + 530, y + 7, "text", C_GOOD, 256);

    px = wd_seite_x(i);
    py = wd_seite_y(i);
    gx_fill(px - 8, py - 4, WD_SEITE_B + 16, hoehe - 30, C_WHITE);
    gx_frame(px - 8, py - 4, WD_SEITE_B + 16, hoehe - 30, C_WINDARK);

    cz = wd_zeile_von_pos();
    wd_hoehe = hoehe - 34;
    wd_sicht = wd_hoehe / 12;
    if (wd_sicht < 3) wd_sicht = 3;
    if (cz < wd_top) wd_top = cz;

    k = py;
    for (z = wd_top; z < wd_zeilen; z++) {
        a = wd_u_absatz(z);
        f = wd_formbyte(a);

        /* Seitenwechsel: eine Trennlinie und die Nummer der neuen Seite.
           Welche Zeile auf welche Seite gehoert, hat der Umbruch schon
           ausgerechnet -- hier wird es nur noch sichtbar gemacht. */
        if (z > wd_top && wd_u_seite(z) != wd_u_seite(z - 1)) {
            if (k + WD_RAND > y + hoehe - 8) break;
            gx_fill(px - 8, k + 4, WD_SEITE_B + 16, 1, C_WINDARK);
            gx_text(px + WD_SEITE_B - 60, k + 7, "Page", C_WINDARK, 256);
            gx_num(px + WD_SEITE_B - 16, k + 7, wd_u_seite(z), C_WINDARK, 256);
            k = k + WD_RAND;
        }

        if (k + wd_zeilenhoehe(z) > y + hoehe - 8) break;
        if (f & WF_BILD) {
            wd_bild_malen(px, k, z, a);
        } else {
            /* Die Marke steht nur an der ERSTEN Zeile eines Absatzes --
               Folgezeilen sind nur eingerueckt. */
            if (z == 0 || wd_u_absatz(z - 1) != a)
                wd_marke_malen(px, k, a, wd_groesse(f));
            wd_zeile_malen(px, k, z, f);
            if (z == cz && wd_sel_von < 0) {
                int sp;
                sp = wd_pos - wd_u_start(z);
                if (sp < 0) sp = 0;
                if (sp > wd_u_laenge(z)) sp = wd_u_laenge(z);
                gx_fill(wd_zeilen_x(px, z, f) + sp * 8 * wd_groesse(f), k, 1,
                       8 * wd_groesse(f), C_ACCENT);
            }
        }
        k = k + wd_zeilenhoehe(z);
    }
    if (cz >= z && wd_top < wd_zeilen - 1) wd_top++;

    if (wd_menue) wd_menue_malen();
}

/* --- Bedienung ------------------------------------------------------------ */

/* Liste an- oder ausschalten. Wie bei der Form gilt: gibt es eine
   Markierung, sind alle Absaetze darin gemeint, sonst der am Cursor. */
void wd_liste_umschalten(int art) {
    int von; int bis; int i; int an;
    if (wd_sel_von >= 0 && wd_sel_bis > wd_sel_von) {
        von = wd_absatz_bei(wd_sel_von);
        bis = wd_absatz_bei(wd_sel_bis);
    } else {
        von = wd_absatz_bei(wd_pos);
        bis = von;
    }
    an = (wd_liste(von) & art) ? 0 : art;
    for (i = von; i <= bis; i++)
        if ((wd_formbyte(i) & WF_BILD) == 0) wd_liste_setzen(i, an);
    wd_umbr_gueltig = 0;
}

/* Wie viele Zeilen passen ab z ins Fenster? Zeilen sind verschieden hoch --
   eine Ueberschrift in Groesse 3 nimmt dreimal so viel Platz wie normaler
   Text, und ein Bild noch viel mehr. Mit einer festen Zahl blaettert man
   deshalb entweder zu weit oder zu kurz. */
int wd_passt(int z) {
    int n; int y;
    n = 0;
    y = 0;
    while (z + n < wd_zeilen) {
        y = y + wd_zeilenhoehe(z + n);
        if (y > wd_hoehe) break;
        n++;
    }
    if (n < 1) n = 1;
    return n;
}

/* Nicht ueber das Ende hinaus blaettern: die letzte Zeile soll unten
   stehen, nicht oben in einer sonst leeren Seite. */
void wd_top_begrenzen() {
    int h; int i;
    if (wd_top > wd_zeilen - 1) wd_top = wd_zeilen - 1;
    if (wd_top < 0) wd_top = 0;
    while (wd_top > 0) {
        h = 0;
        for (i = wd_top - 1; i < wd_zeilen; i++) h = h + wd_zeilenhoehe(i);
        if (h > wd_hoehe) break;
        wd_top--;
    }
}

void wd_form_setzen(int neu) {
    int a; int b_; int von; int bis;
    wd_form = neu;
    /* Bei einer Markierung alle betroffenen Absaetze umstellen, sonst nur
       den, in dem die Schreibmarke steht. */
    if (wd_sel_von >= 0 && wd_sel_bis > wd_sel_von) {
        von = wd_absatz_bei(wd_sel_von);
        bis = wd_absatz_bei(wd_sel_bis);
        for (b_ = von; b_ <= bis; b_++)
            if ((wd_formbyte(b_) & WF_BILD) == 0) wd_formbyte_setzen(b_, neu);
    } else {
        a = wd_absatz_bei(wd_pos);
        if ((wd_formbyte(a) & WF_BILD) == 0) wd_formbyte_setzen(a, neu);
    }
    wd_umbr_gueltig = 0;
}

/* Aus einer Mausposition eine Textstelle machen */
int wd_pos_von_maus(int i, int mx, int my) {
    int px; int py; int z; int f; int k; int sp; int h; int zx;
    px = wd_seite_x(i);
    py = wd_seite_y(i);
    wd_pruefen();
    k = py;
    for (z = wd_top; z < wd_zeilen; z++) {
        h = wd_zeilenhoehe(z);
        if (my < k + h) {
            f = wd_formbyte(wd_u_absatz(z));
            if (f & WF_BILD) return wd_u_start(z);
            zx = wd_zeilen_x(px, z, f);
            sp = (mx - zx + 4 * wd_groesse(f)) / (8 * wd_groesse(f));
            if (sp < 0) sp = 0;
            if (sp > wd_u_laenge(z)) sp = wd_u_laenge(z);
            return wd_u_start(z) + sp;
        }
        k = k + h;
    }
    return wd_len;
}

/* Steckt die Maus im Anfasser eines markierten Bildes? */
int wd_auf_griff(int i, int mx, int my) {
    int px; int py; int z; int k; int a; int bw; int bh; int gx;
    if (wd_bild_sel < 0) return 0;
    px = wd_seite_x(i);
    py = wd_seite_y(i);
    k = py;
    for (z = wd_top; z < wd_zeilen; z++) {
        a = wd_u_absatz(z);
        if (a == wd_bild_sel && (wd_formbyte(a) & WF_BILD)) {
            bw = wd_bw(a);
            bh = wd_bh(a);
            gx = px;
            if (wd_formbyte(a) & WF_MITTE)  gx = px + (WD_SEITE_B - bw) / 2;
            if (wd_formbyte(a) & WF_RECHTS) gx = px + WD_SEITE_B - bw;
            if (gx < px) gx = px;
            if (treffer(mx, my, gx + bw - 8, k + bh - 8, 12, 12)) return 1;
            return 0;
        }
        k = k + wd_zeilenhoehe(z);
    }
    return 0;
}

int wd_klick(int i, int mx, int my) {
    int x; int y; int p; int a;
    x = 0;
    y = 0;

    if (wd_menue) return wd_menue_klick(mx, my);
    wd_meldung = 0;

    if (treffer(mx, my, x + 4, y + 3, 22, 16))
        { wd_form_setzen((wd_form & ~WF_GROESSE) | 1); return 1; }
    if (treffer(mx, my, x + 28, y + 3, 22, 16))
        { wd_form_setzen((wd_form & ~WF_GROESSE) | 2); return 1; }
    if (treffer(mx, my, x + 52, y + 3, 22, 16))
        { wd_form_setzen((wd_form & ~WF_GROESSE) | 3); return 1; }
    if (treffer(mx, my, x + 82, y + 3, 22, 16))
        { wd_form_setzen(wd_form ^ WF_FETT); return 1; }
    if (treffer(mx, my, x + 106, y + 3, 22, 16))
        { wd_form_setzen(wd_form ^ WF_UNTER); return 1; }
    if (treffer(mx, my, x + 136, y + 3, 26, 16))
        { wd_form_setzen(wd_form & ~(WF_MITTE | WF_RECHTS)); return 1; }
    if (treffer(mx, my, x + 164, y + 3, 26, 16))
        { wd_form_setzen((wd_form & ~WF_RECHTS) | WF_MITTE); return 1; }
    if (treffer(mx, my, x + 192, y + 3, 26, 16))
        { wd_form_setzen((wd_form & ~WF_MITTE) | WF_RECHTS); return 1; }
    if (treffer(mx, my, x + 222, y + 3, 24, 16)) { wd_liste_umschalten(WL_PUNKT); return 1; }
    if (treffer(mx, my, x + 248, y + 3, 24, 16)) { wd_liste_umschalten(WL_ZAHL);  return 1; }
    if (treffer(mx, my, x + 294, y + 3, 40, 16)) {
        /* "Neu": erst den Platz aussuchen, dann entsteht das Dokument.
           Der Dateidialog gehoert dem Kernel und steht jedem Programm
           offen -- so sieht er ueberall gleich aus. */
        wd_ort = 0;
        wd_warte = 1;                /* 1 = neu anlegen, wenn der Name steht */
        datei_dialog(DLG_SPEICHERN, ".TBW",
                     wd_name[0] ? wd_name : "DOCUMENT.TBW");
        return 1;
    }
    if (treffer(mx, my, x + 336, y + 3, 46, 16)) {
        if (wd_ort && wd_name[0]) wd_speichern();
        else { wd_warte = 2; datei_dialog(DLG_SPEICHERN, ".TBW", wd_name); }
        return 1;
    }
    if (treffer(mx, my, x + 384, y + 3, 46, 16)) {
        wd_warte = 3; datei_dialog(DLG_OEFFNEN, ".TBW", wd_name);
        return 1;
    }
    if (treffer(mx, my, x + 432, y + 5, 92, 12)) { wd_namemode = 1; return 1; }
    wd_namemode = 0;

    /* Rechte Maustaste: Menue an der Zeigerspitze. Welche Taste es war,
       steht in der Maus-Hardware -- der Kernel reichte es frueher als
       gui_taste durch. */
    if (portin(P_MAUS_BTN) & 4) {
        wd_menue_x = mx;
        wd_menue_y = my;
        if (wd_menue_x + WDM_B > fn_breite) wd_menue_x = fn_breite - WDM_B - 2;
        if (wd_menue_y + WDM_ANZ * WDM_ZH + 6 > fn_hoehe)
            wd_menue_y = fn_hoehe - WDM_ANZ * WDM_ZH - 8;
        wd_menue = 1;
        return 1;
    }

    if (wd_auf_griff(i, mx, my)) {       /* Bildgroesse ziehen */
        wd_griff = 1;
        wd_griff_x = mx;
        wd_griff_y = my;
        wd_win = i;
        return 1;
    }

    p = wd_pos_von_maus(i, mx, my);
    a = wd_absatz_bei(p);
    if (wd_formbyte(a) & WF_BILD) {
        wd_bild_sel = a;
        wd_auswahl_weg();
        wd_pos = p;
        return 1;
    }
    wd_bild_sel = 0 - 1;
    wd_pos = p;
    wd_sel_von = p;
    wd_sel_bis = p;
    wd_zieht = 1;
    wd_win = i;
    return 1;
}

void wd_ziehen(int mx, int my) {
    int p; int bw; int bh; int a;
    if (wd_griff && wd_win >= 0) {
        a = wd_bild_sel;
        if (a < 0) return;
        bw = wd_bw(a) + (mx - wd_griff_x);
        bh = wd_bh(a) + (my - wd_griff_y);
        if (bw < 24) bw = 24;
        if (bh < 24) bh = 24;
        if (bw > WD_SEITE_B) bw = WD_SEITE_B;
        if (bh > 300) bh = 300;
        wd_bw_setzen(a, bw);
        wd_bh_setzen(a, bh);
        wd_griff_x = mx;
        wd_griff_y = my;
        wd_umbr_gueltig = 0;
        return;
    }
    if (wd_zieht == 0 || wd_win < 0) return;
    p = wd_pos_von_maus(wd_win, mx, my);
    if (p < wd_sel_von) { wd_sel_bis = wd_sel_von; wd_sel_von = p; }
    else wd_sel_bis = p;
    wd_pos = p;
}

void wd_loslassen() {
    wd_zieht = 0;
    wd_griff = 0;
    if (wd_sel_bis <= wd_sel_von) wd_auswahl_weg();
}

void wd_taste(int k) {
    int c; int code; int n; int z;
    c = keychar(k);
    code = keycode(k);

    if (wd_namemode) {
        n = strlen(wd_name);
        if (code == K_ENTER) {
            /* Mit dem Namen ist auch der Platz klar -- also gleich
               speichern. Frueher stand hier der Dateidialog des Kernels,
               der genau das tat; ohne ihn wartete der Benutzer auf etwas,
               das nie kam. Modus 2 heisst: der Name war zum Oeffnen. */
            wd_namemode2 = wd_namemode;
            wd_namemode = 0;
            wd_ort = 1;
            if (wd_namemode2 == 2) wd_laden();
            else if (wd_namemode2 == 3) wd_bild_einfuegen_name(wd_name);
            else wd_speichern();
            return;
        }
        if (code == K_ESC) { wd_namemode = 0; return; }
        if (code == K_BACKSPACE) { if (n > 0) wd_name[n - 1] = 0; return; }
        if (c >= 32 && c < 127 && n < 20) {
            wd_name[n] = toupper(c);
            wd_name[n + 1] = 0;
        }
        return;
    }
    if (wd_menue) { wd_menue = 0; return; }

    if (c == 3)  { wd_kopieren(); return; }                     /* Strg+C */
    if (c == 24) { wd_ausschneiden(); return; }                 /* Strg+X */
    if (c == 22) { wd_clip_einfuegen(); return; }               /* Strg+V */
    if (c == 1)  { wd_sel_von = 0; wd_sel_bis = wd_len; return; } /* Strg+A */

    if (code == K_BACKSPACE || code == K_DEL) {
        /* Ist ein Bild angeklickt, ist das Bild gemeint -- nicht das
           Zeichen davor. */
        if (wd_bild_sel >= 0) { wd_bild_loeschen(wd_bild_sel); return; }
        if (wd_sel_von >= 0 && wd_sel_bis > wd_sel_von) {
            wd_auswahl_loeschen();
            return;
        }
        if (code == K_BACKSPACE) wd_loeschen();
        return;
    }
    if (code == K_ENTER) {
        int vorher;
        wd_auswahl_weg();
        vorher = wd_liste(wd_absatz_bei(wd_pos));
        wd_einfuegen(10);
        wd_form_setzen(wd_form);
        /* In einer Liste bleibt man in der Liste -- der naechste Punkt
           kommt von selbst. */
        wd_liste_setzen(wd_absatz_bei(wd_pos), vorher);
        wd_umbr_gueltig = 0;
        return;
    }
    if (code == K_LEFT)  { wd_auswahl_weg(); if (wd_pos > 0) wd_pos--; return; }
    if (code == K_RIGHT) { wd_auswahl_weg(); if (wd_pos < wd_len) wd_pos++; return; }
    if (code == K_UP) {
        wd_auswahl_weg();
        z = wd_zeile_von_pos();
        if (z > 0) {
            n = wd_pos - wd_u_start(z);
            wd_pos = wd_u_start(z - 1) + n;
            if (wd_pos > wd_u_start(z - 1) + wd_u_laenge(z - 1))
                wd_pos = wd_u_start(z - 1) + wd_u_laenge(z - 1);
            if (z - 1 < wd_top) wd_top = z - 1;
        }
        return;
    }
    if (code == K_DOWN) {
        wd_auswahl_weg();
        z = wd_zeile_von_pos();
        if (z < wd_zeilen - 1) {
            n = wd_pos - wd_u_start(z);
            wd_pos = wd_u_start(z + 1) + n;
            if (wd_pos > wd_u_start(z + 1) + wd_u_laenge(z + 1))
                wd_pos = wd_u_start(z + 1) + wd_u_laenge(z + 1);
        }
        return;
    }
    if (code == K_HOME) { wd_pos = wd_u_start(wd_zeile_von_pos()); return; }
    if (code == K_END) {
        z = wd_zeile_von_pos();
        wd_pos = wd_u_start(z) + wd_u_laenge(z);
        return;
    }
    if (code == K_PGUP) {
        wd_top = wd_top - wd_passt(wd_top);
        if (wd_top < 0) wd_top = 0;
        wd_pos = wd_u_start(wd_top);
        return;
    }
    if (code == K_PGDN) {
        wd_top = wd_top + wd_passt(wd_top);
        wd_top_begrenzen();
        wd_pos = wd_u_start(wd_top);
        return;
    }
    if (c >= 32 && c < 127) {
        wd_auswahl_loeschen();
        wd_einfuegen(c);
    }
}

void wd_init() {
    int i;
    if (wd_name[0] == 0) strncpy(wd_name, "TEXT.TBW", 20);
    for (i = 0; i < 40; i++) {
        wd_formbyte_setzen(i, 1);
        wd_liste_setzen(i, 0);
        wd_bw_setzen(i, 200);
        wd_bh_setzen(i, 130);
    }
    wd_len = 0;
    wd_pos = 0;
    wd_top = 0;
    wd_form = 1;
    wd_stift = C_BLACK;
    wd_bild_sel = 0 - 1;
    wd_bild_geladen = 0 - 2;
    wd_auswahl_weg();
    wd_umbr_gueltig = 0;
}


/* ==========================================================================
   Hauptschleife
   ========================================================================== */

int main() {
    int e[4];
    int art; int laufen;

    wd_init();
    if (fenster_neu("Word", 600, 340) < 0) {
        print("Word braucht den Schreibtisch -- erst WIN eingeben.\n");
        return 1;
    }
    fenster_malziel();
    app_word(0);
    fenster_fertig();

    laufen = 1;
    while (laufen) {
        art = fenster_ereignis(e);

        if (art == FE_SCHLIESS) {
            laufen = 0;
        } else if (art == FE_TASTE) {
            clip_len = clip_holen();
            wd_taste((e[2] << 8) | e[1]);
            clip_setzen(clip_len);
            fenster_malziel();
            app_word(0);
            fenster_fertig();
        } else if (art == FE_KLICK) {
            clip_len = clip_holen();
            wd_klick(0, e[1], e[2]);
            clip_setzen(clip_len);
            fenster_malziel();
            app_word(0);
            fenster_fertig();
        } else if (art == FE_MALEN) {
            fenster_malziel();
            app_word(0);
            fenster_fertig();
        } else {
            if (wd_dialog_pruefen()) {   /* eine Datei gewaehlt -> neu malen */
                fenster_malziel();
                app_word(0);
                fenster_fertig();
            }
            /* Zieht gerade jemand einen Rahmen oder markiert Text? Dann die
               Maus selbst verfolgen -- Ereignisse kommen nur beim Druecken. */
            if (wd_zieht || wd_griff) {
                gx_maus_lesen();
                if (gx_btn & 1) wd_ziehen(gx_mx - fn_x, gx_my - fn_y);
                else wd_loslassen();
                fenster_malziel();
                app_word(0);
                fenster_fertig();
            } else {
                sleep(2);
            }
        }
    }

    fenster_zu();
    return 0;
}
