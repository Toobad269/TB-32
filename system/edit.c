/* ==========================================================================
   EDIT -- der Texteditor von TOOBAD-OS

   Vollbild, mit Statuszeile oben und Hilfe unten. Der Text steht am Stueck
   im Puffer; der Cursor ist einfach eine Stelle darin. Beim Zeichnen wird
   der Text in Zeilen zerlegt.
   ========================================================================== */

#define ED_BUF     0x000D0000        /* bis zu 60 KB Text */
#define ED_MAX     60000
#define ED_ROWS    22                /* sichtbare Textzeilen */
#define ED_COLS    80

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

void ed_status() {
    int i;
    sys_hline(0, 0, 80, 32, INVERS);
    sys_putsat(1, 0, "EDIT  ", INVERS);
    sys_putsat(7, 0, ed_name, INVERS);
    if (ed_dirty) sys_putsat(24, 0, "*", 0x74);
    sys_putsat(40, 0, "Line", INVERS);
    sys_setcursor(45, 0);
    sys_putn(ed_line_of(ed_pos) + 1, INVERS);
    sys_putsat(52, 0, "Col", INVERS);
    sys_setcursor(56, 0);
    sys_putn(ed_col_of(ed_pos) + 1, INVERS);
    sys_putsat(64, 0, "Bytes", INVERS);
    sys_setcursor(70, 0);
    sys_putn(ed_len, INVERS);

    sys_hline(0, 24, 80, 32, INVERS);
    sys_putsat(1, 24, "F2 Save    ESC Exit    Arrow keys move    Home/End line", INVERS);
}

void ed_draw() {
    char* t; int zeile; int p; int x; int y; int c;
    t = ed_text();

    /* Sichtbereich nachfuehren */
    zeile = ed_line_of(ed_pos);
    if (zeile < ed_top) ed_top = zeile;
    if (zeile >= ed_top + ED_ROWS) ed_top = zeile - ED_ROWS + 1;

    p = ed_start_of_line(ed_top);
    for (y = 0; y < ED_ROWS; y++) {
        sys_clearrow(y + 1, NORMAL);
        x = 0;
        while (p < ed_len && t[p] != 10) {
            if (x < ED_COLS) {
                c = t[p];
                if (c == 9) c = 32;
                sys_putat(x, y + 1, c, NORMAL);
            }
            x++;
            p++;
        }
        if (p < ed_len) p++;                 /* Zeilenumbruch ueberspringen */
    }
    ed_status();
    sys_setcursor(ed_col_of(ed_pos), ed_line_of(ed_pos) - ed_top + 1);
}

void ed_save() {
    int r;
    r = fs_write(ed_name, ED_BUF, ed_len);
    if (r == 0) {
        ed_dirty = 0;
        sys_putsat(1, 24, "File saved.                                              ", 0x2F);
    } else {
        sys_putsat(1, 24, "ERROR: could not save file (disk full?)                  ", 0x4F);
    }
    sleep(60);
}

void edit(char* name) {
    int k; int c; int code; int laenge; int i; int zeile; int spalte; int p;

    memset(ed_name, 0, 20);
    strncpy(ed_name, name, 16);
    ed_pos = 0;
    ed_top = 0;
    ed_dirty = 0;

    laenge = fs_read(name, ED_BUF, ED_MAX);
    if (laenge < 0) laenge = 0;              /* neue Datei */
    ed_len = laenge;

    sys_cls(NORMAL);
    ed_draw();

    while (1) {
        k = getkey();
        c = keychar(k);
        code = keycode(k);

        if (code == K_ESC) {
            if (ed_dirty) {
                sys_putsat(1, 24,
                    "File has unsaved changes.  S = save,  D = discard,  any other key = back",
                    0x4F);
                k = getkey();
                c = toupper(keychar(k));
                if (c == 'S') { ed_save(); }
                else if (c != 'D') { ed_draw(); continue; }
            }
            sys_cls(NORMAL);
            sys_setcursor(0, 0);
            return;
        }
        if (code == K_F2)        { ed_save(); ed_draw(); continue; }
        if (code == K_BACKSPACE) { ed_backspace(); ed_draw(); continue; }
        if (code == K_DEL)       { ed_delete(); ed_draw(); continue; }
        if (code == K_ENTER)     { ed_insert(10); ed_draw(); continue; }
        if (code == K_LEFT)      { if (ed_pos > 0) ed_pos--; ed_draw(); continue; }
        if (code == K_RIGHT)     { if (ed_pos < ed_len) ed_pos++; ed_draw(); continue; }
        if (code == K_HOME)      { ed_pos = ed_line_start(ed_pos); ed_draw(); continue; }
        if (code == K_END)       { ed_pos = ed_line_end(ed_pos); ed_draw(); continue; }

        if (code == K_UP) {
            zeile = ed_line_of(ed_pos);
            spalte = ed_col_of(ed_pos);
            if (zeile > 0) {
                p = ed_start_of_line(zeile - 1);
                if (p + spalte > ed_line_end(p)) p = ed_line_end(p);
                else p = p + spalte;
                ed_pos = p;
            }
            ed_draw();
            continue;
        }
        if (code == K_DOWN) {
            zeile = ed_line_of(ed_pos);
            spalte = ed_col_of(ed_pos);
            p = ed_start_of_line(zeile + 1);
            if (p < ed_len || zeile + 1 <= ed_line_of(ed_len)) {
                if (p + spalte > ed_line_end(p)) p = ed_line_end(p);
                else p = p + spalte;
                ed_pos = p;
            }
            ed_draw();
            continue;
        }
        if (code == K_PGUP) {
            zeile = ed_line_of(ed_pos) - ED_ROWS;
            if (zeile < 0) zeile = 0;
            ed_pos = ed_start_of_line(zeile);
            ed_draw();
            continue;
        }
        if (code == K_PGDN) {
            ed_pos = ed_start_of_line(ed_line_of(ed_pos) + ED_ROWS);
            ed_draw();
            continue;
        }
        if (c == 9) { ed_insert(32); ed_insert(32); ed_draw(); continue; }
        if (c >= 32 && c < 127) { ed_insert(c); ed_draw(); continue; }
    }
}
