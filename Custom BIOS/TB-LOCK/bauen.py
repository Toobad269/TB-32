#!/usr/bin/env python3
"""
Baut TB-LOCK.bin -- das BIOS-Abbild mit Setup-Passwort.

    python3 "Custom BIOS/TB-LOCK/bauen.py"

Es benutzt denselben Assembler und denselben Kopfstempel wie build.py, damit
das Ergebnis fuer das Mainboard nicht von einem regulaeren BIOS zu
unterscheiden ist: Sprung, "TBBI", Laenge, Pruefsumme.

Geflasht wird es NICHT von hier. Der Weg dorthin ist DEL -> Firmware ->
Flash BIOS from File, oder zum Ausprobieren `pruefen.py`, das den Rechner
gleich mit diesem Chip startet.
"""

import os
import sys

HIER = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HIER))
sys.path.insert(0, ROOT)

from build import asm_file, bios_kopf_stempeln
from hardware.isa import ROM_SIZE


def bauen(still=False):
    quelle = os.path.join(HIER, "bios.asm")
    ziel = os.path.join(HIER, "TB-LOCK.bin")
    daten, _ = asm_file(quelle, ziel, os.path.join(HIER, "TB-LOCK.sym"))
    if len(daten) > ROM_SIZE:
        raise SystemExit(
            f"TB-LOCK passt nicht ins ROM: {len(daten)} Bytes, Platz sind "
            f"{ROM_SIZE}.")
    daten, summe = bios_kopf_stempeln(ziel)
    if not still:
        print(f"  TB-LOCK {len(daten):6d} Bytes  "
              f"({len(daten) * 100 // ROM_SIZE}% des ROMs, "
              f"Pruefsumme {summe:08X})")
        print(f"  -> {ziel}")
    return ziel, daten, summe


if __name__ == "__main__":
    bauen()
