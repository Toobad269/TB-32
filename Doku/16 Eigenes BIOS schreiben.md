# Writing Your Own BIOS

The TB-32's BIOS is replaceable. You can write your own, flash it, and
the computer boots with it — or doesn't, if it's wrong.

This page is the **contract**: everything a BIOS must provide for
TOOBAD-OS to run on it. Whoever sticks to it has a working BIOS,
regardless of what it looks like inside.

Ready-made template to rebuild from: **`firmware/minimal.asm`** (3324
bytes, can't do anything except boot — and that's exactly enough).

---

## The Header: the First 48 Bytes

Without it, the mainboard won't even accept the image.

| Position | Content |
|---|---|
| `0x00` | a jump over the header (`jmp start`) — this is where the CPU lands after power-on |
| `0x04` | the four characters `TBBI` |
| `0x08` | length of the image in bytes |
| `0x0C` | checksum |
| `0x10` | **name, 32 bytes, terminated with a null byte** |
| `0x30` | code starts here |

```asm
.org ROM_BASE
reset:
    jmp start
    .db "TBBI"
    .dw 0                  ; length     -- build.py fills this in
    .dw 0                  ; checksum   -- build.py fills this in
    .db "MY BIOS", 0       ; 0x10 -- the name shown on the boot screen
    .space 22              ;         pad to exactly 32 bytes
start:                     ; 0x30
    li sp, BIOS_STACK
```

**The name isn't decoration.** The mainboard reads it and writes it to
the center of the screen at power-on — *before* the CPU is even running.
That's why the boot screen looks the same for every BIOS, and yet shows
its own name. If the field is missing (older images), the board shows
`UNNAMED BIOS`.

**`build.py` fills in the length and checksum**, not the assembler —
both depend on the finished image. The calculation lives in
`bios_kopf_stempeln`: round the length up to four, set the checksum
field to zero, then

```
sum = 0x1234
for each 32-bit word:  sum = sum * 31 + word
```

Checked **at three points, at three different times**:

1. **When flashing** — the firmware rejects an image with a missing tag
   or wrong checksum and doesn't write it at all (`bios_pruefen` in
   `firmware/setup.asm`)
2. **At power-on** — the mainboard rechecks and otherwise falls back to
   the backup (`Machine.rom_pruefen` in `hardware/machine.py`)
3. **Secure Boot**, if enabled — then the remembered checksum also has
   to match, see [[13 BIOS-Dienste und was fehlt]]

Point 2 is the important one: **broken firmware can't check itself.**
That's why this check sits in the board.

---

## What Happens at Power-On

The CPU starts at `ROM_BASE` = `0x0F000000`. From there it's all our
business:

1. **Set the stack** (`BIOS_STACK` = `0x0007FFF0`)
2. **Clear the BIOS data area** (from `0x400`) — that's where cursor
   position, color, tick counter, and the keyboard buffer live
3. **Fill the interrupt table** (from address 0, 4 bytes per vector).
   Without these entries, every interrupt jumps to address 0
4. **Start the timer**: `out P_TIMER_HZ, 100`
5. **Enable interrupts** (`sti`)
6. **Load sector 0 to `0x7C00`**, check the `55 AA` signature at offset
   510, and jump into it

What the BIOS **no longer** has to do itself: the boot screen and the
grace period. Both belong to the board (see below). A BIOS that wants to
evaluate the DEL key will find it in the keyboard buffer normally after
startup.

Everything else — boot screen, memory test, setup, Secure Boot — is
optional. The machine runs without it too.

---

## The Interrupt Vectors

| Vector | For | Required? |
|---|---|---|
| `0x08` | Timer IRQ | yes — otherwise no clock counts and `hlt` never wakes up |
| `0x09` | Keyboard IRQ | yes |
| `0x10` | Screen service | yes |
| `0x13` | Disk service | yes |
| `0x16` | Keyboard service | yes |
| `0x1A` | Time service | yes |
| `0x00` | Division by zero | no (the machine will just crash badly) |
| `0x06` | Unknown instruction | no |

Both hardware interrupts must acknowledge the controller
(`out P_PIC_ACK, …`), or none ever comes again.

**And the keyboard handler must drain ALL waiting keys**, not just
one — the controller only tracks one bit per source. Whoever stops
after the first key permanently lags one keystroke behind during fast
typing. That was a real bug, see [[07 Fallstricke]].

