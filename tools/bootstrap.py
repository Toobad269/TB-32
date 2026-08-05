#!/usr/bin/env python3
"""
Bootstrap proof: the C compiler compiles itself.

The chain:
    Stage 1   CC.TBX    was built on the Mac (tools/tcc.py)
    Stage 2   CC2.TBX = CC.TBX  compiles CC.C     -- on the TB-32
    Stage 3   CC3.TBX = CC2.TBX compiles CC.C     -- on the TB-32

If stage 2 and stage 3 are byte-for-byte identical, the compiler is a
fixed point: it reproduces itself unchanged. This is exactly how
bootstrapping is proven -- stage 1 is allowed to differ, since it comes
from a different compiler.

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
    """A running machine that can wait for screen output."""

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
        """Runs until <text> appears on screen."""
        schritte = int(max_sek / self.dt)
        for i in range(schritte):
            self.m.run_slice(self.dt)
            if i % 60 == 0 and text in self.bild():
                self.laufen(0.4)
                return True
            if melde and i % 1800 == 0 and i:
                print(f"      ... {int(i * self.dt)} s of compute time elapsed")
        return text in self.bild()

    def tippe(self, zeile):
        """Types a line and presses Enter -- character by character, with
        enough spacing that the keyboard buffer doesn't overflow."""
        for ch in zeile:
            self.m.keyboard.push(ord(ch), 0)
            self.laufen(0.05)
        self.m.keyboard.push(13, dev.KEY_ENTER)
        self.laufen(0.2)


def main():
    print("Bootstrap proof for the TB-32 C compiler\n")

    pc = PC()
    print("  Machine starting ...")
    if not pc.warte_auf("A:\\>", 30):
        print(f"{ROT}  The system did not reach the command prompt.{WEG}")
        return 1

    pc.tippe("CD SOURCE")            # this is where the compiler's source lives
    pc.laufen(0.5)
    pc.tippe("CLS")
    pc.laufen(0.5)

    print("\n  Stage 2:  CC.TBX compiles its own source CC.C")
    pc.tippe("CC CC.C CC2.TBX")
    if not pc.warte_auf("Created CC2.TBX", 900, melde=True):
        print(f"{ROT}  Stage 2 failed:{WEG}")
        print(pc.bild())
        return 1
    for z in pc.bild().splitlines():
        if "Code:" in z or "Compiling" in z:
            print("     ", z.strip())
    print(f"{GRUEN}      Stage 2 built.{WEG}")

    pc.tippe("CLS")
    pc.laufen(0.5)

    print("\n  Stage 3:  CC2.TBX compiles the same source again")
    pc.tippe("CC2 CC.C CC3.TBX")
    if not pc.warte_auf("Created CC3.TBX", 900, melde=True):
        print(f"{ROT}  Stage 3 failed:{WEG}")
        print(pc.bild())
        return 1
    for z in pc.bild().splitlines():
        if "Code:" in z or "Compiling" in z:
            print("     ", z.strip())
    print(f"{GRUEN}      Stage 3 built.{WEG}")

    pc.tippe("CLS")
    pc.laufen(0.5)

    print("\n  Comparing the two stages (on the device itself):")
    pc.tippe("FC CC2.TBX CC3.TBX")
    pc.warte_auf("FC:", 60)
    pc.laufen(1.0)
    ergebnis = pc.bild()
    for z in ergebnis.splitlines():
        if "Comparing" in z or "identical" in z or "different" in z:
            print("     ", z.strip())

    identisch = "identical" in ergebnis

    print("\n  Cross-check: does the self-built compiler run?")
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
        print(f"{GRUEN}  BOOTSTRAPPING PASSED{WEG}")
        print("  Stage 2 and stage 3 are byte-for-byte identical, and the")
        print("  self-built compiler compiles correct programs.")
        print("  The TB-32 can produce its own compiler without the Mac.")
    else:
        print(f"{ROT}  Bootstrapping not yet achieved.{WEG}")
        if not identisch:
            print("  Stage 2 and stage 3 differ.")
        if not laeuft:
            print("  The built compiler does not produce a working program.")

    pc.m.shutdown()
    return 0 if (identisch and laeuft) else 1


if __name__ == "__main__":
    sys.exit(main())
