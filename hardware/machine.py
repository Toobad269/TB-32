"""
Der Rechner als Ganzes: Mainboard mit CPU, Bus und allen Steckkarten.

Diese Klasse kennt kein pygame -- sie läuft auch komplett ohne Bildschirm.
Das ist Absicht: so kann ich den PC in Tests automatisch booten lassen.
"""

import os
import time

from hardware.isa import (
    PORT_PIC_ACK, PORT_PIC_MASK, PORT_TIMER_HZ, PORT_TIMER_TICKS,
    PORT_KBD_DATA, PORT_KBD_STATUS,
    PORT_DISK_LBA, PORT_DISK_COUNT, PORT_DISK_ADDR, PORT_DISK_CMD,
    PORT_DISK_STATUS, PORT_DISK_SIZE,
    PORT_VGA_MODE, PORT_VGA_CURSOR, PORT_VGA_PALIDX, PORT_VGA_PALVAL,
    PORT_BLT_X, PORT_BLT_Y, PORT_BLT_W, PORT_BLT_H, PORT_BLT_COL,
    PORT_BLT_CMD, PORT_BLT_CHR, PORT_BLT_SRC, PORT_BLT_BG,
    PORT_MCUR_X, PORT_MCUR_Y, PORT_MCUR_ON, PORT_GFX_DOPPEL, PORT_GFX_TAUSCH, PORT_BLT_ZOOM,
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
                              Power, Thermal, Flash, CMOS_CPUSPEED)

# Auswählbare Taktraten im BIOS-Setup (Befehle pro Sekunde)
CPU_SPEEDS = [400_000, 1_000_000, 2_000_000, 4_000_000, 8_000_000]
CPU_SPEED_NAMES = ["0.4 MHz (Eco)", "1 MHz", "2 MHz (Standard)",
                   "4 MHz (Turbo)", "8 MHz (Übertaktet!)"]


