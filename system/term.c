/* ==========================================================================
   Terminal im Fenster

   Die Kommandozeile soll auch auf dem Schreibtisch laufen. Dafür braucht sie
   zweierlei:

     1. Einen eigenen Bildspeicher. Alles, was sonst in den Textbildschirm
        ginge, landet in TERM_BUF -- die Oberflaeche malt es dann als Fenster.
     2. Eine eigene Tastatur. Die Oberflaeche liest die Tasten und reicht sie
        an das Terminal weiter, wenn dessen Fenster gerade vorn ist.

   Die Shell selbst laeuft als eigener Prozess. Waehrend sie auf eine Eingabe
   wartet, gibt sie die Rechenzeit ab -- die Oberflaeche bleibt also bedienbar,
   und Uhr und Systemmonitor laufen weiter.
   ========================================================================== */

#define TERM_W      70
#define TERM_H      22
#define TERM_BUF    0x00120000
#define TERM_KEYS   64

/* Zurueckblaettern: die Zeilen, die oben herauslaufen, wandern in einen
   Ringpuffer. Ohne den waeren sie einfach weg -- die Kommandozeile im
   Textmodus hat so etwas laengst, das Fenster hatte es noch nicht. */
#define TERM_SB     0x00124000
#define TERM_SBMAX  200

int term_aktiv = 0;              /* 1 = Ausgabe geht ins Fenster */
int term_x = 0;
int term_y = 0;
int term_attr = 7;
int term_dirty = 1;
int term_lauf = 0;               /* laeuft der Shell-Prozess schon? */
int term_pid = 0 - 1;

/* --- Ausgabe mitschreiben ------------------------------------------------
   Waehrend der Editor uebersetzen laesst, laeuft der Compiler als eigener
   Prozess und schreibt seine Meldungen dorthin, wo gerade die Ausgabe
   hingeht -- im Grafikmodus also in den unsichtbaren Textbildschirm. Genau
   die Fehlermeldungen will man aber sehen. Deshalb kann die Ausgabe
   zusaetzlich in einen Puffer mitgeschrieben werden, den das
   Uebersetzungsfenster dann anzeigt. */
#define CAP_BUF     0x00128000
#define CAP_ZEILEN  40
#define CAP_BREITE  76

int cap_aktiv = 0;
int cap_zeile = 0;               /* naechste Zeile, die geschrieben wird */
int cap_spalte = 0;
int cap_voll = 0;                /* so viele Zeilen stehen drin */

int cap_adr(int z) { return CAP_BUF + z * (CAP_BREITE + 1); }

void cap_start() {
    int i;
    for (i = 0; i < CAP_ZEILEN; i++) byte_put(cap_adr(i), 0);
    cap_zeile = 0;
    cap_spalte = 0;
    cap_voll = 0;
    cap_aktiv = 1;
}

void cap_putc(int c) {
    int a;
    if (cap_zeile >= CAP_ZEILEN) return;         /* voll: Rest faellt weg */
    if (c == 13) return;
    if (c == 10 || cap_spalte >= CAP_BREITE) {
        cap_zeile++;
        cap_spalte = 0;
        if (cap_zeile > cap_voll) cap_voll = cap_zeile;
        if (cap_zeile < CAP_ZEILEN) byte_put(cap_adr(cap_zeile), 0);
        if (c == 10) return;
    }
    if (cap_zeile >= CAP_ZEILEN) return;
    a = cap_adr(cap_zeile) + cap_spalte;
    byte_put(a, c);
    byte_put(a + 1, 0);
    cap_spalte++;
    if (cap_zeile + 1 > cap_voll) cap_voll = cap_zeile + 1;
}

void cap_puts(char* s) {
    while (*s) { cap_putc(*s); s++; }
}

char* cap_text(int z) { return (char*)cap_adr(z); }

int term_sb_count = 0;           /* wie viele Zeilen gesichert sind */
int term_sb_head = 0;            /* naechster Schreibplatz im Ring */
int term_view = 0;               /* 0 = live, sonst so viele Zeilen zurueck */

int term_keys[TERM_KEYS];
int term_khead = 0;
int term_ktail = 0;

int term_zelle(int x, int y) { return TERM_BUF + (y * TERM_W + x) * 2; }

void term_clear() {
    int i;
    for (i = 0; i < TERM_W * TERM_H; i++) {
        byte_put(TERM_BUF + i * 2, 32);
        byte_put(TERM_BUF + i * 2 + 1, 7);
    }
    term_x = 0;
    term_y = 0;
    term_dirty = 1;
}

