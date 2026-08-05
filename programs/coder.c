/* ==========================================================================
   CODER  --  der Editor mit Farben und Compilerknopf, jetzt als Programm

   Frueher war er der groesste Brocken im Kernel: 780 Zeilen in gui.c, dazu
   die Farbgebung in coder.c und der Textkern aus edit.c. Jetzt liegt er als
   Datei auf der Platte.

   Zwei Dinge musste er dafuer lernen: den Compiler ueber einen Systemaufruf
   zu starten statt ihn einfach zu rufen, und seinen Fortschritt selbst
   anzuzeigen statt den Kernel darum zu bitten.

   Der Textkern (ed_*) ist eine KOPIE aus edit.c -- den braucht der
   Konsolen-Editor EDIT weiterhin, also bleibt er dort auch stehen. Zwei
   Kopien sind hier ehrlicher als eine Bibliothek, die beide binden muessten.

   Uebersetzen auf dem Geraet selbst:  CC CODER.C
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
#define C_WINBG    7

#define K_BACKSPACE 14
#define K_TAB       15
#define K_HOME      71
#define K_END       79
#define K_PGUP      73
#define K_PGDN      81
#define K_DEL       83
#define K_F2        60
#define K_F5        63
#define K_F3        61
#define K_F1        59

#define TITLE_H    16

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

int endet_auf(char* name, char* endung) {
    int n; int e; int i;
    n = strlen(name);
    e = strlen(endung);
    if (n < e) return 0;
    for (i = 0; i < e; i++)
        if (toupper(name[n - e + i]) != toupper(endung[i])) return 0;
    return 1;
}

/* --- Die Knopfleiste des Coders -----------------------------------------
   Position, Breite und Beschriftung stehen an EINER Stelle. Vorher standen
   die Koordinaten doppelt da -- einmal beim Zeichnen, einmal beim Klicken --
   und beim Verschieben eines Knopfes traf man daneben, ohne dass man es sah.

   Ganz rechts die beiden Knoepfe fuer Firmware. Sie bauen dieselbe Quelle,
   pruefen aber ein BIOS-Abbild daraus statt eines Programms.              */

#define CB_BACK   0
#define CB_NEW    1
#define CB_SAVE   2
#define CB_NAME   3
#define CB_BUILD  4
#define CB_RUN    5
#define CB_FIND   6
#define CB_SUCHE  7
#define CB_TEST   8
#define CB_FLASH  9
#define CB_GESAMT 10                 /* alle, auch die gerade versteckten */

/* Ist im Fenster gerade eine Firmware-Quelle offen?
   Erkannt an der Kennung im Kopf -- ein BIOS muss sie ohnehin haben, und
   so braucht es keine Betriebsart, die man vergessen kann umzuschalten. */
/* Gemerkt, nicht jedes Mal gesucht.
   Vorher lief diese Schleife bei JEDEM Neuzeichnen ueber 3000 Byte -- rund
   30.000 Befehle, und ein ganzes Bild hat bei 2 MHz nur etwa 33.000. Damit
   ass allein diese Suche die Bildzeit auf, und Tippen, Loeschen und Rollen
   ruckelten. Jetzt wird nur nachgesehen, wenn sich die Laenge geaendert
   hat, und nur im Kopf der Datei -- weiter hinten darf die Kennung ohnehin
   nicht stehen. */
int edg_bios_wert = 0;
int edg_bios_len = 0 - 1;   /* -1 = noch nie gesucht */

int edg_ist_bios() {
    char* t;
    int i; int n;
    /* Nur nachsehen, wenn sich am KOPF etwas geaendert haben kann. Beim
       Tippen weiter unten bleibt die Kennung, was sie war -- vorher wurde
       bei jedem Anschlag neu gesucht, weil sich ed_len aendert. Das waren
       25 Prozent der Rechenzeit des Coders. */
    if (ed_len == edg_bios_len) return edg_bios_wert;
    if (edg_bios_len >= 0 && ed_pos > 400) { edg_bios_len = ed_len; return edg_bios_wert; }
    edg_bios_len = ed_len;
    edg_bios_wert = 0;
    t = ed_text();
    n = ed_len;
    if (n > 400) n = 400;
    for (i = 0; i < n - 3; i++)
        if (t[i] == 'T' && t[i+1] == 'B' && t[i+2] == 'B' && t[i+3] == 'I') {
            edg_bios_wert = 1;
            return 1;
        }
    return 0;
}

/* Welche Knoepfe gehoeren zu welcher Art Quelltext?

   ART_PROG   C und Assembler -- uebersetzen und starten
   ART_PY     Python          -- nur starten, es wird nichts uebersetzt
   ART_BIOS   Firmware        -- weder noch, dafuer testen und flashen

   Build auf eine .PY zu lassen hiesse, den C-Compiler auf Python-Quelltext
   zu werfen; dabei kann nichts als eine Fehlerliste herauskommen. Und ein
   BIOS startet man nicht als Programm.                                   */

#define ART_PROG  0
#define ART_PY    1
#define ART_BIOS  2

int cb_w(int i) {
    if (i == CB_BACK)  return 50;
    if (i == CB_NEW)   return 38;
    if (i == CB_SAVE)  return 44;
    if (i == CB_NAME)  return 46;
    if (i == CB_BUILD) return 50;
    if (i == CB_RUN)   return 40;
    if (i == CB_FIND)  return 40;
    if (i == CB_SUCHE) return 100;
    if (i == CB_TEST)  return 52;
    return 56;
}

char* cb_text(int i) {
    if (i == CB_BACK)  return "< Back";
    if (i == CB_NEW)   return "New";
    if (i == CB_SAVE)  return "Save";
    if (i == CB_NAME)  return "Name";
    if (i == CB_BUILD) return "Build";
    if (i == CB_RUN)   return "Run";
    if (i == CB_FIND)  return "Find";
    if (i == CB_TEST)  return "Test";
    return "Flash";
}

int edg_art() {
    if (edg_ist_bios()) return ART_BIOS;
    if (endet_auf(edg_name, ".PY")) return ART_PY;
    return ART_PROG;
}

int cb_sichtbar(int art, int i) {
    if (i == CB_BUILD) return art == ART_PROG;
    if (i == CB_RUN)   return art != ART_BIOS;
    if (i == CB_TEST)  return art == ART_BIOS;
    if (i == CB_FLASH) return art == ART_BIOS;
    return 1;
}

/* Die Knoepfe ruecken zusammen, wenn einer fehlt -- sonst klafft mitten in
   der Leiste ein Loch. Zeichnen und Klicken fragen dieselbe Funktion, damit
   die beiden nicht auseinanderlaufen koennen. */
int cb_pos(int art, int ziel) {
    int i; int x;
    x = 0;
    for (i = 0; i < CB_GESAMT; i++) {
        if (cb_sichtbar(art, i) == 0) continue;
        if (i == ziel) return x;
        x = x + cb_w(i) + 4;
    }
    return 0 - 1;
}

/* Der Coder ist zum Programmieren da. Notizen und Texte schreibt man in
   Word -- deshalb gibt es hier nur noch die drei Sprachen. */
#define EDG_NEU_ANZ 4


/* Ausschneiden, Kopieren, Einfuegen -- die Zwischenablage liegt fest im
   Speicher, ihre Laenge holt und setzt ein Systemaufruf. */
void ed_kopieren() {
    int i; char* t;
    if (ed_sel_von < 0 || ed_sel_bis <= ed_sel_von) return;
    clip_len = ed_sel_bis - ed_sel_von;
    if (clip_len > CLIP_MAX) clip_len = CLIP_MAX;
    t = ed_text();
    for (i = 0; i < clip_len; i++)
        byte_put(CLIP_BUF + i, t[ed_sel_von + i]);
}

void ed_loesche_auswahl() {
    int i; int n;
    if (ed_sel_von < 0 || ed_sel_bis <= ed_sel_von) return;
    n = ed_sel_bis - ed_sel_von;
    ed_pos = ed_sel_bis;
    for (i = 0; i < n; i++) ed_backspace();
    ed_sel_von = 0 - 1;
    ed_sel_bis = 0 - 1;
}

void ed_einfuegen() {
    int i;
    if (clip_len <= 0) return;
    ed_loesche_auswahl();
    for (i = 0; i < clip_len; i++) ed_insert(byte_get(CLIP_BUF + i));
    edg_meldung = 0;
}


/* Die Dateiliste des Startschirms: der Kernel liefert Name, Art und Groesse
   ueber einen Systemaufruf. Frueher las der Coder das Verzeichnis selbst --
   das ging nur, weil er im Kernel stand. */
#define FT_DIR   2
#define FT_FILE  1
char eintrag_puffer[32];
int  eintrag_art = 0;

