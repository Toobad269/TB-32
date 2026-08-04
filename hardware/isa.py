"""
TB-32 Befehlssatz (ISA) -- die "Baupläne" der CPU.

Diese Datei ist die einzige Wahrheit über die Architektur. Sowohl die CPU
(hardware/cpu.py) als auch der Assembler (tools/assembler.py) lesen von hier,
damit die beiden niemals auseinanderlaufen können.

Grundidee (bewusst RISC-artig, wie ARM/RISC-V, nicht wie x86):
  * Jeder Befehl ist GENAU 4 Bytes lang und liegt auf einer 4er-Adresse.
  * 16 Universalregister R0..R15, dazu PC und FLAGS.
  * Konstanten sind max. 16 Bit breit; für 32-Bit-Werte gibt es MOVH.

Befehlsformate (Bit 31 ist links):

  R-Typ:   [31:24] opcode | [23:20] rd | [19:16] ra | [15:12] rb | [11:0] frei
  I-Typ:   [31:24] opcode | [23:20] rd | [19:16] ra | [15:0]  imm16
  J-Typ:   [31:24] opcode | [23:20] cond| [19:0]  offset (in Wörtern, signed)
  C-Typ:   [31:24] opcode | [23:0]  offset (in Wörtern, signed)   -- für CALL
"""

# ---------------------------------------------------------------------------
# Register
# ---------------------------------------------------------------------------

NUM_REGS = 16
SP = 15          # Stackpointer
FP = 14          # Framepointer (vom Compiler benutzt)

REG_ALIASES = {
    "sp": 15,
    "fp": 14,
    "at": 13,    # Hilfsregister des Assemblers (wie $at bei MIPS)
    "rv": 0,     # Rückgabewert-Konvention: R0
}

# ---------------------------------------------------------------------------
# Flags im FLAGS-Register
# ---------------------------------------------------------------------------

FLAG_Z = 1 << 0      # Zero          - Ergebnis war 0
FLAG_N = 1 << 1      # Negative      - höchstes Bit gesetzt
FLAG_C = 1 << 2      # Carry/Borrow  - Übertrag (unsigned)
FLAG_V = 1 << 3      # Overflow      - Vorzeichen-Überlauf (signed)
FLAG_I = 1 << 9      # Interrupt Enable

# ---------------------------------------------------------------------------
# Sprungbedingungen (4 Bit, stehen im rd-Feld des J-Typs)
# ---------------------------------------------------------------------------

COND = {
    "al": 0,    # always            - unbedingter Sprung
    "z":  1, "eq": 1,
    "nz": 2, "ne": 2,
    "c":  3, "b":  3,      # unsigned <
    "nc": 4, "ae": 4,      # unsigned >=
    "n":  5,               # negativ
    "nn": 6,
    "v":  7,
    "nv": 8,
    "be": 9,               # unsigned <=
    "a": 10,               # unsigned >
    "l": 11,               # signed <
    "ge": 12,              # signed >=
    "le": 13,              # signed <=
    "g": 14,               # signed >
}

COND_NAMES = {v: k for k, v in reversed(list(COND.items()))}

# ---------------------------------------------------------------------------
# Opcodes
#
# Format-Kürzel:
#   "n"  - kein Operand            (NOP, HLT, RET, ...)
#   "r"  - ein Register            (PUSH r3)
#   "rr" - zwei Register           (MOV r1, r2)
#   "rrr"- drei Register           (ADD r1, r2, r3)
#   "ri" - Register + Konstante    (MOVI r1, 42)
#   "rri"- Register, Register, Imm (ADDI r1, r2, 4)
#   "mem"- Register + [Basis+Off]  (LDW r1, [r2+8])
#   "j"  - Sprungziel              (JMP label)
#   "c"  - Call-Ziel               (CALL label)
#   "i"  - reine Konstante         (INT 0x10)
# ---------------------------------------------------------------------------

