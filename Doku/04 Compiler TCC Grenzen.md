# Compiler TCC — what the language can and cannot do

**Read before every line in `system/*.c` or `programs/*.c`.** These files
look like C, but are compiled by `tools/tcc.py`. What real C can do, TCC
is nowhere near — and the compiler doesn't always report this as an
error, but sometimes silently generates wrong code.

## What's MISSING

| Missing | Replacement |
|---|---|
| `struct`, `union`, `enum`, `typedef` | parallel arrays + index arithmetic |
| `float`, `double`, `long`, `short`, `unsigned` as distinct types | everything is `int` (32-bit) or `char` |
| Multidimensional arrays `a[y][x]` | `a[y * WIDTH + x]` |
| String arrays `char* t[] = {"a","b"}` | function with an `if` chain returning pointers |
| `switch` / `case` | `if`/`else if` chain |
| `do…while` | `while (1)` with `break` |
| `&&=`, comma operator in `for` | write them out separately |
| `#ifdef`, function-like macros | only `#define NAME number` |
| Standard library | custom versions in `lib.c` / `proglib.c` |

## What EXISTS (and works reliably)

`int`, `char`, `int*`, `char*`, arrays, global and local variables,
functions with **up to 5 parameters**, recursion, `if`/`else`, `while`,
`for`, `break`, `continue`, `return`, all arithmetic and comparison
operators, `&&` `||` `!`, `& | ^ ~ << >>`, `=` `+=` `-=` `*=` `/=`,
`++` `--`, `&x`, `*p`, `a[i]`, strings, `'A'`, `?:`, `asm("...")`,
`#include "file.c"`, `#define NAME number`, both comment styles.

## Pitfalls that silently generate wrong code

**Negative numbers** — `-1` works, but the project writes `0 - 1`
everywhere. That's historical and doesn't hurt; don't "clean it up."

**A `#` at the start of a line inside a comment** — both preprocessors
now know whether a line is inside a block comment (`zeilen_im_kommentar()`
in `tcc.py`, `komm_folge()` in `cc.c`). Before this fix, such a line got
deleted, the `*/` disappeared, and the comment ate real code — the most
expensive bug in the project, see [[07 Fallstricke]].

**More than 5 arguments** — TCC puts them on the stack, but the
assembler bridge in `start.asm` expects them in `r1`–`r5`. So for
`sys_*` functions, **never more than 5**. **`cc.c` on the device can
only handle 5 at all** (it does `for (i = argn; i >= 1; i--) e_pop(i)`
and would write into `r6` with a sixth). Programs under `programs/` that
should be able to compile themselves therefore stick to a maximum of
five.

**Global initial values** work (`int x = 5;`), but only with a constant
expression. `char text[4] = {65,66,67,0}` works, `char* s = "x"` does
not.

**Local arrays** live in the stack frame. Large local buffers (`char
buf[4096]`) blow it up — such buffers belong at fixed addresses, see
[[02 Speicherkarte und Ports]].

**Visibility across file boundaries**: the compiler collects *all*
function and variable names from all `#include` files **upfront**.
That's why no forward declarations are needed — and why a declaration
like `int term_aktiv;` in one file plus `int term_aktiv = 0;` in another
is a **duplicate label** and aborts the assembler. Never declare
something twice.

**Order of `#include`** in `kernel.c` doesn't matter for names, but does
for `#define`: macros only take effect from their line onward.

## How to spot compiler errors

The generated assembly ends up in `system/kernel.asm` after building.
When behavior looks odd, check there — the code is quite readable, every
C function begins with `; ===== name() =====`.

`python3 tools/ctest.py --selftest` checks 11 language features directly
on the emulated CPU. Anyone changing the compiler runs this **always**.

Test a single C snippet without building the whole system:

```bash
python3 tools/ctest.py mytestprogram.c --asm
```

## The second C compiler

`programs/cc.c` is **another** C compiler that runs on the TB-32 and can
compile itself. It understands roughly the same subset as TCC, plus type
casts `(char*)x` and constant expressions in array sizes. Anyone
changing `cc.c` must afterward check **both**: that TCC still compiles
it *and* that it still compiles itself ([[09 Selbst-Compilierung]]).

Related: [[05 Konventionen]], [[07 Fallstricke]]
