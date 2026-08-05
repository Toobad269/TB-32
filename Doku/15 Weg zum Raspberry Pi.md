# Path to the Raspberry Pi

**Goal:** TOOBAD-OS runs on a real device — without Linux underneath, and
**without the TB-32 disappearing**.

This page holds the plan, the reasoning behind it, and the current
status. Status: step 1 done, everything else is waiting for Colin to
return from vacation.

---

## The Decision: Emulate, Don't Port

There were two paths, and the choice has been made.

| | **Emulator in C, bare metal** (chosen) | Port the OS to ARM |
|---|---|---|
| Write screen, timer, SD, USB ourselves | yes | yes |
| Compiler gets an ARM64 backend | — | yes |
| BIOS and `start.asm` rewritten in ARM assembly | — | yes, completely |
| Rebuild the blitter in software | — | yes, all seven commands |
| Every `portout` in `gui.c`, `paint.c`, `word.c` rewritten | — | yes |
| TOOBAD-OS needs to be changed | **not a single byte** | everywhere |

The hard part (bare-metal drivers, especially USB) is the same on
**both** paths. The porting path is the same mountain **plus** four more.

And the decisive point: with an ARM port, the **TB-32 would be gone** —
the custom instruction set, the assembler, **CC** (the compiler that runs
on the device itself and compiles itself), the bootstrapping. What would
remain is "an OS on someone else's hardware."

With the emulator path, everything is preserved: **the emulator then
becomes the mainboard.** It takes the real Pi hardware and presents the
TB-32 with its familiar ports. The OS side notices nothing.

---

## The Steps

### Step 1 — Emulator in C ✅ **done**

`emu/` alongside `hardware/`. See [[14 Aenderungsjournal]] and
[[06 Bauen und Testen]].

- `emu/cpu.c` — all 57 instructions
- `emu/machine.c` — bus, graphics card with blitter, disk, keyboard,
  timer, CMOS, block copier, heat
- `emu/main.c` — headless startup for comparison
- `tools/emu_vergleich.py` — checks C against Python, instruction by
  instruction

Measured: **1.8 → 287 million instructions per second**, a factor of
160. TOOBAD-OS boots, the screen matches character for character.

### Step 1b — Window for the C Version (open)

SDL2 instead of pygame, so the C emulator can be used like `pc.py`.
Keyboard, mouse, video output. After that, the Python version becomes
just a reference for comparison.

### Step A — Pi with Linux (open, no risk)

Push the project to the Pi over SSH, compile it there, run it fullscreen
on HDMI. Colin's Linux stays completely untouched (`rm -rf` and it's
gone).

**Why this intermediate stage isn't skipped:** otherwise you'd be
debugging two unknowns at once — the emulator on ARM *and* bare metal.
This way you first see whether the emulator computes correctly on ARM.

### Step B — Bare Metal (open)

The Pi boots `kernel8.img` directly: inside it is the TB-32 emulator as
native ARM code. No Linux.

---

## How a Pi Boots (no imager needed)

At power-on it's not the ARM CPU that runs first, but the graphics chip.
It reads the **first partition of the SD card (FAT32)** and looks for
fixed file names. If it finds `kernel8.img`, it loads the file and starts
the ARM cores into it. Done — no dedicated bootloader, no image format.

On a Pi 4 the card would look like this:

| File | Where from |
|---|---|
| `start4.elf`, `fixup4.dat` | official Pi firmware, unmodified |
| `config.txt` | we write it, three lines |
| **`kernel8.img`** | **our emulator** |

**The Pi 5 boots differently** (EEPROM bootloader, different firmware
files) — see below.

---

## The Process with Colin's Card

Colin has **one** SD card, on which Baronie, the Toobad server, and
SideEye run. He doesn't want to buy anything. So:

1. **We never touch the Linux partition.** macOS only mounts the FAT32
   boot partition anyway — it can't even get at ext4. That's the
   safeguard: no raw writing, no `sudo`, no risk.
2. **Backup = the boot partition** (a few hundred MB instead of 32 GB),
   pulled while the card is in the Mac. Not over SSH — that would take
   hours.
3. Our `kernel8.img` goes **alongside** the existing one, switched via a
   single line in `config.txt`. Back to Linux = revert that line. Keep a
   copy of `config.txt` beforehand.

**Limit:** writing to a raw card would need `sudo` — Claude can't enter a
password. Copying onto the mounted FAT partition works without it.

---

## The Pi 5 Is the Hardest Pi (as of August 2026)

Colin has a **Pi 5**. That happens to be the model with the thinnest
bare-metal support, and the reason is the **RP1**: USB, networking, and
GPIO no longer hang off the main chip, but off a separate chip behind
PCIe. For bare metal that means: bring up PCIe, address the RP1, an
xHCI driver, only then a USB keyboard.

Researched status from [Circle](https://circle-rpi.readthedocs.io/en/50.0/appendices/raspberry-pi-5.html)
(a bare-metal driver collection for the Pi, **not** an operating system):

- **Screen:** firmware support for the framebuffer is "less
  convenient" — no configuration via `config.txt`, resolution can't be
  set from the program
- **USB:** "should work," but there are reports of detection problems
  at startup — **especially when HDMI is connected**. Exactly our
  combination.
- **Network:** ~~not at all~~ **works by now.** Circle's README (as of
  08/04/2026) lists both "MACB / GEM Gigabit Ethernet NIC of
  Raspberry Pi 5" and "Wireless LAN access" as supported for the Pi 5.
  The Pi 5 appendix page says nothing about this and points to the
  README instead — so whoever looks here has to read the README, not
  the doc page. Tested according to Circle only with BCM2712 steppings
  C1 and D0.

### What This Means in Practice

- A **Pi 4** would bring step B from a month with an uncertain outcome
  down to about a week. Colin doesn't want to buy one — accepted.
- On the Pi 5 we're tackling it via **Circle**: that means we *don't*
  have to write USB ourselves, and that was the wall.
- If Circle turns out too shaky: our own drivers. But that needs a
  **USB-to-serial adapter (~€10)**. Without serial output, debugging
  bare metal is blind — as soon as the Pi boots without Linux, SSH is
  gone.

---

## What Does NOT Work (asked multiple times)

- **Booting from a USB stick on the Ryzen 7600X.** The Ryzen is x86-64,
  the TB-32 is a different CPU. Not a single byte of our code runs
  there. Loading our own BIOS "onto the mainboard" additionally fails
  on signed UEFI.
- **Claude Code or brew on the TB-32.** Node.js programs are for
  macOS/Linux on x86/ARM; they would need POSIX, TCP/IP, TLS, and a
  JavaScript runtime — and even then the instruction set wouldn't
  match.
- **Realistic RAM speed in the emulator.** Doesn't exist: every memory
  access completes instantly, no cache, no wait states. Only the CPU is
  throttled, via its instruction budget. Would be its own piece of
  hardware to rebuild.

## What Would Work, But Would Be Its Own Project

- **Networking and a simple browser.** Network card as a device, its
  own TCP/IP stack in TB-32 code, fetching over HTTP, rendering simple
  HTML. Not HTTPS, JavaScript, or CSS.
- **FPGA.** The TB-32 is a genuine processor design — on an FPGA board
  (€50–150) you could **really build** the chip. That's the honest path
  for a homebuilt CPU.

---

Related: [[14 Aenderungsjournal]], [[06 Bauen und Testen]],
[[02 Speicherkarte und Ports]]
