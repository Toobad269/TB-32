/* ==========================================================================
   CALC  --  Taschenrechner mit grafischer Oberflaeche fuer TOOBAD-OS

   Bedienung mit Maus oder Tastatur. ESC beendet.

   Der TB-32 hat keine Fliesskomma-Einheit -- es gibt nur ganze Zahlen.
   Ein Taschenrechner ohne Komma waere aber armselig, also rechnet dieses
   Programm in Tausendsteln: die Zahl 1.5 steht intern als 1500. Das nennt
   man Festkomma, und genau so haben es die ersten Taschenrechner auch
   gemacht, lange bevor es Fliesskomma in Hardware gab.

     Addieren/Subtrahieren:  einfach die Tausendstel addieren
     Multiplizieren:         a * b / 1000  (sonst waere das Ergebnis 1000x zu gross)
     Dividieren:             erst ganzzahlig teilen, dann den Rest verfeinern --
                             so laeuft nichts ueber, auch bei grossen Zahlen

   Uebersetzen auf dem Geraet selbst:   CC CALC.C
   ========================================================================== */

#include "proglib.c"
#include "gfxlib.c"

#define EINS      1000               /* so viele Tausendstel sind eine 1 */
#define MAXZIF    10                 /* so viele Zeichen darf man eingeben */

/* --- Aussehen ------------------------------------------------------------ */

#define GEH_X     190                /* Gehaeuse */
#define GEH_Y      30
#define GEH_W     260
#define GEH_H     330

#define LCD_X     204                /* Anzeigefeld */
#define LCD_Y      44
#define LCD_W     232
#define LCD_H      48

#define KN_X      199                /* erste Knopfspalte */
#define KN_Y      108
#define KN_W       56
#define KN_H       44
#define KN_DX      62
#define KN_DY      50

#define F_LCD     149                /* helles Gelbgruen  (3,4,1 im Farbwuerfel) */
#define F_ZIFFER   22                /* dunkles Gruen     (0,1,0) */
#define F_HINTER   17                /* dunkles Blau      (0,0,1) */
#define F_GEHAEUSE  8

/* --- Zustand ------------------------------------------------------------- */

char eingabe[20];                    /* was gerade getippt wird, als Text */
int  elen = 0;
int  akku = 0;                       /* die gemerkte Zahl, in Tausendsteln */
int  op = 0;                         /* wartender Operator, 0 = keiner */
int  neu = 1;                        /* 1 = die naechste Ziffer faengt neu an */
int  fehler = 0;
char anzeige[32];
char hilf[20];

/* ==========================================================================
   Zahlen: Text  <->  Festkomma
   ========================================================================== */

int text_wert() {
    int i; int v; int neg; int nk; int stellen;
    v = 0; neg = 0; nk = 0; stellen = 0;
    i = 0;
    if (eingabe[0] == '-') { neg = 1; i = 1; }
    while (eingabe[i]) {
        if (eingabe[i] == '.') {
            nk = 1;
        } else {
            if (nk == 0) {
                v = v * 10 + (eingabe[i] - '0');
            } else if (stellen < 3) {
                v = v * 10 + (eingabe[i] - '0');
                stellen++;
            }
        }
        i++;
    }
    if (nk) { while (stellen < 3) { v = v * 10; stellen++; } }
    else    { v = v * EINS; }
    if (neg) v = 0 - v;
    return v;
}

/* Festkommazahl als Text: Nachkommastellen nur, wenn es welche gibt. */
void wert_text(int v) {
    int ip; int fp; int n;
    anzeige[0] = 0;
    if (v < 0) { strcat(anzeige, "-"); v = 0 - v; }
    ip = v / EINS;
    fp = v - ip * EINS;
    itoa(ip, hilf);
    strcat(anzeige, hilf);
    if (fp) {
        hilf[0] = '0' + fp / 100;
        hilf[1] = '0' + (fp / 10) % 10;
        hilf[2] = '0' + fp % 10;
        hilf[3] = 0;
        n = 2;
        while (n > 0 && hilf[n] == '0') { hilf[n] = 0; n--; }
        strcat(anzeige, ".");
        strcat(anzeige, hilf);
    }
}

/* Was gerade auf der Anzeige stehen soll */
void anzeige_bauen() {
    if (fehler) { strcpy(anzeige, "ERROR"); return; }
    if (neu) { wert_text(akku); return; }
    strcpy(anzeige, eingabe);
}

