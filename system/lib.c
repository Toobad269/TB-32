/* ==========================================================================
   Systembibliothek von TOOBAD-OS

   Alles hier läuft im virtuellen Rechner. Die sys_*-Funktionen stecken in
   start.asm und rufen die BIOS-Dienste auf -- so wie ein echtes C-Programm
   unter DOS über Interrupts mit der Firmware spricht.
   ========================================================================== */

/* --- Brücke zum Assembler ------------------------------------------------ */
int sys_putc(int ch, int attr);
int sys_puts(char* s, int attr);
int sys_setcursor(int x, int y);
int sys_cls(int attr);
int sys_getcursor();
int sys_putat(int x, int y, int ch, int attr);
int sys_putn(int n, int attr);
int sys_puthex(int v, int attr, int digits);
int sys_setmode(int mode);
int sys_box(int x, int y, int w, int h, int attr);
int sys_fillrect(int x, int y, int w, int h, int attr);
int sys_hline(int x, int y, int len, int ch, int attr);
int sys_scroll();
int sys_clearrow(int y, int attr);
int sys_putsat(int x, int y, char* s, int attr);
int sys_diskread(int lba, int count, int addr);
int sys_diskwrite(int lba, int count, int addr);
int sys_disksize();
int sys_getkey();
int sys_haskey();
int net_bearbeiten();   /* steht in net.c, wird hier schon gebraucht */
int sys_flushkeys();
int sys_ticks();
int sys_clock();
int sys_date();
int sys_in(int port);
int sys_out(int port, int value);
int sys_halt();
int sys_blit(int xy, int wh, int col, int cmd);
int sys_blitchar(int xy, int col, int ch, int bg);
int sys_memcpy(int dst, int src, int n);
int sys_memset(int dst, int val, int n);
int sys_sbcount();
int sys_sbline(int nr, int addr);

/* --- Farben und Tasten --------------------------------------------------- */
#define NORMAL   0x07
#define BRIGHT   0x0F
#define GREEN    0x0A
#define RED      0x0C
#define YELLOW   0x0E
#define CYAN     0x0B
#define BLUE     0x09
#define TITLE    0x1F
#define INVERS   0x70

#define K_ESC        1
#define K_BACKSPACE  14
#define K_TAB        15
#define K_ENTER      28
#define K_F1         59
#define K_F2         60
#define K_F3         61
#define K_F5         63
#define K_F10        68
#define K_HOME       71
#define K_UP         72
#define K_PGUP       73
#define K_LEFT       75
#define K_RIGHT      77
#define K_END        79
#define K_DOWN       80
#define K_PGDN       81
#define K_INS        82
#define K_DEL        83

#define P_SPK_FREQ   0x50
#define P_SPK_ON     0x51
#define P_POWER      0x90
/* Der BIOS-Chip. Siehe Doku 16 -- 5 holt den Puffer aus dem RAM, 6 meldet
   ihn fuer genau einen Start an, 8 traegt einen dauerhaften Flashwunsch
   ein, den die Firmware beim naechsten Start bestaetigen laesst. */
#define P_FLASH_CMD  0xB0
#define P_FLASH_SIZE 0xB1
#define P_FLASH_ADDR 0xB2
#define P_VGA_MODE   0x40
#define P_MOUSE_X    0x60
#define P_MOUSE_Y    0x61
#define P_MOUSE_BTN  0x62

int text_attr = 0x07;

/* --- Bildschirmsperre ----------------------------------------------------
   Sobald mehrere Prozesse laufen, wollen sie alle auf denselben Bildschirm
   schreiben -- und ihre Ausgaben landen mitten im Wort des anderen. Deshalb
   holt sich jeder Prozess vor der Ausgabe kurz eine Sperre.

   Das Prüfen und Setzen muss ununterbrechbar sein, sonst könnte der Timer
   genau dazwischen umschalten und beide Prozesse hielten die Sperre. Wir
   sperren dafür kurz die Interrupts -- auf einem Prozessor mit einem Kern ist
   das genau der richtige Weg. Wer warten muss, gibt die Rechenzeit ab,
   statt sie zu verbrennen.                                                  */

int screen_owner = 0;
int screen_depth = 0;

void screen_lock() {
    if (mt_active == 0) return;
    while (1) {
        asm("cli");
        if (screen_owner == 0 || screen_owner == p_current + 1) {
            screen_owner = p_current + 1;
            screen_depth++;
            asm("sti");
            return;
        }
        asm("sti");
        asm("int 0x41");                  /* frei geben und später nochmal */
    }
}

void screen_unlock() {
    if (mt_active == 0) return;
    screen_depth--;
    if (screen_depth <= 0) {
        screen_depth = 0;
        screen_owner = 0;
    }
}

