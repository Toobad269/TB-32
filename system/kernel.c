/* ==========================================================================
   TOOBAD-OS  --  Kernel and command interpreter

   Wird vom Bootsektor geladen und uebernimmt den Rechner. Geschrieben in der
   eigenen Sprache TC und uebersetzt vom eigenen Compiler (tools/tcc.py) in
   Maschinencode fuer die eigene CPU.

   (Kommentare bleiben auf Deutsch -- die Oberflaeche ist Englisch, wie bei
   einem echten Betriebssystem.)
   ========================================================================== */

#include "lib.c"
#include "fs.c"
#include "edit.c"
#include "font8.c"
#include "diag.c"
#include "proc.c"
#include "term.c"
#include "syscall.c"
#include "gui.c"
#include "coder.c"
#include "paint.c"
#include "word.c"
#include "dialog.c"

#define CMDMAX 128

char cmdline[CMDMAX];
char cmd[32];
char arg1[64];
char arg2[64];
char progname[24];
char argzeile[80];      /* Befehlszeile fuer den Python-Interpreter */

/* --- Eingabezeile in Woerter zerlegen ------------------------------------ */

int copy_word(char* src, int pos, char* out, int max) {
    int n;
    n = 0;
    while (src[pos] == ' ') pos++;
    while (src[pos] && src[pos] != ' ' && n < max - 1) {
        out[n] = src[pos];
        n++;
        pos++;
    }
    out[n] = 0;
    return pos;
}

void parse(char* line) {
    int p;
    p = copy_word(line, 0, cmd, 32);
    p = copy_word(line, p, arg1, 64);
    p = copy_word(line, p, arg2, 64);
}

/* ==========================================================================
   Help
   ========================================================================== */

void cmd_help(char* topic) {
    if (topic[0] == 0) {
        printc("\nTOOBAD-OS command reference\n\n", CYAN);
        printc("File commands\n", BRIGHT);
        print("  DIR [/W]        List files on the disk\n");
        print("  TYPE <file>     Display the contents of a file\n");
        print("  MORE <file>     Display a file one screen at a time\n");
        print("  EDIT <file>     Full screen text editor\n");
        print("  COPY <a> <b>    Copy a file\n");
        print("  REN <a> <b>     Rename a file\n");
        print("  DEL <file>      Delete a file\n");
        printc("\nDirectory commands\n", BRIGHT);
        print("  CD <name>       Change directory (CD .. goes up, CD \\ to root)\n");
        print("  MD <name>       Create a directory\n");
        print("  RD <name>       Remove an empty directory\n");
        printc("\nDisk commands\n", BRIGHT);
        print("  CHKDSK          Check the file system for errors\n");
        print("  FORMAT          Erase and re-initialise the disk\n");
        print("  VOL             Show volume information\n");
        print("  FC <a> <b>      Compare two files byte by byte\n");
        printc("\nSystem commands\n", BRIGHT);
        print("  CLS  VER  MEM  TIME  DATE  ECHO  COLOR <hex>\n");
        print("  SYSTEMINFO      Detailed hardware and system report\n");
        print("  DUMP <addr>     Hexadecimal memory dump\n");
        print("  DISPTEST        Display adapter test pattern\n");
        print("  TEMP [mode]     Temperature, fan and throttling\n");
        print("  SHUTDOWN [/R]   Power off, /R restarts the machine\n");
        print("  PgUp key        Scroll back through earlier output\n");
        printc("\nProcess commands\n", BRIGHT);
        print("  START <file> [/B]  Run a program, /B runs it in background\n");
        print("  TASKLIST        Show running processes\n");
        print("  TASKKILL <id>   Terminate a process\n");
        printc("\nGraphical interface\n", BRIGHT);
        print("  WIN             Start the desktop environment\n");
        print("\nType HELP <command> for details.\n");
        return;
    }
    if (stricmp(topic, "dir") == 0) {
        print("DIR [/W]\n  Lists the files stored on drive A:.\n");
        print("  /W  Wide format, names only.\n");
    } else if (stricmp(topic, "start") == 0) {
        print("START <file> [/B]\n  Loads a .TBX program from disk and runs it.\n");
        print("  /B  Starts the program in the background; the prompt returns\n");
        print("      immediately and the program keeps running.\n");
    } else if (stricmp(topic, "chkdsk") == 0) {
        print("CHKDSK\n  Verifies the directory, checks every file for a valid\n");
        print("  sector range and reports overlapping or lost sectors.\n");
    } else if (stricmp(topic, "dump") == 0) {
        print("DUMP <address>\n  Shows 128 bytes of memory in hexadecimal and\n");
        print("  as text. The address may be decimal or 0x-prefixed.\n");
    } else if (stricmp(topic, "shutdown") == 0) {
        print("SHUTDOWN [/R]\n  Powers the machine off. /R restarts it instead.\n");
    } else if (stricmp(topic, "color") == 0) {
        print("COLOR <hex>\n  Sets text colour. First digit is the background,\n");
        print("  second the foreground, e.g. COLOR 1F = white on blue.\n");
    } else {
        print("No help available for that command.\n");
    }
}

/* ==========================================================================
   System information
   ========================================================================== */

void cmd_ver() {
    printc("TOOBAD-OS Version 1.0\n", CYAN);
    print("TOOBAD BIOS v2.5.2, TB-32 architecture\n");
}

