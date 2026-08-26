"""
SECURITY PoC -- deliberate sandbox-escape demonstrator.

This module is NOT part of the emulated hardware. It exists only to show, on
your own machine, how severe the unprivileged, unauthenticated BIOS flash port
really is. It adds ONE extra flash command:

    command 12  --  write the flash buffer to ANY host path the guest names

The REAL bug cannot do this: the genuine burn (command 3) only ever overwrites
the fixed BIOS file. This PoC widens that into an arbitrary host-file write so
a plain .TBX can drop a real file anywhere in your home directory -- with no
dialog and no click. See security/flash-escape-poc/ for the full write-up and
the fix. Remove this module (and its use in hardware/machine.py) as part of
fixing the escape.

It is kept in its own file, as a subclass, so the real Flash device in
devices.py stays untouched and the whole demonstrator is trivial to delete.
"""

import os

from hardware.devices import Flash

# The RAM address of a NUL-terminated host path, handed in for command 12.
# 0xB3 sits right after the three genuine flash ports (0xB0..0xB2).
PORT_FLASH_PATH = 0x00B3


class FlashPoC(Flash):
    def __init__(self, rom_path):
        super().__init__(rom_path)
        self.pfad_addr = 0                     # RAM address of the target path

    def port_out(self, port, value):
        if port == PORT_FLASH_PATH:
            self.pfad_addr = value
        else:
            super().port_out(port, value)      # ADDR/SIZE/CMD go to the real chip

    def _befehl(self, cmd):
        if cmd == 12:                          # buffer -> arbitrary host file
            if self.bus is None or not self.puffer or not self.pfad_addr:
                return self.KEIN_PUFFER
            roh = bytes(self.bus.read_block(self.pfad_addr, 4096))
            roh = roh.split(b"\x00", 1)[0]     # cut at the NUL terminator
            try:
                pfad = os.path.expanduser(roh.decode("utf-8", "replace"))
                with open(pfad, "wb") as f:
                    f.write(self.puffer)
            except OSError:
                return self.SCHREIBFEHLER
            return self.OK
        return super()._befehl(cmd)            # everything else = the real chip
