/* ==========================================================================
   TBOS 0.1
   A small operating-system kernel for the TOOBAD TB-32.

   The code is compiled by the TB-32 project's own TC compiler and then
   assembled into real TB-32 machine code by the project's own assembler.
   ========================================================================== */

/* Assembly bridge from tbos_system/start.asm */
int sys_putc(int ch, int attr);
int sys_puts(char* text, int attr);
int sys_cls(int attr);
int sys_putn(int number, int attr);
int sys_puthex(int value, int attr, int digits);

int sys_getkey();
int sys_haskey();
int sys_flushkeys();

int sys_ticks();
int sys_clock();
int sys_date();

int sys_out(int port, int value);
int sys_halt();

/* Text attributes */
#define NORMAL  0x07
#define BRIGHT  0x0F
#define GREEN   0x0A
#define RED     0x0C
#define YELLOW  0x0E
#define CYAN    0x0B
#define BLUE    0x09
#define MAGENTA 0x0D

/* Keyboard scan codes returned in bits 8..15 by BIOS INT 0x16. */
#define K_BACKSPACE 14
#define K_ENTER     28

/* Hardware ports. */
#define P_SPK_FREQ 0x50
#define P_SPK_ON   0x51
#define P_POWER    0x90

#define LINE_MAX 96

int text_attr = NORMAL;
char line[LINE_MAX];
char command[24];
char argument[72];

/* --------------------------------------------------------------------------
   Basic terminal functions
   -------------------------------------------------------------------------- */

void putch(int c) {
    sys_putc(c, text_attr);
}

void print(char* s) {
    sys_puts(s, text_attr);
}

void printc(char* s, int attr) {
    sys_puts(s, attr);
}

void nl() {
    sys_putc(10, text_attr);
}

void cls() {
    sys_cls(text_attr);
}

void printn(int n) {
    sys_putn(n, text_attr);
}

void printhex(int n, int digits) {
    sys_puthex(n, text_attr, digits);
}

int key_char(int k) {
    return k & 255;
}

int key_code(int k) {
    return (k >> 8) & 255;
}

int upper(int c) {
    if (c >= 'a' && c <= 'z') return c - 32;
    return c;
}

int streq(char* a, char* b) {
    while (*a && *b) {
        if (*a != *b) return 0;
        a++;
        b++;
    }
    if (*a == 0 && *b == 0) return 1;
    return 0;
}

/* Read one command line with echo and backspace. */
int readline(char* buf, int max) {
    int n;
    int k;
    int c;
    int code;

    n = 0;
    while (1) {
        k = sys_getkey();
        c = key_char(k);
        code = key_code(k);

        if (code == K_ENTER) {
            buf[n] = 0;
            nl();
            return n;
        }

        if (code == K_BACKSPACE) {
            if (n > 0) {
                n--;
                putch(8);
            }
        } else {
            if (c >= 32 && c < 127 && n < max - 1) {
                buf[n] = c;
                n++;
                putch(c);
            }
        }
    }
}

/* Split a line into an upper-case command and the remaining argument. */
void parse_line(char* src) {
    int i;
    int j;

    i = 0;
    while (src[i] == ' ') i++;

    j = 0;
    while (src[i] && src[i] != ' ' && j < 23) {
        command[j] = upper(src[i]);
        i++;
        j++;
    }
    command[j] = 0;

    while (src[i] == ' ') i++;

    j = 0;
    while (src[i] && j < 71) {
        argument[j] = src[i];
        i++;
        j++;
    }
    argument[j] = 0;
}

void print2(int n) {
    if (n < 10) putch('0');
    printn(n);
}

/* --------------------------------------------------------------------------
   Small kernel services
   -------------------------------------------------------------------------- */

void sleep_ticks(int ticks) {
    int target;
    target = sys_ticks() + ticks;
    while (sys_ticks() < target) {
        sys_halt();
    }
}

void beep() {
    sys_out(P_SPK_FREQ, 700);
    sys_out(P_SPK_ON, 1);
    sleep_ticks(20);
    sys_out(P_SPK_ON, 0);
}

void show_time() {
    int t;
    int h;
    int m;
    int s;

    t = sys_clock();
    h = (t >> 16) & 255;
    m = (t >> 8) & 255;
    s = t & 255;

    print2(h);
    putch(':');
    print2(m);
    putch(':');
    print2(s);
    nl();
}

void show_date() {
    int d;
    int y;
    int m;
    int day;

    d = sys_date();
    y = (d >> 16) & 65535;
    m = (d >> 8) & 255;
    day = d & 255;

    print2(day);
    putch('.');
    print2(m);
    putch('.');
    printn(y);
    nl();
}