void cmd_mem() {
    int kb; int sekt; int belegt;
    kb = mem_get(0x000004A0);
    sekt = mem_get(0x000004A4);
    belegt = fs_used_sectors();
    printc("\nMemory\n", BRIGHT);
    print("  Total physical memory      ");
    printnc(kb, BRIGHT);
    print(" KB\n");
    print("  Kernel and system area     ");
    printnc(640, BRIGHT);
    print(" KB\n");
    print("  Available to programs      ");
    printnc(kb - 640, BRIGHT);
    print(" KB\n");
    printc("\nDisk\n", BRIGHT);
    print("  Capacity                   ");
    printnc(sekt / 2048, BRIGHT);
    print(" MB (");
    printn(sekt);
    print(" sectors)\n");
    print("  Used by files              ");
    printnc(belegt / 2, BRIGHT);
    print(" KB in ");
    printn(fs_count());
    print(" file(s)\n");
    print("  Free                       ");
    printnc((sekt - FS_DATA - belegt) / 2, BRIGHT);
    print(" KB\n");
}

void cmd_systeminfo() {
    int t; int d;
    t = sys_clock();
    d = sys_date();
    printc("\nSystem Information\n\n", CYAN);
    print("  OS Name                    TOOBAD-OS\n");
    print("  OS Version                 1.0\n");
    print("  System Manufacturer        Toobad\n");
    print("  System Model               TB-32\n");
    print("  Processor                  TOOBAD TB-32, 32-bit, 16 registers\n");
    print("  Instruction Set            fixed 4-byte words, RISC style\n");
    print("  BIOS Version               TOOBAD BIOS v2.5.2\n");
    print("  Total Physical Memory      ");
    printn(mem_get(0x000004A0));
    print(" KB\n");
    print("  Address Space              16 MB RAM, ROM at 0x0F000000\n");
    print("  Display Adapter            TB-VGA, 80x25 text / 640x400 x 256\n");
    print("  Storage Controller         TB-IDE, 512-byte sectors, DMA\n");
    print("  File System                TBFS\n");
    print("  Interrupt Vectors          256 entries at 0x00000000\n");
    print("  System Time                ");
    print2((t >> 16) & 255);
    putch(':');
    print2((t >> 8) & 255);
    putch(':');
    print2(t & 255);
    print("\n  System Date                ");
    print2(d & 255);
    putch('.');
    print2((d >> 8) & 255);
    putch('.');
    printn((d >> 16) & 65535);
    print("\n  System Up Time             ");
    printn(sys_ticks() / 100);
    print(" seconds\n");
    print("  Running Processes          ");
    printn(proc_count());
    print("\n  CPU Temperature            ");
    printn(sys_in(P_TEMP) / 10);
    print(" C, fan at ");
    printn(sys_in(P_FAN));
    print(" %");
    if (sys_in(P_THROTTLE)) {
        printc("  (throttling)", RED);
    }
    nl();
}

void cmd_time() {
    int t;
    t = sys_clock();
    print("Current time: ");
    print2((t >> 16) & 255);
    putch(':');
    print2((t >> 8) & 255);
    putch(':');
    print2(t & 255);
    nl();
}

void cmd_date() {
    int d;
    d = sys_date();
    print("Current date: ");
    print2(d & 255);
    putch('.');
    print2((d >> 8) & 255);
    putch('.');
    printn((d >> 16) & 65535);
    nl();
}

/* ==========================================================================
   File commands
   ========================================================================== */

/* Zahl rechtsbuendig in einem Feld der angegebenen Breite ausgeben */
void print_right(int wert, int breite, int farbe) {
    int stellen; int t;
    stellen = 1;
    t = wert;
    while (t >= 10) { t = t / 10; stellen++; }
    while (stellen < breite) { putch(' '); stellen++; }
    printnc(wert, farbe);
}

void cmd_dir(char* option) {
    int i; int n; int t; int breit; int spalte;
    char pfad[40];
    breit = 0;
    if (option[0] == '/' && toupper(option[1]) == 'W') breit = 1;

    print("\n Volume in drive A is TOOBAD-OS\n");
    print(" Directory of ");
    fs_path(pfad);
    print(pfad);
    print("\n\n");
    spalte = 0;
    for (i = 0; i < FS_MAXFILES; i++) {
        if (ent_type(i) == 0) continue;
        if (ent_versteckt(i)) continue;      /* Systemdateien nicht zeigen */
        if (ent_parent(i) != cwd) continue;
        if (ent_type(i) == FT_DIR) {
            if (breit) {
                putch('[');
                printc(ent_name(i), CYAN);
                putch(']');
                n = strlen(ent_name(i)) + 2;
                while (n < 18) { putch(' '); n++; }
                spalte++;
                if (spalte == 4) { nl(); spalte = 0; }
            } else {
                t = ent_time(i);
                print(" ");
                print2((t >> 16) & 255);
                putch(':');
                print2((t >> 8) & 255);
                print("       <DIR>  ");
                printc(ent_name(i), CYAN);
                nl();
            }
            continue;
        }
        if (breit) {
            printc(ent_name(i), BRIGHT);
            n = strlen(ent_name(i));
            while (n < 18) { putch(' '); n++; }
            spalte++;
            if (spalte == 4) { nl(); spalte = 0; }
            continue;
        }
        t = ent_time(i);
        print(" ");
        print2((t >> 16) & 255);
        putch(':');
        print2((t >> 8) & 255);
        print_right(ent_size(i), 12, NORMAL);
        print("  ");
        printc(ent_name(i), BRIGHT);
        nl();
    }
    if (breit && spalte) nl();
    nl();
    n = 0;
    for (i = 0; i < FS_MAXFILES; i++)
        if (ent_type(i) == FT_DIR && ent_parent(i) == cwd) n++;
    if (n) {
        print_right(n, 10, BRIGHT);
        print(" Dir(s)\n");
    }
    n = 0;
    t = 0;
    for (i = 0; i < FS_MAXFILES; i++) {
        if (ent_type(i) == FT_FILE && ent_parent(i) == cwd) { n++; t = t + ent_size(i); }
    }
    print_right(n, 10, BRIGHT);
    print(" File(s)");
    print_right(t, 12, BRIGHT);
    print(" bytes\n");
    print_right((sys_disksize() - FS_DATA - fs_used_sectors()) / 2, 10, BRIGHT);
    print(" KB free\n");
}

