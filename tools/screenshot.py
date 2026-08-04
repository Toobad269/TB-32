#!/usr/bin/env python3
"""
Bootet den PC ohne Fenster und speichert ein Bildschirmfoto als PNG.
So kann ich pruefen, wie der Bildschirm wirklich aussieht.

    python3 tools/screenshot.py bild.png [sekunden] [--keys "DEL,DOWN,ENTER"]
        [--type "10.0:text|ENTER"]  [--mouse "6:x:y:click"]
"""

import os
import sys

os.environ["SDL_VIDEODRIVER"] = "dummy"
os.environ["SDL_AUDIODRIVER"] = "dummy"
os.environ.setdefault("PYGAME_HIDE_SUPPORT_PROMPT", "1")

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)

import pygame
from hardware.machine import Machine
from tools.headless import KEYNAMES, test_cmos, test_platte


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "screen.png"
    seconds = float(sys.argv[2]) if len(sys.argv) > 2 and not sys.argv[2].startswith("-") else 3.0
    scale = int(sys.argv[sys.argv.index("--scale") + 1]) if "--scale" in sys.argv else 2

    keys = []
    if "--keys" in sys.argv:
        for part in sys.argv[sys.argv.index("--keys") + 1].split(","):
            part = part.strip()
            if part.upper() in KEYNAMES:
                keys.append(KEYNAMES[part.upper()])
            else:
                for ch in part:
                    keys.append((0, ord(ch)))

    pygame.init()
    pygame.display.set_mode((640 * scale, 400 * scale))
    from pc import Monitor
    monitor = Monitor(scale)

    m = Machine(ROOT, disk=test_platte(), cmos=test_cmos())
    m.power_on()
    dt = 1 / 60
    start_typing = int(float(sys.argv[sys.argv.index("--after") + 1]) / dt) \
        if "--after" in sys.argv else int(2.4 / dt)

    # Tippen zu bestimmten Zeiten:  --type "10.0:hallo|, 12.0:ENTER"
    # (mehrere Eintraege durch Komma, Sondertasten mit Namen wie bei --keys)
    tippen = []
    if "--type" in sys.argv:
        for teil in sys.argv[sys.argv.index("--type") + 1].split(","):
            sek, text = teil.strip().split(":", 1)
            folge = []
            for stueck in text.split("|"):
                if stueck.upper() in KEYNAMES:
                    folge.append(KEYNAMES[stueck.upper()])
                else:
                    for ch in stueck:
                        folge.append((0, ord(ch)))
            tippen.append([float(sek), folge])

    # Mausskript:  --mouse "sekunde:x:y:aktion, ..."  (aktion: move|click|down|up)
    maus = []
    if "--mouse" in sys.argv:
        for teil in sys.argv[sys.argv.index("--mouse") + 1].split(","):
            f = teil.strip().split(":")
            maus.append((float(f[0]), int(f[1]), int(f[2]),
                         f[3] if len(f) > 3 else "move"))

    frame = 0
    for i in range(int(seconds / dt)):
        m.run_slice(dt)
        if keys and i >= start_typing and (i - start_typing) % 6 == 0:
            sc, a = keys.pop(0)
            m.keyboard.push(a, sc)
        while tippen and tippen[0][0] <= i * dt:
            for sc, a in tippen.pop(0)[1]:
                m.keyboard.push(a, sc)
                for _ in range(5):
                    m.run_slice(dt)
        while maus and maus[0][0] <= i * dt:
            _, mxp, myp, aktion = maus.pop(0)
            if aktion == "click":
                m.mouse.move(mxp, myp, 0)
                for _ in range(4):
                    m.run_slice(dt)
                m.mouse.move(mxp, myp, 1)
                for _ in range(4):
                    m.run_slice(dt)
                m.mouse.move(mxp, myp, 0)
            elif aktion == "down":
                m.mouse.move(mxp, myp, 1)
            elif aktion == "up":
                m.mouse.move(mxp, myp, 0)
            else:
                m.mouse.move(mxp, myp, m.mouse.buttons)
        if not m.running:
            break

    target = pygame.Surface((640 * scale, 400 * scale))
    if "--scrollback" in sys.argv:
        import pc as pcmod
        n = int(sys.argv[sys.argv.index("--scrollback") + 1])
        monitor.render(m.vga, target, force=True,
                       history=pcmod.build_history_view(m, n))
    else:
        monitor.render(m.vga, target, force=True)
    pygame.image.save(target, out)
    print(f"{out} gespeichert  ({m.total_instructions:,} Befehle ausgeführt)")
    m.shutdown()


if __name__ == "__main__":
    main()
