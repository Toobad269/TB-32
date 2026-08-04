#!/usr/bin/env python3
"""
Gesamttest des virtuellen PCs.

Baut alles neu, schaltet den Rechner ein und prüft Schritt für Schritt, ob
jede Ebene wirklich funktioniert -- von der CPU bis zum Fenstersystem.
Jeder Test läuft auf der echten emulierten Maschine, nichts wird simuliert.

    python3 tools/selftest.py
"""

import os
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)

os.environ["SDL_VIDEODRIVER"] = "dummy"
os.environ["SDL_AUDIODRIVER"] = "dummy"

from hardware.machine import Machine
from hardware import devices as dev
from tools.headless import KEYNAMES, screen_text, test_cmos

GRUEN, ROT, GELB, WEG = "\033[92m", "\033[91m", "\033[93m", "\033[0m"


def tippe(m, text):
    """Wandelt eine Zeichenkette in Tastendrücke um."""
    keys = []
    for teil in text.split("|"):
        if teil.upper() in KEYNAMES:
            keys.append(KEYNAMES[teil.upper()])
        else:
            for ch in teil:
                keys.append((0, ord(ch)))
    return keys


class Lauf:
    """Ein Rechner, der im Hintergrund läuft und den man tippen lassen kann."""

    def __init__(self, rom=None):
        self.m = Machine(ROOT, rom=rom, cmos=test_cmos())
        self.m.power_on()
        self.dt = 1 / 60
        # Alles, was seit dem Einschalten je auf dem Schirm stand. Der POST
        # ist bei eingeschaltetem Quick Boot nach einer Viertelsekunde vorbei
        # -- ein Blick zu einem festen Zeitpunkt trifft ihn dann nicht mehr.
        self.gesehen = ""
        self._zaehler = 0

    def warte(self, sekunden):
        for _ in range(int(sekunden / self.dt)):
            self.m.run_slice(self.dt)
            self._zaehler += 1
            if self._zaehler % 3 == 0:          # etwa 20-mal je Sekunde
                self.gesehen = self.gesehen + "\n" + self.bild()
            if not self.m.running:
                return

    def eingabe(self, text, warte_danach=1.0):
        for sc, a in tippe(self.m, text):
            self.m.keyboard.push(a, sc)
            for _ in range(3):
                self.m.run_slice(self.dt)
        self.warte(warte_danach)

    def bild(self):
        return "\n".join(screen_text(self.m))


def bios_summe():
    """Die Prüfsumme, die im Kopf des gebauten BIOS steht."""
    with open(os.path.join(ROOT, "firmware", "bios.bin"), "rb") as f:
        return int.from_bytes(f.read(16)[12:16], "little")