void cmd_type(char* name) {
    int n; int i; char* t;
    if (name[0] == 0) { printc("Syntax: TYPE <file>\n", RED); return; }
    n = fs_read(name, FILEBUF, FILEBUF_MAX);
    if (n < 0) { printc("File not found\n", RED); return; }
    t = (char*)FILEBUF;
    for (i = 0; i < n; i++) putch(t[i]);
    nl();
}

void cmd_more(char* name) {
    int n; int i; int zeilen; int k; char* t;
    if (name[0] == 0) { printc("Syntax: MORE <file>\n", RED); return; }
    n = fs_read(name, FILEBUF, FILEBUF_MAX);
    if (n < 0) { printc("File not found\n", RED); return; }
    t = (char*)FILEBUF;
    zeilen = 0;
    for (i = 0; i < n; i++) {
        putch(t[i]);
        if (t[i] == 10) {
            zeilen++;
            if (zeilen >= 22) {
                printc("-- More --", INVERS);
                k = getkey();
                print("\r          \r");
                if (keycode(k) == K_ESC) return;
                zeilen = 0;
            }
        }
    }
    nl();
}

void cmd_del(char* name) {
    if (name[0] == 0) { printc("Syntax: DEL <file>\n", RED); return; }
    if (fs_delete(name) == 0) print("File deleted\n");
    else printc("File not found\n", RED);
}

void cmd_ren(char* a, char* b) {
    int r;
    if (a[0] == 0 || b[0] == 0) { printc("Syntax: REN <old> <new>\n", RED); return; }
    r = fs_rename(a, b);
    if (r == 0) print("File renamed\n");
    else if (r == 0 - 2) printc("A file with that name already exists\n", RED);
    else printc("File not found\n", RED);
}

void cmd_copy(char* a, char* b) {
    int n;
    if (a[0] == 0 || b[0] == 0) { printc("Syntax: COPY <source> <target>\n", RED); return; }
    n = fs_read(a, FILEBUF, FILEBUF_MAX);
    if (n < 0) { printc("Source file not found\n", RED); return; }
    if (fs_write(b, FILEBUF, n) == 0) {
        print("        1 file(s) copied\n");
    } else {
        printc("Insufficient disk space\n", RED);
    }
}

/* ==========================================================================
   Disk commands
   ========================================================================== */

void cmd_vol() {
    printc("\n Volume in drive A is TOOBAD-OS\n", NORMAL);
    print(" Volume Serial Number is TB32-0001\n");
    print(" File system is TBFS\n");
    print(" Total size ");
    printn(sys_disksize() / 2048);
    print(" MB, sector size 512 bytes\n");
}

void cmd_chkdsk() {
    int i; int j; int fehler; int belegt; int s1; int e1; int s2; int e2;
    printc("\nChecking file system on A:\n\n", CYAN);
    print("The type of the file system is TBFS.\n\n");

    fehler = 0;
    print("Stage 1: Examining directory entries ...\n");
    for (i = 0; i < FS_MAXFILES; i++) {
        if (ent_used(i) != 1) continue;
        if (ent_start(i) < FS_DATA || ent_start(i) >= sys_disksize()) {
            printc("  Bad start sector in entry ", RED);
            printn(i);
            nl();
            fehler++;
        }
        if (ent_name(i)[0] == 0) {
            printc("  Entry with empty name found\n", RED);
            fehler++;
        }
    }

    print("Stage 2: Checking for overlapping files ...\n");
    for (i = 0; i < FS_MAXFILES; i++) {
        if (ent_used(i) != 1) continue;
        s1 = ent_start(i);
        e1 = s1 + sectors_for(ent_size(i));
        for (j = i + 1; j < FS_MAXFILES; j++) {
            if (ent_used(j) != 1) continue;
            s2 = ent_start(j);
            e2 = s2 + sectors_for(ent_size(j));
            if (s1 < e2 && s2 < e1) {
                printc("  Cross-linked: ", RED);
                print(ent_name(i));
                print(" and ");
                print(ent_name(j));
                nl();
                fehler++;
            }
        }
    }

    print("Stage 3: Verifying free space ...\n\n");
    belegt = fs_used_sectors();
    printnc(sys_disksize() * 512, BRIGHT);
    print(" bytes total disk space\n");
    printnc(FS_DATA * 512, BRIGHT);
    print(" bytes in system area\n");
    printnc(belegt * 512, BRIGHT);
    print(" bytes in ");
    printn(fs_count());
    print(" file(s)\n");
    printnc((sys_disksize() - FS_DATA - belegt) * 512, BRIGHT);
    print(" bytes available on disk\n\n");

    if (fehler == 0) printc("File system is clean. No errors found.\n", GREEN);
    else {
        printc("Errors found: ", RED);
        printn(fehler);
        nl();
    }
}