/* --- Ausgabe -------------------------------------------------------------

   Laeuft die Kommandozeile in einem Fenster, gehen alle Ausgaben nicht auf
   den Textbildschirm, sondern in den Puffer des Terminalfensters. Diese
   Weiche steckt an genau einer Stelle -- alles andere merkt nichts davon.

   (term_aktiv und die term_*-Funktionen stehen in term.c -- der Compiler
   sammelt alle Namen vorab ein, deshalb genügt das.) */

void putch(int c) {
    if (cap_aktiv) cap_putc(c);
    if (term_aktiv) term_putc(c, text_attr);
    else sys_putc(c, text_attr);
}

void putcolor(int c, int a) {
    if (cap_aktiv) cap_putc(c);
    if (term_aktiv) term_putc(c, a);
    else sys_putc(c, a);
}

void print(char* s) {
    screen_lock();
    if (cap_aktiv) cap_puts(s);
    if (term_aktiv) term_puts(s, text_attr);
    else sys_puts(s, text_attr);
    screen_unlock();
}

void printc(char* s, int a) {
    screen_lock();
    if (cap_aktiv) cap_puts(s);
    if (term_aktiv) term_puts(s, a);
    else sys_puts(s, a);
    screen_unlock();
}

void printn(int n) {
    char zahl[16];
    screen_lock();
    if (cap_aktiv) { itoa(n, zahl); cap_puts(zahl); }
    if (term_aktiv) term_putn(n, text_attr);
    else sys_putn(n, text_attr);
    screen_unlock();
}

void printnc(int n, int a) {
    char zahl[16];
    screen_lock();
    if (cap_aktiv) { itoa(n, zahl); cap_puts(zahl); }
    if (term_aktiv) term_putn(n, a);
    else sys_putn(n, a);
    screen_unlock();
}

void nl() {
    if (cap_aktiv) cap_putc(10);
    if (term_aktiv) term_putc(10, text_attr);
    else sys_putc(10, text_attr);
}

void cls() {
    if (term_aktiv) term_clear();
    else sys_cls(text_attr);
}

void color(int a)              { text_attr = a; }

void println(char* s) {
    print(s);
    nl();
}

void printnum(char* label, int n) {
    print(label);
    printnc(n, BRIGHT);
    nl();
}

/* Zahl mit fester Stellenzahl, mit führenden Nullen */
void print2(int n) {
    if (n < 10) putch('0');
    printn(n);
}

/* --- Zeichenketten ------------------------------------------------------- */

int strlen(char* s) {
    int n;
    n = 0;
    while (*s) { n++; s++; }
    return n;
}

int strcmp(char* a, char* b) {
    while (*a && *a == *b) { a++; b++; }
    return *a - *b;
}

int strncmp(char* a, char* b, int n) {
    while (n > 0 && *a && *a == *b) { a++; b++; n--; }
    if (n == 0) return 0;
    return *a - *b;
}

void strcpy(char* d, char* s) {
    while (*s) { *d = *s; d++; s++; }
    *d = 0;
}

void strncpy(char* d, char* s, int n) {
    while (n > 1 && *s) { *d = *s; d++; s++; n--; }
    *d = 0;
}

void strcat(char* d, char* s) {
    while (*d) d++;
    strcpy(d, s);
}

int toupper(int c) {
    if (c >= 'a' && c <= 'z') return c - 32;
    return c;
}

int tolower(int c) {
    if (c >= 'A' && c <= 'Z') return c + 32;
    return c;
}

int stricmp(char* a, char* b) {
    while (*a && tolower(*a) == tolower(*b)) { a++; b++; }
    return tolower(*a) - tolower(*b);
}

int atoi(char* s) {
    int n; int neg;
    n = 0; neg = 0;
    while (*s == ' ') s++;
    if (*s == '-') { neg = 1; s++; }
    while (*s >= '0' && *s <= '9') { n = n * 10 + (*s - '0'); s++; }
    if (neg) return 0 - n;
    return n;
}

void itoa(int n, char* buf) {
    char tmp[16];
    int i; int j;
    i = 0;
    if (n < 0) { *buf = '-'; buf++; n = 0 - n; }
    if (n == 0) { tmp[0] = '0'; i = 1; }
    while (n > 0) { tmp[i] = '0' + (n % 10); n = n / 10; i++; }
    j = 0;
    while (i > 0) { i--; buf[j] = tmp[i]; j++; }
    buf[j] = 0;
}

/* --- Speicher ------------------------------------------------------------ */

void memcpy(char* d, char* s, int n)  { sys_memcpy((int)d, (int)s, n); }
void memset(char* d, int v, int n)    { sys_memset((int)d, v, n); }

int  byte_get(int addr)        { char* p; p = (char*)addr; return *p & 255; }
void byte_put(int addr, int v) { char* p; p = (char*)addr; *p = v; }

/* Wortweise lesen und schreiben. Fuer Pruefsummen ueber ganze Abbilder --
   byteweise waere dieselbe Rechnung viermal so lang. */