char* ent_name(int n) {
    if (ordner_eintrag(n, (int)eintrag_puffer) <= n) { eintrag_puffer[0] = 0; }
    eintrag_art = mem_get((int)eintrag_puffer + 16);
    return eintrag_puffer;
}
int  ent_type(int n) { ent_name(n); return eintrag_art; }
int  file_index(int zeile) { return zeile < ordner_eintrag(0, 0) ? zeile : 0 - 1; }
int  file_anzahl() { return ordner_eintrag(0, 0); }
int  fs_chdir(char* name) { return ordner_wechseln(name); }
void fs_path(char* aus) { ordner_pfad(aus); }

/* Der Textkern, kopiert aus edit.c */
#define ED_BUF     0x000D0000        /* bis zu 60 KB Text */
#define ED_MAX     60000
char ed_name[20];
int  ed_len = 0;                     /* Laenge des Textes */
int  ed_pos = 0;                     /* Cursorstelle im Text */
int  ed_top = 0;                     /* erste angezeigte Zeile */
int  ed_dirty = 0;                   /* ungespeicherte Aenderungen? */

char* ed_text() { return (char*)ED_BUF; }

/* Anfang der Zeile, in der Stelle p liegt */
int ed_line_start(int p) {
    char* t;
    t = ed_text();
    while (p > 0 && t[p - 1] != 10) p--;
    return p;
}

int ed_line_end(int p) {
    char* t;
    t = ed_text();
    while (p < ed_len && t[p] != 10) p++;
    return p;
}

/* Wievielte Zeile ist Stelle p? */
int ed_line_of(int p) {
    char* t; int i; int n;
    t = ed_text();
    n = 0;
    for (i = 0; i < p; i++) if (t[i] == 10) n++;
    return n;
}

int ed_col_of(int p) { return p - ed_line_start(p); }

/* Anfang der Zeile Nummer n */
int ed_start_of_line(int n) {
    char* t; int i; int cur;
    t = ed_text();
    if (n == 0) return 0;
    cur = 0;
    for (i = 0; i < ed_len; i++) {
        if (t[i] == 10) {
            cur++;
            if (cur == n) return i + 1;
        }
    }
    return ed_len;
}

void ed_insert(int c) {
    char* t; int i;
    if (ed_len >= ED_MAX - 1) return;
    t = ed_text();
    for (i = ed_len; i > ed_pos; i--) t[i] = t[i - 1];
    t[ed_pos] = c;
    ed_len++;
    ed_pos++;
    ed_dirty = 1;
}

void ed_backspace() {
    char* t; int i;
    if (ed_pos == 0) return;
    t = ed_text();
    for (i = ed_pos - 1; i < ed_len - 1; i++) t[i] = t[i + 1];
    ed_len--;
    ed_pos--;
    ed_dirty = 1;
}

void ed_delete() {
    char* t; int i;
    if (ed_pos >= ed_len) return;
    t = ed_text();
    for (i = ed_pos; i < ed_len - 1; i++) t[i] = t[i + 1];
    ed_len--;
    ed_dirty = 1;
}


/* Die Farbgebung */
/* ==========================================================================
   CODER -- was den Editor zu einem Werkzeug fuer Programme macht

   Farben, Zeilennummern, Suchen, Einruecken. Der Editor selbst (in gui.c)
   kuemmert sich weiter um Text, Cursor und Dateien -- hier steht nur, WIE
   der Text aussieht.

   Zur Faerbung: Fuer jedes sichtbare Zeichen wird einmal je Bild eine Farbe
   ausgerechnet und in einen kleinen Puffer geschrieben. Das Malen liest sie
   dann nur noch ab. So bleibt die Zeichenschleife in gui.c einfach, und die
   ganze Zustandsmaschine steht an einer Stelle.

   Der Haken bei Blockkommentaren: ob Zeile 200 in einem Kommentar steht,
   haengt vom ganzen Text davor ab. Diesen Zustand einmal je Bild neu
   auszurechnen waere bei einer 50-KB-Datei zu teuer. Er wird deshalb nur
   dann neu bestimmt, wenn sich der Anfang des Sichtbereichs aendert -- beim
   Tippen also nie.
   ========================================================================== */

#define SYN_BUF     0x00700000       /* eine Farbe je sichtbarem Zeichen */
#define SYN_MAX     8000

/* Farben (auf weissem Grund gedacht) */
#define SF_NORMAL   C_BLACK
#define SF_WORT     1                /* Schluesselwoerter: blau */
#define SF_TEXT     2                /* Zeichenketten: gruen */
#define SF_KOMM     8                /* Kommentare: grau */
#define SF_ZAHL     5                /* Zahlen: magenta */
#define SF_RAUTE    6                /* Praeprozessor: braun */

/* Zustaende der Maschine */
#define SZ_NORMAL   0
#define SZ_BLOCK    1                /* in einem Blockkommentar */
#define SZ_TEXT     2                /* in einer Zeichenkette */
#define SZ_ZEICHEN  3                /* in einer Zeichenkonstante */

int syn_art = 0;                     /* 0 = aus, 1 = C, 2 = Assembler, 3 = Python */
int syn_top = 0 - 1;                 /* fuer welche erste Zeile syn_zustand gilt */
int syn_fertig_ab = 0 - 1;           /* fuer diesen Anfang steht der Farbpuffer */
int syn_fertig_len = 0 - 1;          /* ... bei dieser Textlaenge */
int syn_zustand = SZ_NORMAL;
char syn_wort[24];

int syn_wortzeichen(int c) {
    if (c >= 'a' && c <= 'z') return 1;
    if (c >= 'A' && c <= 'Z') return 1;
    if (c >= '0' && c <= '9') return 1;
    if (c == '_') return 1;
    return 0;
}

/* Vorfilter: erst der Anfangsbuchstabe, dann erst vergleichen. Ohne ihn
   liefe fuer JEDES Wort im Text die ganze Liste durch -- und das 1600-mal
   je Bild. */
int syn_ist_schluessel(char* w) {
    int c;
    c = w[0];
    if (syn_art == 1) {
        if (c == 'i') return strcmp(w, "int") == 0 || strcmp(w, "if") == 0;
        if (c == 'c') return strcmp(w, "char") == 0 || strcmp(w, "continue") == 0;
        if (c == 'v') return strcmp(w, "void") == 0;
        if (c == 'u') return strcmp(w, "unsigned") == 0;
        if (c == 'e') return strcmp(w, "else") == 0;
        if (c == 'w') return strcmp(w, "while") == 0;
        if (c == 'f') return strcmp(w, "for") == 0;
        if (c == 'r') return strcmp(w, "return") == 0;
        if (c == 'b') return strcmp(w, "break") == 0;
        if (c == 'a') return strcmp(w, "asm") == 0;
        if (c == 's') return strcmp(w, "sizeof") == 0;
        return 0;
    }
    if (syn_art == 2) {
        if (c == 'm') return strcmp(w, "mov") == 0 || strcmp(w, "movi") == 0;
        if (c == 'a') return strcmp(w, "add") == 0;
        if (c == 's') return strcmp(w, "sub") == 0;
        if (c == 'c') return strcmp(w, "cmp") == 0 || strcmp(w, "call") == 0;
        if (c == 'j') return strcmp(w, "jmp") == 0;
        if (c == 'r') return strcmp(w, "ret") == 0;
        if (c == 'p') return strcmp(w, "push") == 0 || strcmp(w, "pop") == 0;
        if (c == 'i') return strcmp(w, "int") == 0 || strcmp(w, "in") == 0;
        if (c == 'h') return strcmp(w, "hlt") == 0;
        if (c == 'o') return strcmp(w, "out") == 0;
        return 0;
    }
    if (syn_art == 3) {
        if (c == 'd') return strcmp(w, "def") == 0;
        if (c == 'c') return strcmp(w, "class") == 0;
        if (c == 'i') return strcmp(w, "if") == 0 || strcmp(w, "in") == 0
                          || strcmp(w, "import") == 0;
        if (c == 'e') return strcmp(w, "else") == 0 || strcmp(w, "elif") == 0;
        if (c == 'w') return strcmp(w, "while") == 0;
        if (c == 'f') return strcmp(w, "for") == 0 || strcmp(w, "False") == 0;
        if (c == 'r') return strcmp(w, "return") == 0;
        if (c == 'p') return strcmp(w, "print") == 0;
        if (c == 'T') return strcmp(w, "True") == 0;
        if (c == 'N') return strcmp(w, "None") == 0;
        return 0;
    }
    return 0;
}

/* Welche Sprache? Nach der Endung des Dateinamens. */
void syn_sprache(char* name) {
    syn_art = 0;
    if (endet_auf(name, ".C")) syn_art = 1;
    if (endet_auf(name, ".H")) syn_art = 1;
    if (endet_auf(name, ".ASM")) syn_art = 2;
    if (endet_auf(name, ".PY")) syn_art = 3;
    syn_top = 0 - 1;                 /* Zwischenspeicher verwerfen */
    syn_fertig_ab = 0 - 1;
}

/* Laeuft von vorne bis zur Stelle und meldet, ob dort ein Blockkommentar
   oder eine Zeichenkette offen ist. Nur fuer C noetig. */