INSTRUCTIONS = {
    # --- Steuerung ------------------------------------------------------
    "nop":   (0x00, "n"),
    "hlt":   (0x01, "n"),     # CPU anhalten bis zum nächsten Interrupt
    "cli":   (0x02, "n"),     # Interrupts sperren
    "sti":   (0x03, "n"),     # Interrupts erlauben
    "iret":  (0x04, "n"),     # Rückkehr aus Interrupt
    "ret":   (0x05, "n"),
    "brk":   (0x06, "n"),     # Haltepunkt für den eingebauten Debugger

    # --- Datentransfer --------------------------------------------------
    "mov":   (0x10, "rr"),    # rd = ra
    "movi":  (0x11, "ri"),    # rd = imm16 (vorzeichenerweitert)
    "movh":  (0x13, "ri"),    # rd = (rd & 0xFFFF) | (imm16 << 16)

    # --- Speicherzugriff (rd, [ra + off16]) -----------------------------
    "ldb":   (0x18, "mem"),   # 8 Bit laden, mit Nullen aufgefüllt
    "ldsb":  (0x19, "mem"),   # 8 Bit laden, vorzeichenerweitert
    "ldh":   (0x1A, "mem"),   # 16 Bit laden
    "ldw":   (0x1B, "mem"),   # 32 Bit laden
    "stb":   (0x1C, "mem"),   # 8 Bit speichern
    "sth":   (0x1D, "mem"),
    "stw":   (0x1E, "mem"),

    # --- Rechenwerk, Register-Variante (rd = ra OP rb) ------------------
    "add":   (0x20, "rrr"),
    "sub":   (0x21, "rrr"),
    "mul":   (0x22, "rrr"),
    "div":   (0x23, "rrr"),   # signed
    "mod":   (0x24, "rrr"),   # signed
    "and":   (0x25, "rrr"),
    "or":    (0x26, "rrr"),
    "xor":   (0x27, "rrr"),
    "shl":   (0x28, "rrr"),
    "shr":   (0x29, "rrr"),   # logisch (Nullen nachziehen)
    "sar":   (0x2A, "rrr"),   # arithmetisch (Vorzeichen nachziehen)
    "not":   (0x2B, "rr"),    # rd = ~ra
    "neg":   (0x2C, "rr"),    # rd = -ra
    "cmp":   (0x2D, "rr"),    # nur Flags: ra - rb
    "tst":   (0x2E, "rr"),    # nur Flags: ra & rb
    "udiv":  (0x2F, "rrr"),
    "umod":  (0x3F, "rrr"),

    # --- Rechenwerk, Konstanten-Variante (rd = ra OP imm16) -------------
    "addi":  (0x30, "rri"),
    "subi":  (0x31, "rri"),
    "muli":  (0x32, "rri"),
    "divi":  (0x33, "rri"),
    "modi":  (0x34, "rri"),
    "andi":  (0x35, "rri"),
    "ori":   (0x36, "rri"),
    "xori":  (0x37, "rri"),
    "shli":  (0x38, "rri"),
    "shri":  (0x39, "rri"),
    "sari":  (0x3A, "rri"),
    "cmpi":  (0x3D, "ri"),    # Flags: rd - imm16
    "tsti":  (0x3E, "ri"),

    # --- Stack ----------------------------------------------------------
    "push":  (0x40, "r"),
    "pop":   (0x41, "r"),
    "call":  (0x42, "c"),     # relativer Aufruf, Rücksprungadresse auf Stack
    "callr": (0x43, "r"),     # Aufruf über Registerinhalt (Funktionszeiger)
    "pushf": (0x44, "n"),
    "popf":  (0x45, "n"),

    # --- Sprünge --------------------------------------------------------
    "jmp":   (0x50, "j"),     # Bedingung steckt im cond-Feld
    "jmpr":  (0x51, "r"),

    # --- Ein-/Ausgabe und System ---------------------------------------
    "in":    (0x60, "ri"),    # rd = port[imm16]
    "out":   (0x62, "ir"),    # port[imm16] = rd
    "inr":   (0x61, "rr"),    # rd = port[ra]
    "outr":  (0x63, "rr"),    # port[ra] = rd  (ra = Portnummer)
    "int":   (0x64, "i"),     # Software-Interrupt
}

