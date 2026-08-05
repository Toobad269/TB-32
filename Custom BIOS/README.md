# Custom BIOS

Custom firmware for the TB-32. Each subfolder is a complete BIOS image
with its own name, its own source, and its own test.

| | |
|---|---|
| [`TB-LOCK/`](TB-LOCK/) | The stock BIOS plus a **Setup password**. Without the password, nobody gets into Setup anymore |
| [`COMPANY-OS/`](COMPANY-OS/) | A **company BIOS**: power-on password, owner entry, blocked programs, event log, Secure Boot in three stages, flash lock in the chip itself |
| [`TB-HACK/`](TB-HACK/) | The other direction — a **tinkerer's BIOS**: hex monitor, port console, raw CMOS editor, sector editor, free choice of boot sector, boot patches, and a disassembler that reads a `.TBX` off the host without its source. In green phosphor instead of BIOS blue |

## Why this works

The TB-32's BIOS chip is swappable. At power-on, the mainboard only
checks two things in the image's header — the `TBBI` marker and a
checksum — and falls back to the backup otherwise. What's behind that
is none of the board's business. How a custom BIOS needs to be
structured is fully documented in
[`Doku/16 Eigenes BIOS schreiben.md`](../Doku/16%20Eigenes%20BIOS%20schreiben.md);
the smallest working template is `firmware/minimal.asm`.

## Building and using a custom BIOS

Every folder works the same way — `bauen.py` builds, `pruefen.py` boots the
machine with the result and checks it:

```bash
python3 "Custom BIOS/TB-LOCK/bauen.py"      # produces TB-LOCK.bin
python3 "Custom BIOS/TB-LOCK/pruefen.py"    # boots the machine with it and checks it
```

It's installed on the running machine via: **DEL → Firmware → Flash BIOS
from File**. The board saves the old BIOS as a backup beforehand, and
*Restore Backup BIOS* brings it back — so if you lock yourself out, you
can still get back in.

The finished `.bin` **ships in the folder** — if you just want to flash
it, there's nothing to build. This is the exception to the project's
rule that build output isn't checked in; here, a BIOS image is the
deliverable, not just an intermediate step. If you build it yourself
anyway, you get the exact same bytes. The `.sym` symbol table stays out,
since only the debugger needs that.

## Adding another one

New folder, source files inside, `bauen.py` alongside them. The folders
are independent of each other: each one has its own copy of `bios.asm`,
`setup.asm`, `video.asm`, and `const.inc` and doesn't touch `firmware/`.
Anyone changing the stock BIOS has to deliberately bring the change
over — which means nothing tried in here can break the regular machine.

That independence is also why `TB-LOCK` and `TB-HACK` can be opposites
without either one having to know about the other.
