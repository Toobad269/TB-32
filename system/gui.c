/* ==========================================================================
   TOOBAD Desktop  --  die grafische Oberflaeche von TOOBAD-OS

   Laeuft im Grafikmodus 640x400 mit 256 Farben. Gezeichnet wird ueber den
   2D-Beschleuniger der Grafikkarte: das Betriebssystem schreibt Koordinaten
   und Farbe in die Register der Karte und loest dann ein Kommando aus --
   genau so spricht man eine echte Grafikkarte an.

   Anwendungen: Dateiverwaltung, Texteditor-Ansicht, Systemmonitor,
   Systemsteuerung, Uhr und Systeminfo.
   ========================================================================== */

#define P_BLT_X    0x44
#define P_BLT_Y    0x45
#define P_BLT_W    0x46
#define P_BLT_H    0x47
#define P_BLT_COL  0x48
#define P_BLT_CMD  0x49
#define P_BLT_CHR  0x4A
#define P_BLT_SRC  0x4B
#define P_BLT_ZOOM 0x54
#define P_GFX_DOPPEL 0x52
#define P_GFX_TAUSCH 0x53
#define P_BLT_BG   0x4C
#define P_MOUSE_WHEEL 0x63
#define P_MCUR_X   0x4D
#define P_MCUR_Y   0x4E
#define P_MCUR_ON  0x4F
#define P_CMOS_IDX 0x70
#define P_CMOS_DAT 0x71
#define P_TEMP     0xA0
#define P_FAN      0xA1
#define P_THROTTLE 0xA2
#define P_FANMODE  0xA4

#define BLT_FILL   1
#define BLT_FRAME  2
#define BLT_CHAR   3

#define G_W        640
#define G_H        400
#define BAR_Y      378
#define MAXWIN     6
#define TITLE_H    16

#define C_DESK     17
#define C_WIN      7
#define C_WINDARK  8
#define C_WHITE    15
#define C_BLACK    0
#define C_TITLEBAR 1
#define C_TITLEOFF 8
#define C_TEXT     0
#define C_ACCENT   9
#define C_GOOD     2
#define C_WARN     4

#define APP_FILES   1
#define APP_CLOCK   2
#define APP_MONITOR 3
#define APP_ABOUT   4
#define APP_CONTROL 6
#define APP_TERM    7
#define APP_EDITOR  8
#define APP_BUILD   9
#define APP_PAINT   10
#define APP_WORD    11
#define APP_DIALOG  12
#define APP_BIOSFRAGE 13
#define APP_BIOSHILFE 14
#define APP_SETTINGS  15
#define APP_POWER     16

#define EDG_COLS    edg_cols
#define EDG_ROWS    edg_rows

int win_type[MAXWIN];
int win_x[MAXWIN];
int win_y[MAXWIN];
int win_w[MAXWIN];
int win_h[MAXWIN];
int win_top;
int gui_running;
/* Ein Programm hat den Bildschirmmodus umgeschaltet und braucht die ganze
   Flaeche: 0 = alles normal, 1 = der Bildschirm gehoert dem Programm,
   2 = das Programm ist fertig, die Oberflaeche darf sich zurueckholen.
   gui_selbst schuetzt die eigenen Moduswechsel davor, sich selbst zu melden. */
int gui_fremd = 0;
int gui_selbst = 0;

char wtitle[MAXWIN * 20];
char gui_pfad[40];
char edg_name[20];               /* Dateiname im Editorfenster */
int  edg_top = 0;                /* erste sichtbare Zeile */
int  edg_namemode = 0;           /* 1 = der Dateiname wird gerade getippt */
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
int  win_voll[MAXWIN];
int  win_ax[MAXWIN];
int  win_ay[MAXWIN];
int  win_aw[MAXWIN];
int  win_ah[MAXWIN];
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
    edg_cols = (win_w[w] - 14 - EDG_GUTTER) / 8;
    /* Titelleiste, Statuszeile, Knopfleiste -- und unten Rand, damit die
       Knoepfe nicht am Fensterrand kleben. */
    edg_rows = (win_h[w] - TITLE_H - 54) / 9;
    if (edg_cols < 10) edg_cols = 10;
    if (edg_rows < 3) edg_rows = 3;
}
int  file_sel = 0;              /* markierte Zeile in der Dateiverwaltung */
int  file_top = 0;              /* erste sichtbare Zeile (Blaettern) */
int  file_rows = 11;            /* wie viele Zeilen ins Fenster passen */
int  move_quelle = 0 - 1;       /* Eintrag, der gerade verschoben wird */
int  desk_sel = 0 - 1;          /* markiertes Symbol auf dem Schreibtisch */
int  zieh_idx = 0 - 1;          /* Datei, die gerade mit der Maus gezogen wird */
int  zieh_von = 0 - 1;          /* aus welchem Fenster sie stammt */
int  zieh_sym = 0 - 1;          /* Schreibtischsymbol, das gerade wandert */
int  zieh_dx = 0;
int  zieh_dy = 0;
int  klick_zeit = 0;            /* fuer den Doppelklick */
int  klick_was = 0 - 1;

/* Die Knopfleiste der Dateiverwaltung an einer Stelle beschrieben --
   Zeichnen und Klicken lesen dieselbe Tabelle, dann koennen sie nicht
   auseinanderlaufen. */
/* Nur noch vier Knoepfe, nach Haeufigkeit sortiert. Oeffnen und Starten
   macht der Doppelklick -- dafuer braucht es keinen Knopf mehr. */
int fb_breite(int n) {
    if (n == 0) return 40;      /* Up     */
    if (n == 1) return 56;      /* Move   */
    if (n == 2) return 60;      /* Delete   */
    return 82;                  /* Open/Run */
}

int fb_x(int n) {
    int i; int x;
    x = 0;
    for (i = 0; i < n; i++) x = x + fb_breite(i) + 6;
    return x;
}

char* fb_text(int n) {
    if (n == 0) return "Up";
    if (n == 1) { if (move_quelle >= 0) return "Drop"; return "Move"; }
    if (n == 2) return "Delete";
    return "Open/Run";
}
int  ctrl_sel = 0;              /* markierte Zeile in der Systemsteuerung */

/* --- Grundfunktionen ueber den Beschleuniger ----------------------------- */

/* Ein Aufruf statt sechs -- die Rechnung dazu steht bei sys_blit in
   start.asm. Koordinaten stecken zu zweit in einem Wort, der Blitter rechnet
   Werte ab 0x8000 selbst wieder ins Negative. */
/* ==========================================================================
   Aufzeichnung: ein Fenster in Text verwandeln

   Statt fuer jedes Fenster von Hand nachzubauen, was darin steht, laesst
   wt_bauen() das Fenster einfach NOCH EINMAL malen -- nur landet dabei
   jeder Text im Puffer statt auf dem Schirm. Damit liefert auch jedes
   kuenftige Fenster seinen Inhalt, ohne dass hier eine Zeile dazukommt.

   Texte in derselben Bildzeile bleiben zusammen (Beschriftung und Wert),
   eine neue Bildzeile wird eine neue Textzeile.
   ========================================================================== */

#define WT_BUF   0x00770000          /* hierhin kommt der Text */
#define WT_MAX   60000

int wt_aktiv = 0;                    /* 1 = malen heisst aufschreiben */
int wt_zeile_y = 0 - 1;              /* zuletzt aufgeschriebene Bildzeile */
int wt_wunsch = 0;                   /* 1 = das Gehaeuse haette gern Text */
int wt_len = 0;                      /* so viele Bytes liegen bereit */

void wt_zeichen(int c) {
    if (wt_len < WT_MAX - 1) { byte_put(WT_BUF + wt_len, c); wt_len++; }
}

void wt_text(char* s) {
    while (*s) { wt_zeichen(*s); s++; }
}

void wt_zeile(char* s) {
    wt_text(s);
    wt_zeichen(10);
}

/* Neue Bildzeile -> neue Textzeile. Gleiche Bildzeile -> mit Abstand dran,
   damit Beschriftung und Wert zusammenbleiben. */
void wt_trenner(int y) {
    if (wt_len == 0) { wt_zeile_y = y; return; }
    if (y == wt_zeile_y) { wt_zeichen(32); wt_zeichen(32); return; }
    wt_zeichen(10);
    wt_zeile_y = y;
}

void wt_merken(int y, char* s) {
    if (s == 0 || s[0] == 0) return;
    wt_trenner(y);
    wt_text(s);
}

void wt_merken_zahl(int y, int n) {
    char t[16];
    int i; int j; int m;
    wt_trenner(y);
    if (n == 0) { wt_zeichen('0'); return; }
    m = n;
    if (m < 0) { wt_zeichen('-'); m = 0 - m; }
    i = 0;
    while (m > 0) { t[i] = '0' + m % 10; m = m / 10; i++; }
    j = i - 1;
    while (j >= 0) { wt_zeichen(t[j]); j--; }
}

void g_fill(int x, int y, int w, int h, int col) {
    if (wt_aktiv) return;
    if (gui_fremd) return;
    sys_blit((x & 65535) | ((y & 65535) << 16),
             (w & 65535) | ((h & 65535) << 16), col, BLT_FILL);
}

void g_frame(int x, int y, int w, int h, int col) {
    if (wt_aktiv) return;
    if (gui_fremd) return;
    sys_blit((x & 65535) | ((y & 65535) << 16),
             (w & 65535) | ((h & 65535) << 16), col, BLT_FRAME);
}

void g_char(int x, int y, int c, int col, int bg) {
    if (wt_aktiv) return;
    if (gui_fremd) return;
    sys_blitchar((x & 65535) | ((y & 65535) << 16), col, c, bg);
}

/* Eine ganze Zeichenkette in EINEM Malbefehl. Der Blitter holt sich den
   Text selbst aus dem Speicher (Kommando 6). Vorher war es ein Befehl je
   Buchstabe -- bei einer Editorseite 1600 Stueck. */
void g_str(int x, int y, int adresse, int laenge, int col, int bg) {
    if (wt_aktiv) return;
    if (gui_fremd) return;
    if (laenge <= 0) return;
    sys_out(P_BLT_X, x & 65535);
    sys_out(P_BLT_Y, y & 65535);
    sys_out(P_BLT_W, laenge);
    sys_out(P_BLT_COL, col);
    sys_out(P_BLT_BG, bg);
    sys_out(P_BLT_CHR, adresse);
    sys_out(P_BLT_CMD, 6);
    sys_out(P_BLT_CHR, 32);              /* Register wieder unverfaenglich */
}

void g_text(int x, int y, char* s, int col, int bg) {
    if (wt_aktiv) { wt_merken(y, s); return; }
    g_str(x, y, (int)s, strlen(s), col, bg);
}

char gt_puffer[104];

/* Text, der garantiert im Fenster bleibt.
   g_text malt sonst ueber den Rand hinaus. Bei festen Beschriftungen faellt
   das nicht auf, bei Compilermeldungen sofort -- deren Laenge kennt vorher
   niemand. Was nicht passt, endet mit zwei Punkten. */
void g_text_max(int x, int y, char* s, int col, int bg, int maxpx) {
    int n; int i;
    n = maxpx / 8;
    if (n < 1) return;
    if (n > 102) n = 102;
    i = 0;
    while (s[i] && i < n) { gt_puffer[i] = s[i]; i++; }
    if (s[i] && i > 2) { gt_puffer[i - 1] = '.'; gt_puffer[i - 2] = '.'; }
    gt_puffer[i] = 0;
    g_text(x, y, gt_puffer, col, bg);
}

void g_num(int x, int y, int n, int col, int bg) {
    if (wt_aktiv) { wt_merken_zahl(y, n); return; }
    char buf[16];
    itoa(n, buf);
    g_text(x, y, buf, col, bg);
}

/* Achtstellig hexadezimal -- fuer Pruefsummen. */
void g_hex(int x, int y, int n, int col, int bg) {
    char t[12];
    if (wt_aktiv) { wt_merken_zahl(y, n); return; }
    int i; int d;
    i = 0;
    while (i < 8) {
        d = (n >> (28 - i * 4)) & 15;
        t[i] = d < 10 ? '0' + d : 'A' + d - 10;
        i++;
    }
    t[8] = 0;
    g_text(x, y, t, col, bg);
}

void g_num2(int x, int y, int n, int col, int bg) {
    if (n < 10) {
        g_char(x, y, '0', col, bg);
        g_num(x + 8, y, n, col, bg);
    } else {
        g_num(x, y, n, col, bg);
    }
}

void g_panel(int x, int y, int w, int h, int gedrueckt) {
    g_fill(x, y, w, h, C_WIN);
    if (gedrueckt) {
        g_fill(x, y, w, 1, C_WINDARK);
        g_fill(x, y, 1, h, C_WINDARK);
        g_fill(x, y + h - 1, w, 1, C_WHITE);
        g_fill(x + w - 1, y, 1, h, C_WHITE);
    } else {
        g_fill(x, y, w, 1, C_WHITE);
        g_fill(x, y, 1, h, C_WHITE);
        g_fill(x, y + h - 1, w, 1, C_BLACK);
        g_fill(x + w - 1, y, 1, h, C_BLACK);
    }
}

void g_button(int x, int y, int w, int h, char* text, int gedrueckt) {
    int tx;
    g_panel(x, y, w, h, gedrueckt);
    tx = x + (w - strlen(text) * 8) / 2;
    g_text(tx + gedrueckt, y + (h - 8) / 2 + gedrueckt, text, C_TEXT, 256);
}

int cmos_get(int reg) {
    sys_out(P_CMOS_IDX, reg);
    return sys_in(P_CMOS_DAT);
}

void cmos_set(int reg, int wert) {
    sys_out(P_CMOS_IDX, reg);
    sys_out(P_CMOS_DAT, wert);
}

/* --- Fensterverwaltung --------------------------------------------------- */

char* win_title(int i) { return (char*)((int)wtitle + i * 20); }

int win_open(int typ, char* titel, int x, int y, int w, int h) {
    int i;
    for (i = 0; i < MAXWIN; i++) {
        if (win_type[i] == 0) {
            win_type[i] = typ;
            win_x[i] = x;  win_y[i] = y;
            win_w[i] = w;  win_h[i] = h;
            strncpy(win_title(i), titel, 18);
            win_top = i;
            return i;
        }
    }
    return 0 - 1;
}

int win_find(int typ) {
    int i;
    for (i = 0; i < MAXWIN; i++) if (win_type[i] == typ) return i;
    return 0 - 1;
}

/* ==========================================================================
   Anwendung: File Manager
   ========================================================================== */

/* Eintraege des aktuellen Ordners, Ordner zuerst.

   Frueher durchsuchte das hier bei JEDEM Aufruf alle 128 Verzeichnis-
   eintraege -- zweimal, einmal fuer Ordner und einmal fuer Dateien. Und
   aufgerufen wurde es fuer jede gezeichnete Zeile, dazu einmal je Zeile
   fuer die Zeilenzahl. Ein Neuzeichnen des Schreibtischs kostete dadurch
   ueber 400.000 Befehle und war acht Bilder lang beim Malen zuzusehen.

   Jetzt wird die Liste einmal gebaut und gemerkt. Sie gilt, solange sich
   weder der Ordner noch das Verzeichnis geaendert hat -- fs_gen in fs.c
   zaehlt jede Aenderung mit. */
#define FLISTE_MAX  96
int fliste[FLISTE_MAX];
int fliste_n = 0;
int fliste_cwd = 0 - 2;              /* -2 = noch nie gebaut */
int fliste_gen = 0 - 1;

void fliste_bauen() {
    int i; int durchgang;
    fliste_n = 0;
    for (durchgang = 0; durchgang < 2; durchgang++) {
        for (i = 0; i < FS_MAXFILES; i++) {
            if (ent_type(i) == 0) continue;
            if (ent_versteckt(i)) continue;   /* Systemdateien nicht zeigen */
            if (ent_parent(i) != cwd) continue;
            if (durchgang == 0 && ent_type(i) != FT_DIR) continue;
            if (durchgang == 1 && ent_type(i) == FT_DIR) continue;
            if (fliste_n < FLISTE_MAX) { fliste[fliste_n] = i; fliste_n++; }
        }
    }
    fliste_cwd = cwd;
    fliste_gen = fs_gen;
}

void fliste_pruefen() {
    if (fliste_cwd != cwd || fliste_gen != fs_gen) fliste_bauen();
}

int file_index(int zeile) {
    fliste_pruefen();
    if (zeile < 0 || zeile >= fliste_n) return 0 - 1;
    return fliste[zeile];
}

int file_anzahl() {
    fliste_pruefen();
    return fliste_n;
}

/* Zeilenzahl aus der Fenstergroesse, und den Ausschnitt der Auswahl
   nachfuehren. Frueher waren es fest 11 Zeilen ohne Blaettern -- alles
   darunter war schlicht unsichtbar. */
void file_masse(int w) {
    int n;
    file_rows = (win_h[w] - TITLE_H - 62) / 11;
    if (file_rows < 1) file_rows = 1;
    n = file_anzahl();
    if (file_sel >= n) file_sel = n - 1;
    if (file_sel < 0) file_sel = 0;
    if (file_sel < file_top) file_top = file_sel;
    if (file_sel >= file_top + file_rows) file_top = file_sel - file_rows + 1;
    if (file_top > n - file_rows) file_top = n - file_rows;
    if (file_top < 0) file_top = 0;
}