class DebugPort:
    """Alles, was das BIOS/OS hier hinausschreibt, landet im Entwickler-Log.
    Extrem nützlich, um Firmware zu debuggen, bevor der Bildschirm läuft."""

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

        self.cpu_ref = [None]                      # Geräte brauchen die CPU für IRQs
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
                              PORT_BLT_ZOOM])
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
        b.register(self, [PORT_PIC_ACK, PORT_PIC_MASK])

        self.running = False
        self.gehaeuse = False        # setzt pc.py: es gibt ein Startbild
        self.neustart_wunsch = False
        self.bios_test = False
        self.total_instructions = 0
        self.zeitmangel = 0          # wie oft das Bildbudget nicht reichte
        self.letzte_ips = 0

    # PIC (Interrupt-Controller) ist bei uns fest verdrahtet und quittiert nur
    def port_out(self, port, value):
        pass

    def port_in(self, port):
        return 0

    # -- Ein-/Ausschalten --------------------------------------------------

    # -- Der BIOS-Chip -----------------------------------------------------

    @staticmethod
    def rom_pruefen(daten):
        """Ist das ein gültiges BIOS-Abbild?

        Aufbau (siehe Doku 16): Sprung, dann "TBBI", Länge, Prüfsumme.
        Diese Prüfung sitzt bewusst im MAINBOARD und nicht in der Firmware --
        eine kaputte Firmware kann sich nicht selbst prüfen."""
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
        """Der Name, den das BIOS in seinem Kopf über sich selbst angibt.

        Er steht auf Position 0x10, 32 Byte, mit Nullbyte abgeschlossen. Das
        Mainboard zeigt ihn beim Einschalten in der Bildmitte -- so hat jedes
        BIOS seinen eigenen Namen, und das Startbild sieht trotzdem überall
        gleich aus.

        Ältere Abbilder haben das Feld nicht, dort steht an der Stelle schon
        Code. Deshalb wird geprüft: druckbar, mit Nullbyte beendet, danach
        nur noch Nullbytes. Passt das nicht, gibt es eben keinen Namen."""
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
        """Holt das BIOS -- und greift auf die Sicherung zurück, wenn der
        Chip Unsinn enthält. Das ist Dual BIOS: auf echten Boards springt
        genauso ein zweiter Baustein ein, wenn der erste nicht durch den
        Selbsttest kommt. Ohne das wäre ein misslungenes Flashen endgültig."""
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
                with open(self.rom_path, "wb") as f:      # Chip zurückschreiben
                    f.write(alt)
                self.rom_gerettet = True
                return alt

        if haupt is None:
            raise FileNotFoundError(
                f"Kein BIOS gefunden ({self.rom_path}).\n"
                f"Bitte zuerst 'python3 build.py' ausführen.")
        raise ValueError(
            f"Das BIOS-Abbild ({self.rom_path}) hat keine gültige "
            f"TBBI-Kennung oder eine falsche Prüfsumme, und es gibt keine "
            f"brauchbare Sicherung daneben.\n"
            f"Zurück zum Auslieferungszustand: python3 build.py")

    def power_on(self):
        self.rom_gerettet = False
        self.bios_test = False
        # Ein angemeldetes Testabbild gilt fuer genau diesen einen Start.
        # Es wird hier verbraucht -- der naechste Start holt wieder den
        # echten Chip. Ein Versuch, der haengenbleibt, kostet damit nichts
        # weiter als einen Druck auf den Reset-Knopf.
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
        """Der im BIOS eingestellte Takt."""
        idx = self.cmos.data[CMOS_CPUSPEED]
        return CPU_SPEEDS[idx if idx < len(CPU_SPEEDS) else 2]

    @property
    def ips(self):
        """Takt nach Abzug der Drosselung: wird der Rechner zu warm, rechnet
        er von selbst langsamer -- so wie jeder heutige Prozessor."""
        return max(50_000, int(self.ips_soll * (100 - self.thermal.throttle) / 100))

    # -- Zeitscheibe ausführen --------------------------------------------

    def run_slice(self, dt, max_ms=None):
        """Lässt den Rechner für dt Sekunden virtueller Zeit arbeiten.

        max_ms begrenzt, wie viel ECHTE Rechenzeit dafür verbraucht werden
        darf. Das Fenster bekommt so immer genug Zeit zum Zeichnen und für
        Tastendrücke -- bei einem echten PC liest die Grafikkarte den
        Bildspeicher schließlich auch dann 60-mal je Sekunde aus, wenn der
        Prozessor gerade voll ausgelastet ist. Reicht die Rechenleistung des
        Wirtsrechners nicht, läuft die virtuelle Maschine eben langsamer,
        statt dass die Oberfläche stehenbleibt."""
        if not self.running:
            return 0
        self.timer.advance(dt)
        budget = max(1, int(self.ips * min(dt, 0.1)))

        if max_ms is None:
            n = self.cpu.run(budget)
        else:
            frist = time.perf_counter() + max_ms / 1000.0
            n = 0
            while n < budget:
                n += self.cpu.run(min(4000, budget - n))   # feine Haeppchen, damit
                                                          # die Frist genau greift
                if self.cpu.halted or not self.running:
                    break
                if time.perf_counter() > frist:
                    self.zeitmangel += 1
                    break
        self.total_instructions += n
        self.letzte_ips = n / dt if dt > 0 else 0

        # Wärmemodell: wie ausgelastet war der Prozessor in dieser Zeitscheibe?
        auslastung = min(1.0, n / budget) if budget > 0 else 0.0
        # Der EFFEKTIVE Takt heizt -- sonst würde Drosseln nichts bringen.
        self.thermal.advance(dt, auslastung, self.ips / 1000000.0)
        if self.thermal.notaus:
            self.thermal.notaus = False
            self.power_off()

        if self.disk.led and time.time() > self.disk.busy_until:
            self.disk.led = False

        if self.power.request == "off":
            self.power_off()
        elif self.power.request == "reboot":
            # Nicht sofort wieder anschalten: ein Neustart geht durch das
            # Startbild des Gehaeuses, sonst saehe man weder den Namen des
            # BIOS noch die Bedenkzeit -- und ein angemeldetes Testabbild
            # wuerde ohne jeden Hinweis losstarten.
            #
            # Wer die Maschine ohne Gehaeuse benutzt (Tests, kopfloser
            # Betrieb), bekommt den Neustart weiterhin von allein.
            self.power.request = None
            self.neustart_wunsch = True
            if self.gehaeuse:
                self.power_off()
            else:
                self.power_on()
        return n

    def shutdown(self):
        self.disk.close()
        self.cmos.save()
