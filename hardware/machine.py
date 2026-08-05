"""
The machine as a whole: mainboard with CPU, bus, and all expansion cards.

This class knows nothing about pygame -- it also runs completely without a
screen. That's intentional: it lets the PC boot automatically in tests.
"""

import os
import time

from hardware.isa import (
    PORT_PIC_ACK, PORT_PIC_MASK, PORT_TIMER_HZ, PORT_TIMER_TICKS,
    PORT_NET_STATUS, PORT_NET_ADDR, PORT_NET_LEN, PORT_NET_CMD,
    PORT_NET_MAC_HI, PORT_NET_MAC_LO, PORT_NET_ZAEHLER, PORT_NET_ZINDEX,
    PORT_KBD_DATA, PORT_KBD_STATUS,
    PORT_DISK_LBA, PORT_DISK_COUNT, PORT_DISK_ADDR, PORT_DISK_CMD,
    PORT_DISK_STATUS, PORT_DISK_SIZE,
    PORT_VGA_MODE, PORT_VGA_CURSOR, PORT_VGA_PALIDX, PORT_VGA_PALVAL,
    PORT_BLT_X, PORT_BLT_Y, PORT_BLT_W, PORT_BLT_H, PORT_BLT_COL,
    PORT_BLT_CMD, PORT_BLT_CHR, PORT_BLT_SRC, PORT_BLT_BG,
    PORT_MCUR_X, PORT_MCUR_Y, PORT_MCUR_ON, PORT_GFX_DOPPEL, PORT_GFX_TAUSCH, PORT_BLT_ZOOM,
    PORT_BLT_ZIEL, PORT_BLT_ZIELB, PORT_BLT_ZIELH,
    PORT_DMA_SRC, PORT_DMA_DST, PORT_DMA_LEN, PORT_DMA_VAL, PORT_DMA_CMD,
    PORT_SPK_FREQ, PORT_SPK_ON,
    PORT_MOUSE_X, PORT_MOUSE_Y, PORT_MOUSE_BTN, PORT_MOUSE_WHEEL,
    PORT_CMOS_IDX, PORT_CMOS_DATA, PORT_DEBUG, PORT_POWER,
    PORT_TEMP, PORT_FAN, PORT_THROTTLE, PORT_TEMP_LIMIT, PORT_FANMODE,
    PORT_TEMP_MAX,
    PORT_FLASH_CMD, PORT_FLASH_SIZE, PORT_FLASH_ADDR, ROM_SIZE,
)
from hardware.bus import Bus
from hardware.cpu import CPU
from hardware.devices import (DMA, VGA, Keyboard, Disk, Timer, Speaker, Mouse, CMOS,
                              Power, Thermal, Flash, Netzkarte, CMOS_CPUSPEED)

# Selectable clock speeds in BIOS setup (instructions per second)
CPU_SPEEDS = [400_000, 1_000_000, 2_000_000, 4_000_000, 8_000_000]
CPU_SPEED_NAMES = ["0.4 MHz (Eco)", "1 MHz", "2 MHz (Standard)",
                   "4 MHz (Turbo)", "8 MHz (Overclocked!)"]


class DebugPort:
    """Everything the BIOS/OS writes out here ends up in the developer log.
    Extremely useful for debugging firmware before the screen is running."""

    def __init__(self):
        self.line = ""
        self.log = []

    def port_out(self, port, value):
        ch = value & 0xFF
        if ch == 10:
            self.log.append(self.line)
            print("[DBG]", self.line)
            self.line = ""
        else:
            self.line += chr(ch)

    def port_in(self, port):
        return 0


