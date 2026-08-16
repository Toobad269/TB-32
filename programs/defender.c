/* ==========================================================================
   TOOBAD DEFENDER  --  der Virenwaechter fuer TOOBAD-OS

   Gegenstueck zu \SOURCE\TAKT.C. Drei Dinge kann er:

     SCAN   Die Platte nach den Spuren des Virus absuchen: den Autostart-
            Eintrag AUTORUN.DAT, den Zaehler STAGE.DAT und das eingenistete
            \PROGS\TAKT.TBX. Zeigt an, ob und wie tief der Rechner infiziert
            ist.

     CLEAN  Die Spuren loeschen und den zerfressenen Bildschirm heilen: die
            Farbtabelle wird auf die normalen PC-Farben zurueckgesetzt.

     PROTECT  Den WAECHTER einschalten -- den eigentlichen Schutz. Er sitzt
            im Kernel (syscall.c) und faengt rohe Schreibbefehle an die
            Platte ab, BEVOR sie ankommen. Ein Programm, das den Bootsektor
            zerstoeren will, prallt dann ab und man bekommt einen Warnkasten.
            Die Flagge DEFENDER.ON sorgt dafuer, dass der Waechter schon beim
            naechsten Start wieder aktiv ist -- vor jedem Autostart.

   ==========================================================================
   EHRLICH BLEIBEN

   SCAN und CLEAN sind Aufraeumen -- sie helfen NACHHER. Der einzige echte
   Schutz ist PROTECT, weil er den Angriff im Kernel abfaengt, wo ein Programm
   nicht mehr hinkommt. Aber auch er hat eine Grenze: Virus und Waechter haben
   auf diesem Rechner DIESELBEN Rechte. Wer TAKT.TBX von Hand startet, kann
   AUTORUN.DAT neu schreiben -- der Waechter (Disk-Schutz) haelt trotzdem, weil
   er im Kernel sitzt und nicht in einer Datei. Was fehlt, ist die Trennung
   von Benutzer und Kernel (Ringe/MMU); erst die macht einen Waechter
   unangreifbar. Auf einem echten Rechner ist genau das der Unterschied.

   Uebersetzen:  im Coder oeffnen und "Build", oder  CC DEFENDER.C DEF.TBX
   ========================================================================== */

#include "proglib.c"

#define P_PAL_IDX   0x42
#define P_PAL_VAL   0x43

char nbuf[24];                       /* Name aus AUTORUN.DAT */
int  ibuf[1];                        /* vier Byte, z.B. der Zaehler */
char pruef[8];                       /* Existenz-Probe */

int has_autorun; int has_stage; int stage; int has_takt;

/* Zustand des Waechters, wie ihn dieses Programm kennt, plus die Statuszeile.
   Vor allem, was sie benutzt -- der Compiler liest von oben nach unten. */
char meldung[40];
char* wacht_text()  { if (guard_state()) return "ON "; return "OFF"; }
int   guard_farbe() { if (guard_state()) return GREEN; return RED; }

/* --- Die normale Farbtabelle (wie crash.c sie herstellt) ----------------- */
void pal_set(int nr, int r, int g, int b) {
    portout(P_PAL_IDX, nr);
    portout(P_PAL_VAL, (r << 16) | (g << 8) | b);
}
int pal16(int i) {
    if (i== 0) return 0x000000; if (i== 1) return 0x0000AA;
    if (i== 2) return 0x00AA00; if (i== 3) return 0x00AAAA;
    if (i== 4) return 0xAA0000; if (i== 5) return 0xAA00AA;
    if (i== 6) return 0xAA5500; if (i== 7) return 0xAAAAAA;
    if (i== 8) return 0x555555; if (i== 9) return 0x5555FF;
    if (i==10) return 0x55FF55; if (i==11) return 0x55FFFF;
    if (i==12) return 0xFF5555; if (i==13) return 0xFF55FF;
    if (i==14) return 0xFFFF55; return 0xFFFFFF;
}
void pal_heilen() {
    int i; int r; int g; int b; int n; int v;
    for (i = 0; i < 16; i++) { portout(P_PAL_IDX, i); portout(P_PAL_VAL, pal16(i)); }
    n = 16;
    for (r = 0; r < 6; r++) for (g = 0; g < 6; g++) for (b = 0; b < 6; b++) {
        pal_set(n, r * 51, g * 51, b * 51); n++;
    }
    while (n < 256) { v = (n - 232) * 10 + 8; if (v > 255) v = 255; pal_set(n, v, v, v); n++; }
}

/* --- Suchen --------------------------------------------------------------
   Der Autostart-Eintrag liegt geschuetzt in \SYSTEM, der Zaehler des Virus in
   \ und das eingenistete Programm in \PROGS. */
