# TOOBAD TB-32

> **Beta.** The project is a work in progress and is published with no
> warranty whatsoever. There's no guarantee it will run for you, and no
> liability for anything.

A complete virtual PC. The core principle: **Python emulates only the
chips.** BIOS, operating system, interface, and all programs are real
TB-32 machine code, written in a custom assembly language and a custom
C dialect.

```bash
python3 build.py     # build BIOS, kernel, programs, and drive
python3 pc.py        # power on the machine
```

On the **first boot**, the machine sets itself up: username, password,
repeat password. After that you sign in with that password on every
boot. An empty password means: the machine is open and doesn't ask. The
account lives as a hidden file `\USER.DAT` on the virtual drive -- delete
it via *Start ▸ Settings ▸ Reset this machine* or, on the Mac, with
`python3 reset.py`.

## Networking

The TB-32 has a network card and its own network stack — ARP, IP,
ICMP, UDP, DNS, and TCP are all **TB-32 code**, not Python.

**`python3 pc.py` is all you need.** The router and proxy start
alongside it — in their own threads within the same process. Start a
second window and it notices both are already running and won't start
them a second time.

```bash
python3 pc.py                 # network runs alongside
python3 pc.py --kein-netz     # without router and proxy
```

Then `HOST example.com` (resolves the address) and `FETCH example.com`
(actually fetches the page — DNS, TCP connection, HTTP request,
response) both work. Two running machines also see each other
immediately: `NET` shows the machine's own address, `PING 10.0.0.1`
reaches the other one.

The router and proxy can also run standalone if you want to watch them
work (`python3 router.py`, `python3 proxy.py`) — then use `pc.py
--kein-netz`.

And on the desktop there's a **browser**: *Start ▸ Browser*, type an
address, ENTER. Headings, paragraphs, lists, and clickable links. No
CSS, no JavaScript, no images.

The proxy handles **HTTPS** — the TB-32 itself still only speaks HTTP,
and a separate program next to it handles encryption.

The proxy is preconfigured (`127.0.0.1:8080`), so `https://example.com`
works right away. If nothing answers there, the browser falls back to
the direct route — then everything except HTTPS works. Turn it off with
`NET PROXY OFF`.

Commands: `NET`, `NET IP`, `NET GW`, `NET DNS`, `NET ARP`, `NET SEND`,
`NET WATCH`, `PING`, `HOST`, `FETCH`.

## Where programs live

The Start menu shows what's **there**: the built-in windows at the top,
followed by everything from `\SYSTEM\PROGS` (the system's programs) and
`\PROGS` (your own). Seven at a time, the rest via the arrows. Drop a
`.TBX` into either folder — it shows up in the menu the next time it's
opened.

Anything in `\SYSTEM` is protected from deletion: `DEL` refuses and
points you to `SUDO`. That asks for your password once and then leaves
you alone for five minutes. A machine without a password is open — it
doesn't ask there.

## Custom windows for your own programs

A program from the drive can have a window on the desktop — it doesn't
need to be part of the kernel for that. `FENSTER.TBX` shows how:

```
WIN                          (desktop)
START FENSTER.TBX /B         in the Command Prompt
```

The Blitter draws into the program's **own frame buffer** (ports
0x5B–0x5D), and the desktop composites the windows from that. The
program picks up events (keys, clicks, close) via syscalls 40–44; the
library for that lives in `programs/gfxlib.c`.

## Case keys

These keys belong to the window, not the virtual machine — so they work
everywhere, even in the BIOS, where no operating system is running yet.

| Key | Effect |
|---|---|
| `ü` | **Power button**, when the machine is off. Followed by a five-second grace period with the splash screen |
| `DEL` (on the Mac `fn`+`⌫`) or `F2` | into BIOS Setup — can be pressed even during the grace period |
| **`Strg`+`K`** / `Cmd`+`K` | **copy everything, with no feedback.** In text mode, the whole screen (including in the BIOS and in Setup); in graphics mode, the *full* text of the Coder — not just the visible portion |
| `Strg`+`V` / `Cmd`+`V` | paste text from the host machine |
| `Cmd`+`C` | copy the selection from TOOBAD-OS back to the host machine |
| `Strg`+`R` | Reset — the button on the case |
| `F11` / `F12` | fullscreen / overlay with clock speed, temperature, and frame rate |
| `Strg`+`Q` | quit |

## What's inside

