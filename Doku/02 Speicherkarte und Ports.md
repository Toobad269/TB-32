# Memory Map and Ports

**The most important page.** Almost every hard-to-find bug in the project
came from overlapping memory regions. Check here before adding any new
buffer.

Source of truth: `hardware/isa.py`, `firmware/const.inc`, the `#define`s
in `system/*.c` and `programs/*.c`.

## Address space

| Range | What | Owner |
|---|---|---|
| `0x00000000`–`0x000003FF` | Interrupt vectors, 256 × 4 bytes | BIOS |
| `0x00000400`–`0x000004FF` | BIOS data area (BDA) | BIOS |
| `0x00007C00` | Boot sector is loaded here | BIOS |
| `0x00008000` | Shell input line | Kernel |
| `0x00008200` | **Arguments for programs** | `prog_setargs` |
| `0x00010000` | Kernel (currently ~157 KB, **room up to `0xB0000`**) | Bootloader |
| `0x0007FFF0` | BIOS stack | BIOS |
| `0x0009FFF0` | Kernel stack (= process 0) | `start.asm` |
| `0x000A0000`–`0x000B0000` | Process stacks, 8 KB each | `proc.c` |
| `0x000B0000` | Sector buffer | `fs.c` |
| `0x000B1000` | Directory in RAM (4 KB) | `fs.c` |
| `0x000C0000` | FILEBUF, 64 KB | `fs.c` |
| `0x000D0000` | ED_BUF — editor text, 60 KB | `edit.c`, GUI editor |
| `0x00100000`–`0x00114000` | Scrollback ring buffer, 512 lines | BIOS `video.asm` |
| `0x00114000` | Screen backup for the scrollback viewer | `lib.c` |
| `0x00120000` | **Terminal window buffer** 70×22×2 | `term.c` |
| `0x00124000` | Terminal window scrollback ring, 200 lines | `term.c` |
| `0x00128000` | Capture of compiler output, 40 lines | `term.c` (`cap_*`) |
| `0x00130000` | **Clipboard**, max 8 KB (`clip_len`) | `gui.c`, `pc.py` |
| `0x00200000` | **PROG_ADDR** — where the OS loads programs | `syscall.c` |
| `0x00240000` | DATA_ADDR — global variables of generated programs | `cc.c` |
| `0x00280000` | SRC_BUF — source code in CC/ASM/PY | Tools |
| `0x00292000` | OUT_BUF of the assembler | `asm.c` |
| `0x00300000` | OUT_BUF of the compiler / heap of PY | `cc.c`, `py.c` |
| `0x00380000` | STR_BUF of the compiler | `cc.c` |
| `0x003A0000` | INC_BUF (`#include`) | `cc.c` |
| `0x00400000`, `0x00500000` | FC_BUF1/2 for the file comparison | `kernel.c` |
| `0x00FFFFFF` | End of RAM (16 MB) | |
| `0x02000000` | Text framebuffer 80×25×2 | Graphics card |
| `0x02100000` | Graphics framebuffer 640×400×1 | Graphics card |
| `0x0F000000` | BIOS ROM, 64 KB, read-only | |

**Rule:** A program at `0x200000` may grow to at most `0x280000`
(PROG_MAX = 512 KB), otherwise it eats into the tool buffers. CC.TBX at
~170 KB is the biggest chunk.

## BIOS data area

| Address | Contents |
|---|---|
| `0x400` / `0x404` | Cursor x / y |
| `0x408` | Text attribute |
| `0x40C` | Timer ticks (100/s) |
| `0x414` / `0x418` | Keyboard buffer head / tail |
| `0x420` | Keyboard buffer, 32 × 4 bytes |
| `0x4A0` | **Memory size in KB** (determined by POST) |
| `0x4A4` | Disk size in sectors |
| `0x4A8` / `0x4AC` | Scrollback: write pointer / fill level |
| `0x4B0` | Scratch space for number output |

## I/O ports

| Port | Device | Meaning |
|---|---|---|
| `0x00` / `0x01` | PIC | acknowledge interrupt / mask |
| `0x10` / `0x11` | Timer | set frequency / read ticks |
| `0x20` / `0x21` | Keyboard | data / status |
| `0x30`–`0x35` | Disk | LBA, count (**16-bit**), address, command, status, size |
| `0x40`–`0x43` | Graphics | mode, cursor, palette index, palette value |
| `0x44`–`0x4C` | **Blitter** | X, Y, W, H, color, command, char, source, background |
| `0x4D`–`0x4F` | Mouse cursor | X, Y, visible |
| `0x50` / `0x51` | Speaker | frequency / on |
| `0x52` | **Double buffering** | 1 = on, 0 = off |
| `0x53` | **Show frame** | 1 = swap pages, 2 = copy back buffer to front |
| `0x54` | **Zoom** | factor for blitter command 3 (1 = normal, up to 16) |
| `0x56`–`0x5A` | **Block copier (DMA)** | source, destination, length, value, command |
| `0x60`–`0x62` | Mouse | X, Y, buttons |
| `0x63` | Mouse wheel | notches since last read; **reading resets it** |
| `0x70` / `0x71` | CMOS | select register / read+write |
| `0x80` | Debug | character to the Mac's developer log |
| `0x90` | Power supply | 1 = off, 2 = restart |
| `0xA0`–`0xA5` | Thermal | temperature, fan, throttling, limit, fan mode, peak value |
| `0xB0`–`0xB2` | BIOS chip | command / buffer size / target address — reflash the ROM, see [[16 Eigenes BIOS schreiben]] |

