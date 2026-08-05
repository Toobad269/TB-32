# Desktop Structure

Everything is in `system/gui.c`. Graphics mode 640×400, drawn via the
graphics card's blitter (ports `0x44`–`0x4C`) — never pixel by pixel over
the bus, that's a hundred times slower.

## Drawing functions

`g_fill(x,y,w,h,farbe)`, `g_frame`, `g_char(x,y,zeichen,farbe,bg)`,
`g_text`, `g_num`, `g_num2`, `g_panel(…,gedrueckt)`, `g_button`.
`bg = 256` means transparent.

The character set only knows 32–127. Draw block characters as rectangles
yourself.

## Windows

`win_type[]` `win_x[] win_y[] win_w[] win_h[]`, `win_top` = frontmost window.
`MAXWIN` = 6. `starte(typ, titel, w, h)` opens a window or brings it to the
front and **clamps the position to the screen**.

Every window has two buttons in its title bar: **fullscreen** (12px wide,
at `win_w - 30`) and **close** (at `win_w - 16`). `win_vollbild(i)`
remembers the previous dimensions in `win_ax/ay/aw/ah` and sets
`win_voll[i]`.

The **resize handle** for dragging sits in the bottom-right corner (12×12,
only when not fullscreen). It's drawn **after** the application content,
otherwise the application would paint over it. While dragging,
`groesse_zieht` holds the window number; minimum size 160×80, clamped to
the screen and the taskbar.

Important: the hit test for the corner comes **before** all other hits in
the click chain — otherwise the application would intercept the click.

