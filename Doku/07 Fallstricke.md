# Pitfalls — Hard-Won Lessons

Every entry here cost me real debugging time. If something similar comes up:
check here first.

## Sleep didn't sleep — the machine hit 65 degrees

**Symptom:** As soon as a game with frame timing ran, the temperature went
from 23 to **65 degrees**, the chipset throttled to **60%**, and the whole
system turned sluggish — it felt like it had frozen. Load: 100%, even though
the game wanted to sleep for most of every frame.

**Cause:** `proc_next()` returns the **calling** process itself when nobody
else is ready to run — otherwise the scheduler would have nothing to pick.
That made `proc_sleep()` pointless: the process went to sleep, got woken up
again within the very same interrupt, went back to sleep … With only a
single running program, nobody ever actually slept, and the processor ran at
full load the whole time.

Found by sampling the instruction pointer against `kernel.sym`: the hot spot
wasn't in the game, but in `proc_next` / `proc_schedule` / `proc_sleep`.

**Fixed in:** `system/proc.c`. `proc_sleep()` now waits out the remaining
time itself, with the **processor halted** (`hlt`), giving others a chance
first via `int 0x41`. The `sti` before it is mandatory — we're inside a
system call, where interrupts are disabled.

| | before | after |
|---|---|---|
| Temperature during Flappy | 65.1 °C | 26.6 °C |
| Throttling | 60% | 0% |
| Load | 100% | 10% |

**Lesson:** An "idle" state only counts as idle once the processor actually
executes `hlt`. Whether that happens shows up in the temperature — the
thermal model is a more honest sensor here than any counter.

## Whoever paints alone paints over everything

**Symptom:** The clock appeared in the middle of the Control Panel, even
though the clock window was actually behind it.
**Cause:** The clock refreshed itself once per second — `app_clock(i)` only
paints its own content, without knowing which windows sit in front of it.
The same was true for the System Monitor.
**Fixed in:** `system/gui.c` — they now request a normal redraw (`neu = 1`),
and `draw_desktop()` knows the stacking order.
**Lesson:** In a windowing system, **only** the code that knows the z-order
is allowed to paint. Any shortcut of "I'll just quickly paint my own window"
is wrong exactly when something else is in front of it.

## Mouse state belongs in the events, not in a poll

**Symptom:** Right-click never arrived on the TB-32 — the menu in Word
opened during testing but not on Colin's machine.
**Cause:** `pc.py` read the button state with `pygame.mouse.get_pressed()`,
triggered by a click event. Depending on the platform this returns the old
state at release time, and on some trackpads nothing at all for the right
button.
**Fixed in:** `pc.py` — the state is now tracked from the events themselves
(`e.button`: 1 left, 2 middle, 3 right). In addition, Ctrl+click counts as a
right-click, as is customary on Mac.
**Lesson:** Whoever receives events should evaluate them directly. A state
poll inside an event handler is always one tick too late.

## Mouse state belongs in the events, not in a poll

## "Broken" actually meant "not finished yet"

**Symptom:** The fill tool in Paint filled only three lines and then
stopped. Every measurement confirmed it: lines = 3, queue = 4 entries, and
reading the counters showed 0.

**Cause:** None of this was actually a bug. The function was **still
computing** at the moment I measured it. The click waited 0.6 seconds, but
the fill took a whole half minute -- a function call with four bounds
checks per pixel, 26,000 times over. The seemingly contradictory numbers
(counters in memory = 3 and 5, my own logged copies = 2 and 4) were simply
snapshots from different moments in time.

**Fixed by measuring instead of guessing:** A look at the instruction
pointer showed the loop still happily running. After that: the block
copier fills the lines, and the hardware's block search finds the run
boundaries. What was half a minute became about one second.

**And again, two hours later:** In the Coder, the editor window stayed
empty, the status line was missing, and every counter looked wrong. Same
thing again -- the picture simply wasn't finished being painted yet,
because syntax highlighting initially cost 476,000 instructions per
redraw. This pitfall was expensive enough that it struck twice.

**Lesson:** Before hunting for a bug, check whether the thing is even
done yet. A "wrong" intermediate result that keeps changing on every
measurement is usually not a bug but an ongoing computation. And if
logged values and memory contents disagree, memory is the current one --
not the copy.

