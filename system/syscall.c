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

int pw_geprueft = 0;             /* Passwort in diesem Lauf genannt? */
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
    /* --- Ein Programm startet ein Programm -------------------------------
       Der Coder braucht das: er ruft den Compiler. Frueher stand er im
       Kernel und konnte prog_run einfach aufrufen; als eigenstaendiges
       Programm braucht er einen Weg dorthin. */
    if (fn == 47) { prog_setargs((char*)a2); return prog_run((char*)a1, 1); }
    if (fn == 48) return a1 >= 0 && a1 < MAXPROC ? p_state[a1] : 0;
    if (fn == 49) return build_progress;
    if (fn == 50) return (int)build_status;
    /* Die mitgeschriebenen Meldungen des Compilers. */
    if (fn == 51) { cap_start(); return 0; }
    if (fn == 52) { cap_aktiv = 0; return 0; }
    if (fn == 53) return cap_voll;
    if (fn == 54) return cap_adr(a1);
    /* Der n-te sichtbare Eintrag im aktuellen Ordner -- Name, Art, Groesse.
       Rueckgabe: wie viele es insgesamt sind. Damit kann ein Programm einen
       Dateibrowser bauen, ohne die Innereien des Dateisystems zu kennen.
       Ordner zuerst, dann Dateien -- dieselbe Reihenfolge wie im
       Dateifenster. */
    if (fn == 55) {
        int i; int k; int n; int d; char* nm;
        n = 0;
        for (d = 0; d < 2; d++) {
            for (i = 0; i < FS_MAXFILES; i++) {
                if (ent_type(i) == 0 || ent_versteckt(i)) continue;
                if (ent_parent(i) != cwd) continue;
                if (d == 0 && ent_type(i) != FT_DIR) continue;
                if (d == 1 && ent_type(i) == FT_DIR) continue;
                if (n == a1 && a2) {
                    nm = ent_name(i);
                    for (k = 0; k < 16; k++) byte_put(a2 + k, nm[k]);
                    mem_put(a2 + 16, ent_type(i));
                    mem_put(a2 + 20, ent_size(i));
                }
                n++;
            }
        }
        return n;
    }
    /* Ordner wechseln und den Pfad erfragen -- dasselbe, was CD tut. */
    if (fn == 56) return fs_chdir((char*)a1);
    if (fn == 57) { fs_path((char*)a1); return 0; }
    /* Die BIOS-Sachen bleiben im Kernel. Das Brennen eines BIOS ist die
       einzige Stelle, an der man den Rechner unbrauchbar machen kann --
       die Sicherungen dafuer (Pruefsumme, rote Rueckfrage, Einmal-Test)
       gehoeren dorthin, wo sie niemand umgehen kann. Der Coder bittet
       darum, statt es selbst zu tun. */
    if (fn == 58) { bh_top = 0; starte(APP_BIOSHILFE, "Writing a BIOS", 460, 300); return 0; }
    if (fn == 59) { bios_bauen(a1, (char*)a2); return 0; }
    /* Ein Programm in der Kommandozeile starten -- der Coder benutzt das
       fuer "Run": das Ergebnis soll seine Ausgaben irgendwo hinschreiben
       koennen, und dafuer ist das Terminalfenster da. */
    if (fn == 60) { gui_im_fenster((char*)a1); return 0; }
    /* --- Das Netz fuer Programme -----------------------------------------
       Der Browser ist ausgezogen und braucht trotzdem DNS und TCP. Die
       Protokolle bleiben im Kernel -- ein Programm bekommt eine
       Steckdose, keine zweite Fassung des Stapels. */
    if (fn == 61) return dns_aufloesen((char*)a1);
    if (fn == 62) return tcp_verbinden(a1, a2);
    if (fn == 63) return tcp_schreiben(a1, a2);
    if (fn == 64) return tcp_lesen(a1, a2, a3);
    if (fn == 65) { tcp_schliessen(); return 0; }
    if (fn == 66) return ip_lesen((char*)a1);
    if (fn == 67) { ip_text(a1, (char*)a2); return 0; }
    if (fn == 68) return net_da() ? ip_meine : 0;
    if (fn == 69) return a1 == 0 ? br_proxy : br_proxy_port;
    /* --- Auskunft ueber das System ---------------------------------------
       Damit der System Monitor als eigenes Programm laufen kann. Er liest
       nur -- veraendern kann er nichts. */
    if (fn == 70) return a1 < MAXPROC ? p_state[a1] : 0;
    if (fn == 71) return a1 < MAXPROC ? (int)proc_name(a1) : 0;
    if (fn == 72) return a1 < MAXPROC ? p_ticks[a1] : 0;
    if (fn == 73) return p_switches;
    if (fn == 74) return fs_used_sectors();
    if (fn == 75) return sys_disksize();
    /* --- Konto und Zuruecksetzen -----------------------------------------
       Fuer das Settings-Programm. Das Passwort pruefen darf jeder; aendern
       und zuruecksetzen erst, wenn er es in diesem Lauf einmal richtig
       genannt hat. Das ist keine echte Sperre -- auf dem TB-32 gibt es
       keinen Speicherschutz -- aber es verhindert das Versehen. */
    if (fn == 76) {
        if (benutzer_passt((char*)a1)) { pw_geprueft = 1; return 1; }
        return 0;
    }
    if (fn == 77) return (int)benutzer_name();
    if (fn == 78) {
        if (pw_geprueft == 0 && konto_offen() == 0) return 0 - 1;
        benutzer_anlegen(benutzer_name(), (char*)a1);
        return 0;
    }
    if (fn == 79) {
        /* Ein Rechner ohne Passwort ist offen -- dann darf auch das
           Zuruecksetzen ohne Passwort gehen. Sonst kaeme man auf einer
           frisch gebauten Maschine nie daran, weil es nichts zu pruefen
           gibt. Genau das war kaputt. */
        if (pw_geprueft == 0 && konto_offen() == 0) return 0 - 1;
        return st_zuruecksetzen();
    }
    if (fn == 80) return konto_offen();
    /* --- Dateien: loeschen, umbenennen, Ordner anlegen ------------------- */
    if (fn == 81) return fs_delete((char*)a1);
    if (fn == 82) return fs_rename((char*)a1, (char*)a2);
    if (fn == 83) return fs_mkdir((char*)a1);
    /* --- Die Kommandozeile: die Schale bleibt im Kernel, das FENSTER wird
       ein Programm. Es malt den Puffer und reicht Tasten hinein. */
    if (fn == 84) {
        if (term_lauf == 0) {
            if (mt_active == 0) mt_enable();
            term_pid = proc_start("cmd", (int)term_main);
            if (term_pid >= 0) { term_lauf = 1; term_aktiv = 1; }
        }
        return term_lauf;
    }
    if (fn == 85) { term_push_key(a1); return 0; }
    if (fn == 86) { int d; d = term_dirty; term_dirty = 0; return d; }
    /* Wo steht die Schreibmarke der Schale, und welche Zeile ist wo? */
    if (fn == 88) return a1 == 0 ? term_x : term_y;
    if (fn == 89) return term_sicht(a1, a2);
    if (fn == 87) {
        if (term_pid >= 0) p_state[term_pid] = PS_FREI;
        term_lauf = 0;
        term_aktiv = 0;
        return 0;
    }
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
