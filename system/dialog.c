/* ==========================================================================
   Der Dateiauswahl-Dialog

   Bis jetzt hatte jedes Programm ein Textfeld fuer den Dateinamen: man
   musste wissen, wie die Datei heisst und in welchem Ordner sie liegt.
   Das ist der Stand von 1981.

   Ab hier gibt es EIN Fenster, das alle Programme benutzen: Coder, Paint
   und Word fragen danach, und es liefert einen Namen zurueck. Genau so
   machen es richtige Oberflaechen -- der Dialog gehoert dem System, nicht
   dem einzelnen Programm.

   Der Rueckweg laeuft ueber `dlg_ziel`: das Fenster merkt sich, wer
   gefragt hat, und ruft beim Klick auf OK die passende Funktion dort auf.
   ========================================================================== */

#define DLG_OEFFNEN   0
#define DLG_SPEICHERN 1
#define DLG_BILD      2              /* Word holt ein Bild zum Einfuegen */

int dlg_modus = DLG_OEFFNEN;
int dlg_ziel = 0;                    /* APP_EDITOR, APP_WORD */
int dlg_sel = 0;                     /* markierte Zeile */
int dlg_top = 0;                     /* erste sichtbare Zeile */
int dlg_zeilen = 12;
char dlg_name[24];
char dlg_endung[8];                  /* nur Dateien mit dieser Endung, "" = alle */
/* 1 = hier wird gerade eine NEUE Datei angelegt. Dann entsteht das
   Dokument erst, wenn ein Platz gewaehlt ist -- bricht man ab, bleibt
   alles, wie es war. Ohne das legte "New" schon ein leeres Blatt an und
   der Abbruch loeschte einem die offene Arbeit. */
int dlg_neu = 0;

/* Passt der Eintrag zum Filter? Ordner immer -- man muss ja hinnavigieren. */
int dlg_passt(int idx) {
    if (ent_type(idx) == FT_DIR) return 1;
    if (dlg_endung[0] == 0) return 1;
    return endet_auf(ent_name(idx), dlg_endung);
}

int dlg_anzahl() {
    int i; int n;
    n = 0;
    for (i = 0; i < file_anzahl(); i++)
        if (dlg_passt(file_index(i))) n++;
    return n;
}

/* Die n-te sichtbare Zeile -> Eintrag im Dateisystem */
int dlg_eintrag(int zeile) {
    int i; int n;
    n = 0;
    for (i = 0; i < file_anzahl(); i++) {
        if (!dlg_passt(file_index(i))) continue;
        if (n == zeile) return file_index(i);
        n++;
    }
    return 0 - 1;
}

/* Wer fragt, sagt womit. endung = "" heisst: alles anzeigen. */
void dlg_oeffne(int ziel, int modus, char* endung, char* vorschlag) {
    dlg_ziel = ziel;
    dlg_modus = modus;
    dlg_sel = 0;
    dlg_top = 0;
    strncpy(dlg_endung, endung, 6);
    strncpy(dlg_name, vorschlag, 20);
    starte(APP_DIALOG, "File", 380, 250);
}

/* --- Malen ---------------------------------------------------------------- */

void app_dialog(int i) {
    int x; int y; int b; int h; int z; int idx; int n;

    x = win_x[i];
    y = win_y[i] + TITLE_H;
    b = win_w[i];
    h = win_h[i] - TITLE_H;
    dlg_zeilen = (h - 74) / 12;
    if (dlg_zeilen < 3) dlg_zeilen = 3;

    if (dlg_modus == DLG_SPEICHERN)  g_text(x + 8, y + 6, "Save as", C_TEXT, 256);
    else if (dlg_modus == DLG_BILD)  g_text(x + 8, y + 6, "Picture", C_TEXT, 256);
    else                             g_text(x + 8, y + 6, "Open", C_TEXT, 256);

    fs_path(gui_pfad);
    g_text(x + 90, y + 6, gui_pfad, C_ACCENT, 256);
    g_button(x + b - 54, y + 3, 46, 16, "Up", 0);

    /* Liste */
    g_fill(x + 8, y + 24, b - 16, dlg_zeilen * 12 + 4, C_WHITE);
    g_frame(x + 8, y + 24, b - 16, dlg_zeilen * 12 + 4, C_WINDARK);

    n = dlg_anzahl();
    if (dlg_sel >= n) dlg_sel = n - 1;
    if (dlg_sel < 0) dlg_sel = 0;
    if (dlg_sel < dlg_top) dlg_top = dlg_sel;
    if (dlg_sel >= dlg_top + dlg_zeilen) dlg_top = dlg_sel - dlg_zeilen + 1;

    for (z = 0; z < dlg_zeilen; z++) {
        idx = dlg_eintrag(dlg_top + z);
        if (idx < 0) break;
        if (dlg_top + z == dlg_sel)
            g_fill(x + 10, y + 26 + z * 12, b - 20, 12, C_TITLEBAR);
        if (ent_type(idx) == FT_DIR) {
            g_text(x + 12, y + 28 + z * 12, ent_name(idx),
                   dlg_top + z == dlg_sel ? C_WHITE : C_ACCENT, 256);
            g_text(x + b - 60, y + 28 + z * 12, "DIR",
                   dlg_top + z == dlg_sel ? C_WHITE : C_ACCENT, 256);
        } else {
            g_text(x + 12, y + 28 + z * 12, ent_name(idx),
                   dlg_top + z == dlg_sel ? C_WHITE : C_TEXT, 256);
            g_num(x + b - 90, y + 28 + z * 12, ent_size(idx),
                  dlg_top + z == dlg_sel ? C_WHITE : C_WINDARK, 256);
        }
    }

    /* Namensfeld und Knoepfe */
    z = y + 24 + dlg_zeilen * 12 + 10;
    /* "Cancel" sind sechs Zeichen, also 48 Punkte Text -- in einem 44 Punkte
       breiten Knopf ragte das Wort rechts heraus, bis an den Fensterrand.
       g_button zentriert nur, es kuerzt nichts. Breiten also nachrechnen,
       nicht schaetzen: 8 Punkte Rand, dann Cancel (56), dann OK (44). */
    g_text(x + 8, z + 2, "Name:", C_TEXT, 256);
    g_fill(x + 54, z, b - 176, 12, C_WHITE);
    g_frame(x + 54, z, b - 176, 12, C_WINDARK);
    g_text(x + 57, z + 2, dlg_name, C_TEXT, 256);
    g_fill(x + 57 + strlen(dlg_name) * 8, z + 2, 7, 8, C_ACCENT);

    g_button(x + b - 114, z - 2, 44, 18, "OK", 0);
    g_button(x + b - 64, z - 2, 56, 18, "Cancel", 0);
}

