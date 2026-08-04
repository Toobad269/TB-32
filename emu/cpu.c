/* ==========================================================================
   Der TB-32-Prozessor in C

   Zeile fuer Zeile dieselbe Maschine wie in hardware/cpu.py -- dieselben
   57 Befehle, dieselben Flags, dieselbe Reihenfolge beim Interrupt. Wenn
   die beiden Fassungen jemals unterschiedlich rechnen, ist eine davon
   falsch; genau dafuer gibt es den Vergleichstest.

   Der Ablauf ist der eines jeden echten Prozessors:
       1. HOLEN        vier Bytes an der Adresse im Programmzaehler
       2. DEKODIEREN   welcher Befehl, welche Register
       3. AUSFUEHREN   rechnen, Speicher anfassen, springen
       4. Interrupts   meldet sich Hardware?
   ========================================================================== */

#include "tb32.h"
#include <string.h>
#include <stdio.h>

/* --- Flags ---------------------------------------------------------------
   Genau die Rechnung aus cpu.py. Uebertrag und Ueberlauf sind zwei
   verschiedene Dinge: der Uebertrag gilt fuer vorzeichenlose Zahlen, der
   Ueberlauf fuer vorzeichenbehaftete. */

static void flags_logic(Machine *m, uint32_t res) {
    uint32_t f = m->flags & ~(FLAG_Z | FLAG_N | FLAG_C | FLAG_V);
    if (res == 0)   f |= FLAG_Z;
    if (res & SIGN) f |= FLAG_N;
    m->flags = f;
}

static void flags_add(Machine *m, uint32_t a, uint32_t b, uint64_t full) {
    uint32_t res = (uint32_t)full;
    uint32_t f = m->flags & ~(FLAG_Z | FLAG_N | FLAG_C | FLAG_V);
    if (res == 0)   f |= FLAG_Z;
    if (res & SIGN) f |= FLAG_N;
    if (full > 0xFFFFFFFFull) f |= FLAG_C;
    if ((~(a ^ b) & (a ^ res)) & SIGN) f |= FLAG_V;
    m->flags = f;
}

static uint32_t flags_sub(Machine *m, uint32_t a, uint32_t b) {
    uint32_t res = a - b;
    uint32_t f = m->flags & ~(FLAG_Z | FLAG_N | FLAG_C | FLAG_V);
    if (res == 0)   f |= FLAG_Z;
    if (res & SIGN) f |= FLAG_N;
    if (a < b)      f |= FLAG_C;              /* "Borrow" */
    if (((a ^ b) & (a ^ res)) & SIGN) f |= FLAG_V;
    m->flags = f;
    return res;
}

static int cond_true(uint32_t f, int cond) {
    int Z = (f & FLAG_Z) != 0;
    int N = (f & FLAG_N) != 0;
    int C = (f & FLAG_C) != 0;
    int V = (f & FLAG_V) != 0;
    switch (cond) {
        case 0:  return 1;
        case 1:  return Z;
        case 2:  return !Z;
        case 3:  return C;
        case 4:  return !C;
        case 5:  return N;
        case 6:  return !N;
        case 7:  return V;
        case 8:  return !V;
        case 9:  return C || Z;
        case 10: return !C && !Z;
        case 11: return N != V;
        case 12: return N == V;
        case 13: return Z || (N != V);
        case 14: return !Z && (N == V);
        default: return 0;
    }
}

/* --- Interrupt ----------------------------------------------------------- */

static int enter_interrupt(Machine *m, int vector) {
    uint32_t handler = bus_read32(m, IVT_BASE + (uint32_t)vector * 4);
    uint32_t sp;
    if (handler == 0) return 0;               /* kein Handler eingetragen */
    sp = m->r[15] - 4;
    bus_write32(m, sp, m->flags);
    sp -= 4;
    bus_write32(m, sp, m->pc);
    m->r[15] = sp;
    m->flags &= ~FLAG_I;
    m->pc = handler;
    m->halted = 0;
    return 1;
}

static void software_interrupt(Machine *m, int vector) {
    if (!enter_interrupt(m, vector)) {
        snprintf(m->fault, sizeof m->fault,
                 "INT 0x%02X ohne Handler bei PC=0x%08X", vector, m->pc);
        m->halted = 1;
    }
}