void cmd_format() {
    int k;
    printc("WARNING: ALL DATA ON DRIVE A: WILL BE LOST!\n", YELLOW);
    print("Proceed with Format (Y/N)? ");
    k = toupper(keychar(getkey()));
    putch(k);
    nl();
    if (k == 'Y') {
        print("Formatting ...\n");
        fs_format();
        print("Format complete.\n\n");
        printn(sys_disksize() * 512);
        print(" bytes total disk space\n");
    } else {
        print("Format cancelled\n");
    }
}

/* --- Temperatur und Kühlung --------------------------------------------- */

#define P_TEMP        0xA0
#define P_FAN         0xA1
#define P_THROTTLE    0xA2
#define P_TEMP_LIMIT  0xA3
#define P_FANMODE     0xA4
#define P_TEMP_MAX    0xA5

void balken(int wert, int maximum, int breite, int farbe) {
    int i; int voll;
    voll = wert * breite / maximum;
    if (voll > breite) voll = breite;
    putch('[');
    for (i = 0; i < breite; i++) {
        if (i < voll) putcolor(219, farbe);
        else putcolor(176, 0x08);
    }
    putch(']');
}

void cmd_temp(char* arg) {
    int t; int fan; int thr; int lim; int hoch; int farbe;

    if (arg[0]) {                                /* TEMP AUTO|QUIET|FULL */
        if (stricmp(arg, "auto") == 0)  { sys_out(P_FANMODE, 0); print("Fan control: automatic\n"); return; }
        if (stricmp(arg, "quiet") == 0) { sys_out(P_FANMODE, 1); print("Fan control: quiet\n"); return; }
        if (stricmp(arg, "full") == 0)  { sys_out(P_FANMODE, 2); print("Fan control: full speed\n"); return; }
        if (stricmp(arg, "reset") == 0) { sys_out(P_TEMP_MAX, 0); print("Peak temperature cleared\n"); return; }
        printc("Syntax: TEMP [AUTO|QUIET|FULL|RESET]\n", RED);
        return;
    }

    t = sys_in(P_TEMP);
    fan = sys_in(P_FAN);
    thr = sys_in(P_THROTTLE);
    lim = sys_in(P_TEMP_LIMIT);
    hoch = sys_in(P_TEMP_MAX);

    farbe = GREEN;
    if (t > 700) farbe = YELLOW;
    if (t > lim * 10) farbe = RED;

    printc("\nThermal status\n\n", CYAN);
    print("  CPU temperature   ");
    printnc(t / 10, farbe);
    putch('.');
    printnc(t % 10, farbe);
    print(" C   ");
    balken(t / 10, 110, 30, farbe);
    nl();
    print("  Fan speed         ");
    printnc(fan, BRIGHT);
    print(" %       ");
    balken(fan, 100, 30, CYAN);
    nl();
    print("  Throttling        ");
    if (thr) {
        printnc(thr, RED);
        print(" %       ");
        balken(thr, 100, 30, RED);
        nl();
        printc("  The CPU is running slower to cool down.\n", YELLOW);
    } else {
        printc("none\n", GREEN);
    }
    print("  Throttle limit    ");
    printn(lim);
    print(" C\n  Peak since boot   ");
    printn(hoch / 10);
    putch('.');
    printn(hoch % 10);
    print(" C\n\n  Fan control: TEMP AUTO | TEMP QUIET | TEMP FULL\n");
}

/* --- Ordner ------------------------------------------------------------- */

void cmd_md(char* name) {
    int r;
    if (name[0] == 0) { printc("Syntax: MD <name>\n", RED); return; }
    r = fs_mkdir(name);
    if (r == 0) print("Directory created\n");
    else if (r == 0 - 2) printc("A file or directory with that name exists\n", RED);
    else printc("Directory table is full\n", RED);
}

void cmd_cd(char* name) {
    int r;
    char pfad[40];
    if (name[0] == 0) {                      /* ohne Angabe: Pfad anzeigen */
        fs_path(pfad);
        print(pfad);
        nl();
        return;
    }
    r = fs_chdir(name);
    if (r == 0 - 1) printc("The system cannot find the path specified\n", RED);
    else if (r == 0 - 2) printc("Not a directory\n", RED);
}

void cmd_rd(char* name) {
    int r;
    if (name[0] == 0) { printc("Syntax: RD <name>\n", RED); return; }
    r = fs_rmdir(name);
    if (r == 0) print("Directory removed\n");
    else if (r == 0 - 3) printc("The directory is not empty\n", RED);
    else if (r == 0 - 2) printc("Not a directory\n", RED);
    else printc("Directory not found\n", RED);
}

/* --- FC: zwei Dateien Byte fuer Byte vergleichen ------------------------ */

#define FC_BUF1  0x00400000
#define FC_BUF2  0x00500000
#define FC_MAX   0x000F0000

