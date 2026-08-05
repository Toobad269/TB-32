# COMPANY-OS BIOS — what it's supposed to do

A BIOS for company machines. This file is the **spec**: what's already
done, what's still missing, where it belongs, and what pitfalls you'll
run into along the way. Build it here:

```bash
python3 "Custom BIOS/COMPANY-OS/bauen.py"
```

---

## First: why your flashed BIOS kept disappearing

That's not a bug in the BIOS. **`build.py` overwrites the chip.**

The ROM chip is the file `firmware/bios.bin`. `build.py` rebuilds this
exact file from `firmware/bios.asm` on every run:

```python
bios, syms = asm_file(os.path.join(fw, "bios.asm"),
                      os.path.join(fw, "bios.bin"), ...)
```

So: you flash COMPANY-OS → it's there. Then one of us changes something
in the system and runs `build.py` → the chip carries the stock BIOS
again. To you it looked like "sometimes the flashed one is there,
sometimes it isn't" — in reality it was the same step wiping it out
every single time.

On a real mainboard this can't happen, because the compiler has no
access to the flash chip. For us, both live in the same folder.

**Three ways out — pick one:**

1. **`build.py` leaves the chip alone if a foreign BIOS is on it.**
   Read the name in the header before writing (`BIOSHDR_NAME`, byte
   0x10). If it doesn't say the stock BIOS's name, don't overwrite it —
   report instead: *"firmware/bios.bin carries COMPANY-OS — left
   untouched. To reset: `python3 build.py --bios-neu`."* That's the
   most honest approach, because it respects the machine's current
   state.
2. **A startup flag `--bios <file>`** in `pc.py`. `Machine(ROOT, rom=...)`
   already supports this, `pc.py` just doesn't pass it through yet.
   Then you can test COMPANY-OS without flashing at all.
3. **Both together** is the convenient way: build it, try it with
   `--bios`, and once you like it, flash it in Setup — where it then
   stays put.

Also check the second spot afterward: `flash.einmal` (flash command 6)
is an image for **exactly one boot** and gets consumed in
`Machine.power_on()`. That's intentional, but if a tool accidentally
sends command 6 instead of 3, the result looks the same all over again.
The Setup button *Flash BIOS from File* does it correctly (command 3).

---

## What's already done

| What | Where |
|---|---|
| Setup locked with a supervisor password, its own *Password* tab | `passwort.asm`, `tab_password` |
| Three failed attempts, then locked out | `pw_tor` |
| F5 "Load Defaults" leaves the password alone | `setup_load_defaults` |
| Secure Boot (checksum over boot sector, kernel, and ROM) | `secure_summe` |
| Flashing a BIOS from a file, restoring the backup | `flash_bios`, `flash_restore` |
| Owner entry in memory (`BDA_FIRMA`) | `bios.asm`, `s_firma:` |
| Display top right and on the login screen | `system/gui.c`, `firma_da()` |

The *Password* tab is already exactly what you wanted: its own tab, with
nothing else in it.

---

# The feature list

Sorted by importance, not by effort. Every line says where it belongs
and where its setting lives.

## A — Required: what makes this a company BIOS

### A1 Two passwords instead of one
*Tab: Password · CMOS 0x27–0x2B*

Today there's only the **supervisor** password, and it only guards
Setup. A company machine needs two:

* **Supervisor** — opens Setup. Already there.
* **Power-On (User)** — required **before booting**. Without it, the
  machine won't even start.

That's the heart of the whole thing. A BIOS password that only protects
Setup doesn't stop anyone from switching the machine on and using it.

Not much needs to be built: `pw_tor` already exists, it just needs a
second checksum (flag + 4 bytes) and a call in `bios.asm` before the
boot attempt. The supervisor password must **also** get through the
power-on gate — otherwise the boss locks himself out the moment he
forgets the user password.

### A2 Count failed attempts in CMOS
*Tab: Password · CMOS 0x2C*