/* Wie syn_zustand_bei, aber ab einer bekannten Stelle mit bekanntem
   Zustand. Genau dieselbe Rechnung, nur der Anfang ist schon erledigt. */
int syn_zustand_ab(int von, int z, int ende) {
    char* t; int p;
    if (syn_art != 1) return SZ_NORMAL;
    t = ed_text();
    p = von;
    while (p < ende) {
        if (z == SZ_BLOCK) {
            if (t[p] == '*' && p + 1 < ende && t[p + 1] == '/') { z = SZ_NORMAL; p++; }
        } else if (z == SZ_TEXT) {
            if (t[p] == 92) p++;
            else if (t[p] == 34) z = SZ_NORMAL;
            else if (t[p] == 10) z = SZ_NORMAL;
        } else {
            if (t[p] == '/' && p + 1 < ende && t[p + 1] == '*') { z = SZ_BLOCK; p++; }
            else if (t[p] == '/' && p + 1 < ende && t[p + 1] == '/') {
                while (p < ende && t[p] != 10) p++;
            }
            else if (t[p] == 34) z = SZ_TEXT;
        }
        p++;
    }
    return z;
}

int syn_zustand_bei(int ende) {
    char* t; int p; int z;
    if (syn_art != 1) return SZ_NORMAL;
    t = ed_text();
    z = SZ_NORMAL;
    p = 0;
    while (p < ende) {
        if (z == SZ_BLOCK) {
            if (t[p] == '*' && p + 1 < ende && t[p + 1] == '/') { z = SZ_NORMAL; p++; }
        } else if (z == SZ_TEXT) {
            if (t[p] == 92) p++;
            else if (t[p] == 34) z = SZ_NORMAL;
            else if (t[p] == 10) z = SZ_NORMAL;      /* offene Zeichenkette endet */
        } else {
            if (t[p] == '/' && p + 1 < ende && t[p + 1] == '*') { z = SZ_BLOCK; p++; }
            else if (t[p] == '/' && p + 1 < ende && t[p + 1] == '/') {
                while (p < ende && t[p] != 10) p++;   /* Zeilenkommentar */
            }
            else if (t[p] == 34) z = SZ_TEXT;
        }
        p++;
    }
    return z;
}

/* Faerbt den sichtbaren Bereich ein. start = Textstelle der ersten Zeile. */
void syn_bauen(int start, int zeilen, int spalten) {
    char* t; char* sb; int p; int z; int s; int c; int f; int i; int q; int zust;
    int wortfarbe; int laenge;

    if (syn_art == 0) return;
    if (spalten > 200) spalten = 200;
    if (zeilen * spalten > SYN_MAX) zeilen = SYN_MAX / spalten;

    /* Farben haengen nur am Text und am Sichtbereich -- nicht am Cursor.
       Das Meiste, was ein Neuzeichnen ausloest (Mausbewegung, Pfeiltasten,
       Klicks), aendert daran nichts. */
    if (syn_fertig_ab == start && syn_fertig_len == ed_len) return;
    syn_fertig_ab = start;
    syn_fertig_len = ed_len;

    t = ed_text();
    sb = (char*)SYN_BUF;
    if (syn_top != start) {                 /* nur beim Blaettern neu rechnen */
        /* Vorwaerts weiterrechnen statt jedes Mal bei null anzufangen.
           syn_zustand_bei() laeuft vom Dateianfang bis zur Stelle -- beim
           Rollen in einer langen Datei war das die ganze Datei, bei JEDEM
           Schritt. Der Zustand ist ein Automat, der nur vorwaerts laeuft:
           von einer bekannten Stelle aus weiterzuzaehlen liefert genau
           dasselbe Ergebnis, kostet aber nur die Strecke dazwischen. */
        if (start > syn_top && syn_top >= 0)
            syn_zustand = syn_zustand_ab(syn_top, syn_zustand, start);
        else
            syn_zustand = syn_zustand_bei(start);
        syn_top = start;
    }
    zust = syn_zustand;
    p = start;

    for (z = 0; z < zeilen; z++) {
        s = 0;
        if (zust == SZ_TEXT) zust = SZ_NORMAL;      /* Zeichenkette endet am Zeilenende */
        while (p < ed_len && t[p] != 10) {
            c = t[p];
            f = SF_NORMAL;

            if (zust == SZ_BLOCK) {
                f = SF_KOMM;
                if (c == '*' && p + 1 < ed_len && t[p + 1] == '/') {
                    if (s < spalten) sb[z * spalten + s] = f;
                    s++; p++;
                    c = t[p];                        /* der Schraegstrich noch grau */
                    zust = SZ_NORMAL;
                }
            } else if (zust == SZ_TEXT) {
                f = SF_TEXT;
                if (c == 92 && p + 1 < ed_len) {     /* Fluchtzeichen */
                    if (s < spalten) sb[z * spalten + s] = f;
                    s++; p++;
                    c = t[p];
                } else if (c == 34) {
                    zust = SZ_NORMAL;
                }
            } else {
                /* Zeilenkommentar: Rest der Zeile grau */
                if ((syn_art == 1 && c == '/' && p + 1 < ed_len && t[p + 1] == '/')
                    || (syn_art == 2 && c == ';')
                    || (syn_art == 3 && c == '#')) {
                    while (p < ed_len && t[p] != 10) {
                        if (s < spalten) sb[z * spalten + s] = SF_KOMM;
                        s++; p++;
                    }
                    break;
                }
                if (syn_art == 1 && c == '/' && p + 1 < ed_len && t[p + 1] == '*') {
                    zust = SZ_BLOCK;
                    f = SF_KOMM;
                } else if (c == 34) {
                    zust = SZ_TEXT;
                    f = SF_TEXT;
                } else if (syn_art == 1 && c == '#' && s == 0) {
                    while (p < ed_len && t[p] != 10) {
                        if (s < spalten) sb[z * spalten + s] = SF_RAUTE;
                        s++; p++;
                    }
                    break;
                } else if (c >= '0' && c <= '9') {
                    f = SF_ZAHL;
                } else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
                           || c == '_') {
                    /* Wort einsammeln und in einem Rutsch faerben */
                    i = 0;
                    q = p;
                    while (q < ed_len && syn_wortzeichen(t[q])) {
                        if (i < 22) { syn_wort[i] = t[q]; i++; }
                        q++;
                    }
                    syn_wort[i] = 0;
                    wortfarbe = SF_NORMAL;
                    if (syn_ist_schluessel(syn_wort)) wortfarbe = SF_WORT;
                    laenge = q - p;
                    for (i = 0; i < laenge; i++) {
                        if (s < spalten) sb[z * spalten + s] = wortfarbe;
                        s++;
                    }
                    p = q;
                    continue;
                }
            }

            if (s < spalten) sb[z * spalten + s] = f;
            s++;
            p++;
        }
        while (s < spalten) { sb[z * spalten + s] = SF_NORMAL; s++; }
        if (p < ed_len) p++;
    }
}

int syn_farbe(int zeile, int spalte, int spalten) {
    char* sb;
    if (syn_art == 0) return C_BLACK;
    if (spalte >= spalten) return C_BLACK;
    sb = (char*)SYN_BUF;
    return sb[zeile * spalten + spalte] & 255;
}

/* --- Suchen -------------------------------------------------------------- */

char cod_suche[32];
int  cod_suchmode = 0;               /* Suchtext wird gerade getippt */
int  cod_gefunden = 0;               /* 1 = gefunden, 2 = nichts mehr da */

/* Sucht ab der Stelle nach dem Cursor, faengt am Ende wieder vorne an. */
void cod_finden() {
    char* t; int n; int i; int j; int start; int runde;
    n = strlen(cod_suche);
    if (n == 0) return;
    t = ed_text();
    start = ed_pos + 1;
    if (start > ed_len) start = 0;
    runde = 0;
    i = start;
    while (runde < 2) {
        if (i + n > ed_len) { i = 0; runde++; continue; }
        j = 0;
        while (j < n && toupper(t[i + j]) == toupper(cod_suche[j])) j++;
        if (j == n) {
            ed_pos = i;
            ed_sel_von = i;
            ed_sel_bis = i + n;
            edg_folgen = 1;
            cod_gefunden = 1;
            return;
        }
        i++;
        if (i >= ed_len) { i = 0; runde++; }
    }
    cod_gefunden = 2;
}

/* --- Einruecken ---------------------------------------------------------- */

/* Nach der Eingabetaste die Einrueckung der vorigen Zeile uebernehmen, und
   nach einer offenen geschweiften Klammer zwei Stellen mehr. So bleibt der
   Text von selbst in Form. */
void cod_umbruch() {
    char* t; int anfang; int i; int n; int letzte;
    t = ed_text();
    anfang = ed_line_start(ed_pos);
    n = 0;
    i = anfang;
    while (i < ed_pos && (t[i] == 32 || t[i] == 9)) { n++; i++; }
    letzte = 0;
    i = ed_pos - 1;
    while (i >= anfang) {
        if (t[i] != 32 && t[i] != 9) { letzte = t[i]; i = 0 - 1; }
        else i--;
    }
    ed_insert(10);
    if (letzte == '{') n = n + 2;
    for (i = 0; i < n; i++) ed_insert(32);
}

