# Bauen und Testen

## Der Bauvorgang

`python3 build.py` macht der Reihe nach:

1. `firmware/bios.asm` → `bios.bin` (ROM, max 64 KB). Danach trägt
   `bios_kopf_stempeln` Länge und Prüfsumme in den Kopf ein — ohne die
   nimmt das Mainboard das Abbild nicht an, siehe [[16 Eigenes BIOS schreiben]].
   Dasselbe für `firmware/minimal.asm` → `minimal.bin`, das kleine BIOS zum
   Selberumbauen
2. `system/boot.asm` → Bootsektor (max 512 Byte)
3. `system/kernel.c` → TCC → `+ start.asm` → Assembler → `kernel.bin`
   (aktuell ~250 KB). `build.py` prüft, dass der Kernel im RAM nicht bis
   `0xB0000` reicht — dort beginnen die festen Puffer des Dateisystems,
   siehe [[07 Fallstricke]]
4. **Nur Sektor 0** ins Abbild schreiben. Das Dateisystem ab Sektor 512
   bleibt unangetastet — sonst verliert ein nebenher laufender Emulator
   seine Dateien, siehe [[07 Fallstricke]]
5. `programs/*.c` übersetzen und einsortieren:
   `CC/ASM/PY.TBX` → `\SYSTEM`, Rest → `\PROGS`,
   `cc.c` + `proglib.c` → `\SOURCE`, alles aus `diskfiles/` 1:1 aufs Laufwerk
6. Der **Kernel selbst als Datei** `\SYSTEM\KERNEL.BIN`, dazu `BIOS.BIN` und
   `KERNEL.SYM`. Das ist keine Kopie zum Ansehen: **der Bootsektor sucht
   genau diese Datei.** Löscht man sie, startet der Rechner nicht mehr —
   `python3 build.py` legt sie wieder hin

Zwischenergebnisse zum Nachsehen: `system/kernel.asm` (erzeugter Assembler),
`system/kernel.sym` (**Symboltabelle — Adressen aller Variablen**, sehr
nützlich zum Debuggen).

## Testwerkzeuge

| Werkzeug | Zweck | Dauer |
|---|---|---|
| `tools/selftest.py` | 55 Prüfungen vom Einschalten bis zum Desktop, inklusive BIOS-Flashen auf einer Kopie des Chips | ~2 min |
| `tools/ctest.py --selftest` | 11 Sprachtests für TCC | Sekunden |
| `tools/bootstrap.py` | Compiler übersetzt sich selbst | ~5 min |
| `tools/headless.py` | bootet ohne Fenster, gibt den Bildschirm als Text | frei |
| `tools/screenshot.py` | PNG, mit Tasten- und Mausskript | frei |
| `tools/tbfs.py` | Dateien aufs virtuelle Laufwerk schieben | — |
| `tools/opstat.py` | misst die Befehlshäufigkeit — Grundlage für die Reihenfolge der Ausführungskette | ~1 min |

`tools/screenshot.py` kann außerdem zu bestimmten Zeiten tippen:
`--type "10.0:int main() {|ENTER, 11.0:}"` — Sondertastennamen wie bei
`--keys`, mehrere Stücke mit `|` getrennt.

### headless

```bash
python3 tools/headless.py 12 --keys "DIR,ENTER,TEMP,ENTER"
python3 tools/headless.py 8 --keys "DEL" --after 0.9      # ins BIOS-Setup
```

Tastennamen: `ENTER ESC DEL F1 F2 F5 F10 UP DOWN LEFT RIGHT BACKSPACE TAB
SPACE PGUP PGDN HOME END`. Alles andere wird Zeichen für Zeichen getippt.
`--after` legt fest, ab welcher Sekunde getippt wird (Standard 2.4, damit der
POST nicht dazwischenfunkt).

### screenshot mit Maus

```bash
python3 tools/screenshot.py /tmp/x.png 14 --keys "WIN,ENTER" \
    --mouse "6.0:25:387:click, 7.5:60:290:click"
```

Format: `sekunde:x:y:aktion`, Aktionen `click move down up`.
Koordinaten in Bildschirmpunkten (640×400), **nicht** in Fensterpixeln.

### Eigene Prüfskripte