`pw_tor` currently counts in a register: three attempts, then locked
out — but a press of Reset starts back at three. So you can guess at
your leisure, as often as you like.

Real BIOSes therefore count **in the coin-cell backed memory**. After
three failed attempts the machine halts, and the counter survives a
restart; only the correct entry resets it. One byte, and it changes
everything.

### A3 The owner entry, configurable
*Tab: Company · see "Where the settings live"*

Text up to 31 characters and an on/off switch. Currently hard-coded
into the image.

### A4 Blocking programs
*Tab: Company · CMOS 0x25/0x26*

A BIOS doesn't know about files. It knows a **fixed list** of programs,
the same way a real BIOS knows a fixed list of ports ("USB Ports:
Enabled/Disabled"):

```asm
sperr_namen:  .dw s_p_coder, s_p_prompt, s_p_browser, s_p_monitor, ...
```

One bit per program, sixteen programs across two CMOS bytes. Because
the BIOS knows the names, it lays them out in memory as plain text at
boot — the system then doesn't have to interpret bits or keep its own
table. Adding a new program later only requires changing the BIOS.

### A5 Blocking the compiler and networking
*Tab: Company · policy word, bits 1 and 2*

The compiler absolutely has to be on this list, and not out of
paranoia: the TB-32 has no memory protection. Anyone with the Coder can
write a program that talks to the ports directly — and then every other
lock is just decoration.

### A6 Boot only from the internal drive
*Tab: Company or Hardware · policy word, bit 4*

The classic first move against someone else's machine is: boot your own
system from elsewhere and read the drive undisturbed. If you don't
block that, every other lock was pointless — they only live in the
system that never even comes up.

For us that means: pin down the boot order and refuse `Network`/`Floppy`
as a boot source while this bit is set.

### A7 Write protection for the chip — and this one's a real hole
*Tab: Firmware · in the device, not CMOS*

`P_FLASH_CMD` is a perfectly ordinary port. The TB-32 has no port
permissions — **any program in the running system can overwrite the
BIOS chip.** Your whole firmware lock hangs off a single `portout`.

A switch in Setup alone doesn't help against that, because only Setup
ever reads it. The lock has to sit in the **device**, in
`hardware/devices.py`:

* `Flash` gets a lock latch, e.g. `self.gesperrt = False`.
* A new command (say, 10) sets it. Only a **restart** can clear it.
* Once set, commands 3 (burn) and 4 (restore backup) refuse to run.
* The BIOS sets it as the **last step before booting** — up to that
  point it can still flash itself, afterward nobody can.

That's exactly how real chipsets do it: a lock bit that the firmware
sets, released only by a reset. This is the one item on this list where
you have to work at the device level — and the most important one.

### A8 Noticing that the coin cell was pulled
*Tab: Company (display only) · CMOS magic 0x2F*

Anyone who gets to `disk/cmos.bin` erases every lock. You can't prevent
that — a real mainboard can't either. But you can make it **visible**:
if the magic value is invalid at boot, the BIOS reports, in red,
*"Configuration was cleared — contact your administrator"* and writes it
to the event log.

Real company machines call this *chassis intrusion*. The idea behind it
matters more than the mechanism: where you can't prevent something,
make sure it gets noticed.

## B — Very worthwhile

### B1 Event log
*Its own "Event Log" tab · needs NVRAM*

The last eight events with date and time: wrong password, CMOS cleared,
BIOS flashed, Secure Boot halted, defaults loaded. Eight entries of
8 bytes each are enough.

For a company machine this is often more useful than any lock — locks
tell you what's not allowed to happen, the log tells you what did
happen. Real BIOSes have this as *SMBIOS Type 15 Event Log*.

### B2 Secure Boot: halt or just warn
*Tab: Security · extend CMOS 0x18 to three values*

Today it's only on/off. Real firmware has three levels: *Enforce*
(halt), *Audit* (boot but report and log), *Off*. You'll want the
middle one most yourself — while developing you want to be warned, not
locked out.

### B3 Inventory data the system can read
*Tab: Company (read-only) · NVRAM*

Model, serial number, BIOS version, commissioning date, boot count,
uptime hours. Read-only in Setup, stored in memory for the system —
the System Monitor then displays it.

That's exactly what SMBIOS is for on real PCs, and it's little work:
increment a couple of counters and lay down a bit of text.

### B4 Boot menu with F12
*no tab, a keypress at startup*

Boot from elsewhere just once, **without** changing the setting. If A6's
lock is in place, F12 demands the supervisor password — then it's the
administrator's tool, not a loophole.

### B5 Network boot
*Tab: Hardware · a big one*

Setup currently says "Network (not installed)." That's no longer true —
the card exists, with ARP, IP, UDP, and TCP on top. A company machine
fetching its system from a server is the normal case for real companies
(PXE).

This is the biggest item on this list, and it fits the Pi goal: a
machine that fetches its system over the network doesn't need anything
on disk at all.

### B6 Company splash screen
*Tab: Company*

The mainboard already paints the BIOS name in the middle of the screen.
Three lines of company text below it, from the same memory as the owner
entry — small effort, big impact.

## C — Nice to have, if there's time

* **Numlock at boot**, **startup beep on/off** (half done), **show POST
  timing**.
* **Auto power-on after a power outage** — on real servers this is *AC
  Power Recovery*. Mostly cosmetic for us, but easy.
* **Boot delay** — wait a few seconds before booting, so you can catch
  DEL reliably. Genuinely helps on slow displays.
* **A dedicated "Exit" tab** with *Save & Exit*, *Discard & Exit*,
  *Load Defaults*. Purely cosmetic, but real BIOSes have it, and it
  makes the F-keys visible to less experienced users.

## D — What I'd leave out, and why

* **Encrypted disk / TPM.** Without memory protection and without real
  keys, that would be a claim, not security — and a security indicator
  that lies is worse than none at all.
* **Master or recovery password.** A second key is a second hole. If
  you lock yourself out, pull the coin cell; that's the same route real
  machines use.
* **Remote management (like vPro/AMT).** That's a whole separate small
  computer inside the computer — a project of its own, not a tab.
* **Fingerprint, smart card.** No hardware for it, and faking it would
  just be a second password field with a fancier name.

## Build order

1. **A7 flash lock** and the `build.py` guard from the top of this doc.
   As long as the chip is overwritable from outside, you're testing in
   sand.
2. **A3 Company tab with Owner Tag** — the smallest thing that gets the
   whole new tab working end to end.
3. **A1/A2 power-on password and counter.**
4. **A6 lock boot source.**
5. **A4/A5 programs, compiler, networking** — together with the system
   side below.
6. **A8 and B1** — first noticing, then logging.
7. Everything else, as time allows.

---

## The *Company* tab

Built like every other tab: extend `setup_tabs`, `SET_TABS` to 7, table
`tab_company`, name `s_tab_comp`.

| Row | Behavior | Storage |
|---|---|---|
| `Owner Tag` | On / Off | `CM_POLICY` bit 0 |
| `Owner Text` | ENTER opens a text editor, up to 31 characters | see below |
| `Block Compiler` | On / Off | `CM_POLICY` bit 1 |
| `Block Network` | On / Off | `CM_POLICY` bit 2 |
| `Require Login Password` | On / Off | `CM_POLICY` bit 3 |
| `Boot From Internal Disk Only` | On / Off | `CM_POLICY` bit 4 |
| `Blocked Programs` | ENTER opens a checklist | `CM_BLOCK0/1` |
| `Configuration Cleared` | display only: Yes / No | magic 0x2F |
| `Item Help` | plain explanation line | `REG_INFO` |

The on/off rows need **no** special case: one CMOS slot with two values
and `opts_onoff` is enough — `setup_change` handles that on its own.
Only `Owner Text` and `Blocked Programs` are buttons (registers from
`0xE0`, entered in `setup_change`'s dispatch table — same as
`REG_PWSET`).

One bit per switch is fiddlier than one byte per switch. If you'd
rather, use a separate CMOS slot per switch — space is tight, but
there's room for four (see the table below).

### Owner Text — the editor

`passwort.asm` almost has it already: `pw_eingabe` reads a line, but
draws asterisks instead of letters (`pw_sterne`). For the company text
you need the same loop with visible output. Cleanest approach: a shared
`text_eingabe` routine with a "visible / asterisks" switch, so the
typing logic exists only once.

---

## Where the settings live

This is the real decision, and it comes down to the text.

**Free CMOS slots (64 bytes total, `hardware/devices.py`):**

| Range | State |
|---|---|
| `0x00`–`0x09` | clock |
| `0x10`–`0x1D` | settings |
| **`0x1E`–`0x1F`** | **free, within the checksum** |
| `0x20`–`0x24` | supervisor password (TB-LOCK) |
| **`0x25`–`0x2D`** | **free, within the checksum** |
| `0x2E` / `0x2F` | checksum over `0x10`–`0x2D`, magic |
| `0x30`–`0x33` | clock drift |
| `0x34`–`0x3E` | free, **outside** the checksum |
| `0x3F` | writing this backs up the coin cell |

Eleven free bytes under the checksum, and the list above needs exactly
eleven: policy word (1), block list (2), power-on password (5), failed
attempts (1) — leaving two spare. It fits, but with no margin.

**The 32 bytes of company text don't fit.** Two ways to handle that:

**A — an NVRAM device (recommended).** In `hardware/devices.py`, add a
second small memory, 256 bytes, its own file `disk/nvram.bin`, its own
port pair (0x72 index, 0x73 data — right next to CMOS). Real mainboards
do exactly this: the clock CMOS stayed at 64 bytes, everything else
moved into a separate chip. Costs you twenty lines of Python and, in
the BIOS, one read loop and one write loop. The event log (B1) and the
inventory data (B3) would get a home this way too.

**B — the BIOS writes to its own chip.** Purer: the text then really
lives in the firmware and survives even pulling the coin cell. The path
there is already in place — flash command 5 ("buffer out of RAM"), then
3 ("burn"). Sequence: copy the ROM into RAM, change the text bytes,
**recompute the checksum**, burn.

With option B, two things are critical:

* The header checksum (`BIOSHDR_SUM`, byte 0x0C) must be recomputed —
  zero the field, then `h = h*31 + word` over the whole image, starting
  value `0x1234`, then write it back. Forget this, and the mainboard
  rejects the chip on the next boot and falls back to the backup. That
  would look exactly like "sometimes it's there, sometimes it isn't"
  all over again.
* Secure Boot hashes the **first 16 KB** of the ROM (`secure_summe`,
  `li r8, 4096` words). If your text field is in there, every change to
  the company text invalidates the stored fingerprint and the machine
  halts at boot. So place the company block **past 16 KB**, for example
  at image offset `0x8000`.

Take A if you want it done this week. Take B if it matters to you that
the entry survives even pulling the coin cell — that's the kind of task
where you really understand what firmware is. And if you take B: **A7
first**, otherwise any program in the system rewrites the chip right
back.

---

## How it reaches the system at boot

Same as before, via the BIOS data area — that's our SMBIOS:

| Address | Contents | State |
|---|---|---|
| `0x00000500` | 32-byte owner entry, null-terminated | there |
| `0x00000524` | policy word | there, but hardwired to 1 |
| `0x00000528` | **new:** 8 blocked programs of 16 bytes each, empty name = end | missing |
| `0x000005A8` | **new:** inventory — serial number, boot count, uptime hours | missing |

The block list ends at `0x5A8`, Setup only starts at `0x600`. So 88
bytes remain for the inventory; anyone needing more shifts `SETUP_TAB`
upward.

Policy word, as already fixed in `firmware/const.inc`:

| Bit | Meaning |
|---|---|
| 0 | show the owner entry |
| 1 | no compiler |
| 2 | no network |
| 3 | password required |
| 4 | boot only from the internal drive |

**Important:** the stock BIOS must clear **all** of these areas at
boot, including the new ones. Today it only clears the first two
(`bios.asm`, line 584). Otherwise, after flashing back to stock, a
company BIOS's block list would linger, and the system would keep
blocking programs for which there's long since no firmware backing.

---

## What the system still needs to do

That's the other half, and without it the BIOS has no effect. All of
it in `system/gui.c`:

1. **Evaluate `firma_policy()`.** The function already exists, it's
   just unused anywhere. Let bit 0 decide the display instead of
   `firma_da()`.
2. **Intercept blocked programs** — a function `gesperrt(char* name)`
   (checks whether a program is blocked) that walks the list from
   `0x528`, and **one** call in `gui_prog_starten()` (line 1901). Every
   launch goes through there, including from the file manager.
3. **Draw it greyed out in the Start menu**, don't hide it. A program
   that's invisible looks like a bug; a greyed-out one looks like a
   rule. On click, a message: *"Blocked by system policy."*
4. **Hang the compiler and networking off the same point** (bits 1
   and 2).
5. **Bit 3 "password required"** — the login must then no longer let
   through any account without a password.
6. **Show the inventory** in the System Monitor, once B3 is in place.

---

## Pitfalls I ran into while building this

* **The name in the header must be exactly 32 bytes.** "COMPANY-OS BIOS
  v1.0" is 20 characters, 21 with the null terminator — so `.space 11`.
  One byte too many and the code starts at `0x31` instead of `0x30`;
  the machine jumps into the middle of an instruction at power-on and
  runs off into nothing, **with no error message at all**.
* **`.align 4` after every string**, before a word table follows. A
  single extra character in the text otherwise shifts the whole table.
* **`cmp`, `cmpi`, `tst`, `jmpr`, `callr` use `rd`, not `ra`.**
* **A new row in a tab, but `setup_tabs` not updated to match** — then
  Setup draws the row, but Up/Down don't reach it. The number after the
  table pointer is the row count.
* **New CMOS slots past `0x2D`** fall outside the checksum. They
  survive a restart fine, but nobody notices if the CMOS gets wiped.

---

## Acceptance test

Use `Custom BIOS/TB-LOCK/pruefen.py` as a template — it actually boots
the machine, with its own coin cell in a temp folder, and the test
presses the same keys a human would. Done means all of this passes:

1. Open Setup, go to *Company*, set the text to something custom, F10.
2. Reboot → the new text appears top right on the desktop.
3. Set *Owner Tag* to Off, F10, reboot → nothing appears there anymore.
4. Set a power-on password, reboot → the machine asks **before** it
   boots. Three wrong tries → it halts. Reset → it doesn't start
   counting over from zero.
5. Block a program, reboot → it's greyed out in the Start menu and
   won't launch.
6. Write a program in the system that does `portout` on `P_FLASH_CMD` →
   the chip stays intact (A7).
7. Run `build.py` → COMPANY-OS is **still there**.
8. Flash the stock BIOS back → text gone, locks gone.

---

## The honest limit

The TB-32 has **no memory protection**. A policy is therefore a rule,
not a wall: anyone with the Coder writes a program that talks to the
ports directly, and the lock means nothing to it. That's why "block the
compiler" sits so high on this list, and why the flash lock has to live
in the device, not in Setup.

And anyone who gets to `disk/cmos.bin` has pulled the coin cell. On a
real mainboard that's the same physical act, and that's why every
manual says the same sentence: physical access beats any firmware lock.
What you can do about it isn't prevention, it's **making it visible** —
that's what A8 is for.
