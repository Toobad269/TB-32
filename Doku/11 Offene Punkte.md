# Open Items

## Next Up (as of August 2026, after Colin's vacation)

1. **Window for the C emulator** (SDL2 instead of pygame) — step 1b in
   [[15 Weg zum Raspberry Pi]]
2. **Pi with Linux**: install over SSH, start in fullscreen. No risk to
   Colin's server.
3. **Bare metal on the Pi 5** — via Circle, because writing USB support from
   scratch is the real wall. Details and pitfalls: [[15 Weg zum Raspberry Pi]]

Explicitly **not** wanted: Pac-Man (Colin turned it down), GPIO LEDs.

## Ideas That Came Up in Conversation

- **Network card + simple browser.** HTTP is a text protocol, simple HTML
  can be rendered. HTTPS, JavaScript, and CSS cannot.
- **Realistic memory.** Right now every access completes instantly — no
  cache, no wait states. Would be its own piece of hardware to model.
- **Proportional font for Word.** Would need a second, hand-drawn character
  set with a width table.
- **FPGA** — build the TB-32 as an actual chip.

## Low-Hanging Fruit

- **Kernel size**: ~250 KB. There are no more fixed kernel sectors — the
  kernel lives as a file in the filesystem, and the limit is contiguous free
  space. The superblock sits at sector **512**; whoever shifts the layout
  must change `fs.c`, `tools/tbfs.py`, `system/boot.asm`, **and**
  `firmware/setup.asm` together.
- **Editor**: no search function, no undo.
- **Selection** only works with the mouse, not with Shift+arrow keys.
- **Clipboard** only works in the editor; the terminal window and File
  Manager still can't paste anything.
- **Desktop icons** don't rearrange themselves when one is deleted — a gap
  stays behind until you close it up by hand.

- **BIOS services**: memory size, feature list, wait, beeper, and above all
  the **mouse** are missing as services — the OS bypasses the firmware for
  these and accesses ports and the BIOS data area directly. Full list in
  [[13 BIOS-Dienste und was fehlt]]

## ~~`#include` only finds the current folder~~ — done

The search path is in use: the **main file** is looked up in the current
folder, **included files** additionally in `\SOURCE` (`fs_read_lib` in
`fs.c`, **syscall 33**, `fileread_lib` in `proglib.c`).

The fact that the call previously always returned −1 wasn't its own fault:
an `#include` inside a comment had caused the preprocessor to delete the
line with the `*/` — the now-open comment ate the `if (fn == 33)`. Both
preprocessors now check for this. See [[07 Fallstricke]].

## ~~Graphics is bottlenecked by system calls~~ — done

Programs now talk to the blitter **directly** via the ports, without
`int 0x40`. Ports aren't protected on the TB-32 — a program is allowed to do
that. The blitter in the emulator was also sped up along the way (drawing
6.5×, port access 5.8×). Measured on Flappy: **9 → 53 fps**. Details in the
[[14 Aenderungsjournal]].

What remains open here, in case it needs to get even faster:

1. **Multiple draw commands in a list** in memory, which the blitter works
   through in one go — also saves the remaining port accesses
2. `syscall_asm` saves 15 registers and dispatches through an `if` chain.
   A jump table would help every *other* system call

## Speed (measured, see [[01 Architektur TB-32]])

The emulation manages ~3.1 million instructions/s; for the configured
8 MHz, that's a factor of 2.6 short. Colin deliberately decided against a C
core. What could still be done within Python:

1. **Newer Python version** — the system Python is 3.9.6; since 3.11 there's
   the specializing interpreter. 1.4–1.8× with zero code changes
2. **Generate fewer instructions**: `tcc.py` routes everything through `r0`
   and constantly pushes to the stack — hence the 40% `push`/`pop`. A simple
   register allocator would act like a faster emulator
3. **Mini-JIT**: translate basic blocks into Python functions once and cache
   them. 3–8×, but self-modifying code (our own compiler!) needs
   invalidation

## Bigger

- **Network card**: let two running instances talk to each other. Would be
  a visible new chapter (ports + driver + simple protocol).
- **Sound chip** with multiple voices instead of just a square wave.
- **Memory protection**: currently any program can overwrite anything. A
  simple range check in the CPU would be doable and educational.
- **Python interpreter**: no dictionaries, no classes, no string methods
  besides `len`/`+`.

## Deliberately Not Done

- Porting real CPython — impossible, see README.
- Colored syntax highlighting in the editor — costs a lot of draw time for
  little gain.
- Free-wrapping text in the terminal window: the shell's buffer stays at
  70×22, and the window only shows a cropped view when shrunk.

Related: [[00 START HIER]]