/* --- Speicherzugriffe im heissen Pfad ------------------------------------
   Fuer Adressen im RAM greifen wir direkt zu; alles andere (Bildspeicher,
   ROM) laeuft ueber den Bus. Das ist derselbe Trick wie die Wortsicht in
   der Python-Fassung, nur dass er in C nichts kostet. */

static inline uint32_t ld32(Machine *m, uint32_t a) {
    if (a + 3 < RAM_SIZE) {
        const uint8_t *p = m->ram + a;
        return (uint32_t)p[0] | ((uint32_t)p[1] << 8)
             | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
    }
    return bus_read32(m, a);
}

static inline void st32(Machine *m, uint32_t a, uint32_t v) {
    if (a + 3 < RAM_SIZE) {
        uint8_t *p = m->ram + a;
        p[0] = (uint8_t)v;
        p[1] = (uint8_t)(v >> 8);
        p[2] = (uint8_t)(v >> 16);
        p[3] = (uint8_t)(v >> 24);
        return;
    }
    bus_write32(m, a, v);
}

/* ==========================================================================
   Der eigentliche Takt
   ========================================================================== */

int cpu_run(Machine *m, int budget) {
    uint32_t *r = m->r;
    uint32_t pc = m->pc;
    uint32_t flags = m->flags;
    int executed = 0;

    while (executed < budget) {
        uint32_t word, npc, res, a, b, imm, sp;
        int32_t simm, off;
        uint64_t full;
        int op, rd, ra, rb;

        /* --- Interrupts vor dem naechsten Befehl ---------------------- */
        if (m->irq_pending && (flags & FLAG_I)) {
            uint32_t anliegend = m->irq_pending;
            int bit = 0;
            while (((anliegend >> bit) & 1) == 0) bit++;
            m->irq_pending &= ~(1u << bit);
            m->pc = pc;
            m->flags = flags;
            enter_interrupt(m, bit);
            pc = m->pc;
            flags = m->flags;
            if (m->halted) {
                m->cycles += (uint64_t)executed;
                return executed;
            }
        }

        /* --- HOLEN ---------------------------------------------------- */
        word = ld32(m, pc);
        npc = pc + 4;
        executed++;

        /* --- DEKODIEREN ----------------------------------------------- */
        op = (int)(word >> 24);
        rd = (int)((word >> 20) & 0xF);
        ra = (int)((word >> 16) & 0xF);

        switch (op) {

        case 0x40:                            /* push */
            sp = r[15] - 4;
            r[15] = sp;
            st32(m, sp, r[rd]);
            break;

        case 0x41:                            /* pop */
            sp = r[15];
            r[rd] = ld32(m, sp);
            r[15] = sp + 4;
            break;

        case 0x1B:                            /* ldw rd, [ra+off] */
            imm = word & 0xFFFF;
            simm = (imm & 0x8000) ? (int32_t)imm - 0x10000 : (int32_t)imm;
            r[rd] = ld32(m, r[ra] + (uint32_t)simm);
            break;

        case 0x10:                            /* mov */
            r[rd] = r[ra];
            break;

        case 0x1E:                            /* stw [ra+off], rd */
            imm = word & 0xFFFF;
            simm = (imm & 0x8000) ? (int32_t)imm - 0x10000 : (int32_t)imm;
            st32(m, r[ra] + (uint32_t)simm, r[rd]);
            break;

        case 0x11:                            /* movi */
            imm = word & 0xFFFF;
            simm = (imm & 0x8000) ? (int32_t)imm - 0x10000 : (int32_t)imm;
            r[rd] = (uint32_t)simm;
            break;

        case 0x50:                            /* jmp / bedingter Sprung */
            if (cond_true(flags, rd)) {
                off = (int32_t)(word & 0xFFFFF);
                if (off & 0x80000) off -= 0x100000;
                npc = pc + (uint32_t)(off * 4);
            }
            break;

        case 0x20:                            /* add */
            rb = (int)((word >> 12) & 0xF);
            a = r[ra]; b = r[rb];
            full = (uint64_t)a + (uint64_t)b;
            r[rd] = (uint32_t)full;
            m->flags = flags;
            flags_add(m, a, b, full);
            flags = m->flags;
            break;

        case 0x30:                            /* addi */
            imm = word & 0xFFFF;
            simm = (imm & 0x8000) ? (int32_t)imm - 0x10000 : (int32_t)imm;
            r[rd] = r[ra] + (uint32_t)simm;
            break;

        case 0x18:                            /* ldb */
            imm = word & 0xFFFF;
            simm = (imm & 0x8000) ? (int32_t)imm - 0x10000 : (int32_t)imm;
            r[rd] = bus_read8(m, r[ra] + (uint32_t)simm);
            break;

        case 0x13:                            /* movh */
            imm = word & 0xFFFF;
            r[rd] = (r[rd] & 0xFFFF) | (imm << 16);
            break;

        case 0x2D:                            /* cmp rd, ra */
            /* Achtung: der Vergleich geht ueber rd und ra, NICHT ueber ra
               und rb. Beim Portieren einmal verwechselt -- der
               Vergleichstest gegen die Python-Fassung hat es nach 13
               Befehlen gefunden. */
            m->flags = flags;
            flags_sub(m, r[rd], r[ra]);
            flags = m->flags;
            break;

        case 0x42:                            /* call */
            off = (int32_t)(word & 0xFFFFFF);
            if (off & 0x800000) off -= 0x1000000;
            sp = r[15] - 4;
            r[15] = sp;
            st32(m, sp, npc);
            npc = pc + (uint32_t)(off * 4);
            break;

        case 0x31:                            /* subi */
            imm = word & 0xFFFF;
            simm = (imm & 0x8000) ? (int32_t)imm - 0x10000 : (int32_t)imm;
            r[rd] = r[ra] - (uint32_t)simm;
            break;

        case 0x05:                            /* ret */
            sp = r[15];
            npc = ld32(m, sp);
            r[15] = sp + 4;
            break;

        case 0x1C:                            /* stb */
            imm = word & 0xFFFF;
            simm = (imm & 0x8000) ? (int32_t)imm - 0x10000 : (int32_t)imm;
            bus_write8(m, r[ra] + (uint32_t)simm, (uint8_t)r[rd]);
            break;

        case 0x3D:                            /* cmpi rd, imm */
            imm = word & 0xFFFF;
            simm = (imm & 0x8000) ? (int32_t)imm - 0x10000 : (int32_t)imm;
            m->flags = flags;
            flags_sub(m, r[rd], (uint32_t)simm);
            flags = m->flags;
            break;

        case 0x21:                            /* sub */
            rb = (int)((word >> 12) & 0xF);
            m->flags = flags;
            r[rd] = flags_sub(m, r[ra], r[rb]);
            flags = m->flags;
            break;

        case 0x25:                            /* and */
            rb = (int)((word >> 12) & 0xF);
            res = r[ra] & r[rb];
            r[rd] = res;
            m->flags = flags; flags_logic(m, res); flags = m->flags;
            break;

        case 0x28:                            /* shl */
            rb = (int)((word >> 12) & 0xF);
            b = r[rb] & 31;
            res = r[ra] << b;
            r[rd] = res;
            m->flags = flags; flags_logic(m, res); flags = m->flags;
            break;

        case 0x00:                            /* nop */
            break;

        case 0x01:                            /* hlt */
            m->halted = 1;
            m->pc = npc;
            m->flags = flags;
            m->cycles += (uint64_t)executed;
            return executed;

        case 0x02:                            /* cli */
            flags &= ~FLAG_I;
            break;

        case 0x03:                            /* sti */
            flags |= FLAG_I;
            break;

        case 0x04:                            /* iret */
            sp = r[15];
            npc = bus_read32(m, sp);
            flags = bus_read32(m, sp + 4);
            r[15] = sp + 8;
            break;

        case 0x06:                            /* brk */
            m->halted = 1;
            snprintf(m->fault, sizeof m->fault, "Haltepunkt bei 0x%08X", pc);
            m->pc = npc;
            m->flags = flags;
            m->cycles += (uint64_t)executed;
            return executed;

        case 0x19:                            /* ldsb -- mit Vorzeichen */
            imm = word & 0xFFFF;
            simm = (imm & 0x8000) ? (int32_t)imm - 0x10000 : (int32_t)imm;
            b = bus_read8(m, r[ra] + (uint32_t)simm);
            r[rd] = (b & 0x80) ? (b | 0xFFFFFF00u) : b;
            break;

        case 0x1A:                            /* ldh */
            imm = word & 0xFFFF;
            simm = (imm & 0x8000) ? (int32_t)imm - 0x10000 : (int32_t)imm;
            a = r[ra] + (uint32_t)simm;
            r[rd] = (uint32_t)bus_read8(m, a) | ((uint32_t)bus_read8(m, a + 1) << 8);
            break;

        case 0x1D:                            /* sth */
            imm = word & 0xFFFF;
            simm = (imm & 0x8000) ? (int32_t)imm - 0x10000 : (int32_t)imm;
            a = r[ra] + (uint32_t)simm;
            bus_write8(m, a, (uint8_t)r[rd]);
            bus_write8(m, a + 1, (uint8_t)(r[rd] >> 8));
            break;

        case 0x22:                            /* mul */
            rb = (int)((word >> 12) & 0xF);
            full = (uint64_t)(int64_t)(int32_t)r[ra] * (int64_t)(int32_t)r[rb];
            res = (uint32_t)full;
            r[rd] = res;
            m->flags = flags; flags_logic(m, res); flags = m->flags;
            break;

        case 0x23:                            /* div (mit Vorzeichen) */
        case 0x24:                            /* mod */
        {
            int32_t x = (int32_t)r[ra];
            int32_t y;
            rb = (int)((word >> 12) & 0xF);
            y = (int32_t)r[rb];
            if (y == 0) {
                m->pc = pc;
                m->flags = flags;
                software_interrupt(m, 0);
                pc = m->pc; flags = m->flags;
                if (m->halted) { m->cycles += executed; return executed; }
                continue;
            }
            /* Wie in Python: Richtung zur Null hin abschneiden */
            if (op == 0x23) res = (uint32_t)(x / y);
            else            res = (uint32_t)(x % y);
            r[rd] = res;
            m->flags = flags; flags_logic(m, res); flags = m->flags;
            break;
        }

        case 0x26:                            /* or */
            rb = (int)((word >> 12) & 0xF);
            res = r[ra] | r[rb];
            r[rd] = res;
            m->flags = flags; flags_logic(m, res); flags = m->flags;
            break;

        case 0x27:                            /* xor */
            rb = (int)((word >> 12) & 0xF);
            res = r[ra] ^ r[rb];
            r[rd] = res;
            m->flags = flags; flags_logic(m, res); flags = m->flags;
            break;

        case 0x29:                            /* shr (logisch) */
            rb = (int)((word >> 12) & 0xF);
            res = r[ra] >> (r[rb] & 31);
            r[rd] = res;
            m->flags = flags; flags_logic(m, res); flags = m->flags;
            break;

        case 0x2A:                            /* sar (arithmetisch) */
            rb = (int)((word >> 12) & 0xF);
            res = (uint32_t)((int32_t)r[ra] >> (r[rb] & 31));
            r[rd] = res;
            m->flags = flags; flags_logic(m, res); flags = m->flags;
            break;

        case 0x2B:                            /* not */
            res = ~r[ra];
            r[rd] = res;
            m->flags = flags; flags_logic(m, res); flags = m->flags;
            break;

        case 0x2C:                            /* neg */
            res = (uint32_t)(-(int32_t)r[ra]);
            r[rd] = res;
            m->flags = flags; flags_logic(m, res); flags = m->flags;
            break;

        case 0x2E:                            /* tst rd, ra */
            m->flags = flags; flags_logic(m, r[rd] & r[ra]); flags = m->flags;
            break;

        case 0x2F:                            /* udiv */
        case 0x3F:                            /* umod */
            rb = (int)((word >> 12) & 0xF);
            if (r[rb] == 0) {
                m->pc = pc;
                m->flags = flags;
                software_interrupt(m, 0);
                pc = m->pc; flags = m->flags;
                if (m->halted) { m->cycles += executed; return executed; }
                continue;
            }
            if (op == 0x2F) res = r[ra] / r[rb];
            else            res = r[ra] % r[rb];
            r[rd] = res;
            m->flags = flags; flags_logic(m, res); flags = m->flags;
            break;

        case 0x32:                            /* muli */
            imm = word & 0xFFFF;
            simm = (imm & 0x8000) ? (int32_t)imm - 0x10000 : (int32_t)imm;
            r[rd] = (uint32_t)((int32_t)r[ra] * simm);
            break;

        case 0x33:                            /* divi */
            imm = word & 0xFFFF;
            simm = (imm & 0x8000) ? (int32_t)imm - 0x10000 : (int32_t)imm;
            if (simm == 0) {
                m->pc = pc; m->flags = flags;
                software_interrupt(m, 0);
                pc = m->pc; flags = m->flags;
                if (m->halted) { m->cycles += executed; return executed; }
                continue;
            }
            r[rd] = (uint32_t)((int32_t)r[ra] / simm);
            break;

        case 0x34:                            /* modi */
            imm = word & 0xFFFF;
            simm = (imm & 0x8000) ? (int32_t)imm - 0x10000 : (int32_t)imm;
            if (simm == 0) {
                m->pc = pc; m->flags = flags;
                software_interrupt(m, 0);
                pc = m->pc; flags = m->flags;
                if (m->halted) { m->cycles += executed; return executed; }
                continue;
            }
            r[rd] = (uint32_t)((int32_t)r[ra] % simm);
            break;

        case 0x35:                            /* andi */
            imm = word & 0xFFFF;
            res = r[ra] & imm;
            r[rd] = res;
            m->flags = flags; flags_logic(m, res); flags = m->flags;
            break;

        case 0x36:                            /* ori */
            imm = word & 0xFFFF;
            res = r[ra] | imm;
            r[rd] = res;
            m->flags = flags; flags_logic(m, res); flags = m->flags;
            break;

        case 0x37:                            /* xori */
            imm = word & 0xFFFF;
            res = r[ra] ^ imm;
            r[rd] = res;
            m->flags = flags; flags_logic(m, res); flags = m->flags;
            break;

        case 0x38:                            /* shli */
            imm = word & 0x1F;
            r[rd] = r[ra] << imm;
            break;

        case 0x39:                            /* shri */
            imm = word & 0x1F;
            r[rd] = r[ra] >> imm;
            break;

        case 0x3A:                            /* sari */
            imm = word & 0x1F;
            r[rd] = (uint32_t)((int32_t)r[ra] >> imm);
            break;

        case 0x3E:                            /* tsti rd, imm */
            imm = word & 0xFFFF;
            m->flags = flags; flags_logic(m, r[rd] & imm); flags = m->flags;
            break;

        case 0x43:                            /* callr rd */
            sp = r[15] - 4;
            r[15] = sp;
            st32(m, sp, npc);
            npc = r[rd];                      /* Zielregister steht in rd */
            break;

        case 0x44:                            /* pushf */
            sp = r[15] - 4;
            r[15] = sp;
            st32(m, sp, flags);
            break;

        case 0x45:                            /* popf */
            sp = r[15];
            flags = ld32(m, sp);
            r[15] = sp + 4;
            break;

        case 0x51:                            /* jmpr rd */
            npc = r[rd];                      /* ebenfalls rd, nicht ra */
            break;

        case 0x60:                            /* in rd, port */
            imm = word & 0xFFFF;
            m->pc = pc; m->flags = flags;
            r[rd] = port_in(m, imm);
            break;

        case 0x61:                            /* inr rd, ra */
            m->pc = pc; m->flags = flags;
            r[rd] = port_in(m, r[ra] & 0xFFFF);
            break;

        case 0x62:                            /* out port, rd */
            imm = word & 0xFFFF;
            m->pc = pc; m->flags = flags;
            port_out(m, imm, r[rd]);
            if (!m->powered) {
                m->pc = npc; m->flags = flags;
                m->cycles += executed;
                return executed;
            }
            break;

        case 0x63:                            /* outr ra, rd */
            m->pc = pc; m->flags = flags;
            port_out(m, r[ra] & 0xFFFF, r[rd]);
            if (!m->powered) {
                m->pc = npc; m->flags = flags;
                m->cycles += executed;
                return executed;
            }
            break;

        case 0x64:                            /* int n */
            imm = word & 0xFFFF;
            imm &= 0xFF;
            m->pc = npc;                      /* Ruecksprung hinter das int */
            m->flags = flags;
            software_interrupt(m, (int)imm);
            pc = m->pc;
            flags = m->flags;
            if (m->halted) { m->cycles += executed; return executed; }
            continue;

        default:
            snprintf(m->fault, sizeof m->fault,
                     "Unbekannter Befehl 0x%02X bei PC=0x%08X", op, pc);
            m->halted = 1;
            m->pc = npc;
            m->flags = flags;
            m->cycles += (uint64_t)executed;
            return executed;
        }

        pc = npc;
    }

    m->pc = pc;
    m->flags = flags;
    m->cycles += (uint64_t)executed;
    return executed;
}
