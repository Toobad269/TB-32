/* ==========================================================================
   SETTINGS  --  Passwort aendern und den Rechner zuruecksetzen

   Frueher ein Fenster im Kernel, jetzt ein Programm. Beides, was es tut,
   geht ueber Systemaufrufe: das Passwort pruefen und setzen, und das
   Zuruecksetzen. Aendern und Zuruecksetzen gehen erst, wenn das alte
   Passwort in diesem Lauf einmal richtig genannt wurde -- der Kernel merkt
   sich das, nicht dieses Programm.

   Uebersetzen auf dem Geraet selbst:  CC SETTINGS.C
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
#define C_WINBG    7

#define K_BACKSPACE 14
#define K_TAB       15

#define ST_MENUE   0
#define ST_ALT     1
#define ST_NEU     2
#define ST_RESET   3
#define ST_FERTIG  4

int  st_schritt = ST_MENUE;
int  st_feld = 0;
int  st_fehler = 0;
char st_alt[32];
char st_neu[32];
char st_neu2[32];
char st_meldung[48];

void gx_num(int x, int y, int n, int farbe, int bg) { gx_zahl(x, y, n, farbe); }

int treffer(int mx, int my, int x, int y, int w, int h) {
    return mx >= x && mx < x + w && my >= y && my < y + h;
}

void p_knopf(int x, int y, int w, int h, char* text, int gedrueckt) {
    gx_panel(x, y, w, h, gedrueckt);
    if (text[0])
        gx_text_mitte(x + gedrueckt, y + (h - 8) / 2 + gedrueckt, w, text, C_TEXT);
}

/* Der Rechner startet nach dem Zuruecksetzen neu -- Port 0x90, Wert 2. */
#define P_POWER 0x90

void st_feldkasten(int x, int y, char* beschriftung, char* inhalt, int aktiv) {
    int i; int n;
    gx_text(x, y, beschriftung, C_TEXT, 256);
    gx_fill(x + 130, y - 3, 150, 14, C_WHITE);
    gx_frame(x + 130, y - 3, 150, 14, aktiv ? C_ACCENT : C_WINDARK);
    n = strlen(inhalt);
    for (i = 0; i < n && i < 18; i++)
        gx_char(x + 134 + i * 8, y, '*', C_TEXT, 256);
    if (aktiv) gx_fill(x + 134 + n * 8, y, 7, 8, C_ACCENT);
}

void app_settings(int i) {
    int x; int y; int b;
    x = 12;
    y = 12;
    b = fn_breite;
    gx_fill(0, 0, fn_breite, fn_hoehe, C_WINBG);

    if (st_schritt == ST_MENUE) {
        gx_text(x, y, "Settings", C_ACCENT, 256);
        p_knopf(x, y + 26, 200, 20, "Change password", 0);
        p_knopf(x, y + 54, 200, 20, "Reset this machine", 0);
        gx_text(x, y + 86, "User:", C_WINDARK, 256);
        gx_text(x + 48, y + 86, benutzer_name(), C_TEXT, 256);
        if (st_meldung[0]) gx_text(x, y + 104, st_meldung, C_GOOD, 256);
        return;
    }
    if (st_schritt == ST_ALT) {
        gx_text(x, y, "Change password", C_ACCENT, 256);
        gx_text(x, y + 22, "Enter your current password.", C_TEXT, 256);
        st_feldkasten(x, y + 48, "Current password", st_alt, 1);
        if (st_fehler) gx_text(x, y + 72, "Wrong password.", C_WARN, 256);
        p_knopf(x + b - 190, y + 96, 80, 20, "OK", 0);
        p_knopf(x + b - 100, y + 96, 80, 20, "Cancel", 0);
        return;
    }
    if (st_schritt == ST_NEU) {
        gx_text(x, y, "Change password", C_ACCENT, 256);
        st_feldkasten(x, y + 32, "New password", st_neu, st_feld == 0);
        st_feldkasten(x, y + 58, "Repeat", st_neu2, st_feld == 1);
        if (st_fehler) gx_text(x, y + 80, "The two entries differ.", C_WARN, 256);
        else gx_text(x, y + 80, "Click a field or press TAB.", C_WINDARK, 256);
        p_knopf(x + b - 190, y + 100, 80, 20, "Save", 0);
        p_knopf(x + b - 100, y + 100, 80, 20, "Cancel", 0);
        return;
    }
    if (st_schritt == ST_RESET) {
        gx_text(x, y, "Reset this machine", C_WARN, 256);
        gx_text(x, y + 26, "This deletes your account and every file you", C_TEXT, 256);
        gx_text(x, y + 40, "created. The system itself stays, so the machine", C_TEXT, 256);
        gx_text(x, y + 54, "still starts -- it will ask you to set it up again.", C_TEXT, 256);
        if (st_fehler)
            gx_text_max(x, y + 76, "The SYSTEM folder is gone -- not resetting.",
                       C_WARN, 256, b - 32);
        else gx_text(x, y + 76, "Are you sure?", C_WARN, 256);
        p_knopf(x + b - 190, y + 100, 80, 20, "Reset", 0);
        p_knopf(x + b - 100, y + 100, 80, 20, "Cancel", 0);
        return;
    }
    gx_text(x, y, "Done. The machine restarts now.", C_GOOD, 256);
}

int st_klick(int i, int mx, int my) {
    int x; int y; int b;
    x = 12;
    y = 12;
    b = fn_breite;
    gx_fill(0, 0, fn_breite, fn_hoehe, C_WINBG);

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
            if (passwort_pruefen(st_alt)) {
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
                passwort_setzen(st_neu);
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
            if (pc_zuruecksetzen() == 0) { st_fehler = 1; return 1; }
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

int main() {
    int e[4];
    int art; int laufen; int k;

    if (fenster_neu("Settings", 420, 200) < 0) {
        print("Braucht den Schreibtisch -- erst WIN eingeben.\n");
        return 1;
    }
    st_schritt = ST_MENUE;
    st_meldung[0] = 0;
    laufen = 1;
    while (laufen) {
        art = fenster_ereignis(e);
        if (art == FE_SCHLIESS) {
            laufen = 0;
        } else if (art == FE_TASTE) {
            st_taste((e[2] << 8) | e[1]);
        } else if (art == FE_KLICK) {
            k = st_klick(0, e[1], e[2]);
            if (k == 2) { portout(P_POWER, 2); laufen = 0; }
        }
        fenster_malziel();
        app_settings(0);
        fenster_fertig();
        if (art == FE_NICHTS) sleep(3);
    }
    fenster_zu();
    return 0;
}