/* --- Sprung zur Fehlerzeile ---------------------------------------------- */

/* Sucht in der mitgeschriebenen Compilerausgabe die erste Zeilennummer und
   springt dorthin. Die Meldungen haben die Form "  datei.c line 42: ..." */
int cod_fehlerzeile() {
    int z; int i; int a; int c; int n; int gefunden;
    for (z = 0; z < mitschrift_zeilen(); z++) {
        a = mitschrift_zeile(z);
        i = 0;
        while (i < 60) {
            c = byte_get(a + i);
            if (c == 0) break;
            if (c == 'l' && byte_get(a + i + 1) == 'i' && byte_get(a + i + 2) == 'n'
                && byte_get(a + i + 3) == 'e' && byte_get(a + i + 4) == 32) {
                i = i + 5;
                n = 0;
                gefunden = 0;
                while (byte_get(a + i) >= '0' && byte_get(a + i) <= '9') {
                    n = n * 10 + byte_get(a + i) - '0';
                    i++;
                    gefunden = 1;
                }
                if (gefunden && n > 0) return n;
            }
            i++;
        }
    }
    return 0;
}

void cod_zur_fehlerzeile() {
    int n;
    n = cod_fehlerzeile();
    if (n <= 0) return;
    ed_pos = ed_start_of_line(n - 1);
    ed_sel_von = ed_pos;
    ed_sel_bis = ed_line_end(ed_pos);
    edg_folgen = 1;
}


int fw_lese[6];
int fw_schreib[6];
int fw_frisch[6];                 /* Programm hat neu gemalt */

#define EDG_COLS    edg_cols
#define EDG_ROWS    edg_rows

/* Fensterfelder gehoeren dem Schreibtisch -- hier zaehlt nur die
   Groesse des eigenen Puffers. */
int gui_running;
/* Ein Programm hat den Bildschirmmodus umgeschaltet und braucht die ganze
   Flaeche: 0 = alles normal, 1 = der Bildschirm gehoert dem Programm,
   2 = das Programm ist fertig, die Oberflaeche darf sich zurueckholen.
   gui_selbst schuetzt die eigenen Moduswechsel davor, sich selbst zu melden. */
int gui_fremd = 0;
int gui_selbst = 0;

char gui_pfad[40];
char edg_name[20];               /* Dateiname im Editorfenster */
int  edg_top = 0;                /* erste sichtbare Zeile */
int  edg_namemode = 0;           /* 1 = der Dateiname wird gerade getippt */
/* Worauf wir warten, waehrend der Dateidialog offen ist:
   1 = neue Datei, 2 = speichern unter, 3 = oeffnen. */
int  edg_warte = 0;
int  edg_meldung = 0;            /* 0 keine, 1 gespeichert, 2 uebersetzt, 3 Fehler */
int  edg_build = 0;              /* laeuft gerade ein Uebersetzungslauf? */
int  edg_ort = 0;                /* 1 = Name und Ordner stehen fest */
int  edg_pid = 0 - 1;
int  edg_run_danach = 0;         /* nach dem Uebersetzen gleich starten? */
char edg_ziel[20];
int  menu_offen = 0;            /* Startmenue aufgeklappt? */
/* Welche Maustaste hat den letzten Klick ausgeloest? Bit 0 links, Bit 1
   Mitte, Bit 2 rechts -- so liefert es die Maus. Der Schreibtisch hat das
   bisher weggeworfen und jeden Klick gleich behandelt. */
int  gui_taste = 1;
/* Vollbild: alte Masse merken, damit man zurueckschalten kann */
int  groesse_zieht = 0 - 1;     /* Fenster, dessen Ecke gerade gezogen wird */

/* Zwischenablage des Systems */
#define CLIP_BUF  0x00130000
#define CLIP_MAX  8192
int  clip_len = 0;
int  ed_sel_von = 0 - 1;        /* Auswahl im Editor */
int  ed_sel_bis = 0 - 1;
int  edg_zieht = 0;             /* Maus zieht gerade eine Auswahl auf */
/* Wie viel Text ins Fenster passt -- haengt von der Fenstergroesse ab und
   wird vor jedem Zeichnen neu ausgerechnet. */
int  edg_cols = 70;
int  edg_rows = 24;
/* Der Editor hat zwei Ansichten: 0 = Auswahlbildschirm ("Was willst du
   machen?"), 1 = der Text selbst. */
int  edg_screen = 0;
int  edg_liste_top = 0;
int  es_x; int  es_y; int  es_w; int  es_h;
int  es_lw; int  es_py; int  es_fy;
int  es_rx; int  es_rw; int  es_rows;
/* 1 = der Ausschnitt folgt der Schreibmarke. Beim Blaettern mit dem Rad wird
   das abgeschaltet, sonst spraenge die Ansicht sofort wieder zurueck. */
int  edg_folgen = 1;

#define EDG_GUTTER 40                /* Spalte fuer die Zeilennummern */

void edg_masse(int w) {
    edg_cols = (fn_breite - 14 - EDG_GUTTER) / 8;
    /* Titelleiste, Statuszeile, Knopfleiste -- und unten Rand, damit die
       Knoepfe nicht am Fensterrand kleben. */
    edg_rows = (fn_hoehe - 54) / 9;
    if (edg_cols < 10) edg_cols = 10;
    if (edg_rows < 3) edg_rows = 3;
}
int  zieh_dx = 0;
int  zieh_dy = 0;
int  klick_zeit = 0;            /* fuer den Doppelklick */
int  klick_was = 0 - 1;

/* Die Knopfleiste der Dateiverwaltung an einer Stelle beschrieben --
   Zeichnen und Klicken lesen dieselbe Tabelle, dann koennen sie nicht
   auseinanderlaufen. */
/* Nur noch vier Knoepfe, nach Haeufigkeit sortiert. Oeffnen und Starten
   macht der Doppelklick -- dafuer braucht es keinen Knopf mehr. */

void edg_oeffnen(char* name) {
    int n;
    memset(edg_name, 0, 20);
    strncpy(edg_name, name, 18);
    syn_sprache(edg_name);
    n = fileread(name, ED_BUF, ED_MAX);
    if (n < 0) n = 0;                            /* neue Datei */
    ed_len = n;
    ed_pos = 0;
    edg_top = 0;
    edg_meldung = 0;
    edg_namemode = 0;
}

/* ==========================================================================
   Startbildschirm des Editors

   Bevor man in den Text springt, wird gefragt, was man ueberhaupt vorhat:
   etwas Neues schreiben (und in welcher Sprache) oder etwas Vorhandenes
   oeffnen. Gleichzeitig sieht man, in welchem Ordner das Ganze landet --
   das ist derselbe aktuelle Ordner, mit dem auch die Dateiverwaltung und
   die Kommandozeile arbeiten.
   ========================================================================== */

/* --- Die Knopfleiste des Coders -----------------------------------------
   Position, Breite und Beschriftung stehen an EINER Stelle. Vorher standen
   die Koordinaten doppelt da -- einmal beim Zeichnen, einmal beim Klicken --
   und beim Verschieben eines Knopfes traf man daneben, ohne dass man es sah.

   Ganz rechts die beiden Knoepfe fuer Firmware. Sie bauen dieselbe Quelle,
   pruefen aber ein BIOS-Abbild daraus statt eines Programms.              */

#define CB_BACK   0
#define CB_NEW    1
#define CB_SAVE   2
#define CB_NAME   3
#define CB_BUILD  4
#define CB_RUN    5
#define CB_FIND   6
#define CB_SUCHE  7
#define CB_TEST   8
#define CB_FLASH  9
#define CB_GESAMT 10                 /* alle, auch die gerade versteckten */

/* Ist im Fenster gerade eine Firmware-Quelle offen?
   Erkannt an der Kennung im Kopf -- ein BIOS muss sie ohnehin haben, und
   so braucht es keine Betriebsart, die man vergessen kann umzuschalten. */
/* Gemerkt, nicht jedes Mal gesucht.
   Vorher lief diese Schleife bei JEDEM Neuzeichnen ueber 3000 Byte -- rund
   30.000 Befehle, und ein ganzes Bild hat bei 2 MHz nur etwa 33.000. Damit
   ass allein diese Suche die Bildzeit auf, und Tippen, Loeschen und Rollen
   ruckelten. Jetzt wird nur nachgesehen, wenn sich die Laenge geaendert
   hat, und nur im Kopf der Datei -- weiter hinten darf die Kennung ohnehin
   nicht stehen. */


/* Welche Knoepfe gehoeren zu welcher Art Quelltext?

   ART_PROG   C und Assembler -- uebersetzen und starten
   ART_PY     Python          -- nur starten, es wird nichts uebersetzt
   ART_BIOS   Firmware        -- weder noch, dafuer testen und flashen

   Build auf eine .PY zu lassen hiesse, den C-Compiler auf Python-Quelltext
   zu werfen; dabei kann nichts als eine Fehlerliste herauskommen. Und ein
   BIOS startet man nicht als Programm.                                   */

