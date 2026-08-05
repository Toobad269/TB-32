# BIOS Services and What's Missing

Everything in `firmware/bios.asm`. Calling convention always the same:
**function number in `r0`, arguments in `r1`–`r5`, result in `r0`** — exactly
like on a real PC, except it uses `r0` instead of `ah`.

## What the BIOS can do today

| Interrupt | Functions | |
|---|---|---|
| `INT 0x10` screen | 17 | 0 putc, 1 puts, 2 setcursor, 3 cls, 4 getcursor, 5 putat, 6 putn, 7 puthex, 8 setmode, 9 box, 10 fillrect, 11 hline, 12 scroll, 13 clearrow, 14 putsat, 15 sbcount, 16 sbline |
| `INT 0x13` disk | 3 | 0 read, 1 write, 2 size in sectors |
| `INT 0x16` keyboard | 3 | 0 wait for key, 1 peek, 2 flush buffer |
| `INT 0x1A` time | 3 | 0 ticks since start, 1 time of day, 2 date |
| `IRQ 0x08` | — | Timer, counts up `BDA_TICKS` |
| `IRQ 0x09` | — | Keyboard, fills the ring buffer in the BDA |
| `INT 0x00` / `INT 0x06` | — | Division by zero and invalid instruction → panic screen |

Plus: POST, boot process, panic screen — and the **setup with four
tabs** (see below).

## The Setup

With `DEL` or `F2` at startup. **Left/Right switches the tab**,
Up/Down the line, ENTER or `+`/`-` changes the value, `F10` saves,
`F5` loads default values, `ESC` discards.

| Tab | Content |
|---|---|
| **Main** | Time, date, Quick Boot, POST beeper, POST messages, load defaults |
| **Hardware** | Clock speed, boot device, memory size, disk, graphics card (the last three display-only) |
| **Cooling** | Fan control, throttling limit, plus temperature, fan, throttling and peak value **live from the chipset** |
| **Security** | Secure Boot, checksum, "Trust Current Boot Image" |

**Setting time and date:** ENTER on the line opens a small field editor —
Up/Down changes the value, Left/Right switches between
hour/minute/second or day/month/year, ENTER ends it. Which field is
active is shown in the help box below.

For this to be possible at all, the **clock chip needed a time of its
own**: previously `CMOS._refresh_clock()` simply read the Mac's clock,
and every write attempt was gone again on the next read. Now the CMOS
remembers an **offset in seconds** (four bytes starting at register
`0x30`), and writing to a clock register recalculates the offset —
exactly like turning a knob on a real RTC chip.

## A BIOS of Your Own

The BIOS is replaceable: **DEL → Firmware → Flash BIOS from File** takes
a `.bin` from the Mac and burns it into the chip. What a BIOS has to
provide for that — header, interrupt vectors, all four services with
their registers — is fully documented in [[16 Eigenes BIOS schreiben]].
Ready-made template: `firmware/minimal.asm`, 3324 bytes.

## Secure Boot

The idea is the same as on a real PC: before starting, it's recomputed
whether the boot image is still the known one. Only here the computation
is a **checksum over the boot sector, kernel, and the first 16 KB of the
ROM** — no signature with a key. The principle "check first, then boot"
is the same, the tamper-resistance is not.

The firmware looks for the kernel **as a file**, `\SYSTEM\KERNEL.BIN`,
via `kernel_finden` in `firmware/setup.asm` — the same search the boot
sector also does. It has to measure what actually starts, otherwise the
check would be a sham; see [[07 Fallstricke]].

- The remembered checksum lives in four CMOS slots (`CM_SUM0`–`CM_SUM3`),
  so in the button cell
- If it doesn't match, **the computer won't boot** — a red screen appears
  with the note that `DEL` leads into setup
- There, *Trust Current Boot Image* remembers the current checksum

**Important:** Secure Boot is **off** by default, and for good reason —
every `python3 build.py` changes the kernel or BIOS and invalidates the
remembered checksum. Whoever turns it on has to go into setup and
re-trust once after every rebuild. That's exactly the point.

## What's Missing — And Whether It's Worth It

### Worth it, little work

**Memory size as a service** (on the PC `INT 0x12`). The value sits in
the BDA at `0x4A0`, and the OS **reads it directly from memory**
(`syscall.c`, function 22). That's a shortcut past the firmware: a
program shouldn't need to know where the BIOS keeps its notes. The same
applies to the disk size at `0x4A4`.

**Equipment list** (on the PC `INT 0x11`). A bitmask: is there a mouse?
A speaker? How many disks? Today the OS has to poll the ports one by one
and guess.

**Wait** (on the PC `INT 0x15`, function `86h`). The `delay` routine has
long existed in the BIOS — it's just not exposed. The OS builds its own
wait loop in `lib.c`.

**Beeper.** The speaker hangs off ports `0x50`/`0x51`, and the OS writes
directly to them. A BIOS service for this would be the clean layer.

### Worth it, more work

**Mouse.** The TB-32 *has* a mouse with a hardware cursor (ports
`0x60`–`0x63`, `IRQ 12`), but **the BIOS doesn't know about it at all.**
The desktop talks to the ports directly. On a real PC a driver does this
via `INT 0x33` — here a BIOS service would be the natural place.

**Font address.** A program in graphics mode needs it for the blitter.
This is currently solved via **kernel system call 30** — which returns
the address of a table in the *kernel*. Cleaner would be a font in ROM
and a BIOS service for it (on the PC: `INT 0x10`, `AX=1130h`). Then a
program could draw even without TOOBAD-OS.

**Reading characters off the screen** (on the PC `INT 0x10`, function
`08h`). We can write, but not read back. For things like "what's
actually there," the OS has to keep its own copy of the screen buffer.

**Key state** (on the PC `INT 0x16`, function `02h`): is Shift, Ctrl or
Alt currently pressed? In `hardware/devices.py` there are already even
the fields `self.ctrl` and `self.alt` for this — **but they're never set
and never read.** Dead connections: the hardware would need to fill them
on keypress and report them via the status port, then the BIOS could
pass them along.

**Reboot** (on the PC `INT 0x19`). There's the port command `P_POWER`,
but no "reload the boot sector" service.

### Deliberately unnecessary

| Missing | Why it makes no sense here |
|---|---|
| Printer (`INT 0x17`) | There's no printer |
| Serial port (`INT 0x14`) | No hardware — but would be the natural starting point for a networking chapter, see [[11 Offene Punkte]] |
| ROM BASIC (`INT 0x18`) | Pure 80s legacy |
| Cylinder/head/sector | The TB-32 speaks in **LBA** — i.e. sequentially numbered sectors — from the start, so the old-drive conversion math is gone |
| Extended memory map (`E820`) | The layout is fixed, see [[02 Speicherkarte und Ports]] |

## Things to Keep in Mind When Adding a Service

A new service is **three** changes, not one:

1. Entry in the jump table in `firmware/bios.asm` (and the vector in
   `bios_init`, if it's a new interrupt)
2. Constant in `firmware/const.inc`
3. If the OS is meant to use it: bridge in `system/start.asm` and
   wrapper in `system/lib.c` — and for programs additionally a
   syscall number, see [[05 Konventionen]]

And: the kernel must **not grow larger than `0xB0000`**, or it eats into
the file system's buffers — the trap is described in [[07 Fallstricke]].

Related: [[05 Konventionen]], [[02 Speicherkarte und Ports]], [[11 Offene Punkte]]