| | |
|---|---|
| **CPU** | TB-32 — 32-bit, 16 registers, fixed 4-byte instructions, 57 opcodes |
| **BIOS** | custom firmware with Setup, Secure Boot, and a swappable chip |
| **OS** | TOOBAD-OS with a filesystem, multitasking, windows, Paint, Word, Coder |
| **Tools** | assembler and C compiler — **each one built once for the Mac and once for the TB-32 itself** |
| **Emulator** | once in Python (reference), once in C (~150× faster) |

The machine builds its own compiler **and its own firmware** — the
result is byte-for-byte identical to what the Mac tool produces.

## What it runs on

| | |
|---|---|
| **macOS** | developed and tested here |
| **Linux** | tested — the build, all four test tools, and the desktop all run through. Two things are tied to macOS, though, and simply do nothing here: the clipboard to the host machine (`pbcopy`/`pbpaste`) and the file dialog for flashing the BIOS (`osascript`) |
| **Windows** | **not tried.** It's just Python and pygame, so it should run; the same two macOS bindings are missing there as well |
| **Raspberry Pi without Linux** | **doesn't work yet.** The C emulator is the path there and has been proven to compute identically, but the splash screen, keyboard, and display output are still stuck in `pc.py` to this day. See `Doku/15 Weg zum Raspberry Pi` |

You need Python 3 and `pygame`:

```bash
pip3 install -r requirements.txt
```

## What this actually is

A **CPU emulator**, a **custom instruction set**, an **assembler**, a
**C compiler**, a **BIOS**, and an **operating system with windows** —
all built from scratch, nothing borrowed. If you're into *osdev*,
retro computing, compiler construction, or simply curious how a
computer works under the hood, every layer is here, separate and
readable.

The machine builds its own compiler and its own firmware — that's
called bootstrapping, and it's the real test of correctness.

## What's on it

**On the command line** (`HELP` shows the same list within the system):

| | |
|---|---|
| Files | `DIR` `CD` `MD` `RD` `COPY` `REN` `DEL` `TYPE` `MORE` `FC` `DUMP` |
| Drive | `FORMAT` `CHKDSK` `VOL` |
| System | `VER` `MEM` `SYSTEMINFO` `TEMP` `DATE` `TIME` `CLS` `COLOR` `ECHO` |
| Processes | `START` `TASKLIST` `TASKKILL` |
| Interface | `WIN` starts the desktop |
| Exit | `SHUTDOWN` `REBOOT` `EXIT` |

**Programs on the drive:**

| | |
|---|---|
| `CC` | C compiler — **runs on the TB-32 itself and compiles itself** |
| `ASM` | assembler, also on the device. Can also build a BIOS |
| `PY` | small Python interpreter |
| `CALC` | calculator |
| `FLAPPY` | game, shows off the graphics performance |
| `BENCH` `MEMTEST` `KELLERTEST` `CRASH` | measuring and breaking things, for testing |

**On the desktop** (Start menu):

| | |
|---|---|
| **File Manager** | view, rename, delete files, drag with the mouse |
| **Command Prompt** | the command line in a window |
| **Coder** | editor with syntax highlighting, search, compiling, running — and **building firmware** (`Test` / `Flash`) |
| **Paint** | drawing with tools, fill, undo, its own format `.TBI` |
| **Word** | word processing with selection, colors, lists, pages, and **embedded images** |
| **System Monitor** | processes, memory, clock speed, temperature |
| **Control Panel** | CPU clock, fan, POST settings — writes to CMOS |
| **Clock** | clock and uptime |

## Docs

**For AI assistants:** [`AI_README.md`](AI_README.md) — architecture,
every command, every interface, and the pitfalls, all in one place.

The working reference lives in [`Doku/`](Doku/) as an Obsidian vault.
Start at `00 START HIER`. Especially worth reading: `07 Fallstricke`
(hard-won lessons) and `16 Eigenes BIOS schreiben`.

## Tests

```bash
python3 tools/selftest.py       # 62 checks from power-on to the desktop
python3 tools/ctest.py          # language tests for the compiler
python3 tools/bootstrap.py      # the compiler compiles itself
python3 tools/emu_vergleich.py  # C vs. Python, instruction by instruction
```

The last one builds the C version of the emulator and therefore needs
`make` and a C compiler; the other three need only Python and pygame.

## License

MIT — see [LICENSE](LICENSE). Do whatever you want with it.