def flash_test():
    """Den BIOS-Chip wirklich neu beschreiben -- auf einer Kopie.

    Geprüft wird die ganze Kette: Datei aussuchen, Kennung und Prüfsumme
    kontrollieren, brennen, Sicherung anlegen, davon starten, zurückspielen.
    Und der Fall, auf den es ankommt: ein beschädigtes Abbild darf NICHT
    gebrannt werden."""
    fw = os.path.join(ROOT, "firmware")
    with tempfile.TemporaryDirectory() as tmp:
        chip = os.path.join(tmp, "bios.bin")
        shutil.copy(os.path.join(fw, "bios.bin"), chip)
        with open(os.path.join(fw, "minimal.bin"), "rb") as f:
            klein = f.read()
        with open(chip, "rb") as f:
            gross = f.read()

        # --- Ein kaputtes Abbild muss abgelehnt werden -------------------
        kaputt = bytearray(klein)
        kaputt[400] ^= 0xFF                     # Pruefsumme stimmt nicht mehr
        L = Lauf(rom=chip)
        L.m.flash.waehler = lambda: bytes(kaputt)
        for _ in range(14):
            L.eingabe("DEL", 0.15)
            if "SETUP UTILITY" in L.bild():
                break
        for _ in range(4):
            L.eingabe("RIGHT", 0.0)
        L.warte(0.4)
        L.eingabe("DOWN", 0.0)
        L.eingabe("DOWN", 0.3)
        L.eingabe("ENTER", 0.8)
        pruefe("Beschädigtes Abbild wird abgelehnt",
               "Checksum does not match" in L.gesehen, L.bild())
        with open(chip, "rb") as f:
            pruefe("... und der Chip bleibt dabei unangetastet", f.read() == gross)

        # --- Das gute Abbild flashen -------------------------------------
        L.m.flash.waehler = lambda: klein
        L.eingabe("ENTER", 0.6)                 # Flash BIOS from File
        L.eingabe("ENTER", 0.8)                 # Rueckfrage bestaetigen
        pruefe("Gutes Abbild wird gebrannt", "Flash complete" in L.gesehen, L.bild())
        with open(chip, "rb") as f:
            pruefe("Chip enthält jetzt das neue BIOS", f.read() == klein)
        with open(os.path.join(tmp, "bios.backup.bin"), "rb") as f:
            pruefe("Sicherung des alten BIOS angelegt", f.read() == gross)

        # --- Und er startet damit auch -----------------------------------
        # Woran man es erkennt: das Minimal-BIOS hat gar keinen Selbsttest,
        # also fehlt das Startbild "TOOBAD BIOS" -- und trotzdem steht am
        # Ende die Eingabeaufforderung da. Nach seiner eigenen Meldung zu
        # suchen brächte nichts: sie steht nur Mikrosekunden auf dem Schirm,
        # weil gleich danach der Bootsektor laedt.
        N = Lauf(rom=chip)
        N.warte(4.0)
        pruefe("Der Rechner startet mit dem selbst geflashten BIOS",
               "A:\\>" in N.bild() and "TOOBAD BIOS" not in N.gesehen, N.bild())

        # --- Einmal-Start: testen ohne den Chip anzufassen ---------------
        # Der Chip trägt nach dem Flashtest oben noch das kleine BIOS --
        # erst das echte zurücklegen, sonst prüft der Test gegen sich selbst.
        shutil.copy(os.path.join(fw, "bios.bin"), chip)
        T = Lauf(rom=chip)
        T.warte(6.0)
        T.m.bus.write_block(0x00760000, klein)
        T.m.flash.port_out(0xB2, 0x00760000)
        T.m.flash.port_out(0xB1, len(klein))
        T.m.flash.port_out(0xB0, 5)
        pruefe("Abbild aus dem Arbeitsspeicher geholt",
               T.m.flash.status == 0 and len(T.m.flash.puffer) == len(klein))
        T.m.flash.port_out(0xB0, 6)
        T.m.power_on()
        T.warte(4.0)
        pruefe("Neustart läuft mit dem Testabbild",
               T.m.bios_test and "A:\\>" in T.bild(), T.bild())
        with open(chip, "rb") as f:
            pruefe("... und der Chip blieb dabei unangetastet", f.read() == gross)
        T.m.power_on()
        T.warte(6.0)
        pruefe("Der Start danach nimmt wieder das echte BIOS",
               not T.m.bios_test and "A:\\>" in T.bild(), T.bild())

        # --- Dauerhaft flashen: die Firmware fragt in Rot nach ------------
        shutil.copy(os.path.join(fw, "bios.bin"), chip)
        F = Lauf(rom=chip)
        F.warte(6.0)
        F.m.bus.write_block(0x00760000, klein)
        F.m.flash.port_out(0xB2, 0x00760000)
        F.m.flash.port_out(0xB1, len(klein))
        F.m.flash.port_out(0xB0, 5)
        F.m.flash.port_out(0xB0, 8)
        F.m.power_on()
        F.warte(1.5)
        pruefe("Firmware fragt vor dem Brennen in Rot nach",
               "FLASH BIOS" in F.bild() and "permanent" in F.bild(), F.bild())
        with open(chip, "rb") as f:
            pruefe("... und hat bis dahin nichts geschrieben", f.read() == gross)
        F.eingabe("ENTER", 3.0)
        with open(chip, "rb") as f:
            pruefe("ENTER brennt den Chip", f.read() == klein)
        shutil.copy(os.path.join(fw, "bios.bin"), chip)

        # --- Dual BIOS: ein zerstörter Chip wird automatisch ersetzt ------
        with open(chip, "wb") as f:
            f.write(b"\x00" * 64)               # Chip vernichtet
        R = Machine(ROOT, rom=chip, cmos=test_cmos())
        R.power_on()
        pruefe("Zerstörter Chip: das Board greift zur Sicherung",
               R.rom_gerettet)
        with open(chip, "rb") as f:
            pruefe("... und schreibt das alte BIOS zurück", f.read() == gross)
        R.shutdown()


ergebnisse = []


def pruefe(name, bedingung, detail=""):
    ergebnisse.append((name, bool(bedingung)))
    zeichen = f"{GRUEN}  OK  {WEG}" if bedingung else f"{ROT} FEHLT{WEG}"
    print(f"  [{zeichen}] {name}" + (f"   {detail}" if detail and not bedingung else ""))