void app_files(int w) {
    int x; int y; int j; int zeile; int idx; int attr; int n; int hoch;
    file_masse(w);
    x = win_x[w] + 6;
    y = win_y[w] + TITLE_H + 6;

    g_text(x, y, "Name", C_ACCENT, 256);
    g_text(x + 136, y, "Size", C_ACCENT, 256);
    g_text(x + 200, y, "Type", C_ACCENT, 256);
    g_fill(x, y + 10, win_w[w] - 12, 1, C_WINDARK);

    zeile = 0;
    while (zeile < file_rows) {
        idx = file_index(file_top + zeile);
        if (idx < 0) break;
        attr = C_TEXT;
        if (file_top + zeile == file_sel) {
            g_fill(x - 2, y + 15 + zeile * 11, win_w[w] - 14, 10, C_TITLEBAR);
            attr = C_WHITE;
        }
        if (ent_type(idx) == FT_DIR) {
            if (file_top + zeile != file_sel) attr = C_ACCENT;
            g_text(x, y + 16 + zeile * 11, ent_name(idx), attr, 256);
            g_text(x + 200, y + 16 + zeile * 11, "Folder", attr, 256);
        } else {
            g_text(x, y + 16 + zeile * 11, ent_name(idx), attr, 256);
            g_num(x + 136, y + 16 + zeile * 11, ent_size(idx), attr, 256);
            if (endet_auf(ent_name(idx), ".TBX"))
                g_text(x + 200, y + 16 + zeile * 11, "Program", attr, 256);
            else
                g_text(x + 200, y + 16 + zeile * 11, "Document", attr, 256);
        }
        zeile++;
    }
    if (zeile == 0) g_text(x, y + 16, "This folder is empty.", C_WINDARK, 256);

    /* Rollbalken, sobald mehr Eintraege da sind als Platz */
    n = file_anzahl();
    if (n > file_rows) {
        j = win_x[w] + win_w[w] - 10;
        g_fill(j, y + 15, 6, file_rows * 11, C_WINDARK);
        hoch = file_rows * 11 * file_rows / n;
        if (hoch < 8) hoch = 8;
        g_fill(j, y + 15 + file_rows * 11 * file_top / n, 6, hoch, C_WHITE);
    }

    /* Pfadzeile */
    fs_path(gui_pfad);
    g_text(x, win_y[w] + win_h[w] - 36, gui_pfad, C_ACCENT, 256);
    if (move_quelle >= 0) {
        g_text(x + 120, win_y[w] + win_h[w] - 36, "Moving:", C_WARN, 256);
        g_text(x + 184, win_y[w] + win_h[w] - 36, ent_name(move_quelle), C_WARN, 256);
    } else {
        /* Nur anzeigen, wenn der Text auch wirklich hineinpasst */
        if (win_w[w] >= 330)
            g_text(x + 120, win_y[w] + win_h[w] - 36,
                   "Double-click opens", C_WINDARK, 256);
    }

    y = win_y[w] + win_h[w] - 22;
    /* Nur zeichnen, was ganz ins Fenster passt -- bei einem schmalen Fenster
       ragte die Leiste sonst ueber den Rand hinaus. */
    for (j = 0; j < 4; j++) {
        if (x + fb_x(j) + fb_breite(j) > win_x[w] + win_w[w] - 6) break;
        g_button(x + fb_x(j), y, fb_breite(j), 16, fb_text(j), 0);
    }
}

/* ==========================================================================
   Anwendung: Editor

   Die Bearbeitungslogik (Einfuegen, Loeschen, Zeilen zaehlen) steckt schon
   in edit.c und wird hier einfach mitbenutzt -- gezeichnet wird nur anders.
   ========================================================================== */

