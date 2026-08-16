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
int prog_groesse = 0;                /* Groesse des zuletzt geladenen Programms */

/* ==========================================================================
   Absturzsicherung -- der Kernel haelt nicht mehr fuer immer an

   Bis jetzt riss ein Programmfehler (Division durch Null, ungueltiger Befehl,
   Sprung ins Leere, endlose Rekursion) die ganze Maschine mit: die CPU sprang
   in den leeren Interrupt-Vektor und blieb stehen. crash.tbx macht das vor.

   Jetzt sitzt in den Vektoren 0x00 (Division) und 0x06 (ungueltiger Befehl,
   auch verirrte Spruenge und Stack-Ueberlauf) ein Handler (fault_asm in
   start.asm). Statt ewig anzuhalten, zeigt der Kernel eine Meldung und
   startet SAUBER neu -- so wie ein echtes System bei einer Kernel-Panik
   automatisch rebootet, statt tot liegenzubleiben.

   'crashing' verhindert eine Endlosschleife, falls das Aufraeumen selbst noch
   in kaputten Kernel-Speicher laeuft (crash.tbx Menue 9 ohne Waechter): beim
   zweiten Mal startet fault_asm dann direkt neu, ohne C anzufassen. */
int crashing = 0;

int fault_asm();                     /* der Fehler-Handler in start.asm */

void nach_absturz() {
    crashing = 1;
    sys_setmode(0);                  /* zurueck in den Textmodus */
    cls(NORMAL);
    printc("\n\n  A program crashed.\n", RED);
    print("  The system caught it and is restarting ...\n");
    sleep(140);
    sys_out(0x90, 2);                /* P_POWER: 2 = Neustart */
    while (1) { }                    /* falls der Neustart haengt */
}

/* ==========================================================================
   TOOBAD DEFENDER -- der Waechter, hier die Kernel-Seite

   Der Waechter selbst sitzt NICHT im Kernel, sondern im Disk-Controller
   (hardware/devices.py). Das muss so sein: ein Programm schreibt nicht ueber
   einen Systemaufruf auf die Platte, sondern mit der CPU-Instruktion "outr"
   direkt an die Ports -- am Kernel vorbei. Ein "if" im Kernel saehe das nie.

   Die Hardware dagegen sieht bei jedem Portbefehl, WOHER er kommt (die
   Adresse der Instruktion). Kommt ein SCHREIB-Befehl aus dem Programm-Band
   (0x200000..0x300000), verweigert der Controller ihn -- der Kernel und das
   BIOS liegen ausserhalb und duerfen. Das ist "privilegiertes I/O" in klein,
   genau die Grenze, die einem echten Rechner Ring 0 von Ring 3 trennt.

   Hier stehen nur die Zugaenge dazu: die Ports schalten und auslesen, und die
   Meldung fuer den Schreibtisch. */
#define PORT_DGUARD  0x36            /* an/aus -- hoert nur auf den Kernel */
#define PORT_DALARM  0x37            /* 1 = etwas abgewehrt (Lesen quittiert) */
#define PORT_DGLBA   0x38            /* Ziel des letzten Angriffs */
#define PORT_DGCNT   0x39            /* Gesamtzahl der Abwehren */
#define PORT_DGKIND  0x3A            /* Art: 1 = Platte, 2 = Speicher */

int  wacht_alarm = 0;                /* 1 = eine Warnung wartet auf Anzeige */
int  wacht_lba   = 0;                /* worauf der letzte Angriff zielte */
int  wacht_kind  = 0;                /* 1 = Platte, 2 = Kernel-Speicher */

/* --- Autostart auf Nachfrage ---------------------------------------------
   JEDES Programm darf sich in den Autostart eintragen -- aber nicht heimlich.
   Es ruft autostart_anmelden(name), das nur einen WUNSCH hinterlegt. Der
   Schreibtisch sieht den Wunsch, zeigt das Defender-Fenster ("Programm X
   moechte bei jedem Start laufen -- erlauben?") und schreibt den Eintrag NUR,
   wenn der Benutzer zustimmt. So braucht ein braves Programm kein Passwort,
   und ein heimliches kommt trotzdem nicht durch: wer nicht fragt, schreibt
   \SYSTEM\AUTORUN.DAT gar nicht (der Passwortschutz bleibt davor). */
int  as_anfrage = 0;                 /* 1 = ein Programm moechte in den Autostart */
char as_name[20];                    /* welches */

/* Wird aus der Schreibtischschleife regelmaessig aufgerufen: hat der
   Waechter etwas abgewehrt? Dann fuer das Popup vormerken. */
void defender_poll() {
    if (sys_in(PORT_DALARM)) {
        wacht_alarm = 1;
        wacht_lba = sys_in(PORT_DGLBA);
        wacht_kind = sys_in(PORT_DGKIND);
    }
}
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

    /* Ordner wechseln -- wie CD in der Shell. Bis jetzt arbeiteten
       fileread/filewrite immer im Ordner, in dem gerade jemand stand; ein
       Programm konnte nicht gezielt ins Hauptverzeichnis schreiben. Der
       Schutz aus fs.c bleibt unberuehrt: fs_chdir liest nur, es loescht
       nichts. */
    if (fn == 50) return fs_chdir((char*)a1);
    if (fn == 51) return prog_groesse;    /* wie gross bin ich selbst? */
    /* Datei loeschen, wie DEL. Der Schutz aus fs.c gilt: eine Systemdatei
       liefert -4. Der Defender braucht das, um die Spuren eines Virus im
       Hauptverzeichnis und in \PROGS zu entfernen. */
    if (fn == 52) return fs_delete((char*)a1);
    /* TOOBAD DEFENDER: den Waechter schalten (das darf nur der Kernel -- ein
       Programm kommt nur hierdurch), seinen Zustand und seine Bilanz lesen.
       Weil DIESER Aufruf im Kernel laeuft (nicht im Programm-Band), nimmt der
       Controller das Schalten an. */
    /* Einschalten darf jeder (schadet nie). AUSschalten nur mit offener
       SUDO-Freigabe -- also nach dem Passwort. Sonst waere der Waechter
       wertlos: ein Virus riefe guard_set(0), und der Kernel schaltete ihn
       brav ab (dieser Aufruf laeuft ja im Kernel). fs_sudo ist genau die
       Unterscheidung: der Benutzer mit Passwort kommt durch, das Programm im
       Hintergrund nicht. */
    if (fn == 53) {
        if (a1) sys_out(PORT_DGUARD, 1);
        else if (fs_sudo) sys_out(PORT_DGUARD, 0);
        return 0;
    }
    if (fn == 54) return sys_in(PORT_DGCNT);
    if (fn == 55) return sys_in(PORT_DGUARD);
    /* Autostart anmelden: nur einen Wunsch hinterlegen, der Schreibtisch
       fragt. Rueckgabe hier immer 0 -- ob es klappt, entscheidet der Nutzer. */
    if (fn == 57) {
        strncpy(as_name, (char*)a1, 18);
        as_anfrage = 1;
        return 0;
    }
    return 0 - 1;
}

void syscall_init() {
    mem_put(0x40 * 4, (int)syscall_asm);
    /* Fehler-Handler einhaengen: ab jetzt haelt ein Programmabsturz nicht
       mehr die Maschine an, sondern nur das Programm. */
    mem_put(0x00 * 4, (int)fault_asm);   /* Division durch Null */
    mem_put(0x06 * 4, (int)fault_asm);   /* ungueltiger Befehl */
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
    prog_groesse = n;                 /* damit ein Programm seine eigene Groesse erfragen kann */
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
