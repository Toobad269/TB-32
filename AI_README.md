# AI_README — everything an assistant needs to know about this project

This file is written for an AI that is meant to help a human with TOOBAD
TB-32. It contains the architecture, every command, every interface, and
the pitfalls that have already cost someone hours here.

**The one rule everything else follows from:** Python emulates **only the
chips**. BIOS, operating system, windows, and all programs are real
TB-32 machine code. If you're missing a feature, build it in TB-32 code —
not in Python. Bypassing this misses the point of the project.

---

## 1. Starting up

```bash
python3 build.py     # build BIOS, kernel, programs, and drive — ALWAYS first
python3 pc.py        # power on the machine
python3 pc.py --scale 3      # larger window
python3 pc.py --turbo        # as fast as the host can go
python3 reset.py     # factory state (asks for confirmation; --bios chip only, --ja skips the question)
```

After power-on, **five seconds of grace period** run with a blue splash
screen. Only after that does the CPU get power. Pressing `DEL` during
this time drops you into BIOS Setup — the keypress is held and delivered
later.

Without a window, for tests and for you as an assistant:

```bash
python3 tools/headless.py 8                       # boot for 8 s, screen as text
python3 tools/headless.py 8 --keys "DIR,ENTER"    # type while booting
python3 tools/headless.py 8 --after 0.5 --keys "DEL"   # into Setup
python3 tools/screenshot.py /tmp/x.png 12         # PNG, with --keys and --mouse
```

## 2. Case keys

They belong to the **window**, not the virtual machine — so they work
everywhere, even in the BIOS, where no operating system is running yet.

| Key | Effect |
|---|---|
| `ü` | Power button, when the machine is off |
| `DEL` (macOS `fn`+`⌫`) or `F2` | BIOS Setup; can be pressed even during the grace period |
| `Strg`+`K` | **copy everything, with no feedback.** Text mode: the whole screen. Graphics mode: the system supplies the text of the topmost window — **every** window, see 11.13 |
| `Strg`+`V` / `Cmd`+`V` | paste from the host machine |
| `Cmd`+`C` | copy selection from TOOBAD-OS to the host |
| `Strg`+`R` | Reset (no power-supply restart, so no grace period) |
| `F11` / `F12` | fullscreen / overlay with clock speed, temperature, frame rate |
| `Strg`+`Q` | quit |
| `Bild↑` / `Bild↓` | scroll back in text mode |

## 3. Command line

| Command | Usage | what it does |
|---|---|---|
| `DIR` | `DIR` | contents of the current folder |
| `CD` / `CHDIR` | `CD name` \| `CD ..` | change folder |
| `MD` / `MKDIR` | `MD name` | create folder |
| `RD` / `RMDIR` | `RD name` | delete empty folder |
| `COPY` | `COPY source dest` | copy file |
| `REN` | `REN old new` | rename |
| `DEL` / `ERASE` | `DEL name` | **to the recycle bin** `\RECYCLED`; deleting there is permanent |
| `TYPE` | `TYPE name` | print file |
| `MORE` | `MORE name` | page by page |
| `FC` | `FC a b` | compare two files |
| `DUMP` | `DUMP name` | hex dump |
| `FORMAT` | `FORMAT` | reformat the drive |
| `CHKDSK` | `CHKDSK` | check the filesystem |
| `VOL` | `VOL` | drive label |
| `VER` | `VER` | system and BIOS version |
| `MEM` | `MEM` | memory usage |
| `SYSTEMINFO` | `SYSTEMINFO` | everything about the machine |
| `TEMP` | `TEMP` | temperature, fan, throttling |
| `DATE` / `TIME` | | date / time |
| `CLS` | | clear screen |
| `COLOR` | `COLOR nn` | set color attribute |
| `ECHO` | `ECHO text` | print text |
| `START` | `START prog.tbx` | start a program |
| `TASKLIST` | | running processes |
| `TASKKILL` | `TASKKILL nr` | end a process |
| `EDIT` | `EDIT name` | editor (the Coder in text mode) |
| `DISPTEST` | | screen test |
| `WIN` / `DESKTOP` | | **start the desktop** |
| `SHUTDOWN` / `REBOOT` / `EXIT` | | power off / restart / go back |
| `HELP` | | same list within the system |