# Bedingte Sprünge sind alle derselbe Opcode 0x50 mit anderem cond-Feld.
# Der Assembler kennt sie als eigene Mnemonics: jz, jnz, je, jne, ...
for _name, _code in COND.items():
    if _name != "al":
        INSTRUCTIONS["j" + _name] = (0x50, "j")

OPCODE_NAMES = {}
for _mn, (_op, _fmt) in INSTRUCTIONS.items():
    OPCODE_NAMES.setdefault(_op, _mn)

# ---------------------------------------------------------------------------
# Speicherkarte des Systems
# ---------------------------------------------------------------------------

RAM_BASE   = 0x00000000
RAM_SIZE   = 16 * 1024 * 1024        # 16 MiB Arbeitsspeicher

VRAM_TEXT  = 0x02000000              # Textmodus: 80x25, je 2 Byte (Zeichen|Attribut)
VRAM_TEXT_SIZE = 80 * 25 * 2

VRAM_GFX   = 0x02100000              # Grafikmodus: 640x400, 1 Byte je Pixel (Palette)
GFX_W, GFX_H = 640, 400
VRAM_GFX_SIZE = GFX_W * GFX_H

ROM_BASE   = 0x0F000000              # BIOS-ROM, 64 KiB, nur lesbar
ROM_SIZE   = 64 * 1024

RESET_VECTOR = ROM_BASE              # Hier startet die CPU nach dem Einschalten
IVT_BASE     = 0x00000000            # 256 Interrupt-Vektoren à 4 Byte (wie beim 8086)
BOOT_ADDR    = 0x00007C00            # Bootsektor wird hierhin geladen (Retro-Hommage)

# ---------------------------------------------------------------------------
# I/O-Ports
# ---------------------------------------------------------------------------

PORT_PIC_ACK     = 0x0000   # Interrupt bestätigen ("End of Interrupt")
PORT_PIC_MASK    = 0x0001   # Welche IRQs sind erlaubt (Bitmaske)

PORT_TIMER_HZ    = 0x0010   # Timer-Frequenz setzen (0 = aus)
PORT_TIMER_TICKS = 0x0011   # gelesene Ticks seit Start

PORT_KBD_DATA    = 0x0020   # nächste Taste aus dem Puffer holen
PORT_KBD_STATUS  = 0x0021   # 1 = Taste liegt bereit

PORT_DISK_LBA    = 0x0030   # Sektornummer
PORT_DISK_COUNT  = 0x0031   # Anzahl Sektoren
PORT_DISK_ADDR   = 0x0032   # Zieladresse im RAM
PORT_DISK_CMD    = 0x0033   # 1 = lesen, 2 = schreiben
PORT_DISK_STATUS = 0x0034   # 0 = ok, sonst Fehlercode
PORT_DISK_SIZE   = 0x0035   # Größe der Platte in Sektoren

PORT_VGA_MODE    = 0x0040   # 0 = Text, 1 = Grafik
PORT_VGA_CURSOR  = 0x0041   # Cursorposition (y*80+x), 0xFFFF = unsichtbar
PORT_VGA_PALIDX  = 0x0042   # Paletten-Index wählen
PORT_VGA_PALVAL  = 0x0043   # Farbe schreiben (0x00RRGGBB)

