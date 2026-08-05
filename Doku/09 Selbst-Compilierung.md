# Self-Compilation (Bootstrapping)

**Proven.** `programs/cc.c` sits on the virtual drive as `\SOURCE\CC.C` and
gets compiled by its own compiler.

## The chain

| Stage | Who builds | Result |
|---|---|---|
| 1 | `tools/tcc.py` on the Mac | `\SYSTEM\CC.TBX` |
| 2 | `CC.TBX` on the TB-32 | `CC2.TBX` |
| 3 | `CC2.TBX` on the TB-32 | `CC3.TBX` |

Stages 2 and 3 are **byte-identical** (66224 bytes). That makes the compiler
a fixed point. Stage 1 is allowed to differ — it comes from a different
compiler.

Verify: `python3 tools/bootstrap.py` (~5 min), or by hand:

```
CD SOURCE
CC  CC.C CC2.TBX
CC2 CC.C CC3.TBX
FC  CC2.TBX CC3.TBX      -> "no differences encountered"
```

## What CC had to be able to do for this

- `#define` (macro table in the lexer) and `#include` (loads **from its own
  filesystem**, one level deep)
- type casts `(char*)x` — recognized by the type keyword right after the
  parenthesis
- constant expressions in array sizes (`char n[MAX * LEN]`)
- global variables with an initial value — the assignments run as generated
  code before `main()`, invoked via an appended jump label
- `sc()` as a built-in system call, so `proglib.c` works unchanged
- `portout()` / `portin()` also built in (numbers 98 and 97): they generate
  `outr` and `inr` **directly at the call site**, without going through the
  kernel. On the Mac, `prog_start.asm` provides the same two functions —
  so both compilers end up doing the same thing, and `gfxlib.c` stays a
  single file for both
- larger tables: 256 globals, 192 functions, 3000 pending jumps

## How CC is built

A single-pass compiler with direct code generation, no syntax tree. Four
things get backpatched: forward jumps, calls to functions defined later,
string addresses, and stack frame size.

The **lvalue trick**: while parsing a name, it's still unclear whether
`x = 5` (address needed) or `y = x` (value needed) follows. So the
*address* always stays in `r0`, tracked by a flag; `rvalue()` only loads the
value once it's actually needed.

## When `cc.c` is changed

Then check **both**:

```bash
python3 build.py                  # TCC still has to compile it
python3 tools/bootstrap.py        # and it still has to compile itself
```

Related: [[04 Compiler TCC Grenzen]], [[06 Bauen und Testen]]