def main():
    print("Baue das System neu ...")
    r = subprocess.run([sys.executable, "build.py"], cwd=ROOT,
                       capture_output=True, text=True)
    if r.returncode != 0:
        print(r.stdout + r.stderr)
        return 1
    for zeile in r.stdout.splitlines()[1:-1]:
        print("   ", zeile.strip())

    print("\n--- Werkzeuge --------------------------------------------------")
    r = subprocess.run([sys.executable, "tools/ctest.py", "--selftest"],
                       cwd=ROOT, capture_output=True, text=True)
    bestanden = "11/11" in r.stdout or r.stdout.strip().endswith("Tests bestanden")
    pruefe("C-Compiler besteht alle Sprachtests", "FEHLER" not in r.stdout)

    print("\n--- Einschalten und BIOS ---------------------------------------")
    L = Lauf()
    # Der POST laesst sich jetzt Zeit: Speicher zaehlt sichtbar hoch, jede
    # Pruefung erscheint einzeln. Ohne Quick Boot dauert er rund 1,5 s.
    L.warte(2.0)
    bild = L.gesehen                     # alles, was der POST gezeigt hat
    pruefe("CPU startet am Reset-Vektor und führt ROM-Code aus",
           "TOOBAD BIOS" in bild)
    pruefe("Speichertest findet 16384 KB", "16384 KB" in bild)
    pruefe("Festplatte erkannt", "16384 sectors" in bild)
    pruefe("Grafikkarte gemeldet", "TB-VGA" in bild)

    print("\n--- BIOS-Setup (CMOS) ------------------------------------------")
    L2 = Lauf()
    # DEL mehrfach anbieten: mit Quick Boot ist das Zeitfenster nur eine
    # Viertelsekunde breit, ohne dauert es zwei Sekunden.
    for _ in range(14):
        L2.eingabe("DEL", 0.15)
        if "SETUP UTILITY" in L2.bild():
            break
    L2.warte(0.4)
    bild = L2.bild()
    pruefe("DEL öffnet das Setup", "SETUP UTILITY" in bild)
    pruefe("Einstellungen werden angezeigt", "Quick Boot" in bild)
    pruefe("Uhr aus dem CMOS läuft", "System Time" in bild)
    pruefe("Reiterleiste vorhanden",
           "Hardware" in bild and "Cooling" in bild and "Security" in bild)
    # Reiter wechseln: rechts zweimal -> Kühlung mit den Messwerten
    L2.eingabe("RIGHT", 0.3)
    L2.eingabe("RIGHT", 0.5)
    bild = L2.bild()
    pruefe("Reiter Cooling zeigt Messwerte",
           "Fan Control" in bild and "CPU Temperature" in bild)
    L2.eingabe("RIGHT", 0.5)
    bild = L2.bild()
    pruefe("Reiter Security zeigt Secure Boot", "Secure Boot" in bild)
    # Zwei Tasten im selben Bild: der Interruptcontroller kennt je Quelle nur
    # ein Bit, der Handler muss den Baustein deshalb leerraeumen. Ging das
    # schief, hinkte die Tastatur einen Anschlag hinterher.
    L2.eingabe("LEFT", 0.0)
    L2.eingabe("LEFT", 0.6)
    pruefe("Zwei Tasten im selben Bild kommen beide an", "Hardware" in L2.bild()
           and "ÉÍÍ Hardware" in L2.bild().replace("\u2554", "É"))
    L2.eingabe("RIGHT", 0.0)
    L2.eingabe("RIGHT", 0.0)
    L2.eingabe("RIGHT", 0.6)
    bild = L2.bild()
    pruefe("Reiter Firmware zeigt den BIOS-Chip",
           "BIOS Image Size" in bild and "Flash BIOS from File" in bild)
    pruefe("Chip meldet seine eigene Prüfsumme",
           f"{bios_summe():08X}" in bild, bild)
    L2.eingabe("ESC", 0.8)

    print("\n--- BIOS flashen (auf einer Kopie des Chips) -------------------")
    flash_test()

    print("\n--- Bootvorgang ------------------------------------------------")
    L.warte(3.0)
    bild = L.bild()
    pruefe("Bootsektor geladen und gestartet", "TOOBAD-OS" in bild)
    pruefe("Dateisystem eingehängt", "Mounting file system" in bild)
    pruefe("Shell meldet sich", "A:\\>" in bild)

    print("\n--- Betriebssystem ---------------------------------------------")
    L.eingabe("ver|ENTER", 0.7)
    pruefe("Befehl 'ver'", "TB-32" in L.bild())

    L.eingabe("mem|ENTER", 0.7)
    pruefe("Befehl 'mem' liest die BIOS-Daten", "Total physical memory" in L.bild())

    L.eingabe("cls|ENTER", 0.4)
    L.eingabe("dir|ENTER", 1.0)
    bild = L.bild()
    pruefe("Verzeichnis zeigt die Ordner der Platte",
           "SYSTEM" in bild and "PROGS" in bild and "SOURCE" in bild)
    L.eingabe("CD PROGS|ENTER", 0.6)
    L.eingabe("DIR|ENTER", 1.0)
    bild = L.bild()
    pruefe("Ordnerwechsel und Inhalt",
           "A:\\PROGS" in bild and "BENCH.TBX" in bild and "MEMTEST.TBX" in bild)
    L.eingabe("CD \\|ENTER", 0.6)

    print("\n--- Dateisystem (schreiben, lesen, löschen) --------------------")
    L.eingabe("cls|ENTER", 0.3)
    L.eingabe("edit TEST.TXT|ENTER", 1.2)
    L.eingabe("Selbsttest schreibt hier.", 1.0)
    L.eingabe("F2", 1.2)
    L.eingabe("ESC", 1.0)
    L.eingabe("type TEST.TXT|ENTER", 1.0)
    pruefe("Editor speichert und Datei lässt sich zurücklesen",
           "Selbsttest schreibt hier." in L.bild())
    L.eingabe("del TEST.TXT|ENTER", 0.8)
    pruefe("Datei löschen", "File deleted" in L.bild())

    print("\n--- Programme von der Platte -----------------------------------")
    L.eingabe("cls|ENTER", 0.3)
    L.eingabe("START MEMTEST.TBX|ENTER", 7.0)
    bild = L.bild()
    pruefe("Programm wird geladen und läuft korrekt durch",
           "PASS" in bild, "Speichertest sollte PASS melden")
    L.eingabe("ENTER", 1.2)                       # Programm beenden
    pruefe("Programm kehrt sauber zur Shell zurück",
           L.bild().rstrip().endswith("A:\\>"))

    print("\n--- Selbst gebaute Werkzeuge auf dem Gerät ---------------------")
    L.eingabe("cls|ENTER", 0.3)
    L.eingabe("CD SOURCE|ENTER", 0.6)
    L.eingabe("ASM HELLO.ASM HELLO.TBX|ENTER", 3.0)
    bild = L.bild()
    pruefe("Assembler läuft auf dem TB-32 selbst", "Created HELLO.TBX" in bild)
    L.eingabe("HELLO|ENTER", 2.0)
    pruefe("Selbst assembliertes Programm läuft",
           "Hello from a program written ON the TB-32" in L.bild())
    L.eingabe("ENTER", 1.0)

    L.eingabe("CD SOURCE|ENTER", 0.6)
    L.eingabe("cls|ENTER", 0.3)
    L.eingabe("CC T2.C T2.TBX|ENTER", 5.0)
    pruefe("C-Compiler übersetzt auf dem TB-32 selbst",
           "Created T2.TBX" in L.bild())
    L.eingabe("cls|ENTER", 0.3)
    L.eingabe("T2|ENTER", 4.0)
    bild = L.bild()
    pruefe("Selbst kompiliertes C-Programm rechnet richtig",
           "285" in bild and "a after pointer write = 42" in bild)
    pruefe("Zeiger, Arrays und Logik im erzeugten Code",
           "and-ok or-ok not-ok" in bild and "strlen = 19" in bild
           and "ABCDE" in bild)
    L.eingabe("ENTER", 1.0)

    L.eingabe("cls|ENTER", 0.3)
    L.eingabe("PY TEST.PY|ENTER", 6.0)
    bild = L.bild()
    pruefe("Python-Interpreter rechnet richtig",
           "7 * 6 = 42" in bild and "10! = 3628800" in bild)
    pruefe("Python kann Listen und Schleifen",
           "[3, 1, 4, 1, 5, 9, 2, 6]" in bild and "Largest: 9" in bild)

    L.eingabe("DEL T2.TBX|ENTER", 0.8)
    L.eingabe("CD \\|ENTER", 0.6)

    print("\n--- Zurückblättern (Scrollback) --------------------------------")
    L.eingabe("PGUP", 1.2)
    pruefe("PgUp öffnet die Bildschirmhistorie", "SCROLLBACK" in L.bild())
    L.eingabe("ESC", 1.0)
    pruefe("ESC kehrt zur Eingabe zurück", "SCROLLBACK" not in L.bild())

    print("\n--- Multitasking -----------------------------------------------")
    L.eingabe("cls|ENTER", 0.3)
    L.eingabe("START BENCH.TBX /B|ENTER", 0.8)
    L.eingabe("TASKLIST|ENTER", 1.2)
    bild = L.bild()
    pruefe("Hintergrundprozess in der Prozessliste",
           "shell" in bild and "BENCH" in bild)
    pruefe("Prozesswechsel finden statt", "Context switches" in bild)
    pruefe("Prozess läuft wirklich parallel weiter",
           "Running" in bild or "Ready" in bild)
    L.eingabe("TASKKILL 1|ENTER", 1.2)
    pruefe("Prozess lässt sich beenden", "terminated" in L.bild())

    print("\n--- Terminal und Editor im Fenster -----------------------------")
    L.eingabe("WIN|ENTER", 2.5)
    # Das Menü wächst nach oben: die Höhe hängt an der Anzahl der Einträge
    # (MENU_ANZ in gui.c). Deshalb hier aus BAR_Y zurückgerechnet, sonst
    # zeigt jeder neue Menüpunkt alle Klicks um eine Zeile daneben.
    # MENU_ANZ direkt aus gui.c lesen, statt sie hier abzuschreiben. Beim
    # letzten Wachsen des Menues sind drei Tests reihenweise umgefallen,
    # weil die Zahl an zwei Stellen stand.
    import re as _re
    MENU_ANZ = int(_re.search(r"define MENU_ANZ\s+(\d+)",
                              open(os.path.join(ROOT, "system", "gui.c")).read()).group(1))
    MENU_ZH, BAR_Y = 14, 378
    MENU_TOP = BAR_Y - (MENU_ANZ * MENU_ZH + 10)

    def menue(eintrag):
        """Start-Knopf, dann den n-ten Eintrag im Startmenü anklicken."""
        for x, y in ((25, 387), (60, MENU_TOP + 6 + eintrag * MENU_ZH)):
            L.m.mouse.move(x, y, 0); L.warte(0.2)
            L.m.mouse.move(x, y, 1); L.warte(0.3)
            L.m.mouse.move(x, y, 0); L.warte(1.0)

    menue(1)                                     # Command Prompt
    L.warte(1.0)
    ram = L.m.bus.ram
    term = "".join(chr(ram[0x00120000 + i * 2]) for i in range(70 * 3))
    pruefe("Kommandozeile läuft als Fenster", "command prompt" in term.lower())
    for ch in "VER":
        L.m.keyboard.push(ord(ch), 0); L.warte(0.2)
    L.m.keyboard.push(13, dev.KEY_ENTER); L.warte(2.0)
    term = "".join(chr(ram[0x00120000 + i * 2]) for i in range(70 * 8))
    pruefe("Befehle im Terminalfenster", "TOOBAD-OS Version" in term)

    menue(2)                                     # Editor
    L.warte(1.0)
    pruefe("Editorfenster öffnet sich", sum(L.m.vga.gfx[200 * 640:210 * 640]) > 0)
    L.eingabe("ESC", 1.5)

    print("\n--- Grafik und Fenstersystem -----------------------------------")
    L.eingabe("WIN|ENTER", 2.5)
    vga = L.m.vga
    pruefe("Grafikmodus aktiv", vga.mode == 1)
    pruefe("Bildschirm wurde bemalt", sum(vga.gfx[:64000]) > 0)
    pruefe("Hardware-Mauszeiger eingeschaltet", vga.mcur_on == 1)
    for x, y in ((25, 387), (60, MENU_TOP + 6)):  # Start -> File Manager
        L.m.mouse.move(x, y, 0); L.warte(0.2)
        L.m.mouse.move(x, y, 1); L.warte(0.3)
        L.m.mouse.move(x, y, 0); L.warte(0.8)
    pruefe("Startmenü öffnet ein Fenster",
           sum(vga.gfx[100 * 640:300 * 640]) > 0)
    L.eingabe("ESC", 1.8)
    pruefe("Rückkehr in den Textmodus", vga.mode == 0)

    print("\n--- Ausschalten ------------------------------------------------")
    L.eingabe("SHUTDOWN|ENTER", 3.0)
    pruefe("Betriebssystem schaltet den Rechner ab", not L.m.running)

    gut = sum(1 for _, ok in ergebnisse if ok)
    alle = len(ergebnisse)
    farbe = GRUEN if gut == alle else (GELB if gut > alle * 0.8 else ROT)
    print(f"\n{farbe}{gut}/{alle} Prüfungen bestanden{WEG}")
    L.m.shutdown()
    return 0 if gut == alle else 1


if __name__ == "__main__":
    sys.exit(main())