void cmd_fc(char* a, char* b) {
    int n1; int n2; int i; int unterschiede; int erste;
    char* p1;
    char* p2;

    if (a[0] == 0 || b[0] == 0) { printc("Syntax: FC <file1> <file2>\n", RED); return; }
    n1 = fs_read(a, FC_BUF1, FC_MAX);
    if (n1 < 0) { printc("File not found: ", RED); print(a); nl(); return; }
    n2 = fs_read(b, FC_BUF2, FC_MAX);
    if (n2 < 0) { printc("File not found: ", RED); print(b); nl(); return; }

    print("\nComparing ");
    printc(a, BRIGHT);
    print(" (");
    printn(n1);
    print(" bytes) and ");
    printc(b, BRIGHT);
    print(" (");
    printn(n2);
    print(" bytes)\n");

    if (n1 != n2) {
        printc("Files are different: sizes do not match.\n", RED);
        return;
    }
    p1 = (char*)FC_BUF1;
    p2 = (char*)FC_BUF2;
    unterschiede = 0;
    erste = 0 - 1;
    for (i = 0; i < n1; i++) {
        if (p1[i] != p2[i]) {
            unterschiede++;
            if (erste < 0) erste = i;
        }
    }
    if (unterschiede == 0) {
        printc("FC: no differences encountered -- the files are identical.\n", GREEN);
    } else {
        printc("Files are different: ", RED);
        printn(unterschiede);
        print(" byte(s), first at offset ");
        printn(erste);
        nl();
    }
}

/* ==========================================================================
   Debug / hardware
   ========================================================================== */

int parse_addr(char* s) {
    int v; int i;
    if (s[0] == '0' && toupper(s[1]) == 'X') {
        v = 0;
        i = 2;
        while (s[i]) {
            v = v * 16;
            if (s[i] >= '0' && s[i] <= '9') v = v + s[i] - '0';
            else v = v + (toupper(s[i]) - 'A' + 10);
            i++;
        }
        return v;
    }
    return atoi(s);
}

void cmd_dump(char* a) {
    int addr; int zeile; int i; int b; char* p;
    if (a[0] == 0) { printc("Syntax: DUMP <address>\n", RED); return; }
    addr = parse_addr(a);
    nl();
    for (zeile = 0; zeile < 8; zeile++) {
        sys_puthex(addr, CYAN, 8);
        print("  ");
        p = (char*)addr;
        for (i = 0; i < 16; i++) {
            b = p[i] & 255;
            sys_puthex(b, BRIGHT, 2);
            putch(' ');
            if (i == 7) putch(' ');
        }
        print(" ");
        for (i = 0; i < 16; i++) {
            b = p[i] & 255;
            if (b < 32 || b > 126) b = '.';
            putch(b);
        }
        nl();
        addr = addr + 16;
    }
}

void cmd_color(char* a) {
    int c;
    if (a[0] == 0) { text_attr = NORMAL; print("Colour reset\n"); return; }
    c = parse_addr(a);
    if (c == 0) {
        c = 0;
        if (a[0] >= '0' && a[0] <= '9') c = (a[0] - '0') * 16;
        else c = (toupper(a[0]) - 'A' + 10) * 16;
        if (a[1] >= '0' && a[1] <= '9') c = c + a[1] - '0';
        else c = c + toupper(a[1]) - 'A' + 10;
    }
    text_attr = c;
    print("Colour set\n");
}

/* ==========================================================================
   Processes and programs
   ========================================================================== */

void cmd_tasklist() {
    int i; int z;
    printc("\nImage Name          PID   Status      CPU Time\n", CYAN);
    print("==========================================================\n");
    for (i = 0; i < MAXPROC; i++) {
        if (p_state[i] == PS_FREI) continue;
        print(proc_name(i));
        z = strlen(proc_name(i));
        while (z < 20) { putch(' '); z++; }
        printn(i);
        print("     ");
        z = p_state[i];
        if (z == PS_LAEUFT)   printc("Running     ", GREEN);
        if (z == PS_BEREIT)   printc("Ready       ", BRIGHT);
        if (z == PS_SCHLAEFT) printc("Sleeping    ", NORMAL);
        printn(p_ticks[i] * 10);
        print(" ms\n");
    }
    print("\nMultitasking: ");
    if (mt_active) printc("enabled", GREEN);
    else printc("disabled", NORMAL);
    print("     Context switches: ");
    printnc(p_switches, BRIGHT);
    nl();
}

void cmd_taskkill(char* a) {
    int n;
    n = atoi(a);
    if (a[0] == 0) { printc("Syntax: TASKKILL <pid>\n", RED); return; }
    if (n <= 0 || n >= MAXPROC || p_state[n] == PS_FREI) {
        printc("No process with that ID\n", RED);
        return;
    }
    p_state[n] = PS_FREI;
    print("SUCCESS: process ");
    printn(n);
    print(" has been terminated\n");
}

