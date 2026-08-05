/* ==========================================================================
   System library of TOOBAD-OS

   Everything here runs on the virtual machine. The sys_* functions live
   in start.asm and call the BIOS services -- just like a real C program
   under DOS talks to the firmware through interrupts.
   ========================================================================== */

/* --- Bridge to the assembler ---------------------------------------------- */
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
int net_bearbeiten();   /* lives in net.c, already needed here */
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

/* --- Colours and keys ----------------------------------------------------- */
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
/* The BIOS chip. See doc 16 -- 5 fetches the buffer from RAM, 6 registers
   it for exactly one boot, 8 records a persistent flash request that the
   firmware confirms on the next boot. */
#define P_FLASH_CMD  0xB0
#define P_FLASH_SIZE 0xB1
#define P_FLASH_ADDR 0xB2
#define P_VGA_MODE   0x40
#define P_MOUSE_X    0x60
#define P_MOUSE_Y    0x61
#define P_MOUSE_BTN  0x62

int text_attr = 0x07;

/* --- Screen lock -----------------------------------------------------------
   As soon as several processes are running, they all want to write to the
   same screen -- and their output ends up in the middle of each other's
   words. So every process grabs a brief lock before writing.

   The check-and-set must be uninterruptible, or the timer could switch
   contexts right in the middle and both processes would end up holding the
   lock. We briefly disable interrupts for this -- on a single-core
   processor that is exactly the right approach. Whoever has to wait gives
   up its time slice instead of burning it.                                  */

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
        asm("int 0x41");                  /* yield and try again later */
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

/* --- Output ----------------------------------------------------------------

   If the command line is running in a window, all output does not go to
   the text screen but into the terminal window's buffer. This switch sits
   at exactly one place -- nothing else notices the difference.

   (term_aktiv and the term_* functions live in term.c -- the compiler
   collects all names up front, so that's enough.) */

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

/* Number with a fixed digit count, zero-padded */
void print2(int n) {
    if (n < 10) putch('0');
    printn(n);
}

/* --- Strings --------------------------------------------------------------- */

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

/* --- Memory ----------------------------------------------------------------- */

void memcpy(char* d, char* s, int n)  { sys_memcpy((int)d, (int)s, n); }
void memset(char* d, int v, int n)    { sys_memset((int)d, v, n); }

int  byte_get(int addr)        { char* p; p = (char*)addr; return *p & 255; }
void byte_put(int addr, int v) { char* p; p = (char*)addr; *p = v; }

/* Read and write a whole word at a time. For checksums over whole images --
   doing it byte by byte would take four times as long for the same result. */
int  word_get(int addr)        { int* p; p = (int*)addr; return *p; }
void word_put(int addr, int v) { int* p; p = (int*)addr; *p = v; }

/* --- Input ------------------------------------------------------------------- */

int getkey() {
    if (term_aktiv) return term_getkey();
    /* While nobody is typing, the mail gets handled. This is the only way
       the machine answers ARP and PING without anyone sitting in front of
       it -- exactly what you'd expect from a machine on the network. The
       sys_halt() in between keeps it quiet while doing so: it is woken by
       the next interrupt, i.e. the timer or an incoming frame. */
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
    /* Without this sti the whole machine would hang as soon as a program
       ends up here through a system call: with INT 0x40 the CPU disables
       interrupts until the iret. The hlt below would then wait for a timer
       interrupt that never comes. Whoever waits inside an interrupt has to
       re-enable interrupts itself -- as in proc_exit(). */
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

/* Reads a line with echo, backspace and escape. Returns the length,
   or -1 if ESC was pressed. */
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
        if (code == K_PGUP) {                    /* scroll back */
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

/* --- Scrolling back through the screen history ----------------------------
   Everything that has scrolled off the top has been kept by the BIOS in a
   ring buffer. Here that becomes a scrollable view -- just like the
   scrollback of a real terminal.                                          */

#define VRAM_TEXT_ADDR 0x02000000
#define SB_SAVE        0x00114000        /* backup of the current screen */

void scrollback() {
    int count; int pos; int k; int code; int i;

    count = sys_sbcount();
    if (count == 0) {
        printc("Nothing in the scrollback buffer yet.\n", NORMAL);
        return;
    }
    sys_memcpy(SB_SAVE, VRAM_TEXT_ADDR, 4000);   /* save the current screen */
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
    sys_memcpy(VRAM_TEXT_ADDR, SB_SAVE, 4000);   /* restore the screen */
}

/* Waits for any key */
void anykey() {
    printc("  -- press any key --", 0x08);
    getkey();
    nl();
}
