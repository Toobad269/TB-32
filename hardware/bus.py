"""
Der Systembus -- die Leiterbahnen des Mainboards.

Er entscheidet bei jedem Speicherzugriff, WER gemeint ist:
    0x00000000  RAM (16 MiB)
    0x02000000  Bildspeicher Textmodus
    0x02100000  Bildspeicher Grafikmodus
    0x0F000000  BIOS-ROM (nur lesen -- ein Schreibversuch verpufft, wie echt)

Und er verteilt IN/OUT-Befehle an die richtigen Geräte.
"""

from hardware.isa import (
    RAM_SIZE, ROM_BASE, ROM_SIZE,
    VRAM_TEXT, VRAM_TEXT_SIZE, VRAM_GFX, VRAM_GFX_SIZE,
)


class Bus:
    def __init__(self, vga):
        self.ram = bytearray(RAM_SIZE)
        self.rom = bytearray(ROM_SIZE)
        self.vga = vga
        self.port_devices = {}
        self.unknown_ports = set()

    # -- Geräte anmelden ---------------------------------------------------

    def register(self, device, ports):
        for p in ports:
            self.port_devices[p] = device

    def load_rom(self, data):
        if len(data) > ROM_SIZE:
            raise ValueError(f"BIOS ist zu groß: {len(data)} > {ROM_SIZE}")
        self.rom = bytearray(ROM_SIZE)
        self.rom[:len(data)] = data

    # -- Adressdekodierung -------------------------------------------------

    def _region(self, addr):
        if addr < RAM_SIZE:
            return self.ram, addr
        if VRAM_TEXT <= addr < VRAM_TEXT + VRAM_TEXT_SIZE:
            self.vga.dirty = True
            return self.vga.text, addr - VRAM_TEXT
        if VRAM_GFX <= addr < VRAM_GFX + VRAM_GFX_SIZE:
            self.vga.dirty = True
            return self.vga.gfx, addr - VRAM_GFX
        if ROM_BASE <= addr < ROM_BASE + ROM_SIZE:
            return self.rom, addr - ROM_BASE
        return None, 0

    def read8(self, addr):
        mem, off = self._region(addr)
        return mem[off] if mem is not None else 0xFF

    def read16(self, addr):
        mem, off = self._region(addr)
        if mem is None or off + 1 >= len(mem):
            return 0xFFFF
        return mem[off] | (mem[off + 1] << 8)

    def read32(self, addr):
        mem, off = self._region(addr)
        if mem is None or off + 3 >= len(mem):
            return 0xFFFFFFFF
        return (mem[off] | (mem[off + 1] << 8) |
                (mem[off + 2] << 16) | (mem[off + 3] << 24))

    def write8(self, addr, value):
        if ROM_BASE <= addr < ROM_BASE + ROM_SIZE:
            return                                   # ROM ist schreibgeschützt
        mem, off = self._region(addr)
        if mem is not None and off < len(mem):
            mem[off] = value & 0xFF

    def write16(self, addr, value):
        if ROM_BASE <= addr < ROM_BASE + ROM_SIZE:
            return
        mem, off = self._region(addr)
        if mem is not None and off + 1 < len(mem):
            mem[off] = value & 0xFF
            mem[off + 1] = (value >> 8) & 0xFF

    def write32(self, addr, value):
        if ROM_BASE <= addr < ROM_BASE + ROM_SIZE:
            return
        mem, off = self._region(addr)
        if mem is not None and off + 3 < len(mem):
            mem[off] = value & 0xFF
            mem[off + 1] = (value >> 8) & 0xFF
            mem[off + 2] = (value >> 16) & 0xFF
            mem[off + 3] = (value >> 24) & 0xFF

    # -- Blocktransfer (DMA der Festplatte) --------------------------------

    def write_block(self, addr, data):
        mem, off = self._region(addr)
        if mem is None:
            return
        n = min(len(data), len(mem) - off)
        mem[off:off + n] = data[:n]
        if mem is not self.ram:
            self.vga.dirty = True

    def read_block(self, addr, length):
        mem, off = self._region(addr)
        if mem is None:
            return b"\x00" * length
        return bytes(mem[off:off + length]).ljust(length, b"\x00")

    # -- Ein-/Ausgabe ------------------------------------------------------

    def port_in(self, port):
        dev = self.port_devices.get(port)
        if dev is None:
            self.unknown_ports.add(port)
            return 0
        return dev.port_in(port) or 0

    def port_out(self, port, value):
        dev = self.port_devices.get(port)
        if dev is None:
            self.unknown_ports.add(port)
            return
        dev.port_out(port, value & 0xFFFFFFFF)
