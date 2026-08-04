#!/usr/bin/env python3
"""
Bootet den virtuellen PC ohne Fenster und gibt den Bildschirminhalt als Text
aus. Damit kann ich Firmware testen, ohne jedes Mal hinzuschauen.

    python3 tools/headless.py [sekunden] [--keys "DEL,ENTER"] [--regs]
"""

import os
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)

from hardware.machine import Machine
from hardware import devices as dev

KEYNAMES = {
    "ESC": (dev.KEY_ESC, 27), "ENTER": (dev.KEY_ENTER, 13),
    "DEL": (dev.KEY_DEL, 0), "F10": (dev.KEY_F10, 0),
    "F1": (dev.KEY_F1, 0), "F2": (dev.KEY_F2, 0), "F5": (dev.KEY_F5, 0),
    "UP": (dev.KEY_UP, 0), "DOWN": (dev.KEY_DOWN, 0),
    "LEFT": (dev.KEY_LEFT, 0), "RIGHT": (dev.KEY_RIGHT, 0),
    "BACKSPACE": (dev.KEY_BACKSPACE, 8), "TAB": (dev.KEY_TAB, 9),
    "SPACE": (0x39, 32),
    "PGUP": (dev.KEY_PGUP, 0), "PGDN": (dev.KEY_PGDN, 0),
    "HOME": (dev.KEY_HOME, 0), "END": (dev.KEY_END, 0),
}


def test_cmos():
    """Ein eigenes CMOS fuer Testlaeufe: Startziel = Textkonsole.

    Seit der Rechner in den Schreibtisch startet, stehen im Textbildspeicher
    nur noch die Reste von vorher -- jedes Werkzeug, das screen_text() liest,
    waere blind. Es bekommt deshalb eine Kopie des CMOS mit Byte 0x1D = 1.
    Das eigene CMOS des Nutzers bleibt unberuehrt, auch beim Herunterfahren."""
    import shutil
    import tempfile
    ziel = os.path.join(tempfile.gettempdir(), "tb32_test_cmos.bin")
    quelle = os.path.join(ROOT, "disk", "cmos.bin")
    if os.path.exists(quelle):
        shutil.copy(quelle, ziel)
    d = bytearray(open(ziel, "rb").read().ljust(64, b"\x00")) if os.path.exists(ziel) \
        else bytearray(64)
    d[0x1D] = 1                       # 1 = Textkonsole
    with open(ziel, "wb") as f:
        f.write(bytes(d))
    return ziel


_platte_nr = 0


def test_platte():
    """Eine Arbeitskopie der Platte, auf der ein offenes Konto liegt.

    Zwei Gruende fuer die Kopie. Erstens legt `build.py` kein Benutzerkonto
    mehr an -- wer den Rechner herunterlaedt, soll seinen Benutzer selbst
    einrichten. Ohne Konto staende jedes Werkzeug in der Ersteinrichtung und
    wartete auf einen Namen; mit dem *eigenen* Konto des Nutzers kaeme es
    nicht am Passwort vorbei. Zweitens hat ein Test auf der Platte des
    Nutzers ohnehin nichts verloren: er schreibt und loescht Dateien.

    Das Konto der Kopie heisst "user" und hat kein Passwort -- pw_summe("")
    ist 0x1234, und das gilt als "nicht gesperrt"."""
    import shutil
    import tempfile
    from tools.tbfs import TBFS
    quelle = os.path.join(ROOT, "disk", "hd0.img")
    global _platte_nr
    _platte_nr = _platte_nr + 1
    ziel = os.path.join(tempfile.gettempdir(),
                        "tb32_test_hd0_%d.img" % _platte_nr)
    shutil.copy(quelle, ziel)
    fs = TBFS(ziel)
    if not fs.formatted():
        return ziel
    konto = bytearray(24)
    konto[:4] = b"user"
    konto[20:24] = (0x1234).to_bytes(4, "little")     # pw_summe("")
    alt = fs.find("USER.DAT", -1)
    if alt >= 0:
        fs.delete("USER.DAT", -1)
    i = fs.put("USER.DAT", bytes(konto))
    e = fs._ent(i) + 24
    fs._put32(e, fs._u32(e) | 256)                    # verstecken
    fs.markiere(513, 8)
    fs.save()
    return ziel


def screen_text(m):
    out = []
    t = m.vga.text
    for y in range(25):
        row = "".join(chr(t[(y * 80 + x) * 2]) or " " for x in range(80))
        out.append(row.rstrip())
    return out


def main():
    seconds = float(sys.argv[1]) if len(sys.argv) > 1 and not sys.argv[1].startswith("-") else 2.0
    keys = []
    if "--keys" in sys.argv:
        spec = sys.argv[sys.argv.index("--keys") + 1]
        for part in spec.split(","):
            part = part.strip()
            if not part:
                continue
            if part.upper() in KEYNAMES:
                keys.append(KEYNAMES[part.upper()])
            else:
                for ch in part:
                    keys.append((0, ord(ch)))

    m = Machine(ROOT, cmos=test_cmos())
    m.power_on()

    dt = 1.0 / 60
    steps = int(seconds / dt)
    # Tasten erst schicken, wenn der POST durch ist -- sonst wirft ihn
    # die Wartschleife des BIOS weg.
    start_typing = int(float(sys.argv[sys.argv.index("--after") + 1]) / dt) \
        if "--after" in sys.argv else int(2.4 / dt)

    for i in range(steps):
        m.run_slice(dt)
        if keys and i >= start_typing and (i - start_typing) % 4 == 0:
            sc, ascii_ = keys.pop(0)
            m.keyboard.push(ascii_, sc)
        if not m.running:
            break

    print("+" + "-" * 80 + "+")
    for row in screen_text(m):
        print("|" + row.ljust(80) + "|")
    print("+" + "-" * 80 + "+")
    print(f"Befehle ausgeführt: {m.total_instructions:,}   "
          f"CPU {'ANGEHALTEN' if m.cpu.halted else 'läuft'}   "
          f"PC=0x{m.cpu.pc:08X}")
    if m.cpu.last_fault:
        print(f"Letzter Fehler: {m.cpu.last_fault}")
    if m.bus.unknown_ports:
        print(f"Unbekannte Ports angesprochen: "
              f"{[hex(p) for p in sorted(m.bus.unknown_ports)]}")
    if "--regs" in sys.argv:
        print(m.cpu.dump())
    m.shutdown()


if __name__ == "__main__":
    main()