void cmd_start(char* name, char* option) {
    int r; int hg; int i; int j; int n;
    char args[80];
    if (name[0] == 0) { printc("Syntax: START <file.TBX> [/B] [Argumente]\n", RED); return; }
    hg = 0;

    /* Alles hinter dem Programmnamen bekommt das Programm als Argumente.
       /B darf dabei an JEDER Stelle stehen -- frueher zaehlte nur das erste
       Wort danach, und "START X.TBX COLORS /B" lief deshalb im Vordergrund
       und blockierte die Kommandozeile. */
    i = 0;
    while (cmdline[i] == ' ') i++;
    while (cmdline[i] && cmdline[i] != ' ') i++;      /* START ueberspringen */
    while (cmdline[i] == ' ') i++;
    while (cmdline[i] && cmdline[i] != ' ') i++;      /* Programmname */

    n = 0;
    while (cmdline[i] && n < 78) {
        while (cmdline[i] == ' ') i++;
        if (cmdline[i] == 0) break;
        j = i;
        while (cmdline[j] && cmdline[j] != ' ') j++;  /* ein Wort */
        if ((cmdline[i] == '/' || cmdline[i] == '-')
            && toupper(cmdline[i + 1]) == 'B' && j == i + 2) {
            hg = 1;                                   /* das Wort war /B */
        } else if (cmdline[i] == '&' && j == i + 1) {
            hg = 1;
        } else {
            if (n > 0 && n < 78) { args[n] = ' '; n++; }
            while (i < j && n < 78) { args[n] = cmdline[i]; n++; i++; }
        }
        i = j;
    }
    args[n] = 0;
    prog_setargs(args);

    r = prog_run(name, hg);
    if (r == 0 - 1) {
        printc("Program not found: ", RED);
        printc(name, BRIGHT);
        print("\nUse DIR to list the programs on this disk.\n");
        return;
    }
    if (hg) {
        print("Started in background, process ID ");
        printnc(r, BRIGHT);
        nl();
    }
}

/* ==========================================================================
   Shell
   ========================================================================== */

void boot_screen() {
    sys_cls(NORMAL);
    sys_hline(0, 0, 80, 32, 0x1F);
    sys_putsat(2, 0, "TOOBAD-OS 2.5.2", 0x1F);
    sys_putsat(62, 0, "TB-32 System", 0x1F);
    sys_setcursor(0, 2);
}

void shell() {
    int n; int i;
    char pfad[40];
    while (1) {
        color(NORMAL);
        fs_path(pfad);
        printc(pfad, GREEN);
        printc("> ", GREEN);
        n = readline(cmdline, CMDMAX);
        nl();
        if (n <= 0) continue;
        parse(cmdline);

        if      (stricmp(cmd, "help") == 0)       cmd_help(arg1);
        else if (stricmp(cmd, "?") == 0)          cmd_help(arg1);
        else if (stricmp(cmd, "ver") == 0)        cmd_ver();
        else if (stricmp(cmd, "cls") == 0)        cls();
        else if (stricmp(cmd, "mem") == 0)        cmd_mem();
        else if (stricmp(cmd, "systeminfo") == 0) cmd_systeminfo();
        else if (stricmp(cmd, "time") == 0)       cmd_time();
        else if (stricmp(cmd, "date") == 0)       cmd_date();
        else if (stricmp(cmd, "echo") == 0)       { print(cmdline + 5); nl(); }
        else if (stricmp(cmd, "color") == 0)      cmd_color(arg1);

        else if (stricmp(cmd, "dir") == 0)        cmd_dir(arg1);
        else if (stricmp(cmd, "type") == 0)       cmd_type(arg1);
        else if (stricmp(cmd, "more") == 0)       cmd_more(arg1);
        else if (stricmp(cmd, "del") == 0)        cmd_del(arg1);
        else if (stricmp(cmd, "erase") == 0)      cmd_del(arg1);
        else if (stricmp(cmd, "ren") == 0)        cmd_ren(arg1, arg2);
        else if (stricmp(cmd, "copy") == 0)       cmd_copy(arg1, arg2);
        else if (stricmp(cmd, "edit") == 0) {
            if (arg1[0] == 0) printc("Syntax: EDIT <file>\n", RED);
            else edit(arg1);
        }

        else if (stricmp(cmd, "chkdsk") == 0)     cmd_chkdsk();
        else if (stricmp(cmd, "format") == 0)     cmd_format();
        else if (stricmp(cmd, "vol") == 0)        cmd_vol();
        else if (stricmp(cmd, "temp") == 0)       cmd_temp(arg1);
        else if (stricmp(cmd, "md") == 0)         cmd_md(arg1);
        else if (stricmp(cmd, "mkdir") == 0)      cmd_md(arg1);
        else if (stricmp(cmd, "cd") == 0)         cmd_cd(arg1);
        else if (stricmp(cmd, "chdir") == 0)      cmd_cd(arg1);
        else if (stricmp(cmd, "rd") == 0)         cmd_rd(arg1);
        else if (stricmp(cmd, "rmdir") == 0)      cmd_rd(arg1);
        else if (stricmp(cmd, "fc") == 0)         cmd_fc(arg1, arg2);

        else if (stricmp(cmd, "dump") == 0)       cmd_dump(arg1);
        else if (stricmp(cmd, "disptest") == 0)   display_test();

        else if (stricmp(cmd, "start") == 0)      cmd_start(arg1, arg2);
        else if (stricmp(cmd, "tasklist") == 0)   cmd_tasklist();
        else if (stricmp(cmd, "taskkill") == 0)   cmd_taskkill(arg1);

        else if (stricmp(cmd, "win") == 0 || stricmp(cmd, "desktop") == 0) {
            /* Der Desktop laeuft in Prozess 0. Ein zweiter Aufruf aus dem
               Terminalfenster heraus wuerde zwei Oberflaechen auf denselben
               Bildschirm malen -- deshalb hier abfangen. */
            if (gui_running) {
                printc("The desktop is already running.\n", YELLOW);
                print("This window is part of it. Use EXIT to close the window.\n");
            } else {
                gui_main();
            }
        }

        else if (stricmp(cmd, "shutdown") == 0) {
            if (arg1[0] == '/' && toupper(arg1[1]) == 'R') {
                print("The system is restarting ...\n");
                sleep(40);
                sys_out(P_POWER, 2);
            } else {
                print("The system is shutting down ...\n");
                sleep(30);
                sys_out(P_POWER, 1);
            }
        }
        else if (stricmp(cmd, "exit") == 0) {
            if (term_aktiv) return;              /* Terminalfenster schliessen */
            print("Not running in a window.\n");
        }
        else if (stricmp(cmd, "tbcmd") == 0) {
            /* Aus dem Fenster in die grosse Vollbildkonsole. Gegenstueck
               zu WIN, das zurueck in den Schreibtisch fuehrt. */
            if (term_aktiv) {
                term_aktiv = 0;
                gui_running = 0;
            } else {
                print("You are already in the full-screen console.\n");
            }
        }
        else if (stricmp(cmd, "reboot") == 0) {
            print("The system is restarting ...\n");
            sleep(40);
            sys_out(P_POWER, 2);
        }
        else {
            /* Kein eingebauter Befehl: dann suchen wir ein gleichnamiges
               Programm auf der Platte -- genau wie DOS und Unix es tun.
               So wird aus  ASM QUELLE.ASM ZIEL.TBX  ein Programmaufruf. */
            n = strlen(cmd);
            i = 0;
            while (cmdline[i] == ' ') i++;
            while (cmdline[i] && cmdline[i] != ' ') i++;   /* hinter den Namen */

            if (endet_auf(cmd, ".PY")) {
                /* Ein Python-Skript ist kein Maschinenprogramm -- es braucht
                   den Interpreter davor. Der Benutzer soll GUESS.PY tippen
                   duerfen und nicht wissen muessen, dass PY.TBX existiert.
                   Genau so macht es ein grosses System mit der Zeile #!. */
                strcpy(progname, "PY.TBX");
                strncpy(argzeile, cmd, 20);
                strcat(argzeile, cmdline + i);
                prog_setargs(argzeile);
            } else {
                if (n > 4 && cmd[n - 4] == '.') strncpy(progname, cmd, 20);
                else {
                    strncpy(progname, cmd, 16);
                    strcat(progname, ".TBX");
                }
                prog_setargs(cmdline + i);
            }
            if (prog_run(progname, 0) == 0 - 1) {
                printc("'", RED);
                printc(cmd, BRIGHT);
                printc("' is not recognized as a command or program.\n", RED);
                print("Type HELP for a list of commands, DIR for programs.\n");
            }
        }
    }
}