void show_help() {
    printc("TBOS command reference\n", CYAN);
    print("\n");
    printc("  HELP", BRIGHT);
    print("       Show this command list\n");

    printc("  VER", BRIGHT);
    print("        Show TBOS version\n");

    printc("  CLS", BRIGHT);
    print("        Clear the screen\n");

    printc("  ECHO text", BRIGHT);
    print("  Print text\n");

    printc("  TIME", BRIGHT);
    print("       Show BIOS clock\n");

    printc("  DATE", BRIGHT);
    print("       Show BIOS date\n");

    printc("  TICKS", BRIGHT);
    print("      Show timer ticks since boot\n");

    printc("  INFO", BRIGHT);
    print("       Show TB-32 hardware information\n");

    printc("  BEEP", BRIGHT);
    print("       Play a short speaker tone\n");

    printc("  COLORS", BRIGHT);
    print("     Display some text colours\n");

    printc("  ABOUT", BRIGHT);
    print("      About this operating system\n");

    printc("  REBOOT", YELLOW);
    print("     Restart the virtual PC\n");

    printc("  SHUTDOWN", RED);
    print("   Power the virtual PC off\n");
}

void show_info() {
    printc("TOOBAD TB-32\n", CYAN);
    print("CPU:          TB-32 custom 32-bit CPU\n");
    print("Registers:    16\n");
    print("Instruction:  fixed 4-byte instructions\n");
    print("RAM:          16 MiB\n");
    print("Kernel load:  0x");
    printhex(0x00010000, 8);
    nl();
    print("Text VRAM:    0x");
    printhex(0x02000000, 8);
    nl();
    print("BIOS ROM:     0x");
    printhex(0x0F000000, 8);
    nl();
}

void show_colors() {
    printc("BRIGHT\n", BRIGHT);
    printc("GREEN\n", GREEN);
    printc("YELLOW\n", YELLOW);
    printc("CYAN\n", CYAN);
    printc("BLUE\n", BLUE);
    printc("MAGENTA\n", MAGENTA);
    printc("RED\n", RED);
}

void show_about() {
    printc("TBOS 0.1\n", CYAN);
    print("A small operating system written specifically for TB-32.\n");
    print("It is not x86 code and it is not a Python fake shell.\n");
    print("The kernel is compiled into native TB-32 machine code.\n");
}

/* --------------------------------------------------------------------------
   Command interpreter
   -------------------------------------------------------------------------- */

void execute_command() {
    if (command[0] == 0) return;

    if (streq(command, "HELP")) {
        show_help();
        return;
    }

    if (streq(command, "VER")) {
        printc("TBOS version 0.1\n", CYAN);
        return;
    }

    if (streq(command, "CLS")) {
        cls();
        return;
    }

    if (streq(command, "ECHO")) {
        print(argument);
        nl();
        return;
    }

    if (streq(command, "TIME")) {
        show_time();
        return;
    }

    if (streq(command, "DATE")) {
        show_date();
        return;
    }

    if (streq(command, "TICKS")) {
        printn(sys_ticks());
        nl();
        return;
    }

    if (streq(command, "INFO")) {
        show_info();
        return;
    }

    if (streq(command, "BEEP")) {
        beep();
        print("beep!\n");
        return;
    }

    if (streq(command, "COLORS")) {
        show_colors();
        return;
    }

    if (streq(command, "ABOUT")) {
        show_about();
        return;
    }

    if (streq(command, "REBOOT")) {
        printc("Restarting TB-32...\n", YELLOW);
        sys_out(P_POWER, 2);
        while (1) sys_halt();
    }

    if (streq(command, "SHUTDOWN")) {
        printc("TBOS is shutting down...\n", YELLOW);
        sys_out(P_POWER, 1);
        while (1) sys_halt();
    }

    printc("Unknown command: ", RED);
    printc(command, BRIGHT);
    nl();
    print("Type HELP for the command list.\n");
}

void shell() {
    while (1) {
        printc("TBOS", CYAN);
        printc("> ", BRIGHT);
        readline(line, LINE_MAX);
        parse_line(line);
        execute_command();
    }
}

/* --------------------------------------------------------------------------
   Kernel entry from start.asm
   -------------------------------------------------------------------------- */

int main() {
    sys_flushkeys();
    text_attr = NORMAL;
    cls();

    printc("========================================\n", CYAN);
    printc("              TBOS 0.1\n", BRIGHT);
    printc("          Operating System for TB-32\n", CYAN);
    printc("========================================\n\n", CYAN);

    print("Native TB-32 kernel loaded at 0x");
    printhex(0x00010000, 8);
    print(".\n");
    print("Type ");
    printc("HELP", BRIGHT);
    print(" to see the commands.\n\n");

    shell();
    return 0;
}
