# TB-HACK

The stock TOOBAD BIOS with the lid taken off. Where COMPANY-OS locks the
machine down, this one opens it up: **a hex monitor, a port console, a raw
CMOS editor, a disk sector editor, a freely chosen boot sector, and two
bytes written into memory in the last instant before the boot sector
runs.**

```bash
python3 "Custom BIOS/TB-HACK/bauen.py"      # builds TB-HACK.bin
python3 "Custom BIOS/TB-HACK/pruefen.py"    # 55 checks on the real machine
```

Installing it: **DEL → Firmware → Flash BIOS from File**, then select
`TB-HACK.bin` and power the machine off and back on.

## How it's used

Setup gets one new tab, **Hack**, at the far right — after *Firmware*:

| Row | |
|---|---|
| **Memory Monitor** | 256 bytes at a time, hex and text, every byte editable |
| **Port Console** | read and write any of the machine's I/O ports |
| **CMOS Editor** | all 64 bytes of the coin cell, raw |
| **Load Disk Sector** | pulls a sector into the buffer and opens it in the monitor |
| **Boot Sector** | which sector the machine starts from |
| **Ignore Boot Signature** | start a sector that has no `55 AA` at the end |
| **Apply Boot Patches** | on/off for the two patches below |
| **Edit Boot Patches** | two slots, each an address and a byte |

### The memory monitor

`G` jumps to an address, the arrow keys walk the cursor — at the edge the
page scrolls with it, so you can reach all 16 MB with the arrows alone.
`PgUp`/`PgDn` move a page. `ENTER` changes the byte under the cursor. The
line under the dump always says where you are and what's there.

Point it at `0x02000000` and you are looking at the text screen — at the
monitor's own output, live. Point it at `0x0F000000` and the `TBBI` of the
running chip is four bytes in. Writes to the ROM are simply swallowed,
exactly as on a real chip.

### The sector editor

*Load Disk Sector* asks for a sector, reads it into the buffer at 4 MB and
opens the monitor there. Change what you like, then `W` writes the buffer
back to the sector it came from — after one confirmation. `S` inside the
monitor loads another sector without going back to Setup.

That is a full disk editor: read, patch, write. It is also the fastest way
to break the machine, and there's nothing between you and that.

### Boot patches

A patch is an address and one byte. Both are written **after the boot
sector is in memory and before the jump into it** — a moment you never get
at on a real machine. Address `0` means the slot is unused.

The BIOS reports what it did (`Boot patches applied: 1`) instead of doing
it quietly. A patch that changes the running system without saying so would
be the one feature in here that lies to you.

### The boot sector

The stock BIOS always reads sector 0. Here the number lives in the coin
cell, in two bytes — one would have stopped at 255, and the disk has 16384.
The `55 AA` at the end of a boot sector is a convention, not a property of
the disk, so it can be switched off for a sector you wrote yourself.

## What it looks like

Green phosphor on black, not BIOS blue — POST, Setup and all four tools.
A colour attribute on the TB-32 is `background << 4 | foreground`, both
indices into the card's 16 colours, so the whole look is seven numbers:

| | | |
|---|---|---|
| `A_BG` / `ATTR_NORMAL` | `0x02` | green on black — frames, rows, hex bytes, key help: everything at rest |
| `A_TITLE` / `ATTR_BRIGHT` | `0x0A` | bright green on black — headings, the address column, measured values |
| `A_SEL` / `ATTR_TITLE` | `0x20` | black on green — the header bar and the selected row, inverse rather than a different hue |
| `ATTR_ERR` | `0x0C` | **stays red.** An error message that looks like everything else isn't one |

Three brightness levels of one colour — that's what a terminal has, and
more would stop being one. Because the rest of the firmware draws through
exactly these four names, the four tools are coloured by the same change.

Each tool also carries a header bar with its name on the left and the chip's
name on the right. That isn't decoration: a tool fills the whole screen, and
then the bar is the only place still saying whose firmware you are inside.

## What was changed

Four of the five files are copies from `firmware/`. Only `hack.asm` is new.

| File | |
|---|---|
| `hack.asm` | **new** — monitor, ports, CMOS, sectors, patches, hex input |
| `bios.asm` | five places: name in the header and in the POST line, one `.include`, the boot sector from CMOS, the skippable signature check, the call to `hk_patch_anwenden` |
| `setup.asm` | the *Hack* tab, six `REG_` numbers, the dispatch branches, F5, the four colour attributes |
| `const.inc` | `HK_*` scratch addresses, `CM_HKBSEC0`…`CM_HKP2V`, the `ATTR_*` colours |
| `video.asm` | unchanged |

### Why F5 resets the boot settings

`Load Setup Defaults` puts the boot sector back to 0 and turns off both
*Ignore Boot Signature* and *Apply Boot Patches*. That is deliberate: those
three are the only settings in here that can leave you with a machine that
won't start, and a BIOS full of sharp edges needs exactly one blunt one.
The patch **addresses** are left alone — they're work, and turning the
patches off is the change you actually want undone.

### Where the settings live

CMOS `0x20`–`0x2D`. Above `0x1F` because `setup_backup` only saves
`0x10`–`0x1F` for ESC — a boot sector set here would otherwise vanish again
on *Exit Without Saving*. At most `0x2D` because the coin cell computes its
own checksum over `0x10`–`0x2D`; `0x2E` is that checksum, `0x2F` the magic
byte, and `0x30`–`0x33` are the clock offset. The CMOS editor shows all of
them and will happily let you write to every one.

**F10 saves.** Everything on the Hack tab is an ordinary CMOS setting and
needs `F10` like the rest of Setup — except the CMOS editor, which has its
own `F10` because you are already editing the coin cell there. ESC does not
undo Hack settings; they sit above the range Setup restores.

## Three honest limitations

**Nothing here is checked.** No address range is refused, no port is
protected, no confirmation stands between you and a bad write — that is the
entire point of this BIOS, and it means you can stop the running machine
from inside Setup. Writing to port `0x0090` cuts the power. Overwriting the
interrupt table at address 0 makes the next keystroke jump into nowhere.
None of that is a bug report.

**Reading a port is not always free.** The console does a real `inr`.
Port `0x0020` hands out a keystroke and is then rid of it; the flash
controller at `0xB0` reports the result of its last command. Devices that
change when read, change when you read them here.

**The monitor cannot show what the machine cannot address.** The dump walks
straight into unmapped space and shows zeros — it doesn't fault, and it
doesn't tell you the difference between "zero" and "nothing there". The
memory map is in [`Doku/02`](../../Doku/02%20Speicherkarte%20und%20Ports.md);
that's the map, this is only the window.

## A pitfall when rebuilding this

Instructions on the TB-32 are a fixed four bytes wide and are fetched from
an address divisible by four. `hack.asm` is included **after** `setup.asm`,
and `setup.asm` ends with its string table — which ends on an odd address.
Without a `.align 4` before the first instruction, everything that follows
starts two bytes off: the machine still boots, POST runs cleanly, and the
first `DEL` takes it apart with *Invalid opcode*. The same line for the same
reason is in TB-LOCK. There is a second one at the end of this file, before
the pointer table `s_hk_patnamen` — `.dw` wants to be aligned too.