#define ART_PROG  0
#define ART_PY    1
#define ART_BIOS  2


char* edg_neu_name(int i) {
    if (i == 1) return "NEW.ASM";
    if (i == 2) return "NEW.PY";
    if (i == 3) return "MYBIOS.ASM";
    return "NEW.C";
}

char* edg_neu_text(int i) {
    if (i == 1) return "Assembler        .ASM";
    if (i == 2) return "Python script    .PY";
    if (i == 3) return "BIOS             .ASM";
    return "C program        .C";
}

/* Eine kleine Vorlage, damit man nicht vor einer leeren Seite sitzt. */

char* edg_vorlage(int i) {
    if (i == 0) return "
int main() {\n    print(\"Hello from TOOBAD-OS\\n\");\n    getkey();\n    return 0;\n}\n";
    if (i == 1) return "; TB-32 assembler\nstart:\n    li r1, text\n    movi r0, 1\n    int 0x10\n    hlt\ntext:\n    .db \"Hello\", 0\n";
    if (i == 2) return "print(\"Hello from TOOBAD-OS\")\n";
    /* Die BIOS-Vorlage startet sofort -- sie laedt den Bootsektor und
       springt hinein. Alles Weitere baut man drumherum. Was ein BIOS
       liefern muss, sagt der ?-Knopf. */
    if (i == 3) return ".include \"const.inc\"\n.org ROM_BASE\n\nentry:\n    jmp startup                   ; 0x00\n    .db \"TBBI\"                    ; 0x04 signature\n    .dw 0                         ; 0x08 length\n    .dw 0                         ; 0x0C checksum\n    .db \"MY BIOS\", 0              ; 0x10 name on the splash screen\n    .space 24\n\nstartup:                          ; 0x30\n    li sp, BIOS_STACK\n    cli\n    li r10, BDA_BASE              ; clear the BIOS data area\n    li r11, 256\n    movi r12, 0\n.clear:\n    stw [r10], r12\n    addi r10, r10, 4\n    subi r11, r11, 1\n    cmpi r11, 0\n    jnz .clear\n    movi r10, ATTR_NORMAL\n    stwa BDA_ATTR, r10\n\n    movi r10, 100                 ; 100 timer ticks per second\n    out P_TIMER_HZ, r10\n    sti\n\n    movi r1, 0                    ; read the boot sector\n    movi r2, 1\n    li r3, BOOT_ADDR\n    out P_DISK_LBA, r1\n    out P_DISK_COUNT, r2\n    out P_DISK_ADDR, r3\n    movi r10, 1\n    out P_DISK_CMD, r10\n    in r0, P_DISK_STATUS\n    cmpi r0, 0\n    jnz .stop\n\n    li r10, BOOT_ADDR             ; ... and jump into it\n    jmpr r10\n.stop:\n    hlt\n    jmp .stop\n";
    return "
int main() {\n    print(\"Hello from TOOBAD-OS\\n\");\n    getkey();\n    return 0;\n}\n";
}

int edg_neu_wahl = 0;            /* welche Vorlage gerade angelegt wird */

void edg_neu(int i) {
    char* v; char* t;
    memset(edg_name, 0, 20);
    strncpy(edg_name, edg_neu_name(i), 18);
    syn_sprache(edg_name);
    v = edg_vorlage(i);
    t = ed_text();
    ed_len = 0;
    while (*v) { t[ed_len] = *v; ed_len++; v++; }
    ed_pos = ed_len;
    edg_top = 0;
    edg_meldung = 0;
    edg_namemode = 0;
    ed_sel_von = 0 - 1;
    ed_sel_bis = 0 - 1;
    edg_screen = 1;
}

/* "New" fragt erst nach dem Platz. Das Schreibfenster geht ueberhaupt nur
   auf, wenn einer gewaehlt wurde -- bricht man ab, bleibt die Startseite
   stehen und es entsteht keine halbe Datei. */

/* Neue Datei: erst den Platz aussuchen, dann entsteht sie. Der Dateidialog
   gehoert dem Kernel und steht jedem Programm offen. */
void edg_neu_starten(int i) {
    edg_neu_wahl = i;
    edg_ort = 0;
    edg_warte = 1;
    datei_dialog(DLG_SPEICHERN, "", edg_neu_name(i));
}

void edg_neu_anlegen() {
    edg_neu(edg_neu_wahl);
}

void edg_start_masse(int w) {
    es_x = 4;
    es_y = 4;
    es_w = fn_breite - 8;
    es_h = fn_hoehe - 8;
    es_lw = 210;
    if (es_lw > es_w / 2) es_lw = es_w / 2;
    es_py = es_y + 30;
    es_fy = es_y + es_h - 34;
    es_rx = es_x + es_lw + 16;
    es_rw = es_x + es_w - 8 - es_rx;
    if (es_rw < 40) es_rw = 40;
    es_rows = (es_fy - 22 - (es_py + 16)) / 12;
    if (es_rows < 1) es_rows = 1;
    if (es_rows > 26) es_rows = 26;
}

void edg_startscreen(int w) {
    int i; int idx; int y; int farbe;

    edg_start_masse(w);
    gx_fill(es_x, es_y, es_w, es_h, C_WIN);
    gx_text(es_x + 8, es_y + 8, "What do you want to do?", C_TEXT, 256);

    /* --- links: etwas Neues --- */
    gx_text(es_x + 8, es_py, "Create new file", C_ACCENT, 256);
    for (i = 0; i < EDG_NEU_ANZ; i++)
        p_knopf(es_x + 8, es_py + 16 + i * 32, es_lw - 16, 26, edg_neu_text(i), 0);

    /* --- rechts: etwas Vorhandenes --- */
    gx_text(es_rx, es_py, "Open file or folder", C_ACCENT, 256);
    /* Der Weg einen Ordner hoeher gehoert direkt an die Liste, wie in der
       Dateiverwaltung -- unten in der Ecke findet ihn niemand. */
    p_knopf(es_rx + es_rw - 48, es_py - 4, 48, 16, "Up", 0);
    gx_fill(es_rx, es_py + 16, es_rw, es_rows * 12 + 4, C_WHITE);
    gx_frame(es_rx, es_py + 16, es_rw, es_rows * 12 + 4, C_BLACK);
    for (i = 0; i < es_rows; i++) {
        idx = file_index(edg_liste_top + i);
        if (idx < 0) break;
        y = es_py + 20 + i * 12;
        farbe = C_TEXT;
        if (ent_type(idx) == FT_DIR) farbe = C_ACCENT;
        gx_text(es_rx + 4, y, ent_name(idx), farbe, 256);
        if (ent_type(idx) == FT_DIR) gx_text(es_rx + es_rw - 36, y, "DIR", C_ACCENT, 256);
    }

    /* --- unten: wo das Ganze liegt --- */
    fs_path(gui_pfad);
    gx_text(es_x + 8, es_fy, "Folder:", C_TEXT, 256);
    gx_text(es_x + 70, es_fy, gui_pfad, C_ACCENT, 256);
    gx_text(es_x + 8, es_fy + 14,
           "New files are saved here. Click a folder to change it.", C_WINDARK, 256);
}

int edg_start_klick(int w, int mx, int my) {
    int i; int idx;
    edg_start_masse(w);
    for (i = 0; i < EDG_NEU_ANZ; i++) {
        if (treffer(mx, my, es_x + 8, es_py + 16 + i * 32, es_lw - 16, 26)) {
            edg_neu_starten(i);
            return 1;
        }
    }
    if (treffer(mx, my, es_rx + es_rw - 48, es_py - 4, 48, 16)) {
        fs_chdir("..");
        edg_liste_top = 0;
        return 1;
    }
    for (i = 0; i < es_rows; i++) {
        if (treffer(mx, my, es_rx, es_py + 20 + i * 12, es_rw, 12)) {
            idx = file_index(edg_liste_top + i);
            if (idx < 0) return 0;
            if (ent_type(idx) == FT_DIR) {
                fs_chdir(ent_name(idx));
                edg_liste_top = 0;
                return 1;
            }
            edg_oeffnen(ent_name(idx));
            edg_screen = 1;
            return 1;
        }
    }
    return 0;
}

/* Welche Meldung steht gerade rechts in der Statuszeile? 0 = keine. */

char* edg_statustext() {
    if (edg_build) {
        if (edg_run_danach) return "building, then run";
        return "building ...";
    }
    if (edg_meldung == 1) return "saved";
    if (edg_meldung == 2) return "built";
    if (edg_meldung == 3) return "errors";
    if (cod_gefunden == 2) return "not found";
    return 0;
}

int edg_statusfarbe() {
    if (edg_build) return C_ACCENT;
    if (edg_meldung == 3 || cod_gefunden == 2) return C_WARN;
    return C_GOOD;
}

void app_editor(int w) {
    char* t;
    int x; int y; int zeile; int spalte; int p; int c; int i;
    int cz; int breite; int hoehe; int n;
    /* Der Hintergrund gehoerte frueher dem Schreibtisch: er malte den
       Fensterrahmen samt Fuellung, bevor die Anwendung dran war. Im eigenen
       Puffer muss das Programm selbst wischen, sonst stehen die Reste des
       vorigen Bildes darunter durch. */
    gx_fill(0, 0, fn_breite, fn_hoehe, C_WINBG);

    if (edg_screen == 0) { edg_startscreen(w); return; }
    edg_masse(w);
    x = 4;
    y = 4;
    breite = fn_breite - 8;
    hoehe = EDG_ROWS * 9 + 4;
    t = ed_text();

    /* Sichtbereich dem Cursor nachfuehren */
    cz = ed_line_of(ed_pos);
    if (edg_folgen) {
        if (cz < edg_top) edg_top = cz;
        if (cz >= edg_top + EDG_ROWS) edg_top = cz - EDG_ROWS + 1;
    }

    gx_fill(x, y, breite, hoehe, C_WHITE);
    gx_frame(x, y, breite, hoehe, C_BLACK);
    /* Spalte fuer die Zeilennummern, leicht abgesetzt */
    gx_fill(x + 1, y + 1, EDG_GUTTER - 3, hoehe - 2, C_WIN);
    gx_fill(x + EDG_GUTTER - 2, y + 1, 1, hoehe - 2, C_WINDARK);

    p = ed_start_of_line(edg_top);
    syn_bauen(p, EDG_ROWS, EDG_COLS);
    for (zeile = 0; zeile < EDG_ROWS; zeile++) {
        int lauf; int lauf_farbe; int lauf_start; int lauf_sp;
        if (p <= ed_len)
            gx_num(x + 4, y + 3 + zeile * 9, edg_top + zeile + 1, C_WINDARK, 256);
        spalte = 0;
        lauf = 0;
        lauf_farbe = C_BLACK;
        lauf_sp = 0;
        lauf_start = (int)t + p;
        while (p < ed_len && t[p] != 10) {
            if (spalte < EDG_COLS) {
                c = syn_farbe(zeile, spalte, EDG_COLS);
                /* Markierter Text wird einzeln und invers gemalt -- das
                   passiert selten genug, dass es nichts kostet. */
                if (ed_sel_von >= 0 && p >= ed_sel_von && p < ed_sel_bis) {
                    if (lauf > 0) {
                        gx_str(x + EDG_GUTTER + 3 + lauf_sp * 8,
                              y + 3 + zeile * 9, lauf_start, lauf, lauf_farbe, 256);
                        lauf = 0;
                    }
                    gx_fill(x + EDG_GUTTER + 3 + spalte * 8, y + 2 + zeile * 9, 8, 9,
                           C_TITLEBAR);
                    gx_char(x + EDG_GUTTER + 3 + spalte * 8, y + 3 + zeile * 9,
                           t[p], C_WHITE, 256);
                    lauf_start = (int)t + p + 1;
                } else if (lauf > 0 && c == lauf_farbe) {
                    lauf++;
                } else {
                    if (lauf > 0)
                        gx_str(x + EDG_GUTTER + 3 + lauf_sp * 8,
                              y + 3 + zeile * 9, lauf_start, lauf, lauf_farbe, 256);
                    lauf_start = (int)t + p;
                    lauf_farbe = c;
                    lauf_sp = spalte;
                    lauf = 1;
                }
            }
            spalte++;
            p++;
        }
        if (lauf > 0)
            gx_str(x + EDG_GUTTER + 3 + lauf_sp * 8, y + 3 + zeile * 9,
                  lauf_start, lauf, lauf_farbe, 256);
        if (p < ed_len) p++;
    }

    /* Schreibmarke */
    spalte = ed_col_of(ed_pos);
    if (spalte < EDG_COLS && cz - edg_top < EDG_ROWS) {
        gx_fill(x + EDG_GUTTER + 3 + spalte * 8, y + 3 + (cz - edg_top) * 9 + 8, 7, 1,
               C_TITLEBAR);
    }

    /* Statuszeile */
    y = y + hoehe + 4;
    gx_text(x, y, "File:", C_TEXT, 256);
    if (edg_namemode) {
        gx_fill(x + 44, y - 1, 150, 10, C_TITLEBAR);
        gx_text(x + 46, y, edg_name, C_WHITE, 256);
        gx_fill(x + 48 + strlen(edg_name) * 8, y, 7, 8, C_WHITE);
    } else {
        gx_text(x + 46, y, edg_name, C_ACCENT, 256);
    }
    fs_path(gui_pfad);
    gx_text(x + 210, y, "in", C_TEXT, 256);
    gx_text(x + 234, y, gui_pfad, C_TEXT, 256);

    /* Suchfeld -- rechts neben dem Pfad, sonst wird es zu eng */
    gx_text(x + 350, y, "Ln", C_TEXT, 256);
    gx_num(x + 372, y, cz + 1, C_TEXT, 256);
    gx_text(x + 410, y, "Col", C_TEXT, 256);
    gx_num(x + 440, y, spalte + 1, C_TEXT, 256);
    /* Die Byte-Zahl nur, wenn rechts keine Meldung steht -- beide teilen
       sich denselben Platz. Vorher standen sie uebereinander, und das
       gruene "saved" lief ausserdem unter dem ?-Knopf hindurch aus dem
       Fenster heraus. */
    if (edg_statustext() == 0) {
        gx_text(x + 480, y, "Bytes", C_TEXT, 256);
        gx_num(x + 528, y, ed_len, C_TEXT, 256);
    }

    /* Knopfleiste */
    y = y + 14;
    /* Ganz links der Weg zurueck zur Dateiauswahl -- wie der Up-Knopf in der
       Dateiverwaltung. Ohne ihn kommt man aus einer offenen Datei nicht mehr
       heraus, ausser ueber "New". */
    n = edg_art();
    /* Das ? erklaert, wie man ein BIOS schreibt -- bei einem C-Programm
       waere es nur im Weg. */
    if (n == ART_BIOS) p_knopf(x + fn_breite - 36, y - 14, 20, 14, "?", 0);
    for (i = 0; i < CB_GESAMT; i++) {
        if (cb_sichtbar(n, i) == 0 || i == CB_SUCHE) continue;
        p_knopf(x + cb_pos(n, i), y, cb_w(i), 16, cb_text(i),
                 i == CB_FIND ? cod_suchmode : 0);
    }
    p = cb_pos(n, CB_SUCHE);
    gx_fill(x + p, y + 1, cb_w(CB_SUCHE), 14, C_WHITE);
    gx_frame(x + p, y + 1, cb_w(CB_SUCHE), 14, C_WINDARK);
    gx_text(x + p + 3, y + 4, cod_suche, C_TEXT, 256);
    if (cod_suchmode)
        gx_fill(x + p + 3 + strlen(cod_suche) * 8, y + 4, 7, 8, C_ACCENT);

    /* Statusfeld rechts neben den Knoepfen -- Ladebalken und Meldungen
       teilen sich denselben Platz. */
    y = y - 14;                          /* Meldungen in die Statuszeile */
    t = edg_statustext();
    if (t) {
        /* Rechtsbuendig, und zwar VOR dem ?-Knopf. Feste Spalte 560 hiess
           bei 588 Punkten Platz: drei Zeichen -- alles andere ragte hinaus. */
        gx_text(x + fn_breite - 44 - strlen(t) * 8, y, t, edg_statusfarbe(), 256);
    }
}

/* --- Zwischenablage ------------------------------------------------------
   Ausschneiden, Kopieren, Einfuegen mit Strg+X / Strg+C / Strg+V.
   Der markierte Bereich wird von der Maus gesetzt. */

int edg_pos_aus_maus(int w, int mx, int my) {
    int x; int y; int zeile; int spalte; int p; int ende;
    edg_masse(w);
    x = 4;
    y = 4;
    zeile = (my - y - 3) / 9 + edg_top;
    spalte = (mx - x - 3 - EDG_GUTTER) / 8;
    if (zeile < 0) zeile = 0;
    if (spalte < 0) spalte = 0;
    p = ed_start_of_line(zeile);
    ende = ed_line_end(p);
    if (p + spalte > ende) return ende;
    return p + spalte;
}

/* Speichert den Text unter dem eingestellten Namen. */

void edg_speichern() {
    if (filewrite(edg_name, ED_BUF, ed_len) == 0) edg_meldung = 1;
    else edg_meldung = 3;
}

/* Uebersetzt die Datei: passendes Werkzeug im Hintergrund starten. */
/* ==========================================================================
   Firmware aus dem Coder
   ==========================================================================
   "Test" und "Flash" bauen dieselbe Quelle wie "Build", nur heisst das Ziel
   .BIN statt .TBX und danach wird ein BIOS-Abbild daraus geprueft.

   Die Arbeitsteilung ist mit Absicht so:
     Coder     baut, stempelt Laenge und Pruefsumme, fragt einmal nach
     Mainboard nimmt das Abbild entgegen -- fuer einen Start oder dauerhaft
     Firmware  fragt beim dauerhaften Brennen ein zweites Mal, in Rot

   Ein Programm darf nicht allein entscheiden, dass der Chip ueberschrieben
   wird. Deshalb die zweite Rueckfrage, und deshalb stellt sie das BIOS.  */

#define BIOS_TEST   0
#define BIOS_FLASH  1
#define BIOS_PUFFER 0x00760000       /* hierhin kommt das gebaute Abbild */

int bios_modus = BIOS_TEST;
int bios_wartet = 0;                 /* wir warten auf das Ende des Baus */
int bios_len = 0;
int bios_summe = 0;
char bios_bname[24];

/* Laenge und Pruefsumme in den Kopf schreiben -- dieselbe Rechnung wie
   build.py auf dem Mac und wie Machine.rom_pruefen im Mainboard. */

void edg_uebersetzen() {
    char args[48];
    int i; int n;

    edg_speichern();
    if (edg_meldung == 3) return;

    /* Zielname: NAME.C -> NAME.TBX */
    memset(edg_ziel, 0, 20);
    n = strlen(edg_name);
    i = n;
    while (i > 0 && edg_name[i - 1] != '.') i--;
    if (i == 0) i = n + 1;
    strncpy(edg_ziel, edg_name, i);
    edg_ziel[i - 1] = 0;
    strcat(edg_ziel, ".TBX");

    strcpy(args, edg_name);
    strcat(args, " ");
    strcat(args, edg_ziel);
    /* Frueher stand der Coder im Kernel und konnte prog_run einfach rufen.
       Als Programm geht er ueber einen Systemaufruf -- und schreibt die
       Meldungen des Compilers mit, um sie danach selbst anzuzeigen. */
    mitschrift_an();
    if (endet_auf(edg_name, ".ASM")) edg_pid = prog_starten("ASM.TBX", args);
    else                             edg_pid = prog_starten("CC.TBX", args);
    if (edg_pid >= 0) {
        edg_build = 1;
        edg_meldung = 0;
    } else {
        edg_meldung = 3;
        mitschrift_aus();
    }
}

/* Tastendruck im Editorfenster */

void edg_taste(int k) {
    int c; int code; int z; int sp2; int p;
    if (edg_screen == 0) return;      /* dort wird nur geklickt */
    edg_folgen = 1;              /* beim Tippen springt die Ansicht mit */
    c = keychar(k);
    code = keycode(k);

    /* Im Suchmodus geht jeder Anschlag ins Suchfeld. Die Eingabetaste sucht
       das naechste Vorkommen, ESC beendet die Suche. */
    if (cod_suchmode) {
        z = strlen(cod_suche);
        if (code == K_ENTER) { cod_finden(); return; }
        if (code == K_ESC)   { cod_suchmode = 0; cod_gefunden = 0; return; }
        if (code == K_BACKSPACE) { if (z > 0) cod_suche[z - 1] = 0; return; }
        if (c >= 32 && c < 127 && z < 28) {
            cod_suche[z] = c;
            cod_suche[z + 1] = 0;
        }
        return;
    }
    if (c == 6) { cod_suchmode = 1; cod_gefunden = 0; return; }   /* Strg+F */
    if (code == K_F3) { cod_finden(); return; }

    if (edg_namemode) {                          /* Dateiname wird getippt */
        z = strlen(edg_name);
        if (code == K_ENTER) { edg_namemode = 0; return; }
        if (code == K_ESC)   { edg_namemode = 0; return; }
        if (code == K_BACKSPACE) { if (z > 0) edg_name[z - 1] = 0; return; }
        if (c >= 32 && c < 127 && z < 17) {
            edg_name[z] = toupper(c);
            edg_name[z + 1] = 0;
        }
        return;
    }

    if (code == K_F2) { edg_speichern(); return; }
    if (code == K_F5) { edg_uebersetzen(); return; }
    if (c == 3)  { ed_kopieren(); return; }                  /* Strg+C */
    if (c == 24) { ed_kopieren(); ed_loesche_auswahl(); return; }   /* Strg+X */
    if (c == 22) { ed_einfuegen(); return; }                 /* Strg+V */
    if (c == 1) {                                            /* Strg+A */
        ed_sel_von = 0;
        ed_sel_bis = ed_len;
        return;
    }
    if (code == K_BACKSPACE) { ed_backspace(); edg_meldung = 0; return; }
    if (code == K_DEL)       { ed_delete(); edg_meldung = 0; return; }
    if (code == K_ENTER)     { cod_umbruch(); edg_meldung = 0; return; }
    if (code == K_LEFT)      { if (ed_pos > 0) ed_pos--; return; }
    if (code == K_RIGHT)     { if (ed_pos < ed_len) ed_pos++; return; }
    if (code == K_HOME)      { ed_pos = ed_line_start(ed_pos); return; }
    if (code == K_END)       { ed_pos = ed_line_end(ed_pos); return; }
    if (code == K_UP) {
        z = ed_line_of(ed_pos);
        sp2 = ed_col_of(ed_pos);
        if (z > 0) {
            p = ed_start_of_line(z - 1);
            if (p + sp2 > ed_line_end(p)) p = ed_line_end(p);
            else p = p + sp2;
            ed_pos = p;
        }
        return;
    }
    if (code == K_DOWN) {
        z = ed_line_of(ed_pos);
        sp2 = ed_col_of(ed_pos);
        p = ed_start_of_line(z + 1);
        if (p + sp2 > ed_line_end(p)) p = ed_line_end(p);
        else p = p + sp2;
        ed_pos = p;
        return;
    }
    if (code == K_PGUP) {
        z = ed_line_of(ed_pos) - EDG_ROWS;
        if (z < 0) z = 0;
        ed_pos = ed_start_of_line(z);
        return;
    }
    if (code == K_PGDN) {
        ed_pos = ed_start_of_line(ed_line_of(ed_pos) + EDG_ROWS);
        return;
    }
    if (c == 9) { ed_insert(32); ed_insert(32); edg_meldung = 0; return; }
    if (c >= 32 && c < 127) {
        if (ed_sel_von >= 0 && ed_sel_bis > ed_sel_von) ed_loesche_auswahl();
        ed_insert(c);
        edg_meldung = 0;
    }
}

/* Klick im Editorfenster */

int edg_klick(int w, int mx, int my) {
    int x; int y; int n;
    if (edg_screen == 0) return edg_start_klick(w, mx, my);
    edg_folgen = 1;
    edg_masse(w);
    x = 4;

    /* Klick in den Text: Schreibmarke setzen, Auswahl beginnen */
    y = 4;
    if (my >= y && my < y + EDG_ROWS * 9 + 4) {
        ed_pos = edg_pos_aus_maus(w, mx, my);
        ed_sel_von = ed_pos;
        ed_sel_bis = ed_pos;
        edg_zieht = 1;
        return 1;
    }

    y = 4 + EDG_ROWS * 9 + 8;

    if (my >= y - 4 && my < y + 10) {            /* Zeile mit dem Dateinamen */
        /* Das ? liegt in DIESER Zeile, ganz rechts. Es muss vor dem
           `return 0` geprueft werden -- sonst verschluckt die Statuszeile
           den Klick, und der Knopf tat nie etwas. */
        /* Genau dieselbe Zahl wie beim Zeichnen: dort steht der Knopf bei
           `y - 14` der Knopfleiste, und das ist genau diese Statuszeile. */
        if (edg_art() == ART_BIOS
            && treffer(mx, my, x + fn_breite - 36, y, 20, 14)) {
            bios_hilfe();
            return 1;
        }
        if (mx >= x + 40 && mx < x + 200) { edg_namemode = 1; return 1; }
        return 0;
    }
    y = y + 14;
    if (my >= y && my < y + 16) {
        n = edg_art();
        if (treffer(mx, my, x + cb_pos(n, CB_BACK), y, cb_w(CB_BACK), 16)) {
            edg_screen = 0;          /* zurueck zur Auswahl */
            edg_liste_top = 0;
            return 1;
        }
        if (treffer(mx, my, x + cb_pos(n, CB_NEW), y, cb_w(CB_NEW), 16)) {
            /* Der Name muss mit weg. Sonst zeigt der Editor eine leere Seite,
               heisst aber weiter PROGLIB.C -- und Save wuerde die Datei
               loeschen. Die Endung bleibt, damit Compile weiter passt. */
            int i;
            i = strlen(edg_name);
            while (i > 0 && edg_name[i - 1] != '.') i--;
            if (i == 0) strcpy(edg_name, "NEW.C");
            else {
                char endung[12];
                strncpy(endung, edg_name + i, 10);
                strcpy(edg_name, "NEW.");
                strcat(edg_name, endung);
            }
            ed_len = 0;
            ed_pos = 0;
            edg_top = 0;
            edg_meldung = 0;
            ed_sel_von = 0 - 1;
            ed_sel_bis = 0 - 1;
            return 1;
        }
        if (treffer(mx, my, x + cb_pos(n, CB_SAVE), y, cb_w(CB_SAVE), 16)) {
            /* Speichern fragt jetzt nach Ort und Namen -- wie es sich
               gehoert. Der bisherige Name steht als Vorschlag drin. */
            if (edg_ort && edg_name[0]) edg_speichern();
            else { edg_warte = 2; datei_dialog(DLG_SPEICHERN, "", edg_name); }
            return 1;
        }
        if (treffer(mx, my, x + cb_pos(n, CB_NAME), y, cb_w(CB_NAME), 16))
            { edg_namemode = 1; return 1; }
        if (cb_sichtbar(n, CB_BUILD)
            && treffer(mx, my, x + cb_pos(n, CB_BUILD), y, cb_w(CB_BUILD), 16))
            { edg_uebersetzen(); return 1; }
        if (cb_sichtbar(n, CB_TEST)
            && treffer(mx, my, x + cb_pos(n, CB_TEST), y, cb_w(CB_TEST), 16))
            { edg_speichern(); bios_bauen(BIOS_TEST, edg_name); return 1; }
        if (cb_sichtbar(n, CB_FLASH)
            && treffer(mx, my, x + cb_pos(n, CB_FLASH), y, cb_w(CB_FLASH), 16))
            { edg_speichern(); bios_bauen(BIOS_FLASH, edg_name); return 1; }
        if (treffer(mx, my, x + cb_pos(n, CB_FIND), y, cb_w(CB_FIND), 16)
            || treffer(mx, my, x + cb_pos(n, CB_SUCHE), y + 1, cb_w(CB_SUCHE), 14)) {
            cod_suchmode = 1;
            cod_gefunden = 0;
            return 1;
        }
        if (cb_sichtbar(n, CB_RUN)
            && treffer(mx, my, x + cb_pos(n, CB_RUN), y, cb_w(CB_RUN), 16)) {
            edg_speichern();
            if (edg_meldung == 3) return 1;
            /* Quelltext muss erst uebersetzt werden -- danach starten wir
               das Ergebnis von selbst. Python und fertige Programme
               koennen dagegen sofort los. */
            if (endet_auf(edg_name, ".PY") || endet_auf(edg_name, ".TBX")) {
                im_fenster_starten(edg_name);
            } else {
                edg_run_danach = 1;
                edg_uebersetzen();
            }
            return 1;
        }
    }
    return 0;
}

/* ==========================================================================
   Anwendung: Terminal

   Zeigt den Bildspeicher der Kommandozeile (term.c). Die Shell schreibt
   dort hinein, wir malen es -- beide laufen als eigene Prozesse.
   ========================================================================== */

void app_build(int w) {
    int x; int y; int breite; int i; int zeilen;
    x = 0 + 12;
    y = 0 + 12;
    breite = fn_breite - 24;

    /* Ist der Lauf vorbei und ging etwas schief, zeigt dasselbe Fenster die
       Meldungen des Compilers -- mitgeschrieben ueber cap_* in lib.c. Vorher
       standen sie nur im unsichtbaren Textbildschirm, und im Editor stand
       bloss "Errors". */
    if (edg_build == 0) {
        gx_text_max(x, y, "The compiler reported:", C_WARN, 256, breite);
        zeilen = (fn_hoehe - TITLE_H - 46) / 9;
        if (zeilen > mitschrift_zeilen()) zeilen = mitschrift_zeilen();
        for (i = 0; i < zeilen; i++)
            gx_text_max(x, y + 16 + i * 9, (char*)mitschrift_zeile(i), C_TEXT, 256, breite);
        gx_text_max(x, 0 + fn_hoehe - 16,
                   "Close this window when you have read it.",
                   C_WINDARK, 256, breite);
        return;
    }

    gx_text_max(x, y, edg_name, C_ACCENT, 256, breite / 2);
    gx_text(x + strlen(edg_name) * 8 + 8, y, "->", C_TEXT, 256);
    gx_text_max(x + strlen(edg_name) * 8 + 32, y, edg_ziel, C_ACCENT, 256,
               breite - strlen(edg_name) * 8 - 32);

    /* Balken, die Zahl steht rechts daneben und nicht darauf */
    y = y + 20;
    gx_panel(x, y, breite - 48, 16, 1);
    if (build_fortschritt() > 0)
        gx_fill(x + 2, y + 2, (breite - 52) * build_fortschritt() / 100, 12, C_TITLEBAR);
    gx_fill(x + breite - 44, y + 4, 44, 8, C_WIN);
    gx_num(x + breite - 40, y + 4, build_fortschritt(), C_TEXT, 256);
    if (build_fortschritt() < 10)      gx_text(x + breite - 32, y + 4, "%", C_TEXT, 256);
    else if (build_fortschritt() < 100) gx_text(x + breite - 24, y + 4, "%", C_TEXT, 256);
    else                           gx_text(x + breite - 16, y + 4, "%", C_TEXT, 256);

    /* Was das Werkzeug gerade tut */
    y = y + 24;
    gx_fill(x, y, breite, 10, C_WIN);
    if (build_text()[0]) gx_text(x, y, build_text(), C_TEXT, 256);
    else                 gx_text(x, y, "Starting the compiler ...", C_WINDARK, 256);

    y = y + 16;
    gx_text(x, y, "This window closes by itself.", C_WINDARK, 256);
}

/* ==========================================================================
   Anwendung: System Monitor
   ========================================================================== */

/* ==========================================================================
   Hauptschleife
   ========================================================================== */

/* Hat der Benutzer im Dateidialog etwas ausgewaehlt?
   Rueckgabe: 1 = es hat sich etwas geaendert, also neu malen. Genau daran
   hing es: das Programm handelte richtig, zeichnete aber nicht nach, und
   das Fenster zeigte weiter den alten Auswahlschirm. */
int edg_dialog_pruefen() {
    char name[24];
    int r;
    if (edg_warte == 0) return 0;
    r = datei_gewaehlt(name);
    if (r == 0) return 0;
    if (r == 2) { edg_warte = 0; return 1; }
    if (edg_warte == 1) {
        edg_neu(edg_neu_wahl);
        memset(edg_name, 0, 20);
        strncpy(edg_name, name, 18);
        syn_sprache(edg_name);
        edg_ort = 1;
        edg_screen = 1;
        edg_speichern();
    } else if (edg_warte == 2) {
        memset(edg_name, 0, 20);
        strncpy(edg_name, name, 18);
        syn_sprache(edg_name);
        edg_ort = 1;
        edg_speichern();
    } else {
        edg_ort = 1;
        edg_oeffnen(name);
        edg_screen = 1;
    }
    edg_warte = 0;
    return 1;
}

int main() {
    int e[4];
    int art; int laufen;

    if (fenster_neu("Coder", 596, 300) < 0) {
        print("Der Coder braucht den Schreibtisch -- erst WIN eingeben.\n");
        return 1;
    }
    /* Wurde uns eine Datei mitgegeben (Doppelklick im Dateifenster), dann
       gleich hinein -- sonst erst fragen, was ansteht. */
    edg_screen = 0;
    edg_liste_top = 0;
    if (byte_get(0x00008200) > 32) {
        edg_oeffnen((char*)0x00008200);
        edg_screen = 1;
        edg_ort = 1;
    }
    fenster_malziel();
    app_editor(0);
    fenster_fertig();

    laufen = 1;
    while (laufen) {
        art = fenster_ereignis(e);

        if (art == FE_SCHLIESS) {
            laufen = 0;
        } else if (art == FE_TASTE) {
            clip_len = clip_holen();
            if (edg_screen == 0) {
                if (e[2] == K_ESC) laufen = 0;
            } else {
                edg_taste((e[2] << 8) | e[1]);
            }
            clip_setzen(clip_len);
            fenster_malziel();
            app_editor(0);
            fenster_fertig();
        } else if (art == FE_KLICK) {
            clip_len = clip_holen();
            if (edg_screen == 0) edg_start_klick(0, e[1], e[2]);
            else edg_klick(0, e[1], e[2]);
            clip_setzen(clip_len);
            fenster_malziel();
            app_editor(0);
            fenster_fertig();
        } else if (art == FE_MALEN) {
            fenster_malziel();
            app_editor(0);
            fenster_fertig();
        } else {
            if (edg_dialog_pruefen()) {
                fenster_malziel();
                app_editor(0);
                fenster_fertig();
            }
            /* Laeuft gerade eine Uebersetzung? Dann den Fortschritt zeigen
               und nachsehen, ob sie fertig ist. */
            if (edg_build) {
                if (prog_laeuft(edg_pid) == 0) {
                    edg_build = 0;
                    mitschrift_aus();
                    edg_meldung = 2;
                    if (edg_run_danach) {
                        edg_run_danach = 0;
                        im_fenster_starten(edg_ziel);
                    }
                }
                fenster_malziel();
                app_editor(0);
                fenster_fertig();
                sleep(6);
            } else {
                sleep(2);
            }
        }
    }

    fenster_zu();
    return 0;
}