## An unknown port is silently swallowed

**Symptom:** The newly built double buffering simply did nothing. No error,
no message — `gx_doppelpuffer(1)` had no effect.
**Cause:** `bus.port_out` looks the port up in `port_devices`; if it isn't
listed there, it ends up in `unknown_ports` and is discarded. I had entered
the port in `isa.py` and in the device, but not in the device list in
`machine.py`.
**Lesson:** A new port needs **three** entries: a constant in `isa.py`,
handling in the device, and registration in `machine.py`. If a new hardware
feature "does nothing," check `m.bus.unknown_ports` first.

## A reused process slot inherits old flags

**Symptom:** After compiling in the editor, the launched program received
not a single keystroke. The same program run from the text console worked
fine.
**Cause:** `p_bg[pid]` ("started in the background," deliberately gets no
keyboard) was set but never cleared. The compiler runs in the background and
frees its slot; the command line started afterward got the same slot
**along with the old flag**.
**Fixed in:** `system/proc.c`, `proc_start()` now sets `p_bg[i] = 0`.
**Lesson:** Whoever reuses a slot has to reset *all* fields — not most of
them. And: if keys "aren't arriving," measure the key buffer first
(`BDA_KEYHEAD` / `BDA_KEYTAIL`). If `tail` grows but `head` stays put, the
problem isn't the keyboard — it's that nobody is reading.

## Whoever paints concurrently must check at the drawing call, not in the loop

**Symptom:** The desktop painted its windows onto the frame of a running
fullscreen game.
**Cause:** The check "does a program own the screen?" sat at the start of
the main loop. If the program switches mid-frame, the rest of that frame
still runs anyway. Even a guard at the start of `draw_desktop()` isn't
enough — the function paints many windows one after another, and the switch
happens in the middle of that.
**Fixed in:** `system/gui.c`, `gui_fremd` is now checked inside `g_fill`,
`g_frame`, and `g_char` themselves.
**Lesson:** With two concurrent painters, the check belongs as far down as
possible — at the spot that actually writes.

## An `#include` **inside a comment** ate real source code

**Symptom:** `fs_read_lib` and system call 33 behaved as if they didn't
exist — every call returned −1. Code right next to it, obviously correct,
had no effect at all. A measurement showed that even
`fs_find_in("SOURCE", 0 - 1)` failed, even though the very same call with a
string *from a program* found the folder without any trouble.

**Cause:** The preprocessor works line by line and only checked whether a
line starts with `#`. In `syscall.c` there was this comment:

```c
/* Datei lesen mit Suchpfad: aktueller Ordner, dann \SOURCE. Fuer
   #include im Compiler auf dem Geraet. */
if (fn == 33) return fs_read_lib((char*)a1, a2, a3);
```

The second line begins (after leading whitespace) with `#` → the
preprocessor took it for a directive and **replaced it with a blank line**.
That removed the closing `*/`, so the comment stayed open and swallowed
everything up to the next `*/` — including the `if (fn == 33)` and the start
of the next block. Counting braces made it obvious: the function `syscall`
ended at a point where the source code doesn't actually end.

**Fixed in:** `tools/tcc.py` (`zeilen_im_kommentar()` — tracks, for each
line, whether it starts inside a block comment; both passes, `#include` and
`#define`, skip such lines) and likewise in `programs/cc.c`
(`komm_folge()`), so the on-device compiler gets the same protection.

**Lesson:** A line-based preprocessor must never delete lines without
knowing whether they're inside a comment. And: if code "has no effect" even
though it's obviously correct, first count whether the compiler even sees it
at all — counting braces is cheaper than checking logic for days.

## The process switcher must save R0

**Symptom:** Programs compute wrong results or crash as soon as
multitasking is running.
**Cause:** `sched_irq_asm` saved r1–r14. But the compiler uses **r0** as the
working register for *every* expression. Every timer interrupt could
therefore destroy an intermediate result right in the middle of a
computation.
**Fixed in:** `system/start.asm` (15 registers instead of 14) and `proc.c`
(stack layout for new processes).

## Whoever gets stuck inside a system call must issue `sti`