/* ==========================================================================
   Rechnen
   ========================================================================== */

int betrag(int v) { if (v < 0) return 0 - v; return v; }

int mal_fix(int a, int b) {
    int aa; int bb;
    aa = betrag(a);
    bb = betrag(b);
    /* Passt a*b ueberhaupt in 32 Bit? Wenn nicht, lieber ehrlich ERROR
       zeigen als still eine falsche Zahl. */
    if (aa != 0 && bb > 2147483647 / aa) { fehler = 1; return 0; }
    return a * b / EINS;
}

int geteilt_fix(int a, int b) {
    int q; int r;
    if (b == 0) { fehler = 1; return 0; }
    q = a / b;
    r = a - q * b;
    if (q > 2000000 || q < 0 - 2000000) { fehler = 1; return 0; }
    if (betrag(b) > 2147483) return q * EINS;        /* Rest faellt weg */
    return q * EINS + r * EINS / b;
}

int rechne(int a, int b, int o) {
    if (o == '+') return a + b;
    if (o == '-') return a - b;
    if (o == '*') return mal_fix(a, b);
    if (o == '/') return geteilt_fix(a, b);
    return b;
}

/* ==========================================================================
   Eingabe verarbeiten
   ========================================================================== */

void loeschen() {
    eingabe[0] = 0;
    elen = 0;
    akku = 0;
    op = 0;
    neu = 1;
    fehler = 0;
}

void ziffer(int c) {
    int i;                       /* CC auf dem Geraet will alle Variablen
                                    am Anfang der Funktion sehen */
    if (fehler) loeschen();
    if (neu) { eingabe[0] = 0; elen = 0; neu = 0; }
    if (elen >= MAXZIF) return;
    if (c == '.') {
        /* nur ein Komma, und nie als erstes Zeichen ohne Null davor */
        for (i = 0; i < elen; i++) if (eingabe[i] == '.') return;
        if (elen == 0) { eingabe[elen] = '0'; elen++; }
    }
    eingabe[elen] = c;
    elen++;
    eingabe[elen] = 0;
}

void rueckwaerts() {
    if (fehler) { loeschen(); return; }
    if (neu) return;
    if (elen > 0) { elen--; eingabe[elen] = 0; }
    if (elen == 0 || (elen == 1 && eingabe[0] == '-')) {
        eingabe[0] = 0; elen = 0; neu = 1; akku = 0;
    }
}

void vorzeichen() {
    int i;
    if (fehler) return;
    if (neu) { akku = 0 - akku; return; }
    if (eingabe[0] == '-') {
        for (i = 0; i < elen; i++) eingabe[i] = eingabe[i + 1];
        elen--;
    } else {
        for (i = elen; i >= 0; i--) eingabe[i + 1] = eingabe[i];
        eingabe[0] = '-';
        elen++;
    }
}

/* Ein Operator schliesst die vorige Rechnung ab -- so verhaelt sich jeder
   echte Taschenrechner: 2 + 3 + zeigt schon 5 an. */
void operator(int o) {
    int b;
    if (fehler) return;
    b = akku;                    /* CC auf dem Geraet kennt kein ?: */
    if (neu == 0) b = text_wert();
    if (op) akku = rechne(akku, b, op);
    else    akku = b;
    op = o;
    neu = 1;
    elen = 0;
    eingabe[0] = 0;
}

void gleich() {
    int b;
    if (fehler) return;
    b = akku;
    if (neu == 0) b = text_wert();
    if (op) akku = rechne(akku, b, op);
    else    akku = b;
    op = 0;
    neu = 1;
    elen = 0;
    eingabe[0] = 0;
}

void prozent() {
    int b;
    if (fehler) return;
    b = akku;
    if (neu == 0) b = text_wert();
    akku = b / 100;
    op = 0;
    neu = 1;
    elen = 0;
    eingabe[0] = 0;
}

/* ==========================================================================
   Die Tastatur des Rechners
   ========================================================================== */

char* knopf_text(int i) {
    if (i ==  0) return "C";
    if (i ==  1) return "+/-";
    if (i ==  2) return "%";
    if (i ==  3) return "/";
    if (i ==  4) return "7";
    if (i ==  5) return "8";
    if (i ==  6) return "9";
    if (i ==  7) return "*";
    if (i ==  8) return "4";
    if (i ==  9) return "5";
    if (i == 10) return "6";
    if (i == 11) return "-";
    if (i == 12) return "1";
    if (i == 13) return "2";
    if (i == 14) return "3";
    if (i == 15) return "+";
    if (i == 16) return "0";
    if (i == 17) return ".";
    if (i == 18) return "<-";
    return "=";
}

