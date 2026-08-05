"""
Assembler for the TB-32 -- translates readable assembly text into machine code.

Works in two passes ("two-pass"), just like real assemblers:
  Pass 1: determine the size of each line and record all label addresses along the way.
  Pass 2: now all labels are known -> generate bytes.

Supported directives:
    .org  <address>        Code is placed here from this point on
    .equ  NAME, value      Define a constant
    .db   1, 2, "text"     Emit bytes (strings allowed)
    .dh   0x1234           16-bit values
    .dw   0x12345678       32-bit values
    .space <count>         Fill space with zeros
    .align <n>             Pad to an n-byte boundary
    .fill  <to>, <value>   Fill up to the address with <value>

Pseudo-instructions:
    li rd, <32-bit value>  -> movi + movh (the CPU only knows 16-bit constants)
"""

import re
import sys
import os

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from hardware.isa import (
    INSTRUCTIONS, COND, REG_ALIASES,
    encode_r, encode_i, encode_j, encode_c,
)


class AsmError(Exception):
    pass


# ---------------------------------------------------------------------------
# Small expression evaluator: understands  LABEL + 4*3 - (1<<8)
# ---------------------------------------------------------------------------

TOKEN_RE = re.compile(r"""
      (?P<num>0[xX][0-9a-fA-F]+|0[bB][01]+|\d+)
    | (?P<chr>'(?:\\.|[^'])')
    | (?P<name>[A-Za-z_.][A-Za-z0-9_.]*)
    | (?P<op><<|>>|[-+*/%&|^()~])
    | (?P<here>\$)
    | (?P<ws>\s+)
""", re.VERBOSE)

ESCAPES = {"n": 10, "r": 13, "t": 9, "0": 0, "\\": 92, "'": 39, '"': 34, "e": 27}


class Expr:
    """Tiny recursive-descent parser -- plenty for assembler expressions."""

    def __init__(self, text, symbols, here, line, lenient=False, local_prefix=""):
        self.toks = []
        pos = 0
        while pos < len(text):
            m = TOKEN_RE.match(text, pos)
            if not m:
                raise AsmError(f"Line {line}: unrecognized character '{text[pos]}'")
            pos = m.end()
            if m.lastgroup != "ws":
                self.toks.append((m.lastgroup, m.group()))
        self.i = 0
        self.symbols = symbols
        self.here = here
        self.line = line
        self.lenient = lenient      # pass 1: unknown labels = 0
        self.local_prefix = local_prefix

    def peek(self):
        return self.toks[self.i] if self.i < len(self.toks) else (None, None)

    def eat(self):
        t = self.peek()
        self.i += 1
        return t

    def parse(self):
        v = self.expr(0)
        if self.i < len(self.toks):
            raise AsmError(f"Line {self.line}: leftover in expression: {self.toks[self.i][1]}")
        return v

    PREC = {"|": 1, "^": 2, "&": 3, "<<": 4, ">>": 4,
            "+": 5, "-": 5, "*": 6, "/": 6, "%": 6}

    def expr(self, min_prec):
        left = self.unary()
        while True:
            kind, val = self.peek()
            if kind != "op" or val not in self.PREC or self.PREC[val] < min_prec:
                return left
            self.eat()
            right = self.expr(self.PREC[val] + 1)
            left = {
                "+": lambda a, b: a + b, "-": lambda a, b: a - b,
                "*": lambda a, b: a * b, "/": lambda a, b: a // b if b else 0,
                "%": lambda a, b: a % b if b else 0,
                "&": lambda a, b: a & b, "|": lambda a, b: a | b,
                "^": lambda a, b: a ^ b,
                "<<": lambda a, b: a << b, ">>": lambda a, b: a >> b,
            }[val](left, right)

    def unary(self):
        kind, val = self.peek()
        if kind == "op" and val == "-":
            self.eat()
            return -self.unary()
        if kind == "op" and val == "~":
            self.eat()
            return ~self.unary() & 0xFFFFFFFF
        if kind == "op" and val == "+":
            self.eat()
            return self.unary()
        return self.atom()

    def atom(self):
        kind, val = self.eat()
        if kind == "num":
            return int(val, 0)
        if kind == "chr":
            body = val[1:-1]
            if body.startswith("\\"):
                return ESCAPES.get(body[1], ord(body[1]))
            return ord(body)
        if kind == "here":
            return self.here
        if kind == "name":
            if val.startswith(".") and self.local_prefix:
                val = self.local_prefix + val      # local label within the current block
            if val not in self.symbols:
                if self.lenient:
                    return 0
                raise AsmError(f"Line {self.line}: unknown symbol '{val}'")
            return self.symbols[val]
        if kind == "op" and val == "(":
            v = self.expr(0)
            k2, v2 = self.eat()
            if v2 != ")":
                raise AsmError(f"Line {self.line}: missing ')'")
            return v
        raise AsmError(f"Line {self.line}: unexpected '{val}'")