**Symptom:** After launching a background program, the whole system froze.
`p_switches` stopped incrementing.
**Cause:** A program exits with `int 0x40`. On an interrupt, the CPU
disables interrupts; they're only re-enabled by `iret`. But `proc_exit()`
never returns — it waits in a loop **with interrupts disabled**. No more
timer, no more scheduler, dead.
**Fixed in:** `system/proc.c`, `asm("sti")` before the wait loop.
**Lesson:** Any function that doesn't return from within an interrupt has to
re-enable interrupts itself.

## `funktion()[i]` scaled the index incorrectly

**Symptom:** The Python tokenizer wrote into foreign memory.
**Cause:** `tools/tcc.py` didn't know the return type of functions and
always assumed 4 bytes per element for `f()[i]` — even when `f()` returns a
`char*`.
**Fixed in:** `tcc.py`, `self.func_types` and `type_of()` for `call`.

## The disk's sector counter was 8 bits wide

**Symptom:** From ~128 KB of program size onward, "Invalid opcode" at
startup.
**Cause:** `PORT_DISK_COUNT` masked with `0xFF`. With 327 sectors, only 71
got loaded, the rest was garbage.
**Fixed in:** `hardware/devices.py`, now 16 bits.

## Keyboard input lagged one keystroke behind

**Symptom:** Type `w` → nothing, type `i` → `w` appears.
**Cause:** `event.unicode` on `KEYDOWN` is empty under SDL depending on the
layout; the character only arrives with the following text event.
**Fixed in:** `pc.py` — characters via `pygame.TEXTINPUT`, special keys via
`KEYDOWN`.

## The window must not wait on the CPU

**Symptom:** The UI turns sluggish while a program is computing.
**Cause:** CPU emulation and drawing ran in the same loop without any time
limit. The emulation manages 1.5–3.5 million instructions/s; at a nominal
clock of 2 MHz it ate up the entire frame.
**Fixed in:** `machine.run_slice(dt, max_ms)` — at most 8ms of real compute
time per frame, in chunks of 4000 instructions (anything coarser misses the
deadline too imprecisely). If that's not enough, the virtual clock runs
slower instead.

## Text programs and the desktop don't share a screen

**Symptom:** "Run" in the File Manager → desktop stops responding.
**Cause:** The program ran in the background, wrote to the invisible text
screen buffer, and captured every key with `getkey()` — including ESC.
**Solved:** `gui_ausfuehren()` leaves graphics mode, lets the program run
visibly, and returns afterward (like Windows 3.1 with DOS programs).

## The terminal process must die on exit

**Symptom:** After leaving the desktop, the normal command line was mute.
**Cause:** The cmd process kept running, `term_aktiv` stayed at 1 — all
output ended up in the invisible window buffer.
**Fixed in:** `gui.c`, terminate the process and reset `term_aktiv = 0` both
on exit and when the window is closed.

## One interrupt can stand for several events

The interrupt controller has **one bit** per source. If two events arrive
before the handler runs, there's still only one interrupt. A handler that
picks up exactly *one* event therefore loses the second — until another one
happens to come along by chance.

This bug hit me **twice** in this project:

- **Timer**: the clock ran too slow (details below)
- **Keyboard**: `irq_kbd` fetched one key per interrupt. In the BIOS setup,
  nothing happened on the first arrow press, and the next press then carried
  out the previous movement. Fixed by having the handler drain the chip in a
  loop for as long as `P_KBD_STATUS` reports something

**Rule:** An interrupt handler asks the chip *how much* is pending — it
never assumes it's exactly one.

## The timer tick must not be counted by hand

**Symptom:** The clock ran too slow, wait times too long.
**Cause:** Several ticks per time slice only set *one* interrupt bit; the
handler only counted up by one.
**Fixed in:** `firmware/bios.asm` — the handler now reads the counter value
directly from the chip (`in r1, P_TIMER_TICKS`).

## `gui_running` was only reset by the "Exit" menu item

**Symptom:** After leaving the desktop with ESC once, `WIN` never started
the desktop again — the self-test dropped from 41 to 39.
**Cause:** The new guard against a second desktop checks `gui_running`. But
you can also leave the main loop via ESC (`break`), and there the variable
stayed at 1.
**Fixed in:** `gui.c`, `gui_running = 0` at the **end of `gui_main()`**.
**Lesson:** A state flag belongs at the place where the state actually
ends — not at every single exit point.

