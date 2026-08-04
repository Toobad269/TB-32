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
    for (z = 0; z < cap_voll; z++) {
        a = cap_adr(z);
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
