# Abbreviations and Names

All names in this project are made up — they don't exist anywhere else.
So they don't end up interpreted differently at some point, they're
recorded here **bindingly**.

## The Machine

| Abbreviation | Stands for | What it is |
|---|---|---|
| **TB** | **T**oo**b**ad | The abbreviation for the whole family, after Colin's name for his projects |
| **TB-32** | Toobad, **32** bit | The processor, and with it the whole computer. 32 bits wide, 16 registers, every instruction 4 bytes |
| **TOOBAD-OS** | — | The operating system that runs on the TB-32 |
| **TOOBAD BIOS** | **B**asic **I**nput **O**utput **S**ystem | The firmware in ROM. The term is taken from real PCs |
| **TB-VGA** | **V**ideo **G**raphics **A**rray | The graphics card. Name also taken from the real PC |

## Files and Formats

| Abbreviation | Stands for | What it is |
|---|---|---|
| **.TBX** | **TB**-32 E**x**ecutable | A finished program for the TB-32. Pure machine code, loaded and jumped to at `0x200000` — the counterpart to `.exe` |
| **TBFS** | **TB**-32 **F**ile **S**ystem | The file system on the virtual disk. Superblock, directory, data — see [[02 Speicherkarte und Ports]] |
| **.C** | — | Source code in **TC** (see below) |
| **.ASM** | **Ass**e**m**bler | Source code in TB-32 assembly |
| **.PY** | **Py**thon | Script for the built-in Python interpreter `PY.TBX` |
| **.MD** | **M**ark**d**own | Notes and text, like this page here |

## The Tools

| Abbreviation | Stands for | What it is |
|---|---|---|
| **TC** | **T**oobad **C** | The language: looks like C, but can do less. Exactly what, is written up in [[04 Compiler TCC Grenzen]] |
| **TCC** | **T**oobad **C** **C**ompiler | The compiler **on the Mac** (`tools/tcc.py`). It builds the kernel |
| **CC** | **C** **C**ompiler | The same compiler, but **on the TB-32 itself** (`programs/cc.c` → `SYSTEM\CC.TBX`). It can compile itself, see [[09 Selbst-Compilierung]] |
| **ASM** | Assembler | `SYSTEM\ASM.TBX`, translates `.ASM` to `.TBX` |
| **PY** | Python | `SYSTEM\PY.TBX`, the Python interpreter on the device |

Why two compilers with almost the same name? Because they translate the
same language but run in different places. **TCC runs on the Mac and only
knows the TB-32 as a target; CC runs on the TB-32 and doesn't know the Mac
at all.** Whoever says "the compiler" has to say which one they mean.

## Terms from Hardware

These are *not* made up — they come from real computer engineering and
mean the same thing here as everywhere else:

| Term | Stands for | Meaning here |
|---|---|---|
| **POST** | **P**ower **O**n **S**elf **T**est | The self-test at power-on: count memory, look for the disk, report the graphics card |
| **CMOS** | originally the chip type | The 64 bytes of settings that the button cell keeps alive (`disk/cmos.bin`) |
| **IVT** | **I**nterrupt **V**ector **T**able | 256 addresses starting at memory location 0. On interrupt *n*, the CPU jumps there |
| **BDA** | **B**IOS **D**ata **A**rea | The BIOS's scratch block starting at `0x400`: keyboard buffer, tick counter, cursor position |
| **IRQ** | **I**nterrupt **Re****q**uest | A component signals: timer (8), keyboard (9), mouse (12) |
| **LBA** | **L**ogical **B**lock **A**ddress | Sectors are simply numbered in sequence, 0, 1, 2 … — no cylinder/head/sector like on old drives |
| **DMA** | **D**irect **M**emory **A**ccess | The disk writes directly into memory itself, without the CPU touching every byte |
| **Blitter** | from *block image transfer* | The graphics card's 2D accelerator: one command fills an entire area |
| **Throttling** | — | If the chip gets too hot, the chipset pulls back the clock — see [[10 Temperatur]] |
| **Bootstrapping** | from *pulling yourself up by your own bootstraps* | The compiler compiles itself, see [[09 Selbst-Compilierung]] |

Related: [[00 START HIER]], [[01 Architektur TB-32]]

## Added later

| Abbreviation | Meaning |
|---|---|
| **TBI** | *TB-32 Image* — Paint's image format: width, height, then one byte per pixel |
| **TBW** | *TB-32 Word* — document format: length, paragraphs, style bytes, image sizes, colors, text |
| **Coder** | the extended editor: line numbers, syntax colors, search, indentation |
| **`emu/`** | the emulator in **real** C for the host machine — not to be confused with `system/*.c` (TC for the TB-32) |
