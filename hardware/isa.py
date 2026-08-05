"""
TB-32 instruction set (ISA) -- the "blueprints" of the CPU.

This file is the single source of truth for the architecture. Both the CPU
(hardware/cpu.py) and the assembler (tools/assembler.py) read from here, so
the two can never drift apart.

Basic idea (deliberately RISC-like, as in ARM/RISC-V, not like x86):
  * Every instruction is EXACTLY 4 bytes long and sits at a 4-byte address.
  * 16 general-purpose registers R0..R15, plus PC and FLAGS.
  * Constants are at most 16 bits wide; for 32-bit values there's MOVH.

Instruction formats (bit 31 is on the left):

  R-type:  [31:24] opcode | [23:20] rd | [19:16] ra | [15:12] rb | [11:0] unused
  I-type:  [31:24] opcode | [23:20] rd | [19:16] ra | [15:0]  imm16
  J-type:  [31:24] opcode | [23:20] cond| [19:0]  offset (in words, signed)
  C-type:  [31:24] opcode | [23:0]  offset (in words, signed)   -- for CALL
"""

# ---------------------------------------------------------------------------
# Registers
# ---------------------------------------------------------------------------

NUM_REGS = 16
SP = 15          # stack pointer
FP = 14          # frame pointer (used by the compiler)

REG_ALIASES = {
    "sp": 15,
    "fp": 14,
    "at": 13,    # assembler scratch register (like $at on MIPS)
    "rv": 0,     # return-value convention: R0
}

# ---------------------------------------------------------------------------
# Flags in the FLAGS register
# ---------------------------------------------------------------------------

FLAG_Z = 1 << 0      # Zero          - result was 0
FLAG_N = 1 << 1      # Negative      - highest bit set
FLAG_C = 1 << 2      # Carry/Borrow  - carry (unsigned)
FLAG_V = 1 << 3      # Overflow      - sign overflow (signed)
FLAG_I = 1 << 9      # Interrupt Enable

# ---------------------------------------------------------------------------
# Jump conditions (4 bits, live in the rd field of the J-type)
# ---------------------------------------------------------------------------