void edg_oeffnen(char* name) {
    int n;
    memset(edg_name, 0, 20);
    strncpy(edg_name, name, 18);
    syn_sprache(edg_name);
    n = fs_read(name, ED_BUF, ED_MAX);
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
    if (i == 0) return "int main() {\n    print(\"Hello from TOOBAD-OS\\n\");\n    getkey();\n    return 0;\n}\n";
    if (i == 1) return "; TB-32 assembler\nstart:\n    li r1, text\n    movi r0, 1\n    int 0x10\n    hlt\ntext:\n    .db \"Hello\", 0\n";
    if (i == 2) return "print(\"Hello from TOOBAD-OS\")\n";
    /* Die BIOS-Vorlage startet sofort -- sie laedt den Bootsektor und
       springt hinein. Alles Weitere baut man drumherum. Was ein BIOS
       liefern muss, sagt der ?-Knopf. */
    if (i == 3) return ".include \"const.inc\"\n.org ROM_BASE\n\nentry:\n    jmp startup                   ; 0x00\n    .db \"TBBI\"                    ; 0x04 signature\n    .dw 0                         ; 0x08 length\n    .dw 0                         ; 0x0C checksum\n    .db \"MY BIOS\", 0              ; 0x10 name on the splash screen\n    .space 24\n\nstartup:                          ; 0x30\n    li sp, BIOS_STACK\n    cli\n    li r10, BDA_BASE              ; clear the BIOS data area\n    li r11, 256\n    movi r12, 0\n.clear:\n    stw [r10], r12\n    addi r10, r10, 4\n    subi r11, r11, 1\n    cmpi r11, 0\n    jnz .clear\n    movi r10, ATTR_NORMAL\n    stwa BDA_ATTR, r10\n\n    movi r10, 100                 ; 100 timer ticks per second\n    out P_TIMER_HZ, r10\n    sti\n\n    movi r1, 0                    ; read the boot sector\n    movi r2, 1\n    li r3, BOOT_ADDR\n    out P_DISK_LBA, r1\n    out P_DISK_COUNT, r2\n    out P_DISK_ADDR, r3\n    movi r10, 1\n    out P_DISK_CMD, r10\n    in r0, P_DISK_STATUS\n    cmpi r0, 0\n    jnz .stop\n\n    li r10, BOOT_ADDR             ; ... and jump into it\n    jmpr r10\n.stop:\n    hlt\n    jmp .stop\n";
    return "int main() {\n    print(\"Hello from TOOBAD-OS\\n\");\n    getkey();\n    return 0;\n}\n";
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
void edg_neu_starten(int i) {
    edg_neu_wahl = i;
    edg_ort = 0;
    memset(edg_name, 0, 20);
    strncpy(edg_name, edg_neu_name(i), 18);
    dlg_oeffne(APP_EDITOR, DLG_SPEICHERN, "", edg_name);
    dlg_neu = 1;
}

void edg_neu_anlegen() {
    edg_neu(edg_neu_wahl);
}

void edg_start_masse(int w) {
    es_x = win_x[w] + 4;
    es_y = win_y[w] + TITLE_H + 4;
    es_w = win_w[w] - 8;
    es_h = win_h[w] - TITLE_H - 8;
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
    g_fill(es_x, es_y, es_w, es_h, C_WIN);
    g_text(es_x + 8, es_y + 8, "What do you want to do?", C_TEXT, 256);

    /* --- links: etwas Neues --- */
    g_text(es_x + 8, es_py, "Create new file", C_ACCENT, 256);
    for (i = 0; i < EDG_NEU_ANZ; i++)
        g_button(es_x + 8, es_py + 16 + i * 32, es_lw - 16, 26, edg_neu_text(i), 0);

    /* --- rechts: etwas Vorhandenes --- */
    g_text(es_rx, es_py, "Open file or folder", C_ACCENT, 256);
    /* Der Weg einen Ordner hoeher gehoert direkt an die Liste, wie in der
       Dateiverwaltung -- unten in der Ecke findet ihn niemand. */
    g_button(es_rx + es_rw - 48, es_py - 4, 48, 16, "Up", 0);
    g_fill(es_rx, es_py + 16, es_rw, es_rows * 12 + 4, C_WHITE);
    g_frame(es_rx, es_py + 16, es_rw, es_rows * 12 + 4, C_BLACK);
    for (i = 0; i < es_rows; i++) {
        idx = file_index(edg_liste_top + i);
        if (idx < 0) break;
        y = es_py + 20 + i * 12;
        farbe = C_TEXT;
        if (ent_type(idx) == FT_DIR) farbe = C_ACCENT;
        g_text(es_rx + 4, y, ent_name(idx), farbe, 256);
        if (ent_type(idx) == FT_DIR) g_text(es_rx + es_rw - 36, y, "DIR", C_ACCENT, 256);
    }

    /* --- unten: wo das Ganze liegt --- */
    fs_path(gui_pfad);
    g_text(es_x + 8, es_fy, "Folder:", C_TEXT, 256);
    g_text(es_x + 70, es_fy, gui_pfad, C_ACCENT, 256);
    g_text(es_x + 8, es_fy + 14,
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

    if (edg_screen == 0) { edg_startscreen(w); return; }
    edg_masse(w);
    x = win_x[w] + 4;
    y = win_y[w] + TITLE_H + 4;
    breite = win_w[w] - 8;
    hoehe = EDG_ROWS * 9 + 4;
    t = ed_text();

    /* Sichtbereich dem Cursor nachfuehren */
    cz = ed_line_of(ed_pos);
    if (edg_folgen) {
        if (cz < edg_top) edg_top = cz;
        if (cz >= edg_top + EDG_ROWS) edg_top = cz - EDG_ROWS + 1;
    }

    g_fill(x, y, breite, hoehe, C_WHITE);
    g_frame(x, y, breite, hoehe, C_BLACK);
    /* Spalte fuer die Zeilennummern, leicht abgesetzt */
    g_fill(x + 1, y + 1, EDG_GUTTER - 3, hoehe - 2, C_WIN);
    g_fill(x + EDG_GUTTER - 2, y + 1, 1, hoehe - 2, C_WINDARK);

    p = ed_start_of_line(edg_top);
    syn_bauen(p, EDG_ROWS, EDG_COLS);
    for (zeile = 0; zeile < EDG_ROWS; zeile++) {
        int lauf; int lauf_farbe; int lauf_start; int lauf_sp;
        if (p <= ed_len)
            g_num(x + 4, y + 3 + zeile * 9, edg_top + zeile + 1, C_WINDARK, 256);
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
                        g_str(x + EDG_GUTTER + 3 + lauf_sp * 8,
                              y + 3 + zeile * 9, lauf_start, lauf, lauf_farbe, 256);
                        lauf = 0;
                    }
                    g_fill(x + EDG_GUTTER + 3 + spalte * 8, y + 2 + zeile * 9, 8, 9,
                           C_TITLEBAR);
                    g_char(x + EDG_GUTTER + 3 + spalte * 8, y + 3 + zeile * 9,
                           t[p], C_WHITE, 256);
                    lauf_start = (int)t + p + 1;
                } else if (lauf > 0 && c == lauf_farbe) {
                    lauf++;
                } else {
                    if (lauf > 0)
                        g_str(x + EDG_GUTTER + 3 + lauf_sp * 8,
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
            g_str(x + EDG_GUTTER + 3 + lauf_sp * 8, y + 3 + zeile * 9,
                  lauf_start, lauf, lauf_farbe, 256);
        if (p < ed_len) p++;
    }

    /* Schreibmarke */
    spalte = ed_col_of(ed_pos);
    if (spalte < EDG_COLS && cz - edg_top < EDG_ROWS) {
        g_fill(x + EDG_GUTTER + 3 + spalte * 8, y + 3 + (cz - edg_top) * 9 + 8, 7, 1,
               C_TITLEBAR);
    }

    /* Statuszeile */
    y = y + hoehe + 4;
    g_text(x, y, "File:", C_TEXT, 256);
    if (edg_namemode) {
        g_fill(x + 44, y - 1, 150, 10, C_TITLEBAR);
        g_text(x + 46, y, edg_name, C_WHITE, 256);
        g_fill(x + 48 + strlen(edg_name) * 8, y, 7, 8, C_WHITE);
    } else {
        g_text(x + 46, y, edg_name, C_ACCENT, 256);
    }
    fs_path(gui_pfad);
    g_text(x + 210, y, "in", C_TEXT, 256);
    g_text(x + 234, y, gui_pfad, C_TEXT, 256);

    /* Suchfeld -- rechts neben dem Pfad, sonst wird es zu eng */
    g_text(x + 350, y, "Ln", C_TEXT, 256);
    g_num(x + 372, y, cz + 1, C_TEXT, 256);
    g_text(x + 410, y, "Col", C_TEXT, 256);
    g_num(x + 440, y, spalte + 1, C_TEXT, 256);
    /* Die Byte-Zahl nur, wenn rechts keine Meldung steht -- beide teilen
       sich denselben Platz. Vorher standen sie uebereinander, und das
       gruene "saved" lief ausserdem unter dem ?-Knopf hindurch aus dem
       Fenster heraus. */
    if (edg_statustext() == 0) {
        g_text(x + 480, y, "Bytes", C_TEXT, 256);
        g_num(x + 528, y, ed_len, C_TEXT, 256);
    }

    /* Knopfleiste */
    y = y + 14;
    /* Ganz links der Weg zurueck zur Dateiauswahl -- wie der Up-Knopf in der
       Dateiverwaltung. Ohne ihn kommt man aus einer offenen Datei nicht mehr
       heraus, ausser ueber "New". */
    n = edg_art();
    /* Das ? erklaert, wie man ein BIOS schreibt -- bei einem C-Programm
       waere es nur im Weg. */
    if (n == ART_BIOS) g_button(x + win_w[w] - 36, y - 14, 20, 14, "?", 0);
    for (i = 0; i < CB_GESAMT; i++) {
        if (cb_sichtbar(n, i) == 0 || i == CB_SUCHE) continue;
        g_button(x + cb_pos(n, i), y, cb_w(i), 16, cb_text(i),
                 i == CB_FIND ? cod_suchmode : 0);
    }
    p = cb_pos(n, CB_SUCHE);
    g_fill(x + p, y + 1, cb_w(CB_SUCHE), 14, C_WHITE);
    g_frame(x + p, y + 1, cb_w(CB_SUCHE), 14, C_WINDARK);
    g_text(x + p + 3, y + 4, cod_suche, C_TEXT, 256);
    if (cod_suchmode)
        g_fill(x + p + 3 + strlen(cod_suche) * 8, y + 4, 7, 8, C_ACCENT);

    /* Statusfeld rechts neben den Knoepfen -- Ladebalken und Meldungen
       teilen sich denselben Platz. */
    y = y - 14;                          /* Meldungen in die Statuszeile */
    t = edg_statustext();
    if (t) {
        /* Rechtsbuendig, und zwar VOR dem ?-Knopf. Feste Spalte 560 hiess
           bei 588 Punkten Platz: drei Zeichen -- alles andere ragte hinaus. */
        g_text(x + win_w[w] - 44 - strlen(t) * 8, y, t, edg_statusfarbe(), 256);
    }
}

/* --- Zwischenablage ------------------------------------------------------
   Ausschneiden, Kopieren, Einfuegen mit Strg+X / Strg+C / Strg+V.
   Der markierte Bereich wird von der Maus gesetzt. */

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

/* Position im Text aus Mauskoordinaten bestimmen */
int edg_pos_aus_maus(int w, int mx, int my) {
    int x; int y; int zeile; int spalte; int p; int ende;
    edg_masse(w);
    x = win_x[w] + 4;
    y = win_y[w] + TITLE_H + 4;
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
    if (fs_write(edg_name, ED_BUF, ed_len) == 0) edg_meldung = 1;
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
int bios_frage = 0;                  /* Rueckfragefenster offen? */
int bios_len = 0;
int bios_summe = 0;
char bios_bname[24];

/* Laenge und Pruefsumme in den Kopf schreiben -- dieselbe Rechnung wie
   build.py auf dem Mac und wie Machine.rom_pruefen im Mainboard. */
int bios_stempeln(int adr, int len) {
    int i; int h; int w;
    while (len & 3) { byte_put(adr + len, 0); len++; }
    word_put(adr + 8, len);
    word_put(adr + 12, 0);
    h = 0x1234;
    i = 0;
    while (i < len) {
        w = word_get(adr + i);
        h = h * 31 + w;
        i = i + 4;
    }
    word_put(adr + 12, h);
    bios_len = len;
    bios_summe = h;
    return h;
}

/* Steht im Kopf ueberhaupt ein BIOS?  -> 0 gut, 1 keine Kennung */
int bios_pruefen(int adr, int len) {
    if (len < 48) return 1;
    if (word_get(adr + 4) != 0x49424254) return 1;      /* "TBBI" */
    return 0;
}

void bios_bauen(int modus) {
    char args[48];
    int i; int n;

    bios_modus = modus;
    edg_speichern();
    if (edg_meldung == 3) return;

    memset(edg_ziel, 0, 20);
    n = strlen(edg_name);
    i = n;
    while (i > 0 && edg_name[i - 1] != '.') i--;
    if (i == 0) i = n + 1;
    strncpy(edg_ziel, edg_name, i);
    edg_ziel[i - 1] = 0;
    strcat(edg_ziel, ".BIN");

    strcpy(args, edg_name);
    strcat(args, " ");
    strcat(args, edg_ziel);
    prog_setargs(args);

    build_progress = 0;
    if (mt_active == 0) mt_enable();
    edg_pid = prog_run("ASM.TBX", 1);
    if (edg_pid >= 0) {
        edg_build = 1;
        bios_wartet = 1;
        edg_meldung = 0;
        build_status[0] = 0;
        cap_start();
        starte(APP_BUILD, "Building", 320, 90);
    }
}

/* Nach dem Bauen: Abbild holen, stempeln, pruefen, nachfragen. */
void bios_fertig() {
    int len;
    bios_wartet = 0;
    len = fs_read(edg_ziel, BIOS_PUFFER, 65536);
    if (len <= 0) { edg_meldung = 3; return; }
    if (bios_pruefen(BIOS_PUFFER, len)) {
        strcpy(build_status, "Not a BIOS image -- the TBBI header is missing.");
        edg_meldung = 3;
        return;
    }
    bios_stempeln(BIOS_PUFFER, len);
    memset(bios_bname, 0, 24);
    strncpy(bios_bname, (char*)(BIOS_PUFFER + 16), 20);
    bios_frage = 1;
    starte(APP_BIOSFRAGE, "Firmware", 420, 150);
}

/* Die Rueckfrage ist bestaetigt: dem Mainboard das Abbild hinhalten. */
void bios_los() {
    bios_frage = 0;
    sys_out(P_FLASH_ADDR, BIOS_PUFFER);
    sys_out(P_FLASH_SIZE, bios_len);
    sys_out(P_FLASH_CMD, 5);                 /* Puffer aus dem RAM holen */
    if (sys_in(P_FLASH_CMD) != 0) { edg_meldung = 3; return; }
    if (bios_modus == BIOS_TEST) sys_out(P_FLASH_CMD, 6);   /* nur ein Start */
    else                         sys_out(P_FLASH_CMD, 8);   /* Flashwunsch */
    sys_out(P_POWER, 2);                     /* Neustart */
}

/* Die Rueckfrage vor dem Testen oder Flashen. */
void app_biosfrage(int i) {
    int x; int y; int b;
    x = win_x[i];
    y = win_y[i] + TITLE_H;
    b = win_w[i];

    g_text(x + 10, y + 8, bios_bname, C_ACCENT, 256);
    g_num(x + 10, y + 22, bios_len, C_TEXT, 256);
    g_text(x + 60, y + 22, "bytes", C_TEXT, 256);
    g_text(x + 120, y + 22, "checksum", C_WINDARK, 256);
    g_hex(x + 190, y + 22, bios_summe, C_TEXT, 256);

    if (bios_modus == BIOS_TEST) {
        g_text_max(x + 10, y + 46, "The machine restarts once with this BIOS.", C_TEXT, 256, b - 20);
        g_text_max(x + 10, y + 58, "The chip stays as it is -- the next restart", C_TEXT, 256, b - 20);
        g_text_max(x + 10, y + 70, "brings your normal BIOS back.", C_TEXT, 256, b - 20);
    } else {
        g_text_max(x + 10, y + 46, "This writes the BIOS chip permanently.", C_TEXT, 256, b - 20);
        g_text_max(x + 10, y + 58, "The firmware will ask you once more, in red,", C_TEXT, 256, b - 20);
        g_text_max(x + 10, y + 70, "before anything is written.", C_TEXT, 256, b - 20);
    }
    g_button(x + b - 180, y + 92, 84, 18,
             bios_modus == BIOS_TEST ? "Test once" : "Continue", 0);
    g_button(x + b - 90, y + 92, 80, 18, "Cancel", 0);
}

/* Was ein BIOS liefern muss -- die Kurzfassung von Doku 16, auf dem Geraet. */
int bh_top = 0;

char* bh_zeile(int n) {
    if (n == 0)  return "THE HEADER -- the first 48 bytes";
    if (n == 1)  return "  0x00  jmp over the header";
    if (n == 2)  return "  0x04  the four letters TBBI";
    if (n == 3)  return "  0x08  length in bytes   (Test/Flash fill these in)";
    if (n == 4)  return "  0x0C  checksum";
    if (n == 5)  return "  0x10  name, 32 bytes, ends with a zero byte";
    if (n == 6)  return "  0x30  your code starts here";
    if (n == 7)  return "";
    if (n == 8)  return "INTERRUPT VECTORS -- table at address 0, 4 bytes each";
    if (n == 9)  return "  0x08 timer   0x09 keyboard   -- both must ack the PIC";
    if (n == 10) return "  0x10 screen  0x13 disk  0x16 keyboard  0x1A time";
    if (n == 11) return "";
    if (n == 12) return "INT 0x10 SCREEN -- function number in r0, order matters";
    if (n == 13) return "  0 putc      1 puts      2 setcursor  3 clear";
    if (n == 14) return "  4 getcursor 5 putat     6 putn       7 puthex (r3=digits)";
    if (n == 15) return "  8 setmode   9 box      10 fillrect  11 hline";
    if (n == 16) return " 12 scroll   13 clearrow 14 putsat    15/16 scrollback";
    if (n == 17) return "";
    if (n == 18) return "CONTROL CHARACTERS -- putc must handle these itself";
    if (n == 19) return "  8 backspace   9 tab   10 newline   13 carriage return";
    if (n == 20) return "  Forget the 8 and the delete key prints a box instead.";
    if (n == 21) return "";
    if (n == 22) return "INT 0x13 DISK    0 read  1 write  2 size";
    if (n == 23) return "  r1 = sector, r2 = count, r3 = address, r0 = status";
    if (n == 24) return "INT 0x16 KEYBOARD 0 wait  1 peek  2 flush";
    if (n == 25) return "  Put a hlt in the wait loop or the CPU runs hot.";
    if (n == 26) return "INT 0x1A TIME     0 ticks  1 clock  2 date";
    if (n == 27) return "";
    if (n == 28) return "BOOTING -- read sector 0 to 0x7C00, check 55 AA at 510,";
    if (n == 29) return "  then jump there. That is all a BIOS has to do.";
    if (n == 30) return "";
    if (n == 31) return "The screen, blitter, mouse and sound need no BIOS at all --";
    if (n == 32) return "the system talks to those ports directly.";
    return 0;
}

void app_bioshilfe(int i) {
    int x; int y; int z; int zeilen; char* t;
    x = win_x[i];
    y = win_y[i] + TITLE_H;
    zeilen = (win_h[i] - TITLE_H - 26) / 12;
    for (z = 0; z < zeilen; z++) {
        t = bh_zeile(bh_top + z);
        if (t == 0) break;
        g_text_max(x + 8, y + 6 + z * 12, t,
                   t[0] == ' ' || t[0] == 0 ? C_TEXT : C_ACCENT, 256,
                   win_w[i] - 16);
    }
    g_text_max(x + 8, win_y[i] + win_h[i] - 16, "PgUp/PgDn scroll    ESC close",
               C_WINDARK, 256, win_w[i] - 16);
}

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
    prog_setargs(args);

    build_progress = 0;
    if (mt_active == 0) mt_enable();
    if (endet_auf(edg_name, ".ASM")) edg_pid = prog_run("ASM.TBX", 1);
    else                             edg_pid = prog_run("CC.TBX", 1);
    if (edg_pid >= 0) {
        edg_build = 1;
        edg_meldung = 0;
        build_status[0] = 0;
        cap_start();                     /* Meldungen mitschreiben */
        starte(APP_BUILD, "Compiling", 480, 300);
    } else {
        edg_meldung = 3;
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
    x = win_x[w] + 4;

    /* Klick in den Text: Schreibmarke setzen, Auswahl beginnen */
    y = win_y[w] + TITLE_H + 4;
    if (my >= y && my < y + EDG_ROWS * 9 + 4) {
        ed_pos = edg_pos_aus_maus(w, mx, my);
        ed_sel_von = ed_pos;
        ed_sel_bis = ed_pos;
        edg_zieht = 1;
        return 1;
    }

    y = win_y[w] + TITLE_H + 4 + EDG_ROWS * 9 + 8;

    if (my >= y - 4 && my < y + 10) {            /* Zeile mit dem Dateinamen */
        /* Das ? liegt in DIESER Zeile, ganz rechts. Es muss vor dem
           `return 0` geprueft werden -- sonst verschluckt die Statuszeile
           den Klick, und der Knopf tat nie etwas. */
        /* Genau dieselbe Zahl wie beim Zeichnen: dort steht der Knopf bei
           `y - 14` der Knopfleiste, und das ist genau diese Statuszeile. */
        if (edg_art() == ART_BIOS
            && treffer(mx, my, x + win_w[w] - 36, y, 20, 14)) {
            bh_top = 0;
            starte(APP_BIOSHILFE, "Writing a BIOS", 460, 300);
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
            else dlg_oeffne(APP_EDITOR, DLG_SPEICHERN, "", edg_name);
            return 1;
        }
        if (treffer(mx, my, x + cb_pos(n, CB_NAME), y, cb_w(CB_NAME), 16))
            { edg_namemode = 1; return 1; }
        if (cb_sichtbar(n, CB_BUILD)
            && treffer(mx, my, x + cb_pos(n, CB_BUILD), y, cb_w(CB_BUILD), 16))
            { edg_uebersetzen(); return 1; }
        if (cb_sichtbar(n, CB_TEST)
            && treffer(mx, my, x + cb_pos(n, CB_TEST), y, cb_w(CB_TEST), 16))
            { bios_bauen(BIOS_TEST); return 1; }
        if (cb_sichtbar(n, CB_FLASH)
            && treffer(mx, my, x + cb_pos(n, CB_FLASH), y, cb_w(CB_FLASH), 16))
            { bios_bauen(BIOS_FLASH); return 1; }
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
                gui_im_fenster(edg_name);
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

void app_term(int w) {
    int x; int y; int zx; int zy; int c; int a; int adr; int mx; int my;
    int zeilenadr;

    x = win_x[w] + 4;
    y = win_y[w] + TITLE_H + 4;

    g_fill(x, y, win_w[w] - 8, win_h[w] - TITLE_H - 8, C_BLACK);

    /* Der Bildspeicher der Shell ist 70x22 gross und aendert sich nicht.
       Ist das Fenster kleiner, malen wir nur so viel, wie hineinpasst --
       sonst schriebe der Text ueber den Fensterrand hinaus. */
    mx = (win_w[w] - 8) / 8;
    my = (win_h[w] - TITLE_H - 8) / 9;
    if (mx > TERM_W) mx = TERM_W;
    if (my > TERM_H) my = TERM_H;

    for (zy = 0; zy < my; zy++) {
        zeilenadr = term_sicht(zy, term_view);
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
            if (c == 219) { g_fill(x + zx * 8, y + zy * 9, 8, 8, a); continue; }
            if (c == 176) { g_fill(x + zx * 8 + 2, y + zy * 9 + 2, 4, 4, a); continue; }
            g_char(x + zx * 8, y + zy * 9, c, a, 256);
        }
    }

    /* Schreibmarke -- nur wenn man live zusieht */
    if (term_view) {
        g_fill(x, y + (my - 1) * 9, mx * 8, 9, C_TITLEBAR);
        g_text(x + 4, y + (my - 1) * 9, "-- scrolled back, press a key --",
               C_WHITE, 256);
    } else if (term_lauf) {
        if (term_x < mx && term_y < my)
            g_fill(x + term_x * 8, y + term_y * 9 + 7, 7, 2, 10);
    } else {
        g_text(x + 8, y + 8, "The command prompt has closed.", C_WINDARK, 256);
    }
    term_dirty = 0;
}

/* ==========================================================================
   Anwendung: Uebersetzungsfenster

   Waehrend CC oder ASM im Hintergrund laufen, zeigt dieses Fenster den
   Fortschritt. Die Zahl kommt vom uebersetzenden Programm selbst: es meldet
   ueber Systemaufruf 28, wie weit es ist, und ueber 29 einen kurzen Text.
   Das Fenster schliesst sich von allein, sobald der Lauf fertig ist.
   ========================================================================== */

void app_build(int w) {
    int x; int y; int breite; int i; int zeilen;
    x = win_x[w] + 12;
    y = win_y[w] + TITLE_H + 12;
    breite = win_w[w] - 24;

    /* Ist der Lauf vorbei und ging etwas schief, zeigt dasselbe Fenster die
       Meldungen des Compilers -- mitgeschrieben ueber cap_* in lib.c. Vorher
       standen sie nur im unsichtbaren Textbildschirm, und im Editor stand
       bloss "Errors". */
    if (edg_build == 0) {
        g_text_max(x, y, "The compiler reported:", C_WARN, 256, breite);
        zeilen = (win_h[w] - TITLE_H - 46) / 9;
        if (zeilen > cap_voll) zeilen = cap_voll;
        for (i = 0; i < zeilen; i++)
            g_text_max(x, y + 16 + i * 9, cap_text(i), C_TEXT, 256, breite);
        g_text_max(x, win_y[w] + win_h[w] - 16,
                   "Close this window when you have read it.",
                   C_WINDARK, 256, breite);
        return;
    }

    g_text_max(x, y, edg_name, C_ACCENT, 256, breite / 2);
    g_text(x + strlen(edg_name) * 8 + 8, y, "->", C_TEXT, 256);
    g_text_max(x + strlen(edg_name) * 8 + 32, y, edg_ziel, C_ACCENT, 256,
               breite - strlen(edg_name) * 8 - 32);

    /* Balken, die Zahl steht rechts daneben und nicht darauf */
    y = y + 20;
    g_panel(x, y, breite - 48, 16, 1);
    if (build_progress > 0)
        g_fill(x + 2, y + 2, (breite - 52) * build_progress / 100, 12, C_TITLEBAR);
    g_fill(x + breite - 44, y + 4, 44, 8, C_WIN);
    g_num(x + breite - 40, y + 4, build_progress, C_TEXT, 256);
    if (build_progress < 10)      g_text(x + breite - 32, y + 4, "%", C_TEXT, 256);
    else if (build_progress < 100) g_text(x + breite - 24, y + 4, "%", C_TEXT, 256);
    else                           g_text(x + breite - 16, y + 4, "%", C_TEXT, 256);

    /* Was das Werkzeug gerade tut */
    y = y + 24;
    g_fill(x, y, breite, 10, C_WIN);
    if (build_status[0]) g_text(x, y, build_status, C_TEXT, 256);
    else                 g_text(x, y, "Starting the compiler ...", C_WINDARK, 256);

    y = y + 16;
    g_text(x, y, "This window closes by itself.", C_WINDARK, 256);
}

/* ==========================================================================
   Anwendung: System Monitor
   ========================================================================== */

void app_monitor(int w) {
    int x; int y; int i; int zeile; int breite; int belegt; int gesamt;
    int temp; int farbe;
    x = win_x[w] + 6;
    y = win_y[w] + TITLE_H + 6;

    g_text(x, y, "Process", C_ACCENT, 256);
    g_text(x + 120, y, "PID", C_ACCENT, 256);
    g_text(x + 160, y, "Status", C_ACCENT, 256);
    g_text(x + 232, y, "CPU ms", C_ACCENT, 256);
    g_fill(x, y + 10, win_w[w] - 12, 1, C_WINDARK);

    zeile = 0;
    for (i = 0; i < MAXPROC; i++) {
        if (p_state[i] == PS_FREI) continue;
        g_text(x, y + 16 + zeile * 10, proc_name(i), C_TEXT, 256);
        g_num(x + 120, y + 16 + zeile * 10, i, C_TEXT, 256);
        if (p_state[i] == PS_LAEUFT)
            g_text(x + 160, y + 16 + zeile * 10, "Running", C_GOOD, 256);
        if (p_state[i] == PS_BEREIT)
            g_text(x + 160, y + 16 + zeile * 10, "Ready", C_TEXT, 256);
        if (p_state[i] == PS_SCHLAEFT)
            g_text(x + 160, y + 16 + zeile * 10, "Sleeping", C_WINDARK, 256);
        g_num(x + 232, y + 16 + zeile * 10, p_ticks[i] * 10, C_TEXT, 256);
        zeile++;
    }
    if (zeile == 0) g_text(x, y + 16, "Multitasking is disabled.", C_WINDARK, 256);

    y = y + 16 + zeile * 10 + 8;
    g_text(x, y, "Disk usage", C_ACCENT, 256);
    gesamt = (sys_disksize() - FS_DATA) / 64;
    belegt = fs_used_sectors() / 64;
    breite = win_w[w] - 24;
    g_fill(x, y + 12, breite, 12, C_WHITE);
    g_frame(x, y + 12, breite, 12, C_BLACK);
    if (gesamt > 0)
        g_fill(x + 1, y + 13, (belegt * (breite - 2)) / gesamt, 10, C_ACCENT);
    g_num(x, y + 30, fs_used_sectors() / 2, C_TEXT, 256);
    g_text(x + 40, y + 30, "KB used of", C_TEXT, 256);
    g_num(x + 128, y + 30, (sys_disksize() - FS_DATA) / 2, C_TEXT, 256);
    g_text(x + 190, y + 30, "KB", C_TEXT, 256);

    g_text(x, y + 44, "Context switches:", C_TEXT, 256);
    g_num(x + 144, y + 44, p_switches, C_ACCENT, 256);
    g_text(x, y + 54, "System up time:", C_TEXT, 256);
    g_num(x + 144, y + 54, sys_ticks() / 100, C_ACCENT, 256);
    g_text(x + 190, y + 54, "s", C_TEXT, 256);

    /* Temperatur, Lüfter und Drosselung */
    y = y + 70;
    temp = sys_in(P_TEMP);
    g_text(x, y, "Temperature", C_ACCENT, 256);
    g_num(x + 100, y, temp / 10, C_TEXT, 256);
    g_text(x + 128, y, "C", C_TEXT, 256);
    farbe = C_GOOD;
    if (temp > 700) farbe = 6;
    if (temp > 850) farbe = C_WARN;
    g_fill(x + 150, y, breite - 150, 8, C_WHITE);
    g_frame(x + 150, y, breite - 150, 8, C_BLACK);
    g_fill(x + 151, y + 1, (temp / 10) * (breite - 152) / 110, 6, farbe);

    g_text(x, y + 12, "Fan", C_ACCENT, 256);
    g_num(x + 100, y + 12, sys_in(P_FAN), C_TEXT, 256);
    g_text(x + 128, y + 12, "%", C_TEXT, 256);
    g_fill(x + 150, y + 12, breite - 150, 8, C_WHITE);
    g_frame(x + 150, y + 12, breite - 150, 8, C_BLACK);
    g_fill(x + 151, y + 13, sys_in(P_FAN) * (breite - 152) / 100, 6, C_ACCENT);

    if (sys_in(P_THROTTLE)) {
        g_text(x, y + 24, "THROTTLING", C_WARN, 256);
        g_num(x + 100, y + 24, sys_in(P_THROTTLE), C_WARN, 256);
        g_text(x + 128, y + 24, "% - CPU slowed down to cool", C_WARN, 256);
    }
}

/* ==========================================================================
   Anwendung: Control Panel
   ========================================================================== */

char* speed_name(int i) {
    if (i == 0) return "0.4 MHz";
    if (i == 1) return "1 MHz";
    if (i == 2) return "2 MHz";
    if (i == 3) return "4 MHz";
    return "8 MHz";
}

int ctrl_gesichert = 0;              /* wann zuletzt ins CMOS geschrieben */

void app_control(int w) {
    int x; int y; int attr; int i; int k;
    x = win_x[w] + 8;
    y = win_y[w] + TITLE_H + 8;

    g_text(x, y, "Hardware settings (stored in CMOS)", C_ACCENT, 256);

    for (i = 0; i < 5; i++) {
        attr = C_TEXT;
        if (i == ctrl_sel) {
            g_fill(x - 4, y + 16 + i * 14, win_w[w] - 12, 12, C_TITLEBAR);
            attr = C_WHITE;
        }
        if (i == 0) {
            g_text(x, y + 18, "CPU clock speed", attr, 256);
            g_text(x + 176, y + 18, speed_name(cmos_get(0x13)), attr, 256);
        }
        if (i == 1) {
            g_text(x, y + 32, "POST beep", attr, 256);
            if (cmos_get(0x12)) g_text(x + 176, y + 32, "Enabled", attr, 256);
            else                g_text(x + 176, y + 32, "Disabled", attr, 256);
        }
        if (i == 2) {
            g_text(x, y + 46, "Quick boot", attr, 256);
            if (cmos_get(0x11)) g_text(x + 176, y + 46, "Enabled", attr, 256);
            else                g_text(x + 176, y + 46, "Disabled", attr, 256);
        }
        if (i == 3) {
            g_text(x, y + 60, "POST messages", attr, 256);
            if (cmos_get(0x15)) g_text(x + 176, y + 60, "Verbose", attr, 256);
            else                g_text(x + 176, y + 60, "Minimal", attr, 256);
        }
        if (i == 4) {
            g_text(x, y + 74, "Fan control", attr, 256);
            k = sys_in(P_FANMODE);
            if (k == 0)      g_text(x + 176, y + 74, "Automatic", attr, 256);
            else if (k == 1) g_text(x + 176, y + 74, "Quiet", attr, 256);
            else if (k == 2) g_text(x + 176, y + 74, "Full speed", attr, 256);
            else             g_text(x + 176, y + 74, "Manual", attr, 256);
        }
    }

    g_text(x, y + 92, "Click a row to change the value.", C_WINDARK, 256);
    g_button(x, y + 104, 96, 16, "Save to CMOS", 0);
    /* Ohne Rueckmeldung weiss niemand, ob der Klick angekommen ist -- die
       Werte sehen vorher und nachher gleich aus. Die Meldung verschwindet
       von selbst, weil das Fenster sich jede Sekunde auffrischt. */
    if (ctrl_gesichert > 0) {
        if (sys_ticks() - ctrl_gesichert < 300)
            g_text(x + 104, y + 108, "Saved", C_GOOD, 256);
        else
            ctrl_gesichert = 0;
    }
    g_text(x + 110, y + 108, "Temperature:", C_WINDARK, 256);
    g_num(x + 210, y + 108, sys_in(P_TEMP) / 10, C_ACCENT, 256);
    g_text(x + 234, y + 108, "C", C_WINDARK, 256);
}

void control_click(int w, int mx, int my) {
    int y; int zeile; int v;
    y = win_y[w] + TITLE_H + 8;

    /* Auch die Breite pruefen. Vorher stand hier nur die Zeile, und ein
       Klick irgendwo daneben -- etwa auf die Temperaturanzeige rechts --
       schrieb das CMOS. */
    if (my >= y + 104 && my < y + 120) {
        if (mx >= win_x[w] + 8 && mx < win_x[w] + 8 + 96) {
            cmos_set(0x3F, 1);
            ctrl_gesichert = sys_ticks();
        }
        return;
    }
    zeile = (my - (y + 16)) / 14;
    if (zeile < 0 || zeile > 4) return;
    ctrl_sel = zeile;
    if (zeile == 0) {
        v = cmos_get(0x13) + 1;
        if (v > 4) v = 0;
        cmos_set(0x13, v);
    }
    if (zeile == 1) cmos_set(0x12, 1 - cmos_get(0x12));
    if (zeile == 2) cmos_set(0x11, 1 - cmos_get(0x11));
    if (zeile == 3) cmos_set(0x15, 1 - cmos_get(0x15));
    if (zeile == 4) {                            /* Lüftermodus umschalten */
        v = sys_in(P_FANMODE) + 1;
        if (v > 2) v = 0;
        sys_out(P_FANMODE, v);
    }
}

/* ==========================================================================
   Anwendung: Clock und About
   ========================================================================== */

void app_clock(int w) {
    int t; int d; int x; int y;
    x = win_x[w] + 16;
    y = win_y[w] + TITLE_H + 12;
    t = sys_clock();
    d = sys_date();

    g_fill(x - 8, y - 6, win_w[w] - 20, 32, C_BLACK);
    g_num2(x + 12, y + 6, (t >> 16) & 255, 10, 256);
    g_char(x + 28, y + 6, ':', 10, 256);
    g_num2(x + 36, y + 6, (t >> 8) & 255, 10, 256);
    g_char(x + 52, y + 6, ':', 10, 256);
    g_num2(x + 60, y + 6, t & 255, 10, 256);

    /* Beschriftung links, Wert immer an derselben Spalte. Vorher begann die
       Betriebszeit schon bei x+36 und lag damit auf dem Wort "Up time". */
    y = y + 38;
    g_text(win_x[w] + 8, y, "Date", C_TEXT, 256);
    g_num2(win_x[w] + 72, y, d & 255, C_TEXT, 256);
    g_char(win_x[w] + 88, y, '.', C_TEXT, 256);
    g_num2(win_x[w] + 96, y, (d >> 8) & 255, C_TEXT, 256);
    g_char(win_x[w] + 112, y, '.', C_TEXT, 256);
    g_num(win_x[w] + 120, y, (d >> 16) & 65535, C_TEXT, 256);

    g_text(win_x[w] + 8, y + 14, "Up time", C_TEXT, 256);
    g_fill(win_x[w] + 72, y + 14, win_w[w] - 80, 8, C_WIN);   /* alte Zahl weg */
    g_num(win_x[w] + 72, y + 14, sys_ticks() / 100, C_ACCENT, 256);
    g_text(win_x[w] + 120, y + 14, "seconds", C_TEXT, 256);
}

void app_about(int w) {
    int x; int y;
    x = win_x[w] + 8;
    y = win_y[w] + TITLE_H + 8;
    g_text(x, y,      "TOOBAD-OS 2.5.2", C_ACCENT, 256);
    g_text(x, y + 14, "Copyright (C) Toobad", C_TEXT, 256);
    g_text(x, y + 32, "System", C_ACCENT, 256);
    g_text(x, y + 44, "Processor", C_TEXT, 256);
    g_text(x + 112, y + 44, "TOOBAD TB-32, 32-bit", C_TEXT, 256);
    g_text(x, y + 54, "Memory", C_TEXT, 256);
    g_num(x + 112, y + 54, mem_get(0x000004A0), C_TEXT, 256);
    g_text(x + 160, y + 54, "KB", C_TEXT, 256);
    g_text(x, y + 64, "Display", C_TEXT, 256);
    g_text(x + 112, y + 64, "TB-VGA 640x400x256", C_TEXT, 256);
    g_text(x, y + 74, "Storage", C_TEXT, 256);
    g_num(x + 112, y + 74, sys_disksize() / 2048, C_TEXT, 256);
    g_text(x + 128, y + 74, "MB, TBFS", C_TEXT, 256);
    g_text(x, y + 92, "Built from scratch: CPU, BIOS, assembler,", C_WINDARK, 256);
    g_text(x, y + 102, "C compiler, file system, kernel, desktop.", C_WINDARK, 256);
}

/* ==========================================================================
   Fenster zeichnen
   ========================================================================== */

/* Fenster auf volle Flaeche und wieder zurueck */
void win_vollbild(int i) {
    if (win_voll[i]) {
        win_x[i] = win_ax[i];  win_y[i] = win_ay[i];
        win_w[i] = win_aw[i];  win_h[i] = win_ah[i];
        win_voll[i] = 0;
    } else {
        win_ax[i] = win_x[i];  win_ay[i] = win_y[i];
        win_aw[i] = win_w[i];  win_ah[i] = win_h[i];
        win_x[i] = 0;  win_y[i] = 0;
        win_w[i] = G_W;  win_h[i] = BAR_Y;
        win_voll[i] = 1;
    }
}

/* Solange ein gestartetes Programm den ganzen Bildschirm hat, malt der
   Schreibtisch nichts -- sonst liegen seine Fenster mitten im Spiel. Die
   Pruefung steht hier unten an der Quelle und nicht bloss in der Hauptschleife:
   dort kann ein Programm mitten in der Runde auf Vollbild schalten, und alles,
   was danach noch gemalt wird, landet im fremden Bild. */
/* Nur der Inhalt, ohne Rahmen und Knoepfe. Getrennt, weil wt_bauen() das
   Fenster ein zweites Mal malen laesst -- in den Textpuffer statt auf den
   Schirm. */
void draw_window_inhalt(int i) {
    if (win_type[i] == APP_FILES)   app_files(i);
    if (win_type[i] == APP_CLOCK)   app_clock(i);
    if (win_type[i] == APP_MONITOR) app_monitor(i);
    if (win_type[i] == APP_ABOUT)   app_about(i);
    if (win_type[i] == APP_CONTROL) app_control(i);
    if (win_type[i] == APP_TERM)    app_term(i);
    if (win_type[i] == APP_EDITOR)  app_editor(i);
    if (win_type[i] == APP_BUILD)   app_build(i);
    if (win_type[i] == APP_PAINT)   app_paint(i);
    if (win_type[i] == APP_WORD)    app_word(i);
    if (win_type[i] == APP_DIALOG)  app_dialog(i);
    if (win_type[i] == APP_BIOSFRAGE) app_biosfrage(i);
    if (win_type[i] == APP_BIOSHILFE) app_bioshilfe(i);
    if (win_type[i] == APP_SETTINGS)  app_settings(i);
    if (win_type[i] == APP_POWER)     app_power(i);
}

void draw_window(int i) {
    int tc; int kx; int ky; int k;
    if (gui_fremd) return;
    if (win_type[i] == 0) return;
    tc = C_TITLEOFF;
    if (i == win_top) tc = C_TITLEBAR;

    g_panel(win_x[i], win_y[i], win_w[i], win_h[i], 0);
    g_fill(win_x[i] + 2, win_y[i] + 2, win_w[i] - 4, TITLE_H - 2, tc);
    g_text(win_x[i] + 6, win_y[i] + 6, win_title(i), C_WHITE, 256);
    /* Vollbild- und Schliessknopf. Die Zeichen kommen nicht mehr aus dem
       Zeichensatz: dessen Muster sind 5x7 in einer 8x8-Zelle und sitzen
       darin links oben -- im 12x11-Kaestchen sah das Kreuz deshalb immer
       verrutscht aus. Selbst gemalt sitzt es genau in der Mitte. */
    kx = win_x[i] + win_w[i] - 30;
    ky = win_y[i] + 3;
    g_panel(kx, ky, 12, 11, 0);
    g_frame(kx + 3, ky + 3, 7, 6, C_BLACK);        /* kleines Fenster */
    g_fill(kx + 3, ky + 3, 7, 2, C_BLACK);

    kx = win_x[i] + win_w[i] - 16;
    g_panel(kx, ky, 12, 11, 0);
    for (k = 0; k < 5; k++) {                      /* ein sauberes X */
        g_fill(kx + 4 + k, ky + 3 + k, 1, 1, C_BLACK);
        g_fill(kx + 8 - k, ky + 3 + k, 1, 1, C_BLACK);
    }

    draw_window_inhalt(i);

    /* Anfasser zum Groessenaendern, unten rechts */
    if (win_voll[i] == 0) {
        g_fill(win_x[i] + win_w[i] - 11, win_y[i] + win_h[i] - 5, 8, 2, C_WINDARK);
        g_fill(win_x[i] + win_w[i] - 7, win_y[i] + win_h[i] - 9, 2, 6, C_WINDARK);
        g_fill(win_x[i] + win_w[i] - 5, win_y[i] + win_h[i] - 11, 2, 8, C_WINDARK);
    }
}

/* Ein Programm im Terminalfenster laufen lassen.

   Der Trick ist einfach und ehrlich: Das Fenster ist die richtige Shell, mit
   eigenem Bildspeicher und eigener Tastatur. Wir oeffnen es (falls noetig)
   und tippen den Befehl fuer den Benutzer hinein. Die Shell startet das
   Programm dann als ihr Kind -- die Ausgabe landet damit von selbst im
   Fenster, und Tasten gehen dorthin, solange das Fenster vorn ist.

   Grafische Programme gehoeren hier nicht hinein: die schalten den
   Bildschirmmodus um und wuerden den Schreibtisch uebermalen. Fuer die gibt
   es weiterhin den Knopf "Run", der die Oberflaeche kurz verlaesst. */
void gui_im_fenster(char* name) {
    int i;
    starte(APP_TERM, "Command Prompt", 580, 230);
    if (term_lauf == 0) {
        if (mt_active == 0) mt_enable();
        term_pid = proc_start("cmd", (int)term_main);
        if (term_pid >= 0) term_lauf = 1;
    }
    /* Die Shell erwartet dasselbe Format wie von der Tastatur: unten das
       Zeichen, oben der Tastencode. Ein blankes 13 erkennt sie NICHT als
       Eingabetaste -- sie schaut auf den Code. */
    for (i = 0; name[i]; i++) term_push_key(name[i]);
    term_push_key(13 + (K_ENTER << 8));
}

/* --- Startmenue ----------------------------------------------------------
   Sieben Knoepfe nebeneinander wurden zu eng. Wie bei richtigen
   Oberflaechen liegen die Anwendungen jetzt in einem Menue, und die Leiste
   zeigt stattdessen, welche Fenster gerade offen sind. */

#define MENU_ANZ  12
#define MENU_X    2
#define MENU_W    180
#define MENU_ZH   14

char* menu_text(int i) {
    if (i == 0) return "File Manager";
    if (i == 1) return "Command Prompt";
    if (i == 2) return "Coder";
    if (i == 3) return "System Monitor";
    if (i == 4) return "Control Panel";
    if (i == 5) return "Paint";
    if (i == 6) return "Word";
    if (i == 7) return "Clock";
    if (i == 8) return "Settings";
    if (i == 9) return "About TOOBAD-OS";
    if (i == 10) return "Power options";
    return "Exit desktop";
}

void draw_menu() {
    int y; int i; int hoehe;
    hoehe = MENU_ANZ * MENU_ZH + 10;
    y = BAR_Y - hoehe;
    g_panel(MENU_X, y, MENU_W, hoehe, 0);
    g_fill(MENU_X + 2, y + 2, 12, hoehe - 4, C_TITLEBAR);
    for (i = 0; i < MENU_ANZ; i++) {
        if (i == MENU_ANZ - 1) g_fill(MENU_X + 18, y + 5 + i * MENU_ZH - 2,
                                      MENU_W - 24, 1, C_WINDARK);
        g_text(MENU_X + 20, y + 6 + i * MENU_ZH, menu_text(i), C_TEXT, 256);
    }
}

/* Kurzname eines Fensters fuer die Leiste */
char* win_kurz(int typ) {
    if (typ == APP_FILES)   return "Files";
    if (typ == APP_TERM)    return "Prompt";
    if (typ == APP_EDITOR)  return "Coder";
    if (typ == APP_BUILD)   return "Compile";
    if (typ == APP_PAINT)   return "Paint";
    if (typ == APP_WORD)    return "Word";
    if (typ == APP_DIALOG)  return "File";
    if (typ == APP_BIOSFRAGE) return "Firmware";
    if (typ == APP_BIOSHILFE) return "Help";
    if (typ == APP_SETTINGS)  return "Settings";
    if (typ == APP_POWER)     return "Power";
    if (typ == APP_MONITOR) return "Monitor";
    if (typ == APP_CONTROL) return "Control";
    if (typ == APP_CLOCK)   return "Clock";
    if (typ == APP_ABOUT)   return "About";
    return "Window";
}

void draw_taskbar() {
    int t; int i; int x;
    char kurz[10];
    if (gui_fremd) return;
    g_panel(0, BAR_Y, G_W, G_H - BAR_Y, 0);
    g_button(2, BAR_Y + 2, 52, 18, "Start", menu_offen);

    /* offene Fenster als Knoepfe -- das vorderste erscheint gedrueckt */
    x = 58;
    for (i = 0; i < MAXWIN; i++) {
        if (win_type[i] == 0) continue;
        if (x + 66 > G_W - 80) break;
        /* Beschriftung notfalls kuerzen -- 64 Punkte fassen 8 Zeichen, ein
           laengeres Wort stand vorher links und rechts aus dem Knopf heraus. */
        strncpy(kurz, win_kurz(win_type[i]), 9);
        g_button(x, BAR_Y + 2, 64, 18, kurz, i == win_top);
        x = x + 66;
    }

    t = sys_clock();
    g_fill(G_W - 70, BAR_Y + 4, 62, 14, C_WINDARK);
    g_num2(G_W - 64, BAR_Y + 7, (t >> 16) & 255, C_WHITE, 256);
    g_char(G_W - 48, BAR_Y + 7, ':', C_WHITE, 256);
    g_num2(G_W - 40, BAR_Y + 7, (t >> 8) & 255, C_WHITE, 256);
}

/* ==========================================================================
   Symbole auf dem Schreibtisch

   "Auf dem Schreibtisch liegen" heisst hier: im Ordner \DESKTOP liegen.
   Damit ist es kein Sonderfall, sondern ein ganz normaler Ordner -- die
   Kommandozeile sieht ihn, die Dateiverwaltung sieht ihn, und Verschieben
   funktioniert mit derselben Funktion wie sonst auch. Genau so machen es
   auch die grossen Systeme.
   ========================================================================== */

#define DESK_X0    16
#define DESK_Y0    32
#define DESK_DX    92
#define DESK_DY    62
#define DESK_SPALTEN 6

/* Wo die Symbole liegen. Ein Wort je Verzeichniseintrag: unten x, oben y.
   0 heisst "noch nie angefasst" -- dann kommt das Symbol ins Raster.
   Die Tabelle liegt als Datei im Schreibtischordner, damit die Anordnung
   einen Neustart ueberlebt. */
int icon_pos[FS_MAXFILES];
int icon_geladen = 0;

/* Lesen und Schreiben gehen immer ueber den aktuellen Ordner. Die Tabelle
   gehoert aber in den Schreibtischordner, egal wo der Benutzer gerade steht
   -- also kurz umschalten und danach zurueck. */
void icon_laden() {
    int i; int alt;
    for (i = 0; i < FS_MAXFILES; i++) icon_pos[i] = 0;
    alt = cwd;
    cwd = desk_ordner();
    fs_read("ICONS.DAT", (int)icon_pos, FS_MAXFILES * 4);
    cwd = alt;
    icon_geladen = 1;
}

void icon_speichern() {
    int alt;
    alt = cwd;
    cwd = desk_ordner();
    fs_write("ICONS.DAT", (int)icon_pos, FS_MAXFILES * 4);
    cwd = alt;
}

int desk_ordner() {
    int i;
    i = fs_find_in("DESKTOP", 0 - 1);
    if (i < 0) {
        int alt;
        alt = cwd;
        cwd = 0 - 1;
        fs_mkdir("DESKTOP");
        cwd = alt;
        i = fs_find_in("DESKTOP", 0 - 1);
    }
    return i;
}

/* Dieselbe Rechnung fuer den Schreibtisch -- und derselbe Grund, sie zu
   merken: desk_x und desk_y riefen sie bisher fuer JEDES Symbol einzeln
   auf, und jeder Aufruf ging durch alle 128 Eintraege. */
int dliste[32];
int dliste_n = 0;
int dliste_gen = 0 - 1;

void dliste_pruefen() {
    int i; int ord;
    if (dliste_gen == fs_gen) return;
    ord = desk_ordner();
    dliste_n = 0;
    for (i = 0; i < FS_MAXFILES; i++) {
        if (ent_type(i) == 0) continue;
        if (ent_parent(i) != ord) continue;
        if (stricmp(ent_name(i), "ICONS.DAT") == 0) continue;
        if (dliste_n < 32) { dliste[dliste_n] = i; dliste_n++; }
    }
    dliste_gen = fs_gen;
}

int desk_index(int n) {
    dliste_pruefen();
    if (n < 0 || n >= dliste_n) return 0 - 1;
    return dliste[n];
}

/* Platz im Raster, solange das Symbol noch nie verschoben wurde */
int desk_raster_x(int n) { return DESK_X0 + (n % DESK_SPALTEN) * DESK_DX; }
int desk_raster_y(int n) { return DESK_Y0 + (n / DESK_SPALTEN) * DESK_DY; }

int desk_x(int n) {
    int idx;
    idx = desk_index(n);
    if (idx >= 0 && icon_pos[idx]) return icon_pos[idx] & 65535;
    return desk_raster_x(n);
}

int desk_y(int n) {
    int idx;
    idx = desk_index(n);
    if (idx >= 0 && icon_pos[idx]) return (icon_pos[idx] >> 16) & 65535;
    return desk_raster_y(n);
}

/* Position merken. Die 1 im unteren Bit von x wuerde stoeren, deshalb wird
   x immer auf gerade Punkte gelegt -- 0 bleibt so eindeutig "nie gesetzt". */
void desk_setzen(int idx, int x, int y) {
    if (x < 4) x = 4;            /* nicht bis an den Rand -- der Name darunter
                                    braucht links etwas Platz */
    if (y < 4) y = 4;
    if (x > G_W - 68) x = G_W - 68;
    if (y > BAR_Y - 54) y = BAR_Y - 54;
    if (x == 0 && y == 0) x = 1;
    icon_pos[idx] = (x & 65535) | ((y & 65535) << 16);
}

/* Ein Symbol. Gemalt wird nur mit Rechtecken -- schraege Kanten entstehen
   als Treppe. Jede Sorte sieht anders aus, und Quelltexte tragen ihre
   Endung als farbigen Streifen, damit man .C von .PY unterscheidet. */

int endung_farbe(char* nm) {
    if (endet_auf(nm, ".C"))   return 12;        /* hellrot   */
    if (endet_auf(nm, ".ASM")) return 14;        /* gelb      */
    if (endet_auf(nm, ".PY"))  return 10;        /* hellgruen */
    if (endet_auf(nm, ".MD"))  return 11;        /* tuerkis   */
    return C_TITLEBAR;                           /* blau      */
}

char* endung_text(char* nm) {
    if (endet_auf(nm, ".C"))   return "C";
    if (endet_auf(nm, ".ASM")) return "ASM";
    if (endet_auf(nm, ".PY"))  return "PY";
    if (endet_auf(nm, ".MD"))  return "MD";
    if (endet_auf(nm, ".TXT")) return "TXT";
    return "";
}

char kurzname[13];

void desk_symbol(int n, int idx) {
    int x; int y; int f; int k; char* nm;
    x = desk_x(n);
    y = desk_y(n);
    nm = ent_name(idx);

    if (ent_type(idx) == FT_DIR) {
        /* Ordner: Reiter, dunkler Ruecken, hellere Vorderseite */
        g_fill(x + 16, y + 4, 18, 5, 6);
        g_fill(x + 12, y + 8, 44, 28, 6);
        g_fill(x + 14, y + 13, 40, 21, 14);
        g_frame(x + 12, y + 8, 44, 28, C_BLACK);
        g_frame(x + 14, y + 13, 40, 21, C_BLACK);

    } else if (endet_auf(nm, ".TBX")) {
        /* Programm: kleiner Bildschirm mit Startpfeil */
        g_fill(x + 15, y + 7, 40, 30, C_WINDARK);       /* Schatten */
        g_fill(x + 13, y + 5, 40, 30, C_WIN);
        g_frame(x + 13, y + 5, 40, 30, C_BLACK);
        g_fill(x + 14, y + 6, 38, 6, C_TITLEBAR);
        g_fill(x + 16, y + 14, 34, 19, C_BLACK);
        for (k = 0; k < 7; k++)                          /* Pfeil nach rechts */
            g_fill(x + 28, y + 17 + k, k + 1, 1, C_GOOD);
        for (k = 0; k < 6; k++)
            g_fill(x + 28, y + 24 + k, 6 - k, 1, C_GOOD);

    } else {
        /* Blatt Papier mit umgeknickter Ecke */
        g_fill(x + 18, y + 8, 36, 30, C_WINDARK);        /* Schatten */
        g_fill(x + 16, y + 6, 36, 30, C_WHITE);
        g_frame(x + 16, y + 6, 36, 30, C_BLACK);
        for (k = 0; k < 8; k++) {                        /* Eselsohr */
            g_fill(x + 44 + k, y + 6, 8 - k, 1, C_WIN);
            g_fill(x + 44 + k, y + 6 + k, 1, 1, C_BLACK);
        }
        g_fill(x + 20, y + 18, 22, 1, C_WINDARK);
        g_fill(x + 20, y + 22, 22, 1, C_WINDARK);
        g_fill(x + 20, y + 26, 14, 1, C_WINDARK);
        /* Endung als farbiger Streifen unten links */
        f = strlen(endung_text(nm));
        if (f) {
            g_fill(x + 16, y + 28, f * 8 + 4, 8, endung_farbe(nm));
            g_text(x + 18, y + 28, endung_text(nm), C_BLACK, 256);
        }
    }

    /* Name darunter, mittig. Lange Namen werden wirklich gekuerzt -- vorher
       ging nur die Mittenrechnung von 10 Zeichen aus, gezeichnet wurde aber
       der ganze Name, und bei einem Symbol am linken Rand lief er aus dem
       Bild heraus. */
    strncpy(kurzname, nm, 12);   /* 11 Zeichen -- "MEMTEST.TBX" passt genau */
    f = strlen(kurzname);
    k = x + 34 - f * 4;
    if (k < 2) k = 2;
    if (k + f * 8 > G_W - 2) k = G_W - 2 - f * 8;
    if (desk_sel == n) g_fill(k - 2, y + 40, f * 8 + 4, 10, C_TITLEBAR);
    g_text(k, y + 41, kurzname, C_WHITE, 256);
}

void draw_icons() {
    int n; int idx;
    if (icon_geladen == 0) icon_laden();
    for (n = 0; n < 21; n++) {
        idx = desk_index(n);
        if (idx < 0) break;
        if (desk_y(n) + 52 > BAR_Y) break;
        desk_symbol(n, idx);
    }
}

/* Auf welchem Symbol liegt die Maus? -1 = auf keinem */
int desk_treffer(int mx, int my) {
    int n; int idx;
    for (n = 0; n < 21; n++) {
        idx = desk_index(n);
        if (idx < 0) return 0 - 1;
        if (treffer(mx, my, desk_x(n), desk_y(n), 68, 52)) return n;
    }
    return 0 - 1;
}

int win_unter(int mx, int my) {
    int k; int i;
    for (k = 0 - 1; k < MAXWIN; k++) {
        i = k;
        if (k == 0 - 1) i = win_top;
        else if (k == win_top) continue;
        if (i < 0 || win_type[i] == 0) continue;
        if (treffer(mx, my, win_x[i], win_y[i], win_w[i], win_h[i])) return i;
    }
    return 0 - 1;
}

/* ==========================================================================
   "Gib mir den Text dieses Fensters"

   Das Gehaeuse (Strg+K) kann im Grafikmodus nichts auslesen -- dort stehen
   Bildpunkte, kein Text. Es kann aber BITTEN: es setzt wt_wunsch auf 1, der
   Schreibtisch sieht das in seiner Schleife, legt den Text hin und setzt den
   Wunsch zurueck.

   Der Vorteil gegenueber "das Gehaeuse liest jeden Puffer selbst": jedes
   Programm beantwortet die Frage fuer sich. Ein neues Fenster braucht eine
   Zeile hier -- und nicht eine Zeile in pc.py, das sonst viel zu viel ueber
   das System wissen muesste.                                              */

/* Fuellt WT_BUF mit dem Inhalt des obersten Fensters. */
void wt_bauen() {
    int i; int n; int typ; char* t;
    wt_len = 0;
    i = win_top;
    if (i < 0 || win_type[i] == 0) return;
    typ = win_type[i];

    if (typ == APP_EDITOR) {                 /* Coder: der ganze Quelltext */
        t = ed_text();
        n = 0;
        while (n < ed_len) { wt_zeichen(t[n]); n++; }
        return;
    }
    if (typ == APP_WORD) {                   /* Word: der ganze Fliesstext */
        t = (char*)WD_TEXT;
        n = 0;
        while (n < wd_len) { wt_zeichen(t[n]); n++; }
        return;
    }
    if (typ == APP_FILES || typ == APP_DIALOG) {
        fs_path(gui_pfad);
        wt_zeile(gui_pfad);
        for (n = 0; n < file_anzahl(); n++) {
            i = file_index(n);
            wt_text(ent_name(i));
            if (ent_type(i) == FT_DIR) wt_text("   <DIR>");
            wt_zeichen(10);
        }
        return;
    }
    if (typ == APP_TERM) {                   /* Terminalfenster, Zelle fuer Zelle */
        n = 0;
        while (n < TERM_H) {
            i = 0;
            while (i < TERM_W) {
                wt_zeichen(byte_get(TERM_BUF + (n * TERM_W + i) * 2));
                i++;
            }
            wt_zeichen(10);
            n++;
        }
        return;
    }
    /* Alles andere -- Control Panel, Monitor, Uhr, About, die
       Firmware-Fenster -- malt sich selbst NOCH EINMAL, nur landet dabei
       jeder Text im Puffer statt auf dem Schirm. So liefert auch jedes
       kuenftige Fenster seinen Inhalt, ohne dass hier eine Zeile dazukommt. */
    /* Titel ohne Zeilenende -- den setzt der erste aufgezeichnete Text.
       Sonst stehen drei Leerzeilen am Anfang. */
    wt_text(win_title(i));
    wt_zeile_y = 0 - 1;
    wt_aktiv = 1;
    draw_window_inhalt(i);
    wt_aktiv = 0;
    wt_zeichen(10);
}

void draw_desktop() {
    int i;
    if (gui_fremd) return;
    g_fill(0, 0, G_W, BAR_Y, C_DESK);
    g_text(8, 8, "TOOBAD-OS Desktop", C_WHITE, 256);
    g_fill(8, 18, 136, 1, C_ACCENT);
    draw_icons();

    for (i = 0; i < MAXWIN; i++)
        if (i != win_top) draw_window(i);
    if (win_top >= 0) draw_window(win_top);

    draw_taskbar();
    if (menu_offen) draw_menu();
}

/* ==========================================================================
   Maus und Hauptschleife
   ========================================================================== */

/* Auf welchem Fenster liegt der Punkt? -1 = auf keinem (freier Schreibtisch) */
int win_unter(int mx, int my);

int treffer(int x, int y, int bx, int by, int bw, int bh) {
    if (x < bx || x >= bx + bw) return 0;
    if (y < by || y >= by + bh) return 0;
    return 1;
}

void starte(int typ, char* titel, int w, int h) {
    int i; int x; int y;
    i = win_find(typ);
    if (i >= 0) { win_top = i; return; }
    x = 20 + typ * 18;                           /* leicht versetzt stapeln */
    y = 16 + typ * 12;
    if (x + w > G_W) x = G_W - w - 4;            /* aber immer im Bild bleiben */
    if (y + h > BAR_Y) y = BAR_Y - h - 4;
    if (x < 2) x = 2;
    if (y < 2) y = 2;
    win_open(typ, titel, x, y, w, h);
}

/* Endet der Name auf <endung>? (Vergleich ohne Gross-/Kleinschreibung) */
int endet_auf(char* name, char* endung) {
    int n; int e; int i;
    n = strlen(name);
    e = strlen(endung);
    if (n < e) return 0;
    for (i = 0; i < e; i++)
        if (toupper(name[n - e + i]) != toupper(endung[i])) return 0;
    return 1;
}

/* Ein Programm aus der Oberflaeche heraus starten.

   Textprogramme und die grafische Oberflaeche koennen sich den Bildschirm
   nicht teilen -- und wer auf eine Taste wartet, nimmt sie der Oberflaeche
   weg. Deshalb machen wir es wie die fruehen Fenstersysteme mit ihren
   DOS-Programmen: Oberflaeche verlassen, das Programm im Textmodus laufen
   lassen, danach zurueck auf den Schreibtisch. */
void gui_ausfuehren(char* name) {
    char datei[20];
    strncpy(datei, name, 18);
    gui_selbst = 1;              /* hier steuern wir den Modus selbst */

    sys_out(P_MCUR_ON, 0);
    sys_setmode(0 + 256);
    sys_cls(NORMAL);
    sys_setcursor(0, 0);
    printc("Running ", CYAN);
    printc(datei, BRIGHT);
    print("\n\n");

    if (endet_auf(datei, ".TBX")) {
        prog_setargs("");
        prog_run(datei, 0);
    } else if (endet_auf(datei, ".PY")) {
        prog_setargs(datei);                     /* Interpreter mit Datei */
        prog_run("PY.TBX", 0);
    } else if (endet_auf(datei, ".ASM")) {
        printc("This is assembly source. Assemble it first:\n", NORMAL);
        print("  ASM ");
        print(datei);
        print(" PROG.TBX\n");
    } else if (endet_auf(datei, ".C")) {
        printc("This is C source. Compile it first:\n", NORMAL);
        print("  CC ");
        print(datei);
        print(" PROG.TBX\n");
    } else {
        printc("Not a program. Use Open to view this file.\n", NORMAL);
    }

    print("\n");
    printc(" -- finished, press a key to return to the desktop -- ", INVERS);
    sys_flushkeys();
    getkey();

    sys_setmode(1 + 256);                        /* zurueck zur Oberflaeche */
    sys_out(P_BLT_SRC, (int)font8);
    sys_out(P_MCUR_ON, 1);
    gui_selbst = 0;
    gui_fremd = 0;
}

/* Was passiert, wenn man einen Eintrag oeffnet -- egal ob per Doppelklick
   in der Dateiverwaltung oder auf dem Schreibtisch. */
void eintrag_oeffnen(int idx) {
    if (idx < 0) return;
    if (ent_type(idx) == FT_DIR) {
        fs_chdir(ent_name(idx));
        file_sel = 0;
        file_top = 0;
        starte(APP_FILES, "File Manager", 400, 230);
        return;
    }
    /* Der Ordner der Datei wird zum aktuellen Ordner. Ohne das sucht die
       Shell im Terminalfenster an der falschen Stelle: der Suchpfad ist
       aktueller Ordner, dann \SYSTEM, dann \PROGS -- \DESKTOP steht nicht
       darin. Ein Doppelklick auf ein Programm dort brachte deshalb
       "is not recognized as a command or program". */
    if (cwd != ent_parent(idx)) {
        cwd = ent_parent(idx);
        file_sel = 0;
        file_top = 0;
    }
    if (endet_auf(ent_name(idx), ".TBX")
        || endet_auf(ent_name(idx), ".PY")) {
        /* Vom Schreibtisch gestartete Programme bekommen den ganzen
           Bildschirm. Im Fenster laeuft nur, was man in der Kommandozeile
           selbst eintippt -- dort ist das Fenster ja die Shell. */
        gui_ausfuehren(ent_name(idx));
    } else {
        edg_oeffnen(ent_name(idx));          /* alles andere in den Editor */
        edg_screen = 1;
        starte(APP_EDITOR, "Coder", 596, 292);
    }
}

/* Klick in die Dateiverwaltung auswerten.
   Rueckgabe: 0 nichts, 1 neu zeichnen, 2 auf einer Zeile (Ziehen moeglich) */
int files_click(int w, int mx, int my) {
    int y; int zeile; int idx; int bx; int by; int r;
    y = win_y[w] + TITLE_H + 6;
    by = win_y[w] + win_h[w] - 22;
    bx = win_x[w] + 6;

    if (my >= by && my < by + 16) {
        if (treffer(mx, my, bx + fb_x(0), by, fb_breite(0), 16)) {   /* Up */
            fs_chdir("..");
            file_sel = 0;
            file_top = 0;
            return 1;
        }
        if (treffer(mx, my, bx + fb_x(1), by, fb_breite(1), 16)) {   /* Move */
            if (move_quelle >= 0) {                  /* zweiter Klick: ablegen */
                r = fs_move(move_quelle, cwd);
                /* Nur loslassen, wenn es geklappt hat -- sonst bleibt die
                   Datei aufgenommen und man kann einen anderen Ordner
                   probieren. */
                if (r == 0) {
                    move_quelle = 0 - 1;
                    file_sel = 0;
                    file_top = 0;
                }
            } else {
                move_quelle = file_index(file_sel);  /* erster Klick: aufnehmen */
            }
            return 1;
        }
        idx = file_index(file_sel);
        if (idx < 0) return 1;
        if (treffer(mx, my, bx + fb_x(2), by, fb_breite(2), 16)) {   /* Delete */
            if (idx == move_quelle) move_quelle = 0 - 1;
            if (ent_type(idx) == FT_DIR) fs_rmdir(ent_name(idx));
            else fs_delete(ent_name(idx));
            if (file_sel > 0) file_sel--;
            return 1;
        }
        /* Open/Run auf ganzem Bildschirm: fuer grafische Programme, die die
           volle Flaeche brauchen -- die Oberflaeche tritt dafuer kurz ab. */
        if (treffer(mx, my, bx + fb_x(3), by, fb_breite(3), 16)) {
            if (ent_type(idx) == FT_DIR) {
                fs_chdir(ent_name(idx));
                file_sel = 0;
                file_top = 0;
            } else {
                gui_ausfuehren(ent_name(idx));
            }
            return 1;
        }
        return 1;
    }
    zeile = (my - (y + 15)) / 11;
    if (zeile >= 0 && zeile < file_rows && file_index(file_top + zeile) >= 0) {
        file_sel = file_top + zeile;
        return 2;                     /* auf einer Zeile: Ziehen ist erlaubt */
    }
    return 0;
}

/* ==========================================================================
   Anmeldung im Grafikmodus

   Sie kommt VOR den Schreibtisch, nicht hinein: sonst saehe man kurz die
   Fenster von jemand anderem, bevor gefragt wird.

   Dieselbe Pruefsumme wie die Textfassung in kernel.c -- neu ist nur die
   Oberflaeche. Und dieselbe Einschraenkung: das haelt neugierige Leute auf,
   es ist keine Sicherheit.
   ========================================================================== */

char gl_name[24];
char gl_pw[32];
char gl_pw2[32];
int  gl_feld = 0;                    /* welches Feld gerade dran ist */
int  gl_fehler = 0;

void gl_kasten(int y, char* beschriftung, char* inhalt, int sterne, int aktiv) {
    int x; int i; int n;
    x = 180;
    g_text(x, y, beschriftung, C_TEXT, 256);
    g_fill(x + 96, y - 3, 180, 14, C_WHITE);
    g_frame(x + 96, y - 3, 180, 14, aktiv ? C_ACCENT : C_WINDARK);
    n = strlen(inhalt);
    if (sterne) {
        for (i = 0; i < n && i < 22; i++)
            g_char(x + 100 + i * 8, y, '*', C_TEXT, 256);
    } else {
        g_text(x + 100, y, inhalt, C_TEXT, 256);
    }
    if (aktiv) g_fill(x + 100 + n * 8, y, 7, 8, C_ACCENT);
}

/* Der Ein-/Ausschalter unten rechts. Gezeichnet, nicht getippt: ein Ring
   mit einem Strich oben, wie auf jedem Geraet seit dreissig Jahren. */
#define PW_X   (G_W - 40)
#define PW_Y   (G_H - 36)
#define PW_B   28
#define PW_H   26

int gl_menue = 0;                    /* Klappmenue offen? */

/* Ein Ring mit Luecke oben und ein Strich hinein. x,y ist die linke obere
   Ecke eines 12x12-Feldes. Der Ring wird aus einzelnen Punkten gesetzt --
   vier Balken sahen aus wie ein Kasten, nicht wie ein Schalter. */
void gl_punkt(int x, int y, int col) { g_fill(x, y, 2, 2, col); }

void gl_power_symbol(int x, int y, int col) {
    int cx; int cy;
    cx = x + 5;
    cy = y + 6;
    gl_punkt(cx - 3, cy - 4, col);            /* oben links neben der Luecke */
    gl_punkt(cx - 5, cy - 2, col);
    gl_punkt(cx - 5, cy,     col);
    gl_punkt(cx - 4, cy + 3, col);
    gl_punkt(cx - 2, cy + 4, col);            /* unten herum */
    gl_punkt(cx,     cy + 5, col);
    gl_punkt(cx + 2, cy + 4, col);
    gl_punkt(cx + 4, cy + 3, col);
    gl_punkt(cx + 5, cy,     col);
    gl_punkt(cx + 5, cy - 2, col);
    gl_punkt(cx + 3, cy - 4, col);            /* oben rechts neben der Luecke */
    g_fill(cx, cy - 6, 2, 7, col);            /* der Strich in die Luecke */
}

void gl_power_malen() {
    int mx; int my;
    g_panel(PW_X, PW_Y, PW_B, PW_H, gl_menue);
    gl_power_symbol(PW_X + 8, PW_Y + 6, C_TEXT);
    if (gl_menue == 0) return;
    mx = PW_X + PW_B - 150;
    my = PW_Y - 44;
    g_fill(mx, my, 150, 40, C_WIN);
    g_frame(mx, my, 150, 40, C_WINDARK);
    g_text(mx + 10, my + 6, "Restart", C_TEXT, 256);
    g_text(mx + 10, my + 24, "Shut down", C_TEXT, 256);
}

/* -1 = nichts getroffen, 0 = Knopf, 1 = Neustart, 2 = Ausschalten */
int gl_power_klick(int mx, int my) {
    if (treffer(mx, my, PW_X, PW_Y, PW_B, PW_H)) return 0;
    if (gl_menue) {
        if (treffer(mx, my, PW_X + PW_B - 150, PW_Y - 44, 150, 18)) return 1;
        if (treffer(mx, my, PW_X + PW_B - 150, PW_Y - 26, 150, 18)) return 2;
    }
    return 0 - 1;
}

void gl_malen(int neu_anlegen) {
    g_fill(0, 0, G_W, G_H, C_DESK);
    g_fill(140, 120, 360, neu_anlegen ? 150 : 120, C_WIN);
    g_frame(140, 120, 360, neu_anlegen ? 150 : 120, C_WINDARK);
    g_fill(140, 120, 360, 16, C_TITLEBAR);
    g_text(148, 124, neu_anlegen ? "Welcome to TOOBAD-OS" : "TOOBAD-OS", C_WHITE, 256);

    if (neu_anlegen) {
        g_text(160, 148, "This is the first start of this machine.", C_TEXT, 256);
        gl_kasten(174, "User name", gl_name, 0, gl_feld == 0);
        gl_kasten(196, "Password",  gl_pw,   1, gl_feld == 1);
        gl_kasten(218, "Repeat",    gl_pw2,  1, gl_feld == 2);
        if (gl_fehler) g_text(160, 244, "The two entries differ.", C_WARN, 256);
        else g_text(160, 244, "Click a field or press TAB, ENTER confirms", C_WINDARK, 256);
    } else {
        g_text(160, 150, "User", C_WINDARK, 256);
        /* Der Name des Kontos, nicht der Eingabepuffer -- beim Anmelden
           tippt niemand einen Namen, das Feld waere immer leer. */
        g_text(200, 150, benutzer_name(), C_ACCENT, 256);
        gl_kasten(176, "Password", gl_pw, 1, 1);
        if (gl_fehler) g_text(160, 206, "Wrong password.", C_WARN, 256);
        else g_text(160, 206, "ENTER to sign in", C_WINDARK, 256);
    }
    gl_power_malen();
    sys_out(P_GFX_TAUSCH, 2);
}

/* Rueckgabe: 1 = angemeldet. Laeuft, bis es stimmt. */
int gui_anmelden(int neu_anlegen) {
    int k; int c; int code; int n;
    int mx; int my; int btn; int alt_btn;
    char* ziel;

    memset(gl_pw, 0, 32);
    memset(gl_pw2, 0, 32);
    gl_feld = neu_anlegen ? 0 : 1;
    gl_fehler = 0;

    alt_btn = 0;
    while (1) {
        gl_malen(neu_anlegen);

        /* Maus: der Schalter unten rechts. Sie wird hier selbst abgefragt,
           weil der Schreibtisch noch gar nicht laeuft. */
        mx = sys_in(0x60);
        my = sys_in(0x61);
        btn = sys_in(0x62);
        sys_out(P_MCUR_X, mx);
        sys_out(P_MCUR_Y, my);
        sys_out(P_MCUR_ON, 1);
        if ((btn & 1) && alt_btn == 0) {
            /* In ein Feld geklickt? Dann dorthin wechseln. */
            if (neu_anlegen) {
                if (treffer(mx, my, 180, 166, 276, 20)) gl_feld = 0;
                else if (treffer(mx, my, 180, 188, 276, 20)) gl_feld = 1;
                else if (treffer(mx, my, 180, 210, 276, 20)) gl_feld = 2;
            }
            n = gl_power_klick(mx, my);
            if (n == 0) gl_menue = 1 - gl_menue;
            else if (n == 1) sys_out(P_POWER, 2);
            else if (n == 2) sys_out(P_POWER, 1);
            else gl_menue = 0;
        }
        alt_btn = btn & 1;

        if (sys_haskey() == 0) { sleep(1); continue; }
        k = sys_getkey();
        c = keychar(k);
        code = keycode(k);

        if (gl_feld == 0) ziel = gl_name;
        else if (gl_feld == 1) ziel = gl_pw;
        else ziel = gl_pw2;
        n = strlen(ziel);

        if (code == K_TAB) {
            if (neu_anlegen) { gl_feld++; if (gl_feld > 2) gl_feld = 0; }
            continue;
        }
        if (code == K_BACKSPACE) { if (n > 0) ziel[n - 1] = 0; continue; }
        if (code == K_ENTER) {
            if (neu_anlegen) {
                if (gl_feld < 2) { gl_feld++; continue; }
                if (gl_name[0] == 0) { gl_feld = 0; continue; }
                if (strcmp(gl_pw, gl_pw2) != 0) {
                    gl_fehler = 1;
                    memset(gl_pw, 0, 32);
                    memset(gl_pw2, 0, 32);
                    gl_feld = 1;
                    continue;
                }
                benutzer_anlegen(gl_name, gl_pw);
                return 1;
            }
            if (benutzer_passt(gl_pw)) return 1;
            gl_fehler = 1;
            memset(gl_pw, 0, 32);
            continue;
        }
        if (c >= 32 && c < 127 && n < 20) { ziel[n] = c; ziel[n + 1] = 0; }
    }
}

/* ==========================================================================
   Einstellungen: Passwort aendern und den Rechner zuruecksetzen
   ==========================================================================
   Ein kleiner Ablauf mit Schritten. Das Passwort wird in ZWEI Schritten
   geaendert -- erst das alte pruefen, dann das neue zweimal -- damit
   niemand an einem unbeaufsichtigten Rechner einfach umstellen kann.

   "Zuruecksetzen" loescht das Konto und alle eigenen Dateien im
   Hauptverzeichnis. Die Systemordner bleiben: der Bootsektor holt den
   Kernel als Datei aus \SYSTEM, ein echtes Formatieren machte den Rechner
   also unstartbar, bis jemand am Mac build.py aufruft.
   ========================================================================== */

#define ST_MENUE     0
#define ST_ALT       1
#define ST_NEU       2
#define ST_RESET     3
#define ST_FERTIG    4

int  st_schritt = ST_MENUE;
int  st_feld = 0;
int  st_fehler = 0;
char st_alt[32];
char st_neu[32];
char st_neu2[32];
char st_meldung[48];

void st_feldkasten(int x, int y, char* beschriftung, char* inhalt, int aktiv) {
    int i; int n;
    g_text(x, y, beschriftung, C_TEXT, 256);
    g_fill(x + 130, y - 3, 150, 14, C_WHITE);
    g_frame(x + 130, y - 3, 150, 14, aktiv ? C_ACCENT : C_WINDARK);
    n = strlen(inhalt);
    for (i = 0; i < n && i < 18; i++)
        g_char(x + 134 + i * 8, y, '*', C_TEXT, 256);
    if (aktiv) g_fill(x + 134 + n * 8, y, 7, 8, C_ACCENT);
}

void app_settings(int i) {
    int x; int y; int b;
    x = win_x[i] + 12;
    y = win_y[i] + TITLE_H + 12;
    b = win_w[i];

    if (st_schritt == ST_MENUE) {
        g_text(x, y, "Settings", C_ACCENT, 256);
        g_button(x, y + 26, 200, 20, "Change password", 0);
        g_button(x, y + 54, 200, 20, "Reset this machine", 0);
        g_text(x, y + 86, "User:", C_WINDARK, 256);
        g_text(x + 48, y + 86, benutzer_name(), C_TEXT, 256);
        if (st_meldung[0]) g_text(x, y + 104, st_meldung, C_GOOD, 256);
        return;
    }
    if (st_schritt == ST_ALT) {
        g_text(x, y, "Change password", C_ACCENT, 256);
        g_text(x, y + 22, "Enter your current password.", C_TEXT, 256);
        st_feldkasten(x, y + 48, "Current password", st_alt, 1);
        if (st_fehler) g_text(x, y + 72, "Wrong password.", C_WARN, 256);
        g_button(x + b - 190, y + 96, 80, 20, "OK", 0);
        g_button(x + b - 100, y + 96, 80, 20, "Cancel", 0);
        return;
    }
    if (st_schritt == ST_NEU) {
        g_text(x, y, "Change password", C_ACCENT, 256);
        st_feldkasten(x, y + 32, "New password", st_neu, st_feld == 0);
        st_feldkasten(x, y + 58, "Repeat", st_neu2, st_feld == 1);
        if (st_fehler) g_text(x, y + 80, "The two entries differ.", C_WARN, 256);
        else g_text(x, y + 80, "Click a field or press TAB.", C_WINDARK, 256);
        g_button(x + b - 190, y + 100, 80, 20, "Save", 0);
        g_button(x + b - 100, y + 100, 80, 20, "Cancel", 0);
        return;
    }
    if (st_schritt == ST_RESET) {
        g_text(x, y, "Reset this machine", C_WARN, 256);
        g_text(x, y + 26, "This deletes your account and every file you", C_TEXT, 256);
        g_text(x, y + 40, "created. The system itself stays, so the machine", C_TEXT, 256);
        g_text(x, y + 54, "still starts -- it will ask you to set it up again.", C_TEXT, 256);
        if (st_fehler)
            g_text_max(x, y + 76, "The SYSTEM folder is gone -- not resetting.",
                       C_WARN, 256, b - 32);
        else g_text(x, y + 76, "Are you sure?", C_WARN, 256);
        g_button(x + b - 190, y + 100, 80, 20, "Reset", 0);
        g_button(x + b - 100, y + 100, 80, 20, "Cancel", 0);
        return;
    }
    g_text(x, y, "Done. The machine restarts now.", C_GOOD, 256);
}

/* Konto und eigene Dateien loeschen -- die Systemordner bleiben stehen. */
int st_zuruecksetzen() {
    int i; int sys; int u;
    /* Alles weg ausser dem Ordner SYSTEM und seinem Inhalt. Dort liegt der
       Kernel, den der Bootsektor als Datei holt -- ohne ihn startet der
       Rechner nicht mehr, und niemand kaeme ohne den Mac wieder heran.
       Alles andere ist entweder deins oder kommt mit build.py zurueck. */
    sys = fs_find_in("SYSTEM", 0 - 1);
    /* Ohne den Ordner SYSTEM wuerde die Schleife unten genau das Falsche
       tun: sys waere -1, und -1 ist der Elternordner des HAUPTverzeichnisses.
       Geschuetzt waere dann alles oben, geloescht alles in den Ordnern --
       einschliesslich KERNEL.BIN. Der Rechner startete nie wieder. Also
       lieber gar nichts anfassen. */
    if (sys < 0 || ent_type(sys) != FT_DIR) return 0;
    for (i = 0; i < FS_MAXFILES; i++) {
        if (ent_type(i) == 0) continue;
        if (i == sys) continue;                /* der Ordner selbst */
        if (ent_parent(i) == sys) continue;    /* und was darin liegt */
        ent_setinfo(i, 0, 0 - 1);
        memset(ent_name(i), 0, 16);
    }
    /* Ein Konto gehoert nie zum System. Landete es einmal in \SYSTEM (das
       ging, solange USER.DAT im gerade offenen Ordner geschrieben wurde),
       dann ueberlebte es hier jedes Zuruecksetzen. */
    u = fs_find_in("USER.DAT", sys);
    if (u >= 0) { ent_setinfo(u, 0, 0 - 1); memset(ent_name(u), 0, 16); }
    fs_save_dir();
    /* Und zum Schluss die Probe aufs Exempel: den Kernel muss es danach
       noch geben. Sonst wird NICHT neu gestartet -- ein Rechner, der nicht
       mehr hochkommt, ist schlimmer als ein misslungenes Zuruecksetzen. */
    if (fs_find_in("KERNEL.BIN", sys) < 0) return 0;
    return 1;
}

int st_klick(int i, int mx, int my) {
    int x; int y; int b;
    x = win_x[i] + 12;
    y = win_y[i] + TITLE_H + 12;
    b = win_w[i];

    if (st_schritt == ST_MENUE) {
        if (treffer(mx, my, x, y + 26, 200, 20)) {
            st_schritt = ST_ALT;
            memset(st_alt, 0, 32); st_fehler = 0; st_meldung[0] = 0;
            return 1;
        }
        if (treffer(mx, my, x, y + 54, 200, 20)) {
            st_schritt = ST_RESET; st_fehler = 0; return 1;
        }
        return 0;
    }
    if (st_schritt == ST_ALT) {
        if (treffer(mx, my, x + b - 190, y + 96, 80, 20)) {
            if (benutzer_passt(st_alt)) {
                st_schritt = ST_NEU;
                memset(st_neu, 0, 32); memset(st_neu2, 0, 32);
                st_feld = 0; st_fehler = 0;
            } else {
                st_fehler = 1;
                memset(st_alt, 0, 32);
            }
            return 1;
        }
        if (treffer(mx, my, x + b - 100, y + 96, 80, 20)) { st_schritt = ST_MENUE; return 1; }
        return 0;
    }
    if (st_schritt == ST_NEU) {
        /* Die Felder anklickbar machen. TAB allein reicht nicht -- wer mit
           der Maus arbeitet, klickt in das Feld, das er meint. Der Kasten ist
           nur 14 Punkte hoch; getroffen wird die ganze Zeile samt Beschriftung,
           sonst klickt man daneben, ohne zu merken warum. */
        if (treffer(mx, my, x, y + 24, 280, 24)) { st_feld = 0; return 1; }
        if (treffer(mx, my, x, y + 50, 280, 24)) { st_feld = 1; return 1; }
        if (treffer(mx, my, x + b - 190, y + 100, 80, 20)) {
            if (strcmp(st_neu, st_neu2) == 0) {
                benutzer_anlegen(benutzer_name(), st_neu);
                strcpy(st_meldung, "Password changed.");
                st_schritt = ST_MENUE;
            } else {
                st_fehler = 1;
                memset(st_neu, 0, 32); memset(st_neu2, 0, 32);
                st_feld = 0;
            }
            return 1;
        }
        if (treffer(mx, my, x + b - 100, y + 100, 80, 20)) { st_schritt = ST_MENUE; return 1; }
        return 0;
    }
    if (st_schritt == ST_RESET) {
        if (treffer(mx, my, x + b - 190, y + 100, 80, 20)) {
            if (st_zuruecksetzen() == 0) { st_fehler = 1; return 1; }
            st_schritt = ST_FERTIG;
            return 2;                    /* der Aufrufer startet neu */
        }
        if (treffer(mx, my, x + b - 100, y + 100, 80, 20)) { st_schritt = ST_MENUE; return 1; }
    }
    return 0;
}

void st_taste(int k) {
    int c; int code; int n;
    char* ziel;
    c = keychar(k);
    code = keycode(k);
    if (st_schritt == ST_ALT) ziel = st_alt;
    else if (st_schritt == ST_NEU) ziel = st_feld == 0 ? st_neu : st_neu2;
    else return;
    n = strlen(ziel);
    if (code == K_TAB && st_schritt == ST_NEU) { st_feld = 1 - st_feld; return; }
    if (code == K_BACKSPACE) { if (n > 0) ziel[n - 1] = 0; return; }
    if (code == K_ESC) { st_schritt = ST_MENUE; return; }
    if (c >= 32 && c < 127 && n < 20) { ziel[n] = c; ziel[n + 1] = 0; }
}

/* --- Power options -------------------------------------------------------
   Dasselbe wie der Schalter im Anmeldeschirm, nur aus dem laufenden System
   heraus -- und mit einem dritten Weg: abmelden, ohne den Rechner
   auszuschalten. Danach steht wieder der Anmeldeschirm da.               */

int gui_abmelden = 0;                /* 1 = zurueck zum Anmeldeschirm */

void app_power(int i) {
    int x; int y;
    x = win_x[i] + 16;
    y = win_y[i] + TITLE_H + 14;
    gl_power_symbol(x, y - 2, C_TEXT);
    g_text(x + 20, y, "Power options", C_ACCENT, 256);
    g_button(x, y + 26, 180, 20, "Restart", 0);
    g_button(x, y + 52, 180, 20, "Shut down", 0);
    g_button(x, y + 78, 180, 20, "Sign out", 0);
    g_text_max(x, y + 106, "Back to the login screen.", C_WINDARK, 256,
               win_w[i] - 32);
}

/* 0 = nichts, 1 = neu gezeichnet, 2 = der Aufrufer soll den Schreibtisch
   verlassen (abmelden). */
int power_klick(int i, int mx, int my) {
    int x; int y;
    x = win_x[i] + 16;
    y = win_y[i] + TITLE_H + 14;
    if (treffer(mx, my, x, y + 26, 180, 20)) { sys_out(P_POWER, 2); return 1; }
    if (treffer(mx, my, x, y + 52, 180, 20)) { sys_out(P_POWER, 1); return 1; }
    if (treffer(mx, my, x, y + 78, 180, 20)) {
        win_type[i] = 0;
        win_voll[i] = 0;
        gui_abmelden = 1;
        gui_running = 0;
        return 2;
    }
    return 0;
}

void gui_main() {
    int mx; int my; int btn; int alt_btn; int i; int k;
    int drag; int drag_dx; int drag_dy; int neu; int letzte_sek;

    for (i = 0; i < MAXWIN; i++) { win_type[i] = 0; win_voll[i] = 0; }
    win_top = 0 - 1;
    gui_running = 1;
    drag = 0 - 1;
    /* Den aktuellen Stand uebernehmen, nicht 0 annehmen. Wer sich gerade
       mit einem Klick angemeldet hat, haelt die Taste beim ersten Bild des
       Schreibtischs womoeglich noch -- und das galt dann als frischer
       Klick: der SYSTEM-Ordner hing sofort am Mauszeiger. */
    alt_btn = sys_in(0x62) & 1;
    letzte_sek = 0 - 1;
    file_sel = 0;

    sys_setmode(1 + 256);
    sys_out(P_BLT_SRC, (int)font8);
    sys_out(P_MCUR_ON, 1);
    sys_flushkeys();

    /* Zweite Bildseite: der Schreibtisch malt ab jetzt in den unsichtbaren
       Speicher und schiebt das fertige Bild erst danach nach vorn. Vorher
       war jede halbfertige Zeichnung sofort zu sehen -- das war das
       Flackern beim Ziehen von Fenstern und beim Malen in Paint.
       Kopieren statt tauschen (Wert 2), weil oft nur EIN Fenster neu
       gemalt wird und der Rest stehen bleiben muss. */
    gui_selbst = 1;
    sys_out(P_GFX_DOPPEL, 1);
    gui_selbst = 0;

    win_open(APP_FILES, "File Manager", 40, 40, 400, 230);
    draw_desktop();
    sys_out(P_GFX_TAUSCH, 2);

    while (gui_running) {
        /* Hat das Gehaeuse um den Text des obersten Fensters gebeten?
           Dann liegt er eine Schleifenrunde spaeter bereit. Billiger als
           ihn staendig aktuell zu halten -- gefragt wird selten. */
        if (wt_wunsch) {
            wt_bauen();
            wt_wunsch = 0;
        }
        /* Ein Programm im Fenster hat in den Grafikmodus geschaltet -- es
           braucht den ganzen Bildschirm. Solange malen wir nichts und lesen
           auch keine Tasten, sonst kaempfen zwei Programme um beides. */
        if (gui_fremd == 1) {
            sys_halt();
            continue;
        }
        if (gui_fremd == 2) {                    /* Programm ist fertig */
            gui_selbst = 1;
            sys_setmode(1 + 256);
            sys_out(P_GFX_DOPPEL, 1);            /* unsere zweite Seite zurueck */
            sys_out(P_BLT_SRC, (int)font8);
            sys_out(P_MCUR_ON, 1);
            gui_selbst = 0;
            gui_fremd = 0;
            sys_flushkeys();
            draw_desktop();
        }
        mx = sys_in(0x60);
        my = sys_in(0x61);
        btn = sys_in(0x62);
        sys_out(P_MCUR_X, mx);
        sys_out(P_MCUR_Y, my);
        neu = 0;

        if (sys_haskey()) {
            k = sys_getkey();
            if (win_top >= 0 && win_type[win_top] == APP_TERM && term_lauf) {
                if (term_view) {                 /* Tippen holt nach vorn */
                    term_view = 0;
                    draw_window(win_top);
                }
                term_push_key(k);                /* geht an die Kommandozeile */
            } else if (win_top >= 0 && win_type[win_top] == APP_EDITOR) {
                if (keycode(k) == K_ESC && edg_namemode == 0) break;
                edg_taste(k);
                draw_window(win_top);
            } else if (win_top >= 0 && win_type[win_top] == APP_PAINT) {
                if (keycode(k) == K_ESC && pt_namemode == 0) break;
                pt_taste(k);
                draw_window(win_top);
            } else if (win_top >= 0 && win_type[win_top] == APP_BIOSHILFE) {
                /* Blaettern war nie angeschlossen -- das Fenster sagte
                   "PgUp/PgDn scroll" und tat nichts. */
                if (keycode(k) == K_PGDN) bh_top = bh_top + 8;
                else if (keycode(k) == K_PGUP) bh_top = bh_top - 8;
                else if (keycode(k) == K_DOWN) bh_top++;
                else if (keycode(k) == K_UP) bh_top--;
                else if (keycode(k) == K_HOME) bh_top = 0;
                else if (keycode(k) == K_ESC) {
                    win_type[win_top] = 0; win_voll[win_top] = 0;
                    neu = 1;
                }
                if (bh_top < 0) bh_top = 0;
                if (bh_top > 24) bh_top = 24;
                if (neu == 0) draw_window(win_top);
            } else if (win_top >= 0 && win_type[win_top] == APP_SETTINGS) {
                st_taste(k);
                draw_window(win_top);
            } else if (win_top >= 0 && win_type[win_top] == APP_DIALOG) {
                dlg_taste(k);
                neu = 1;
            } else if (win_top >= 0 && win_type[win_top] == APP_WORD) {
                if (keycode(k) == K_ESC && wd_namemode == 0) break;
                wd_taste(k);
                draw_window(win_top);
            } else if (keycode(k) == K_ESC) {
                break;
            }
        }

        /* Laeuft gerade eine Uebersetzung? Balken auffrischen und schauen,
           ob der Vorgang fertig ist. */
        if (edg_build) {
            i = win_find(APP_BUILD);
            if (edg_pid < 0 || p_state[edg_pid] == PS_FREI) {
                edg_build = 0;
                cap_aktiv = 0;
                /* Bei Fehlern bleibt das Fenster stehen und zeigt sie an --
                   sonst verschwindet es wie bisher. */
                if (i >= 0 && fs_find(edg_ziel) >= 0) {
                    win_type[i] = 0;
                    win_voll[i] = 0;
                    i = 0 - 1;
                } else if (i >= 0) {
                    /* Aus dem schmalen Fortschrittsfenster wird ein
                       Meldungsfenster. 320x90 reichten fuer einen Balken,
                       nicht fuer Compilerzeilen -- die sind gut 50 Zeichen
                       lang und ragten rechts heraus. */
                    strncpy(win_title(i), "Compiler messages", 18);
                    win_w[i] = 520;
                    win_h[i] = 240;
                    if (win_x[i] + win_w[i] > G_W) win_x[i] = G_W - win_w[i] - 4;
                    if (win_y[i] + win_h[i] > BAR_Y) win_y[i] = BAR_Y - win_h[i] - 4;
                    if (win_x[i] < 0) win_x[i] = 4;
                    if (win_y[i] < 0) win_y[i] = 4;
                }
                neu = 1;
                /* War es ein Firmware-Bau? Dann geht es nicht ums Starten,
                   sondern ums Pruefen und Nachfragen. */
                if (bios_wartet && fs_find(edg_ziel) >= 0) {
                    bios_fertig();
                    if (i >= 0 && bios_frage) { win_type[i] = 0; win_voll[i] = 0; }
                    neu = 1;
                } else if (fs_find(edg_ziel) >= 0) {
                    edg_meldung = 2;
                    if (edg_run_danach) {        /* Run hatte darauf gewartet */
                        edg_run_danach = 0;
                        gui_im_fenster(edg_ziel);   /* Ausgabe ins Fenster */
                        neu = 1;
                    }
                } else {
                    edg_meldung = 3;
                    edg_run_danach = 0;
                    bios_wartet = 0;
                    /* Direkt an die Stelle springen, die der Compiler
                       genannt hat -- sonst sucht man sie von Hand. */
                    cod_zur_fehlerzeile();
                }
            }
            if (i >= 0 && neu == 0) draw_window(i);
        }

        /* Hat die Kommandozeile etwas geschrieben? Dann neu malen. */
        if (term_dirty) {
            i = win_find(APP_TERM);
            if (i >= 0) draw_window(i);
            term_dirty = 0;
        }

        if (btn && alt_btn == 0) {
            gui_taste = btn;
            /* Erst das Startmenue, falls es offen ist */
            if (menu_offen) {
                k = BAR_Y - (MENU_ANZ * MENU_ZH + 10);
                if (mx >= MENU_X && mx < MENU_X + MENU_W &&
                    my >= k && my < BAR_Y) {
                    i = (my - k - 4) / MENU_ZH;
                    menu_offen = 0;
                    if (i == 0) starte(APP_FILES, "File Manager", 400, 230);
                    if (i == 1) {
                        starte(APP_TERM, "Command Prompt", 580, 230);
                        if (term_lauf == 0) {
                            if (mt_active == 0) mt_enable();
                            term_pid = proc_start("cmd", (int)term_main);
                            if (term_pid >= 0) term_lauf = 1;
                        }
                    }
                    if (i == 2) {
                        if (win_find(APP_EDITOR) < 0) {
                            edg_screen = 0;       /* erst fragen, was ansteht */
                            edg_liste_top = 0;
                        }
                        starte(APP_EDITOR, "Coder", 596, 292);
                    }
                    if (i == 3) starte(APP_MONITOR, "System Monitor", 320, 230);
                    if (i == 4) starte(APP_CONTROL, "Control Panel", 320, 190);
                    if (i == 5) {
                        if (win_find(APP_PAINT) < 0) pt_init();
                        starte(APP_PAINT, "Paint", 536, 356);
                    }
                    if (i == 6) {
                        if (win_find(APP_WORD) < 0) wd_init();
                        starte(APP_WORD, "Word", 600, 330);
                    }
                    if (i == 7) starte(APP_CLOCK, "Clock", 200, 130);
                    if (i == 8) {
                        st_schritt = ST_MENUE;
                        st_meldung[0] = 0;
                        starte(APP_SETTINGS, "Settings", 420, 200);
                    }
                    if (i == 9) starte(APP_ABOUT, "About TOOBAD-OS", 340, 150);
                    if (i == 10) starte(APP_POWER, "Power", 240, 190);
                    if (i == 11) gui_running = 0;
                    /* Selbst neu zeichnen: das continue unten springt am
                       "if (neu) draw_desktop()" am Schleifenende vorbei, und
                       dann bliebe das Menue stehen, bis man irgendwo anders
                       hinklickt. */
                    if (gui_running) draw_desktop();
                    alt_btn = btn;
                    continue;
                }
                menu_offen = 0;                      /* daneben geklickt */
                neu = 1;
            }

            if (my >= BAR_Y) {
                if (treffer(mx, my, 2, BAR_Y + 2, 52, 18)) {
                    menu_offen = 1 - menu_offen;
                    neu = 1;
                } else {
                    /* Knopf eines offenen Fensters? -> nach vorne holen */
                    k = 58;
                    for (i = 0; i < MAXWIN; i++) {
                        if (win_type[i] == 0) continue;
                        if (k + 66 > G_W - 80) break;
                        if (treffer(mx, my, k, BAR_Y + 2, 64, 18)) {
                            win_top = i;
                            neu = 1;
                            break;
                        }
                        k = k + 66;
                    }
                }
            } else if (win_unter(mx, my) < 0) {
                /* Freie Flaeche oder ein Symbol darauf */
                i = desk_treffer(mx, my);
                if (i != desk_sel) { desk_sel = i; neu = 1; }
                if (i >= 0) {
                    /* Doppelklick = oeffnen. Zwei Klicks auf dasselbe Symbol
                       innerhalb einer halben Sekunde. */
                    if (klick_was == 1000 + i && sys_ticks() - klick_zeit < 50) {
                        eintrag_oeffnen(desk_index(i));
                        klick_was = 0 - 1;
                        neu = 1;
                    } else {
                        klick_was = 1000 + i;
                        klick_zeit = sys_ticks();
                        zieh_idx = desk_index(i);   /* Symbol laesst sich ziehen */
                        zieh_von = 0 - 1;
                        zieh_sym = i;               /* ... und verschieben */
                        zieh_dx = mx - desk_x(i);
                        zieh_dy = my - desk_y(i);
                    }
                }
            } else {
                /* Reihenfolge wie beim Zeichnen: erst das vorderste Fenster,
                   dann der Rest. Vorher lief die Schleife stur nach
                   Fensternummer rueckwaerts -- lag das vorderste Fenster auf
                   einem Platz mit kleinerer Nummer, bekam das Fenster
                   DARUNTER den Klick, obwohl man sichtbar den Knopf oben
                   getroffen hatte. */
                /* RUECKWAERTS durch die Fensternummern, denn genau so malt
                   draw_desktop(): erst 0, 1, 2 ... und win_top zuletzt. Wer
                   die hoehere Nummer hat, liegt sichtbar weiter vorn.
                   Vorwaerts gesucht bekam das Fenster DAHINTER den Klick --
                   der Command Prompt war nicht mehr anklickbar, sobald ein
                   Fenster mit kleinerer Nummer darunter lag. */
                for (k = 0 - 1; k < MAXWIN; k++) {
                    if (k == 0 - 1) i = win_top;
                    else            i = MAXWIN - 1 - k;
                    if (k != 0 - 1 && i == win_top) continue;
                    if (i < 0 || win_type[i] == 0) continue;
                    if (treffer(mx, my, win_x[i], win_y[i], win_w[i], win_h[i])) {
                        if (win_top != i) { win_top = i; neu = 1; }
                        if (treffer(mx, my, win_x[i] + win_w[i] - 12,
                                    win_y[i] + win_h[i] - 12, 12, 12)
                            && win_voll[i] == 0) {
                            groesse_zieht = i;       /* Ecke gepackt */
                        } else if (treffer(mx, my, win_x[i] + win_w[i] - 30,
                                           win_y[i] + 3, 12, 11)) {
                            win_vollbild(i);
                            neu = 1;
                        } else if (treffer(mx, my, win_x[i] + win_w[i] - 16,
                                    win_y[i] + 3, 12, 11)) {
                            if (win_type[i] == APP_TERM && term_lauf) {
                                if (term_pid >= 0) p_state[term_pid] = PS_FREI;
                                term_lauf = 0;
                                term_aktiv = 0;
                            }
                            win_type[i] = 0;
                            win_voll[i] = 0;
                            neu = 1;
                        } else if (my < win_y[i] + TITLE_H) {
                            drag = i;
                            drag_dx = mx - win_x[i];
                            drag_dy = my - win_y[i];
                        } else if (win_type[i] == APP_EDITOR) {
                            if (edg_klick(i, mx, my)) neu = 1;
                        } else if (win_type[i] == APP_FILES) {
                            k = files_click(i, mx, my);
                            if (k) neu = 1;
                            if (k == 2) {                 /* auf einer Zeile */
                                if (klick_was == file_sel
                                    && sys_ticks() - klick_zeit < 50) {
                                    eintrag_oeffnen(file_index(file_sel));
                                    klick_was = 0 - 1;
                                } else {
                                    klick_was = file_sel;
                                    klick_zeit = sys_ticks();
                                    zieh_idx = file_index(file_sel);
                                    zieh_von = i;
                                }
                            }
                        } else if (win_type[i] == APP_CONTROL) {
                            control_click(i, mx, my);
                            neu = 1;
                        } else if (win_type[i] == APP_PAINT) {
                            if (pt_klick(i, mx, my)) neu = 1;
                        } else if (win_type[i] == APP_WORD) {
                            if (wd_klick(i, mx, my)) neu = 1;
                        } else if (win_type[i] == APP_DIALOG) {
                            if (dlg_klick(i, mx, my)) neu = 1;
                        } else if (win_type[i] == APP_POWER) {
                            if (power_klick(i, mx, my)) neu = 1;
                        } else if (win_type[i] == APP_SETTINGS) {
                            k = st_klick(i, mx, my);
                            if (k == 2) {          /* zurueckgesetzt */
                                sys_out(P_POWER, 2);
                            }
                            if (k) neu = 1;
                        } else if (win_type[i] == APP_BIOSFRAGE) {
                            if (treffer(mx, my, win_x[i] + win_w[i] - 180,
                                        win_y[i] + TITLE_H + 92, 84, 18)) {
                                win_type[i] = 0; win_voll[i] = 0;
                                bios_los();
                            } else if (treffer(mx, my, win_x[i] + win_w[i] - 90,
                                               win_y[i] + TITLE_H + 92, 80, 18)) {
                                win_type[i] = 0; win_voll[i] = 0;
                                bios_frage = 0;
                            }
                            neu = 1;
                        }
                        break;
                    }
                }
            }
        }

        /* Symbol wandert mit der Maus mit */
        if (zieh_sym >= 0 && zieh_idx >= 0 && btn) {
            i = mx - zieh_dx;
            k = my - zieh_dy;
            if (i != desk_x(zieh_sym) || k != desk_y(zieh_sym)) {
                desk_setzen(zieh_idx, i, k);
                neu = 1;
            }
        }

        /* Ziehen beendet: Datei dorthin verschieben, wo die Maus losgelassen
           wurde. Auf den freien Schreibtisch heisst: in den Ordner DESKTOP.
           Auf ein Dateifenster heisst: in dessen aktuellen Ordner. */
        if (zieh_idx >= 0 && btn == 0) {
            i = win_unter(mx, my);
            if (my < BAR_Y) {
                if (i < 0) {
                    if (ent_parent(zieh_idx) != desk_ordner()) {
                        /* aus einem Fenster auf den Schreibtisch gezogen */
                        fs_move(zieh_idx, desk_ordner());
                        desk_setzen(zieh_idx, mx - 34, my - 20);
                        icon_speichern();
                        neu = 1;
                    } else if (zieh_sym >= 0) {
                        icon_speichern();      /* nur umgelegt */
                    }
                } else if (win_type[i] == APP_FILES
                           && ent_parent(zieh_idx) != cwd) {
                    fs_move(zieh_idx, cwd);
                    icon_pos[zieh_idx] = 0;    /* liegt nicht mehr hier */
                    icon_speichern();
                    file_sel = 0;
                    file_top = 0;
                    neu = 1;
                }
            }
            zieh_idx = 0 - 1;
            zieh_von = 0 - 1;
            zieh_sym = 0 - 1;
        }

        /* Mausrad: im Editorfenster blaettern */
        k = sys_in(P_MOUSE_WHEEL);
        if (k) {
            if (k > 1000) k = k - 65536;         /* negativ zurueckrechnen */
            i = win_find(APP_TERM);
            if (i >= 0 && i == win_top) {
                term_view = term_view + k * 3;
                if (term_view > term_sb_count) term_view = term_sb_count;
                if (term_view < 0) term_view = 0;
                draw_window(i);
            }
            i = win_find(APP_FILES);
            if (i >= 0 && i == win_top) {
                file_top = file_top - k * 3;
                if (file_top > file_anzahl() - file_rows)
                    file_top = file_anzahl() - file_rows;
                if (file_top < 0) file_top = 0;
                if (file_sel < file_top) file_sel = file_top;
                if (file_sel >= file_top + file_rows)
                    file_sel = file_top + file_rows - 1;
                draw_window(i);
            }
            i = win_find(APP_EDITOR);
            if (i >= 0 && i == win_top && edg_screen == 0) {
                /* Nach unten nur so weit, bis die letzte Datei sichtbar ist --
                   sonst scrollt man endlos in ein leeres Feld. */
                edg_liste_top = edg_liste_top - k * 3;
                if (edg_liste_top > file_anzahl() - es_rows)
                    edg_liste_top = file_anzahl() - es_rows;
                if (edg_liste_top < 0) edg_liste_top = 0;
                draw_window(i);
            } else if (i >= 0 && i == win_top) {
                edg_folgen = 0;
                edg_top = edg_top - k * 3;
                if (edg_top < 0) edg_top = 0;
                if (edg_top > ed_line_of(ed_len)) edg_top = ed_line_of(ed_len);
                draw_window(i);
            }
        }

        /* Auswahl im Editor mit gedrueckter Maustaste aufziehen */
        if (edg_zieht) {
            if (btn == 0) {
                edg_zieht = 0;
                if (ed_sel_bis == ed_sel_von) { ed_sel_von = 0 - 1; ed_sel_bis = 0 - 1; }
            } else {
                i = win_find(APP_EDITOR);
                if (i >= 0) {
                    k = edg_pos_aus_maus(i, mx, my);
                    if (k != ed_sel_bis) {
                        if (k < ed_sel_von) { ed_sel_bis = ed_sel_von; ed_sel_von = k; }
                        else ed_sel_bis = k;
                        ed_pos = k;
                        draw_window(i);
                    }
                }
            }
        }

        /* Groesse ziehen */
        if (groesse_zieht >= 0) {
            if (btn == 0) {
                groesse_zieht = 0 - 1;
            } else {
                i = mx - win_x[groesse_zieht] + 6;
                k = my - win_y[groesse_zieht] + 6;
                if (i < 160) i = 160;
                if (k < 80) k = 80;
                if (win_x[groesse_zieht] + i > G_W) i = G_W - win_x[groesse_zieht];
                if (win_y[groesse_zieht] + k > BAR_Y) k = BAR_Y - win_y[groesse_zieht];
                if (i != win_w[groesse_zieht] || k != win_h[groesse_zieht]) {
                    win_w[groesse_zieht] = i;
                    win_h[groesse_zieht] = k;
                    neu = 1;
                }
            }
        }

        /* Zeichnen mit gedrueckter Maustaste. Der Schreibtisch kannte bisher
           nur den Klick -- ein Malprogramm braucht aber die ganze Bewegung
           und den Moment des Loslassens. */
        if (wd_zieht || wd_griff) {
            if (btn == 0) { wd_loslassen(); neu = 1; }
            else {
                wd_ziehen(mx, my);
                i = win_find(APP_WORD);
                if (i >= 0) draw_window(i);
            }
        }

        if (pt_zieht) {
            if (btn == 0) {
                pt_loslassen();
                neu = 1;
            } else {
                pt_ziehen(mx, my);
                i = win_find(APP_PAINT);
                if (i >= 0) {
                    /* Nur die Leinwand, nicht das ganze Fenster -- sonst
                       flackern Knoepfe und Palette bei jeder Mausbewegung. */
                    pt_leinwand_malen(i);
                    pt_vorschau(i);      /* Figur im Entstehen, nur am Schirm */
                }
            }
        }

        if (drag >= 0) {
            if (btn == 0) {
                drag = 0 - 1;
            } else {
                i = mx - drag_dx;
                k = my - drag_dy;
                if (i < 0) i = 0;
                if (k < 0) k = 0;
                if (i > G_W - win_w[drag]) i = G_W - win_w[drag];
                if (k > BAR_Y - 20) k = BAR_Y - 20;
                if (i != win_x[drag] || k != win_y[drag]) {
                    win_x[drag] = i;
                    win_y[drag] = k;
                    neu = 1;
                }
            }
        }

        /* Uhr und Monitor einmal pro Sekunde auffrischen */
        k = sys_clock() & 255;
        if (k != letzte_sek && gui_fremd == 0) {
            letzte_sek = k;
            /* Uhr und Monitor NICHT einzeln malen: sie wuerden ihren
               Inhalt ueber jedes Fenster legen, das vor ihnen liegt --
               die Uhrzeit stand mitten im Control Panel. Stattdessen malt
               der Schreibtisch neu, und der kennt die Reihenfolge. */
            /* Das Control Panel gehoert dazu: es zeigt die Temperatur, und
               die stand bisher still, bis irgendetwas anderes ein
               Neuzeichnen ausloeste. */
            if (win_find(APP_CLOCK) >= 0 || win_find(APP_MONITOR) >= 0
                || win_find(APP_CONTROL) >= 0)
                neu = 1;
            else
                draw_taskbar();
        }

        /* Erst hier entscheidet sich, ob wirklich gemalt wird. Mitten in
           dieser Runde kann ein gestartetes Programm auf Vollbild geschaltet
           haben -- die Pruefung ganz oben kommt dafuer eine Runde zu spaet,
           und der Schreibtisch hat dem laufenden Spiel seine Fenster ins
           Bild gemalt. Also unmittelbar vor dem Malen nachsehen. */
        if (neu && gui_fremd == 0) draw_desktop();

        /* Fertiges Bild nach vorn. Kostet einen Speicherblock je Runde und
           macht dafuer jedes Flackern weg -- man sieht nur noch fertige
           Bilder, nie einen halb gemalten Zustand. */
        if (gui_fremd == 0) sys_out(P_GFX_TAUSCH, 2);

        alt_btn = btn;
        sys_halt();
    }

    gui_selbst = 1;
    sys_out(P_GFX_DOPPEL, 0);
    gui_selbst = 0;

    /* Beim Verlassen der Oberflaeche muss die Kommandozeile im Fenster
       aufhoeren -- sonst liefe sie unsichtbar weiter und wuerde die
       Ausgaben des Textmodus abfangen. */
    if (term_lauf && term_pid >= 0) {
        p_state[term_pid] = PS_FREI;
        term_lauf = 0;
    }
    term_aktiv = 0;
    /* Muss hier stehen und nicht nur beim Menuepunkt "Exit": aus der Schleife
       kommt man auch mit ESC heraus. Sonst haelt der Shell-Befehl WIN den
       Schreibtisch faelschlich fuer noch laufend. */
    gui_running = 0;

    sys_out(P_MCUR_ON, 0);
    sys_setmode(0 + 256);
    sys_cls(NORMAL);
    sys_setcursor(0, 0);
}