A program can also be started **without** `START`, simply by its name.

Names are **at most 15 characters**; case doesn't matter when searching.
Programs are looked up in: current folder → `\SYSTEM` → `\PROGS`.

Folders on the drive: `\SYSTEM` (tools and system files), `\PROGS`
(programs), `\SOURCE` (source code), `\RECYCLED` (recycle bin),
`\DESKTOP` (icons).

## 4. Programs

| Usage | what it does |
|---|---|
| `CC source.c dest.tbx` | C compiler **on the device**. Compiles itself |
| `ASM source.asm dest.tbx` | Assembler on the device. Supports `.org`, `.equ`, `.include`, parenthesized expressions, `ldwa`/`stwa` — enough for a **complete BIOS** |
| `PY file.py` | small Python interpreter |
| `CALC` | calculator |
| `FLAPPY` | game in graphics mode |
| `BENCH` `MEMTEST` `KELLERTEST` | measurement tools |
| `CRASH` | deliberately triggers errors to test error handling |

## 5. The Desktop

`WIN` starts it, *Exit desktop* in the Start menu goes back. Start menu:
File Manager, Command Prompt, Coder, System Monitor, Control Panel, Paint,
Word, Clock, About, Exit desktop.

### Coder

Editor with syntax highlighting and search. **The button bar depends on
the kind of source file** — detected by the `TBBI` marker in the header:

| Source | Buttons |
|---|---|
| C / Assembler | `< Back  New  Save  Name  Build  Run  Find` |
| Python | the same **without Build** — a `.PY` file isn't compiled |
| BIOS | `< Back  New  Save  Name  Find  Test  Flash` — no Build, no Run |

`New` asks **first** for the save location; canceling creates no file.
After that, `Save` saves without asking again. The `?` in the top right
opens the on-device guide to writing a BIOS.

### Paint and Word

Paint: tools, stroke width, fill, undo, format `.TBI`.
Word: selection, right-click menu, text colors, lists, page breaks,
embedded Paint images with resizing, format `.TBW`.
Both ask for the save location first on `New`.

### File dialog

All programs use the same window (`system/dialog.c`), filtered by
extension. `DEL` moves to `\RECYCLED`; deleting **there** is permanent.

## 6. BIOS and firmware

Setup via `DEL`, five tabs: **Main, Hardware, Cooling, Security, Firmware**.

The BIOS chip is swappable. An image has a **48-byte header**:

| Offset | Contents |
|---|---|
| `0x00` | jump over the header |
| `0x04` | the four characters `TBBI` |
| `0x08` | length in bytes |
| `0x0C` | checksum (`h = 0x1234`, per word `h = h*31 + word`) |
| `0x10` | name, 32 bytes, null-terminated — **the mainboard shows it on the splash screen** |
| `0x30` | code starts here |

Length and checksum are filled in by `build.py` (or by the Coder during
`Test`/`Flash`).

**Three safety nets, three points in time:** the firmware checks before
burning, the mainboard checks at power-on and falls back to the backup
otherwise (Dual BIOS), and *Restore Backup BIOS* brings back a valid
image that got stuck. The **one-shot start** (`Test`) applies only to the
next boot; the image lives on the board, not on the disk.

Full guide with all services: `Doku/16 Eigenes BIOS schreiben`.

## 7. Architecture — the complete tables

### Registers