/* ==========================================================================
   Einrichtung beim ersten Start, danach die Anmeldung

   Beim allerersten Hochfahren gibt es noch keinen Benutzer. Dann fragt das
   System nach Name und Passwort und legt beides in \SYSTEM\USER.DAT ab.
   Jeder weitere Start verlangt das Passwort, bevor die Kommandozeile kommt.

   WAS DAS IST UND WAS NICHT: Es haelt neugierige Leute auf. Es ist KEINE
   Sicherheit. Die Platte ist unverschluesselt -- wer hd0.img in die Hand
   bekommt, liest alles mit einem Hex-Editor. Das Passwort liegt nur als
   Pruefsumme da, nicht mit einem richtigen Verfahren; der TB-32 hat keins.
   Wer die Datei loescht, ist wieder drin -- so wie ein BIOS-Passwort weg
   ist, wenn man die Knopfzelle zieht. Das passt zu der Zeit, die wir
   nachbauen, und es soll niemand fuer mehr halten.
   ========================================================================== */

#define CM_BOOTMODE 0x1D             /* 0 = Schreibtisch, 1 = Textkonsole */

#define USER_BUF   0x000C0000
#define USER_NAME  0                     /* 20 Byte Name */
#define USER_HASH  20                    /* 4 Byte Pruefsumme des Passworts */
#define USER_LEN   24

int pw_summe(char* s) {
    int h;
    h = 0x1234;
    while (*s) { h = h * 31 + *s; s++; }
    return h;
}

/* Liest eine Zeile, zeigt aber Sterne statt der Zeichen. */
int passwort_lesen(char* buf, int max) {
    int n; int k; int c; int code;
    n = 0;
    while (1) {
        k = getkey();
        c = keychar(k);
        code = keycode(k);
        if (code == K_ENTER) { buf[n] = 0; nl(); return n; }
        if (code == K_BACKSPACE) {
            if (n > 0) { n--; putch(8); }
            continue;
        }
        if (c >= 32 && c < 127 && n < max - 1) {
            buf[n] = c;
            n++;
            putcolor('*', BRIGHT);
        }
    }
}

void benutzer_anlegen(char* name, char* pw) {
    int n;
    memset((char*)USER_BUF, 0, USER_LEN);
    strncpy((char*)(USER_BUF + USER_NAME), name, 19);
    mem_put(USER_BUF + USER_HASH, pw_summe(pw));
    fs_write("USER.DAT", USER_BUF, USER_LEN);
    n = fs_find("USER.DAT");
    if (n >= 0) { ent_verstecken(n); fs_save_dir(); }
}

