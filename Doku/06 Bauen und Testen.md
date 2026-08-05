# Building and Testing

## The Build Process

`python3 build.py` does, in order:

1. `firmware/bios.asm` → `bios.bin` (ROM, max 64 KB). Afterward
   `bios_kopf_stempeln` writes length and checksum into the header — without
   that the mainboard won't accept the image, see [[16 Eigenes BIOS schreiben]].
   Same for `firmware/minimal.asm` → `minimal.bin`, the small BIOS meant for
   hands-on rebuilding
2. `system/boot.asm` → boot sector (max 512 bytes)
3. `system/kernel.c` → TCC → `+ start.asm` → assembler → `kernel.bin`
   (currently ~250 KB). `build.py` checks that the kernel in RAM doesn't reach
   `0xB0000` — that's where the filesystem's fixed buffers begin,
   see [[07 Fallstricke]]
4. Write **only sector 0** into the image. The filesystem from sector 512
   onward is left untouched — otherwise an emulator running alongside would
   lose its files, see [[07 Fallstricke]]
5. Compile `programs/*.c` and sort them in:
   `CC/ASM/PY.TBX` → `\SYSTEM`, everything else → `\PROGS`,
   `cc.c` + `proglib.c` → `\SOURCE`, everything from `diskfiles/` copied 1:1 onto the drive
6. The **kernel itself as a file** `\SYSTEM\KERNEL.BIN`, plus `BIOS.BIN` and
   `KERNEL.SYM`. This isn't a copy for viewing: **the boot sector looks for
   exactly this file.** Delete it and the machine won't boot anymore —
   `python3 build.py` puts it back

Intermediate results worth checking: `system/kernel.asm` (generated assembly),
`system/kernel.sym` (**symbol table — addresses of all variables**, very
useful for debugging).

## Test Tools

| Tool | Purpose | Duration |
|---|---|---|
| `tools/selftest.py` | 55 checks from power-on to the desktop, including flashing the BIOS onto a copy of the chip | ~2 min |
| `tools/ctest.py --selftest` | 11 language tests for TCC | seconds |
| `tools/bootstrap.py` | compiler compiles itself | ~5 min |
| `tools/headless.py` | boots without a window, returns the screen as text | free |
| `tools/screenshot.py` | PNG, with a key/mouse script | free |
| `tools/tbfs.py` | push files onto the virtual drive | — |
| `tools/opstat.py` | measures instruction frequency — basis for the ordering of the dispatch chain | ~1 min |

`tools/screenshot.py` can also type at specific times:
`--type "10.0:int main() {|ENTER, 11.0:}"` — special key names as with
`--keys`, multiple pieces separated by `|`.

### headless

```bash
python3 tools/headless.py 12 --keys "DIR,ENTER,TEMP,ENTER"
python3 tools/headless.py 8 --keys "DEL" --after 0.9      # into BIOS setup
```

Key names: `ENTER ESC DEL F1 F2 F5 F10 UP DOWN LEFT RIGHT BACKSPACE TAB
SPACE PGUP PGDN HOME END`. Anything else is typed character by character.
`--after` determines the second at which typing begins (default 2.4, so it
doesn't interfere with POST).

### screenshot with mouse

```bash
python3 tools/screenshot.py /tmp/x.png 14 --keys "WIN,ENTER" \
    --mouse "6.0:25:387:click, 7.5:60:290:click"
```

Format: `second:x:y:action`, actions `click move down up`.
Coordinates are in screen points (640×400), **not** window pixels.

### Custom test scripts

For anything finer, write a Python snippet that drives `Machine` directly.
Pattern:

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
run(3.5)                      # up to the prompt
```

**Reading variables of the running system** (worth gold for debugging):

```python
adr = {n: int(a,16) for a, n in (z.split() for z in open('system/kernel.sym'))}
import struct
def gw(name, i=0): return struct.unpack_from('<i', m.bus.ram, adr[name] + i*4)[0]
print(gw('p_switches'), gw('edg_build'), gw('p_state', 1))
```

The output of a program running invisibly in graphics mode sits in the
text screen buffer — `screen_text(m)` shows it anyway.

## Coordinates for Click Tests

Screen 640×400, taskbar starting at y = 378.

| Target | Click point |
|---|---|
| Start button | 25, 387 |
| First desktop icon | 50, 55 (only if no window is on top of it) |
| Start menu entry *n* (0 = File Manager) | 60, 262 + n·14 |
| Taskbar window button 1 | 90, 387 |

Start menu: 0 File Manager, 1 Command Prompt, 2 Editor, 3 System Monitor,
4 Control Panel, 5 Clock, 6 About, 7 Exit.

**The self-test no longer depends on boot speed:** `Lauf` collects in
`gesehen` everything that has appeared on screen since power-on — a single
glance at a fixed point in time is not enough.

**How long booting takes:** without *Quick Boot*, about **4 seconds** to
reach the prompt (POST with visibly counting-up memory ~1.5s, then 2s of
waiting for DEL). With *Quick Boot* enabled in CMOS it's **0.6s**. Anyone
writing tests has to wait long enough.

Editor window sits at (40, 82), 596×292 — button bar at y = 348:
New 44–88, Save 94–146, Rename 152–224, Compile 230–306, Run 312–360.

## When a Test Fails

1. Is the wait time long enough? Compile runs need simulated seconds.
2. Is the machine still running at all? `m.cpu.halted`, `m.cpu.last_fault`.
3. Is the scheduler stuck? Measure `gw('p_switches')` twice — see
   [[07 Fallstricke]].
4. Look at the screen: `screen_text(m)` or a screenshot.

Related: [[00 START HIER]], [[07 Fallstricke]]

## The C Version of the Emulator

```bash
cd emu && make          # builds emu/tb32
./emu/tb32 4.0 "dir"    # boot headless and type a command
```

Checked against the Python version:

```bash
python3 tools/emu_vergleich.py
```

The test runs individual instructions in **both** emulators and compares
program counter and flags after each one — the first discrepancy is printed
along with its context. After that, the entire boot process is compared
character by character. Anyone who changes anything in `hardware/cpu.py` or
`emu/cpu.c` should run this test.
