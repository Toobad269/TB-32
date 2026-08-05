# Files and Responsibilities

Who does what — so changes land in the right place.

## On the Mac (Python)

| File | Responsible for |
|---|---|
| `pc.py` | Window, keyboard, mouse, sound, **scrolling in the window**, F12 overlay, per-frame time budget, **window size/fullscreen**, **bridge to the macOS clipboard** |
| `hardware/isa.py` | **Instruction set, memory map, port numbers** — the single source of truth for both CPU *and* assembler |
| `hardware/cpu.py` | The CPU. Main loop keeps PC and flags in local variables (for speed) |
| `hardware/bus.py` | Address decoding, ROM write protection, port routing |
| `hardware/devices.py` | Graphics card incl. **blitter**, keyboard, disk, timer, CMOS, speaker, mouse, **thermal**, power supply |
| `hardware/machine.py` | Wires everything together, time slices, clock and throttling |
| `tools/assembler.py` | Assembler (two passes, labels, directives, pseudo-instructions) |
| `tools/tcc.py` | **C compiler on the Mac** — produces the kernel |
| `tools/mkfont.py` | 8×8 font from hand-drawn 5×7 patterns |
| `tools/tbfs.py` | Filesystem from the outside (including folders) |
| `tools/opstat.py` | measures instruction frequency — basis for the execution chain's ordering |
| `build.py` | Builds everything; writes **only** sector 0 raw, the kernel comes as the file `\SYSTEM\KERNEL.BIN` |

## Firmware (TB-32 assembly)

| File | Contents |
|---|---|
| `firmware/const.inc` | Constants for all assembler files |
| `firmware/bios.asm` | Reset, interrupt vectors, POST, boot process, BIOS services, panic screen |
| `firmware/video.asm` | Screen routines, **scrollback ring buffer** |
| `firmware/setup.asm` | BIOS setup: four tabs, field editor for the clock, secure boot |
| `system/boot.asm` | Boot sector, 512 bytes — **reads TBFS** and loads `\SYSTEM\KERNEL.BIN` |
| `firmware/minimal.asm` | The smallest BIOS that boots the machine (3324 bytes) — template for a custom one, see [[16 Eigenes BIOS schreiben]] |
| `system/start.asm` | Kernel entry point, **bridge C → BIOS**, process switcher, syscall entry |

## Operating system (TC)

| File | Contents |
|---|---|
| `system/kernel.c` | Command interpreter, all shell commands, `main()` |
| `system/lib.c` | Output (**soft switch text/terminal window**), strings, input, screen lock, scrollback view |
| `system/fs.c` | TBFS: superblock, directory, **folders**, search path, **move** |
| `system/edit.c` | Text editor in text mode — the GUI editor also uses this editing logic |
| `system/proc.c` | Processes, scheduler half in C, `mt_enable` |
| `system/syscall.c` | Counterpart of `INT 0x40`, program loader, progress reporting |
| `system/term.c` | Framebuffer and keyboard of the **terminal window** |
| `system/diag.c` | Display test |
| `emu/cpu.c` | TB-32 processor in real C — all 57 instructions |
| `emu/machine.c` | Bus and devices in C: graphics, blitter, disk, timer, DMA |
| `emu/main.c` | headless startup of the C version for comparison |
| `tools/emu_vergleich.py` | checks C against Python, instruction by instruction |
| `system/dialog.c` | File-picker window, used by Coder, Paint, and Word |
| `system/word.c` | Word processor with paragraphs, formatting, and word wrap |
| `system/coder.c` | Syntax highlighting, line numbers, search, indentation for the editor |
| `system/paint.c` | Drawing program as a desktop window |
| `system/gui.c` | **Desktop**: windows, start menu, all applications |
| `system/font8.c` | generated font — don't edit by hand |

## Programs for the TB-32 (TC)

| File | Becomes | Contents |
|---|---|---|
| `programs/proglib.c` | — | Library for programs (syscall wrappers) |
| `programs/gfxlib.c` | — | **Graphics for programs**: blitter, text, large text, buttons, mouse |
| `programs/prog_start.asm` | — | Startup code for every program |
| `programs/cc.c` | `\SYSTEM\CC.TBX` | **C compiler that compiles itself** |
| `programs/asm.c` | `\SYSTEM\ASM.TBX` | Assembler |
| `programs/py.c` | `\SYSTEM\PY.TBX` | Python interpreter |
| `programs/memtest.c` | `\PROGS\MEMTEST.TBX` | Memory test |
| `programs/bench.c` | `\PROGS\BENCH.TBX` | Performance benchmark |
| `programs/calc.c` | `\PROGS\CALC.TBX` | **Calculator**, graphical, fixed-point |
| `programs/flappy.c` | `\PROGS\FLAPPY.TBX` | **Flappy Bird**, physics in sixteenths, with frame-rate display |
| `programs/crash.c` | only `\SOURCE\CRASH.C` | **Stress test and fault injector** — burn-in until throttling kicks in, color chaos, flicker, plus five real crashes. **Deliberately not** built by default: Colin compiles it on the TB-32 himself (`NUR_QUELLTEXT` in `build.py`) |

## Miscellaneous

- `diskfiles/` — mirrored 1:1 onto the virtual disk during the build
- `disk/hd0.img` — the virtual hard disk (files survive reboots)
- `disk/cmos.bin` — the coin cell; deleting it resets the BIOS setup
- `README.md` — docs for Colin (narrative)
- `Doku/` — this vault (working reference)

Related: [[00 START HIER]], [[02 Speicherkarte und Ports]]
