# Changelog

Every change, every bug found, and every new feature — **newest entries
on top**. For bugs, the *cause* is included, not just the symptom; the
symptom won't help next time.

The deeper traps additionally have a detailed entry in
[[07 Fallstricke]].

---

## COMPANY-OS: the requirements spec is done, and the flash mystery is explained

Colin is building the company tab himself. So this isn't coding, it's
writing down what the BIOS should be able to do — in
`Custom BIOS/COMPANY-OS/README.md`.

**The find along the way is the real payoff.** His "sometimes the
flashed BIOS is there, sometimes it isn't" is not a bug in the BIOS:
`build.py` rebuilds `firmware/bios.bin` on EVERY run — and that exact
file is the ROM chip. Whoever flashes COMPANY-OS and then builds once
has the stock BIOS back on the chip. On a real mainboard this can't
happen, because the compiler can't reach the flash chip; for us, both
live in the same folder.

The README lays out three ways around it: have `build.py` leave the chip
alone when a foreign BIOS name is in the header; a `--bios <file>` flag
in `pc.py` for trying things out without flashing (`Machine(rom=...)`
already supports it, `pc.py` just doesn't pass it through); or both.

Also in the requirements spec: the six lines of the *Company* tab and
where the settings can live (under the checksum in the CMOS there are
exactly eleven free bytes — switches and a block list fit, the 32 bytes
of company text don't, so an NVRAM chip or a self-writing chip is
needed instead), the path into the system via the BIOS data area
including a new block list starting at 0x528, the four touch points in
`system/gui.c`, and two traps you only experience once: recomputing the
header checksum after patching, and putting the company block behind
the first 16 KB, because that's exactly what Secure Boot measures.

**Added afterward: the full feature list.** Colin wanted to know
everything such a BIOS could conceivably do. Now they're listed in four
groups in the README — mandatory, very useful, nice-to-have, skip —
each with tab, storage location, and effort, plus a build order.

Two findings while going through it:

* **The BIOS password today only protects setup.** A company machine
  needs a second one that's asked BEFORE booting — otherwise you just
  switch the machine on and use it.
* **`pw_tor` counts failed attempts in a register.** Three attempts,
  then rejected — but a press of Reset starts back at three. So you can
  guess at your leisure. Real BIOSes count in the button cell; one
  byte, and it changes everything.
* **`P_FLASH_CMD` is an ordinary port, and the TB-32 has no concept of
  port permissions.** Any program in the running system can overwrite
  the BIOS chip. A switch in setup doesn't help against that — the
  lock has to live in the component itself: a latch in `Flash`, set by
  the firmware shortly before booting, cleared only by a restart.
  That's exactly how real chipsets do it with their lock bit.

The BIOS locks programs via a fixed name table with one bit per
program — the same way a real BIOS has "USB Ports: Enabled/Disabled."
Because the BIOS has the names, it places them in plain text in memory;
the system then doesn't have to interpret bits.

---

## Paint Saves — the Bug Was the Message

Paint could save the whole time. While testing, I had pressed **Open**
instead of **Save**, and Paint responded with *"no such file / name
missing"* — the same sentence for two completely different situations:

* the file to open doesn't exist
* saving didn't work

So I went looking for the bug in saving, where there was none. Now there
are two messages: **"no such file"** and **"could not save."** An error
message that lumps two causes together sends whoever's searching in the
wrong direction — this one cost me half an hour.

Verified with the full path: draw → Save → choose folder and name in
the dialog → *saved* → Open → same file → *loaded*, and the picture is
back.

**Two minor things along the way:** the status line wasn't cleared
before writing, so "saved" and "loaded" overlapped into letter salad.
And `pt_dialog_pruefen()` was placed before the declaration of
`pt_name` — that works with this single-pass compiler, but it's exactly
the kind of ordering where it silently creates a second variable. It
now sits behind it.

---

## The File Dialog's Return Value Carries Through

The dialog had long returned its result, and the program acted on it
correctly too — **it just never redrew.** The check sits in the idle
loop, which otherwise draws nothing; the window kept showing the old
selection screen even though the Coder had already switched to the
editor internally.

This was hard to spot because everything else was correct: the dialog
accepted keys, `dlg_status` was set, the program picked it up. Only a
counter in the kernel ("how often does the program ask?") showed that
the chain ran completely — 110 queries while the dialog was open. That
made it clear: the bug is **after** the return value, not before it.

`*_dialog_pruefen()` now reports whether something changed, and the main
loop redraws accordingly. In Word, Paint, and the Coder alike.

Verified: in the Coder, click "C program" → the dialog appears, pick a
folder and name → the editor opens with the template, and the status
line shows **saved**. The file is where it was placed.

**Still open:** Paint reports "no such file / name missing" when saving
a new image, even though the name arrives correctly from the dialog.
The dialog isn't at fault — the bug is in writing the 125 KB image file.

---

## The File Dialog Is Back — As a Kernel Service

Colin: *"you used to have to pick a path when creating a file — please
add it back, and the kernel bridge for every program."*

`system/dialog.c` is back, but in the right place: **in the kernel, for
every program**. So it looks the same everywhere, knows the current
folder, and lets you choose the location before a file is created —
that's exactly why the big systems have a shared Open dialog.

**The return path had to be rebuilt.** Previously the dialog called a
function on the requester — that worked as long as the editor, Paint,
and Word lived in the kernel. You can't jump into a standalone program.
Now the dialog stores its result, and whoever asked picks it up:

| Call | Meaning |
|---|---|
| `datei_dialog(mode, extension, suggestion)` | open it (open / save / image) |
| `datei_gewaehlt(buffer)` | 0 = still open, 1 = chosen, 2 = cancelled |

**Not yet done:** the dialog appears, lists the folders, accepts a name,
and closes — but the handoff **back into the program** doesn't work yet:
the Coder stays on the selection screen afterward instead of switching
to the editor. That's the one piece still missing.

---

## Follow-up on the Save Bug — and Three Minor Things

A lot of what Colin reported about Word ("the size can't be set
anymore, nothing saves") was **the same memory bug**: Word's text sits
at 7.2 MB, and the memory test wrote its patterns exactly there. After
separating the program slots, the buttons work again — verified: A+
sets the large font, Save writes the file and reports *saved*.

Three real bugs remained:

**The Coder silently opened a foreign file on startup.** Since the
move, it reads a file name from the argument field — and that still
held the name of the last-started program (`PROMPT.TBX`). Whoever
opened the Coder from the menu landed straight in the editor instead of
the selection screen. The argument field is now cleared before a
program is launched from the menu.

**Saving in Word led nowhere.** The kernel's file dialog was gone; the
name field was there, but ENTER just closed it. Now ENTER also saves —
or opens, or inserts an image, depending on what the name field was for.

**A window took a second to appear.** `fw_neu` filled the image buffer
byte by byte — at up to 100000 bytes in TB-32 code, that's exactly the
delay you saw. Now the block copier does it, and the window appears
instantly.

---

## The Bug Behind All the Others: Every Program Loaded at the Same Address

Colin reported four things at once: Paint got garbled strokes as soon
as the memory test ran; Paint couldn't be closed afterward; BENCH was
gray; and once Flappy joined in, everything froze. **That was a single
bug.**

`prog_start.asm` had `.org 0x00200000`. So every program was loaded to
the same location and ran there. As long as only one was running, this
was fine — it worked that way for decades. Since programs moved out of
the kernel and **several now run simultaneously in windows**, the
second one overwrites the first one's code out from under it.

The second half compounded it: `MEMTEST` checked the range **3 to 9
MB** — and exactly there sit Paint's canvas (6 MB) and Word's text
(7.2 MB). The memory test filled them with its test patterns. Those
"crooked little strokes" in Paint were literally the memory test's
patterns.

**The fix is the one early systems also had, long before there was
memory management: every program gets its own place assigned at build
time.**

* `build.py` allocates: tools (CC, ASM, PY) run alone and keep the old
  slot at 512 KB; window programs get 128 KB each starting at 2.5 MB.
* Every program carries `"TBXP"` and its address in its header. The
  loader reads them, moves the program there with the block copier, and
  starts it there.
* Programs **without** this header — such as ones compiled on the
  device itself — land at the first slot as before. Old files keep
  working.
* `MEMTEST` now checks 10 to 14 MB, where nothing else sits.

Verified: Paint, memory test, and Flappy run **simultaneously**, each in
its own window, nothing freezes and nothing paints over someone else's
data.

And the lesson that reaches beyond this project: moving programs out of
the kernel was technically correct — but it broke an assumption nobody
had written down, because it had held true for twenty years.
*Only one program at a time.*

---

## Every Program Now Runs in a Window

Systematically tested: **all thirteen** programs in the start menu open
a window. Three didn't do so before — BENCH, MEMTEST, and KELLERTEST
are **text programs**: they write to the text console with `print()`,
and you can't see that on the desktop. The program ran, but stayed
black.

**New for this: text output in a window** (`tf_*` in `gfxlib.c`). A
rectangle of characters like a text card, with wrapping and scrolling;
`tf_warten()` keeps the window open until it's closed. This means any
future text program can have a window without anyone rewriting
anything.

**While converting these, I stepped into an old trap.** I replaced the
calls with a regular expression — and it treated the comma **inside a
string** as an argument separator:

```c
printc("  FAILED, errors: ", RED);   ->   tf_text("  FAILED);
```

The same kind of bug as the semicolon in assembly strings back then.
Now the replacer looks for the closing parenthesis while tracking
whether it's inside a string.

**Power options did nothing.** The window opened, the buttons were
dead: while cleaning out the extracted windows, the click branch for
`APP_POWER` got removed along with them. A window whose clicks nobody
handles looks like a window — you only notice when you press it.

**Reset didn't work if a password was set.** The kernel only allows it
once the password has been entered correctly during this run — but the
reset button never asked for it. Now it asks: *"Enter your password to
reset this machine."*

---

## Four Bugs From Everyday Use

**Programs from the file window produced a black screen.** The
double-click had already been converted — but the **Open/Run button**
still had its own path that called `gui_ausfuehren`: the desktop steps
aside, the program gets the whole screen. Since every program is now a
windowed program, all that was left to see was black. Two paths to the
same goal are one too many; now the button calls the same function as
the double-click.

**Foreign windows open centered.** The stacking offset in `starte()` is
meant for built-in windows; a program doesn't know where it will land,
and Flappy ended up half off-screen. Now `fw_neu` computes the center.

**Fullscreen distorted the picture.** On resizing, the window width
changed — but the program kept drawing with the old one, and since the
width also serves as the buffer's stride, the picture ran crooked.
Now a foreign window gets an `FE_MALEN` on entering fullscreen and after
dragging the corner, fetches the new size, and draws correctly.

**Reset stopped working.** Settings is a program and asks the kernel;
the kernel required the password to have been named once beforehand.
On a machine **without** a password there's nothing to name, though —
so you could never get there. An open machine may now do it without.

**And one more, only visible if you look closely:** a new window wasn't
there, even though it was in the list. `fw_neu` redrew the desktop from
the **program's** process — and right after, the interface painted its
own picture over it. Now `fw_neu` just sets a flag, and the desktop
redraws in its own loop.

---

## The System Programs Have Moved Out — and the Self-Test Lags Behind

**`\SYSTEM\PROGS` now holds four programs:** `MONITOR.TBX`,
`CONTROL.TBX`, `SETTINGS.TBX`, and `PROMPT.TBX`. All four run and have
been verified in windowed form.

**The command line is split at the right seam.** The **shell** — the
command interpreter with DIR, COPY, CD — stays in the kernel; it's part
of the operating system. The **window** is a program: it draws the
shell's screen buffer and passes keys into it. That's exactly how it is
with the big systems too — shell and terminal are two different things.

**One bug along the way that shows exactly this pattern:** the window
stayed empty. The kernel kept reading `term_dirty` ("has the shell
written something?") and clearing it — stealing the signal from the
program. There was no longer a kernel terminal window that could have
done anything with it. After removing that, the window shows
everything.

**Control Panel needed not a single new system call** — everything it
adjusts lives in the button cell or a port, and a program can reach
both directly.

**The start menu now scrolls with the mouse wheel.** The little arrows
at the edge were eight points wide — missing them was easier than
hitting them, and then you'd accidentally launch a program. They're
also bigger now and colored.

**The file manager stays in the kernel**, for a reason: it drags files
onto the desktop and into other windows. That's not an application,
that's the desktop itself — the way Explorer *is* the shell on Windows.

### Open: the Self-Test

The checks assume in many places that the command line is a window
**in the kernel** — they type into it to launch programs. That no
longer exists. 18 of 86 checks fail because of this, starting with
closing a program window; after that the machine stays on the desktop
and every console check runs into nothing.

This is **not a bug in the programs** — they're tested individually —
but a test mechanism lagging behind the restructuring. It needs
reordering: all console checks first, then the desktop, and programs
should be launched via `menue_prog(name)` instead of a guessed menu
number.

---

## The Coder Has Moved Out — the Biggest Chunk

780 lines in `gui.c`, plus the color scheme from `coder.c` and the text
core from `edit.c`: the Coder was the thickest part of the kernel. Now
it lives on disk as `\PROGS\CODER.TBX`.

**Five new system calls** were needed, because a program needs things
that used to be "just there":

| No. | For |
|---|---|
| 47/48 | a program starts a program and asks whether it's still running — the Coder uses this to call the compiler |
| 51–54 | the compiler's logged messages |
| 55–57 | list the current folder, change it, ask for the path — so any program can build a file browser |
| 58/59 | show the BIOS instructions and build a BIOS |
| 60 | launch something in the command line (its output ends up there) |

**What deliberately stays in the kernel: burning a BIOS.** That's the
one place where you can render the machine unusable. The safeguards
against that — checksum, red confirmation, one-time test — belong where
no program can bypass them. The Coder *asks* for it, instead of doing
it itself.

**Two bugs with the same pattern.** While cutting things out of
`gui.c`, I grabbed too much twice: once the file-management variables
(they sat right behind `edg_masse`), once the BIOS-building ones. Both
times the compiler reported an "unknown variable" somewhere entirely
different. The lesson: when cutting, go to the function's **closing
brace**, not to the start of the next one — there's often other stuff
in between.

**And one you notice immediately:** the background. In the kernel, the
desktop drew the window frame and its fill before the application got
its turn. In its own buffer, the program has to clear it itself —
otherwise the old view kept showing through underneath.

The file dialog (`system/dialog.c`) was removed without replacement:
its three clients — Paint, Word, Coder — have all moved out and now ask
for the name in their own window. A double-click in the file window
launches the Coder with the file name as an argument.

**The kernel is about 2400 lines lighter.** 85/85.

**Status:** still in the kernel are the file manager, command line,
System Monitor, Control Panel, Settings, Browser — plus Clock and
About, which stay there.

---

## Word Has Moved Out

`system/word.c` no longer exists — Word now lives on disk as
`\PROGS\WORD.TBX`. 1200 lines, and the conversion followed the same
pattern as Paint. Three spots needed a bit more:

**The clipboard.** Its buffer sits fixed at `0x00130000` and is open to
everyone — only its *length* was a number in the kernel. There are now
two system calls for that (45 read, 46 set). Word fetches it before
every keystroke and writes it back afterward; that's how text keeps
moving between Word, the Coder, and the host machine.

**The right mouse button.** Word uses it to open its menu. Which button
was pressed used to be passed through by the kernel as `gui_taste` — a
program now just asks the mouse itself (`portin(0x62)`).

**The block copier.** Word copied images with a function from Paint.
Both have moved out; now Word talks to the DMA ports directly.

The kernel's file dialog goes away — the name is typed in its own
window, the way Word already handled "Save as" anyway.

85/85. Verified: window stands, toolbar complete, typing gets through.

---

## Paint, Calc, and Flappy Have Moved Out

Three programs now live as files on disk instead of in the kernel or in
fullscreen. **Paint was the first real move out of the kernel** —
`system/paint.c` no longer exists, the code lives in `programs/paint.c`
and becomes `\PROGS\PAINT.TBX`.

**The conversion was almost mechanical**, and that's the good news for
everything still to come:

| in the kernel | as a program |
|---|---|
| `x = win_x[i]; y = win_y[i] + TITLE_H;` | `x = 0; y = 0;` — in its own buffer, everything starts at zero |
| `g_text`, `g_fill`, `g_frame`, `g_button` | `gx_text`, `gx_fill`, `gx_frame`, `p_knopf` |
| `fs_read`, `fs_write` | `fileread`, `filewrite` (system calls) |
| `sys_out(P_DMA_…)` | `portout(P_DMA_…)` |
| kernel file dialog | type the name in its own window |

**One thing needed new hardware information.** Paint tracks the mouse
itself while dragging a stroke — events wouldn't work for this, they
only fire on press, not on movement. For this, `fw_groesse` now also
returns the **window's position on screen**, so the program can convert
screen coordinates to window coordinates.

Flappy lost double buffering in the process, and rightly so: that's for
the screen. Whoever draws into their own buffer only shows it with
`fenster_fertig()` — nothing can flicker there.

Word needed one line changed: it copied images with `pt_kopieren` from
Paint. But the block copier is hardware and open to everyone — now Word
talks to the ports directly.

**Status of the moves:** Calc, Flappy, Paint are files. Still in the
kernel: file manager, command line, Coder, System Monitor, Control
Panel, Word, Browser, Settings — plus Clock and About, which are meant
to stay there.

85/85.

---

## The First Real Move: the Calculator Runs in a Window

`CALC.TBX` was a fullscreen program — it took over the whole screen and
the desktop was gone. Now it lives in `\SYSTEM\PROGS`, appears in the
start menu, and runs in a window you can move. Keys and mouse clicks
come through window events, drawing happens into its own image buffer.

The conversion itself is small: `gx_start()` removed (it switches the
screen mode and cleared the desktop), `fenster_neu()` added, a
`fenster_malziel()` before every draw call, and the main loop fetches
events instead of keys. The measurements had to shrink — the calculator
was designed for 640×400 and doesn't fit any window like that.

**The bug that cost half an hour, and the lesson from it.** After the
conversion, the screen went black even though the new code can't do
that anymore. The reason: **CALC.TBX existed twice on disk.** The new
version in `\SYSTEM\PROGS`, the old one still in `\PROGS` — and the
search path (cwd, `\SYSTEM`, `\PROGS`) found the **old** one. So the
computer dutifully started yesterday's program, `gx_start()` and all.

Two things fixed:

1. The search path now also knows `\SYSTEM\PROGS`, and **before**
   `\PROGS`. What belongs to the system wins against a same-named file
   in the user folder — otherwise the system could be undermined by a
   planted file.
2. `build.py` cleans out the old version from the other folder while
   building, and says so. A move that leaves a corpse behind isn't a
   move.

Foreign windows now show their own name in the title bar — "Calculator"
instead of "Program." Only the program itself knows what it's called.

Two new checks. 85/85.

---

## The Start Menu Shows What's There — and \SYSTEM Is Protected

**The menu reads the folders.** It shows the built-in windows at the
top, then everything found in `\SYSTEM\PROGS` and `\PROGS`. Seven at a
time, the rest via the arrows on the right; *Power options* and
*Exit desktop* always stay at the bottom — you shouldn't have to scroll
just to turn the machine off. Drop a `.TBX` into either folder and it
shows up in the menu next time it's opened. Programs from these folders
are shown in the concrete color, so you can see what's a file and
what's still baked into the kernel.

**`\SYSTEM\PROGS` is new** — the programs that belong to the system.
Like `System32` on Windows: they're perfectly normal files, but
deleting them costs the password.

**The protection.** `fs_delete` and permanent deletion ask
`darf_system()`: if the entry sits in `\SYSTEM` (including a
subfolder), it's only allowed with permission. `SUDO` asks for the
password once and grants five minutes of peace — exactly like real
sudo. A machine without a password counts as open, and there the
protection doesn't ask either. The point isn't distrust: a single lost
`KERNEL.BIN`, and the machine never starts again.

**Two small things that came up while testing:**

The menu remembered how far you'd scrolled. Opening it again left you
in the middle of the list and the first entries seemed to be missing.
Now it always starts at the top.

`MENU_ANZ` no longer exists — the number of entries isn't known until
the menu opens anyway. The self-test now works with `MENU_SICHT` and
scrolls itself if an entry isn't visible.

83/83.

---

## The Window Server: a Program From Disk Gets a Window

Up to this point the rule was: either a program has the **entire**
screen (Flappy, Calc), or it's compiled as a window straight into the
kernel (Word, Paint, the Coder). `FENSTER.TBX` is neither — it lives as
a file on disk, runs as its own process, and still has a window that
can be moved while everything else keeps running alongside it.

**The core of it: the blitter can now draw into memory.** Ports
0x5B–0x5D give it a target buffer instead of the screen. Every foreign
window has one (256 KB, starting at 0x00800000), the program draws into
it, and the desktop composites the buffers together. This means no
program can draw over a foreign window, and a covered window is allowed
to keep drawing without anyone seeing it — exactly like a compositor.

Events go the reverse way: the desktop places keys, clicks, and "please
close" into a ring of eight entries per window, and the program picks
them up. Syscalls 40–44, library in `gfxlib.c`.

**The bug that was the real lesson.** The screen went black. Cause: the
blitter is **a single piece of hardware**, and its target buffer is a
register. The program pointed it at its own buffer, got interrupted
mid-draw — and the desktop then painted its own picture into the
**foreign buffer** instead of onto the screen. Nothing was left on the
screen.

The fix is the one every real operating system uses: **the graphics
hardware's state belongs to the process** and gets saved on a context
switch, just like the registers. `proc_schedule()` now reads out target
buffer, size, and font address, and restores them when the process gets
its turn again. For this, the four ports were made readable — in both
emulators, otherwise they'd have drifted apart.

**Second bug, same family:** the program called `gx_start()`. That
switches the screen mode and clears the picture — correct for a
fullscreen program, the end of the desktop for a window. A window only
needs the font address.

Four new checks: a window is created, the program draws into its
buffer, the screen stays intact throughout, and the program cleans up
its own window. **82/82**, and both emulators keep computing instruction
for instruction identically.

**What this means for Word, Paint, and the Coder:** the path is now
open. They'll need drawing functions that paint into their own buffer
instead of the screen — that's grunt work now, not research anymore.

---

## One Call Is Enough: pc.py Starts the Network Along With It

Colin: *"but I only want to have to start pc.py for networking to
work."* He's right — three windows for one computer isn't usable.

The router and the proxy now run in their own threads **within the same
process** as the case. Both are pure Python, both spend most of their
time waiting anyway, and both go away with the window.

**The gatekeeper is the proxy's port.** Start a second TOOBAD window,
and 8080 is taken — then that instance starts neither the proxy nor the
router. This matters: **two routers on the same wire** would trip over
each other answering ARP, and connections would end up split half with
one and half with the other.

**The proxy is preset** (127.0.0.1:8080). If nobody answers there,
browsers like `FETCH` fall back to the direct path — then everything
except HTTPS works. Whoever uses the kernel without `pc.py` doesn't end
up sitting in front of a dead display.

`python3 pc.py --kein-netz` leaves both out. Starting them individually
still works (`router.py`, `proxy.py`) — then you can watch them work.

78/78.

---

## Networking Complete: the Proxy Brings HTTPS

`https://example.com` works in the browser window. The TB-32 still only
speaks **HTTP** — the encryption is done by `proxy.py`.

**Why that's the honest path.** HTTPS means TLS, certificates, half a
dozen cryptographic procedures. On the TB-32 that would be years of
work for something you wouldn't even see in the end. A proxy isn't a
crutch, it's the solution the internet has always had — companies and
schools have exactly one of these. The TB-32 hands off what it can't
do, instead of pretending it can.

**How the browser uses it:** with a proxy configured, the connection
goes to IT instead of the server, and the request carries the **full**
address (`GET https://example.com/ HTTP/1.0`) instead of just the path.
That's exactly how every browser handles a proxy. `FETCH` takes the
same route.

**A small thing with consequences:** `NET PROXY 0.0.0.0` was meant to
turn it off. That doesn't work — `ip_lesen` returns 0 for it, and 0
also means "unusable input." So the proxy stayed on, and afterward four
checks in the self-test ran into nothing because they went through a
proxy that had long since been shut down. Now it's `NET PROXY OFF`.

Two new checks, unencrypted against a server on the Mac — that the
proxy can also do TLS is Python's job and needs no proof inside the
TB-32. **78/78.**

That completes networking: card, ARP, IP, ICMP, UDP, DNS, TCP, HTTP,
browser, proxy. Next up: the path to the Pi.

---

## Networking, Stage 5: the Browser

*Start ▸ Browser*, type an address, ENTER. The TB-32 looks up the name,
establishes the connection, fetches the page, and displays it —
headings in the concrete color, links blue and underlined, text wrapped
to window width. Clicking a link fetches the next page.
`system/browser.c` understands the page, `gui.c` draws it.

**What the wrapping has to handle:** not breaking in the middle of a
word. If a line is full, the last space is found, and the rest moves to
the next line. And multiple consecutive spaces collapse into one — HTML
doesn't count them, and without this every page looks ragged.

**Three bugs, each its own lesson:**

1. **The scratch buffer sat on the first line.** While wrapping, the
   rest of the line briefly moves elsewhere — that was `BR_ROH +
   BR_ROHMAX`, and that lands exactly on `BR_TEXT`, i.e. line 0 of the
   page. A word fragment from the middle of the text ended up at the
   top of the window. Computing addresses is convenient, until two
   computations collide.

2. **The link was gone before the line was finished.** A line usually
   only ends at the next paragraph — by then `</a>` is long past and the
   link state already reset. So `br_zeilenlink` now remembers that
   *this* line has a link until the line is completed.

3. **Relative links lost the port.** `/b` became `127.0.0.1/b` —
   without `:8080`. Nobody was listening on port 80. The port belongs
   to the host.

**Honest about the color:** headings and links were both blue and
indistinguishable. Links now have an underline.

**What it can't do:** CSS, JavaScript, images, tables as a grid — and
HTTPS. That leaves it able to show little of today's pages. The next
step is therefore the proxy on the Pi, which takes over the encryption.

Four new checks against two real pages on the Mac: render, heading,
link, and following a link. Instead of fixed wait times, it waits
**until** the page is ready — how long DNS and the transfer take
depends on the host machine's mood on the day. 76/76, stable twice.

---

## Networking, Stage 4: TCP — a Real Page From the Real Internet

`FETCH example.com` fetches a page from the internet. The TB-32 looks
up the name, establishes a TCP connection, sends an HTTP request, and
reads the response. 828 bytes, with headers from Cloudflare and the HTML
behind them.

**The three-way handshake:** we SYN → they SYN+ACK → we ACK. After
that, a stream of bytes flows, each numbered and acknowledged. FIN at
the end.

**The numbers wrap around.** That's why comparisons are never done with
`<`, but via the difference: `(a - b) < 0` means "a comes before b" —
this holds true even across the wraparound.

**Deliberately left out:** data that doesn't attach exactly is dropped
instead of being managed in gaps. The other side then sends it again.
That's allowed, and it saves half the bookkeeping.

**The bug that cost an hour:** the router sent its segments with **its
own** address as sender. But the TB-32 had addressed 104.20.23.154 and
rightly discarded everything that came from someone else. A router that
translates addresses has to present the reply with the address of the
**real** server. The handshake never came together this way, and yet
the router reported "connected": it really did have its own connection
outward.

**And a find that had been sitting there for a while.** `NET IP`
sometimes complained about an address nobody had typed. The rest of the
line was computed: `cmdline + 4 + strlen(arg1) + 1`. For `net ip` — six
characters — that lands on position 7, **one past the terminating null
byte.** Sometimes there happened to be a zero there and everything
worked, sometimes garbage. It only surfaced once the TCP code shifted
the memory layout. Now `nach_woertern()` counts the words instead of
computing an offset.

`FETCH name[:port] [/path]`. Two new checks with a real web server on
the Mac, so no internet required. 72/72.

Next up: **the browser** — reading HTML and rendering it in a window.

---

## Networking, Stage 3: UDP, DNS, and the Router

IP delivers a packet to the right **machine**. But many programs run
there — which one is meant? That's what port numbers are for, and the
simplest protocol with ports is **UDP**: eight bytes of header, done.
No connection setup, no acknowledgment. That's exactly how **DNS**
works too, the service that turns `example.com` into an address.

**UDP's checksum includes the sender and destination**, even though
they're already in the IP header — the "pseudo header." This catches a
packet that lands at the right machine but in the wrong context. And a
0 means "not computed," so a computed 0 becomes 0xFFFF.

**Names are stored in pieces:** `example.com` becomes
`7 example 3 com 0`. In the response, names may be **abbreviated** —
two bytes starting with `0xC0` point to a location earlier in the same
packet. Ignore that, and skipping past it runs into nothing, never
finding the address.

**Routing:** whatever isn't on the local network goes to the
**gateway**. It's then asked via ARP, not the actual destination —
that's exactly what every machine on the internet does.

**New: `router.py`.** It hangs off the same multicast group as the
cards, has the address 10.0.0.254, answers ARP and PING, and forwards
UDP outward (NAT). No cheating here: the TB-32 builds ARP, the IP
header, the checksum, UDP, and the DNS query **entirely by itself**;
the router just reads and forwards it. On the Pi it will drop away
without replacement, since a real router sits on the network there.
`--dns <address[:port]>` sends name queries to a different server — the
self-test uses this for its own small name service and thereby needs
**no internet** at all.

**A build-time bug that forces some thought:** in the router, both the
router's own address and the method for IP packets were named `ip`.
Python silently overwrites one with the other — `self.ip(...)` threw
"'int' object is not callable." In a language with separate namespaces
this would never have happened.

Verified: `PING 10.0.0.254` answers, `HOST example.com` reports the real
address. Two new checks, 70/70.

Next up: **TCP** — the big piece. Then HTTP and the browser.

---

## Networking, Stage 2: ARP, IP, and ICMP — PING Answers

The card only knows hardware addresses, the internet only knows IP
addresses. What ties both together is **ARP**: *"Who has 10.0.0.2?
Reply to me."* Whoever has it replies with its hardware address, and we
remember it in a table with eight slots.

On top of that sits **IP** (header with sender, destination, lifetime,
checksum) and inside that **ICMP** — the protocol behind PING. All in
`system/net.c`, all TB-32 code. So it will later run unchanged on the
Pi.

**The one calculation that's woven through the whole internet** is the
checksum: add all numbers as 16-bit values, fold the overflow back in
at the bottom, invert the result. Whoever recomputes it and gets 0
knows: nothing broke along the way.

**Numbers are stored on the network with the highest byte first**, the
TB-32 stores them the other way around. That's why there's
`net_get16`/`net_put16`, byte by byte — a word access would deliver
them reversed, and without any error message.

**Why the machine answers when nobody's sitting in front of it.**
`net_bearbeiten()` hangs inside `getkey()` (console) and inside the
desktop's loop. While nobody's typing, mail gets processed. The
`sys_halt()` in between keeps the machine quiet during this: it's woken
by the next interrupt — the timer or an incoming frame.

The address is derived at startup from the hardware address
(`10.0.0.<last byte>`), so something already works without any setup.
`NET IP 10.0.0.5` sets it, `NET ARP` shows who the machine knows.

Verified with two machines: `PING 10.0.0.2` → *4 of 4 answered*, and
the other side's address table then shows our address. Three new
checks, 68/68.

Next up: UDP and DNS, then TCP.

---

## Networking, Stage 1: the Card, the Driver, the NET Command

The start of a network of our own. The principle stays the same:
**Python only emulates the chip.** The card only knows frames and
nothing else — six bytes destination, six bytes sender, two bytes type,
then payload, exactly like Ethernet. What's inside is up to the TB-32
itself.

**Ports 0xC0–0xC7, IRQ 0x0D.** Send: place address and length, command
1. Receive: place address, command 2, read back the length. The
**card**, not the sender, fills in the sender address — so you can't
impersonate someone else, just like with real hardware.

**The wire** is a UDP multicast group on the Mac (239.32.32.32:32032).
Two running TB-32 instances hang off it like two machines on a hub.

**The trap along the way — an hour spent:** with `INADDR_ANY`, macOS
picks the default route's interface for multicast, i.e. Wi-Fi. Frames
then go out and **never** arrive on the same machine; sending happened
without an error, nothing was received. Only `IP_MULTICAST_IF` and
explicitly joining via `127.0.0.1` gets them to their destination.
Extending this to a real network will need touching this same spot
again.

**Second trap:** the address came from the process number. Two cards in
the same process (the self-test!) ended up with the same address — and
since every card drops its own frames, nothing arrived. Now a running
counter is factored in.

`NET` shows status, its own address, and the counters. `NET SEND <text>`
sends a broadcast, `NET WATCH` shows what arrives. Verified with two
machines: A sends, B shows sender, type, and text.

What's still missing, in this order: ARP and IP (then PING answers),
UDP and DNS, TCP, and right at the end HTTP and a browser. We can't do
HTTPS — for that a proxy on the Pi will later fetch pages and pass them
along as plain HTTP.

---

## ESC No Longer Exits the Desktop

Pressing **ESC** on the desktop used to throw you into the big text
console — in the Coder, in Paint, and in Word even right out of your
work, with unsaved text.

**Why it was there:** the text console used to be home base and the
interface was the program you left again with ESC. Since the machine
now boots into the desktop, it's the other way around — and the old key
had become a trap. The way out is now *Start ▸ Exit desktop*.

**The self-test depended on it.** Three checks left the desktop with
ESC; they now click the last menu item. The number is read from
`MENU_ANZ` in `gui.c`, not hardcoded.

**And the tests no longer touch the user's disk.** They work on a
working copy (`test_platte()` in `tools/headless.py`), which has an
open account. Previously every test ran on the real disk, and
`build.py` overwrote the account in the process — Colin's password was
gone after every build. The C emulator takes a fifth parameter for
this: the disk path, so both sides see the same one when comparing.

---

## Account Was in the Wrong Folder — and the First Boot Belongs to the User

**The find.** There was a `USER.DAT` on the disk **in `\SYSTEM`**. That
shouldn't happen, and it explains some odd behavior:
`benutzer_anlegen()` wrote the file with `fs_write("USER.DAT", ...)`,
and `fs_write` works in the **current folder**. If the file window
happened to be sitting in `\SYSTEM`, that's where the account landed.

Two consequences, both unpleasant:

1. Login searches the root directory. An account in `\SYSTEM` is never
   found there — the machine asked for first-time setup again after
   every boot.
2. Reset deliberately leaves `\SYSTEM` untouched. An account sitting
   there survives every reset and can't be gotten rid of.

`benutzer_anlegen()` and `benutzer_vorhanden()` now briefly set `cwd`
to the root and back afterward. Reset also cleans up any stray
`\SYSTEM\USER.DAT`.

**Two safeguards during reset.** If the folder `SYSTEM` isn't found,
`sys == -1` — and -1 is also the parent folder of the root directory.
The protection check would then have protected exactly the wrong thing:
everything above, and everything in the folders would have been
deleted, including `KERNEL.BIN`. The machine would never boot again.
Now nothing happens in that case, with a message. And after deleting,
it's verified that `\SYSTEM\KERNEL.BIN` is still there — only then does
it restart.

**The first boot belongs to the user.** `build.py` no longer creates an
account. Whoever downloads the project, builds it, and starts it sets
up their own user — previously they sat in a stranger's account named
`user` that they'd never created. The test tools create their own
(`test_konto()` in `tools/headless.py`, called from `test_cmos()`).

**Minor things.** The login screen didn't show a `User` field — it drew
the name's input buffer, and nobody types a name when logging in. Now
the account's name is shown there. And `reset.py` crashed with
`EOFError` when started from the editor: there's no keyboard attached
to the program there, so nobody could answer the security question.
Now it says to run it in the terminal instead — or with `--ja`.

---

## Password Fields Are Now Clickable

When changing the password, you could **only** move from one field to
another with TAB. Someone working with the mouse clicks into the field
they mean, though — and nothing happened. The input silently landed in
the wrong field.

Both places with multiple fields were affected: the two-step password
change in **Settings** and the **first-time setup screen** at the very
first boot.

**The hit area is deliberately bigger than the box.** The drawn box is
only 14 points tall; a click nine points below it missed on the first
try, with no way to see why. Now the whole line including its label is
the hit target — 24 points tall at a 26-point spacing, so no overlap
with the neighboring line. Measured and verified with clicks on box
center, box edge, and label.

The hint lines now also say so: *"Click a field or press TAB."*

---

## TOOBAD-OS 2.5.2

1.0 becomes **2.5.2** — system and BIOS together, in all six places
(boot screen, header, `ver`, `about`, setup, header of the BIOS image).

**Nearly a trap:** the name field in the header is **exactly 32 bytes**
long. `TOOBAD BIOS v2.5.2` is four characters longer than before, and
with the old `.space 17` the header would have grown to 36 bytes — the
code would have started at 0x34 instead of 0x30 and the machine
wouldn't have booted at all. Now `.space 13`, recomputed: 19 + 13 = 32.
Verified: name is read, code sits at 0x30 again, image valid.

**And the Coder was stuttering.** `edg_ist_bios()` searched 3000 bytes
on EVERY redraw — around 30,000 instructions, while a whole frame at
2 MHz only has about 33,000. That search alone was eating up the frame
time. Now the result is remembered and only re-searched when the length
changes, and only within the first 400 bytes — the tag can't legally
sit further back anyway.

---

## Clicks, Window Bounds, and a Cancelable "New"

**Two more overflows, shown by Colin with screenshots:**

- In the file dialog, **`Cancel`** stuck out of its button: six
  characters are 48 points of text, the button was 44 wide.
  `g_button` only centers, it doesn't truncate. Now it's computed
  instead of guessed: 8 points margin, Cancel 56, OK 44 — and the name
  field shortened accordingly. Drawing and clicking now use the same
  numbers.
- In the Coder's status line, the green **`saved`** sat at a fixed
  column, 560. With 588 points of space that's three characters — the
  rest ran under the `?` button and out of the window. Now it's
  right-aligned in front of the button, and the byte count yields as
  long as a message is showing. Both share the same space, as the
  comment always said.

A script also now checks every fixed button label against its width —
none was too wide after that.

**The Command Prompt could no longer be clicked** once a window sat
behind it. Cause: `draw_desktop()` draws by window number, with
`win_top` last — the higher number ends up on top. The click search
ran **forward**, though, and took the first hit, i.e. the window
BEHIND it. Now it runs backward, in the same order as the drawing.
Reproduced: three windows, the middle one on top, click in the overlap
of the other two — previously the back one won, now the visibly
front one does.

**Text stuck out of message windows.** Compiler lines run about 50
characters, the progress window was 320 points wide. New:
`g_text_max()`, which truncates to the window width and ends with two
dots; used in compiler messages, the help window, and the firmware
confirmation. And the narrow progress-bar window becomes a proper
message window on error (520x240 instead of 320x90).

**Found a wrong hit area, mechanically:** a script checked every
`g_button()` against every `treffer()`. The *Save to CMOS* button in
Control Panel is 96 points wide, but only the row was checked, no `mx`
— a click on the temperature display next to it wrote to CMOS.

**"New" can now be cancelled.** It used to create an empty document
immediately and only ask for the location afterward; anyone cancelling
had lost their work and an unnamed file left open. Now it asks first,
and the document is only created once a location has been chosen — in
Paint, Word, and the Coder. In the Coder, the write window doesn't even
open without a location; the start page stays up.

---

## New File Asks for the Location Right Away

Colin's wish: when creating a new file — in Paint, Word, or the Coder —
the location window should appear immediately. After that you're
"straight in," and "Save" simply saves the current state.

That's exactly how it works now. `New` opens the file dialog in save
mode with a suggestion (`PICTURE.TBI`, `DOCUMENT.TBW`, `NEW.C` ...).
From then on the program remembers that the name and folder are fixed
(`pt_ort`, `wd_ort`, `edg_ort`), and `Save` writes without asking
again.

The location is also fixed after **opening** — after that, `Save`
writes back there instead of asking every time. Before, it asked
without exception every single time, popping up a window on every
other keystroke.

---

## The TB-32 Now Builds Its Own Firmware

Colin's wish: write a BIOS in the Coder, with a template and
instructions, test it once with no risk, and only flash it permanently
once it works — with a confirmation from the BIOS itself.

**The blocker sat deeper than expected.** `ASM.TBX` couldn't build a
BIOS: no `.org`, no `.equ`, no `.include`, 256 symbols (const.inc alone
has 158). On top of that, three things turned up while trying it out
that nobody had anticipated:

- **Expressions were cut apart at spaces.** `next_token()` only
  returned `IVT_BASE` from `IVT_BASE + IRQ_TIMER*4`; the rest fell away
  silently and the interrupt table ended up full of zeros. Now
  `next_arg()` fetches the whole argument up to the comma.
- **Operator precedence was missing**, and so were parentheses —
  `(SCR_H-1)*SCR_W*2/4` genuinely appears in video.asm.
- **Local labels had no scope.** `.copy` was the same label everywhere;
  the assembler translated it without complaint, and jumps landed in a
  different function. Exactly the kind of bug that only surfaces once
  a finished BIOS no longer boots.

Plus `ldwa`/`stwa` and 512 symbols. **Result:** the device builds
`minimal.asm` to 3356 bytes — **byte for byte identical to the Mac's**,
except for the eight header bytes the Coder stamps itself.

**New in the Coder:** *New → BIOS* with a ready-made template, a `?`
button with the short version of [[16 Eigenes BIOS schreiben]] on the
device, and **Test** and **Flash** at the bottom.

**The one-time boot is the core of it.** The image lives in the board,
not on disk: the next boot uses it, the one after that uses the real
chip again. The boot screen shows `TEST IMAGE -- runs once`. A test
image that hangs costs nothing more than a press of Ctrl+R.

**When flashing permanently, the firmware asks for confirmation
itself**, in red, before the self-test. The Coder can only register an
image — a program must not be allowed to decide alone that the chip
gets overwritten.

**Four bugs along the way, all in [[07 Fallstricke]]:**

1. `#define NAME value /* comment */` pulled the comment into the
   value. Whoever mentioned `NAME` in a comment got a `*/` inserted
   into it and the rest was read as source code. Fixed in
   `tools/tcc.py`.
2. **I destroyed `programs/asm.c` myself** — `s[:i] + new + s[j:]` with
   `j == -1` cut the file from 646 down to 434 lines. No backup, no
   git. The missing part was rewritten from `tools/assembler.py` and
   `hardware/isa.py`; luckily the instruction table was intact.
3. 308 bytes of strings in the middle of the BIOS — without `.align 4`
   every instruction after them was misaligned, and the machine died
   15 instructions after reset.
4. The C version of the emulator answered the question "is a flash
   request pending?" with a 2 instead of 0 — and showed the red
   confirmation on boot even though nobody had registered anything.

**Addendum after Colin looked at the finished Coder:** the BIOS
template was in German (`einstieg`, `hochfahren`, `; clear BIOS data`)
— it appears on the TB-32's screen and therefore belongs in English.
Translated, labels included. While searching, six more German strings
turned up: `"Datei"` as the file dialog's window title,
`-- Taste druecken --` in lib.c, and four in Word (`Bild einfuegen`,
`Bild loeschen`, `Als Text speichern`, `Seite`). All now in English.

**The button bar now goes by the kind of source file.** At first I only
split between BIOS and everything else; Colin spotted the sharper cut:
a `.PY` isn't compiled at all, it just runs. `Build` would have set the
**C compiler loose on Python source** there — nothing but an error list
could come of that.

| | Build | Run | Test | Flash |
|---|---|---|---|---|
| C / Assembly | yes | yes | -- | -- |
| Python | **no** | yes | -- | -- |
| BIOS | no | no | yes | yes |

Firmware is recognized by the `TBBI` tag in the header, which a BIOS
needs anyway — so there's no mode you could forget to switch. The
buttons move together when one is missing; drawing and clicking both
query the same function `cb_pos()`, so the two can't drift apart again.
Recomputed, the bar fits in all three cases: 436, 382, and 454 of 588
points.

**Confirmed afterward:** the background search did turn up copies of
`asm.c` after all (in `~/Desktop/Projekte/PyPC Kopie` and in the iCloud
trash). Comparing against the original shows: no function is missing,
only `parse_mem` is now called `mem_operand`, and both encoding and
jump computation behave identically. As a further check, HELLO.ASM was
compiled on the device and compared against the Mac — **171 bytes,
byte for byte identical.**

Tests: **62/62** instead of 55/55 — new ones are fetching buffers from
RAM, the one-time boot running with the chip left untouched, the
following boot using the real BIOS again, the firmware asking in red,
writing nothing until then, and ENTER burning it. Plus 11/11 compiler
tests and both emulators computing instruction for instruction
identically again.

---

## A BIOS That Doesn't Know Backspace — and the German Recycle Bin

**Colin's screenshot:** `A:\> fff` with three gray boxes behind it.
Every press of the delete key added another one, the letters stayed put.

**Not reproducible at first, and that was the clue.** With the large
BIOS, the key deletes cleanly — through `pc.py` just as much as
headless. Only with **his** BIOS in the chip did the bug appear, and
right away: three presses, three boxes.

**Cause:** Colin wrote his own screen routines instead of including
`video.asm` — which is also why his BIOS is only 3 KB instead of 3.3.
His `scr_putc` catches exactly one control character, 10. The 8 that
`readline` sends for deletion fell into the normal branch and ended up
as a character in the screen buffer. CP437 renders 8 as "◘".

**The tricky part:** the buffer in memory was correct the whole time.
After the three backspaces and ENTER, no error message came, just a
fresh prompt — the shell had dutifully executed the empty command.
**Only the screen was lying.**

**And the docs were to blame.** [[16 Eigenes BIOS schreiben]] listed
every function number and every register — but nowhere did it say that
`putc` has to intercept 8, 9, 10, and 13. Colin couldn't have known.
It's now written up as its own table, and as a trap in
[[07 Fallstricke]].

**Fixed in Colin's `colinbios.asm`** (at his request): backspace,
carriage return, and tab added. The header's name field was filled in
at the same time — the boot screen now shows `ColinBIOS 0.2` instead of
`UNNAMED BIOS`. Verified: three backspaces, three letters gone.

**The recycle bin is now called `\RECYCLED`.** It used to be
`\PAPIERKORB`, and that was the one German name that showed up on the
TB-32's screen — against the project's own rule from
[[05 Konventionen]]. Windows 95 called its own the same thing. The
existing folder on Colin's disk was renamed along with its contents,
not recreated.

---

## The Boot Screen Belongs to the Board — Only the Name Comes From the BIOS

Colin wanted a fixed sequence at power-on, **the same for every BIOS**:
blue slowly runs from top to bottom, the name appears in the center,
below it the hint about DEL, then the machine boots. And the name
should be whatever *each individual BIOS* sets.

That's exactly how it is now — and the split is the interesting part:

| | who does it |
|---|---|
| Blue top to bottom, screen center, DEL line, the five seconds | **the mainboard** (`pc.py`) |
| the name in the center | **the BIOS**, in its image's header |

**The header has grown from 16 to 48 bytes.** At 0x10 there are now 32
bytes of name, terminated with a null byte; the code starts at 0x30.
The board reads it before the CPU even has power (`Machine.rom_name`).
If the field is missing — as in Colin's ColinBIOS at the time — the
board shows `UNNAMED BIOS` instead of guessing. Checked strictly:
printable, ends with a null byte, only null bytes after that.

**Why the picture doesn't belong in the firmware:** a boot screen in the
BIOS is gone exactly when someone flashes their own — and then there's
also no place left to press DEL. In the board, no BIOS can lose it.

The sequence, measured in real `pc.py`:

| Time | Picture |
|---|---|
| 0.4 s | 7 of 25 lines blue |
| 0.9 s | 18 lines |
| 1.7 s | fully blue, `TOOBAD BIOS v1` in the center |
| 2.4 s | `Press DEL to enter SETUP` below it |
| 5.0 s | power reaches the board |

With Colin's BIOS, `UNNAMED BIOS` shows in the same place — everything
else identical. That was exactly the wish.

**The hint lines in the bottom left are gone**, as requested. Powered
off now really just means black.

**What still doesn't work, and fundamentally so:** the board can buy
time, but not conjure up a menu. What happens on DEL is up to the
firmware. Colin's BIOS has no setup, so DEL does nothing there —
verified. For that, a setup within the BIOS itself is needed.

---

## The Grace Period Belongs in the Housing, Not the Firmware

Colin wanted a five-second grace period **for every BIOS**, stored in
Python. The reasoning behind it is sound: a grace period living in the
firmware is gone exactly when you flash a custom BIOS — and then you
can no longer get into setup. His ColinBIOS jumped straight into the
boot sector, and he landed in the console with no stopover.

**Now the housing halts the machine for five seconds**
(`EINSCHALT_HALT_S` in `pc.py`) before any power even reaches the
board. No BIOS can skip this, not even by accident. This applies to
program startup the same way as to the `ü` button.

**Keys pressed during the grace period aren't lost.** They're buffered
and handed to the machine as soon as the CPU runs. Only then — a
keyboard interrupt fizzles out while the CPU is stopped; the key would
sit in the chip and nobody would pick it up.

Verified with both BIOSes in real `pc.py`: still off after 1 s and
after 4 s, running after 6 s. With the large BIOS, a DEL during the
grace period opens **setup**. With ColinBIOS, nothing happens — as it
should, since there is none there. A grace period can buy time, but not
conjure up a menu.

---

## Powering Off Now Really Means Off, and Booting Up Takes Time

**Two wishes from Colin, both from the same idea:** the machine should
feel like a real one.

**The screen turns black on power-off.** Previously the last picture
stayed up with a red bar reading "machine powered off" on top. That was
practical, but wrong — a monitor on a powered-off machine shows
nothing. Now it's really black, with only a small
`off -- ü = power on` at the bottom.

**`ü` is the power button.** It only works while the machine is off,
and **not instantly**: it stays dark for 1.1 seconds first, the way a
real device sits between button press and first picture. During that
second, the hint doesn't show either — the screen is simply black.

**The self-test takes its time.** That was the actual point: the whole
POST used to finish in **16 milliseconds**. All six lines were already
on the first frame — you saw nothing of the boot process at all. Now:

| | before | after |
|---|---|---|
| POST complete | 0.02 s | 1.5 s |
| until prompt | 2.3 s | 3.95 s |
| with Quick Boot | | 0.58 s |

The memory test now **visibly counts up** — 512 KB every 15
milliseconds, from 512 to 16384 — and there's a fifth of a second
between checks (`post_pause` in `firmware/bios.asm`).

**Turn-off-able, via the setting that already existed for it.** With
*Quick Boot* enabled, every pause is skipped. That's exactly what the
switch is for, and exactly how every real BIOS does it.

**Nearly stepped into a documented trap.** The `ü` check first sat with
the key-press events (`KEYDOWN`, `event.unicode`) — and not twenty
lines above that, `pc.py` has a comment saying `unicode` is empty
there depending on layout, or still carries the previous keystroke's
character. Umlauts only arrive reliably via the text-input event. Now
it sits at `TEXTINPUT`, where it belongs.

**Verified with faked events in real `pc.py`:** runs after startup, is
off after `power_off`, stays dark during cold start, runs again after
`ü`, and boots again up to the prompt. Five for five.

The self-test's wait times had to move with it — it now checks POST
after 2.0 s instead of 1.2 s and the boot process after 3.0 s instead
of 2.5 s. Back to **55/55** afterward, and both emulators still compute
instruction for instruction identically.

---

## The BIOS Has Become Replaceable

**The question that started it:** Colin had deleted `\SYSTEM\BIOS.BIN`
and wanted to know why nothing happened. Answer: because the BIOS lives
on a chip, not on the disk — the file is just a visible copy. That's
how it is on a real PC too, and it should stay that way.

But there is a way to actually destroy a BIOS: **flashing**. Colin's
suggestion was to do exactly that — pick a file from the real Mac in
setup. That's the right approach, and for a reason that isn't obvious:
the chip is hardware. Handing in a file from the host means "a
different chip goes on the board" — the emulator is allowed to do that,
because it *is* the mainboard. A program *inside* the TB-32 writing to
the chip it's currently fetching its own instructions from would be the
bad approach.

**New — Setup, "Firmware" tab.** Shows the size and checksum of the
chip currently running, and has two buttons: *Flash BIOS from File* and
*Restore Backup BIOS*. The first opens macOS's file dialog
(`osascript`, in `pc.py`).

**New — the chip as a device** (`Flash` in `hardware/devices.py`, ports
0xB0-0xB2). Deliberately dumb: it checks **nothing** and accepts every
byte, like a real flash chip. What's valid and what isn't is decided by
the firmware.

**New — a header in the image.** The first 16 bytes: jump, `TBBI` tag,
length, checksum. `build.py` fills in the last two
(`bios_kopf_stempeln`).

**Three safety nets, at three different points in time:**

1. When flashing, the firmware checks the tag and checksum and doesn't
   write at all otherwise (`bios_pruefen` in `firmware/setup.asm`).
2. At power-on, the **mainboard** rechecks and otherwise restores the
   backup automatically (`Machine.rom_pruefen`). This is Dual BIOS, and
   it deliberately sits there: **broken firmware can't check itself.**
3. Anything that's formally valid and still hangs is recovered via
   *Restore Backup BIOS* or `python3 build.py`.

**New — `firmware/minimal.asm`.** A complete BIOS in **3324 bytes**
versus 12216 for the full one: no boot screen, no setup, no Secure
Boot, no memory test. It boots the machine, and TOOBAD-OS runs on it —
verified. This is the template to rebuild from. It's this small because
the system does almost everything itself: graphics, blitter, mouse,
sound, and heat all go through `inr`/`outr` directly to the ports, with
no BIOS at all.

**New — [[16 Eigenes BIOS schreiben]].** The complete contract: the 16
bytes of header, the interrupt vectors, all four services with every
function number and every register. Exactly the file Colin had asked
for.

**Fixed — a semicolon in a string ate half the line.** The assembler
discarded comments with `zeile.split(";")[0]`, without paying attention
to quotes. From `.db "A bad image is refused; keeps a backup", 0` it
silently produced `.db "A bad image is refused` — no null byte, no
error message, and the output ran into the next string. On screen it
looked as if the machine had asked itself a question. Fixed in
`tools/assembler.py` (`ohne_kommentar`).

**Fixed — `vid_puthex` without a digit count in `r3`** printed the
value hundreds of times and filled the whole screen with `5B03E1E0`.
Looked like an infinite loop, was a missing argument.

**Tested, not just claimed.** Six new checks in the self-test, all on a
**copy** of the chip: a damaged image is rejected and the chip stays
untouched, a good image gets burned, a backup is created, the machine
boots with the self-flashed BIOS, and a destroyed chip gets
automatically replaced from the backup at power-on. **55/55** instead
of the previous 45/45.

The three ports also exist in `emu/` — the C version can't open a file
dialog and always reports "no file," but the port numbers have to be
the same. Both emulators still compute instruction for instruction
identically.

---

## The Boot Sector Now Reads the File System

**The question this grew out of:** Colin had deleted all files,
including the ones in `\SYSTEM` — and the machine kept booting.
Rightly suspicious: *"shouldn't it crash on a reboot then?"*

**Why it kept running.** The kernel sat on fixed sectors starting at 1,
outside the file system. The kernel size was stored in the boot sector
at position 506, and that's all it needed to know. `KERNEL.BIN` in
`\SYSTEM` was pure decoration — you could look at it, copy it, delete
it, nothing changed. An operating system whose system files are
dummies.

**Now the boot sector actually searches for the file.**
`system/boot.asm` reads the directory (sectors 513..520), looks for the
`SYSTEM` folder in the root directory, `KERNEL.BIN` inside it, takes the
start sector and size from the entry, and loads exactly those sectors.
Fixed kernel sectors no longer exist; `build.py` only writes sector 0.

Verified, both cases:

- `KERNEL.BIN` deleted, restart → `\SYSTEM\KERNEL.BIN missing`, the
  machine halts. Just like a real machine with no operating system.
- `python3 build.py`, restart → boots again.

**The trick is the 512 bytes.** Searching a directory, with name
comparison and folder checking, in a single sector: 483 bytes used, 29
free. Only possible because TBFS stores files **contiguously** — start
sector and size are enough, there are no block chains to follow.
Savings were made deliberately in two places:

- Names are compared as **three 32-bit words** instead of byte by
  byte. Two words would have been too few: `KERNEL.BIN` and
  `KERNEL.BAK` are identical in their first eight characters.
- The **superblock isn't checked**. The sanity check would have cost
  seven instructions and its own error message, and without a file
  system the name search right after would find nothing anyway.

**Secure Boot had to move along with it.** The firmware computed its
checksum over the boot sector plus the fixed kernel sectors. Had that
stayed as it was, it would have been measuring bytes that nobody boots
anymore — a check that never triggers and still looks like security.
`secure_summe` in `firmware/setup.asm` therefore searches for **the
same file** as the boot sector, via `kernel_finden`. That the search
appears twice is unavoidable: the boot sector can't call into BIOS
internals.

Verified with three runs: correct checksum → boots through; one byte
of the remembered checksum flipped → red SECURE-BOOT screen; **one byte
in the kernel file flipped → red screen.** The last case is the actual
point. The expected checksum was independently recomputed in Python and
matched bit for bit (`0xF61B29C2`).

**When deleted, the kernel moves to the recycle bin** — the boot sector
doesn't find it there, because it checks the parent folder too. So the
machine no longer boots, but the bytes are still there.

**Addendum — the messages were in German.** Colin saw it on screen:
`Bootsektor: lade Kernel ... \SYSTEM\KERNEL.BIN fehlt` in the middle of
an otherwise English boot screen. Now `Boot sector: loading kernel ...
OK` and `... \SYSTEM\KERNEL.BIN missing`, matching the
`Booting from Hard Disk 0 ... OK` line above it. 488 of 512 bytes.

**Rule of thumb:** comments and docs in German, everything that ends up
on the TB-32's screen in English. It's easy to slip up with a freshly
written piece of code.

Tests afterward: 45/45 self-test, 11/11 compiler tests, bootstrapping
passed, C and Python emulator instruction for instruction identical.

---

## File Selection Window, Recycle Bin — and Two Visible Bugs

**New — a file selection window for everyone** (`system/dialog.c`).
Until now, every program had a text field for the file name: you had
to know exactly how the file was named and where it lived. That's the
state of the art from 1981.

Now there's ONE window that the Coder, Paint, and Word all use. It
shows the folder, lets you click into it, has an Up button, a name
field, and OK/Cancel. The return path runs through `dlg_ziel`: the
window remembers who asked and calls the matching function there — no
program has to wait around for a result.

**With a filter.** Paint only sees `.TBI`, Word only `.TBW`, and
*Insert Image* in Word only images. Folders are always shown — you
have to be able to navigate into them.

**New — the recycle bin.** `DEL` now moves to `\PAPIERKORB` instead of
destroying. Only deleting THERE actually deletes. The folder is
created on first use. This isn't a convenience feature: Colin had once
wiped the entire disk by accident in one evening, and no `build.py`
brings back your own source files.

**New — deleting images in Word.** An image is a whole paragraph; with
backspace you'd have been nibbling away at the characters of its file
name. Now Delete or Backspace on a selected image deletes the whole
paragraph, file name and line break included.

**Fixed — the clock painted over other windows.** Once a second,
`app_clock()` drew its content straight onto the screen, ignoring
window order. The time then showed up in the middle of Control Panel.
Now it requests a normal redraw — and the desktop knows the order. The
same applied to the System Monitor.

**Fixed — no key got through after the dialog.** `win_top` pointed at
the closed dialog window. On close, the program that asked now gets
the focus back.

**Renamed — Paint's "Pic" is now called "Get."** It was never for
images, it's the eyedropper: pick up a color from the image and keep
painting with it. The old name read like "Picture."

---

## Word: Lists, Pages, and "Printing" — and Three Bugs Along the Way

**Lists.** Bullets (a drawn box) and numbering. The number counts back
to the first paragraph without one — so every new list starts at one
again. A new paragraph inherits the previous one's list: you just type
a list straight through. Wrapping accounts for the indentation, the
marker only appears on the **first** line of a paragraph.

Technically: a **second style byte per paragraph** (`WD_ABS2` at
`0x00730400`). In the file format it's appended at the very end —
older documents have nothing there and simply load without lists,
instead of breaking.

**Real pages.** The line-wrapping pass splits the lines across pages in
a second run (`WD_SEITE_H` = 620 points). While drawing, a divider with
the page number appears at the boundary. The wrap list gained a fourth
column for this.

**"Printing."** The machine has no printer — so there's the file
instead. *Save as Text* in the right-click menu writes plain text:
styles fall away, list markers are spelled out (`- ` and `1. `), images
appear as `[Image: NAME]`. That lets a document be viewed with `TYPE`
or opened in the Coder.

**Fixed — scrolling ran into nothing.** PgDn moved the start forward by
a fixed number of lines and could land past the end of the text: a
blank page. Now it counts the **actual line heights** (a heading in
size 3 takes up three times the space of normal text, an image even
more) and stops so the last line sits at the bottom.

**Fixed — the right click never reached the TB-32.** `pc.py` read the
mouse buttons with `pygame.mouse.get_pressed()` instead of from the
event. Depending on the platform, that still returns the old state on
release — and the right button fell through entirely. Now the state is
tracked from the events (`e.button`: 1 left, 2 middle, 3 right). On top
of that, **Ctrl+click counts as a right-click** on the Mac, since
that's the convention there anyway and the only option on some
trackpads.

**New — the system now visibly lives in `\SYSTEM`.** `KERNEL.BIN`,
`BIOS.BIN`, and `KERNEL.SYM` are now placed on the disk while building.
They're the same bytes as in the reserved sectors; booting still
happens from there, not from these files. But now you can see them,
view them with `DUMP`, and copy them.

**Verified — what happens if you delete everything?** Colin had
deleted all files including `\SYSTEM`, and the machine kept running.
Tried it with an empty file system: **it still boots after a
restart** — the kernel sits in sectors 1-318, the file system only
starts at sector 512. Deleting can't touch it. Only the files are
gone; `python3 build.py` restores the system, but not your own source
files. That's exactly why the recycle bin comes next.

---

## The Desktop No Longer Flickers — and Is Now Called Coder

**Fixed — everything flickered.** The desktop drew directly into the
*displayed* screen buffer. Every half-finished drawing was immediately
visible: while dragging a window, opening a menu, worst of all while
painting in Paint.

The fix was already there in the hardware — the second screen page
that Flappy has used since this morning. The desktop now gets it too.
For this, port `0x53` needed a second mode:

| Value | What |
|---|---|
| 1 | **Swap** pages — fast, but the new back page has the second-to-last picture. For games that redraw everything anyway. |
| 2 | **Copy** the back page onto the front page — the back page stays as it was. Exactly what the desktop needs, since it usually only redraws ONE window. |

Built into **both** emulators (Python and C). The desktop switches on
the second page at startup, copies it to the front at the end of every
round, and switches it off again on exit. While a fullscreen program is
running, the page belongs to the program — afterward the desktop takes
it back.

**Fixed — Paint painted onto the desktop.** The preview of a large
circle stuck out past the window edge. The shape itself was clipped
correctly (`pt_tupfen` checks bounds), but its **preview** wasn't — that
went straight to the screen with `g_frame`. Now there's
`pt_rahmen_begrenzt()`, which stops at the canvas edge.

**Sped up — Paint redrew the whole window while dragging.** Toolbar,
buttons, palette, and file name on every mouse move. Now only the
canvas goes to the front — a single blitter command
(`pt_leinwand_malen`).

**Renamed — "Editor" becomes "Coder."** Colin's wish: the Coder is for
programming, texts and notes are written in Word. The "Notes / text
.MD" entry has disappeared from the start screen, leaving C, Assembly,
and Python. Existing files can still be opened — you'll want to peek
at a README while programming sometimes too.

**Verified:** 45/45, 11/11, emulator comparison (screen identical
character for character).

---

## The Emulator in C — Step 1 on the Path to the Pi

`emu/` alongside `hardware/`: the same machine, just no longer in
Python. `emu/cpu.c` (all 57 instructions), `emu/machine.c` (bus,
graphics card with blitter, disk, keyboard, timer, CMOS, block copier,
heat), `emu/main.c` (headless startup for comparison).

**TOOBAD-OS didn't change by a single byte in the process.** The TB-32
stays the processor — only what emulates the chips gets swapped out.
That's exactly the difference from a port to ARM, where the TB-32 would
disappear.

**Speed:** 1.8 → **287 million instructions per second**, a factor of
**160**.

**New test: `tools/emu_vergleich.py`.** Two emulators of the same
machine are only worth something if they compute exactly the same
thing. The test has both versions execute individual instructions and
compares the program counter and flags after each one; afterward it
checks the entire boot process character for character. It paid for
itself immediately — three porting bugs, all found within seconds:

| Bug | found at |
|---|---|
| `cmp`/`cmpi`/`tst`/`tsti` compare **rd**, not ra | step 13 |
| `jmpr`/`callr` jump via **rd**, not ra | on the jump into the boot sector |
| `IRQ_TIMER` is **0x08**, not 0 | "division by zero" while booting |

The last one is the most instructive: the IRQ numbers in `isa.py`
**are already the interrupt vectors**. Set 0/1/2 there, and the timer
jumps to vector 0 — which is called "division by zero." The BIOS
dutifully reported exactly that.

**Status:** the C emulator boots TOOBAD-OS, the screen matches
character for character, `dir` works. No window yet — that comes with
SDL in the next step, together with the Pi.

---

## Word: Clipboard — and the Same One as Everywhere Else

Copy, Cut, Paste with Ctrl+C/X/V or via the right-click menu. Word uses
**the same clipboard as the editor** (`CLIP_BUF` at `0x130000`) — text
therefore moves between both programs. And since `pc.py` mirrors it to
the Mac clipboard, also between the TB-32 and the Mac.

The **colors** go into a small separate store next to it
(`0x00760000`). As long as the length stays the same, pasted text keeps
its original color — so copying within Word keeps its color, while
pasting from the Mac comes in black. That's exactly what you'd expect.

**Verified:** "ABC " selected, Ctrl+C, Ctrl+V twice → "ABC ABC ABC ".

---

## Word Can Now Select, Color, and Embed Images

Colin's wish: select text, right-click, color it — and insert Paint
images and resize them. All four are in.

**Selecting** with the mouse: the mouse position is first turned into
the screen line, then the column via the font size. The selection is
drawn inverted, typing replaces it, backspace deletes it.

**Right-click.** The mouse reports bit 0 left, bit 1 middle, **bit 2
right** — the desktop used to discard this and treated every click the
same. Now it remembers in `gui_taste` which button it was. Word opens
a menu on it: six colors, Select All, Clear Selection, Insert Image.

**Text colors** were the actual model change. The style used to sit
**per paragraph** — but a colored selection needs **one byte per
character**. So there's a second buffer next to the text
(`0x00728000`), which moves along on every insert and delete. While
drawing, the line is split into same-colored sections — the same
technique as in the Coder.

**Images as their own paragraph.** Its text is the file name, its
style byte carries the `WF_BILD` flag, its size sits in two of its own
fields. Wrapping treats it like a very tall line, with text running
above and below it. Clicking selects the image, dragging the handle at
the bottom right resizes it. If the file is missing, that's shown
honestly in the frame instead of crashing.

**New in the hardware — blitter command 7: scaled image.** Command 4
only draws images 1:1. For arbitrary sizes the card now computes using
nearest-neighbor: the step size is fixed as a fraction of source and
target size, entirely without floating point — that's exactly how
graphics cards have scaled forever. Source size in the CHR register,
target size in W and H. Measured: **1.7 ms** for 320x240.

**Verified:** a sentence typed, selected with the mouse, colored red via
the right-click menu, an image inserted (160x100 scaled to 200x130),
dragged to about 300x200 via the handle — text keeps flowing around
it. 45/45, 11/11, bootstrapping.

---

## Word — Word Processing as a Window on the Desktop

Start → Word. `system/word.c`.

**The difference from the editor is the model, not the interaction.**
The editor knows lines, the way they sit in the file. A word processor
knows **paragraphs** — where a line wraps is decided by the page width,
not the Enter key. So there are two layers stacked here:

* the text as a continuous buffer, paragraphs separated by line breaks
* a **style byte** per paragraph: size (1-3), bold, underlined,
  alignment

From this, a **line-wrap pass** is computed on every change — a list of
screen lines with start, length, and paragraph. Breaks happen at
spaces, not mid-word.

**What works:** three font sizes (8, 16, 24 points), bold, underlined,
left/center/right, word wrap, scrolling, New, Save, and Open in its own
format **TBW** (length, paragraph count, style bytes, text).

**How bold is done:** the same text drawn again, shifted one point over
— exactly the trick dot-matrix printers used to use for bold. There's
no separate bold font.

**Honest limitation:** the font has **fixed width** and can only be
scaled by whole numbers. Real proportional type would be its own
project — it would need a hand-drawn second font with a width table.

**Extended:** blitter command 6 (string) can now also scale up — a
heading at 24 points is thus a single draw command.

**Verified:** a heading in size 3 centered, a paragraph in size 1 with
automatic wrapping, a subheading in size 2 underlined; save, clear,
reload — all formats come back. 45/45, 11/11, bootstrapping.

---

## Coder — the Editor Becomes a Tool for Programs

The editor on the desktop can now do what a code editor needs to:

* **Line numbers** in their own column on the left
* **Syntax colors** for C, Assembly, and Python: keywords blue, strings
  green, comments gray, numbers magenta, preprocessor brown. The
  language follows from the file extension.
* **Search** (*Find* button, Ctrl+F, next match with F3 or Enter), case
  insensitive, wraps at end of file
* **Automatic indentation:** the new line inherits the indentation of
  the previous one, two more levels after an open curly brace
* **Jump to error line:** if compilation fails, the cursor jumps to the
  line the compiler reported

Source: `system/coder.c`.

**New in the hardware — blitter command 6, "string."** Previously the
operating system sent one draw command per character; an editor page is
1600 of them. Now the blitter fetches the text from memory itself:
address in the CHR register, length in the W register, the font stays
where it is. `g_text` throughout the desktop now goes through it, and
the editor only draws the **color sections** per line.

**How the coloring stayed fast.** The first attempt cost **476,000
instructions** per keystroke — a sixth of a second, typing felt dead.
Three things:

1. Colors only depend on the text and the visible range, not the
   cursor. The color buffer is now only rebuilt when one of those two
   changes — arrow keys, mouse movement, and clicks now cost nothing at
   all.
2. Keyword lookup checked the *entire* list for *every* word. Now the
   first letter decides first.
3. Pointers instead of function calls per pixel, and strings instead of
   individual characters through the blitter.

Result: **476,000 → 190,000** instructions per redraw.

**Two bugs along the way:**

* I measured too early again. The window stayed empty, all the
  counters looked wrong — in reality the picture just wasn't finished
  drawing yet. The same bug as with the fill tool in Paint, two hours
  later. Written up in [[07 Fallstricke]].
* The color section was positioned via `column - length`. For lines
  wider than the window, `column` kept counting anyway — the last
  section ended up drifting further and further right, line by line.
  Now the start column is remembered directly.

**Verified:** CALC.C opened (colors, line numbers), searching for
"rechne" finds the spot, indenting after `{` sets two levels, 45/45,
11/11, bootstrapping.

---

## Paint — the First of the Three New Programs

A drawing program as a **window on the desktop** (start menu → Paint).
Tools: pen, eraser, line, rectangle, filled rectangle, circle, fill,
eyedropper. Plus three stroke widths, 32 colors, New, Undo, Save, and
Open in its own format **TBI** (width, height, then one byte per
pixel). Source: `system/paint.c`.

The canvas does **not** live in the screen buffer, but in RAM at
`0x600008`. A window can be moved or covered — if the picture lived
directly on screen, it would end up broken. It reaches the screen with
a single blitter command.

**New in the hardware — block copier (DMA), ports 0x56-0x5A.** It moves
memory around without the processor: 256 KB in 0.03 ms instead of a
third of a second. Undo and "Redo" run through it. Plus three
**search commands** over whole blocks, the counterpart to real
processors' string instructions. Details in
[[02 Speicherkarte und Ports]].

**Sped up — image from RAM (blitter command 4).** Ran pixel by pixel
through Python: **9.5 ms** for 400x300, i.e. half the frame time. Now
it goes line by line, and only lines with a transparent pixel are
handled individually: **0.06 ms**, 158 times faster.

**The desktop now knows dragging.** Until now there was only clicking.
A drawing program needs the whole movement and the moment of release —
line, rectangle, and circle are only drawn to the screen while
dragging, and only committed to the canvas on release.

**Two of my own blunders, written down so they don't come back:**

* The blitter reads the image source from **the same register as the
  font**. Without `sys_out(P_BLT_SRC, ...)` beforehand, Paint drew the
  font as the canvas — black with noise. And afterward you have to
  reset it, or the whole desktop starts drawing its text from the
  image.
* A text substitution with no count limit copied the double-buffer
  methods into **every** device class (nine of them). Found while
  searching for something completely different.

**And the most expensive lesson:** the fill tool looked broken — it
only filled three lines. I spent an hour hunting for a bug in the
queue, logging counters, splitting the function apart, moving the
stack into RAM. It was **never broken, just slow**: it was still
computing while I was measuring. Written up in [[07 Fallstricke]].

**Verified:** Paint operated by hand (pen, line, rectangle, circle,
fill, colors, stroke widths), 45/45, 11/11, bootstrapping.

---

## The Machine Hit 65 Degrees During Flappy and Throttled Itself

Colin's report: "the PC just froze during Flappy." A hard freeze
couldn't be triggered in 200 seconds of play — the clock kept running,
the picture kept changing. But something else turned up that feels
exactly the same:

**The frame clock never let the processor sleep.** `proc_next()`
returns the current process itself when nobody else is ready to run.
That made `proc_sleep()` ineffective with a single running program:
sleep, wake immediately, sleep again ... 100% load. The TB-32 heated up
to 65 degrees and throttled to 40% clock — and a throttled system
reacts sluggishly to everything, including ESC.

Found by sampling the instruction counter: the hotspot was in
`proc_next`/`proc_schedule`, not in the game. Detailed in
[[07 Fallstricke]].

**Fixed:** `proc_sleep()` now waits out the remaining time itself with
`hlt`.

| | before | after |
|---|---|---|
| Temperature during Flappy | 65.1 °C | **26.6 °C** |
| Throttling | 60% | **0%** |
| Load | 100% | **10%** |
| Frame rate | 50 | 50 |

This applies to **every** program that sleeps — the whole machine now
stays cool while idle.

**Also fixed — double buffering only worked on the first boot.** On
power-off, the variable tracking the back page kept pointing at the
same page; on a game's second launch both screen pages were the same
one and flickering came back. The card now holds two fixed pages and
always picks the other one.

---

## The Graphics Engine Can Now Do What an Engine Needs To

Colin's report: "the count number flickers and doesn't sit on top of
anything properly, there's a blue box around it." Both were true, and
neither was a bug in Flappy — they were holes in the engine.

**Two screen pages (double buffering).** Until now the display read
along while drawing was happening — you saw half-finished digits. The
card now has two frame buffers: drawing happens into the invisible one,
`gx_zeigen()` swaps them. Ports `0x52` (on/off) and `0x53` (swap).

That eliminates the whole old juggling act — only erasing what moved,
counting the order of draw calls, having the display clear a little box
for itself. That exact box was Colin's blue box — the score display had
to cover its old position and punched a sky-blue hole into every pipe
behind it. Now Flappy redraws every frame completely, back to front,
and the digits simply sit on top (with a shadow so they stay readable
against green too).

**The card can scale by itself.** Port `0x54` sets the factor for
blitter command 3. Previously, `gx_gross` drew **a single digit as 576
individual points** at zoom 3 — that was the actual bottleneck of the
whole game, not the pipes.

**A frame clock.** `gx_takt(2)` holds 50 frames per second, regardless
of how fast the machine currently is. Without a brake, the game ran at
500 fps after the improvements — ten times too fast to play. Two
details it needed: the next timestamp is computed from the previous
one (otherwise the remainder of a fractional hundredth of a second is
lost: 40 instead of 50), and the wait uses `sleep(0)` instead of
`sleep(1)` — a whole hundredth of a second would be half a frame's
time.

Flappy's frame rate over the course of this session:

| | fps |
|---|---|
| Start | 9 |
| Blitter in one syscall | 39 |
| Direct ports + faster blitter | 53 |
| full redraw (still with pixel-by-pixel font) | 29 |
| scaling in the blitter | **500** |
| capped with frame clock | **50, steady** |

**New in `gfxlib.c`:** `gx_doppelpuffer(on)`, `gx_zeigen()`,
`gx_takt(hundredths)`. `gx_gross` and `gx_text_gross` now go through
the blitter.

**Found along the way:** a write to an unknown port is silently
swallowed (`bus.unknown_ports`). The new ports weren't registered in
`machine.py` on the first attempt — double buffering simply stayed off,
with no message at all. Whoever adds a port has to register it in
**three** places: `isa.py`, the device, and the device list in
`machine.py`.

**Safeguard:** a mode switch resets the zoom factor to 1, so the
desktop doesn't keep writing in giant text if a program crashes with
zoom set.

**Verified:** Flappy (50 fps, clean picture, display over the pipes),
the calculator, the desktop, 45/45, 11/11, bootstrapping — and `CC
CALC.C` **on the device**, so the compiler there also produces the new
ports correctly.

---

## Graphics: Programs Now Draw Directly, Without the Kernel

**New — the blitter belongs to programs too.** Ports on the TB-32
aren't protected, a program is allowed to operate them itself.
`gx_fill`, `gx_frame`, `gx_char`, and `gx_text` in `gfxlib.c` therefore
write directly to 0x44–0x4C instead of going through `int 0x40`. The
jump into the kernel cost saving 15 registers and a long case
statement — six port writes together are cheaper than that one jump.

For this to work identically in **both** compilers:

* `programs/prog_start.asm` gets `portout:` / `portin:` (for TCC on the
  Mac)
* `programs/cc.c` knows them as built-in functions 98 and 97 and emits
  `outr` or `inr` directly at the call site (`e_outr`, `e_inr`)
* `programs/proglib.c` only declares them now, with no body

Watch out with `outr`: the order is `outr <value>, <port>` — the port
sits in `ra`. My first attempt had them swapped, the screen stayed
completely black.

**New — `gx_text` writes color and line only once.** For twenty
characters that saves forty port accesses.

**Sped up — the blitter in the emulator.** Three things:

| | before | after |
|---|---|---|
| a single port access | 0.58 µs | **0.10 µs** |
| character 8×8, transparent | 9.90 µs | **1.52 µs** |
| character 8×8, with background | 9.90 µs | **2.53 µs** |
| area 52×120 | 17.08 µs | 12.51 µs |
| full screen 640×360 | — | 8.98 µs |

1. `port_out` had an `import` statement **inside the function body** —
   it ran on every single port access, and one draw command writes six
   of them. The same sat in eleven other methods (mouse, timer,
   keyboard, disk, …), all moved to the top.
2. The character blit ran through a Python loop 64 times. If the
   character is entirely on screen, it now sets eight ready-made
   eight-byte sequences; they repeat constantly and sit in cache.
   Transparent only writes the set pixels (`_GESETZT`).
3. Filling across the full width is a single write instead of one per
   screen line.

**Result, measured on Flappy:** 9 fps → 39 fps → **53 fps**.

**Verified:** Flappy from the command line, the desktop (unchanged
picture), and — the actual proof — `CC CALC.C` **on the device**: the
calculator compiles with the new `cc.c` and runs. Plus 45/45, 11/11,
and bootstrapping.

---

## A Process Slot Kept Its Previous Tenant's Marker

**Symptom:** after compiling in the editor, the started program
received **not a single key press**. The bird in Flappy dropped
immediately, ESC didn't help. Started from the text console, the same
program ran normally.

**Cause:** `p_bg[pid]` remembers "started in the background"; such
processes deliberately get no keyboard ([[07 Fallstricke]]). The marker
was set at startup, **never cleared**. The compiler runs in the
background, frees its slot — and the command line started after it got
the same slot along with the old marker. That made it count as a
background program, and every `getkey()` slept forever.

Measurable via the key buffer: `tail` grew with every press, `head`
stayed put — the key arrived, nobody picked it up.

**Fixed in:** `system/proc.c`, `proc_start()` sets `p_bg[i] = 0` when
assigning a slot, just like `p_wake` and `p_ticks` already did.

**Lesson:** whoever reuses a slot must reset *all* fields, not most of
them.

---

## The Desktop Painted Into a Running Fullscreen Program

**Symptom:** starting a program via **Run** in the editor left the
command-prompt window and the taskbar sitting in the middle of the
game's picture. Via double-click in the File Manager it didn't happen.

**Cause:** the desktop and the program run simultaneously. The rule
"while a program has the whole screen, the desktop draws nothing" was
only checked **at the top of the main loop**. If the program switches
mid-round, the desktop still draws the rest of that round — and since a
game only fills its background once at the start of a round, the
window's picture stayed put afterward.

**Two attempts that weren't enough:** moving the check to the end of
the loop didn't help at all; moving it to the start of
`draw_desktop`/`draw_window`/`draw_taskbar` only helped halfway —
`draw_desktop` draws many windows in sequence, and the switch happens
in the middle of that.

**Fixed in:** `system/gui.c`, `gui_fremd` is now checked inside the
draw calls themselves (`g_fill`, `g_frame`, `g_char`). That way the
desktop stops mid-sentence.

---

## `#include` Now Also Finds `\SOURCE` — and a Preprocessor Bug

**Fixed — the preprocessor swallowed source code.** An `#include` that
only appeared *inside a comment* was treated as a directive; the line
vanished along with the `*/`, and the now-open comment ate the next
lines of code. Both compilers were affected: `tools/tcc.py`
(`zeilen_im_kommentar()`) and `programs/cc.c` (`komm_folge()`). Detailed
in [[07 Fallstricke]].

That was the real reason `fileread_lib` always returned −1: system call
33 was in the source, but had never made it into the kernel.
`fs_find_in` had been fine the whole time.

**New — search path for `#include`.** Previously a source file had to
sit in the same folder as `proglib.c`, or compilation aborted. Now:

* The **main file** is looked up wherever you currently are
  (`fileread`).
* **Included files** are additionally looked up in `\SOURCE`
  (`fileread_lib`, system call 33, `fs_read_lib()` in `fs.c`).

This lets a program be compiled in any folder — even on the desktop —
as long as it writes `#include "proglib.c"`. The error message now also
says where it looked:

```
  cannot include: proglib.c -- not in this folder and not in \SOURCE
```

**Verified:** both cases. `CC CRASH.C` in `\SOURCE` (previously the only
working path) and `CC AUSSEN.C` in the root directory — both compile
and run. Plus 45/45 self-test, 11/11 compiler test, and bootstrapping
(stages 2 and 3 byte for byte identical).

---

## Error Messages: Visible and With the Right Line Number

**New — messages appear in the editor.** The compiler runs as its own
process; its output went to the invisible text screen, and the editor
just showed "Errors." Now the system logs the output during compilation
into a buffer (`cap_*` in `term.c`, buffer at `0x128000`, 40 lines), and
the compile window stays up on errors, showing them as
**"Compiler messages."**

Important detail: the logging had to sit **in `syscall.c`**, not just
in `lib.c`. Programs write output through the system calls, not through
the kernel's `print` functions — the first attempt therefore caught
nothing. And `printn` has to be included too, otherwise exactly the
numbers (line numbers!) go missing.

**New — line numbers are correct again.** They used to count the
included text too: an eight-line file reported errors on "line 146,"
because `proglib.c` gets pasted in front of it. `cc.c` now remembers
which range of the concatenated version came from which file
(`inc_start`, `inc_len`, `melde_ort`) and translates back. Errors in an
included file now also report its name.

**New:** "unknown function" now names the line of the call — the line
number is recorded when the call is generated (`fix_zeile`). Previously
only the name was shown, and you had to search through a long file.

Before/after on the same file:

```
  undefined function: gibtsnicht          (before)
  line 4: unknown function gibtsnicht     (now)
```

And "cannot include" now also says that only the current folder was
searched — exactly the trap you fall into with a file outside
`\SOURCE`.

---

## Delete Key Now Repeats — and a Failed Compiler Attempt

**New:** held special keys now repeat. After **0.4 s** of holding, the
key fires again every **30 ms** — for Backspace, Delete, arrows, and
Page Up/Down. Implemented in `pc.py` (`WIEDERHOLBAR`, `halten`), not via
`pygame.key.set_repeat`: this way exactly the keys where it makes sense
repeat, and not F12 or Ctrl+R.

**Discovered:** compiling a `.C` file outside `\SOURCE` gets nothing but
"undefined function." Cause: `CC` looks for `#include` files **only in
the current folder**, but `proglib.c` and `gfxlib.c` live in `\SOURCE`.
Anyone who doesn't know this ends up hunting for the bug in their own
code.

**Failed attempt, reverted:** I built a search path for this
(`fs_read_lib`, syscall 33: current folder first, then `\SOURCE`) and
switched `cc.c` to use it. But the call **always** returned −1, even for
files in the current folder — which meant **no** `#include` worked
anymore, i.e. not a single program could be compiled. Reverted
immediately, `cc.c` reads includes via `fileread` again.

`fs_read_lib` and syscall 33 remain in the kernel, but nobody uses them.
Why they return −1 is **unresolved** — that needs debugging before
anyone touches it again. Noted in [[11 Offene Punkte]].

> **Addendum — resolved and fixed.** Syscall 33 wasn't in the kernel at
> all: an `#include` sitting *inside a comment* above it had caused the
> preprocessor to delete the line with the `*/`. The now-open comment
> ate the code. See the entry at the very top and [[07 Fallstricke]].

**Lesson:** a change to the compiler affects *every* program. Something
like this should be tested against a real compilation before shipping —
I had only tested the new case, not the old one.

---

## Pasting From the Mac Only Worked With Cmd+V

**Fixed.** Whoever had copied something on the Mac and pressed
`Ctrl+V` in the editor got nothing.

**Cause:** there were **two** clipboards and two keys. `Ctrl+V` pasted
TOOBAD-OS's internal clipboard (which was empty), only `Cmd+V` fetched
from the Mac. A distinction nobody wants to keep in their head — and
one with no technical reason either.

**Now:** both keys do the same thing. If something is on the Mac
clipboard, `pc.py` writes it **directly into TOOBAD-OS's buffer**
(`gast_clipboard_setzen`, via the symbol table `system/kernel.sym`) and
then sends `Ctrl+V` to the system — which pastes it from there itself.

Two improvements along the way: previously the characters were injected
as **keystrokes**, one by one through the keyboard buffer — slow, only
usable in the editor, and without tabs. Now it happens in one go,
including line breaks, wherever the system supports pasting. And the
symbol table is now read just **once** instead of on every keystroke.

Verified: two-line text pasted from the Mac (50 characters in, 50
arrived in the editor), then selected with `Ctrl+A`/`Ctrl+C` and
fetched back again.

---

## Flappy, and What It Revealed About the Graphics

**New:** `programs/flappy.c` — Flappy Bird for the TB-32. Physics in
**sixteenths** of a pixel (the TB-32 has no floating point), a bird,
three moving pipes, score, best score, frame-rate display.

**New — and much more important than the game:** system calls **31 and
32** take an entire blitter command **at once**. Previously, a filled
area needed **six** system calls (x, y, w, h, color, command).
Coordinates are packed two to a word, 16 bits each in two's complement —
the blitter reinterprets values from `0x8000` up as negative itself.

Measured on the device (200 filled areas):

| | |
|---|---|
| over six ports (old) | 85 ticks → **4.25 ms per area** |
| over one system call | 30 ticks → **1.5 ms per area** |
| a system call alone (`ticks()`) | **0.4 ms** |

That's why animated graphics on the TB-32 are sluggish: **it's not the
computation that costs, it's the jump into the kernel.** The game needs
around 25 draw calls per frame and manages **9 frames/s** with that —
previously it was 5. The bird itself only uses about 20,000
instructions per frame; the CPU spends over 90% of its time waiting.

Whoever wants it faster has to attack this point, not the game: fewer
system calls per frame, or a cheaper way into the kernel. Noted in
[[11 Offene Punkte]].

**Fixed while building the game**

- Wrong color numbers: the color cube is `16 + red*36 + green*6 + blue`,
  I had mixed up green and brown
- **Flicker:** the game first erased the whole bird, then drew the
  pipes, and only afterward redrew the bird — with two dozen draw calls
  in between, and the screen already reading along the whole time. Now
  the bird is erased immediately before being redrawn
- **The best score disappeared:** the trail behind the pipes wipes over
  it from above, but the display was only redrawn on change

---

## Keyboard Was Lagging One Keystroke Behind

**Fixed.** In the BIOS setup, nothing happened on the first arrow
press; the next press then carried out the previous move — the
controls felt completely inverted.

**Cause:** `irq_kbd` in `firmware/bios.asm` fetched **exactly one** key
per interrupt. The interrupt controller only tracks one bit per source,
though: if two keys arrive before the handler runs, there's still only
one interrupt — the second one sat in the chip until eventually the
next key triggered a new interrupt. It stood out especially in setup,
because the whole screen redraws after every keypress there.

The same bug used to sit in the **timer** and is already documented in
[[07 Fallstricke]] — I didn't think of it while rebuilding the keyboard.

**Fixed:** the handler now drains the chip in a loop as long as the
status port reports a key.

**New:** self-test checks this case (two keys in the same frame),
44 → **45**.

---

## BIOS Setup With Tabs, Adjustable Clock, Secure Boot

**New**

- Setup has four tabs: **Main, Hardware, Cooling, Security**.
  Left/Right switches, `SET_TABS` and `setup_tabs` in
  `firmware/setup.asm`
- **Time and date adjustable** via a field editor
  (`setup_edit_felder`) — Up/Down changes, Left/Right switches the
  field
- **Cooling:** fan control and throttle limit in CMOS
  (`CM_FANMODE`, `CM_TEMPLIMIT`), applied to the ports by
  `kuehlung_anwenden` during POST. Temperature, fan, throttling, and
  peak value live
- **Secure Boot:** checksum over boot sector, kernel, and the first
  16 KB of ROM, remembered in `CM_SUM0`–`CM_SUM3`. If it doesn't match,
  the machine halts and offers `DEL` as the way into setup. Off by
  default
- New ports in `const.inc`: `P_TEMP`, `P_FAN`, `P_THROTTLE`,
  `P_TEMPLIMIT`, `P_FANMODE`, `P_TEMPMAX`
- Self-test extended by three checks: 41 → **44**

**Fixed**

- **The TB-32's clock couldn't be set.** `CMOS._refresh_clock()` read
  the host's clock on every access — any written value was immediately
  overwritten again. The chip now remembers an **offset in seconds**
  (CMOS registers `0x30`–`0x33`); writing to a clock register
  recalculates it, like turning the hands on a real RTC
- **Screen full of zeros on the Security tab.** Two causes: the active
  tab lived in `BDA_SCRATCH`, where `vid_putn` formats its digits — and
  `vid_puthex` expects the digit count in `r3`, which I hadn't set.
  Dedicated space `SETUP_TAB`/`SETUP_ROW`/`SETUP_SAVE` starting at
  `0x600`
- **Infinite loop while drawing:** the line count sat in `r11`, a
  scratch register that any subroutine call is allowed to clobber

---

## CPU Optimization: About 3.4× Faster

**Changed** — all in `hardware/cpu.py` and `pc.py`, measured with
`tools/opstat.py`:

| | before | now |
|---|---|---|
| Raw throughput | 1.74 million instructions/s | **3.11 million/s** |
| usable in the window | ~0.83 million/s | **2.82 million/s at 63 fps** |

1. Memory as a 32-bit view (`memoryview(ram).cast("I")`) — one access
   instead of four bytes plus shifting; misaligned addresses fall back
   to the old path
2. Dispatch chain sorted by **measured** frequency (`push`/`pop`
   together make up 40% of all instructions and used to sit at
   position 13 and 14)
3. `rb`, `imm`, `simm` are only fetched by the branch that needs them
4. Pending interrupts and breakpoint set in local variables; the halt
   check now only happens where a halt can actually occur
5. `pc.py` gives the CPU the whole frame minus drawing time instead of
   a fixed 8 ms

**New:** `tools/opstat.py` measures instruction frequency.

---

## Data Loss While Building

**Fixed — the most serious bug of this round.** `build.py` read the
*entire* disk image, swapped in the boot sector and kernel, and wrote
it all back. If the emulator was running alongside it (writing its own
sectors straight back into the same file), its files ended up back at
their pre-build state afterward — Colin's compiled programs, gone.
`tools/tbfs.py` had the same bug in `save()`.

Now `build.py` only writes sector 0 and the kernel sectors (`r+b` with
targeted `seek`s) and no longer touches the file system starting at
sector 512. `tbfs.py` tracks in `self.dirty` which sectors it has
changed.

---

## Desktop: Icons, Double-Click, Dragging

**New**

- **`\DESKTOP` as a real folder** carries the icons — no special case,
  the command line sees it like any other folder
- Icons **freely movable**, layout in `\DESKTOP\ICONS.DAT`
  (`icon_pos[]`, one word per directory entry)
- **Double-click opens** (list like icon), `eintrag_oeffnen` is the one
  place that decides what that means
- Dragging: from a window onto the desktop moves to `\DESKTOP`, from
  the desktop onto a file window moves into its folder
- Icons redrawn: page with a dog-ear and a colored extension stripe,
  folder with a tab, program as a window with a start arrow
- Files **moved** via `fs_move` — in TBFS this is just a changed field
  in the directory entry, no sector gets moved
- File manager toolbar reduced from six buttons to four, sorted by
  frequency, positions in *one* table (`fb_x`, `fb_breite`)

**Fixed**

- **Clicks landed in the wrong window:** drawing followed stacking
  order, checking followed window number
- **File manager showed a hard-coded 11 rows with no scrolling** — in
  `\SOURCE`, the last three files were invisible
- **Endless scrolling** in the editor's selection screen: only the
  lower bound was checked
- **Double-clicking a program in the desktop folder** failed with
  "is not recognized" — the search path is current folder →
  `\SYSTEM` → `\PROGS`, and `\DESKTOP` isn't in it.
  `eintrag_oeffnen` now sets `cwd` beforehand
- **Graphical programs painted over the desktop:** whoever switches the
  screen mode now gets the whole screen (`gui_fremd`), and
  `term_aktiv` is switched off in the process — otherwise no key got
  through
- **`New` kept the file name** — `Save` would have wiped the open file
- **Text ran out of buttons:** `g_button` only centers, it doesn't
  truncate
- **Icon name ran off the picture:** only the centering math was
  clipped
- **The × wasn't centered:** the font glyphs are 5×7 in an 8×8 cell;
  the cross and fullscreen glyphs are now drawn by hand
- **Number over label** in the clock window

---

## Editor, Terminal Window, Compile Window

**New**

- **Start screen** for the editor: new file (`.C`, `.ASM`, `.PY`, `.MD`
  with a template) or open an existing one, `Up` right in the list,
  `< Back` in the button bar
- **Mouse in the editor:** click sets the caret, dragging selects,
  mouse wheel scrolls (new port `0x63` for the wheel)
- **Clipboard** at `0x130000`, `Ctrl+C/X/V/A`; `Cmd+C`/`Cmd+V` exchange
  with the macOS clipboard
- **Window size and fullscreen** for TOOBAD-OS windows *and* the
  emulator window
- **Compile window** with a progress bar, percentage, and status line;
  `cc.c` reports its three phases via syscall 29
- **Scrollback in the terminal window:** ring buffer at `0x124000`,
  200 lines, `term_sicht()` computes over an imagined total stream

**Fixed**

- **`WIN` inside the terminal window** started a second desktop — and
  `gui_running` was only reset on the *Exit* menu item, not on ESC
- **`continue` skipped past the redraw:** start menu → editor opened
  the window, but the screen kept showing the menu

---

## Programs and Tools

**New**

- **`programs/gfxlib.c`** — graphics for your own programs: blitter,
  font, enlarged font straight into the screen buffer, buttons, mouse
- **Syscall 30** returns the font address
- **`programs/calc.c`** — calculator, computes in thousandths, because
  the TB-32 has no floating point
- **`programs/crash.c`** — stress test and fault injector: burn-in until
  throttling, color chaos, flicker, plus five real crashes. Runs with
  `/B` in the background while the desktop keeps going. Deliberately
  shipped **as source only** (`NUR_QUELLTEXT` in `build.py`)
- **`.PY` starts under its own name** — the shell prepends `PY.TBX`
- `screenshot.py` can type at specific times with
  `--type "10.0:text|ENTER"`

**Fixed**

- **`sleep()`/`beep()` froze the whole machine** when a program reached
  them via a system call: `hlt` waits for the timer, but interrupts are
  disabled during `INT 0x40`. `asm("sti")` in `lib.c`
- **A background program stole the keyboard** — `TASKLIST` arrived as
  `ASKLIST`. Processes started with `/B` are now put to sleep in
  `getkey()`
- **`START X.TBX ARG /B` ran in the foreground:** `/B` was only
  recognized as the second word
- **The kernel grew into the file system's buffers** and overwrote its
  own directory — window titles showed up as file names. Buffers moved
  past `0xB0000`, `build.py` checks the gap
- Limits of `cc.c` found and documented: no `?:`, no `asm()`, no
  variables mid-block, at most 5 arguments — and `#include` is also
  found **inside comments**

---

## Docs

- [[12 Abkuerzungen und Namen]] — what TBX, TBFS, TC, TCC, CC are meant
  to stand for
- [[13 BIOS-Dienste und was fehlt]] — service list, setup, Secure Boot,
  and the known gaps
- This page

Related: [[00 START HIER]], [[07 Fallstricke]]