/* --- Ergebnis zurueckgeben ------------------------------------------------ */

void dlg_schliessen() {
    int i;
    dlg_neu = 0;                     /* abgebrochen: nichts wurde angelegt */
    i = win_find(APP_DIALOG);
    if (i >= 0) { win_type[i] = 0; win_voll[i] = 0; }
    /* Die Tastatur gehoert wieder dem Programm, das gefragt hat. Ohne das
       zeigt win_top auf ein geschlossenes Fenster und jeder Tastendruck
       versickert. */
    i = win_find(dlg_ziel);
    if (i >= 0) win_top = i;
}

/* Der eigentliche Rueckweg: das Fenster ruft beim Auftraggeber die
   passende Funktion auf. Deshalb muss kein Programm auf ein Ergebnis
   warten -- es bekommt es einfach geliefert. */
void dlg_fertig() {
    if (dlg_name[0] == 0) return;

    /* Erst jetzt entsteht das neue Dokument -- der Platz steht fest. */
    if (dlg_neu) {
        dlg_neu = 0;
        if (dlg_ziel == APP_EDITOR) edg_neu_anlegen();
        if (dlg_ziel == APP_WORD)   wd_neu_anlegen();
    }

    if (dlg_ziel == APP_EDITOR) {
        if (dlg_modus == DLG_SPEICHERN) {
            memset(edg_name, 0, 20);
            strncpy(edg_name, dlg_name, 18);
            syn_sprache(edg_name);
            edg_ort = 1;
            edg_speichern();
        } else {
            /* Auch eine geoeffnete Datei hat ihren Platz -- danach
               speichert "Save" ohne Nachfrage dorthin zurueck. */
            edg_ort = 1;
            edg_oeffnen(dlg_name);
        }
    } else if (dlg_ziel == APP_WORD) {
        if (dlg_modus == DLG_BILD) {
            wd_bild_einfuegen_name(dlg_name);
        } else {
            memset(wd_name, 0, 20);
            strncpy(wd_name, dlg_name, 20);
            if (dlg_modus == DLG_SPEICHERN) { wd_ort = 1; wd_speichern(); }
            else                            { wd_ort = 1; wd_laden(); }
        }
    }
    dlg_schliessen();
}

/* --- Bedienung ------------------------------------------------------------ */

int dlg_klick(int i, int mx, int my) {
    int x; int y; int b; int h; int z; int idx;
    x = win_x[i];
    y = win_y[i] + TITLE_H;
    b = win_w[i];
    h = win_h[i] - TITLE_H;

    if (treffer(mx, my, x + b - 54, y + 3, 46, 16)) {
        fs_chdir("..");
        dlg_sel = 0;
        dlg_top = 0;
        return 1;
    }

    z = y + 24 + dlg_zeilen * 12 + 10;
    if (treffer(mx, my, x + b - 114, z - 2, 44, 18)) { dlg_fertig(); return 1; }
    if (treffer(mx, my, x + b - 64, z - 2, 56, 18))  { dlg_schliessen(); return 1; }

    if (treffer(mx, my, x + 8, y + 24, b - 16, dlg_zeilen * 12 + 4)) {
        z = (my - y - 26) / 12 + dlg_top;
        idx = dlg_eintrag(z);
        if (idx < 0) return 1;
        dlg_sel = z;
        if (ent_type(idx) == FT_DIR) {
            /* In den Ordner hinein -- ein Klick genuegt, das ist ein
               Auswahlfenster und kein Dateimanager. */
            fs_chdir(ent_name(idx));
            dlg_sel = 0;
            dlg_top = 0;
            return 1;
        }
        memset(dlg_name, 0, 20);
        strncpy(dlg_name, ent_name(idx), 20);
        /* Beim Oeffnen ist der Klick auf eine Datei schon die Antwort. */
        if (dlg_modus != DLG_SPEICHERN) dlg_fertig();
        return 1;
    }
    return 0;
}

void dlg_taste(int k) {
    int c; int code; int n;
    c = keychar(k);
    code = keycode(k);
    n = strlen(dlg_name);

    if (code == K_ENTER) { dlg_fertig(); return; }
    if (code == K_ESC)   { dlg_schliessen(); return; }
    if (code == K_UP)    { if (dlg_sel > 0) dlg_sel--; return; }
    if (code == K_DOWN)  { if (dlg_sel < dlg_anzahl() - 1) dlg_sel++; return; }
    if (code == K_BACKSPACE) { if (n > 0) dlg_name[n - 1] = 0; return; }
    if (c >= 32 && c < 127 && n < 20) {
        dlg_name[n] = toupper(c);
        dlg_name[n + 1] = 0;
    }
}