COND = {
    "al": 0,    # always            - unconditional jump
    "z":  1, "eq": 1,
    "nz": 2, "ne": 2,
    "c":  3, "b":  3,      # unsigned <
    "nc": 4, "ae": 4,      # unsigned >=
    "n":  5,               # negative
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
# Format codes:
#   "n"  - no operand              (NOP, HLT, RET, ...)
#   "r"  - one register            (PUSH r3)
#   "rr" - two registers           (MOV r1, r2)
#   "rrr"- three registers         (ADD r1, r2, r3)
#   "ri" - register + constant     (MOVI r1, 42)
#   "rri"- register, register, imm (ADDI r1, r2, 4)
#   "mem"- register + [base+off]   (LDW r1, [r2+8])
#   "j"  - jump target             (JMP label)
#   "c"  - call target             (CALL label)
#   "i"  - constant only           (INT 0x10)
# ---------------------------------------------------------------------------

INSTRUCTIONS = {
    # --- Control ------------------------------------------------------
    "nop":   (0x00, "n"),
    "hlt":   (0x01, "n"),     # halt CPU until the next interrupt
    "cli":   (0x02, "n"),     # disable interrupts
    "sti":   (0x03, "n"),     # enable interrupts
    "iret":  (0x04, "n"),     # return from interrupt
    "ret":   (0x05, "n"),
    "brk":   (0x06, "n"),     # breakpoint for the built-in debugger

    # --- Data transfer --------------------------------------------------
    "mov":   (0x10, "rr"),    # rd = ra
    "movi":  (0x11, "ri"),    # rd = imm16 (sign-extended)
    "movh":  (0x13, "ri"),    # rd = (rd & 0xFFFF) | (imm16 << 16)

    # --- Memory access (rd, [ra + off16]) -----------------------------
    "ldb":   (0x18, "mem"),   # load 8 bits, zero-extended
    "ldsb":  (0x19, "mem"),   # load 8 bits, sign-extended
    "ldh":   (0x1A, "mem"),   # load 16 bits
    "ldw":   (0x1B, "mem"),   # load 32 bits
    "stb":   (0x1C, "mem"),   # store 8 bits
    "sth":   (0x1D, "mem"),
    "stw":   (0x1E, "mem"),

    # --- ALU, register variant (rd = ra OP rb) ------------------
    "add":   (0x20, "rrr"),
    "sub":   (0x21, "rrr"),
    "mul":   (0x22, "rrr"),
    "div":   (0x23, "rrr"),   # signed
    "mod":   (0x24, "rrr"),   # signed
    "and":   (0x25, "rrr"),
    "or":    (0x26, "rrr"),
    "xor":   (0x27, "rrr"),
    "shl":   (0x28, "rrr"),
    "shr":   (0x29, "rrr"),   # logical (fills with zeros)
    "sar":   (0x2A, "rrr"),   # arithmetic (fills with sign bit)
    "not":   (0x2B, "rr"),    # rd = ~ra
    "neg":   (0x2C, "rr"),    # rd = -ra
    "cmp":   (0x2D, "rr"),    # flags only: ra - rb
    "tst":   (0x2E, "rr"),    # flags only: ra & rb
    "udiv":  (0x2F, "rrr"),
    "umod":  (0x3F, "rrr"),

    # --- ALU, constant variant (rd = ra OP imm16) -------------
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
    "cmpi":  (0x3D, "ri"),    # flags: rd - imm16
    "tsti":  (0x3E, "ri"),

    # --- Stack ----------------------------------------------------------
    "push":  (0x40, "r"),
    "pop":   (0x41, "r"),
    "call":  (0x42, "c"),     # relative call, return address pushed to stack
    "callr": (0x43, "r"),     # call through register contents (function pointer)
    "pushf": (0x44, "n"),
    "popf":  (0x45, "n"),

    # --- Jumps --------------------------------------------------------
    "jmp":   (0x50, "j"),     # condition lives in the cond field
    "jmpr":  (0x51, "r"),

    # --- Input/output and system ---------------------------------------
    "in":    (0x60, "ri"),    # rd = port[imm16]
    "out":   (0x62, "ir"),    # port[imm16] = rd
    "inr":   (0x61, "rr"),    # rd = port[ra]
    "outr":  (0x63, "rr"),    # port[ra] = rd  (ra = port number)
    "int":   (0x64, "i"),     # software interrupt
}

# Conditional jumps are all the same opcode 0x50 with a different cond field.
# The assembler knows them as their own mnemonics: jz, jnz, je, jne, ...
for _name, _code in COND.items():
    if _name != "al":
        INSTRUCTIONS["j" + _name] = (0x50, "j")

OPCODE_NAMES = {}
for _mn, (_op, _fmt) in INSTRUCTIONS.items():
    OPCODE_NAMES.setdefault(_op, _mn)

# ---------------------------------------------------------------------------
# System memory map
# ---------------------------------------------------------------------------

RAM_BASE   = 0x00000000
RAM_SIZE   = 16 * 1024 * 1024        # 16 MiB of main memory

VRAM_TEXT  = 0x02000000              # text mode: 80x25, 2 bytes each (character|attribute)
VRAM_TEXT_SIZE = 80 * 25 * 2

VRAM_GFX   = 0x02100000              # graphics mode: 640x400, 1 byte per pixel (palette)
GFX_W, GFX_H = 640, 400
VRAM_GFX_SIZE = GFX_W * GFX_H

ROM_BASE   = 0x0F000000              # BIOS ROM, 64 KiB, read-only
ROM_SIZE   = 64 * 1024

RESET_VECTOR = ROM_BASE              # the CPU starts here after power-on
IVT_BASE     = 0x00000000            # 256 interrupt vectors of 4 bytes each (like the 8086)
BOOT_ADDR    = 0x00007C00            # the boot sector is loaded here (a retro homage)

# ---------------------------------------------------------------------------
# I/O ports
# ---------------------------------------------------------------------------

PORT_PIC_ACK     = 0x0000   # acknowledge interrupt ("End of Interrupt")
PORT_PIC_MASK    = 0x0001   # which IRQs are allowed (bitmask)

PORT_TIMER_HZ    = 0x0010   # set timer frequency (0 = off)
PORT_TIMER_TICKS = 0x0011   # ticks read since start

PORT_KBD_DATA    = 0x0020   # fetch next key from the buffer
PORT_KBD_STATUS  = 0x0021   # 1 = a key is ready

PORT_DISK_LBA    = 0x0030   # sector number
PORT_DISK_COUNT  = 0x0031   # number of sectors
PORT_DISK_ADDR   = 0x0032   # target address in RAM
PORT_DISK_CMD    = 0x0033   # 1 = read, 2 = write
PORT_DISK_STATUS = 0x0034   # 0 = ok, otherwise error code
PORT_DISK_SIZE   = 0x0035   # disk size in sectors

PORT_VGA_MODE    = 0x0040   # 0 = text, 1 = graphics
PORT_VGA_CURSOR  = 0x0041   # cursor position (y*80+x), 0xFFFF = hidden
PORT_VGA_PALIDX  = 0x0042   # select palette index
PORT_VGA_PALVAL  = 0x0043   # write color (0x00RRGGBB)

# 2D accelerator ("blitter") of the graphics card -- so that windows and text
# in graphics mode don't have to be painted pixel by pixel over the bus.
PORT_BLT_X       = 0x0044
PORT_BLT_Y       = 0x0045
PORT_BLT_W       = 0x0046
PORT_BLT_H       = 0x0047
PORT_BLT_COL     = 0x0048   # foreground color
PORT_BLT_CMD     = 0x0049   # 1=filled rect 2=outline 3=char 4=image 5=copy
PORT_BLT_CHR     = 0x004A   # character code for command 3
PORT_BLT_SRC     = 0x004B   # source address in RAM (font / image)
PORT_BLT_BG      = 0x004C   # background color (256 = transparent)
PORT_MCUR_X      = 0x004D   # mouse cursor (drawn by the card)
PORT_MCUR_Y      = 0x004E
PORT_MCUR_ON     = 0x004F

PORT_BLT_ZOOM    = 0x0054   # magnification for command 3 (1 = normal)
PORT_GFX_DOPPEL  = 0x0052   # 1 = double buffering on, 0 = off
PORT_GFX_TAUSCH  = 0x0053   # write = make the finished frame visible

# The blitter can also paint into a memory region instead of the screen.
# The window server needs this: every program draws into its OWN buffer, and
# the desktop composites the buffers together afterward. This way no program
# can paint over someone else's window -- and a window that's covered up
# keeps drawing anyway, without it being visible.
PORT_BLT_ZIEL    = 0x005B   # address of the target buffer, 0 = screen
PORT_BLT_ZIELB   = 0x005C   # its width in pixels
PORT_BLT_ZIELH   = 0x005D   # ... and height

PORT_DMA_SRC     = 0x0056   # block copier: source address
PORT_DMA_DST     = 0x0057   # ... target address
PORT_DMA_LEN     = 0x0058   # ... number of bytes
PORT_DMA_VAL     = 0x0059   # ... fill byte for command 2
PORT_DMA_CMD     = 0x005A   # 1 = copy, 2 = fill

PORT_SPK_FREQ    = 0x0050   # speaker frequency in Hz
PORT_SPK_ON      = 0x0051   # 1 = on, 0 = off

PORT_MOUSE_X     = 0x0060
PORT_MOUSE_Y     = 0x0061
PORT_MOUSE_BTN   = 0x0062
PORT_MOUSE_WHEEL = 0x0063   # mouse wheel: reads the delta and resets it

PORT_CMOS_IDX    = 0x0070   # CMOS/real-time clock: select address
PORT_CMOS_DATA   = 0x0071   # ... and read/write

# The second, larger battery-backed memory.
#
# The clock CMOS holds 64 bytes and is full -- passwords, policy switches and
# the block list just about fit, the owner text, event log and inventory do
# not. Real mainboards took the same route: the 64 bytes at the clock stayed,
# everything else moved into a chip of its own.
PORT_NVRAM_IDX   = 0x0072   # NVRAM: select address (0..255)
PORT_NVRAM_DATA  = 0x0073   # ... and read/write

# Temperature sensor and fan control -- like the chipset of a real
# mainboard. If it gets too hot, the hardware throttles the clock on its own.
PORT_TEMP        = 0x00A0   # current temperature, in tenths of a degree
PORT_FAN         = 0x00A1   # fan speed, 0..100 percent
PORT_THROTTLE    = 0x00A2   # how much throttling is currently applied (percent)
PORT_TEMP_LIMIT  = 0x00A3   # threshold above which throttling kicks in (degrees)
PORT_FANMODE     = 0x00A4   # 0 = automatic, 1 = quiet, 2 = full speed
PORT_TEMP_MAX    = 0x00A5   # highest temperature ever measured

PORT_DEBUG       = 0x0080   # write a character to the developer log
PORT_POWER       = 0x0090   # 1 = power off, 2 = restart

# The ROM chip and the slot next to it.
#
# A real mainboard has the BIOS chip socketed, plus a tool to reflash it.
# Both are modeled here: command 1 lets the host machine pick a file (that's
# the USB stick in a BIOS flashback), command 3 burns it into the chip.
# Validation does NOT happen here -- the firmware does that, just like on a
# real board.
PORT_FLASH_CMD   = 0x00B0   # 1 fetch file, 2 into RAM, 3 burn, 4 clear
PORT_FLASH_SIZE  = 0x00B1   # read: bytes in the buffer (0 = no file)
PORT_FLASH_ADDR  = 0x00B2   # target address for command 2

# ---------------------------------------------------------------------------
# TB-NET network card
# ---------------------------------------------------------------------------
# The card only knows frames: six bytes destination, six bytes source, two
# bytes type, then the payload. What's inside -- ARP, IP, whatever -- is
# decided by the TB-32 itself. A real card knows just as little.
#
# On the Mac, frames go out as UDP multicast (group 239.32.32.32, port
# 32032, TTL 1: stays on the local network). Two running TB-32 instances see
# each other this way, even across two different machines on the same Wi-Fi.
# On the Pi, the same interface will later be served by the real card --
# the TB-32 code doesn't change by a single byte for that.
PORT_NET_STATUS  = 0x00C0   # bit 0 = card present, bit 1 = a frame is ready
PORT_NET_ADDR    = 0x00C1   # memory address for send and receive
PORT_NET_LEN     = 0x00C2   # write: length to send; read: length of the frame
PORT_NET_CMD     = 0x00C3   # 1 = send, 2 = receive, 3 = clear queue
PORT_NET_MAC_HI  = 0x00C4   # own address, the upper two bytes
PORT_NET_MAC_LO  = 0x00C5   # ... and the lower four
PORT_NET_ZAEHLER = 0x00C6   # read: frames received (index 0) / sent (1)
PORT_NET_ZINDEX  = 0x00C7   # which counter is being read

# ---------------------------------------------------------------------------
# Interrupt numbers
# ---------------------------------------------------------------------------

IRQ_TIMER = 0x08     # hardware: timer   (like IRQ0 on a PC)
IRQ_KBD   = 0x09     # hardware: keyboard
IRQ_MOUSE = 0x0C     # hardware: mouse
IRQ_NET   = 0x0D     # hardware: network card, a frame is available

INT_VIDEO = 0x10     # BIOS service: screen
INT_DISK  = 0x13     # BIOS service: disk
INT_KBD   = 0x16     # BIOS service: keyboard
INT_TIME  = 0x1A     # BIOS service: time of day
INT_SYS   = 0x40     # operating system call (syscall)


def encode_r(op, rd=0, ra=0, rb=0):
    return (op << 24) | (rd << 20) | (ra << 16) | (rb << 12)


def encode_i(op, rd=0, ra=0, imm=0):
    return (op << 24) | (rd << 20) | (ra << 16) | (imm & 0xFFFF)


def encode_j(op, cond=0, off=0):
    return (op << 24) | (cond << 20) | (off & 0xFFFFF)


def encode_c(op, off=0):
    return (op << 24) | (off & 0xFFFFFF)