class Machine:
    def __init__(self, root, rom=None, disk=None, cmos=None):
        self.root = root
        self.rom_path = rom or os.path.join(root, "firmware", "bios.bin")
        self.disk_path = disk or os.path.join(root, "disk", "hd0.img")
        self.cmos_path = cmos or os.path.join(root, "disk", "cmos.bin")

        self.cpu_ref = [None]                      # devices need the CPU for IRQs
        self.vga = VGA()
        self.bus = Bus(self.vga)
        self.cpu = CPU(self.bus)
        self.cpu_ref[0] = self.cpu

        self.keyboard = Keyboard(self.cpu_ref)
        self.timer = Timer(self.cpu_ref)
        self.speaker = Speaker()
        self.mouse = Mouse(self.cpu_ref)
        self.cmos = CMOS(self.cmos_path)
        self.power = Power()
        self.thermal = Thermal()
        self.debug = DebugPort()
        self.disk = Disk(self.disk_path)
        self.disk.attach_bus(self.bus)

        b = self.bus
        b.register(self.timer, [PORT_TIMER_HZ, PORT_TIMER_TICKS])
        b.register(self.keyboard, [PORT_KBD_DATA, PORT_KBD_STATUS])
        b.register(self.disk, [PORT_DISK_LBA, PORT_DISK_COUNT, PORT_DISK_ADDR,
                               PORT_DISK_CMD, PORT_DISK_STATUS, PORT_DISK_SIZE])
        b.register(self.vga, [PORT_VGA_MODE, PORT_VGA_CURSOR,
                              PORT_VGA_PALIDX, PORT_VGA_PALVAL,
                              PORT_BLT_X, PORT_BLT_Y, PORT_BLT_W, PORT_BLT_H,
                              PORT_BLT_COL, PORT_BLT_CMD, PORT_BLT_CHR,
                              PORT_BLT_SRC, PORT_BLT_BG,
                              PORT_MCUR_X, PORT_MCUR_Y, PORT_MCUR_ON,
                              PORT_GFX_DOPPEL, PORT_GFX_TAUSCH,
                              PORT_BLT_ZOOM,
                              PORT_BLT_ZIEL, PORT_BLT_ZIELB,
                              PORT_BLT_ZIELH])
        self.vga.bus = b
        self.dma = DMA()
        self.dma.bus = b
        b.register(self.dma, [PORT_DMA_SRC, PORT_DMA_DST, PORT_DMA_LEN,
                              PORT_DMA_VAL, PORT_DMA_CMD])
        b.register(self.speaker, [PORT_SPK_FREQ, PORT_SPK_ON])
        b.register(self.mouse, [PORT_MOUSE_X, PORT_MOUSE_Y, PORT_MOUSE_BTN,
                                PORT_MOUSE_WHEEL])
        b.register(self.cmos, [PORT_CMOS_IDX, PORT_CMOS_DATA])
        b.register(self.power, [PORT_POWER])
        b.register(self.thermal, [PORT_TEMP, PORT_FAN, PORT_THROTTLE,
                                  PORT_TEMP_LIMIT, PORT_FANMODE, PORT_TEMP_MAX])
        b.register(self.debug, [PORT_DEBUG])
        self.flash = Flash(self.rom_path)
        self.flash.bus = b
        b.register(self.flash, [PORT_FLASH_CMD, PORT_FLASH_SIZE,
                                PORT_FLASH_ADDR])
        self.netz = Netzkarte(self.cpu_ref)
        self.netz.bus = b
        b.register(self.netz, [PORT_NET_STATUS, PORT_NET_ADDR, PORT_NET_LEN,
                               PORT_NET_CMD, PORT_NET_MAC_HI, PORT_NET_MAC_LO,
                               PORT_NET_ZAEHLER, PORT_NET_ZINDEX])
        b.register(self, [PORT_PIC_ACK, PORT_PIC_MASK])

        self.running = False
        self.gehaeuse = False        # set by pc.py: there's a startup screen
        self.neustart_wunsch = False
        self.bios_test = False
        self.total_instructions = 0
        self.zeitmangel = 0          # how often the frame budget wasn't enough
        self.letzte_ips = 0

    # our PIC (interrupt controller) is hardwired and only acknowledges
    def port_out(self, port, value):
        pass

    def port_in(self, port):
        return 0

    # -- Power on/off --------------------------------------------------

    # -- The BIOS chip -----------------------------------------------------

    @staticmethod
    def rom_pruefen(daten):
        """Is this a valid BIOS image?

        Layout (see Doc 16): jump, then "TBBI", length, checksum.
        This check deliberately lives in the MAINBOARD and not in the
        firmware -- broken firmware can't validate itself."""
        if len(daten) < 16 or len(daten) > ROM_SIZE:
            return False
        if daten[4:8] != b"TBBI":
            return False
        laenge = int.from_bytes(daten[8:12], "little")
        if laenge != len(daten) or laenge % 4:
            return False
        roh = bytearray(daten)
        roh[12:16] = b"\x00\x00\x00\x00"
        h = 0x1234
        for i in range(0, len(roh), 4):
            h = (h * 31 + int.from_bytes(roh[i:i + 4], "little")) & 0xFFFFFFFF
        return h == int.from_bytes(daten[12:16], "little")

    @staticmethod
    def rom_name(daten):
        """The name the BIOS states about itself in its header.

        It sits at offset 0x10, 32 bytes, terminated by a null byte. The
        mainboard shows it in the center of the screen at power-on -- so
        every BIOS gets its own name, and the startup screen still looks
        the same everywhere.

        Older images don't have this field; at that position there's
        already code instead. So it's validated: printable, terminated by
        a null byte, followed by nothing but null bytes. If that doesn't
        match, there's simply no name."""
        feld = daten[0x10:0x30]
        if len(feld) < 32 or b"\x00" not in feld:
            return None
        name, rest = feld.split(b"\x00", 1)
        if not name or rest.strip(b"\x00"):
            return None
        if any(b < 0x20 or b > 0x7E for b in name):
            return None
        return name.decode("latin-1")

    def _rom_laden(self):
        """Loads the BIOS -- and falls back to the backup if the chip
        contains garbage. This is Dual BIOS: on real boards a second chip
        likewise takes over if the first one fails its self-test. Without
        this, a failed flash would be permanent."""
        haupt = None
        if os.path.exists(self.rom_path):
            with open(self.rom_path, "rb") as f:
                haupt = f.read()
        if haupt is not None and self.rom_pruefen(haupt):
            return haupt

        sicherung = os.path.splitext(self.rom_path)[0] + ".backup.bin"
        if os.path.exists(sicherung):
            with open(sicherung, "rb") as f:
                alt = f.read()
            if self.rom_pruefen(alt):
                with open(self.rom_path, "wb") as f:      # write the chip back
                    f.write(alt)
                self.rom_gerettet = True
                return alt

        if haupt is None:
            raise FileNotFoundError(
                f"No BIOS found ({self.rom_path}).\n"
                f"Please run 'python3 build.py' first.")
        raise ValueError(
            f"The BIOS image ({self.rom_path}) has no valid "
            f"TBBI signature or an invalid checksum, and there is no "
            f"usable backup next to it.\n"
            f"Back to factory state: python3 build.py")

    def power_on(self):
        self.rom_gerettet = False
        self.bios_test = False
        # A registered test image applies to exactly this one boot.
        # It is consumed here -- the next boot fetches the real chip again.
        # An attempt that hangs therefore costs nothing more than a press
        # of the reset button.
        rom = None
        if self.flash.einmal is not None:
            probe = self.flash.einmal
            self.flash.einmal = None
            if self.rom_pruefen(probe):
                rom = probe
                self.bios_test = True
        if rom is None:
            rom = self._rom_laden()
        self.bios_name = self.rom_name(rom)
        self.bus.load_rom(rom)
        self.bus.ram[:] = b"\x00" * len(self.bus.ram)
        self.vga.clear_text()
        self.vga.mode = VGA.MODE_TEXT
        self.cpu.reset()
        self.timer.hz = 0
        self.power.request = None
        self.running = True

    def power_off(self):
        self.running = False
        self.cpu.powered = False

    @property
    def ips_soll(self):
        """The clock speed set in the BIOS."""
        idx = self.cmos.data[CMOS_CPUSPEED]
        return CPU_SPEEDS[idx if idx < len(CPU_SPEEDS) else 2]

    @property
    def ips(self):
        """Clock speed after throttling: if the machine gets too warm, it
        computes more slowly on its own -- just like any modern processor."""
        return max(50_000, int(self.ips_soll * (100 - self.thermal.throttle) / 100))

    # -- Run a time slice --------------------------------------------

    def run_slice(self, dt, max_ms=None):
        """Lets the machine work for dt seconds of virtual time.

        max_ms limits how much REAL compute time may be spent on this. That
        way the window always gets enough time to draw and to handle
        keypresses -- on a real PC, after all, the graphics card reads out
        video memory 60 times per second even while the processor is fully
        loaded. If the host machine's compute power isn't enough, the
        virtual machine simply runs slower, instead of the interface
        freezing."""
        if not self.running:
            return 0
        self.timer.advance(dt)
        self.netz.poll()                 # pick up mail from the wire
        budget = max(1, int(self.ips * min(dt, 0.1)))

        if max_ms is None:
            n = self.cpu.run(budget)
        else:
            frist = time.perf_counter() + max_ms / 1000.0
            n = 0
            while n < budget:
                n += self.cpu.run(min(4000, budget - n))   # small chunks, so
                                                          # the deadline is honored precisely
                if self.cpu.halted or not self.running:
                    break
                if time.perf_counter() > frist:
                    self.zeitmangel += 1
                    break
        self.total_instructions += n
        self.letzte_ips = n / dt if dt > 0 else 0

        # Thermal model: how loaded was the processor during this time slice?
        auslastung = min(1.0, n / budget) if budget > 0 else 0.0
        # The EFFECTIVE clock speed generates heat -- otherwise throttling
        # wouldn't help at all.
        self.thermal.advance(dt, auslastung, self.ips / 1000000.0)
        if self.thermal.notaus:
            self.thermal.notaus = False
            self.power_off()

        if self.disk.led and time.time() > self.disk.busy_until:
            self.disk.led = False

        if self.power.request == "off":
            self.power_off()
        elif self.power.request == "reboot":
            # Don't power back on immediately: a reboot goes through the
            # case's startup screen, otherwise you'd see neither the BIOS
            # name nor the think-time delay -- and a registered test image
            # would start up without any indication.
            #
            # Anyone using the machine without a case (tests, headless
            # operation) still gets the reboot on its own.
            self.power.request = None
            self.neustart_wunsch = True
            if self.gehaeuse:
                self.power_off()
            else:
                self.power_on()
        return n

    def shutdown(self):
        self.netz.close()
        self.disk.close()
        self.cmos.save()