void scan() {
    int n;
    has_autorun = 0; has_stage = 0; stage = 0; has_takt = 0;

    chdir("\\"); chdir("SYSTEM");
    n = fileread("AUTORUN.DAT", (int)nbuf, 20);
    if (n > 0) { nbuf[n] = 0; has_autorun = 1; }

    chdir("\\");
    if (fileread("STAGE.DAT", (int)ibuf, 4) >= 4) { has_stage = 1; stage = ibuf[0]; }

    chdir("PROGS");
    if (fileread("TAKT.TBX", (int)pruef, 4) >= 0) has_takt = 1;
    chdir("\\");
}

/* --- Aufraeumen und heilen -----------------------------------------------
   Rueckgabe -4, wenn eine geschuetzte Datei ohne SUDO nicht weggeht. */
int clean() {
    int r;
    chdir("\\"); chdir("SYSTEM");
    r = filedelete("AUTORUN.DAT");        /* geschuetzt: braucht SUDO */
    chdir("\\");
    filedelete("STAGE.DAT");
    chdir("PROGS");
    filedelete("TAKT.TBX");
    chdir("\\");
    pal_heilen();                    /* den zerfressenen Bildschirm zuruecksetzen */
    return r;
}

/* --- Waechter dauerhaft an / aus -----------------------------------------
   Die Flagge liegt in \SYSTEM. An- und Ausschalten ist damit eine Admin-
   Aktion: ohne SUDO liefert der Schreib-/Loeschversuch -4. Der laufende
   Wachdienst wird trotzdem sofort scharf gemacht (guard_set) -- nur das
   DAUERHAFTE ueber Neustarts braucht die Flagge. */
int protect_on() {
    guard_set(1);
    chdir("\\"); chdir("SYSTEM");
    ibuf[0] = 1;
    return filewrite("DEFENDER.ON", (int)ibuf, 1);
}
int protect_off() {
    /* Beides braucht SUDO: der Kernel schaltet den laufenden Waechter nur mit
       offener Freigabe aus (guard_set(0) wirkt sonst nicht), und die Flagge in
       \SYSTEM ist geschuetzt. Ein Virus kommt so nicht an den Schutz. */
    guard_set(0);                    /* sofort aus -- aber nur mit SUDO */
    chdir("\\"); chdir("SYSTEM");
    return filedelete("DEFENDER.ON");
}

/* --- Anzeige ------------------------------------------------------------- */
void zeile(int y, char* s, int a) { setcursor(2, y); printc(s, a); }

void malen() {
    int infiziert;
    cls();
    zeile(1, "===============  TOOBAD DEFENDER 1.0  ===============", CYAN);

    setcursor(2, 3);
    print("Guard:  ");
    printc(wacht_text(), guard_farbe());
    setcursor(30, 3);
    print("Attacks blocked: ");
    printn(guard_blocks());

    infiziert = has_autorun || has_stage || has_takt;
    zeile(5, "Scan:", BRIGHT);
    if (infiziert == 0) {
        zeile(6, "  [ok] No threats found.", GREEN);
    } else {
        if (has_autorun) {
            setcursor(2, 6); printc("  [!] AUTORUN.DAT -> ", RED); print(nbuf);
            print("   (autostart hijack)");
        }
        if (has_stage) {
            setcursor(2, 7); printc("  [!] STAGE.DAT   stage ", RED); printn(stage);
            print("        (virus counter)");
        }
        if (has_takt) {
            zeile(8, "  [!] \\PROGS\\TAKT.TBX               (known malware)", RED);
        }
    }

    zeile(11, "[P] Protect (guard on, every boot)", NORMAL);
    zeile(12, "[C] Clean & heal", NORMAL);
    zeile(13, "[D] Disable guard", NORMAL);
    zeile(14, "[R] Re-scan          [ESC] Quit", NORMAL);
    setcursor(2, 16); printc(meldung, GREEN);
}

int main() {
    int k; int c; int r;
    strcpy(meldung, "");

    scan();
    malen();
    while (1) {
        if (haskey() == 0) { sleep(2); continue; }
        k = getkey();
        c = keychar(k);
        if (keycode(k) == K_ESC) { cls(); return 0; }
        if (c == 'p' || c == 'P') {
            r = protect_on();
            if (r == 0 - 4) strcpy(meldung, "Guard on now. To keep it: run  SUDO DEFENDER");
            else strcpy(meldung, "Guard active. It survives restarts now.");
        }
        if (c == 'c' || c == 'C') {
            r = clean(); scan();
            if (r == 0 - 4)
                strcpy(meldung, "Healed. Autostart needs  SUDO DEFENDER  to remove.");
            else strcpy(meldung, "Cleaned. Screen colours restored.");
        }
        if (c == 'd' || c == 'D') {
            r = protect_off();
            if (r == 0 - 4) strcpy(meldung, "Needs the password: run  SUDO DEFENDER");
            else strcpy(meldung, "Guard off, now and after restart.");
        }
        if (c == 'r' || c == 'R') { scan(); strcpy(meldung, "Re-scanned."); }
        malen();
    }
}