## The viewport snapped straight back while scrolling

**Symptom:** The mouse wheel in the editor didn't move anything.
**Cause:** `app_editor` follows the viewport to the cursor on every redraw.
The wheel moved `edg_top`, and the next screen refresh pulled it right back.
**Fixed in:** `gui.c`, `edg_folgen` — off for the wheel, on for typing and
clicking.

## The kernel grew into its own buffers

**Symptom:** The file manager showed `@`, `ager`, and `Filem` instead of
`PROGS` and `SOURCE`. The command line (`DIR`), by contrast, was correct.
**Cause:** The fixed buffers sat at `0x30000`, right behind the kernel. Once
the kernel grew past 128 KB, it overwrote the in-RAM directory with its own
data — `wtitle` ("File Manager") and `gui_pfad` ("A:\") ended up sitting in
the middle of the directory table. `DIR` re-read from disk and so saw none
of this.
**Fixed in:** `fs.c` and `edit.c` — buffers moved to after `0xB0000`;
`build.py` now aborts if the kernel reaches that far.
**Lesson:** Whoever raises a size limit (here: 255 → 511 sectors) has to
check whether the **RAM** layout can actually accommodate it. The disk
wasn't the limit — memory was.

## A program must not go to sleep inside the kernel

**Symptom:** The calculator froze on the first click, the whole machine
stood still.
**Cause:** `sleep()` (and thus `beep()`) waits for the timer using `hlt`. If
a program calls that via `INT 0x40`, interrupts are disabled — the timer
never arrives, and the `hlt` never wakes up. Same bug as above with
`proc_exit()`.
**Fixed in:** `lib.c`, `asm("sti")` at the start of `sleep()`.

## Clicks landed in the wrong window

**Symptom:** Clicked a button in the front window — the window behind it
reacted.
**Cause:** Drawing follows stacking order (`win_top` drawn last), but hit
testing rigidly went by window number in reverse. If the frontmost window
had a lower slot number, the wrong one won.
**Fixed in:** `gui.c` — check `win_top` first, then the rest.

## `continue` skipped past the redraw

**Symptom:** Start menu → *Editor*: the window opened (the taskbar button
appeared), but the screen kept showing the menu.
**Cause:** The menu branch sets `neu = 1` and does `continue` — but the
`if (neu) draw_desktop()` sits at the **end of the loop**. On the next pass,
`neu` gets reset to 0 right away.
**Fixed in:** `gui.c`, the branch now draws itself.

## A background program stole the keyboard

**Symptom:** After `START BENCH.TBX /B`, `TASKLIST` only received `ASKLIST`.
**Cause:** `getkey()` reads the global keyboard buffer. Whoever asks first
wins — even a program running in the background.
**Fixed in:** `syscall.c` — processes started with `/B` get `p_bg = 1` and
are put to sleep on `getkey()` instead of being served.

## `START X.TBX ARG /B` ran in the foreground

**Symptom:** `START CRASH.TBX COLORS /B` blocked the command line, and not
even the "Started in background" message appeared.
**Cause:** `cmd_start` only checked the **second word** for `/B`. If an
argument came before it, `/B` was just one more argument.
**Fixed in:** `kernel.c` — all words are now scanned, `/B` may appear
anywhere and is dropped from the argument list.

## Double-clicking a program in the desktop folder failed

**Symptom:** `'CALC.TBX' is not recognized as a command or program.` in the
terminal window — even though the icon sat visibly on the desktop.
**Cause:** The window types the file name into the shell, and the shell's
search path is *current folder → `\SYSTEM` → `\PROGS`*. The file was in
`\DESKTOP`, while the prompt stood in `A:\PROGS`.
**Fixed in:** `gui.c`, `eintrag_oeffnen` now sets `cwd` to the file's folder
before sending off the command.

## Building deleted the running PC's files

**Symptom:** Colin compiles programs inside the PC, closes the emulator,
restarts it — everything gone.
**Cause:** `build.py` read the **entire** disk image, swapped in the new
boot sector and kernel, and wrote everything back out. If the emulator was
running alongside (writing its sectors straight back into the same file),
that write-back overwrote its files with the old state from before the
build. The exact same thing applied to `tools/tbfs.py`, whose `save()`
wrote out the whole image.
**Fixed in:** `build.py` now writes **only sector 0 and the kernel
sectors** (`r+b`, targeted `seek`s) and no longer touches the filesystem
from sector 512 onward at all. `tbfs.py` tracks in `self.dirty` which
sectors it changed, and writes back only those.
**Lesson:** A tool that modifies a file another program has open must never
rewrite it completely — only the parts it actually touches.

## Turning up the clock doesn't help when the host is the bottleneck

**Symptom:** "Can we push the CPU above 8 MHz?"
**Finding:** The emulation managed 1.7 million instructions/s — **21%** of
the configured 8 MHz. A bigger number in the BIOS would only have changed
the display.
**Solved:** Measure first, then optimize. `hardware/cpu.py` (32-bit view of
memory, dispatch chain ordered by measured frequency, lazy decoding, local
variables instead of `self.x`) and `pc.py` (time budget instead of a fixed
8ms) — together about **3.4×** more throughput within the frame budget.
**Lesson:** Whoever tunes the emulator measures beforehand with
`tools/opstat.py` and verifies afterward with the self-test **and
bootstrapping** — the latter compares two self-generated compilers byte for
byte and catches every computation error in the CPU.

## The setup state lived in the number scratch pad

**Symptom:** Switching to the *Security* tab filled the whole screen with
zeros.
**Two causes at once**, both instructive:

1. I had stored the active tab in `BDA_SCRATCH` — but that's exactly where
   `vid_putn` formats its digits. After the first number was printed, the
   tab value was garbage and the character loop ran forever.
   Fixed: a dedicated location `SETUP_TAB`/`SETUP_ROW`/`SETUP_SAVE` starting
   at `0x600`.
2. `vid_puthex` expects the **digit count in `r3`** — I hadn't set it, so
   the digit loop ran over whatever random value happened to be in the
   register.

**Lesson:** Whoever calls a foreign BIOS service reads its signature
carefully. And a scratch buffer that "looks free right now" usually already
belongs to someone.

## Line count kept in a scratch register

**Symptom:** Infinite loop while drawing the setup screen.
**Cause:** I had held the row count in `r11`. Per [[05 Konventionen]],
`r10`–`r12` are **scratch registers** — any subroutine call is free to
destroy them, and `vid_hline` promptly did.
**Fixed:** Compare the value right after fetching it, don't cache it.

## Small things that cost time anyway

- **The character set only knows 32–127.** Draw block characters (219, 176)
  in graphics mode as rectangles yourself, otherwise bars don't appear.
- **Windows can stick out past the frame** — `starte()` clamps the position,
  but new window sizes still need checking.
- **Numbers without a background overlap each other** on refresh. Either set
  `bg` or redraw the whole window.
- **`#include` is also found inside comments** — by both `tools/tcc.py`
  *and* `cc.c`, and for the latter even when indented. `gfxlib.c` had an
  example `#include "gfxlib.c"` in its header comment and thereby included
  itself: nine syntax errors on a line that didn't even exist.
- **The resize handle must be drawn after the window content**, otherwise
  the application paints over it.
- **Truncate labels, don't just compute the center.** In the taskbar,
  "Compiling" (9 characters = 72 points) sat in a 64-point-wide button and
  stuck out on both sides; for desktop icons the name ran off the screen.
  `g_button` only centers — it doesn't truncate anything.
- **Number painted over the label.** In the clock window, uptime started at
  `x+36`, but the word "Up time" reached to `x+56` — the digits landed right
  on top of the text. Label on the left, values in a fixed column, and this
  doesn't happen.
- **Scrolling without an upper bound.** `if (top < 0) top = 0;` alone isn't
  enough — without `if (top > anzahl - zeilen)` you scroll endlessly into
  empty space. Both bounds, always.
- **Fixed row counts in lists** only hold up until the folder gets full
  enough. The file manager showed a hard-coded 11 entries with no scrolling —
  the 14 files in `\SOURCE` didn't fit, and the missing ones looked as if
  they didn't exist. Every list needs a row count derived from the window
  size **and** a scrollable viewport.
- **Keyboard buffer overflows** when test scripts keep typing during long
  compute runs. Wait for the prompt in tests (`tools/bootstrap.py` does this
  correctly).
- **TBFS's layout now lives in four places** — `system/fs.c`,
  `tools/tbfs.py`, `system/boot.asm`, and `firmware/setup.asm`. The latter
  two are intentional and can't be trimmed away: the boot sector can't call
  a BIOS routine, and the firmware runs before there even is a boot sector.
  Whoever shifts sector numbers or field offsets must touch **all four** —
  otherwise nothing boots, and the error message points at the kernel
  instead of the filesystem.
- **A semicolon inside a string cut off half the line.** The assembler
  discarded comments with `zeile.split(";")[0]`, without paying any
  attention to quotes. From
  `.db "A bad image is refused; keeps a backup", 0` it silently produced
  `.db "A bad image is refused` — text with no end, no null byte, no error
  message, and the output ran on into the next string.
  Fixed in `tools/assembler.py` (`ohne_kommentar`), but the lesson stands:
  **naive comment stripping is a text destroyer.**
- **`vid_puthex` needs the digit count in `r3`.** Forget it and the value
  gets printed hundreds of times until the whole screen is full. Looks like
  an infinite loop, but is really just a missing argument.
- **A `putc` without control characters makes backspace visible.** Colin's
  first homemade BIOS only handled `\n`. The 8 that `readline` sends to
  delete a character therefore ended up as a character in the frame
  buffer — CP437 renders it as "◘". Every keypress added another box, and
  the text stayed put. The tricky part: the buffer in memory was correct
  the whole time, and ENTER dutifully ran the empty command. **Only the
  screen was lying.** Whoever rewrites an output function must intercept 8,
  9, 10, and 13 before storing a character.
- **Text placed mid-code has to be padded to four bytes.** The new flash
  confirmation prompt added 308 bytes of strings in the middle of the BIOS —
  a number not evenly divisible by 4. Every instruction after it landed
  misaligned, and the machine died 15 instructions after reset, before any
  screen output. The TB-32 has fixed 4-byte instructions: after a `.db`, an
  `.align 4` is needed as soon as code follows again. Until now all strings
  sat at the end of the file, so it never came up.
- **`#define NAME wert /* Kommentar */` pulled the comment into the
  value.** Whoever mentioned `NAME` anywhere inside a comment later got a
  `*/` inserted there — the comment ended right there, and the prose after
  it got read as source code. The error pointed at a completely harmless
  line. Fixed in `tools/tcc.py`; `cc.c` was never affected, since it stores
  `#define` values as numbers.
- **`s[:i] + neu + s[j:]` with `j == -1` swallows half the file.**
  `find` returns −1 when it finds nothing, and `s[-1:]` is the last
  character. That's how I shrank `programs/asm.c` from 646 to 434 lines and
  had to rewrite the rest. Always check whether a `find` result is −1.
- **Hit testing must use the same order as painting.**
  `draw_desktop()` paints windows by number (0, 1, 2 …), with `win_top`
  last — whoever has the higher number appears visibly in front. Hit
  testing, though, ran **forward** and took the first hit, i.e. the window
  *behind* it. Colin could no longer click the Command Prompt once a window
  with a lower number sat underneath it. It now runs backward. **Lesson:**
  drawing order and hit-testing order are the same piece of knowledge — they
  have to be changed together.
- **When measuring, first check whether the test setup even contains the
  thing you're looking for.** While chasing stutter in the Coder I missed
  three times: once by counting only awake samples (which, on an otherwise
  waiting machine, is almost entirely the interrupt handler), once by
  reading a profile out of just 120 samples, and once by measuring with no
  file name set — **which meant syntax highlighting didn't even run**,
  i.e. exactly the part in question was missing. The user gave the crucial
  hint: "it's smooth in Word" — and Word has no highlighting.
- **A checksum must measure what actually boots.** When the kernel moved
  from the fixed sector 1 into the file `\SYSTEM\KERNEL.BIN`, Secure Boot
  would have kept computing over the old sectors — a check that never
  triggers. That's worse than no check at all, because it looks like
  security. `secure_summe` therefore looks up the same file the boot sector
  does.

Related: [[04 Compiler TCC Grenzen]], [[06 Bauen und Testen]]
