#!/usr/bin/env python3
"""
Setzt den TB-32 auf den Auslieferungszustand zurueck.

    python3 reset.py            fragt nach und loescht dann das Laufwerk
    python3 reset.py --bios     nur den BIOS-Chip, das Laufwerk bleibt
    python3 reset.py --ja       ohne Rueckfrage (fuer Skripte)

Warum es das braucht: `build.py` fasst das Dateisystem ab Sektor 512
absichtlich NICHT an. Sonst waeren bei jedem Bau die eigenen Sachen weg --
und liefe nebenher der Emulator, verloere er seine gerade gespeicherten
Dateien. Wer wirklich einen Werkszustand will, sagt es also hier.
"""

import os
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.abspath(__file__))
DISK = os.path.join(ROOT, "disk", "hd0.img")
CMOS = os.path.join(ROOT, "disk", "cmos.bin")
FW = os.path.join(ROOT, "firmware")

ROT, GRUEN, GELB, WEG = "\033[91m", "\033[92m", "\033[93m", "\033[0m"


def groesse(pfad):
    return os.path.getsize(pfad) if os.path.exists(pfad) else 0


def eigene_dateien():
    """Was der Benutzer selbst auf dem Laufwerk hat -- nur zum Warnen."""
    try:
        sys.path.insert(0, ROOT)
        from tools.tbfs import TBFS
        fs = TBFS(DISK)
        if not fs.formatted():
            return []
        bekannt = {"SYSTEM", "PROGS", "SOURCE", "RECYCLED", "DESKTOP"}
        return [n for n, _, _, _ in fs.list(-1) if n.upper() not in bekannt]
    except Exception:
        return []


def bios_zuruecksetzen():
    """Den Chip auf das gebaute BIOS zurueck und die Sicherung wegraeumen."""
    weg = 0
    for name in ("bios.backup.bin", "minimal.bin"):
        p = os.path.join(FW, name)
        if name == "bios.backup.bin" and os.path.exists(p):
            os.remove(p)
            weg += 1
    return weg


def main():
    nur_bios = "--bios" in sys.argv
    ohne_frage = "--ja" in sys.argv

    print("TB-32 zuruecksetzen\n")
    if nur_bios:
        print("  Nur der BIOS-Chip. Das Laufwerk bleibt, wie es ist.")
    else:
        print(f"  Laufwerk   {DISK}")
        print(f"             {groesse(DISK) // 1024} KiB")
        eigene = eigene_dateien()
        if eigene:
            print(f"\n  {ROT}Darauf liegen eigene Dateien im Hauptverzeichnis:{WEG}")
            for n in eigene[:12]:
                print(f"    {n}")
            if len(eigene) > 12:
                print(f"    ... und {len(eigene) - 12} weitere")
            print(f"\n  {ROT}Sie sind danach weg. Endgueltig.{WEG}")
        print(f"\n  {GELB}Alles, was du IM TB-32 erstellt hast, geht verloren.{WEG}")
        print("  Der Quelltext auf dem Mac bleibt unberuehrt.")

    if not ohne_frage:
        print()
        # Ohne Tastatur gibt es keine Rueckfrage -- und ohne Rueckfrage wird
        # nichts geloescht. Das passiert, wenn man die Datei aus einem Editor
        # heraus startet: dort haengt keine Eingabe am Programm, input() bricht
        # sofort mit EOFError ab. Statt eines Absturzes soll dastehen, was zu
        # tun ist.
        if not sys.stdin or not sys.stdin.isatty():
            print(f"  {GELB}Hier haengt keine Tastatur dran{WEG} -- vermutlich "
                  "startest du die Datei\n  aus einem Editor. Die Sicherheitsfrage "
                  "koennte niemand beantworten.\n")
            print("  Im Terminal starten:      python3 reset.py")
            print("  Oder ohne Rueckfrage:     python3 reset.py --ja")
            print("\n  Nichts geaendert.")
            return 1
        try:
            antwort = input("  Wirklich zuruecksetzen? Tippe JA: ")
        except EOFError:
            print("\n  Keine Eingabe moeglich. Nichts geaendert.")
            return 1
        if antwort.strip().upper() != "JA":
            print("\n  Abgebrochen. Nichts geaendert.")
            return 0

    print()
    if not nur_bios:
        for pfad, was in ((DISK, "Laufwerk"), (CMOS, "CMOS")):
            if os.path.exists(pfad):
                os.remove(pfad)
                print(f"  {was} geloescht")
    n = bios_zuruecksetzen()
    if n:
        print(f"  BIOS-Sicherung geloescht")

    print("\n  Baue neu ...\n")
    r = subprocess.run([sys.executable, "build.py"], cwd=ROOT)
    if r.returncode != 0:
        print(f"\n  {ROT}Der Bau ist fehlgeschlagen.{WEG}")
        return 1
    print(f"\n  {GRUEN}Fertig. Der Rechner ist im Auslieferungszustand.{WEG}")
    print("  Starten mit:  python3 pc.py")
    return 0


if __name__ == "__main__":
    sys.exit(main())