/* Alles eine Zeile hochschieben */
/* Die oberste Zeile in den Ringpuffer retten, bevor sie ueberschrieben wird */
void term_sb_push() {
    int i; int z;
    z = TERM_SB + term_sb_head * TERM_W * 2;
    for (i = 0; i < TERM_W * 2; i++) byte_put(z + i, byte_get(TERM_BUF + i));
    term_sb_head = (term_sb_head + 1) % TERM_SBMAX;
    if (term_sb_count < TERM_SBMAX) term_sb_count++;
}

/* Adresse der i-ten sichtbaren Zeile. view sagt, wie viele Zeilen man
   zurueckgeblaettert hat; die Rechnung laeuft ueber einen gedachten
   Gesamtstrom aus Ringpuffer + aktuellem Bild, deshalb geht der Uebergang
   nahtlos. -1 heisst: so weit zurueck gibt es nichts mehr. */
int term_sicht(int i, int view) {
    int gesamt; int n;
    gesamt = term_sb_count + i - view;
    if (gesamt < 0) return 0 - 1;
    if (gesamt >= term_sb_count) return term_zelle(0, gesamt - term_sb_count);
    n = (term_sb_head - term_sb_count + gesamt + TERM_SBMAX * 2) % TERM_SBMAX;
    return TERM_SB + n * TERM_W * 2;
}

void term_scroll() {
    int i; int n;
    term_sb_push();
    n = (TERM_H - 1) * TERM_W * 2;
    for (i = 0; i < n; i++)
        byte_put(TERM_BUF + i, byte_get(TERM_BUF + TERM_W * 2 + i));
    for (i = 0; i < TERM_W; i++) {
        byte_put(TERM_BUF + n + i * 2, 32);
        byte_put(TERM_BUF + n + i * 2 + 1, 7);
    }
}

void term_newline() {
    term_x = 0;
    term_y++;
    if (term_y >= TERM_H) {
        term_scroll();
        term_y = TERM_H - 1;
    }
    term_dirty = 1;
}

void term_putc(int c, int attr) {
    int a;
    if (c == 10) { term_newline(); return; }
    if (c == 13) { term_x = 0; return; }
    if (c == 8) {
        if (term_x > 0) {
            term_x--;
            a = term_zelle(term_x, term_y);
            byte_put(a, 32);
            byte_put(a + 1, attr);
        }
        term_dirty = 1;
        return;
    }
    if (c == 9) {                                /* Tabulator */
        term_x = (term_x + 8) & 0xFFF8;
        if (term_x >= TERM_W) term_newline();
        return;
    }
    a = term_zelle(term_x, term_y);
    byte_put(a, c);
    byte_put(a + 1, attr);
    term_x++;
    if (term_x >= TERM_W) term_newline();
    term_dirty = 1;
}

void term_puts(char* s, int attr) {
    while (*s) {
        term_putc(*s, attr);
        s++;
    }
}

/* Zahl ausgeben (die Shell benutzt sonst den BIOS-Dienst) */
void term_putn(int n, int attr) {
    char buf[16];
    itoa(n, buf);
    term_puts(buf, attr);
}

/* --- Tastatur ------------------------------------------------------------ */

void term_push_key(int k) {
    int neu;
    neu = (term_ktail + 1) % TERM_KEYS;
    if (neu == term_khead) return;               /* Puffer voll */
    term_keys[term_ktail] = k;
    term_ktail = neu;
}

int term_has_key() {
    if (term_khead == term_ktail) return 0;
    return term_keys[term_khead];
}

/* Wartet auf eine Taste und gibt dabei Rechenzeit ab, damit die
   Oberflaeche weiterlaeuft. */
int term_getkey() {
    int k;
    while (term_khead == term_ktail) {
        if (mt_active) asm("int 0x41");
        else sys_halt();
    }
    k = term_keys[term_khead];
    term_khead = (term_khead + 1) % TERM_KEYS;
    return k;
}

/* --- Der Shell-Prozess --------------------------------------------------- */

void shell();                                    /* steht in kernel.c */

void term_main() {
    term_aktiv = 1;
    term_clear();
    term_puts("TOOBAD-OS command prompt\n", 0x0B);
    term_puts("Type HELP for commands, EXIT closes this window.\n\n", 7);
    shell();
    term_aktiv = 0;
    term_lauf = 0;
    proc_exit();
}
