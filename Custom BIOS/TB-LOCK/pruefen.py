#!/usr/bin/env python3
"""
Prueft TB-LOCK auf der echten emulierten Maschine.

    python3 "Custom BIOS/TB-LOCK/pruefen.py"

Gebaut wird vorher automatisch. Getestet wird die ganze Kette, so wie ein
Mensch sie durchlaeuft: Setup aufmachen, Passwort einrichten, neu starten,
ausgesperrt werden, sich mit dem richtigen Passwort hereinlassen, das
Passwort aendern und wieder loeschen.

Der Rechner laeuft dabei wirklich -- kein Bauteil ist nachgebaut. Die
Knopfzelle ist eine eigene Datei im Temp-Ordner, die zwischen den Neustarts
stehen bleibt; genau daran haengt der Test, dass ein Passwort einen Neustart
ueberlebt.
"""

import os
import shutil
import sys
import tempfile

HIER = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HIER))
sys.path.insert(0, ROOT)

os.environ["SDL_VIDEODRIVER"] = "dummy"
os.environ["SDL_AUDIODRIVER"] = "dummy"

from hardware.machine import Machine
from tools.headless import KEYNAMES, screen_text, test_platte

GRUEN, ROT, WEG = "\033[92m", "\033[91m", "\033[0m"
PASST = 0
FEHLT = 0


def pruefe(was, bedingung, bild=None):
    global PASST, FEHLT
    if bedingung:
        PASST += 1
        print(f"  [{GRUEN}  OK  {WEG}] {was}")
    else:
        FEHLT += 1
        print(f"  [{ROT} FEHLT{WEG}] {was}")
        if bild:
            for zeile in bild.split("\n"):
                if zeile.strip():
                    print(f"           | {zeile}")


class Lauf:
    """Ein laufender TB-32 mit TB-LOCK im ROM."""

    def __init__(self, rom, cmos, platte):
        self.m = Machine(ROOT, rom=rom, disk=platte, cmos=cmos)
        self.m.power_on()
        self.dt = 1 / 60
        self.gesehen = ""
        self._n = 0

    def warte(self, sekunden):
        for _ in range(int(sekunden / self.dt)):
            self.m.run_slice(self.dt)
            self._n += 1
            if self._n % 3 == 0:
                self.gesehen += "\n" + self.bild()
            if not self.m.running:
                return

    def eingabe(self, text, warte_danach=0.6):
        for teil in text.split("|"):
            if teil.upper() in KEYNAMES:
                sc, a = KEYNAMES[teil.upper()]
                self.m.keyboard.push(a, sc)
                for _ in range(3):
                    self.m.run_slice(self.dt)
            else:
                for ch in teil:
                    self.m.keyboard.push(ord(ch), 0)
                    for _ in range(3):
                        self.m.run_slice(self.dt)
        self.warte(warte_danach)

    def bild(self):
        return "\n".join(screen_text(self.m))

    def ins_setup(self, versuche=16):
        """DEL druecken, bis das Setup oder das Passworttor da ist."""
        for _ in range(versuche):
            self.eingabe("DEL", 0.15)
            b = self.bild()
            if "SETUP UTILITY" in b or "Setup is locked" in b:
                return True
        return False

    def zum_reiter_password(self):
        """Von Main aus vier Reiter nach rechts."""
        for _ in range(4):
            self.eingabe("RIGHT", 0.0)
        self.warte(0.4)


