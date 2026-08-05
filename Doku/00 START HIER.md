# TOOBAD TB-32 — Getting Started

Working reference for Claude. On context loss **read this page first**, then
go straight to the linked detail page. Everything here reflects the current
state of the running system, not wishful thinking — verified via
`tools/selftest.py` (45/45 green).

## Working rule

After every completed change: **first state the start command, then
immediately update the docs** — and log every bug found, every change, and
every new feature in [[14 Aenderungsjournal]], with **root cause**, not just
symptom. This is Colin's explicit wish; docs added later don't count.

## What the project is

A complete virtual PC in `~/Desktop/Projekte/PyPC/`. Colin's challenge
against other AIs. **Core principle: Python only emulates the chips.**
Everything visible — BIOS, boot process, OS, editor, desktop — is real
machine code running on the emulated CPU. Anyone who waters that down
destroys the point of the project.

The compiler **compiles itself** (bootstrapping proven, see
[[09 Selbst-Compilierung]]).

Since August 2026 the emulator also exists **in real C** (`emu/`) —
160 times faster and the basis for booting the machine on a Raspberry
Pi without Linux. The TB-32 remains the processor throughout; only what
rebuilds the chips gets swapped out. Plan and status:
[[15 Weg zum Raspberry Pi]].

**Applications on the desktop:** File Manager, Command Prompt, Editor
(= "Coder", with syntax highlighting), System Monitor, Control Panel,
**Paint**, **Word**, Clock, About.

## The three layers (never confuse them)

| Layer | Language | Runs on |
|---|---|---|
| `hardware/`, `pc.py`, `tools/` | Python | the Mac |
| `emu/` | **real C** | the Mac (later the Pi) |
| `firmware/*.asm`, `system/start.asm` | TB-32 assembly | the TB-32 |
| `system/*.c`, `programs/*.c` | TC (custom C variant) | the TB-32 |

**`emu/` is real C for the host machine, `system/*.c` is TC for the
TB-32.** Both are called "C" and have nothing to do with each other.
Confusing them costs hours.

`system/*.c` and `programs/*.c` look like C, but are compiled by
**`tools/tcc.py`** — its limits are documented in
[[04 Compiler TCC Grenzen]]. **This is the most common source of coding
errors.**

## Quick commands

```bash
cd ~/Desktop/Projekte/PyPC
python3 build.py            # BIOS + kernel + programs + disk
python3 pc.py               # start (window)
python3 tools/selftest.py   # 41 checks, ~2 min
python3 tools/ctest.py --selftest   # 11 compiler tests
python3 tools/bootstrap.py  # self-compilation, ~5 min
python3 tools/emu_vergleich.py      # C emulator vs. Python version
```

The C version of the emulator:

```bash
cd emu && make && cd ..
./emu/tb32 4.0 "dir"        # boot headless and type a command
```

Testing without a window (the workhorse):

```bash
python3 tools/headless.py 12 --keys "DIR,ENTER,TEMP,ENTER"
python3 tools/screenshot.py /tmp/x.png 10 --keys "WIN,ENTER" --mouse "6:25:387:click"
```

## Pages

- [[01 Architektur TB-32]] — registers, instruction set, encoding
- [[02 Speicherkarte und Ports]] — **all addresses**, avoid collisions
- [[03 Dateien und Zustaendigkeiten]] — who does what
- [[04 Compiler TCC Grenzen]] — **read before every coding session**
- [[05 Konventionen]] — registers, calls, syscalls
- [[06 Bauen und Testen]] — tools, GUI coordinates for click tests
- [[07 Fallstricke]] — hard-won lessons, don't repeat them
- [[08 Desktop Aufbau]] — windows, menu, button positions
- [[09 Selbst-Compilierung]] — bootstrap chain
- [[10 Temperatur]] — thermal model and throttling
- [[11 Offene Punkte]] — what's next
- [[12 Abkuerzungen und Namen]] — what TBX, TBFS, TC, CC … stand for
- [[13 BIOS-Dienste und was fehlt]] — service list, setup, secure boot
- [[14 Aenderungsjournal]] — **every change, every bug, every new feature**
- [[15 Weg zum Raspberry Pi]] — plan, decision, and status for real hardware

## Working with Colin

German. Before a fix, **first describe the understood cause** and
reproduce it — don't just dive in and repair. The system's user interface
is **English**, the source code comments are **German** — that's
intentional.
