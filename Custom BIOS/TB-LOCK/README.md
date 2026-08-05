# TB-LOCK

The stock TOOBAD BIOS, extended by exactly one thing: **a password in
front of Setup.** Without the password, nobody gets at clock speed,
boot device, Secure Boot, or the flash button anymore.

```bash
python3 "Custom BIOS/TB-LOCK/bauen.py"      # builds TB-LOCK.bin
python3 "Custom BIOS/TB-LOCK/pruefen.py"    # 27 checks on the real machine
```

Installing it: **DEL → Firmware → Flash BIOS from File**, then select
`TB-LOCK.bin` and power the machine off and back on.

## How it's used

Setup gets a new tab, **Password**, between *Security* and *Firmware*:

| Row | |
|---|---|
| **Supervisor Password** | shows `Installed` or `Not Installed` |
| **Set / Change Password** | set up or change it |
| **Clear Password** | remove it again |

**Setting it up:** ENTER on *Set / Change Password*, type the new
password, ENTER, type it again, ENTER. If the two entries don't match,
nothing happens — so you can't lock yourself out with a typo. An empty
password is rejected.

**Changing it:** the same row. It first asks for the old password,
then twice for the new one.

**Clearing it:** *Clear Password*, then the current password. After
that, Setup is open again.

A password that's set takes effect **immediately and permanently** — it
isn't confirmed with F10 first, and ESC ("Exit Without Saving") doesn't
undo it. If you've just typed it in twice, you expect it to stick.

**At boot:** `DEL` or `F2` no longer go straight into Setup; instead you
land at a *BIOS Setup is locked* window. Three failed attempts, and
Setup stays locked while the machine continues booting normally. The
password shows up as asterisks.

## What was changed

Four of the five files are copies from `firmware/`. Only
`passwort.asm` is new; the others have a handful of extra lines.

| File | |
|---|---|
| `passwort.asm` | **new** — input, checksum, gate, the two buttons |
| `bios.asm` | three lines: name in the header, `setup_tor` instead of `setup_main` in two places, one `.include` |
| `setup.asm` | the *Password* tab, three `REG_` numbers, two dispatch branches |
| `const.inc` | `CM_PWFLAG`, `CM_PWSUM0..3`, `PW_BUF1/2` |
| `video.asm` | unchanged |

### Why the gate sits in **two** places

`setup_main` was originally called from two places: on a normal `DEL`
during the grace period — and from the **red Secure Boot screen**, when
the boot image is no longer the known one. That's exactly where the
*Trust Current Boot Image* button lives.

If only the first path were guarded, there'd be an open back door:
deliberately corrupt the boot image, hit the red screen, `DEL`, Setup
with no password. Both paths therefore go through `setup_tor`.
`pruefen.py` tests exactly that, by turning on Secure Boot, tampering
with the boot sector, and then trying to get in via the red screen.

### Where the password lives

In the coin-cell backed memory: `CM_PWFLAG` (0x20) says whether one is
set, `CM_PWSUM0..3` (0x21–0x24) hold the checksum.

These slots sit **above 0x1F** — that's not a coincidence.
`setup_backup` saves 0x10–0x1F and restores it on ESC; a password
stored there would silently vanish again when exiting without saving.
But they sit **below 0x2E**, so that the checksum computation over the
CMOS includes them.

`Load Setup Defaults` (F5) leaves them alone — just like on a real
board.

## Two honest limitations

**The coin cell.** The CMOS is the file `disk/cmos.bin`. Delete it, and
the password is gone. On a real mainboard you'd pull the battery or
move the jumper for that — so this is faithful to the original, not a
bug. It does mean, though: the password protects against someone **at
the TB-32**, not against someone **at the host machine**.

**The checksum.** It's computed as `h = h * 31 + character`, starting
from `0x1234` — the same computation TOOBAD-OS uses for login and
`build.py` uses for the BIOS header. This is a **checksum, not a
cryptographic hash function**: anyone reading the four bytes from
`cmos.bin` can find a different password with the same sum in seconds.
The same honesty applies in `Doku/13` for Secure Boot, and for the same
reason: the principle is real, tamper-resistance isn't.

What it **reliably** does: stop someone from casually changing the
clock speed, changing the boot device, turning off Secure Boot, or
flashing a foreign BIOS.

## A pitfall when rebuilding this

Instructions on the TB-32 are a fixed four bytes wide and are fetched
from an address divisible by four. `passwort.asm` is included **after**
`setup.asm`, and `setup.asm` ends with its string table — which ends on
an odd address. Without a `.align 4` before the first instruction, all
the code that follows starts two bytes off. The machine still boots,
POST runs cleanly, and on the first `DEL` it falls apart with *Invalid
opcode*.

That's exactly what happened on the first build. The line is in there
now, and it's commented.