# ---------------------------------------------------------------------------

def ohne_kommentar(zeile):
    """Strips off the comment -- but not in the middle of a string.

    This used to be `zeile.split(";")[0]`, which silently mangled
        .db "A bad image is refused; keeps a backup", 0
    into `.db "A bad image is refused`: a string with no end, no null
    byte, and no error message. Output would then run into the next
    string, and you'd end up hunting for the bug in the screen code.
    """
    im_text = None
    i = 0
    while i < len(zeile):
        c = zeile[i]
        if im_text:
            if c == "\\":
                i += 2
                continue
            if c == im_text:
                im_text = None
        elif c in "\"'":
            im_text = c
        elif c == ";":
            return zeile[:i]
        i += 1
    return zeile


class Assembler:
    def __init__(self):
        self.symbols = {}
        self.output = bytearray()
        self.base = 0
        self.pc = 0
        self.last_global = ""
        self.lenient = True

    # -- Helper functions ---------------------------------------------------

    def value(self, text, line):
        return Expr(text.strip(), self.symbols, self.pc, line,
                    self.lenient, self.last_global).parse()

    def reg(self, text, line):
        t = text.strip().lower()
        if t in REG_ALIASES:
            return REG_ALIASES[t]
        m = re.fullmatch(r"r(\d+)", t)
        if not m or int(m.group(1)) > 15:
            raise AsmError(f"Line {line}: '{text}' is not a register")
        return int(m.group(1))

    def split_args(self, text):
        """Split arguments on commas, but not inside [] or \"\"."""
        args, depth, cur, in_str = [], 0, "", None
        for ch in text:
            if in_str:
                cur += ch
                if ch == in_str and not cur.endswith("\\" + ch):
                    in_str = None
                continue
            if ch in "\"'":
                in_str = ch
                cur += ch
            elif ch == "[":
                depth += 1
                cur += ch
            elif ch == "]":
                depth -= 1
                cur += ch
            elif ch == "," and depth == 0:
                args.append(cur.strip())
                cur = ""
            else:
                cur += ch
        if cur.strip():
            args.append(cur.strip())
        return args

    def memref(self, text, line):
        """[r2+8] / [r2-4] / [r2] / [LABEL]  ->  (base register, offset)"""
        t = text.strip()
        if not (t.startswith("[") and t.endswith("]")):
            raise AsmError(f"Line {line}: memory access needs [brackets]: {text}")
        inner = t[1:-1].strip()
        m = re.match(r"^(r\d+|sp|fp|rv|at)\s*([-+].*)?$", inner, re.I)
        if m:
            base = self.reg(m.group(1), line)
            off = self.value(m.group(2), line) if m.group(2) else 0
            if not (-32768 <= off <= 32767):
                raise AsmError(f"Line {line}: offset {off} doesn't fit in 16 bits")
            return base, off
        raise AsmError(f"Line {line}: [{inner}] needs a base register -- "
                       f"use ldwa/stwa for absolute addresses")

    def emit32(self, word):
        self.output += bytes(((word) & 0xFF, (word >> 8) & 0xFF,
                              (word >> 16) & 0xFF, (word >> 24) & 0xFF))
        self.pc += 4

    def emit_bytes(self, data):
        self.output += bytes(data)
        self.pc += len(data)

    # -- Main pass ----------------------------------------------------

    def assemble(self, text, second_pass=False):
        self.output = bytearray()
        self.pc = self.base
        self.last_global = ""
        self.lenient = not second_pass

        for lineno, raw in enumerate(text.splitlines(), 1):
            line = ohne_kommentar(raw).rstrip()
            if not line.strip():
                continue

            # label at the start of the line
            m = re.match(r"^([A-Za-z_.][A-Za-z0-9_.]*):\s*(.*)$", line.strip()) \
                if not line[0].isspace() else None
            if m:
                name = m.group(1)
                if name.startswith("."):
                    name = self.last_global + name       # local label
                else:
                    self.last_global = name
                if not second_pass:
                    if name in self.symbols:
                        raise AsmError(f"Line {lineno}: duplicate label '{name}'")
                    self.symbols[name] = self.pc
                line = m.group(2)
                if not line.strip():
                    continue

            parts = line.strip().split(None, 1)
            mnem = parts[0].lower()
            rest = parts[1] if len(parts) > 1 else ""

            try:
                if mnem.startswith("."):
                    self.directive(mnem, rest, lineno, second_pass)
                else:
                    self.instruction(mnem, rest, lineno, second_pass)
            except AsmError:
                raise
            except Exception as e:
                raise AsmError(f"Line {lineno}: {e}  ->  {raw.strip()}")

        return bytes(self.output)

    def directive(self, d, rest, line, second):
        if d == ".org":
            target = self.value(rest, line)
            if not self.output and not self.symbols.get("__started__"):
                self.base = target
                self.pc = target
            else:
                if target < self.pc:
                    raise AsmError(f"Line {line}: .org may not go backwards")
                self.emit_bytes(b"\x00" * (target - self.pc))
        elif d == ".equ":
            name, val = self.split_args(rest)
            if not second:
                self.symbols[name.strip()] = self.value(val, line)
        elif d in (".db", ".byte"):
            for a in self.split_args(rest):
                if a.startswith('"'):
                    s = a[1:-1].encode().decode("unicode_escape")
                    self.emit_bytes(s.encode("latin-1"))
                else:
                    self.emit_bytes([self.value(a, line) & 0xFF])
        elif d in (".dh", ".half"):
            for a in self.split_args(rest):
                v = self.value(a, line) & 0xFFFF
                self.emit_bytes([v & 0xFF, v >> 8])
        elif d in (".dw", ".word", ".dd"):
            for a in self.split_args(rest):
                self.emit32(self.value(a, line) & 0xFFFFFFFF)
        elif d in (".string", ".asciz"):
            s = rest.strip()[1:-1].encode().decode("unicode_escape")
            self.emit_bytes(s.encode("latin-1") + b"\x00")
        elif d == ".space":
            self.emit_bytes(b"\x00" * self.value(rest, line))
        elif d == ".align":
            n = self.value(rest, line)
            while self.pc % n:
                self.emit_bytes(b"\x00")
        elif d == ".fill":
            args = self.split_args(rest)
            target = self.value(args[0], line)
            fillv = self.value(args[1], line) & 0xFF if len(args) > 1 else 0
            if target < self.pc:
                if not second:
                    return
                raise AsmError(f"Line {line}: .fill target is behind us "
                               f"(already at 0x{self.pc:X}, target 0x{target:X})")
            self.emit_bytes(bytes([fillv]) * (target - self.pc))
        else:
            raise AsmError(f"Line {line}: unknown directive {d}")

    def instruction(self, mnem, rest, line, second):
        args = self.split_args(rest)

        # --- Pseudo-instruction: load a 32-bit constant ---
        if mnem == "li":
            rd = self.reg(args[0], line)
            v = self.value(args[1], line) & 0xFFFFFFFF
            self.emit32(encode_i(INSTRUCTIONS["movi"][0], rd, 0, v & 0xFFFF))
            self.emit32(encode_i(INSTRUCTIONS["movh"][0], rd, 0, (v >> 16) & 0xFFFF))
            return

        # --- Pseudo-instructions for absolute addresses -------------------
        # Example:  ldwa r5, BDA_CURX   ->   li at, BDA_CURX ; ldw r5, [at]
        # They use R13 as a scratch register ("at" = assembler temporary),
        # exactly like real MIPS assemblers do with $at.
        if mnem in ("ldwa", "ldha", "ldba", "ldsba", "stwa", "stha", "stba"):
            AT = 13
            is_store = mnem.startswith("st")
            real = {"ldwa": "ldw", "ldha": "ldh", "ldba": "ldb", "ldsba": "ldsb",
                    "stwa": "stw", "stha": "sth", "stba": "stb"}[mnem]
            if is_store:                      # stwa ADDRESS, rs
                addr = self.value(args[0], line) & 0xFFFFFFFF
                rs = self.reg(args[1], line)
            else:                             # ldwa rd, ADDRESS
                rs = self.reg(args[0], line)
                addr = self.value(args[1], line) & 0xFFFFFFFF
            if rs == AT and not is_store:
                pass                          # allowed, but mind the order
            self.emit32(encode_i(INSTRUCTIONS["movi"][0], AT, 0, addr & 0xFFFF))
            self.emit32(encode_i(INSTRUCTIONS["movh"][0], AT, 0, (addr >> 16) & 0xFFFF))
            self.emit32(encode_i(INSTRUCTIONS[real][0], rs, AT, 0))
            return

        if mnem not in INSTRUCTIONS:
            raise AsmError(f"Line {line}: unknown instruction '{mnem}'")
        op, fmt = INSTRUCTIONS[mnem]

        if fmt == "n":
            self.emit32(encode_r(op))

        elif fmt == "r":
            self.emit32(encode_r(op, self.reg(args[0], line)))

        elif fmt == "rr":
            self.emit32(encode_r(op, self.reg(args[0], line), self.reg(args[1], line)))

        elif fmt == "rrr":
            self.emit32(encode_r(op, self.reg(args[0], line),
                                 self.reg(args[1], line), self.reg(args[2], line)))

        elif fmt == "ri":
            self.emit32(encode_i(op, self.reg(args[0], line), 0,
                                 self.value(args[1], line) & 0xFFFF))

        elif fmt == "rri":
            self.emit32(encode_i(op, self.reg(args[0], line), self.reg(args[1], line),
                                 self.value(args[2], line) & 0xFFFF))

        elif fmt == "mem":
            if mnem.startswith("st"):          # stw [r2+8], r1
                base, off = self.memref(args[0], line)
                rd = self.reg(args[1], line)
            else:                              # ldw r1, [r2+8]
                rd = self.reg(args[0], line)
                base, off = self.memref(args[1], line)
            self.emit32(encode_i(op, rd, base, off & 0xFFFF))

        elif fmt == "i":
            self.emit32(encode_i(op, 0, 0, self.value(args[0], line) & 0xFFFF))

        elif fmt == "ir":                      # out 0x80, r1
            port = self.value(args[0], line) & 0xFFFF
            rd = self.reg(args[1], line)
            self.emit32(encode_i(op, rd, 0, port))

        elif fmt == "j":
            cond = COND["al"] if mnem == "jmp" else COND[mnem[1:]]
            here = self.pc
            target = self.value(args[0], line) if second else here
            off = (target - here) >> 2
            if not (-(1 << 19) <= off < (1 << 19)):
                raise AsmError(f"Line {line}: jump too far ({off} words)")
            self.emit32(encode_j(op, cond, off))

        elif fmt == "c":
            here = self.pc
            target = self.value(args[0], line) if second else here
            off = (target - here) >> 2
            if not (-(1 << 23) <= off < (1 << 23)):
                raise AsmError(f"Line {line}: call too far")
            self.emit32(encode_c(op, off))

        else:
            raise AsmError(f"Line {line}: format {fmt} not implemented")


