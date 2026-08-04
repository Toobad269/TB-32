#!/usr/bin/env python3
"""
Zaehlt, welche TB-32-Befehle waehrend einer typischen Last wirklich laufen.

Die Ausfuehrungskette in hardware/cpu.py wird von oben nach unten geprueft --
jeder Vergleich kostet Zeit. Wer sie umsortiert, sollte vorher hiermit messen
statt zu raten.

    python3 tools/opstat.py
"""
import collections
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)
os.environ["SDL_VIDEODRIVER"] = "dummy"
os.environ["SDL_AUDIODRIVER"] = "dummy"

from hardware.machine import Machine
from tools.headless import test_cmos, test_platte
from hardware import devices as dev
from hardware.isa import RAM_SIZE, INSTRUCTIONS


def main():
    m = Machine(ROOT, disk=test_platte(), cmos=test_cmos())
    m.power_on()
    dt = 1 / 60

    def run(s):
        for _ in range(int(s / dt)):
            m.run_slice(dt)

    def tippe(text, warten=1.2):
        for ch in text:
            m.keyboard.push(ord(ch), 0)
            run(0.08)
        m.keyboard.push(13, dev.KEY_ENTER)
        run(warten)

    run(3.6)
    tippe("CD SOURCE")
    tippe("CC CC.C X.TBX", 0.5)          # der Compiler ist die typische Last

    zaehler = collections.Counter()
    ram = m.bus.ram
    for _ in range(300000):
        pc = m.cpu.pc
        if pc < RAM_SIZE:
            w = ram[pc] | (ram[pc+1] << 8) | (ram[pc+2] << 16) | (ram[pc+3] << 24)
        else:
            w = m.bus.read32(pc)
        zaehler[w >> 24] += 1
        m.cpu.run(1)

    namen = {v[0]: k for k, v in INSTRUCTIONS.items()}
    gesamt = sum(zaehler.values())
    print("Befehlshäufigkeit während eines Compilerlaufs:\n")
    for op, n in zaehler.most_common(20):
        print(f"   0x{op:02X}  {namen.get(op, '?'):8} {n * 100 / gesamt:5.1f} %")
    print("\nDiese Reihenfolge sollte die Kette in hardware/cpu.py haben.")


if __name__ == "__main__":
    sys.exit(main())
