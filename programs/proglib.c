/* ==========================================================================
   Kleine Bibliothek für TOOBAD-OS-Programme.

   Jede Funktion hier ist nur eine hübsche Verpackung für einen Systemaufruf.
   Genau so sieht eine C-Bibliothek auf einem echten System aus.
   ========================================================================== */

int sc(int fn, int a1, int a2, int a3, int a4);     /* in prog_start.asm */

#define NORMAL 0x07
#define BRIGHT 0x0F
#define GREEN  0x0A
#define RED    0x0C
#define YELLOW 0x0E
#define CYAN   0x0B
#define BLUE   0x09
#define INVERS 0x70

#define K_ESC   1
#define K_ENTER 28
#define K_UP    72
#define K_LEFT  75
#define K_RIGHT 77
#define K_DOWN  80

void putch(int c)                 { sc(0, c, NORMAL, 0, 0); }
void putcolor(int c, int a)       { sc(0, c, a, 0, 0); }
void print(char* s)               { sc(1, (int)s, NORMAL, 0, 0); }
void printc(char* s, int a)       { sc(1, (int)s, a, 0, 0); }
void printn(int n)                { sc(6, n, NORMAL, 0, 0); }
void printnc(int n, int a)        { sc(6, n, a, 0, 0); }
void nl()                         { sc(0, 10, NORMAL, 0, 0); }
void cls()                        { sc(3, NORMAL, 0, 0, 0); }
int  getkey()                     { return sc(2, 0, 0, 0, 0); }
int  haskey()                     { return sc(9, 0, 0, 0, 0); }
void flushkeys()                  { sc(23, 0, 0, 0, 0); }
int  ticks()                      { return sc(5, 0, 0, 0, 0); }
void setcursor(int x, int y)      { sc(7, x, y, 0, 0); }
void putat(int x, int y, int c, int a) { sc(8, x, y, c, a); }
void sleep(int t)                 { sc(14, t, 0, 0, 0); }
void beep(int f, int d)           { sc(15, f, d, 0, 0); }
int  clock_now()                  { return sc(12, 0, 0, 0, 0); }
int  date_now()                   { return sc(13, 0, 0, 0, 0); }
void box(int x, int y, int w, int h) { sc(20, x, y, w, h); }
void hline(int x, int y, int l, int c) { sc(21, x, y, l, c); }
int  memkb()                      { return sc(22, 0, 0, 0, 0); }
int  filecount()                  { return sc(24, 0, 0, 0, 0); }
int  fileread(char* n, int adr, int max) { return sc(10, (int)n, adr, max, 0); }
/* wie fileread, sucht aber zusaetzlich in \SOURCE -- fuer #include */
int  fileread_lib(char* n, int adr, int max) { return sc(33, (int)n, adr, max, 0); }
int  filewrite(char* n, int adr, int len) { return sc(11, (int)n, adr, len, 0); }
void setmode(int m)               { sc(17, m, 0, 0, 0); }
/* Hardware direkt statt ueber den Kernel -- steht in prog_start.asm, und CC
   auf dem Geraet setzt an der Aufrufstelle dieselben zwei Befehle ein. */
void portout(int p, int v);
int  portin(int p);
void ende()                       { sc(4, 0, 0, 0, 0); }
int  fontaddr()                   { return sc(30, 0, 0, 0, 0); }
/* Zwischenablage: der Puffer liegt fest, die Laenge holt man sich. */
#define CLIP_BUF  0x00130000
#define CLIP_MAX  8192
int  clip_holen()                 { return sc(45, 0, 0, 0, 0); }
void clip_setzen(int n)           { sc(46, n, 0, 0, 0); }

int keychar(int k) { return k & 255; }
int keycode(int k) { return (k >> 8) & 255; }

/* --- Zeichenketten und Speicher (laufen ganz im Programm) --------------- */

int strlen(char* s) { int n; n = 0; while (*s) { n++; s++; } return n; }

int strcmp(char* a, char* b) {
    while (*a && *a == *b) { a++; b++; }
    return *a - *b;
}

int toupper(int c) { if (c >= 'a' && c <= 'z') return c - 32; return c; }
int tolower(int c) { if (c >= 'A' && c <= 'Z') return c + 32; return c; }

int stricmp(char* a, char* b) {
    while (*a && tolower(*a) == tolower(*b)) { a++; b++; }
    return tolower(*a) - tolower(*b);
}

void strcpy(char* d, char* s) { while (*s) { *d = *s; d++; s++; } *d = 0; }

void strncpy(char* d, char* s, int n) {
    while (n > 1 && *s) { *d = *s; d++; s++; n--; }
    *d = 0;
}

void strcat(char* d, char* s) {
    while (*d) d++;
    while (*s) { *d = *s; d++; s++; }
    *d = 0;
}

void memcpy(char* d, char* s, int n) {
    while (n > 0) { *d = *s; d++; s++; n--; }
}

void memset(char* d, int v, int n) {
    while (n > 0) { *d = v; d++; n--; }
}

int isdigit(int c) { if (c >= '0' && c <= '9') return 1; return 0; }
int isspace(int c) { if (c == 32 || c == 9 || c == 13) return 1; return 0; }

int isalpha(int c) {
    if (c >= 'a' && c <= 'z') return 1;
    if (c >= 'A' && c <= 'Z') return 1;
    if (c == '_') return 1;
    return 0;
}

int isalnum(int c) { if (isalpha(c) || isdigit(c)) return 1; return 0; }

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
    char tmp[16]; int i; int j;
    i = 0;
    if (n < 0) { *buf = '-'; buf++; n = 0 - n; }
    if (n == 0) { tmp[0] = '0'; i = 1; }
    while (n > 0) { tmp[i] = '0' + (n % 10); n = n / 10; i++; }
    j = 0;
    while (i > 0) { i--; buf[j] = tmp[i]; j++; }
    buf[j] = 0;
}

int mem_get(int addr)         { int* p; p = (int*)addr; return *p; }
void mem_put(int addr, int v) { int* p; p = (int*)addr; *p = v; }
int  byte_get(int addr)       { char* p; p = (char*)addr; return *p & 255; }
void byte_put(int addr, int v){ char* p; p = (char*)addr; *p = v; }

int rnd_state = 7919;
int rnd(int max) {
    rnd_state = rnd_state * 1103515245 + 12345;
    rnd_state = rnd_state & 2147483647;
    return (rnd_state >> 7) % max;
}