---

## The Four Services

Called with `int <number>`, function number in `r0`, arguments from
`r1`, result in `r0`. The counterpart on the system side lives in
`system/start.asm` — that's where you can see every call.

### INT 0x10 — Screen

The **order is mandatory**, the system calls by number.

| r0 | Name | Arguments |
|---|---|---|
| 0 | putc | r1 character, r2 attribute |
| 1 | puts | r1 pointer to 0-terminated text, r2 attribute |
| 2 | setcursor | r1 x, r2 y |
| 3 | clear | r1 attribute |
| 4 | getcursor | — → r0 = `y<<16 \| x` |
| 5 | putat | r1 x, r2 y, r3 character, r4 attribute |
| 6 | putn | r1 number, r2 attribute (decimal, unsigned) |
| 7 | puthex | r1 value, r2 attribute, **r3 digit count** |
| 8 | setmode | r1 = 0 text, 1 graphics |
| 9 | box | r1 x, r2 y, r3 width, r4 height, r5 attribute |
| 10 | fillrect | r1 x, r2 y, r3 width, r4 height, r5 attribute |
| 11 | hline | r1 x, r2 y, r3 length, r4 character, r5 attribute |
| 12 | scroll | — (everything up one line) |
| 13 | clearrow | r1 y, r2 attribute |
| 14 | putsat | r1 x, r2 y, r3 text, r4 attribute |
| 15 | sbcount | — → r0 = lines in the screen history |
| 16 | sbline | r1 line number, r2 target address |

**15 and 16 may be left out** — then they simply return 0, and the
terminal's history is empty. That's what `minimal.asm` does. What you
must *not* leave out is the table entry itself: if it's missing, the
system jumps into nowhere.

**putc must recognize control characters.** This is the requirement
that's easiest to overlook when copying it — it isn't listed under any
function number:

| Code | | what putc must do |
|---|---|---|
| 10 | `\n` | line break, scroll at the bottom edge |
| **8** | **Backspace** | **move cursor back one and delete the character there** |
| 13 | `\r` | cursor to start of line |
| 9 | Tab | to the next multiple of 8 |

If the **8** is missing, the BIOS drops the byte 8 into the screen
buffer as an ordinary character — and CP437 renders 8 as "◘". The input
line then collects a little box on every backspace press instead of
deleting. What makes this confusing: the text *in memory* is correct
regardless, only the screen lies. This is exactly what happened to Colin
with his first own BIOS.

The system sends the 8 to two places: `readline` in `system/lib.c` and
the input routine of the Python interpreter in `programs/py.c`.

`r3 digit count` in **puthex** is a real trap. Forget it, and it prints
the value hundreds of times and fills the entire screen — exactly what
happened while building the firmware tab.

### INT 0x13 — Disk

| r0 | Name | Arguments | Result |
|---|---|---|---|
| 0 | read | r1 sector (LBA), r2 count, r3 target address | r0 = status, 0 = good |
| 1 | write | same | r0 = status |
| 2 | size | — | r0 = sectors |

### INT 0x16 — Keyboard

| r0 | Name | Result |
|---|---|---|
| 0 | wait | r0 = `Scancode<<8 \| ASCII` |
| 1 | peek | 0 or the code — the key stays in the buffer |
| 2 | flush buffer | — |

A **`hlt`** belongs in the wait loop. Without it, the CPU spins idle at
full load, gets hot, and throttles itself — this too has already
happened, see [[10 Temperatur]].

### INT 0x1A — Time

| r0 | Name | Result |
|---|---|---|
| 0 | ticks since start | r0 (100 per second) |
| 1 | time of day | `h<<16 \| m<<8 \| s` |
| 2 | date | `y<<16 \| m<<8 \| d` |

---

## What the BIOS Does **Not** Have to Do

Almost everything. The system talks to the hardware itself for the most
part — graphics, blitter, mouse, speaker, block copier, and temperature
all go over `inr`/`outr` directly to the ports, with no BIOS involved.
The port list is in [[02 Speicherkarte und Ports]].

That's why a working BIOS is so small here: **3324 bytes** versus 12216
for the full one.

---

## Writing a BIOS on the Device Itself

