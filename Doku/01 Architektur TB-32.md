# Architecture TB-32

Source of truth: `hardware/isa.py`. Both CPU and assembler read from there.
What the abbreviations mean is in [[12 Abkuerzungen und Namen]].

## Basics

- 32-bit, 16 general-purpose registers, **every instruction exactly 4 bytes**, RISC-like
- Flags Z, N, C, V; interrupt enable as bit 9
- Interrupt vector table starting at address 0, 256 entries of 4 bytes each (like the 8086)
- 16 MB RAM, ROM starting at `0x0F000000`, reset jumps there

## How fast the emulation is

The target clock from the BIOS setup is a **wish**, not a promise: how
many instructions actually go through depends on the host's Python
interpreter. Measured with `tools/messen` or the pattern below (as of:
after the optimization, with a compiler run as load):

| | |
|---|---|
| Raw emulation throughput | **~3.0 million instructions/s** |
| usable in the window | **~2.9 million/s at 60 fps** |
| 2 MHz (standard) and 4 MHz (turbo) | are fully reached |
| 8 MHz | is **not** reached (~36%) |

Writing higher clock steps into the setup would therefore accomplish
nothing — the number would go up, the machine wouldn't.

**What makes the emulator fast** (all in `hardware/cpu.py`):

1. `self.words` — a 32-bit view of the main memory
   (`memoryview(ram).cast("I")`). An instruction is thus **one** access
   instead of four bytes plus shifting and OR-ing. Misaligned addresses
   fall back to the old path, so even a stray jump still works correctly
2. The execution chain is ordered by **measured** frequency — `push` and
   `pop` together make up 40% of all instructions. Re-measure with:
   `tools/opstat.py`
3. `rb`, `imm`, and `simm` are fetched only by the branch that needs them
4. `pc`, `flags`, pending interrupts, and the breakpoint set live in
   **local** variables; every `self.x` access costs a multiple in Python
5. Whether the CPU is halted is no longer checked on every instruction,
   but only where a halt can arise (`hlt`, `brk`, interrupt without
   handler, invalid instruction)

**And the other half** is in `pc.py`: the CPU no longer gets a fixed
8 ms per frame, but whatever drawing leaves over (measured and smoothed,
capped at 14 ms). Drawing actually costs ~1 ms.

## Instruction formats

```
R-type:  [31:24] op | [23:20] rd | [19:16] ra | [15:12] rb | rest free
I-type:  [31:24] op | [23:20] rd | [19:16] ra | [15:0]  imm16
J-type:  [31:24] op | [23:20] cond | [19:0] jump distance in words
C-type:  [31:24] op | [23:0] call distance in words
```

Jump target = address of the jump instruction + distance × 4.

## Instructions

| Group | Instructions |
|---|---|
| Control | `nop hlt cli sti iret ret brk` |
| Transfer | `mov movi movh`, pseudo `li rd, 32bit` |
| Memory | `ldb ldsb ldh ldw stb sth stw` — always `[reg + off16]` |
| Arithmetic | `add sub mul div mod and or xor shl shr sar not neg cmp tst udiv umod` |
| with constant | `addi subi muli divi modi andi ori xori shli shri sari cmpi tsti` |
| Stack | `push pop call callr pushf popf` |
| Jumps | `jmp` and `jz jnz jc jnc jn jbe ja jl jge jle jg`, `jmpr` |
| I/O | `in out inr outr`, `int n` |

**Important:** `add`/`sub` set flags, `addi`/`subi` **do not**. After a
counting loop, use `cmpi`, otherwise the jump will be wrong.

## Assembler specifics

- Local labels with a leading dot are scoped within the last global label
- `ldwa`/`stwa`/`ldba`/`stba` are pseudo-instructions for absolute
  addresses and clobber `r13` (`at`) in the process
- Directives: `.org .equ .db .dh .dw .string .space .align .fill .include`
- Two passes; in the first, unknown labels are 0

## Speed

The emulation manages 1.5–3.5 million instructions/s (depending on the
program mix). The clock set in the BIOS is therefore a wish. Details and
consequences: [[07 Fallstricke]], [[10 Temperatur]].

Related: [[02 Speicherkarte und Ports]], [[05 Konventionen]]