int  word_get(int addr)        { int* p; p = (int*)addr; return *p; }
void word_put(int addr, int v) { int* p; p = (int*)addr; *p = v; }

/* --- Eingabe ------------------------------------------------------------- */

int getkey() {
    if (term_aktiv) return term_getkey();
    /* Waehrend niemand tippt, wird die Post bearbeitet. Nur so antwortet
       der Rechner auf ARP und PING, ohne dass jemand davor sitzt -- genau
       das erwartet man von einem Rechner im Netz. Das sys_halt() dazwischen
       haelt ihn dabei ruhig: es weckt der naechste Interrupt, also der
       Zeitgeber oder ein ankommender Rahmen. */
    while (sys_haskey() == 0) {
        net_bearbeiten();
        sys_halt();
    }
    return sys_getkey();
}
int keychar(int k)  { return k & 255; }
int keycode(int k)  { return (k >> 8) & 255; }

void sleep(int ticks) {
    int ziel;
    /* Ohne dieses sti steht der ganze Rechner, sobald ein Programm ueber
       einen Systemaufruf hier landet: Bei INT 0x40 sperrt die CPU die
       Interrupts bis zum iret. Das hlt unten wartet dann auf einen
       Timer-Interrupt, der nie kommt. Wer in einem Interrupt wartet, muss
       die Interrupts selbst freigeben -- wie in proc_exit(). */
    asm("sti");
    ziel = sys_ticks() + ticks;
    while (sys_ticks() < ziel) sys_halt();
}

void beep(int freq, int dauer) {
    sys_out(P_SPK_FREQ, freq);
    sys_out(P_SPK_ON, 1);
    sleep(dauer);
    sys_out(P_SPK_ON, 0);
}

/* Liest eine Zeile mit Echo, Backspace und Escape. Gibt die Länge zurück,
   oder -1 wenn ESC gedrückt wurde. */
int readline(char* buf, int max) {
    int n; int k; int c; int code;
    n = 0;
    while (1) {
        k = getkey();
        c = keychar(k);
        code = keycode(k);
        if (code == K_ENTER) { buf[n] = 0; return n; }
        if (code == K_ESC)   { buf[0] = 0; return 0 - 1; }
        if (code == K_BACKSPACE) {
            if (n > 0) { n--; putch(8); }
            continue;
        }
        if (code == K_PGUP) {                    /* zurueckblaettern */
            scrollback();
            continue;
        }
        if (c >= 32 && c < 127 && n < max - 1) {
            buf[n] = c;
            n++;
            putcolor(c, BRIGHT);
        }
    }
}

/* --- Zurueckblaettern in der Bildschirmhistorie -------------------------
   Alles, was oben aus dem Bild gelaufen ist, hat das BIOS in einem
   Ringpuffer aufgehoben. Hier wird daraus eine Ansicht zum Blaettern --
   genau wie der Scrollback eines echten Terminals.                        */

#define VRAM_TEXT_ADDR 0x02000000
#define SB_SAVE        0x00114000        /* Sicherung des aktuellen Bildes */

void scrollback() {
    int count; int pos; int k; int code; int i;

    count = sys_sbcount();
    if (count == 0) {
        printc("Nothing in the scrollback buffer yet.\n", NORMAL);
        return;
    }
    sys_memcpy(SB_SAVE, VRAM_TEXT_ADDR, 4000);   /* aktuelles Bild sichern */
    pos = count - 24;
    if (pos < 0) pos = 0;

    while (1) {
        for (i = 0; i < 24; i++) {
            if (pos + i < count) sys_sbline(pos + i, VRAM_TEXT_ADDR + i * 160);
            else sys_clearrow(i, NORMAL);
        }
        sys_hline(0, 24, 80, 32, INVERS);
        sys_putsat(1, 24, "SCROLLBACK   Up/Down PgUp/PgDn Home/End   ESC = back", INVERS);
        sys_setcursor(58, 24);
        sys_putn(pos + 1, INVERS);
        sys_putc('/', INVERS);
        sys_putn(count, INVERS);
        sys_setcursor(79, 24);

        k = getkey();
        code = keycode(k);
        if (code == K_ESC || code == K_ENTER) break;
        if (code == K_UP)   pos = pos - 1;
        if (code == K_DOWN) pos = pos + 1;
        if (code == K_PGUP) pos = pos - 20;
        if (code == K_PGDN) pos = pos + 20;
        if (code == K_HOME) pos = 0;
        if (code == K_END)  pos = count - 24;
        if (pos > count - 1) pos = count - 1;
        if (pos < 0) pos = 0;
    }
    sys_memcpy(VRAM_TEXT_ADDR, SB_SAVE, 4000);   /* Bild zurueckholen */
}

/* Wartet auf eine beliebige Taste */
void anykey() {
    printc("  -- press any key --", 0x08);
    getkey();
    nl();
}