| Type | Application |
|---|---|
| 1 | File Manager (Up/Move/Delete/**Open/Run**, double-click opens) |
| 2 | Clock |
| 3 | System Monitor (processes, disk, **temperature**) |
| 4 | About |
| 6 | Control Panel (CMOS + fan mode) |
| 7 | **Command Prompt** (see below) |
| 8 | **Editor** |
| 9 | **Compiling** — progress window, opens and closes itself |
| 10 | **Paint** — drawing program, `system/paint.c` |
| 11 | **Word** — word processor, `system/word.c` |

## Taskbar and start menu

`Start` on the left (opens the menu), followed by a button for each open
window (the frontmost one appears pressed), and the clock on the right.
Menu entries live in `menu_text()`.

**Order (MENU_ANZ = 10):**

| No. | Entry |
|---|---|
| 0 | File Manager |
| 1 | Command Prompt |
| 2 | Editor |
| 3 | System Monitor |
| 4 | Control Panel |
| 5 | Paint |
| 6 | Word |
| 7 | Clock |
| 8 | About TOOBAD-OS |
| 9 | Exit desktop |

**Watch out in click tests:** The menu grows **upward**. The position of the
*n*-th entry is

```
MENU_TOP = BAR_Y - (MENU_ANZ * MENU_ZH + 10)      # 378 - (10*14 + 10) = 228
y        = MENU_TOP + 6 + n * 14
```

Every new menu entry shifts **all** click coordinates by one row.
`tools/selftest.py` therefore computes them from `MENU_ANZ` instead of
hard-coding them — tests have already failed twice over exactly this.

## Right mouse button

The mouse reports in port `0x62` bit 0 left, bit 1 middle, **bit 2 right**.
The desktop remembers in `gui_taste` which button triggered the last click.
Word reads this and opens its color menu accordingly.

## Scrolling back in the terminal window

Lines that scroll off the top move into a ring buffer at `0x00124000`
(200 lines of 70×2 bytes). `term_sb_push()` saves the topmost line before
`term_scroll()` overwrites it.

`term_sicht(i, view)` returns the address of the *i*-th visible line. The
calculation runs over a conceptual combined stream of ring buffer + current
frame — that's why the transition is seamless and needs no special cases.
The mouse wheel scrolls (`term_view`), and any keypress jumps back to the
front.

## Terminal window

The shell runs as its **own process** (`term_main` in `system/term.c`).

- Output: `term_aktiv` switches output into the buffer at `0x120000`, in
  `lib.c` and `syscall.c`
- Input: the GUI loop fetches keys and forwards them with `term_push_key()`
  when the terminal window is in front
- When leaving the desktop, the process **must** be terminated and
  `term_aktiv` reset
- The buffer is a fixed 70×22. `app_term` draws only as many rows and
  columns as fit into the window — otherwise the text writes past the edge
- `WIN` inside the terminal window does **not** start a second desktop:
  `kernel.c` checks `gui_running`. That's why `gui_main()` must also set
  `gui_running = 0` when leaving via ESC, not only via the *Exit* menu item

## Editor window

Uses the editing functions from `system/edit.c` (`ed_insert`,
`ed_backspace`, `ed_line_of` …). Only drawing and key dispatch are specific
to the window itself.

## Desktop icons

**"Sitting on the desktop" means: sitting in the `\DESKTOP` folder.** That
makes it not a special case at all — the command line sees the folder, the
file manager sees it, and moving something is the same `fs_move` function
used everywhere else. `desk_ordner()` creates the folder on first draw if
it's missing.

- `desk_index(n)` returns the n-th entry, `desk_symbol` paints it depending
  on type: **sheet** with a folded corner and a colored extension stripe
  (`.C` red, `.ASM` yellow, `.PY` green, `.MD` teal), **folder** with a tab
  and a lighter front face, **program** as a small window with a title bar
  and a green start arrow. Diagonal edges are built as a staircase of
  `g_fill` rectangles
- The name is **truncated to 11 characters** (`kurzname`) and clamped to the
  screen on both left and right. Previously only the centering used 10
  characters while the whole name was still drawn — for an icon near the
  left edge this produced "EADME.TXT"
- **Position:** `icon_pos[]` has one word per directory entry — `x` in the
  lower half, `y` in the upper half, **0 means "never touched"** and yields
  the grid slot (`desk_raster_x/y`, 7 columns of 84×62). `desk_setzen()`
  clamps to the screen and avoids the value 0/0 so that "placed" stays
  unambiguous
- The table is stored as `\DESKTOP\ICONS.DAT` (512 bytes). `icon_laden`/
  `icon_speichern` briefly switch `cwd` to the desktop folder, because
  `fs_read`/`fs_write` always operate on the current folder. The file itself
  is skipped in `desk_index`, otherwise it would show up as an icon of its
  own
- `desk_treffer(mx,my)` reports which icon the mouse is over
- Icons behind a window aren't clickable — the click chain checks
  `win_unter(mx,my)` first

## Graphical programs get the whole screen

A program that switches the screen mode can't fit inside a window — it
paints onto the same surface as the desktop. So the desktop steps aside for
its duration:

- `syscall.c`, function 17: switches a program into graphics mode; while
  `gui_running` holds, `gui_fremd = 1` is set and `term_aktiv = 0`. The
  second part is essential — otherwise the program would pull its keys from
  the terminal window's queue, which the sleeping desktop keeps filling: not
  a single key would ever arrive
- returning to text mode means "done" → `gui_fremd = 2`
- the main loop draws nothing and reads no keys at 1; at 2 it restores
  graphics mode, character set, and mouse cursor, and redraws
- `gui_selbst` protects the desktop's own mode switches from triggering this
  mechanism on themselves (`gui_ausfuehren` sets it for its entire duration)

## Double-click and dragging

`klick_was` + `klick_zeit` remember the last click; two clicks on the same
target within **50 ticks (0.5s)** count as a double-click. File manager rows
are numbered from 0, desktop icons from 1000 — so the two can never be
confused.

`eintrag_oeffnen(idx)` is the *single* place that decides what "open" means:
entering a folder, launching `.TBX`/`.PY` **fullscreen** (`gui_ausfuehren`),
everything else into the editor. Only what you type yourself into the
command line runs inside a window — there, the window *is* the shell.
**Before that, `cwd` is set to the file's folder** — the program search path
is current folder → `\SYSTEM` → `\PROGS`, and `\DESKTOP` isn't part of it.
Double-clicking in the list and on the icon both call this same function —
there's no second path that could ever drift apart from it.

**Dragging:** `files_click` returns **2** when the click landed on a row;
the main loop then remembers `zieh_idx`. On the desktop it also remembers
`zieh_sym` and the grab point (`zieh_dx/dy`), so the icon stays under the
cursor instead of jumping. On release, `win_unter(mx,my)` decides:

| released over | effect |
|---|---|
| empty desktop, file came from a window | `fs_move` to `\DESKTOP` + set position |
| empty desktop, icon was already there | just a new position, `icon_speichern()` |
| a File Manager window | `fs_move` into its folder, clear position |

**File manager button bar** lives in a *single* table (`fb_breite()`,
`fb_x()`, `fb_text()`) — drawing and click handling both read from it, so
they can't drift apart. Only four buttons remain now, sorted by frequency:
`Up`, `Move`/`Drop`, `Delete`, `Open/Run` (the latter takes over the whole
screen, for graphical programs). Opening and launching are both handled by
double-clicking; a separate button for that would just be a duplicate. The
**Text Viewer** (window type 5) was dropped along the way — it was only
reachable via "Open" on a `.TBX` and showed machine code as text. Buttons
that no longer fit the window are left out rather than drawn past the edge.

**Programs in a window** (`gui_im_fenster`): the terminal window *is* a real
shell with its own screen buffer and its own keyboard. Instead of building a
second execution environment, the function opens the window and **types the
command into it** (`term_push_key`). The shell starts the program as its
child, and the output ends up in the window automatically via `term_aktiv`.
Important: the Enter key must arrive as `13 + (K_ENTER << 8)` — the shell
checks the **key code**, and doesn't recognize a bare 13.
Graphical programs do *not* belong here — they switch the screen mode; for
those, `Run` with `gui_ausfuehren` remains the way.

**Moving** (`fs_move` in `fs.c`): changes only the parent folder in the
directory entry, nothing else — no sectors are touched. Two clicks: *Move*
picks it up (`move_quelle`, the path line shows "Moving: …", the button
becomes *Drop*), *Drop* in the target folder places it. If it fails (name
already taken, folder moved into itself), the file stays picked up.

**File manager:** `file_masse()` computes `file_rows` from the window
height, `file_top` is the first visible row, `file_sel` is the selected
entry (an index into the *whole* list, not the visible portion — so both
drawing and click handling always compute `file_top + row`). The mouse
wheel scrolls, and a scrollbar appears on the right once there are more
entries than fit. It used to show a fixed 11 rows with no scrolling at
all: anything below that was invisible, and `\SOURCE` with 14 files never
showed the last three.

**Start screen:** `edg_screen` = 0 shows the question *"What do you want to
do?"* instead of the text — four buttons on the left for a new file (`.C`,
`.ASM`, `.PY`, `.MD`, each with a small template), the current folder's
listing on the right (folders lead inward, and the `Up` button **directly
above the list** leads back out — nobody ever found it tucked away in the
corner). `edg_liste_top` is clamped **both upward and downward** on the
wheel, otherwise you scroll endlessly into empty space; below that sits the
folder where saving happens. The `< Back` button in the button bar leads
back there — without it there was no way out of an open file. `New`, on the
other hand, only clears the sheet and sets the name to `NEW.<extension>`;
carrying the name over would be dangerous, because `Save` would then wipe
out the already-open file. It's the same `cwd` as in the file manager and
the command line — changing folders here changes it everywhere.

**Size adapts:** `edg_masse(w)` computes `edg_cols`/`edg_rows` from
`win_w`/`win_h` and must run before every draw *and* before every mouse
coordinate conversion. `EDG_COLS`/`EDG_ROWS` are now just aliases for it.

**Mouse:** `edg_pos_aus_maus()` converts screen points into a text position
(`zeile = (my - texty - 3)/9 + edg_top`, `spalte = (mx - textx - 3)/8`) and
clamps to the end of the line. A click sets the cursor and starts a
selection, `edg_zieht` extends it while dragging.

**Selection and clipboard:** `ed_sel_von`/`ed_sel_bis` (−1 = none), drawn
inverted. Buffer at `CLIP_BUF = 0x130000`, length in `clip_len`. Ctrl+C
copies, Ctrl+X cuts, Ctrl+V pastes (replaces the selection), Ctrl+A selects
all. **`Ctrl+V` and `Cmd+V` are the same thing**: if there's something in
the clipboard on the Mac side, `pc.py` writes it directly into `CLIP_BUF`
beforehand and sets `clip_len` (`gast_clipboard_setzen`, addresses taken
from `kernel.sym`). Typing while a selection exists replaces it.

**Mouse wheel:** Port `0x63` reports the accumulated notches and resets
itself on read. Scrolling sets `edg_folgen = 0` — otherwise `app_editor`
would immediately pull the viewport back to the cursor. Typing and clicking
turn following back on.

- Button bar: `< Back  New  Save  Rename  Compile  Run`, with the status
  field right next to it (progress bar, "Saved.", "Compiled: …", or the
  shortcut hint)
- `Compile` saves, starts `CC`/`ASM` as a **background process**, and opens
  the `APP_BUILD` window: source and target name, a bar driven by
  `build_progress` (syscall 28), and a status line from `build_status`
  (syscall 29, `cc.c` reports its three phases there). The main loop closes
  the window once the process is gone — **except on errors**: then it stays
  open, is titled "Compiler messages," and shows the logged compiler output
  (`cap_*` in `term.c`). The capture sits in `syscall.c`, because programs
  produce output through system calls, not through the kernel's `print`
  functions
- `Run` compiles source files first and then launches automatically
  (`edg_run_danach`); `.PY` and `.TBX` run immediately
- Programs run **in text mode** (`gui_ausfuehren`), then return to the
  desktop afterward — text programs and the desktop otherwise don't get
  along

Related: [[07 Fallstricke]], [[03 Dateien und Zustaendigkeiten]]
