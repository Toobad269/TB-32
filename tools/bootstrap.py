#!/usr/bin/env python3
"""
Bootstrap-Nachweis: Der C-Compiler übersetzt sich selbst.

Die Kette:
    Stufe 1   CC.TBX    wurde auf dem Mac erzeugt (tools/tcc.py)
    Stufe 2   CC2.TBX = CC.TBX  übersetzt CC.C     -- auf dem TB-32
    Stufe 3   CC3.TBX = CC2.TBX übersetzt CC.C     -- auf dem TB-32

Sind Stufe 2 und Stufe 3 Byte für Byte gleich, ist der Compiler ein
Fixpunkt: er erzeugt sich selbst unverändert. Genau so weist man
Bootstrapping nach -- Stufe 1 darf davon abweichen, weil sie von einem
anderen Compiler stammt.

    python3 tools/bootstrap.py
"""

import os
import sys

os.environ["SDL_VIDEODRIVER"] = "dummy"
os.environ["SDL_AUDIODRIVER"] = "dummy"

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)

from hardware.machine import Machine
from hardware import devices as dev
from tools.headless import screen_text, test_cmos, test_platte

GRUEN, ROT, GELB, WEG = "\033[92m", "\033[91m", "\033[93m", "\033[0m"


class PC:
    """Ein laufender Rechner, der auf Bildschirmausgaben warten kann."""

    def __init__(self):
        self.m = Machine(ROOT, disk=test_platte(), cmos=test_cmos())
        self.m.power_on()
        self.dt = 1 / 60

    def bild(self):
        return "\n".join(screen_text(self.m))

    def laufen(self, sekunden):
        for _ in range(int(sekunden / self.dt)):
            self.m.run_slice(self.dt)

    def warte_auf(self, text, max_sek=600, melde=None):
        """Läuft, bis <text> auf dem Bildschirm steht."""
        schritte = int(max_sek / self.dt)
        for i in range(schritte):
            self.m.run_slice(self.dt)
            if i % 60 == 0 and text in self.bild():
                self.laufen(0.4)
                return True
            if melde and i % 1800 == 0 and i:
                print(f"      ... {int(i * self.dt)} s Rechenzeit vergangen")
        return text in self.bild()

    def tippe(self, zeile):
        """Tippt eine Zeile und drückt Enter -- Zeichen für Zeichen, mit
        genug Abstand, damit der Tastaturpuffer nicht überläuft."""
        for ch in zeile:
            self.m.keyboard.push(ord(ch), 0)
            self.laufen(0.05)
        self.m.keyboard.push(13, dev.KEY_ENTER)
        self.laufen(0.2)


def main():
    print("Bootstrap-Nachweis für den TB-32 C-Compiler\n")

    pc = PC()
    print("  Rechner startet ...")
    if not pc.warte_auf("A:\\>", 30):
        print(f"{ROT}  Das System ist nicht bis zur Eingabeaufforderung gekommen.{WEG}")
        return 1

    pc.tippe("CD SOURCE")            # dort liegt der Quelltext des Compilers
    pc.laufen(0.5)
    pc.tippe("CLS")
    pc.laufen(0.5)

    print("\n  Stufe 2:  CC.TBX übersetzt seinen eigenen Quelltext CC.C")
    pc.tippe("CC CC.C CC2.TBX")
    if not pc.warte_auf("Created CC2.TBX", 900, melde=True):
        print(f"{ROT}  Stufe 2 fehlgeschlagen:{WEG}")
        print(pc.bild())
        return 1
    for z in pc.bild().splitlines():
        if "Code:" in z or "Compiling" in z:
            print("     ", z.strip())
    print(f"{GRUEN}      Stufe 2 erzeugt.{WEG}")

    pc.tippe("CLS")
    pc.laufen(0.5)

    print("\n  Stufe 3:  CC2.TBX übersetzt denselben Quelltext noch einmal")
    pc.tippe("CC2 CC.C CC3.TBX")
    if not pc.warte_auf("Created CC3.TBX", 900, melde=True):
        print(f"{ROT}  Stufe 3 fehlgeschlagen:{WEG}")
        print(pc.bild())
        return 1
    for z in pc.bild().splitlines():
        if "Code:" in z or "Compiling" in z:
            print("     ", z.strip())
    print(f"{GRUEN}      Stufe 3 erzeugt.{WEG}")

    pc.tippe("CLS")
    pc.laufen(0.5)

    print("\n  Vergleich der beiden Stufen (auf dem Gerät selbst):")
    pc.tippe("FC CC2.TBX CC3.TBX")
    pc.warte_auf("FC:", 60)
    pc.laufen(1.0)
    ergebnis = pc.bild()
    for z in ergebnis.splitlines():
        if "Comparing" in z or "identical" in z or "different" in z:
            print("     ", z.strip())

    identisch = "identical" in ergebnis

    print("\n  Gegenprobe: läuft der selbst erzeugte Compiler?")
    pc.tippe("CLS")
    pc.laufen(0.4)
    pc.tippe("CC3 DEMO.C DX.TBX")
    pc.warte_auf("Created DX.TBX", 300)
    pc.tippe("CLS")
    pc.laufen(0.4)
    pc.tippe("DX")
    pc.warte_auf("Fibonacci", 120)
    laeuft = "0 1 1 2 3 5 8 13 21 34 55 89" in pc.bild()
    for z in pc.bild().splitlines():
        if "Fibonacci" in z:
            print("     ", z.strip())

    print()
    if identisch and laeuft:
        print(f"{GRUEN}  BOOTSTRAPPING BESTANDEN{WEG}")
        print("  Stufe 2 und Stufe 3 sind Byte für Byte gleich, und der")
        print("  selbst erzeugte Compiler übersetzt korrekte Programme.")
        print("  Der TB-32 kann seinen eigenen Compiler ohne den Mac herstellen.")
    else:
        print(f"{ROT}  Bootstrapping noch nicht erreicht.{WEG}")
        if not identisch:
            print("  Stufe 2 und Stufe 3 unterscheiden sich.")
        if not laeuft:
            print("  Der erzeugte Compiler liefert kein lauffähiges Programm.")

    pc.m.shutdown()
    return 0 if (identisch and laeuft) else 1


if __name__ == "__main__":
    sys.exit(main())
