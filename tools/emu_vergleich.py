#!/usr/bin/env python3
"""
Prueft die C-Fassung des Emulators gegen die Python-Fassung.

Zwei Emulatoren desselben Rechners sind nur dann etwas wert, wenn sie
GENAU dasselbe rechnen. Dieser Test macht das nachpruefbar:

  1. Schritt fuer Schritt: beide fuehren einzelne Befehle aus, und nach
     jedem werden Programmzaehler und Flags verglichen. Die erste
     Abweichung wird mit Umgebung ausgegeben -- so findet man einen
     Portierungsfehler in Sekunden statt in Tagen.
  2. Der ganze Bootvorgang: am Ende muss der Textbildschirm Zeichen fuer
     Zeichen gleich sein.

    python3 tools/emu_vergleich.py [Schritte]
"""

import os
import subprocess
import sys
import tempfile
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)

os.environ["SDL_VIDEODRIVER"] = "dummy"
os.environ["SDL_AUDIODRIVER"] = "dummy"

from tools.headless import test_cmos, test_platte

GRUEN, ROT, WEG = "\033[92m", "\033[91m", "\033[0m"

SPUR_C = r'''
#include "tb32.h"
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char** argv) {
    int n = argc > 1 ? atoi(argv[1]) : 200000, i;
    Machine *m = m_new("firmware/bios.bin", "disk/hd0.img",
                       argc > 3 ? argv[3] : "disk/cmos.bin");
    FILE* f = fopen(argv[2], "w");
    if (!m || !f) return 1;
    m_power_on(m);
    m->timer_hz = 0;
    for (i = 0; i < n; i++) {
        if (i % 6666 == 0) { m->timer_ticks++; m->irq_pending |= 1u << IRQ_TIMER; }
        fprintf(f, "%08X %08X\n", m->pc, m->flags);
        cpu_run(m, 1);
        if (m->fault[0]) { fprintf(f, "FEHLER %s\n", m->fault); break; }
    }
    fclose(f);
    return 0;
}
'''


def bauen():
    r = subprocess.run(["make"], cwd=os.path.join(ROOT, "emu"),
                       capture_output=True, text=True)
    return r.returncode == 0, r.stdout + r.stderr


def spur_c(schritte, ziel, platte):
    quelle = os.path.join(tempfile.gettempdir(), "spur_c.c")
    binaer = os.path.join(tempfile.gettempdir(), "spur_c")
    with open(quelle, "w") as f:
        f.write(SPUR_C)
    r = subprocess.run(["cc", "-O2", "-std=c99", "-I", os.path.join(ROOT, "emu"),
                        "-o", binaer, quelle,
                        os.path.join(ROOT, "emu", "cpu.c"),
                        os.path.join(ROOT, "emu", "machine.c")],
                       capture_output=True, text=True)
    if r.returncode != 0:
        print(r.stderr)
        return False
    subprocess.run([binaer, str(schritte), ziel, test_cmos(), platte], cwd=ROOT)
    return True


def spur_py(schritte, ziel, platte):
    from hardware.machine import Machine
    from hardware.isa import IRQ_TIMER
    m = Machine(ROOT, disk=platte, cmos=test_cmos())
    m.power_on()
    m.timer.hz = 0
    with open(ziel, "w") as f:
        for i in range(schritte):
            if i % 6666 == 0:
                m.timer.ticks += 1
                m.cpu.raise_irq(IRQ_TIMER)
            f.write("%08X %08X\n" % (m.cpu.pc, m.cpu.flags))
            m.cpu.run(1)
            if m.cpu.last_fault:
                f.write("FEHLER %s\n" % m.cpu.last_fault)
                break


def main():
    schritte = int(sys.argv[1]) if len(sys.argv) > 1 else 200000
    print("Baue die C-Fassung ...")
    ok, aus = bauen()
    if not ok:
        print(aus)
        return 1

    tmp = tempfile.gettempdir()
    a_pfad = os.path.join(tmp, "spur_c.txt")
    b_pfad = os.path.join(tmp, "spur_py.txt")

    print(f"\n--- Schritt fuer Schritt ({schritte} Befehle) ---------------")
    platte = test_platte()          # beide Seiten sehen dieselbe Platte
    if not spur_c(schritte, a_pfad, platte):
        return 1
    spur_py(schritte, b_pfad, platte)

    a = open(a_pfad).read().splitlines()
    b = open(b_pfad).read().splitlines()
    fehler = 0
    for i, (x, y) in enumerate(zip(a, b)):
        if x != y:
            print(f"  [{ROT} ABWEICHUNG {WEG}] bei Schritt {i}:")
            for k in range(max(0, i - 3), min(len(a), i + 2)):
                mark = "   <-- hier" if k == i else ""
                print(f"      {k:7d}  C: {a[k]}   Python: {b[k]}{mark}")
            fehler = 1
            break
    else:
        print(f"  [{GRUEN}   OK   {WEG}] {min(len(a), len(b))} Befehle, "
              f"Programmzaehler und Flags jedes Mal gleich")

    print("\n--- Der ganze Bootvorgang -----------------------------------")
    platte = test_platte()
    r = subprocess.run([os.path.join(ROOT, "emu", "tb32"), "4.0", "",
                        test_cmos(), platte],
                       cwd=ROOT, capture_output=True, text=True)
    c_schirm = [z.rstrip() for z in r.stdout.splitlines()]
    tempo_c = r.stderr.strip().splitlines()[-1] if r.stderr.strip() else ""

    from hardware.machine import Machine
    from tools.headless import screen_text
    m = Machine(ROOT, disk=platte, cmos=test_cmos())
    m.power_on()
    t0 = time.perf_counter()
    for _ in range(240):
        m.run_slice(1 / 60)
    d = time.perf_counter() - t0
    py_schirm = [z.rstrip() for z in screen_text(m)]

    if c_schirm == py_schirm:
        print(f"  [{GRUEN}   OK   {WEG}] Bildschirm Zeichen fuer Zeichen gleich")
    else:
        print(f"  [{ROT} ABWEICHUNG {WEG}] Bildschirme unterscheiden sich:")
        for i, (x, y) in enumerate(zip(c_schirm, py_schirm)):
            if x != y:
                print(f"      Zeile {i}\n        C:      {x!r}\n        Python: {y!r}")
        fehler = 1

    print("\n--- Tempo ---------------------------------------------------")
    print(f"  C:      {tempo_c}")
    print(f"  Python: {m.total_instructions} Befehle in {d:.3f} s "
          f"= {m.total_instructions / d / 1e6:.1f} Millionen/s")

    return fehler


if __name__ == "__main__":
    sys.exit(main())