int knopf_x(int i) { return KN_X + (i % 4) * KN_DX; }
int knopf_y(int i) { return KN_Y + (i / 4) * KN_DY; }

void knopf_tun(int i) {
    char* t;
    if (i == 0)  { loeschen(); return; }
    if (i == 1)  { vorzeichen(); return; }
    if (i == 2)  { prozent(); return; }
    if (i == 18) { rueckwaerts(); return; }
    if (i == 19) { gleich(); return; }
    t = knopf_text(i);
    if (t[0] == '/' || t[0] == '*' || t[0] == '-' || t[0] == '+') operator(t[0]);
    else ziffer(t[0]);
}

/* ==========================================================================
   Zeichnen
   ========================================================================== */

void zeichne_anzeige() {
    int b;
    anzeige_bauen();
    gx_fill(LCD_X + 2, LCD_Y + 2, LCD_W - 4, LCD_H - 4, F_LCD);
    /* rechtsbuendig, wie bei einem echten Rechner */
    b = strlen(anzeige) * 16;
    if (b > LCD_W - 16) {
        gx_text(LCD_X + 8, LCD_Y + 20, anzeige, F_ZIFFER);   /* zu lang: klein */
    } else {
        gx_text_gross(LCD_X + LCD_W - 8 - b, LCD_Y + 16, anzeige, F_ZIFFER, 2);
    }
    /* kleine Anzeige des wartenden Operators, oben links */
    if (op) gx_char(LCD_X + 6, LCD_Y + 6, op, F_ZIFFER, 256);
}

void zeichne_knopf(int i, int gedrueckt) {
    gx_panel(knopf_x(i), knopf_y(i), KN_W, KN_H, gedrueckt);
    gx_text_mitte(knopf_x(i) + gedrueckt, knopf_y(i) + (KN_H - 8) / 2 + gedrueckt,
                  KN_W, knopf_text(i), GX_SCHWARZ);
}

void zeichne_alles() {
    int i;
    gx_fill(0, 0, GX_BREIT, GX_HOCH, F_HINTER);
    gx_text(GEH_X, 12, "TOOBAD CALCULATOR", GX_WEISS);

    gx_panel(GEH_X, GEH_Y, GEH_W, GEH_H, 0);
    gx_panel(LCD_X, LCD_Y, LCD_W, LCD_H, 1);
    for (i = 0; i < 20; i++) zeichne_knopf(i, 0);
    zeichne_anzeige();

    gx_text(GEH_X - 40, 372, "Maus oder Tastatur   --   ESC beendet", GX_GRAU);
}

/* ==========================================================================
   Hauptschleife
   ========================================================================== */

int main() {
    int k; int c; int code; int i; int getroffen;

    loeschen();
    gx_start();
    zeichne_alles();

    while (1) {
        /* --- Tastatur --- */
        if (haskey()) {
            k = getkey();
            c = keychar(k);
            code = keycode(k);
            if (code == K_ESC) break;
            if (c >= '0' && c <= '9') ziffer(c);
            else if (c == '.' || c == ',') ziffer('.');
            else if (c == '+' || c == '-' || c == '*' || c == '/') operator(c);
            else if (c == '=' || code == K_ENTER) gleich();
            else if (c == '%') prozent();
            else if (c == 8) rueckwaerts();
            else if (c == 'c' || c == 'C') loeschen();
            zeichne_anzeige();
        }

        /* --- Maus --- */
        gx_maus_lesen();
        if (gx_klick()) {
            getroffen = 0 - 1;
            for (i = 0; i < 20; i++) {
                if (gx_treffer(knopf_x(i), knopf_y(i), KN_W, KN_H)) { getroffen = i; break; }
            }
            if (getroffen >= 0) {
                zeichne_knopf(getroffen, 1);      /* kurz gedrueckt zeigen */
                beep(880, 2);
                knopf_tun(getroffen);
                sleep(6);
                zeichne_knopf(getroffen, 0);
                zeichne_anzeige();
            }
        }

        sleep(1);            /* Rechenzeit abgeben, sonst frisst die Schleife alles */
    }

    gx_ende();
    return 0;
}
