"""
Die TB-32 CPU -- das Herz des Rechners.

Sie macht exakt das, was jeder echte Prozessor macht, in einer Endlosschleife:
    1. HOLEN    (fetch)   -- 4 Bytes an der Adresse im Programmzähler lesen
    2. DEKODIEREN(decode) -- welcher Befehl ist das, welche Register?
    3. AUSFÜHREN (execute)-- rechnen, Speicher anfassen, springen
    4. Interrupts prüfen  -- meldet sich Hardware (Timer, Tastatur)?

Alles darüber (BIOS, Betriebssystem, Programme) ist NICHT in Python geschrieben,
sondern läuft als echter Maschinencode auf dieser CPU.
"""

from hardware.isa import (
    RAM_SIZE, RESET_VECTOR, IVT_BASE,
    FLAG_Z, FLAG_N, FLAG_C, FLAG_V, FLAG_I,
)

MASK = 0xFFFFFFFF
SIGN = 0x80000000


class CPUException(Exception):
    pass


class CPU:
    def __init__(self, bus):
        self.bus = bus
        self.r = [0] * 16
        self.pc = RESET_VECTOR
        self.flags = 0
        self.halted = False
        self.powered = True

        self.cycles = 0
        self.irq_pending = 0          # Bitmaske: Bit n = IRQ n liegt an
        self.irq_vectors = {}         # Bit -> Interruptnummer
        self.breakpoints = set()
        self.trace = False
        self.last_fault = None
        self._view()

    def _view(self):
        """Sicht auf den Arbeitsspeicher als 32-Bit-Woerter.

        Jeder Befehl ist vier Byte lang und liegt auf einer durch vier
        teilbaren Adresse. Ihn Byte fuer Byte zusammenzuschieben kostet vier
        Zugriffe plus Schieben und Verodern -- ueber diese Sicht ist es ein
        einziger Zugriff. Das ist der groesste einzelne Hebel im Emulator,
        weil es bei JEDEM Befehl anfaellt.

        Achtung: Die Sicht haengt an genau diesem bytearray. bus.ram wird
        beim Einschalten nur ueberschrieben (ram[:] = ...), nicht ersetzt --
        deshalb bleibt sie gueltig. Wer bus.ram jemals neu zuweist, muss
        _view() erneut aufrufen.""" 
        self.words = memoryview(self.bus.ram).cast("I")

    # -- Reset -------------------------------------------------------------

    def reset(self):
        self.r = [0] * 16
        self.r[15] = 0x000FFFF0       # Stack wächst nach unten
        self.pc = RESET_VECTOR
        self.flags = 0
        self.halted = False
        self.cycles = 0
        self.irq_pending = 0

    # -- Hardware-Interrupt anmelden --------------------------------------

    def raise_irq(self, vector):
        """Ein Gerät meldet sich. Wird beim nächsten Befehl bearbeitet."""
        self.irq_pending |= 1 << vector
        if self.flags & FLAG_I:
            self.halted = False

    def _enter_interrupt(self, vector):
        """Wie beim echten PC: Flags und Rücksprungadresse auf den Stack,
        Interrupts sperren, und über die Vektortabelle zum Handler springen."""
        handler = self.bus.read32(IVT_BASE + vector * 4)
        if handler == 0:
            return False                      # kein Handler installiert
        sp = (self.r[15] - 4) & MASK
        self.bus.write32(sp, self.flags)
        sp = (sp - 4) & MASK
        self.bus.write32(sp, self.pc)
        self.r[15] = sp
        self.flags &= ~FLAG_I
        self.pc = handler
        self.halted = False
        return True

    def software_interrupt(self, vector):
        if not self._enter_interrupt(vector):
            self.last_fault = f"INT 0x{vector:02X} ohne Handler bei PC=0x{self.pc:08X}"
            self.halted = True

    # -- Flags -------------------------------------------------------------

    def _flags_logic(self, res):
        f = self.flags & ~(FLAG_Z | FLAG_N | FLAG_C | FLAG_V)
        if res == 0:
            f |= FLAG_Z
        if res & SIGN:
            f |= FLAG_N
        self.flags = f

    def _flags_add(self, a, b, full):
        res = full & MASK
        f = self.flags & ~(FLAG_Z | FLAG_N | FLAG_C | FLAG_V)
        if res == 0:
            f |= FLAG_Z
        if res & SIGN:
            f |= FLAG_N
        if full > MASK:
            f |= FLAG_C
        if (~(a ^ b) & (a ^ res)) & SIGN:
            f |= FLAG_V
        self.flags = f

    def _flags_sub(self, a, b):
        res = (a - b) & MASK
        f = self.flags & ~(FLAG_Z | FLAG_N | FLAG_C | FLAG_V)
        if res == 0:
            f |= FLAG_Z
        if res & SIGN:
            f |= FLAG_N
        if a < b:
            f |= FLAG_C                       # "Borrow" = unsigned kleiner
        if ((a ^ b) & (a ^ res)) & SIGN:
            f |= FLAG_V
        self.flags = f
        return res

    def _cond_true(self, cond):
        f = self.flags
        Z = bool(f & FLAG_Z)
        N = bool(f & FLAG_N)
        C = bool(f & FLAG_C)
        V = bool(f & FLAG_V)
        if cond == 0:  return True
        if cond == 1:  return Z
        if cond == 2:  return not Z
        if cond == 3:  return C
        if cond == 4:  return not C
        if cond == 5:  return N
        if cond == 6:  return not N
        if cond == 7:  return V
        if cond == 8:  return not V
        if cond == 9:  return C or Z              # unsigned <=
        if cond == 10: return not C and not Z     # unsigned >
        if cond == 11: return N != V              # signed <
        if cond == 12: return N == V              # signed >=
        if cond == 13: return Z or (N != V)       # signed <=
        if cond == 14: return not Z and (N == V)  # signed >
        return False

    # -- Der eigentliche Prozessor-Takt ------------------------------------

    def run(self, budget):
        """Führt bis zu <budget> Befehle aus. Gibt zurück, wie viele es waren.

        Der Programmzähler und das Flagregister leben während der Schleife in
        lokalen Variablen -- in Python kostet jeder Zugriff auf self.xxx ein
        Vielfaches davon, und diese beiden werden bei JEDEM Befehl angefasst.
        Vor jedem Aufruf, der sie braucht (Interrupts), werden sie
        zurückgeschrieben und danach wieder eingelesen."""
        bus = self.bus
        ram = bus.ram
        words = self.words
        r = self.r
        pc = self.pc
        flags = self.flags
        executed = 0
        # Die anstehenden Interrupts liegen waehrend der Schleife in einer
        # lokalen Variable. Das darf man, weil alle drei Quellen (Timer,
        # Tastatur, Maus) von AUSSERHALB dieser Schleife melden -- aus
        # run_slice und der Fensterschleife. Nur die Portbefehle unten lesen
        # sicherheitshalber neu, falls je ein Baustein beim Zugriff meldet.
        irq = self.irq_pending
        # Haltepunkte aendert nur der Debugger, nie die laufende Maschine --
        # also einmal holen statt bei jedem Befehl nachzuschlagen.
        bp = self.breakpoints

        while executed < budget:
            # --- Interrupts vor dem nächsten Befehl bearbeiten -------------
            if irq and (flags & FLAG_I):
                bit = (self.irq_pending & -self.irq_pending).bit_length() - 1
                self.irq_pending &= ~(1 << bit)
                self.pc = pc
                self.flags = flags
                self._enter_interrupt(bit)
                pc = self.pc
                flags = self.flags
                irq = self.irq_pending
                if self.halted:              # kein Handler -> Panik
                    self.cycles += executed
                    return executed

            # --- HOLEN ----------------------------------------------------
            if pc < RAM_SIZE:
                if pc & 3:                      # krumme Adresse: Byte fuer Byte
                    word = ram[pc] | (ram[pc + 1] << 8) | (ram[pc + 2] << 16) | (ram[pc + 3] << 24)
                else:
                    word = words[pc >> 2]
            else:
                word = bus.read32(pc)

            npc = pc + 4
            executed += 1

            # --- DEKODIEREN -----------------------------------------------
            # Nur was JEDER Befehl braucht. rb, imm und simm holt sich der
            # jeweilige Zweig selbst -- push, pop und mov (zusammen ueber die
            # Haelfte aller Befehle) brauchen sie gar nicht.
            op = word >> 24
            rd = (word >> 20) & 0xF
            ra = (word >> 16) & 0xF

            # --- AUSFÜHREN ------------------------------------------------
            # Reihenfolge nach GEMESSENER Häufigkeit: Python prüft die Kette
            # von oben nach unten, jeder Vergleich kostet Zeit. Während eines
            # Compilerlaufs sind push und pop zusammen 40 % aller Befehle,
            # ldw 13 %, mov 11 % -- die stehen deshalb ganz vorn.
            # Nachmessen: tools/opstat.py

            if op == 0x40:                   # push
                sp = (r[15] - 4) & MASK
                r[15] = sp
                if sp + 3 < RAM_SIZE:
                    if sp & 3:
                        v = r[rd]
                        ram[sp] = v & 0xFF
                        ram[sp+1] = (v >> 8) & 0xFF
                        ram[sp+2] = (v >> 16) & 0xFF
                        ram[sp+3] = (v >> 24) & 0xFF
                    else:
                        words[sp >> 2] = r[rd]
                else:
                    bus.write32(sp, r[rd])

            elif op == 0x41:                   # pop
                sp = r[15]
                if sp + 3 < RAM_SIZE:
                    if sp & 3:
                        r[rd] = ram[sp] | (ram[sp+1] << 8) | (ram[sp+2] << 16) | (ram[sp+3] << 24)
                    else:
                        r[rd] = words[sp >> 2]
                else:
                    r[rd] = bus.read32(sp)
                r[15] = (sp + 4) & MASK

            elif op == 0x1B:                     # ldw rd, [ra+off]
                imm = word & 0xFFFF
                simm = imm - 0x10000 if imm & 0x8000 else imm
                a = (r[ra] + simm) & MASK
                if a + 3 < RAM_SIZE:
                    if a & 3:
                        r[rd] = ram[a] | (ram[a+1] << 8) | (ram[a+2] << 16) | (ram[a+3] << 24)
                    else:
                        r[rd] = words[a >> 2]
                else:
                    r[rd] = bus.read32(a)

            elif op == 0x10:                   # mov
                r[rd] = r[ra]

            elif op == 0x1E:                   # stw [ra+off], rd
                imm = word & 0xFFFF
                simm = imm - 0x10000 if imm & 0x8000 else imm
                a = (r[ra] + simm) & MASK
                v = r[rd]
                if a + 3 < RAM_SIZE:
                    if a & 3:
                        ram[a] = v & 0xFF
                        ram[a+1] = (v >> 8) & 0xFF
                        ram[a+2] = (v >> 16) & 0xFF
                        ram[a+3] = (v >> 24) & 0xFF
                    else:
                        words[a >> 2] = v
                else:
                    bus.write32(a, v)

            elif op == 0x11:                   # movi
                imm = word & 0xFFFF
                simm = imm - 0x10000 if imm & 0x8000 else imm
                r[rd] = simm & MASK

            elif op == 0x50:                   # jmp / bedingter Sprung
                if rd == 0:
                    nimm = True
                elif rd == 1:   nimm = (flags & 1) != 0          # jz
                elif rd == 2:   nimm = (flags & 1) == 0          # jnz
                elif rd == 3:   nimm = (flags & 4) != 0          # jc  / jb
                elif rd == 4:   nimm = (flags & 4) == 0          # jnc / jae
                elif rd == 5:   nimm = (flags & 2) != 0          # jn
                elif rd == 6:   nimm = (flags & 2) == 0
                elif rd == 7:   nimm = (flags & 8) != 0
                elif rd == 8:   nimm = (flags & 8) == 0
                elif rd == 9:   nimm = (flags & 5) != 0          # jbe
                elif rd == 10:  nimm = (flags & 5) == 0          # ja
                elif rd == 11:  nimm = ((flags >> 1) & 1) != ((flags >> 3) & 1)
                elif rd == 12:  nimm = ((flags >> 1) & 1) == ((flags >> 3) & 1)
                elif rd == 13:  nimm = (flags & 1) != 0 or \
                                       ((flags >> 1) & 1) != ((flags >> 3) & 1)
                elif rd == 14:  nimm = (flags & 1) == 0 and \
                                       ((flags >> 1) & 1) == ((flags >> 3) & 1)
                else:           nimm = False
                if nimm:
                    off = word & 0xFFFFF
                    if off & 0x80000:
                        off -= 0x100000
                    npc = (pc + off * 4) & MASK

            elif op == 0x20:                   # add
                rb = (word >> 12) & 0xF
                a = r[ra]; b = r[rb]
                full = a + b
                res = full & MASK
                r[rd] = res
                flags = flags & ~15
                if res == 0: flags |= 1
                if res & SIGN: flags |= 2
                if full > MASK: flags |= 4
                if (~(a ^ b) & (a ^ res)) & SIGN: flags |= 8

            elif op == 0x30:                   # addi
                imm = word & 0xFFFF
                simm = imm - 0x10000 if imm & 0x8000 else imm
                r[rd] = (r[ra] + simm) & MASK

            elif op == 0x18:                   # ldb (mit Nullen aufgefüllt)
                imm = word & 0xFFFF
                simm = imm - 0x10000 if imm & 0x8000 else imm
                a = (r[ra] + simm) & MASK
                r[rd] = ram[a] if a < RAM_SIZE else bus.read8(a)

            elif op == 0x13:                   # movh
                imm = word & 0xFFFF
                r[rd] = ((r[rd] & 0xFFFF) | (imm << 16)) & MASK

            elif op == 0x2D:                   # cmp
                a = r[rd]; b = r[ra]
                res = (a - b) & MASK
                flags = flags & ~15
                if res == 0: flags |= 1
                if res & SIGN: flags |= 2
                if a < b: flags |= 4
                if ((a ^ b) & (a ^ res)) & SIGN: flags |= 8

            elif op == 0x42:                   # call
                off = word & 0xFFFFFF
                if off & 0x800000:
                    off -= 0x1000000
                sp = (r[15] - 4) & MASK
                r[15] = sp
                if sp + 3 < RAM_SIZE:
                    ram[sp] = npc & 0xFF
                    ram[sp+1] = (npc >> 8) & 0xFF
                    ram[sp+2] = (npc >> 16) & 0xFF
                    ram[sp+3] = (npc >> 24) & 0xFF
                else:
                    bus.write32(sp, npc)
                npc = (pc + off * 4) & MASK

            elif op == 0x31:                   # subi
                imm = word & 0xFFFF
                simm = imm - 0x10000 if imm & 0x8000 else imm
                r[rd] = (r[ra] - simm) & MASK

            elif op == 0x05:                   # ret
                sp = r[15]
                if sp + 3 < RAM_SIZE:
                    npc = ram[sp] | (ram[sp+1] << 8) | (ram[sp+2] << 16) | (ram[sp+3] << 24)
                else:
                    npc = bus.read32(sp)
                r[15] = (sp + 4) & MASK

            elif op == 0x1C:                   # stb
                imm = word & 0xFFFF
                simm = imm - 0x10000 if imm & 0x8000 else imm
                a = (r[ra] + simm) & MASK
                if a < RAM_SIZE:
                    ram[a] = r[rd] & 0xFF
                else:
                    bus.write8(a, r[rd] & 0xFF)

            elif op == 0x3D:                   # cmpi
                imm = word & 0xFFFF
                simm = imm - 0x10000 if imm & 0x8000 else imm
                a = r[rd]; b = simm & MASK
                res = (a - b) & MASK
                flags = flags & ~15
                if res == 0: flags |= 1
                if res & SIGN: flags |= 2
                if a < b: flags |= 4
                if ((a ^ b) & (a ^ res)) & SIGN: flags |= 8

            elif op == 0x21:                   # sub
                rb = (word >> 12) & 0xF
                a = r[ra]; b = r[rb]
                res = (a - b) & MASK
                r[rd] = res
                flags = flags & ~15
                if res == 0: flags |= 1
                if res & SIGN: flags |= 2
                if a < b: flags |= 4
                if ((a ^ b) & (a ^ res)) & SIGN: flags |= 8

            elif op == 0x25:                   # and
                rb = (word >> 12) & 0xF
                r[rd] = r[ra] & r[rb]
                flags = flags & ~15
                if r[rd] == 0: flags |= 1
                if r[rd] & SIGN: flags |= 2

            elif op == 0x28:                   # shl
                rb = (word >> 12) & 0xF
                r[rd] = (r[ra] << (r[rb] & 31)) & MASK
                flags = flags & ~15
                if r[rd] == 0: flags |= 1
                if r[rd] & SIGN: flags |= 2

            elif op == 0x00:                   # nop
                pass

            elif op == 0x01:                   # hlt
                self.halted = True
                self.pc = npc
                self.flags = flags
                self.cycles += executed
                return executed

            elif op == 0x02:                   # cli
                flags &= ~FLAG_I

            elif op == 0x03:                   # sti
                flags |= FLAG_I

            elif op == 0x04:                   # iret
                sp = r[15]
                npc = bus.read32(sp)
                flags = bus.read32(sp + 4)
                r[15] = (sp + 8) & MASK

            elif op == 0x06:                   # brk
                self.halted = True
                self.last_fault = f"Haltepunkt bei 0x{pc:08X}"
                self.pc = npc
                self.flags = flags
                self.cycles += executed
                return executed

            elif op == 0x19:                   # ldsb
                imm = word & 0xFFFF
                simm = imm - 0x10000 if imm & 0x8000 else imm
                a = (r[ra] + simm) & MASK
                v = ram[a] if a < RAM_SIZE else bus.read8(a)
                r[rd] = (v - 256) & MASK if v & 0x80 else v

            elif op == 0x1A:                   # ldh
                imm = word & 0xFFFF
                simm = imm - 0x10000 if imm & 0x8000 else imm
                a = (r[ra] + simm) & MASK
                r[rd] = bus.read16(a)

            elif op == 0x1D:                   # sth
                imm = word & 0xFFFF
                simm = imm - 0x10000 if imm & 0x8000 else imm
                a = (r[ra] + simm) & MASK
                bus.write16(a, r[rd] & 0xFFFF)

            elif op == 0x22:                   # mul
                rb = (word >> 12) & 0xF
                a = r[ra]; b = r[rb]
                a = a - 0x100000000 if a & SIGN else a
                b = b - 0x100000000 if b & SIGN else b
                r[rd] = (a * b) & MASK
                flags = flags & ~15
                if r[rd] == 0: flags |= 1
                if r[rd] & SIGN: flags |= 2

            elif op == 0x23 or op == 0x24:     # div / mod (signed)
                rb = (word >> 12) & 0xF
                a = r[ra]; b = r[rb]
                if b == 0:
                    self.pc = npc
                    self.flags = flags
                    self.software_interrupt(0x00)   # Division durch Null
                    pc = self.pc
                    flags = self.flags
                    if self.halted:
                        self.cycles += executed
                        return executed
                    continue
                a = a - 0x100000000 if a & SIGN else a
                b = b - 0x100000000 if b & SIGN else b
                q = abs(a) // abs(b)
                if (a < 0) != (b < 0):
                    q = -q
                r[rd] = (q if op == 0x23 else a - q * b) & MASK
                flags = flags & ~15
                if r[rd] == 0: flags |= 1
                if r[rd] & SIGN: flags |= 2

            elif op == 0x26:                   # or
                rb = (word >> 12) & 0xF
                r[rd] = r[ra] | r[rb]
                flags = flags & ~15
                if r[rd] == 0: flags |= 1
                if r[rd] & SIGN: flags |= 2

            elif op == 0x27:                   # xor
                rb = (word >> 12) & 0xF
                r[rd] = r[ra] ^ r[rb]
                flags = flags & ~15
                if r[rd] == 0: flags |= 1
                if r[rd] & SIGN: flags |= 2

            elif op == 0x29:                   # shr
                rb = (word >> 12) & 0xF
                r[rd] = r[ra] >> (r[rb] & 31)
                flags = flags & ~15
                if r[rd] == 0: flags |= 1
                if r[rd] & SIGN: flags |= 2

            elif op == 0x2A:                   # sar
                rb = (word >> 12) & 0xF
                a = r[ra]
                a = a - 0x100000000 if a & SIGN else a
                r[rd] = (a >> (r[rb] & 31)) & MASK
                flags = flags & ~15
                if r[rd] == 0: flags |= 1
                if r[rd] & SIGN: flags |= 2

            elif op == 0x2B:                   # not
                r[rd] = (~r[ra]) & MASK
                flags = flags & ~15
                if r[rd] == 0: flags |= 1
                if r[rd] & SIGN: flags |= 2

            elif op == 0x2C:                   # neg
                r[rd] = (-r[ra]) & MASK
                flags = flags & ~15
                if r[rd] == 0: flags |= 1
                if r[rd] & SIGN: flags |= 2

            elif op == 0x2E:                   # tst
                flags = flags & ~15
                if r[rd] & r[ra] == 0: flags |= 1
                if r[rd] & r[ra] & SIGN: flags |= 2

            elif op == 0x2F or op == 0x3F:     # udiv / umod
                rb = (word >> 12) & 0xF
                b = r[rb]
                if b == 0:
                    self.pc = npc
                    self.flags = flags
                    self.software_interrupt(0x00)
                    pc = self.pc
                    flags = self.flags
                    if self.halted:
                        self.cycles += executed
                        return executed
                    continue
                r[rd] = (r[ra] // b if op == 0x2F else r[ra] % b) & MASK
                flags = flags & ~15
                if r[rd] == 0: flags |= 1
                if r[rd] & SIGN: flags |= 2

            elif op == 0x32:                   # muli
                imm = word & 0xFFFF
                simm = imm - 0x10000 if imm & 0x8000 else imm
                r[rd] = (r[ra] * simm) & MASK

            elif op == 0x33:                   # divi
                imm = word & 0xFFFF
                simm = imm - 0x10000 if imm & 0x8000 else imm
                r[rd] = (r[ra] // simm) & MASK if simm else 0

            elif op == 0x34:                   # modi
                imm = word & 0xFFFF
                simm = imm - 0x10000 if imm & 0x8000 else imm
                r[rd] = (r[ra] % simm) & MASK if simm else 0

            elif op == 0x35:                   # andi
                imm = word & 0xFFFF
                r[rd] = r[ra] & imm
                flags = flags & ~15
                if r[rd] == 0: flags |= 1
                if r[rd] & SIGN: flags |= 2

            elif op == 0x36:                   # ori
                imm = word & 0xFFFF
                r[rd] = r[ra] | imm
                flags = flags & ~15
                if r[rd] == 0: flags |= 1
                if r[rd] & SIGN: flags |= 2

            elif op == 0x37:                   # xori
                imm = word & 0xFFFF
                r[rd] = r[ra] ^ imm
                flags = flags & ~15
                if r[rd] == 0: flags |= 1
                if r[rd] & SIGN: flags |= 2

            elif op == 0x38:                   # shli
                imm = word & 0xFFFF
                r[rd] = (r[ra] << (imm & 31)) & MASK

            elif op == 0x39:                   # shri
                imm = word & 0xFFFF
                r[rd] = r[ra] >> (imm & 31)

            elif op == 0x3A:                   # sari
                imm = word & 0xFFFF
                a = r[ra]
                a = a - 0x100000000 if a & SIGN else a
                r[rd] = (a >> (imm & 31)) & MASK

            elif op == 0x3E:                   # tsti
                imm = word & 0xFFFF
                flags = flags & ~15
                if r[rd] & imm == 0: flags |= 1
                if r[rd] & imm & SIGN: flags |= 2

            elif op == 0x43:                   # callr
                sp = (r[15] - 4) & MASK
                r[15] = sp
                bus.write32(sp, npc)
                npc = r[rd] & MASK

            elif op == 0x44:                   # pushf
                sp = (r[15] - 4) & MASK
                r[15] = sp
                bus.write32(sp, flags)

            elif op == 0x45:                   # popf
                flags = bus.read32(r[15])
                r[15] = (r[15] + 4) & MASK

            elif op == 0x51:                   # jmpr
                npc = r[rd] & MASK

            elif op == 0x60:                   # in rd, port
                imm = word & 0xFFFF
                r[rd] = bus.port_in(imm) & MASK
                irq = self.irq_pending         # falls der Baustein gemeldet hat

            elif op == 0x61:                   # inr rd, ra
                r[rd] = bus.port_in(r[ra] & 0xFFFF) & MASK
                irq = self.irq_pending

            elif op == 0x62:                   # out port, rd
                imm = word & 0xFFFF
                bus.port_out(imm, r[rd])
                irq = self.irq_pending

            elif op == 0x63:                   # outr ra, rd
                bus.port_out(r[ra] & 0xFFFF, r[rd])
                irq = self.irq_pending

            elif op == 0x64:                   # int n
                imm = word & 0xFFFF
                self.pc = npc
                self.flags = flags
                self.software_interrupt(imm & 0xFF)
                npc = self.pc
                flags = self.flags
                if self.halted:
                    self.pc = npc
                    self.flags = flags
                    self.cycles += executed
                    return executed

            else:
                self.last_fault = (f"Ungültiger Befehl 0x{word:08X} "
                                   f"(Opcode 0x{op:02X}) bei 0x{pc:08X}")
                self.pc = npc
                self.flags = flags
                self.software_interrupt(0x06)
                npc = self.pc
                flags = self.flags
                if self.halted:              # kein Handler fuer den Fehler
                    self.pc = npc
                    self.flags = flags
                    self.cycles += executed
                    return executed

            pc = npc
            if bp and pc in bp:
                self.halted = True
                self.last_fault = f"Haltepunkt bei 0x{pc:08X}"
                break

        self.pc = pc
        self.flags = flags
        self.cycles += executed
        return executed

    # -- Für den Debugger --------------------------------------------------

    def dump(self):
        lines = [f"PC=0x{self.pc:08X}  FLAGS={self.flags:04X} "
                 f"[{'Z' if self.flags & FLAG_Z else '-'}"
                 f"{'N' if self.flags & FLAG_N else '-'}"
                 f"{'C' if self.flags & FLAG_C else '-'}"
                 f"{'V' if self.flags & FLAG_V else '-'}"
                 f"{'I' if self.flags & FLAG_I else '-'}]"]
        for row in range(4):
            lines.append("  ".join(
                f"R{row*4+c:<2}=0x{self.r[row*4+c]:08X}" for c in range(4)))
        return "\n".join(lines)