def assemble_text(text, includes_dir=None):
    """Assembles text (with support for  .include \"file.asm\")."""
    if includes_dir:
        out_lines = []
        for ln in text.splitlines():
            m = re.match(r'^\s*\.include\s+"([^"]+)"', ln)
            if m:
                path = os.path.join(includes_dir, m.group(1))
                with open(path, "r", encoding="utf-8") as f:
                    out_lines.append(f.read())
            else:
                out_lines.append(ln)
        text = "\n".join(out_lines)

    asm = Assembler()
    asm.assemble(text, second_pass=False)
    data = asm.assemble(text, second_pass=True)
    return data, asm.symbols, asm.base


def main():
    if len(sys.argv) < 3:
        print("Usage: assembler.py <source.asm> <target.bin> [--sym file.sym]")
        return 1
    src, dst = sys.argv[1], sys.argv[2]
    with open(src, "r", encoding="utf-8") as f:
        text = f.read()
    try:
        data, symbols, base = assemble_text(text, os.path.dirname(os.path.abspath(src)))
    except AsmError as e:
        print(f"ERROR in {src}: {e}")
        return 1
    with open(dst, "wb") as f:
        f.write(data)
    if "--sym" in sys.argv:
        symfile = sys.argv[sys.argv.index("--sym") + 1]
        with open(symfile, "w") as f:
            for name, addr in sorted(symbols.items(), key=lambda kv: kv[1]):
                f.write(f"{addr:08X} {name}\n")
    print(f"{src}: {len(data)} bytes from 0x{base:08X}, {len(symbols)} symbols")
    return 0


if __name__ == "__main__":
    sys.exit(main())