Since the assembler was extended, the TB-32 can **build its own
firmware** — verified: the result is byte-for-byte identical to the
Mac's, except for the length and checksum in the header, which the Coder
fills in itself.

**Coder → New → BIOS** creates a source file with a ready-made template.
The `?` button in the top right opens the short version of this page on
the device.

Two buttons at the bottom:

| | |
|---|---|
| **Test** | builds, checks, asks for confirmation — and boots the machine **once** with it. The chip stays as it is; the next restart brings back the normal BIOS. The boot screen shows `TEST IMAGE -- runs once`. |
| **Flash** | the same, but permanently. After confirming in the Coder, the machine restarts, and the **firmware** asks a second time, in red, before anything is written. |

The second confirmation is deliberately posed by the BIOS, not the
Coder: a program must not be allowed to decide alone that the chip gets
overwritten.

**What the on-device assembler had to learn for this:** `.org`, `.equ`,
`.include`, expressions with operator precedence and parentheses,
`ldwa`/`stwa`, 512 instead of 256 symbols — and **local labels**
(`.loop`, `.done`) now belong to the most recently named global label.
Without that, `.copy` was the same label everywhere, and jumps landed in
a different function.

## Flashing from a File

```
python3 build.py                          # -> firmware/minimal.bin
```

Then in the TB-32: **DEL** at startup → **Firmware** tab → *Flash BIOS
from File* → pick the `.bin` in the Mac dialog → confirm with ENTER.

The tab also shows the size and checksum of the chip that's **currently
running** — that's how you can immediately tell whether your own BIOS is
really in there.

What gets burned is the chip file, **not the running chip**: the new
BIOS takes effect starting with the next power-on. Overwriting the
memory the CPU is currently fetching its instructions from would crash
it on the spot; real flash programs copy themselves into RAM first for
that reason.

### If It Goes Wrong

Three safety nets, in this order:

1. An image without `TBBI` or with a wrong checksum is **never written
   in the first place**.
2. Before every burn, the old image moves to
   `firmware/bios.backup.bin`. If the chip contains garbage at power-on,
   the board restores the backup **automatically** (Dual BIOS) and
   reports it in the terminal.
3. An image that passes the check and still hangs is recovered via
   *Restore Backup BIOS* in the same tab — or via `python3 build.py`,
   which restores the factory state.

Net 2 only kicks in for a **corrupted** image. A formally valid BIOS
that simply doesn't work won't boot the machine — that's what net 3 is
for.

---

## The Hardware Behind It

Three ports, see `hardware/devices.py`, class `Flash`:

| Port | Direction | Meaning |
|---|---|---|
| `0xB0` | write | 1 fetch file from host, 2 buffer into RAM, 3 burn, 4 restore backup |
| `0xB0` | read | result of the last command, 0 = good |
| `0xB1` | read | bytes in the buffer, 0 = no file |
| `0xB2` | write | target address for command 2 |

Command 1 opens the **Mac's file dialog** (`pc.py`,
`bios_datei_waehlen`). That's the equivalent of a USB stick during a
real board's BIOS flashback: the file comes from outside, not from the
running system.

The chip checks **nothing**. It accepts every byte it's given — exactly
like a real flash chip. Whether an image is any good is up to the
firmware to decide.

---

Related: [[13 BIOS-Dienste und was fehlt]], [[02 Speicherkarte und Ports]],
[[06 Bauen und Testen]], [[07 Fallstricke]]

---

## The Boot Screen Belongs to the Board

At power-on there are five seconds during which the CPU has no power
yet (`EINSCHALT_HALT_S` in `pc.py`):

| Time | What happens |
|---|---|
| 0.0–1.2 s | Blue fills the screen from top to bottom |
| from 1.5 s | the **name from the BIOS header** appears in the center |
| from 2.0 s | below it, `Press DEL to enter SETUP` |
| 5.0 s | power reaches the board, the BIOS starts |

This is deliberately in the housing, not the firmware. A boot screen in
the BIOS would be gone exactly when someone flashes their own — and then
there would also be no place left to press DEL.

**Keys pressed during this time aren't lost.** They're buffered and
handed to the machine as soon as the CPU is running — only then, because
a keyboard interrupt fizzles out while the CPU is stopped.

**But:** the board can buy time, not conjure up a menu. What happens on
DEL is up to the firmware. A BIOS without a setup does nothing.