# 2D-Beschleuniger ("Blitter") der Grafikkarte -- damit Fenster und Text im
# Grafikmodus nicht Pixel für Pixel über den Bus gemalt werden müssen.
PORT_BLT_X       = 0x0044
PORT_BLT_Y       = 0x0045
PORT_BLT_W       = 0x0046
PORT_BLT_H       = 0x0047
PORT_BLT_COL     = 0x0048   # Vordergrundfarbe
PORT_BLT_CMD     = 0x0049   # 1=Fläche 2=Rahmen 3=Zeichen 4=Bild 5=kopieren
PORT_BLT_CHR     = 0x004A   # Zeichencode für Kommando 3
PORT_BLT_SRC     = 0x004B   # Quelladresse im RAM (Zeichensatz / Bild)
PORT_BLT_BG      = 0x004C   # Hintergrundfarbe (256 = durchsichtig)
PORT_MCUR_X      = 0x004D   # Mauszeiger (wird von der Karte gezeichnet)
PORT_MCUR_Y      = 0x004E
PORT_MCUR_ON     = 0x004F

PORT_BLT_ZOOM    = 0x0054   # Vergroesserung fuer Kommando 3 (1 = normal)
PORT_GFX_DOPPEL  = 0x0052   # 1 = Doppelpufferung an, 0 = aus
PORT_GFX_TAUSCH  = 0x0053   # schreiben = fertiges Bild sichtbar machen

# Der Blitter kann statt in den Bildschirm auch in einen Speicherbereich
# malen. Das braucht der Fenster-Server: jedes Programm zeichnet in seinen
# EIGENEN Puffer, und der Schreibtisch setzt die Puffer nachher zusammen.
# Damit kann kein Programm ueber ein fremdes Fenster malen -- und wer
# verdeckt ist, malt trotzdem weiter, ohne dass man es sieht.
PORT_BLT_ZIEL    = 0x005B   # Adresse des Zielpuffers, 0 = Bildschirm
PORT_BLT_ZIELB   = 0x005C   # dessen Breite in Punkten
PORT_BLT_ZIELH   = 0x005D   # ... und Hoehe

PORT_DMA_SRC     = 0x0056   # Blockkopierer: Quelladresse
PORT_DMA_DST     = 0x0057   # ... Zieladresse
PORT_DMA_LEN     = 0x0058   # ... Anzahl Bytes
PORT_DMA_VAL     = 0x0059   # ... Fuellbyte fuer Kommando 2
PORT_DMA_CMD     = 0x005A   # 1 = kopieren, 2 = fuellen

PORT_SPK_FREQ    = 0x0050   # Lautsprecher-Frequenz in Hz
PORT_SPK_ON      = 0x0051   # 1 = an, 0 = aus

PORT_MOUSE_X     = 0x0060
PORT_MOUSE_Y     = 0x0061
PORT_MOUSE_BTN   = 0x0062
PORT_MOUSE_WHEEL = 0x0063   # Mausrad: liest den Ausschlag und setzt ihn zurueck

PORT_CMOS_IDX    = 0x0070   # CMOS/Echtzeituhr: Adresse wählen
PORT_CMOS_DATA   = 0x0071   # ... und lesen/schreiben

# Temperatursensor und Lüftersteuerung -- wie der Chipsatz eines echten
# Mainboards. Wird es zu heiß, drosselt die Hardware den Takt von selbst.
PORT_TEMP        = 0x00A0   # aktuelle Temperatur in Zehntelgrad
PORT_FAN         = 0x00A1   # Lüfterdrehzahl 0..100 Prozent
PORT_THROTTLE    = 0x00A2   # wie stark gerade gedrosselt wird (Prozent)
PORT_TEMP_LIMIT  = 0x00A3   # ab wann gedrosselt wird (Grad)
PORT_FANMODE     = 0x00A4   # 0 = automatisch, 1 = leise, 2 = volle Drehzahl
PORT_TEMP_MAX    = 0x00A5   # höchste je gemessene Temperatur

PORT_DEBUG       = 0x0080   # Zeichen ins Entwickler-Log schreiben
PORT_POWER       = 0x0090   # 1 = ausschalten, 2 = Neustart