The framebuffer of the graphics mode starts at `0x02100000`, one byte
per pixel. Programs may write into it directly (`gx_punkt` in
`programs/gfxlib.c`) — for individual pixels this is faster than a
blitter call per pixel, because there's no system call in between.

**Ports are not protected.** There are no privilege levels on the TB-32
— a program may use `outr`/`inr` just as freely as the kernel.
`gfxlib.c` makes use of this and writes the blitter ports itself instead
of going through `int 0x40`. In C the two are called `portout(port,
value)` and `portin(port)` — TCC finds them in `prog_start.asm`, CC on
the device inserts the instruction directly at the call site.

Note the order: `outr <value>, <port>` — the **port number is in
`ra`**, i.e. the second operand.

Blitter commands (port `0x49`): 1 = filled area, 2 = outline, 3 =
character from the font, 4 = image from RAM, 5 = copy region, **7 =
scaled image** (source size in the CHR register as `width | height<<16`,
target size in W and H, nearest-neighbor), **6 = string from RAM**
(address in the CHR register `0x4A`, length in the W register `0x46`,
font stays in SRC). One instruction instead of one per letter — an
editor page is 1600 of them.

Coder's buffer: colors per visible character starting at `0x00700000`.
Word's buffers: text starting at `0x00720000`, **color per character**
starting at `0x00728000`, shape bytes per paragraph starting at
`0x00730000`, **second shape byte (lists)** starting at `0x00730400`,
image sizes starting at `0x00730800` and `0x00731800`, wrap list
starting at `0x00733000` (**four words per line**: start, length,
paragraph, page), file buffer starting at `0x00739000`, loaded image
starting at `0x00750000`.

The mouse reports in port `0x62` **bit 0 left, bit 1 middle, bit 2
right**. The desktop remembers in `gui_taste` which button triggered a
click — the right-click menu depends on it.

## Block copier and block search

The block at `0x56`–`0x5A` moves memory **bypassing the processor** — it
sees the same address space, so source and destination can also be the
framebuffer. Commands (port `0x5A`):

| Cmd | What |
|---|---|
| 1 | copy (source → destination, length in bytes) |
| 2 | fill (destination gets the value `length` times) |
| 3 | **search**: how many bytes from source onward equal the value |
| 4 | **search**: at which position from source is the first match (−1 = none) |
| 5 | **search backward**: how many bytes before source (inclusive) equal the value |

The result of the search commands is then in the **length register**
(`0x58`) and is read from there.

Why this exists: shoveling 256 KB byte by byte costs the processor a
million instructions — a third of a second. No undo, no image staging
would be smooth with that. With the block copier it's 0.03 ms. The
search commands are the counterpart to real processors' string
instructions: without it, the fill tool in Paint would need a separate
read instruction per pixel.

Addresses of the Paint buffers: canvas `0x00600008` (480×260), undo copy
`0x00640000`, fill tool queue `0x00680000`.

## Two display pages

The card has two equally sized framebuffers. One is always shown, the
other is always being drawn to; port `0x53` swaps them. As long as
double buffering is off (`0x52` = 0), both are the same field and every
draw call is immediately visible — that's how the desktop works.

That's the difference between "it flickers" and "it doesn't flicker":
without a second page, the screen reads along while drawing happens, and
you see half-drawn images. A game therefore turns on
`gx_doppelpuffer(1)` at the start, redraws the entire frame each time,
and calls `gx_zeigen()` at the end.

**Two modes**, and the choice depends on whether you're redrawing
everything or only parts:

| Port `0x53` | What happens | For whom |
|---|---|---|
| 1 | swap pages (two pointers, instant) | games — they redraw the entire frame each time |
| 2 | copy back buffer to front | **the desktop** — it usually redraws only one window, the rest must stay in place |

The desktop has used mode 2 exclusively since August 2026. That's why
nothing flickers there anymore.

A mode change (port `0x40`) resets zoom to 1 — otherwise the desktop
would keep writing in giant letters if a program crashes with zoom set.

## CMOS registers

| Reg | Contents |
|---|---|
| `0x00`–`0x09` | Time and date (binary, not BCD) |
| `0x10` | Boot order |
| `0x11` | Fast boot |
| `0x12` | Beep on startup |
| `0x13` | **Processor clock index** (0–4 → 0.4/1/2/4/8 MHz) |
| `0x15` | Verbose boot messages |
| `0x3F` | Writing to this saves the coin cell to `disk/cmos.bin` |

Related: [[01 Architektur TB-32]], [[07 Fallstricke]]