int benutzer_passt(char* pw) {
    return pw_summe(pw) == mem_get(USER_BUF + USER_HASH);
}

int benutzer_vorhanden() {
    return fs_read("USER.DAT", USER_BUF, USER_LEN) == USER_LEN;
}

char* benutzer_name() { return (char*)(USER_BUF + USER_NAME); }

void ersteinrichtung() {
    char name[24];
    char pw1[32];
    char pw2[32];

    nl();
    printc("Welcome to TOOBAD-OS
", CYAN);
    print("This is the first start of this machine.\n\n");

    while (1) {
        print("User name: ");
        if (readline(name, 20) > 0) { nl(); break; }
        nl();
        printc("The name must not be empty.\n", RED);
    }
    while (1) {
        print("Password:  ");
        passwort_lesen(pw1, 30);
        print("Repeat:    ");
        passwort_lesen(pw2, 30);
        if (strcmp(pw1, pw2) == 0) break;
        printc("The two entries differ. Try again.\n\n", RED);
    }

    memset((char*)USER_BUF, 0, USER_LEN);
    strncpy((char*)(USER_BUF + USER_NAME), name, 19);
    mem_put(USER_BUF + USER_HASH, pw_summe(pw1));
    if (fs_write("USER.DAT", USER_BUF, USER_LEN) < 0)
        printc("\nCould not save the account.\n", RED);
    else {
        nl();
        printc("Account created.\n", GREEN);
        print("Keep it -- deleting USER.DAT is the only way back in.\n");
    }
    nl();
}

void anmelden() {
    char pw[32];
    int falsch;
    falsch = 0;
    nl();
    print("User: ");
    printc((char*)(USER_BUF + USER_NAME), BRIGHT);
    nl();
    while (1) {
        print("Password: ");
        passwort_lesen(pw, 30);
        if (pw_summe(pw) == mem_get(USER_BUF + USER_HASH)) {
            printc("Welcome back.\n\n", GREEN);
            return;
        }
        falsch++;
        printc("Wrong password.\n", RED);
        /* Nach jedem Fehlversuch etwas laenger warten -- so wird stures
           Durchprobieren muehsam, ohne jemanden auszusperren. */
        sleep(falsch * 50);
    }
}

void benutzer_pruefen() {
    if (benutzer_vorhanden() == 0) {
        ersteinrichtung();               /* frische Platte: einrichten */
        return;
    }
    /* Ein leeres Passwort heisst: dieser Rechner ist nicht gesperrt. So
       kommt eine frisch gebaute Maschine ohne Nachfrage hoch -- und die
       Testwerkzeuge auch, die niemanden zum Tippen haben. Wer eins setzt,
       wird gefragt. */
    if (mem_get(USER_BUF + USER_HASH) == pw_summe("")) return;
    anmelden();
}

int main() {
    int formatiert;

    boot_screen();
    printc("TOOBAD-OS 2.5.2\n", CYAN);
    print("Copyright (C) Toobad. All rights reserved.\n\n");

    syscall_init();
    mt_enable();                       /* Multitasking an, Shell wird Prozess 0 */
    print("Mounting file system on A: ... ");
    formatiert = fs_mount();
    if (formatiert) printc("OK\n", GREEN);
    else printc("new disk, initialised\n", YELLOW);

    /* Startziel: Schreibtisch oder Textkonsole. Steht im CMOS neben Quick
       Boot, aenderbar im Setup und im Control Panel. Standard ist der
       Schreibtisch -- die Testwerkzeuge bekommen ein eigenes CMOS mit
       Konsole, weil sie den TEXTbildschirm auslesen. */
    if (cmos_get(CM_BOOTMODE) == 0) {
        /* Der Blitter braucht die Adresse des Zeichensatzes, sonst malt er
           aus dem Nichts -- der Anmeldeschirm war fast leer. gui_main()
           setzt sie sonst selbst, laeuft hier aber erst danach. */
        sys_setmode(1 + 256);
        sys_out(P_BLT_SRC, (int)font8);
        sys_out(P_MCUR_ON, 0);
        sys_flushkeys();
        /* Abmelden fuehrt zurueck zum Anmeldeschirm, ohne den Rechner
           auszuschalten -- deshalb die Schleife. Danach wird IMMER gefragt,
           auch bei einem offenen Konto: wer sich abmeldet, will das. */
        formatiert = 0;
        while (1) {
            /* gui_main() schaltet beim Verlassen in den Textmodus zurueck.
               Vor jedem Durchgang also wieder Grafik an, sonst malt die
               Anmeldung ins Leere. */
            sys_setmode(1 + 256);
            sys_out(P_BLT_SRC, (int)font8);
            if (benutzer_vorhanden() == 0) gui_anmelden(1);
            else if (formatiert || mem_get(USER_BUF + USER_HASH) != pw_summe(""))
                gui_anmelden(0);
            gui_abmelden = 0;
            gui_main();
            if (gui_abmelden == 0) break;
            formatiert = 1;
        }
        sys_setmode(0);
        cls(NORMAL);
    } else {
        benutzer_pruefen();
    }

    print("Starting command interpreter ...\n\n");
    print("Type ");
    printc("HELP", BRIGHT);
    print(" for a list of commands, ");
    printc("WIN", BRIGHT);
    print(" for the desktop.\n\n");

    shell();
    return 0;
}