def main():
    print("\nTB-LOCK -- Setup-Passwort im BIOS\n")

    from importlib import import_module
    sys.path.insert(0, HIER)
    bauen = import_module("bauen")
    rom, daten, summe = bauen.bauen(still=True)
    print(f"  Abbild: {len(daten)} Bytes, Pruefsumme {summe:08X}\n")

    tmp = tempfile.mkdtemp(prefix="tblock_")
    chip = os.path.join(tmp, "bios.bin")
    shutil.copy(rom, chip)
    cmos = os.path.join(tmp, "cmos.bin")          # bleibt ueber Neustarts hinweg
    platte = test_platte()

    # === 1. Ohne Passwort steht das Setup offen ==========================
    print("--- Ab Werk offen ----------------------------------------------")
    L = Lauf(chip, cmos, platte)
    pruefe("Setup oeffnet sich ohne Passwort", L.ins_setup(), L.bild())
    L.zum_reiter_password()
    b = L.bild()
    pruefe("Der Reiter Password ist da", "Supervisor Password" in b, b)
    pruefe("... und meldet 'Not Installed'", "Not Installed" in b, b)
    pruefe("Die drei Zeilen stehen darunter",
           "Set / Change Password" in b and "Clear Password" in b, b)

    # === 2. Passwort einrichten ==========================================
    print("\n--- Passwort einrichten ----------------------------------------")
    L.eingabe("DOWN", 0.0)
    L.eingabe("ENTER", 0.5)
    pruefe("Der Dialog fragt nach einem neuen Passwort",
           "Enter New Password" in L.bild(), L.bild())
    L.eingabe("geheim", 0.2)
    L.eingabe("ENTER", 0.4)
    pruefe("... und danach nach der Wiederholung",
           "Confirm New Password" in L.bild(), L.bild())
    L.eingabe("geheim", 0.2)
    L.eingabe("ENTER", 0.8)
    pruefe("Das Passwort ist eingerichtet",
           "Password installed" in L.gesehen, L.bild())
    pruefe("Die Zeile meldet jetzt 'Installed'",
           "Installed" in L.bild() and "Not Installed" not in L.bild(), L.bild())

    # Zwei ungleiche Eingaben duerfen nichts aendern
    L.eingabe("ENTER", 0.5)
    L.eingabe("geheim", 0.2)
    L.eingabe("ENTER", 0.4)          # altes Passwort
    L.eingabe("neu1", 0.2)
    L.eingabe("ENTER", 0.4)
    L.eingabe("neu2", 0.2)
    L.eingabe("ENTER", 0.8)
    pruefe("Zwei ungleiche Eingaben aendern nichts",
           "do not match" in L.gesehen, L.bild())

    L.eingabe("F10", 1.0)            # Setup verlassen und sichern

    # === 3. Neustart: jetzt ist zu ======================================
    print("\n--- Nach dem Neustart ------------------------------------------")
    pruefe("Die Knopfzelle wurde geschrieben", os.path.exists(cmos))

    N = Lauf(chip, cmos, platte)
    pruefe("DEL fuehrt an ein Passworttor, nicht ins Setup",
           N.ins_setup() and "Setup is locked" in N.bild(), N.bild())
    pruefe("Das Setup ist dabei NICHT offen",
           "SETUP UTILITY" not in N.bild(), N.bild())

    N.eingabe("falsch", 0.2)
    N.eingabe("ENTER", 1.2)
    pruefe("Ein falsches Passwort wird abgewiesen",
           "Wrong password" in N.gesehen, N.bild())
    pruefe("... und laesst das Setup weiter zu",
           "SETUP UTILITY" not in N.bild(), N.bild())

    N.eingabe("geheim", 0.2)
    N.eingabe("ENTER", 1.2)
    pruefe("Das richtige Passwort oeffnet das Setup",
           "SETUP UTILITY" in N.bild(), N.bild())

    # === 4. Drei Fehlversuche, dann ist Schluss =========================
    print("\n--- Drei Fehlversuche ------------------------------------------")
    D = Lauf(chip, cmos, platte)
    D.ins_setup()
    for _ in range(3):
        D.eingabe("falsch", 0.2)
        D.eingabe("ENTER", 1.2)
    pruefe("Nach drei Fehlversuchen ist Schluss",
           "Access denied" in D.gesehen, D.bild())
    pruefe("... und das Setup bleibt zu",
           "SETUP UTILITY" not in D.bild(), D.bild())

    # === 5. Der zweite Weg ins Setup ====================================
    # Der rote Secure-Boot-Bildschirm fuehrt mit DEL ebenfalls ins Setup --
    # und genau dort liegt "Trust Current Boot Image". Waere dieser Weg
    # unbewacht, koennte man das Passwort umgehen, indem man das Startabbild
    # absichtlich kaputtmacht. Hier wird geprueft, dass das Tor auch da steht.
    print("\n--- Der Weg ueber den roten Bildschirm -------------------------")
    S = Lauf(chip, cmos, platte)
    S.ins_setup()
    S.eingabe("geheim", 0.2)
    S.eingabe("ENTER", 1.2)
    for _ in range(3):                    # Reiter Security
        S.eingabe("RIGHT", 0.0)
    S.warte(0.4)
    S.eingabe("ENTER", 0.4)               # Secure Boot einschalten
    pruefe("Secure Boot laesst sich einschalten", "Enabled" in S.bild(), S.bild())
    S.eingabe("DOWN", 0.0)
    S.eingabe("DOWN", 0.3)
    S.eingabe("ENTER", 0.6)               # Trust Current Boot Image
    S.eingabe("F10", 1.0)

    # Jetzt das Abbild aendern, damit die gemerkte Summe nicht mehr passt.
    # Gerechnet wird ueber Bootsektor + Kernel-Datei + die ersten 16 KB ROM
    # (secure_summe). Angefasst werden die Polsterbytes am Ende von Sektor 0:
    # das aendert die Summe zuverlaessig, laesst den Startcode aber heil --
    # der Bootsektor ist nur 488 der 512 Byte lang.
    kaputte_platte = os.path.join(tmp, "hd_geaendert.img")
    shutil.copy(platte, kaputte_platte)
    with open(kaputte_platte, "r+b") as f:
        f.seek(500)
        f.write(b"\xA5" * 8)

    R = Lauf(chip, cmos, kaputte_platte)
    R.warte(5.0)
    pruefe("Secure Boot schlaegt an", "SECURE BOOT" in R.gesehen, R.bild())
    R.eingabe("DEL", 1.2)
    pruefe("Auch von hier fuehrt DEL nur ans Passworttor",
           "Setup is locked" in R.bild(), R.bild())
    pruefe("... und nicht ins offene Setup",
           "SETUP UTILITY" not in R.bild(), R.bild())
    R.eingabe("geheim", 0.2)
    R.eingabe("ENTER", 1.2)
    pruefe("Mit dem Passwort geht es auch hier weiter",
           "SETUP UTILITY" in R.bild(), R.bild())

    # Secure Boot wieder aus, damit der Rest des Tests normal weiterlaeuft
    for _ in range(3):
        R.eingabe("RIGHT", 0.0)
    R.warte(0.4)
    R.eingabe("ENTER", 0.4)
    R.eingabe("F10", 1.0)

    # === 6. Aendern und loeschen ========================================
    print("\n--- Aendern und loeschen ---------------------------------------")
    A = Lauf(chip, cmos, platte)
    A.ins_setup()
    A.eingabe("geheim", 0.2)
    A.eingabe("ENTER", 1.2)
    A.zum_reiter_password()
    A.eingabe("DOWN", 0.0)
    A.eingabe("ENTER", 0.5)
    pruefe("Zum Aendern kommt zuerst das alte Passwort dran",
           "Enter Current Password" in A.bild(), A.bild())
    A.eingabe("geheim", 0.2)
    A.eingabe("ENTER", 0.5)
    A.eingabe("anders", 0.2)
    A.eingabe("ENTER", 0.4)
    A.eingabe("anders", 0.2)
    A.eingabe("ENTER", 0.8)
    pruefe("Das Passwort laesst sich aendern",
           "Password installed" in A.gesehen, A.bild())

    A.eingabe("DOWN", 0.0)
    A.eingabe("ENTER", 0.5)
    A.eingabe("geheim", 0.2)          # das ALTE, gilt nicht mehr
    A.eingabe("ENTER", 0.8)
    pruefe("Loeschen mit dem alten Passwort geht nicht",
           "Wrong password" in A.gesehen, A.bild())

    A.eingabe("ENTER", 0.5)
    A.eingabe("anders", 0.2)
    A.eingabe("ENTER", 1.0)
    pruefe("Mit dem neuen Passwort laesst es sich loeschen",
           "Password cleared" in A.gesehen, A.bild())

    Z = Lauf(chip, cmos, platte)
    pruefe("Danach steht das Setup wieder offen",
           Z.ins_setup() and "SETUP UTILITY" in Z.bild(), Z.bild())

    shutil.rmtree(tmp, ignore_errors=True)

    farbe = GRUEN if FEHLT == 0 else ROT
    print(f"\n{farbe}{PASST}/{PASST + FEHLT} Pruefungen bestanden{WEG}\n")
    return 1 if FEHLT else 0


if __name__ == "__main__":
    sys.exit(main())