# Der ROM-Baustein und der Schlitz daneben.
#
# Ein echtes Mainboard hat den BIOS-Chip gesockelt und ein Werkzeug, das ihn
# neu beschreibt. Beides steckt hier: Befehl 1 laesst den Wirtsrechner eine
# Datei aussuchen (das ist der USB-Stick beim BIOS-Flashback), Befehl 3
# brennt sie in den Chip. Geprueft wird NICHT hier -- das macht die Firmware,
# genau wie auf einem echten Board.
PORT_FLASH_CMD   = 0x00B0   # 1 Datei holen, 2 in den RAM, 3 brennen, 4 zurueck
PORT_FLASH_SIZE  = 0x00B1   # lesen: Bytes im Puffer (0 = keine Datei)
PORT_FLASH_ADDR  = 0x00B2   # Zieladresse fuer Befehl 2

# ---------------------------------------------------------------------------
# Netzwerkkarte TB-NET
# ---------------------------------------------------------------------------
# Die Karte kennt nur Rahmen: sechs Byte Ziel, sechs Byte Absender, zwei Byte
# Art, dann die Nutzdaten. Was darin steht -- ARP, IP, was auch immer --,
# entscheidet der TB-32 selbst. Genau so wenig weiss eine echte Karte auch.
#
# Auf dem Mac gehen die Rahmen als UDP-Multicast hinaus (Gruppe 239.32.32.32,
# Port 32032, TTL 1: bleibt im eigenen Netz). Zwei laufende TB-32 sehen sich
# damit gegenseitig, auch auf zwei verschiedenen Rechnern im selben WLAN.
# Auf dem Pi wird dieselbe Schnittstelle spaeter von der echten Karte
# bedient -- der TB-32-Code aendert sich dabei um kein Byte.
PORT_NET_STATUS  = 0x00C0   # Bit 0 = Karte da, Bit 1 = Rahmen liegt bereit
PORT_NET_ADDR    = 0x00C1   # Speicheradresse fuer Senden und Empfangen
PORT_NET_LEN     = 0x00C2   # schreiben: Laenge zum Senden; lesen: Laenge des Rahmens
PORT_NET_CMD     = 0x00C3   # 1 = senden, 2 = empfangen, 3 = Warteschlange leeren
PORT_NET_MAC_HI  = 0x00C4   # eigene Adresse, die oberen beiden Byte
PORT_NET_MAC_LO  = 0x00C5   # ... und die unteren vier
PORT_NET_ZAEHLER = 0x00C6   # lesen: Rahmen empfangen (Index 0) / gesendet (1)
PORT_NET_ZINDEX  = 0x00C7   # welcher Zaehler gelesen wird

# ---------------------------------------------------------------------------
# Interrupt-Nummern
# ---------------------------------------------------------------------------

IRQ_TIMER = 0x08     # Hardware: Timer   (wie IRQ0 beim PC)
IRQ_KBD   = 0x09     # Hardware: Tastatur
IRQ_MOUSE = 0x0C     # Hardware: Maus
IRQ_NET   = 0x0D     # Hardware: Netzwerkkarte, ein Rahmen ist da

INT_VIDEO = 0x10     # BIOS-Dienst: Bildschirm
INT_DISK  = 0x13     # BIOS-Dienst: Festplatte
INT_KBD   = 0x16     # BIOS-Dienst: Tastatur
INT_TIME  = 0x1A     # BIOS-Dienst: Uhrzeit
INT_SYS   = 0x40     # Betriebssystem-Aufruf (Syscall)


def encode_r(op, rd=0, ra=0, rb=0):
    return (op << 24) | (rd << 20) | (ra << 16) | (rb << 12)


def encode_i(op, rd=0, ra=0, imm=0):
    return (op << 24) | (rd << 20) | (ra << 16) | (imm & 0xFFFF)


def encode_j(op, cond=0, off=0):
    return (op << 24) | (cond << 20) | (off & 0xFFFFF)


def encode_c(op, off=0):
    return (op << 24) | (off & 0xFFFFFF)
