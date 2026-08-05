# Conventions

## Registers

| Register | Role |
|---|---|
| `r0` | Return value **and the compiler's working register** — every expression ends up here |
| `r1`–`r5` | Arguments 1–5 |
| `r6`–`r9` | must be saved by the called function |
| `r10`–`r12` | scratch registers, may be clobbered at any time |
| `r13` (`at`) | assembler's helper register — always clobbered after `ldwa`/`stwa` |
| `r14` (`fp`) | frame pointer |
| `r15` (`sp`) | stack pointer |

Because the compiler computes everything in `r0`, **every interrupt
handler must save r0** — see [[07 Fallstricke]].

## Calling a function

The compiler evaluates the arguments, pushes them onto the stack, and
pops them back into `r1`…`r5` in reverse order. Prologue of every
function:

```
push fp
mov fp, sp
subi sp, sp, <frame>     ; size is filled in later
push r6 … r9
[store parameters into the frame slots]
```

Epilogue: `mov sp, fp` / `pop fp` / `ret`.

## BIOS services (firmware)

Full list and known gaps: [[13 BIOS-Dienste und was fehlt]]

Function number in `r0`, arguments in `r1`–`r5`, result in `r0`.

| Interrupt | Service |
|---|---|
| `INT 0x10` | Screen: 0 putc, 1 puts, 2 setcursor, 3 cls, 4 getcursor, 5 putat, 6 putn, 7 puthex, 8 setmode, 9 box, 10 fillrect, 11 hline, 12 scroll, 13 clearrow, 14 putsat, **15 sbcount, 16 sbline** |
| `INT 0x13` | Disk: 0 read, 1 write, 2 size |
| `INT 0x16` | Keyboard: 0 wait, 1 peek, 2 flush |
| `INT 0x1A` | Time: 0 ticks, 1 clock, 2 date |
| `INT 0x40` | **Operating system syscall** |
| `INT 0x41` | Voluntarily yield CPU time (scheduler) |
| `IRQ 0x08` | Timer (100 Hz) — carries the process switcher when multitasking is active |
| `IRQ 0x09` | Keyboard |

## System calls (`INT 0x40`)

Number in `r0`, arguments `r1`–`r4`, result in `r0`.

| No | Meaning | No | Meaning |
|---|---|---|---|
| 0 | putc(ch, attr) | 14 | sleep(ticks) |
| 1 | puts(str, attr) | 15 | beep(freq, duration) |
| 2 | getkey() | 16 | disksize() |
| 3 | cls(attr) | 17 | setmode(m) |
| 4 | exit() | 18 | out(port, value) |
| 5 | ticks() | 19 | in(port) |
| 6 | putn(n, attr) | 20 | box(x,y,w,h) |
| 7 | setcursor(x, y) | 21 | hline(x,y,len,ch) |
| 8 | putat(x,y,ch,attr) | 22 | memkb() |
| 9 | haskey() | 23 | flushkeys() |
| 10 | fileread(name, addr, max) | 24–27 | query directory |
| 11 | filewrite(name, addr, len) | **28** | **report progress (0–100)** |
| 12 | clock() | **29** | **report status text** |
| 13 | date() | **30** | **address of the font** |
| | | **31** | **draw filled area/outline** (x\|y<<16, w\|h<<16, color, command) |
| | | **32** | **draw character** (x\|y<<16, color\|char<<16, background) |

Syscalls 28 and 29 feed the compile window; `cc.c` reports progress and
phase through them.

The output calls (0, 1, 2, 3, 6, 9) automatically go to the **terminal
window** when `term_aktiv` is set. Programs don't notice.

## Keyboard shortcuts

| Key | Effect |
|---|---|
| `F11` | Fullscreen of the **emulator window** |
| `Ctrl`+`Q` / `Cmd`+`Q` | Quit |
| `Ctrl`+`R` | Reset (case button) |
| `F12` | Overlay with clock, temperature, frame rate |
| `Cmd`+`V` / `Ctrl`+`V` | Paste text from the Mac into TOOBAD OS |
| Hold a key | Backspace, Delete, and arrows repeat after 0.4 s every 30 ms |
| `Cmd`+`C` | Copy selection from TOOBAD OS to the Mac |
| `Ctrl`+`A/C/X/V` | inside the guest: select all / copy / cut / paste |
| `ü` | **Power button** — only works when the machine is off |
| `Ctrl`+`K` / `Cmd`+`K` | **copy everything**, silently: in text mode the whole screen (also in the BIOS and setup), in graphics mode the full text of the Coder |
| at power-on | **5 s grace period** (`EINSCHALT_HALT_S` in `pc.py`), keys pressed during it are discarded |

`Ctrl`+*letter* is generally passed through to the guest in `pc.py` as
control character 1–26 — this check comes **after** Ctrl+Q and Ctrl+R.

`Ctrl`+`K` lives in the **case** (`pc.py`, `alles_kopieren`), not in the
system. No operating system runs in the BIOS or setup that could
evaluate a key — from there it works everywhere. It deliberately reports
nothing: whoever presses the key knows what they wanted.

The `ü` key is deliberately **not** handled among the key-press events,
but at the text-input event (`TEXTINPUT`). At `KEYDOWN`,
`event.unicode` is either empty or still carries the previous
keystroke's character depending on layout — umlauts only arrive
reliably via the text-input event.

## Naming

- System UI: **English** — and that means absolutely everything that
  ends up on the TB-32's screen, including firmware and boot-sector
  messages. The new boot sector had German text and stood out
  immediately.
- Source code comments: **German**
- Variables in the OS source: a grown mix of German and English —
  don't standardize them, that just creates diffs with no benefit.

## Filesystem

Names **max 15 characters**, case-insensitive when searching,
uppercase in the display. Programs are searched for in: current folder
→ `\SYSTEM` → `\PROGS`.

Related: [[01 Architektur TB-32]], [[04 Compiler TCC Grenzen]]