| | |
|---|---|
| `r0` | return value **and the compiler's working register** — every expression ends up here |
| `r1`–`r5` | arguments 1–5 (there is no more; the compiler can't handle six) |
| `r6`–`r9` | must be preserved by the called function |
| `r10`–`r12` | scratch registers, may be clobbered at any time |
| `r13` (`at`) | assembler's helper register — always clobbered after `ldwa`/`stwa` |
| `r14` (`fp`) | frame pointer |
| `r15` (`sp`) | stack pointer |

**Because the compiler computes everything in `r0`, every interrupt
handler must save `r0`.**

### Memory map

| Address | what |
|---|---|
| `0x00000000` | interrupt vectors, 256 × 4 bytes |
| `0x00000400` | BIOS data area (cursor, color, ticks, keyboard buffer) |
| `0x00007C00` | the boot sector loads here |
| `0x00008000` | the boot sector reads the directory here |
| `0x00010000` | **kernel** |
| `0x0007FFF0` | firmware stack |
| `0x00100000` | screen history, 512 lines |
| `0x000B0000` | fixed filesystem buffers — **the kernel must not grow this far** |
| `0x000D0000` | Coder's text buffer, 60 KB |
| `0x00120000` | terminal window |
| `0x00130000` | TOOBAD-OS clipboard |
| `0x00200000` | the OS loads programs here |
| `0x00600008` | Paint's canvas |
| `0x00720000` | Word's text |
| `0x00770000` | window text for `Strg`+`K` |
| `0x02000000` | text video memory, 80 × 25 × 2 bytes |
| `0x02100000` | graphics video memory, 640 × 400, one byte per pixel |
| `0x0F000000` | **BIOS ROM**, 64 KB, read-only |

RAM: 16 MB.

### Instruction set — all opcodes

Every instruction is **exactly 4 bytes**. Formats: `n` no operand, `r`
one register, `rr`, `rrr`, `ri` register+constant, `rri`, `mem`
`[base+offset]`, `j` jump, `c` call, `i` constant, `ir` port+register.

| Opcode | Mnemonic | Format |
|---|---|---|
| `0x00` | `nop` | n |
| `0x01` | `hlt` | n |
| `0x02` | `cli` | n |
| `0x03` | `sti` | n |
| `0x04` | `iret` | n |
| `0x05` | `ret` | n |
| `0x06` | `brk` | n |
| `0x10` | `mov` | rr |
| `0x11` | `movi` | ri |
| `0x13` | `movh` | ri |
| `0x18` | `ldb` | mem |
| `0x19` | `ldsb` | mem |
| `0x1A` | `ldh` | mem |
| `0x1B` | `ldw` | mem |
| `0x1C` | `stb` | mem |
| `0x1D` | `sth` | mem |
| `0x1E` | `stw` | mem |
| `0x20` | `add` | rrr |
| `0x21` | `sub` | rrr |
| `0x22` | `mul` | rrr |
| `0x23` | `div` | rrr |
| `0x24` | `mod` | rrr |
| `0x25` | `and` | rrr |
| `0x26` | `or` | rrr |
| `0x27` | `xor` | rrr |
| `0x28` | `shl` | rrr |
| `0x29` | `shr` | rrr |
| `0x2A` | `sar` | rrr |
| `0x2B` | `not` | rr |
| `0x2C` | `neg` | rr |
| `0x2D` | `cmp` | rr |
| `0x2E` | `tst` | rr |
| `0x2F` | `udiv` | rrr |
| `0x30` | `addi` | rri |
| `0x31` | `subi` | rri |
| `0x32` | `muli` | rri |
| `0x33` | `divi` | rri |
| `0x34` | `modi` | rri |
| `0x35` | `andi` | rri |
| `0x36` | `ori` | rri |
| `0x37` | `xori` | rri |
| `0x38` | `shli` | rri |
| `0x39` | `shri` | rri |
| `0x3A` | `sari` | rri |
| `0x3D` | `cmpi` | ri |
| `0x3E` | `tsti` | ri |
| `0x3F` | `umod` | rrr |
| `0x40` | `push` | r |
| `0x41` | `pop` | r |
| `0x42` | `call` | c |
| `0x43` | `callr` | r |
| `0x44` | `pushf` | n |
| `0x45` | `popf` | n |
| `0x50` | `ja` `jae` `jb` `jbe` `jc` `jeq` `jg` `jge` `jl` `jle` `jmp` `jn` `jnc` `jne` `jnn` `jnv` `jnz` `jv` `jz` | j |
| `0x51` | `jmpr` | r |
| `0x60` | `in` | ri |
| `0x61` | `inr` | rr |
| `0x62` | `out` | ir |
| `0x63` | `outr` | rr |
| `0x64` | `int` | i |

**Encoding:** `r` formats `(op<<24)|(rd<<20)|(ra<<16)|(rb<<12)`,
`i` formats `(op<<24)|(rd<<20)|(ra<<16)|(imm&0xFFFF)`,
jumps `(op<<24)|(cond<<20)|(off&0xFFFFF)` with `off = (target-pc)/4`.

**Conditions** for `0x50`: `al`=0 `z`/`eq`=1 `nz`/`ne`=2 `c`/`b`=3
`nc`/`ae`=4 `n`=5 `nn`=6 `v`=7 `nv`=8 `be`=9 `a`=10 `l`=11 `ge`=12 `le`=13
`g`=14.

**Pitfall:** `cmp`, `cmpi`, `tst`, `tsti`, `jmpr`, `callr` use **`rd`**,
not `ra`. Mixing this up gets you an emulator that's almost right.

**Pseudo-instructions** of the assembler: `li rd, value32` (expands to
`movi`+`movh`), `ldwa/ldha/ldba/stwa/stha/stba rd, ADDRESS` (expands to
`li at, ADDRESS` plus access through `at`).

**Directives:** `.org` `.equ` `.include` `.db` `.dw` `.space` `.align`.
Expressions support `+ - * /` and parentheses, with standard precedence
(`*`/`/` before `+`/`-`).

### Ports

| Port | for what |
|---|---|
| `0x00`/`0x01` | interrupt controller: acknowledge / mask |
| `0x10`/`0x11` | timer: set frequency / read ticks |
| `0x20`/`0x21` | keyboard: get character / one waiting |
| `0x30`–`0x35` | disk: LBA, count, address, command (1 read, 2 write), status, size |
| `0x40`–`0x43` | graphics card: mode (0 text, 1 graphics), cursor, palette |
| `0x44`–`0x4C` | **Blitter**: x, y, w, h, color, command, character, source, background |
| `0x4D`–`0x4F` | hardware mouse cursor: x, y, enabled |
| `0x50`/`0x51` | speaker: frequency / on |
| `0x52`/`0x53` | double buffering on / make frame visible |
| `0x54` | zoom for Blitter command 3 |
| `0x56`–`0x5A` | **Block copier**: source, destination, length, fill byte, command |
| `0x60`–`0x63` | mouse: x, y, buttons (bit 0 left, 1 middle, **2 right**), wheel |
| `0x70`/`0x71` | CMOS: address / value |
| `0x80` | developer log |
| `0x90` | power supply: 1 off, 2 restart |
| `0xA0`–`0xA5` | temperature, fan, throttling, limit, fan mode, peak value |
| `0xB0`–`0xB2` | **BIOS chip**: command, buffer size, address |

**Blitter commands** (port `0x49`): 1 filled area, 2 outline, 3 character,
4 image, 5 copy, 6 string, 7 scaled image.
**Block copier** (port `0x5A`): 1 copy, 2 fill, 3/4/5 search.
**BIOS chip** (port `0xB0`): 1 fetch file from host, 2 buffer into RAM,
3 burn, 4 restore backup, 5 buffer out of RAM, 6 register for one boot,
7 unregister, 8 register permanently, 9 is a request pending.

**A new port needs three entries:** the constant in `hardware/isa.py`,
handling in the device, registration in `hardware/machine.py`. Missing
the third one, it does nothing — with no error at all. `m.bus.unknown_ports`
reveals it.

### BIOS services

Function number in `r0`, arguments from `r1` on, result in `r0`.

**`INT 0x10` screen** — this order is mandatory:

| r0 | Name | Arguments |
|---|---|---|
| 0 | putc | r1 character, r2 attribute |
| 1 | puts | r1 pointer, r2 attribute |
| 2 | setcursor | r1 x, r2 y |
| 3 | clear | r1 attribute |
| 4 | getcursor | → `y<<16 \| x` |
| 5 | putat | r1 x, r2 y, r3 character, r4 attribute |
| 6 | putn | r1 number, r2 attribute |
| 7 | puthex | r1 value, r2 attribute, **r3 digits** |
| 8 | setmode | r1 = 0 text, 1 graphics |
| 9 | box | r1 x, r2 y, r3 w, r4 h, r5 attribute |
| 10 | fillrect | same |
| 11 | hline | r1 x, r2 y, r3 length, r4 character, r5 attribute |
| 12 | scroll | — |
| 13 | clearrow | r1 y, r2 attribute |
| 14 | putsat | r1 x, r2 y, r3 text, r4 attribute |
| 15/16 | sbcount / sbline | screen history |

`putc` **must handle control characters 8, 9, 10, and 13 itself.**
Without 8, backspace prints a little box instead of deleting.

**`INT 0x13` disk:** 0 read (r1 sector, r2 count, r3 address → r0
status), 1 write, 2 size.
**`INT 0x16` keyboard:** 0 wait (→ `scancode<<8 \| ASCII`), 1 peek,
2 flush. The wait should include a `hlt`.
**`INT 0x1A` time:** 0 ticks (100/s), 1 time of day `h<<16\|m<<8\|s`,
2 date `y<<16\|m<<8\|d`.

### OS system calls — `INT 0x40`

Number in `r0`, arguments `r1`–`r4`.

| Nr | | Nr | |
|---|---|---|---|
| 0 | putc | 17 | setmode |
| 1 | puts | 18 | out(port, value) |
| 2 | getkey | 19 | in(port) |
| 3 | cls | 20 | box |
| 4 | exit | 21 | hline |
| 5 | ticks | 22 | memkb |
| 6 | putn | 23 | flushkeys |
| 7 | setcursor | 24–27 | query directory |
| 8 | putat | 28 | report progress (0–100) |
| 9 | haskey | 29 | report status text |
| 10 | fileread | 30 | address of the character set |
| 11 | filewrite | 31 | draw filled area/outline |
| 12 | clock | 32 | draw character |
| 13 | date | 33 | fileread with search path |
| 14 | sleep | | |
| 15 | beep | | |
| 16 | disksize | | |

`INT 0x41` voluntarily yields CPU time.

### TBFS filesystem

| | |
|---|---|
| Superblock | sector 512, magic `TBFS` = `0x54424653` |
| Directory | sectors 513–520, 128 entries of 32 bytes each |
| Data | from sector 576 |
| Entry | name 16 bytes, start `+16`, size `+20`, info `+24`, time `+28` |
| Info | type in the lowest byte (1 file, 2 folder), **parent folder+1** in bits 16–31 |

**Files are stored contiguously.** That's the only reason a loader fits
in 512 bytes. The layout is defined in **four** places: `system/fs.c`,
`tools/tbfs.py`, `system/boot.asm`, `firmware/setup.asm` — change one,
and you must change all of them.

Custom formats: `.TBX` program (loads to `0x200000`), `.TBI` image
(width, height, then one byte per pixel), `.TBW` Word document.

## 8. Building and testing

```bash
python3 tools/selftest.py       # 62 checks from power-on to the desktop
python3 tools/ctest.py          # language tests for the compiler
python3 tools/bootstrap.py      # the compiler compiles itself
python3 tools/emu_vergleich.py  # C vs. Python, instruction by instruction
```

After **every** change to `hardware/cpu.py` or `emu/cpu.c`,
`emu_vergleich.py` needs to run. After changes to the system,
`selftest.py`.

## 9. Pitfalls — check here first before hunting for a bug

1. **`cmp`, `cmpi`, `tst`, `jmpr`, `callr` use `rd`, not `ra`.**
2. **Text in the middle of code needs `.align 4`.** Instructions are a
   fixed 4 bytes: without padding, every following instruction is
   misaligned, and the machine dies before the first frame.
3. **"Broken" has twice already meant "not finished yet" here.** Check
   first whether the computation has even completed — an intermediate
   result that changes on every measurement is usually not a bug.
4. **Work out the cost per frame.** At 2 MHz, a frame has about 33,000
   instructions. A loop over 3000 bytes per redraw eats that up on its
   own and looks like a hang.
5. **A loop only counts as idle once the CPU executes `hlt`** —
   otherwise the machine heats up and throttles itself.
6. **Draw order and hit-test order are the same knowledge.** Change one
   and you must change the other, or windows stop being clickable.
7. **`g_button` only centers, it never truncates.** Check labels against
   the width — `g_text_max()` truncates where the length is unknown.
8. **A new port needs three entries:** the constant in `isa.py`,
   handling in the device, registration in `machine.py`. Otherwise it
   does nothing, with no error at all (`m.bus.unknown_ports` reveals it).
9. **`#define NAME value /* comment */`** used to pull the comment into
   the value. Fixed now, but this family of bugs lingers.
10. **The TBFS layout is defined in four places** (`fs.c`, `tbfs.py`,
    `boot.asm`, `setup.asm`). Moving sector numbers means changing all
    four.

In detail, with symptom, cause, and location: `Doku/07 Fallstricke`.

## 10. Where things live

| | |
|---|---|
| `pc.py` | **the case**: monitor, keyboard, mouse, sound, splash screen, grace period. No machine logic |
| `hardware/` | CPU, bus, devices — the chips |
| `firmware/` | BIOS and Setup in assembler, plus `minimal.asm` as a template |
| `system/` | the operating system in C and assembler |
| `programs/` | programs for the drive, including the compiler and assembler |
| `tools/` | compiler, assembler, and tests for the host machine |
| `emu/` | the same emulator in C, ~150× faster, the path to the Raspberry Pi |
| `Doku/` | **the working reference** as an Obsidian vault. When unclear, start with `00 START HIER`; log changes in `14 Aenderungsjournal` |

**If you're contributing here:** read `Doku/00 START HIER` first, follow
`Doku/05 Konventionen` (interface in English, comments in German), and
log every change in `14 Aenderungsjournal` — with the *cause*, not just
the symptom.

---

# 11. The interface, window by window

This section describes every window precisely enough that you can tell a
user exactly where to click without looking yourself.

Base dimensions: screen **640 × 400**, characters **8 × 8** in graphics
mode. Taskbar starts at **y = 378**. Every window's title bar is
**14 points** tall.

## 11.1 The Desktop

**Taskbar at the bottom.** Far left the **Start** button (x 2, width 52).
To its right, one button per open window (width 64, starting at x 90) —
clicking one brings it to the front. Far right, the **clock**.

**Start menu** (click on *Start*, entries 14 points tall, starting at
y 262):

| # | Entry | opens |
|---|---|---|
| 0 | File Manager | file management |
| 1 | Command Prompt | command line in a window |
| 2 | Coder | editor |
| 3 | System Monitor | processes and readings |
| 4 | Control Panel | settings |
| 5 | Paint | drawing program |
| 6 | Word | word processor |
| 7 | Clock | clock |
| 8 | Settings | change password, reset the machine |
| 9 | About TOOBAD-OS | system info |
| 10 | Power options | Restart, Shut down, Sign out |
| 11 | Exit desktop | back to the command line |

**ESC does NOT leave the desktop.** Up through 2.5.2 it did -- a holdover
from when the text console was home base. Now that the machine boots
straight into the desktop, that was a trap: an accidentally pressed key
would throw you out of Coder, Paint, or Word into the console mid-edit,
with unsaved text. The only way out is *Exit desktop*.

**User account.** Exactly one, in `\USER.DAT` (24 bytes, hidden): name
from byte 0, password checksum from byte 20. The file always lives
**in the root directory** -- `benutzer_anlegen()` and
`benutzer_vorhanden()` briefly set `cwd` to the root for this, because
`fs_write`/`fs_read` would otherwise operate in whatever folder is
currently open. `pw_summe("") == 0x1234`: an empty password counts as
*not locked*, and the login screen is skipped. `build.py` creates **no**
account -- the first boot asks for one. Test tools create one for
themselves via `test_konto()` in `tools/headless.py`.

**Desktop icons.** Files from `\DESKTOP` appear as icons and can be
dragged with the mouse; `ICONS.DAT` remembers their positions.
Double-click starts or opens them.

**Window frame.** In the title bar, on the right: **Maximize**
(x = width−30, 12 × 11) and **Close** (x = width−16). Bottom right, a
12 × 12 resize handle. Dragging the title bar moves the window.

**Ordering:** windows are drawn by window number, with `win_top` last —
the higher the number, the further to the front. Click hit-testing runs
**backwards** to match the draw order.

## 11.2 File Manager

Columns **Name / Size / Type**. A click selects, double-click opens:
folders navigate in, `.TBX` launches, text files go to the Coder, `.TBI`
to Paint, `.TBW` to Word. The **Up** button top right goes up one level.
Files can be dragged to the desktop with the mouse.
Scroll with `Page Up` / `Page Down`, delete with `Del` (to the recycle bin).

## 11.3 Command Prompt

The command line in a window, **70 × 22** characters. Everything from
section 3 works here. Its own history with `Page Up` / `Page Down`.
Output from programs launched from the Coder also lands here.

## 11.4 Coder

**Header line:** `File: NAME  in PATH  Ln n  Col n  Bytes n`. When a
message appears on the right (`saved`, `built`, `errors`, `building ...`,
`not found`), the byte count yields — the two share the same space. Far
right, the **`?`** (20 × 14) that opens the BIOS-writing guide.

**Button bar** — it depends on the kind of source file, detected by the
`TBBI` marker in the header. Buttons shift together when one is missing:

| Kind | Buttons (from the left) |
|---|---|
| C / Assembler | `< Back` `New` `Save` `Name` `Build` `Run` `Find` + search field |
| Python | the same **without** `Build` |
| BIOS | `< Back` `New` `Save` `Name` `Find` + search field + `Test` `Flash` |

Widths: Back 50, New 38, Save 44, Name 46, Build 50, Run 40, Find 40,
search field 100, Test 52, Flash 56 — 4 points of spacing between each.

| Button | what it does |
|---|---|
| `< Back` | back to the start page with the file list and templates |
| `New` | asks **first** for the save location. Canceling creates nothing |
| `Save` | saves without asking, once the location is set |
| `Name` | edit the filename in the header |
| `Build` | compiles: `.ASM` with `ASM.TBX`, otherwise with `CC.TBX`. A progress window; on errors it becomes a message window (520 × 240) and `ENTER` jumps to the first error line |
| `Run` | saves, compiles if needed, runs — output in the terminal window |
| `Find` | search field; `ENTER` jumps to the next match, `not found` appears on the right |
| `Test` | builds a BIOS, checks it, asks once — and boots the machine **once** with it |
| `Flash` | the same, but permanently; afterward, the **firmware** asks a second time, in red |

**Start page** (after `< Back` or on first open): on the left, the
templates **C program .C**, **Assembler .ASM**, **Python script .PY**,
**BIOS .ASM** — on the right, the current folder's file list with an
`Up` button.

**In the text:** syntax highlighting depending on the language, mouse
selection, scrolling with `Page Up`/`Page Down`, `Home`/`End`,
`Ctrl`+`A/C/X/V`.

## 11.5 Paint

**Tools** (two columns, 24 × 20 points each):

| | | | |
|---|---|---|---|
| `Pen` | `Era` eraser | `Lin` line | `Box` rectangle |
| `Bx*` filled | `Cir` circle | `Fil` fill | `Get` eyedropper |

Below that, **Size** with 1, 2, 4-point stroke widths, then the **color
palette**, then the buttons **New**, **Undo**, **Save**, **Open**
(47 × 14 each, stacked).

`New` asks for the save location first. `Undo` reverts one step. Format
`.TBI`: width and height as a word, then one byte per pixel. While
dragging a line, rectangle, or circle, a preview appears that's
**clipped to the canvas**.

## 11.6 Word

**Button bar at the top:** `B` bold, `U` underline, `A` text color,
`A+`/`A*` size, `1.` numbered list, `*` bullet list, `<`/`>` indent,
`><` line break, then `New`, `Save`, `Open`.

**Right-click** opens a menu with 14 entries (166 points wide, rows
14 tall):

| | |
|---|---|
| Black, Red, Green, Blue, Orange, Grey | text color of the selection |
| Copy `^C`, Cut `^X`, Paste `^V` | clipboard |
| Select all, Deselect | selection |
| Insert picture | insert a Paint image (file dialog, `.TBI` only) |
| Delete picture | delete the clicked image along with its paragraph |
| Save as text | save as a plain text file |

**Keys:** arrows, `Home`/`End`, `Page Up`/`Page Down`, `Delete`,
`Backspace`, `ENTER`. An image is an entire paragraph — once clicked,
`Delete` or `Backspace` removes it completely. Page breaks every
**620 points** of height, with the page number in the margin. Format
`.TBW`.

## 11.7 System Monitor

Shows the process table (number, state, name), memory usage, the
configured clock speed, measured temperature, fan speed, and throttling.
Refreshes once per second — **without drawing itself**; it requests a
normal redraw so it doesn't paint over other windows.

## 11.8 Control Panel

Five rows; clicking a row changes the value (it cycles):

| Row | Values |
|---|---|
| CPU Clock Speed | 0.4 / 1 / 2 / 4 / 8 MHz |
| POST Beep | on / off |
| Quick Boot | on / off — **skips the pauses in the self-test** |
| POST Messages | brief / verbose |
| Fan Control | automatic / quiet / full speed |

Below that, the **Save to CMOS** button (96 × 16) — only this makes the
settings permanent. After clicking, **`Saved`** appears next to it in
green for three seconds. On the right, the current temperature, which
**refreshes once per second** — like the clock and the monitor, this
window triggers a redraw of the whole desktop for that, since only the
desktop knows the window order.

## 11.9 Clock, About

**Clock:** time of day, date, and uptime. **About:** system name,
version, CPU, memory, graphics card.

## 11.10 File dialog

One window for all programs (`system/dialog.c`), 380 × 250.

Top left **Save as** / **Open** / **Picture**, next to it the current
path, and the **Up** button on the right. Below that, the list (folders
marked `DIR`, files with size), filtered by extension — Paint only sees
`.TBI`, Word only `.TBW`, *Insert picture* only images. **Folders are
always shown**, otherwise you couldn't navigate into them.

At the bottom, the **Name:** field and, on the right, **OK** (44 × 18)
and **Cancel** (56 × 18).

**Keys:** `↑`/`↓` select, `ENTER` confirms, `ESC` cancels, `Backspace`
deletes in the name field. Clicking a folder navigates into it; clicking
a file is already the answer when opening.

## 11.11 The firmware windows in the Coder

**Confirmation dialog** (420 × 150): name, size, and checksum of the
image, followed by three lines of explanation — for Test: "runs once,
the chip stays as is"; for Flash: "the firmware will ask once more, in
red". Buttons **Test once** / **Continue** and **Cancel**.

**Help** (`?`, 460 × 300): 33 lines about the header, the interrupt
vectors, all screen functions, the control characters, and boot-sector
loading. `Page Up`/`Page Down` scrolls, `ESC` closes.

## 11.12 BIOS Setup

`DEL` or `F2`. Five tabs, switched with `←`/`→`:

| Tab | Contents |
|---|---|
| **Main** | time, date, Quick Boot, POST beeper, POST messages, load defaults |
| **Hardware** | CPU clock, boot device, memory, disk, graphics card |
| **Cooling** | fan mode, throttle limit, temperature, fan, throttling, peak value |
| **Security** | Secure Boot, checksum, *Trust Current Boot Image* |
| **Firmware** | chip size and checksum, *Flash BIOS from File*, *Restore Backup BIOS* |

**Keys:** `↑`/`↓` row, `←`/`→` tab, `ENTER` or `+`/`−` change, `F5`
defaults, `F10` save and exit, `ESC` discard and exit.

## 11.13 How `Strg`+`K` gets to the text

In graphics mode, the screen holds pixels, not text — the case can't
read anything from there. So it **asks**: `pc.py` sets `wt_wunsch`
(request flag), the desktop sees this on its next loop iteration, places
the text at `0x00770000`, and sets `wt_len` (length). The case picks it
up one-twentieth of a second later.

Four windows respond with their **full** content, not just the visible
portion: **Coder** (the entire source text), **Word** (the entire body
text), **File Manager** and **file dialog** (path and all entries),
**Terminal** (all 22 lines).

All others — Control Panel, System Monitor, Clock, About, the firmware
windows — **simply draw themselves again**, except every piece of text
lands in the buffer instead of on the screen (`wt_aktiv`). Text on the
same pixel row is joined with two spaces; a new pixel row becomes a new
text line.

**This is why nothing needs to be added by hand here:** a new window
supplies its content automatically as soon as it uses `g_text` and
`g_num`. Anyone writing a custom draw routine only needs to honor
`if (wt_aktiv)`.
