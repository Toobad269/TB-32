/* ==========================================================================
   Die Systemaufrufe von TOOBAD-OS

   Ein Programm, das von der Platte geladen wird, kennt weder den Kernel noch
   das BIOS. Es kennt nur eine einzige Tuer: INT 0x40 mit einer Nummer in r0.
   Diese Datei ist die andere Seite dieser Tuer.
   ========================================================================== */

#define PROG_ADDR   0x00200000       /* hierhin werden Programme geladen */
#define PROG_ARGS   0x00008200       /* hier findet ein Programm seine Argumente */
#define PROG_MAX    0x00080000       /* bis zu 512 KB pro Programm */
#define PROG_STACK  0x002F0000

#define P_BLT_X    0x44
#define P_BLT_Y    0x45
#define P_BLT_W    0x46
#define P_BLT_H    0x47
#define P_BLT_COL  0x48
#define P_BLT_CMD  0x49
#define P_BLT_CHR  0x4A
#define P_BLT_BG   0x4C

int build_progress = 0;              /* 0..100, vom laufenden Programm gemeldet */
char build_status[44];
char cap_zahl[16];                   /* Zwischenablage fuer mitgeschriebene Zahlen */

int call_addr(int adresse);           /* steht in start.asm */
int syscall_asm();

int syscall(int fn, int a1, int a2, int a3, int a4) {
    /* Laeuft die Kommandozeile im Fenster, gehen auch die Ausgaben der
       gestarteten Programme dorthin -- sie merken davon nichts. */
    /* Der Mitschnitt muss HIER sitzen, nicht nur in lib.c: der Compiler
       laeuft als eigener Prozess und gibt ueber diese Systemaufrufe aus,
       nicht ueber die print-Funktionen des Kernels. */
    if (fn == 0)  {
        if (cap_aktiv) cap_putc(a1);
        if (term_aktiv) term_putc(a1, a2); else sys_putc(a1, a2);
        return 0;
    }
    if (fn == 1)  {
        if (cap_aktiv) cap_puts((char*)a1);
        if (term_aktiv) term_puts((char*)a1, a2); else sys_puts((char*)a1, a2);
        return 0;
    }
    /* Die Tastatur gehoert dem Programm im Vordergrund. Ein mit /B
       gestartetes Programm, das trotzdem eine Taste haben will, wuerde sonst
       das Tippen an der Eingabeaufforderung wegschnappen -- man tippt
       TASKLIST und es kommt ASKLIST an. Es wartet stattdessen einfach, bis
       es beendet wird. Genau so halten es grosse Systeme auch mit
       Hintergrundprozessen, die von der Tastatur lesen wollen. */
    if (fn == 2)  {
        if (term_aktiv) return term_getkey();
        if (mt_active && p_bg[p_current]) { while (1) proc_sleep(10); }
        return sys_getkey();
    }
    if (fn == 3)  { if (term_aktiv) { term_clear(); return 0; } sys_cls(a1); return 0; }
    if (fn == 4)  { proc_exit(); return 0; }
    if (fn == 5)  return sys_ticks();
    if (fn == 6)  {
        if (cap_aktiv) { itoa(a1, cap_zahl); cap_puts(cap_zahl); }
        if (term_aktiv) term_putn(a1, a2); else sys_putn(a1, a2);
        return 0;
    }
    if (fn == 7)  { sys_setcursor(a1, a2); return 0; }
    if (fn == 8)  { sys_putat(a1, a2, a3, a4); return 0; }
    if (fn == 9)  {
        if (term_aktiv) return term_has_key();
        if (mt_active && p_bg[p_current]) return 0;
        return sys_haskey();
    }
    if (fn == 10) return fs_read((char*)a1, a2, a3);
    if (fn == 11) return fs_write((char*)a1, a2, a3);
    if (fn == 12) return sys_clock();
    if (fn == 13) return sys_date();
    if (fn == 14) { proc_sleep(a1); return 0; }
    if (fn == 15) { beep(a1, a2); return 0; }
    if (fn == 16) return sys_disksize();
    /* Schaltet ein Programm den Bildschirmmodus um, waehrend der
       Schreibtisch laeuft, dann gehoert ihm ab jetzt der ganze Schirm.
       Geht es zurueck in den Textmodus, ist es fertig und die Oberflaeche
       darf sich das Bild zurueckholen. */
    if (fn == 17) {
        if (gui_running && gui_selbst == 0) {
            if (a1 & 1) {
                gui_fremd = 1;
                /* Tastatur und Ausgabe zurueck auf die echte Hardware. Sonst
                   holt sich das Programm seine Tasten aus der Warteschlange
                   des Terminalfensters -- und die fuellt die Oberflaeche,
                   die gerade schlaeft. Es kaeme keine einzige Taste an. */
                term_aktiv = 0;
            } else {
                gui_fremd = 2;
                if (term_lauf) term_aktiv = 1;
            }
        }
        sys_setmode(a1);
        return 0;
    }
    if (fn == 18) { sys_out(a1, a2); return 0; }
    if (fn == 19) return sys_in(a1);
    if (fn == 20) { sys_box(a1, a2, a3, a4); return 0; }
    if (fn == 21) { sys_hline(a1, a2, a3, a4); return 0; }
    if (fn == 22) return mem_get(0x000004A0);        /* Speichergroesse */
    if (fn == 23) { sys_flushkeys(); return 0; }
    if (fn == 24) return fs_count();
    if (fn == 25) return (int)ent_name(a1);
    if (fn == 26) return ent_used(a1);
    if (fn == 27) return ent_size(a1);
    /* Fortschritt melden: die Oberflaeche zeigt daraus einen Ladebalken.
       Programme, die im Textmodus laufen, merken davon nichts. */
    if (fn == 28) { build_progress = a1; return 0; }
    if (fn == 29) { strncpy(build_status, (char*)a1, 40); return 0; }
    /* Adresse des Zeichensatzes. Der Blitter liest ihn direkt aus dem RAM --
       ein Programm im Grafikmodus muss also nur wissen, wo er liegt, und
       braucht keinen eigenen mitzubringen. */
    if (fn == 30) return (int)font8;
    /* Datei lesen mit Suchpfad: aktueller Ordner, dann \SOURCE. Fuer
       #include im Compiler auf dem Geraet. */
    if (fn == 33) return fs_read_lib((char*)a1, a2, a3);
    /* Ein ganzer Malbefehl in EINEM Systemaufruf.
       Bisher brauchte eine gefuellte Flaeche sechs davon (x, y, w, h, Farbe,
       Kommando) -- bei einem Spiel mit vierzig Flaechen je Bild waren das
       ueber 250 Aufrufe, und der Sprung in den Kernel kostet jedes Mal.
       Die Koordinaten liegen zu zweit in einem Wort, jeweils 16 Bit, im
       Zweierkomplement -- der Blitter rechnet Werte ab 0x8000 selbst wieder
       ins Negative zurueck. */
    if (fn == 31 || fn == 32) {
        sys_out(P_BLT_X, a1 & 65535);
        sys_out(P_BLT_Y, (a1 >> 16) & 65535);
        if (fn == 31) {
            sys_out(P_BLT_W, a2 & 65535);
            sys_out(P_BLT_H, (a2 >> 16) & 65535);
            sys_out(P_BLT_COL, a3);
            sys_out(P_BLT_CMD, a4);          /* 1 = Flaeche, 2 = Rahmen */
        } else {
            sys_out(P_BLT_COL, a2 & 65535);
            sys_out(P_BLT_BG, a3);
            sys_out(P_BLT_CHR, (a2 >> 16) & 65535);
            sys_out(P_BLT_CMD, 3);
        }
        return 0;
    }
    /* --- Der Fenster-Server ----------------------------------------------
       Damit kann ein eigenstaendiges Programm ein Fenster auf dem
       Schreibtisch haben, statt den ganzen Bildschirm zu belegen. Es malt
       in seinen eigenen Puffer (Blitter-Ports 0x5B..0x5D), der
       Schreibtisch setzt die Fenster zusammen. */
    if (fn == 40) return fw_neu((char*)a1, a2, a3, p_current);
    if (fn == 41) return fw_holen(a1, a2);
    if (fn == 42) return fw_groesse(a1, a2);
    if (fn == 43) return fw_fertig(a1);
    if (fn == 44) return fw_zu(a1);
    /* Die Zwischenablage. Der Puffer liegt fest bei 0x00130000 und steht
       jedem offen -- nur seine Laenge ist eine Zahl im Kernel, und die
       muessen Programme lesen und setzen koennen, sonst wuesste niemand,
       wie viel drinsteht. */
    if (fn == 45) return clip_len;
    if (fn == 46) { clip_len = a1; return 0; }
    return 0 - 1;
}

void syscall_init() {
    mem_put(0x40 * 4, (int)syscall_asm);
}

/* Uebergibt einem Programm seine Kommandozeile (alles nach dem Programmnamen) */
void prog_setargs(char* args) {
    strncpy((char*)PROG_ARGS, args, 200);
}

/* Laedt ein Programm von der Platte und startet es.
   Rueckgabe: 0 = gelaufen, -1 = nicht gefunden */
int prog_run(char* name, int hintergrund) {
    int n;
    n = fs_read_prog(name, PROG_ADDR, PROG_MAX);
    if (n < 0) return 0 - 1;
    if (hintergrund) {
        int pid;
        if (mt_active == 0) mt_enable();
        pid = proc_start(name, PROG_ADDR);
        if (pid >= 0) p_bg[pid] = 1;
        return pid;
    }
    call_addr(PROG_ADDR);
    return 0;
}