Für alles Feinere ein Python-Schnipsel schreiben, das `Machine` direkt
steuert. Muster:

```python
import os, sys
os.environ["SDL_VIDEODRIVER"] = "dummy"; os.environ["SDL_AUDIODRIVER"] = "dummy"
sys.path.insert(0, '.')
from hardware.machine import Machine
from hardware import devices as dev
from tools.headless import screen_text
m = Machine('.'); m.power_on()
dt = 1/60
def run(s):
    for _ in range(int(s/dt)): m.run_slice(dt)
def tippe(t):
    for ch in t: m.keyboard.push(ord(ch), 0); run(0.06)
    m.keyboard.push(13, dev.KEY_ENTER); run(0.8)
def klick(x, y):
    m.mouse.move(x,y,0); run(0.15); m.mouse.move(x,y,1); run(0.3); m.mouse.move(x,y,0); run(0.9)
run(3.5)                      # bis zur Eingabeaufforderung
```

**Variablen des laufenden Systems auslesen** (Gold wert beim Debuggen):

```python
adr = {n: int(a,16) for a, n in (z.split() for z in open('system/kernel.sym'))}
import struct
def gw(name, i=0): return struct.unpack_from('<i', m.bus.ram, adr[name] + i*4)[0]
print(gw('p_switches'), gw('edg_build'), gw('p_state', 1))
```

Die Ausgabe eines Programms, das im Grafikmodus unsichtbar läuft, steht im
Textbildspeicher — `screen_text(m)` zeigt sie trotzdem.

## Koordinaten für Klicktests

Bildschirm 640×400, Leiste ab y = 378.

| Ziel | Klickpunkt |
|---|---|
| Start-Knopf | 25, 387 |
| Erstes Schreibtischsymbol | 50, 55 (nur wenn kein Fenster darüber liegt) |
| Startmenü Eintrag *n* (0 = File Manager) | 60, 262 + n·14 |
| Fensterknopf 1 in der Leiste | 90, 387 |

Startmenü: 0 File Manager, 1 Command Prompt, 2 Editor, 3 System Monitor,
4 Control Panel, 5 Clock, 6 About, 7 Exit.

**Der Selbsttest hängt nicht mehr an der Startgeschwindigkeit:** `Lauf`
sammelt in `gesehen` alles mit, was seit dem Einschalten auf dem Schirm
stand — ein Blick zu einem festen Zeitpunkt genügt nicht.

**Wie lange der Start dauert:** ohne *Quick Boot* rund **4 Sekunden** bis
zur Eingabeaufforderung (POST mit sichtbar hochzählendem Speicher ~1,5 s,
dann 2 s Bedenkzeit für DEL). Mit *Quick Boot* im CMOS sind es **0,6 s**.
Wer Tests schreibt, muss lang genug warten.

Editorfenster liegt bei (40, 82), 596×292 — Knopfleiste bei y = 348:
New 44–88, Save 94–146, Rename 152–224, Compile 230–306, Run 312–360.

## Wenn ein Test scheitert

1. Reicht die Wartezeit? Kompilierläufe brauchen simulierte Sekunden.
2. Läuft die Maschine überhaupt noch? `m.cpu.halted`, `m.cpu.last_fault`.
3. Steht der Scheduler? `gw('p_switches')` zweimal messen — siehe
   [[07 Fallstricke]].
4. Bildschirm ansehen: `screen_text(m)` oder Screenshot.

Verwandt: [[00 START HIER]], [[07 Fallstricke]]

## Die C-Fassung des Emulators

```bash
cd emu && make          # baut emu/tb32
./emu/tb32 4.0 "dir"    # kopflos booten und einen Befehl tippen
```

Geprüft wird sie gegen die Python-Fassung:

```bash
python3 tools/emu_vergleich.py
```

Der Test führt in **beiden** Emulatoren einzelne Befehle aus und vergleicht
nach jedem Programmzähler und Flags — die erste Abweichung wird mit
Umgebung ausgegeben. Danach wird der ganze Bootvorgang Zeichen für Zeichen
verglichen. Wer an `hardware/cpu.py` oder `emu/cpu.c` etwas ändert, lässt
diesen Test laufen.
