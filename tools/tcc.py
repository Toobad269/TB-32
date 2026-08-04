#!/usr/bin/env python3
"""
TCC -- Toobad C Compiler

Uebersetzt eine C-aehnliche Sprache in TB-32-Assembler. Damit kann ich den
Kernel und die Programme in einer Hochsprache schreiben statt in Assembler.

Aufbau wie bei jedem echten Compiler:
    1. LEXER   -- Text in Wortbausteine (Token) zerlegen
    2. PARSER  -- daraus einen Syntaxbaum bauen
    3. CODEGEN -- den Baum in Assembler-Befehle uebersetzen

Sprachumfang (bewusst klein, aber vollstaendig genug fuer ein Betriebssystem):
    Typen:        int (32 Bit), char (8 Bit), Zeiger (int*, char*), Arrays
    Anweisungen:  if/else, while, for, return, break, continue, Bloecke
    Ausdruecke:   + - * / %  == != < <= > >=  && || !  & | ^ ~ << >>
                  Zuweisung (auch += -= *= usw.), ++/--, ?:
                  Funktionsaufrufe, Adresse-von (&x), Dereferenzierung (*p)
    Sonstiges:    globale Variablen, Arrays, Strings, asm("..."), Kommentare

Aufruf:
    python3 tools/tcc.py quelle.c ziel.asm
"""

import re
import sys
import os

# ---------------------------------------------------------------------------
# 1. LEXER
# ---------------------------------------------------------------------------

KEYWORDS = {
    "int", "char", "void", "if", "else", "while", "for", "return",
    "break", "continue", "asm", "sizeof", "struct", "unsigned", "static",
}

TOKEN_SPEC = [
    ("COMMENT",  r"//[^\n]*|/\*.*?\*/"),
    ("NUMBER",   r"0[xX][0-9a-fA-F]+|\d+"),
    ("CHAR",     r"'(?:\\.|[^'])'"),
    ("STRING",   r'"(?:\\.|[^"])*"'),
    ("NAME",     r"[A-Za-z_][A-Za-z0-9_]*"),
    ("OP",       r"<<=|>>=|\+\+|--|->|<<|>>|<=|>=|==|!=|&&|\|\||"
                 r"[-+*/%&|^]=|[-+*/%=<>!&|^~?:;,(){}\[\].]"),
    ("NEWLINE",  r"\n"),
    ("SKIP",     r"[ \t\r]+"),
]
TOKEN_RE = re.compile("|".join(f"(?P<{n}>{p})" for n, p in TOKEN_SPEC), re.DOTALL)

ESCAPES = {"n": 10, "t": 9, "r": 13, "0": 0, "\\": 92, "'": 39, '"': 34, "e": 27}


class Token:
    __slots__ = ("kind", "value", "line")

    def __init__(self, kind, value, line):
        self.kind, self.value, self.line = kind, value, line

    def __repr__(self):
        return f"{self.kind}:{self.value}"


class CompileError(Exception):
    pass


def unescape(s):
    out = []
    i = 0
    while i < len(s):
        if s[i] == "\\" and i + 1 < len(s):
            out.append(ESCAPES.get(s[i + 1], ord(s[i + 1])))
            i += 2
        else:
            out.append(ord(s[i]))
            i += 1
    return out


def tokenize(text):
    tokens = []
    line = 1
    pos = 0
    while pos < len(text):
        m = TOKEN_RE.match(text, pos)
        if not m:
            raise CompileError(f"Zeile {line}: unbekanntes Zeichen '{text[pos]}'")
        pos = m.end()
        kind, val = m.lastgroup, m.group()
        if kind == "NEWLINE":
            line += 1
        elif kind in ("SKIP",):
            pass
        elif kind == "COMMENT":
            line += val.count("\n")
        elif kind == "NAME":
            tokens.append(Token("KEYWORD" if val in KEYWORDS else "NAME", val, line))
        else:
            tokens.append(Token(kind, val, line))
    tokens.append(Token("EOF", "", line))
    return tokens


# ---------------------------------------------------------------------------
# 2. PARSER -> Syntaxbaum (einfache Tupel: (art, ...))
# ---------------------------------------------------------------------------

class Parser:
    def __init__(self, tokens):
        self.toks = tokens
        self.i = 0

    def peek(self, k=0):
        return self.toks[min(self.i + k, len(self.toks) - 1)]

    def next(self):
        t = self.toks[self.i]
        self.i += 1
        return t

    def at(self, value):
        t = self.peek()
        return t.value == value and t.kind in ("OP", "KEYWORD")

    def accept(self, value):
        if self.at(value):
            self.next()
            return True
        return False

    def expect(self, value):
        if not self.accept(value):
            t = self.peek()
            raise CompileError(f"Zeile {t.line}: '{value}' erwartet, gefunden '{t.value}'")

    # -- Programm ----------------------------------------------------------

    def parse(self):
        decls = []
        while self.peek().kind != "EOF":
            decls.append(self.declaration())
        return decls

    def type_name(self):
        """Liest einen Typ:  int / char / void, danach beliebig viele '*'."""
        t = self.peek()
        if t.kind != "KEYWORD" or t.value not in ("int", "char", "void", "unsigned"):
            return None
        self.next()
        base = t.value
        if base == "unsigned":
            base = "int"
            if self.peek().value in ("int", "char"):
                base = self.next().value
        stars = 0
        while self.accept("*"):
            stars += 1
        return (base, stars)

    def declaration(self):
        static = self.accept("static")
        typ = self.type_name()
        if typ is None:
            t = self.peek()
            raise CompileError(f"Zeile {t.line}: Typ erwartet, gefunden '{t.value}'")
        name = self.next()
        if name.kind != "NAME":
            raise CompileError(f"Zeile {name.line}: Name erwartet, gefunden '{name.value}'")

        if self.at("("):                             # --- Funktion ---
            self.next()
            params = []
            if not self.accept(")"):
                while True:
                    ptyp = self.type_name()
                    if ptyp is None:
                        t = self.peek()
                        raise CompileError(f"Zeile {t.line}: Parametertyp erwartet")
                    pname = self.next().value
                    params.append((ptyp, pname))
                    if not self.accept(","):
                        break
                self.expect(")")
            if self.accept(";"):
                return ("proto", typ, name.value, params)
            body = self.block()
            return ("func", typ, name.value, params, body)

        # --- globale Variable, evtl. Array oder mit Startwert ---
        size = None
        if self.accept("["):
            if not self.at("]"):
                size = self.const_expr()
            self.expect("]")
        init = None
        if self.accept("="):
            init = self.initializer()
        self.expect(";")
        return ("global", typ, name.value, size, init, static)

    def initializer(self):
        if self.accept("{"):
            items = []
            while not self.accept("}"):
                items.append(self.assignment())
                self.accept(",")
            return ("initlist", items)
        return self.assignment()

    def const_expr(self):
        return self.assignment()

    # -- Anweisungen -------------------------------------------------------

    def block(self):
        self.expect("{")
        stmts = []
        while not self.accept("}"):
            stmts.append(self.statement())
        return ("block", stmts)

    def statement(self):
        t = self.peek()

        if self.at("{"):
            return self.block()
        if self.accept("if"):
            self.expect("(")
            cond = self.expression()
            self.expect(")")
            then = self.statement()
            other = self.statement() if self.accept("else") else None
            return ("if", cond, then, other)
        if self.accept("while"):
            self.expect("(")
            cond = self.expression()
            self.expect(")")
            return ("while", cond, self.statement())
        if self.accept("for"):
            self.expect("(")
            init = None if self.at(";") else self.simple_statement()
            self.expect(";")
            cond = None if self.at(";") else self.expression()
            self.expect(";")
            step = None if self.at(")") else self.expression()
            self.expect(")")
            return ("for", init, cond, step, self.statement())
        if self.accept("return"):
            val = None if self.at(";") else self.expression()
            self.expect(";")
            return ("return", val)
        if self.accept("break"):
            self.expect(";")
            return ("break",)
        if self.accept("continue"):
            self.expect(";")
            return ("continue",)
        if self.accept("asm"):
            self.expect("(")
            code = self.next()
            if code.kind != "STRING":
                raise CompileError(f"Zeile {t.line}: asm() braucht einen Text")
            self.expect(")")
            self.expect(";")
            return ("asm", code.value[1:-1])
        if self.accept(";"):
            return ("block", [])

        # lokale Variable?
        if t.kind == "KEYWORD" and t.value in ("int", "char", "void", "unsigned"):
            return self.local_decl()

        st = self.simple_statement()
        self.expect(";")
        return st

    def local_decl(self):
        typ = self.type_name()
        decls = []
        while True:
            name = self.next()
            if name.kind != "NAME":
                raise CompileError(f"Zeile {name.line}: Variablenname erwartet")
            size = None
            if self.accept("["):
                size = self.const_expr()
                self.expect("]")
            init = self.initializer() if self.accept("=") else None
            decls.append((typ, name.value, size, init))
            if not self.accept(","):
                break
        self.expect(";")
        return ("localdecl", decls)

    def simple_statement(self):
        return ("expr", self.expression())

    # -- Ausdruecke (nach Vorrang gestaffelt) ------------------------------

    def expression(self):
        e = self.assignment()
        while self.accept(","):
            e = ("comma", e, self.assignment())
        return e

    ASSIGN_OPS = {"=", "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "<<=", ">>="}

    def assignment(self):
        left = self.ternary()
        t = self.peek()
        if t.kind == "OP" and t.value in self.ASSIGN_OPS:
            self.next()
            right = self.assignment()
            return ("assign", t.value, left, right)
        return left

    def ternary(self):
        cond = self.logical_or()
        if self.accept("?"):
            a = self.assignment()
            self.expect(":")
            b = self.ternary()
            return ("ternary", cond, a, b)
        return cond

    def logical_or(self):
        left = self.logical_and()
        while self.accept("||"):
            left = ("logor", left, self.logical_and())
        return left

    def logical_and(self):
        left = self.bit_or()
        while self.accept("&&"):
            left = ("logand", left, self.bit_or())
        return left

    def bit_or(self):
        left = self.bit_xor()
        while self.at("|"):
            self.next()
            left = ("bin", "|", left, self.bit_xor())
        return left

    def bit_xor(self):
        left = self.bit_and()
        while self.at("^"):
            self.next()
            left = ("bin", "^", left, self.bit_and())
        return left

    def bit_and(self):
        left = self.equality()
        while self.at("&") and self.peek(1).value != "&":
            self.next()
            left = ("bin", "&", left, self.equality())
        return left

    def equality(self):
        left = self.relational()
        while self.peek().value in ("==", "!=") and self.peek().kind == "OP":
            op = self.next().value
            left = ("cmp", op, left, self.relational())
        return left

    def relational(self):
        left = self.shift()
        while self.peek().value in ("<", ">", "<=", ">=") and self.peek().kind == "OP":
            op = self.next().value
            left = ("cmp", op, left, self.shift())
        return left

    def shift(self):
        left = self.additive()
        while self.peek().value in ("<<", ">>") and self.peek().kind == "OP":
            op = self.next().value
            left = ("bin", op, left, self.additive())
        return left

    def additive(self):
        left = self.multiplicative()
        while self.peek().value in ("+", "-") and self.peek().kind == "OP":
            op = self.next().value
            left = ("bin", op, left, self.multiplicative())
        return left

    def multiplicative(self):
        left = self.unary()
        while self.peek().value in ("*", "/", "%") and self.peek().kind == "OP":
            op = self.next().value
            left = ("bin", op, left, self.unary())
        return left

    def unary(self):
        t = self.peek()
        if t.kind == "OP":
            if t.value == "-":
                self.next()
                return ("neg", self.unary())
            if t.value == "+":
                self.next()
                return self.unary()
            if t.value == "!":
                self.next()
                return ("not", self.unary())
            if t.value == "~":
                self.next()
                return ("bnot", self.unary())
            if t.value == "*":
                self.next()
                return ("deref", self.unary())
            if t.value == "&":
                self.next()
                return ("addr", self.unary())
            if t.value == "++":
                self.next()
                return ("preinc", self.unary(), 1)
            if t.value == "--":
                self.next()
                return ("preinc", self.unary(), -1)
            if t.value == "(":
                # Typumwandlung?  (char*)x
                save = self.i
                self.next()
                typ = self.type_name()
                if typ is not None and self.at(")"):
                    self.next()
                    return ("cast", typ, self.unary())
                self.i = save
        if t.kind == "KEYWORD" and t.value == "sizeof":
            self.next()
            self.expect("(")
            typ = self.type_name()
            if typ is None:
                self.expression()
                size = 4
            else:
                size = 4 if (typ[1] > 0 or typ[0] == "int") else 1
            self.expect(")")
            return ("num", size)
        return self.postfix()

    def postfix(self):
        e = self.primary()
        while True:
            if self.at("("):
                self.next()
                args = []
                if not self.accept(")"):
                    while True:
                        args.append(self.assignment())
                        if not self.accept(","):
                            break
                    self.expect(")")
                e = ("call", e, args)
            elif self.at("["):
                self.next()
                idx = self.expression()
                self.expect("]")
                e = ("index", e, idx)
            elif self.at("++"):
                self.next()
                e = ("postinc", e, 1)
            elif self.at("--"):
                self.next()
                e = ("postinc", e, -1)
            else:
                return e

    def primary(self):
        t = self.next()
        if t.kind == "NUMBER":
            return ("num", int(t.value, 0))
        if t.kind == "CHAR":
            body = t.value[1:-1]
            return ("num", unescape(body)[0])
        if t.kind == "STRING":
            return ("str", unescape(t.value[1:-1]))
        if t.kind == "NAME":
            return ("var", t.value)
        if t.value == "(":
            e = self.expression()
            self.expect(")")
            return e
        raise CompileError(f"Zeile {t.line}: unerwartet '{t.value}'")


# ---------------------------------------------------------------------------
# 3. CODEGEN -- Syntaxbaum -> TB-32-Assembler
#
# Aufrufkonvention:
#   Argumente in r1..r5 (weitere auf dem Stack), Rueckgabe in r0.
#   r6..r9 muessen erhalten bleiben, r10..r12 sind frei.
#   Jede Funktion baut sich einen Rahmen mit fp (r14).
#
# Auswertung: Das Ergebnis eines Ausdrucks landet IMMER in r0. Braucht ein
# Operator zwei Werte, wird der linke solange auf den Stack gelegt.
# ---------------------------------------------------------------------------

class Codegen:
    def __init__(self):
        self.out = []
        self.globals = {}          # name -> (typ, groesse in bytes, ist_array)
        self.funcs = set()
        self.func_types = {}       # name -> Rückgabetyp, für  f()[i]  und  *f()
        self.strings = []
        self.locals = {}
        self.local_types = {}
        self.frame = 0
        self.label_n = 0
        self.loops = []
        self.cur_func = ""

    def emit(self, s):
        self.out.append("    " + s)

    def label(self, name):
        self.out.append(name + ":")

    def new_label(self, hint="L"):
        self.label_n += 1
        return f".{hint}{self.label_n}"

    # -- Typhilfen ---------------------------------------------------------

    def is_pointer(self, typ):
        return typ is not None and typ[1] > 0

    def elem_size(self, typ):
        """Groesse dessen, worauf ein Zeiger zeigt."""
        if typ is None:
            return 4
        base, stars = typ
        if stars > 1:
            return 4
        return 1 if base == "char" else 4

    def var_size(self, typ):
        return 1 if (typ[0] == "char" and typ[1] == 0) else 4

    # -- Hauptlauf ---------------------------------------------------------

    def compile(self, decls):
        self.out.append("; --- vom Toobad C Compiler erzeugt ---")

        for d in decls:                             # erst alles anmelden
            if d[0] == "func":
                self.funcs.add(d[2])
                self.func_types[d[2]] = d[1]
            elif d[0] == "proto":
                self.funcs.add(d[2])
                self.func_types[d[2]] = d[1]
            elif d[0] == "global":
                _, typ, name, size, init, static = d
                n = 1
                if size is not None:
                    n = self.const_value(size)
                self.globals[name] = (typ, n, size is not None)

        for d in decls:
            if d[0] == "func":
                self.gen_func(d)

        self.gen_data(decls)
        return "\n".join(self.out) + "\n"

    def const_value(self, node):
        if node[0] == "num":
            return node[1]
        if node[0] == "bin":
            a, b = self.const_value(node[2]), self.const_value(node[3])
            return {"+": a + b, "-": a - b, "*": a * b,
                    "/": a // b if b else 0}[node[1]]
        raise CompileError("Konstante erwartet (Arraygroesse muss fest sein)")

    # -- Datenbereich ------------------------------------------------------

    def gen_data(self, decls):
        self.out.append("")
        self.out.append("; --- Daten ---")
        for i, chars in enumerate(self.strings):
            self.out.append(f"__str{i}:")
            body = ", ".join(str(c) for c in chars)
            self.out.append(f"    .db {body}, 0" if body else "    .db 0")
        for d in decls:
            if d[0] != "global":
                continue
            _, typ, name, size, init, static = d
            self.out.append(f"{name}:")
            esz = self.var_size(typ) if size is None else \
                (1 if typ[0] == "char" and typ[1] == 0 else 4)
            n = 1 if size is None else self.const_value(size)
            if init is None:
                self.out.append(f"    .space {esz * n}")
            elif init[0] == "initlist":
                vals = [self.const_value(x) for x in init[1]]
                directive = ".db" if esz == 1 else ".dw"
                self.out.append(f"    {directive} " + ", ".join(str(v) for v in vals))
                if len(vals) < n:
                    self.out.append(f"    .space {esz * (n - len(vals))}")
            elif init[0] == "str":
                self.out.append("    .db " + ", ".join(str(c) for c in init[1]) + ", 0")
            else:
                self.out.append(f"    .dw {self.const_value(init)}")
            self.out.append("    .align 4")

    # -- Funktionen --------------------------------------------------------

    def gen_func(self, d):
        _, typ, name, params, body = d
        self.cur_func = name
        self.locals = {}
        self.local_types = {}
        self.frame = 0

        for i, (ptyp, pname) in enumerate(params):
            self.frame += 4
            self.locals[pname] = -self.frame
            self.local_types[pname] = ptyp

        self.collect_locals(body)

        self.out.append("")
        self.out.append(f"; ===== {name}() =====")
        self.label(name)
        self.emit("push fp")
        self.emit("mov fp, sp")
        if self.frame:
            self.emit(f"subi sp, sp, {self.frame}")
        self.emit("push r6")
        self.emit("push r7")
        self.emit("push r8")
        self.emit("push r9")

        for i, (ptyp, pname) in enumerate(params):
            off = self.locals[pname]
            if i < 5:
                self.emit(f"stw [fp{off:+d}], r{i+1}")
            else:
                self.emit(f"ldw r10, [fp+{4*(i-5)+8}]")
                self.emit(f"stw [fp{off:+d}], r10")

        self.gen_stmt(body)

        self.label(f".__ret_{name}")
        self.emit("pop r9")
        self.emit("pop r8")
        self.emit("pop r7")
        self.emit("pop r6")
        self.emit("mov sp, fp")
        self.emit("pop fp")
        self.emit("ret")

    def collect_locals(self, node):
        """Alle lokalen Variablen einsammeln und im Rahmen platzieren."""
        if not isinstance(node, tuple):
            return
        if node[0] == "localdecl":
            for typ, name, size, init in node[1]:
                if size is not None:
                    n = self.const_value(size)
                    esz = 1 if (typ[0] == "char" and typ[1] == 0) else 4
                    total = ((esz * n) + 3) & ~3
                    self.frame += total
                    self.locals[name] = -self.frame
                    self.local_types[name] = (typ[0], typ[1] + 1)
                    self.local_types["__arr__" + name] = True
                else:
                    self.frame += 4
                    self.locals[name] = -self.frame
                    self.local_types[name] = typ
        for x in node:
            if isinstance(x, tuple):
                self.collect_locals(x)
            elif isinstance(x, list):
                for y in x:
                    if isinstance(y, tuple):
                        self.collect_locals(y)

    # -- Anweisungen -------------------------------------------------------

    def gen_stmt(self, node):
        kind = node[0]

        if kind == "block":
            for s in node[1]:
                self.gen_stmt(s)

        elif kind == "localdecl":
            for typ, name, size, init in node[1]:
                if init is None:
                    continue
                off = self.locals[name]
                if size is not None and init[0] == "initlist":
                    esz = 1 if (typ[0] == "char" and typ[1] == 0) else 4
                    for i, item in enumerate(init[1]):
                        self.gen_expr(item)
                        st = "stb" if esz == 1 else "stw"
                        self.emit(f"{st} [fp{off + i*esz:+d}], r0")
                elif size is not None and init[0] == "str":
                    for i, c in enumerate(init[1] + [0]):
                        self.emit(f"movi r10, {c}")
                        self.emit(f"stb [fp{off + i:+d}], r10")
                else:
                    self.gen_expr(init)
                    st = "stb" if self.var_size(typ) == 1 else "stw"
                    self.emit(f"{st} [fp{off:+d}], r0")

        elif kind == "expr":
            self.gen_expr(node[1])

        elif kind == "if":
            _, cond, then, other = node
            lelse = self.new_label("else")
            lend = self.new_label("endif")
            self.gen_cond(cond, lelse, False)
            self.gen_stmt(then)
            if other is not None:
                self.emit(f"jmp {lend}")
            self.label(lelse)
            if other is not None:
                self.gen_stmt(other)
                self.label(lend)

        elif kind == "while":
            _, cond, body = node
            ltop = self.new_label("while")
            lend = self.new_label("wend")
            self.loops.append((ltop, lend))
            self.label(ltop)
            self.gen_cond(cond, lend, False)
            self.gen_stmt(body)
            self.emit(f"jmp {ltop}")
            self.label(lend)
            self.loops.pop()

        elif kind == "for":
            _, init, cond, step, body = node
            ltop = self.new_label("for")
            lstep = self.new_label("fstep")
            lend = self.new_label("fend")
            if init is not None:
                self.gen_stmt(init)
            self.loops.append((lstep, lend))
            self.label(ltop)
            if cond is not None:
                self.gen_cond(cond, lend, False)
            self.gen_stmt(body)
            self.label(lstep)
            if step is not None:
                self.gen_expr(step)
            self.emit(f"jmp {ltop}")
            self.label(lend)
            self.loops.pop()

        elif kind == "return":
            if node[1] is not None:
                self.gen_expr(node[1])
            self.emit(f"jmp .__ret_{self.cur_func}")

        elif kind == "break":
            if not self.loops:
                raise CompileError("break ausserhalb einer Schleife")
            self.emit(f"jmp {self.loops[-1][1]}")

        elif kind == "continue":
            if not self.loops:
                raise CompileError("continue ausserhalb einer Schleife")
            self.emit(f"jmp {self.loops[-1][0]}")

        elif kind == "asm":
            for line in node[1].split("\\n"):
                if line.strip():
                    self.emit(line.strip())

        else:
            raise CompileError(f"unbekannte Anweisung {kind}")

    # -- Bedingungen: springt nach <label>, wenn Bedingung falsch (want=False)

    def gen_cond(self, node, label, want_true):
        if node[0] == "cmp":
            _, op, a, b = node
            self.gen_expr(a)
            self.emit("push r0")
            self.gen_expr(b)
            self.emit("mov r10, r0")
            self.emit("pop r0")
            self.emit("cmp r0, r10")
            jumps = {"==": ("jz", "jnz"), "!=": ("jnz", "jz"),
                     "<": ("jl", "jge"), ">": ("jg", "jle"),
                     "<=": ("jle", "jg"), ">=": ("jge", "jl")}
            self.emit(f"{jumps[op][0 if want_true else 1]} {label}")
            return

        if node[0] == "logand":
            if want_true:
                lskip = self.new_label("and")
                self.gen_cond(node[1], lskip, False)
                self.gen_cond(node[2], label, True)
                self.label(lskip)
            else:
                self.gen_cond(node[1], label, False)
                self.gen_cond(node[2], label, False)
            return

        if node[0] == "logor":
            if want_true:
                self.gen_cond(node[1], label, True)
                self.gen_cond(node[2], label, True)
            else:
                lskip = self.new_label("or")
                self.gen_cond(node[1], lskip, True)
                self.gen_cond(node[2], label, False)
                self.label(lskip)
            return

        if node[0] == "not":
            self.gen_cond(node[1], label, not want_true)
            return

        self.gen_expr(node)
        self.emit("cmpi r0, 0")
        self.emit(f"{'jnz' if want_true else 'jz'} {label}")

    # -- Ausdruecke: Ergebnis landet in r0 --------------------------------

    def gen_expr(self, node):
        kind = node[0]

        if kind == "num":
            v = node[1] & 0xFFFFFFFF
            if -32768 <= node[1] <= 32767:
                self.emit(f"movi r0, {node[1]}")
            else:
                self.emit(f"li r0, {v}")

        elif kind == "str":
            idx = len(self.strings)
            self.strings.append(node[1])
            self.emit(f"li r0, __str{idx}")

        elif kind == "var":
            name = node[1]
            if name in self.locals:
                off = self.locals[name]
                if self.local_types.get("__arr__" + name):
                    self.emit(f"addi r0, fp, {off}")        # Array = Adresse
                else:
                    typ = self.local_types[name]
                    ld = "ldb" if self.var_size(typ) == 1 else "ldw"
                    self.emit(f"{ld} r0, [fp{off:+d}]")
            elif name in self.globals:
                typ, n, is_array = self.globals[name]
                if is_array:
                    self.emit(f"li r0, {name}")
                else:
                    ld = "ldba" if self.var_size(typ) == 1 else "ldwa"
                    self.emit(f"{ld} r0, {name}")
            elif name in self.funcs:
                self.emit(f"li r0, {name}")
            else:
                raise CompileError(f"unbekannte Variable '{name}'")

        elif kind == "addr":
            self.gen_addr(node[1])

        elif kind == "deref":
            self.gen_expr(node[1])
            typ = self.type_of(node[1])
            ld = "ldb" if self.elem_size(typ) == 1 else "ldw"
            self.emit(f"{ld} r0, [r0]")

        elif kind == "index":
            self.gen_addr(node)
            typ = self.type_of(node[1])
            ld = "ldb" if self.elem_size(typ) == 1 else "ldw"
            self.emit(f"{ld} r0, [r0]")

        elif kind == "cast":
            self.gen_expr(node[2])

        elif kind == "neg":
            self.gen_expr(node[1])
            self.emit("neg r0, r0")

        elif kind == "bnot":
            self.gen_expr(node[1])
            self.emit("not r0, r0")

        elif kind == "not":
            self.gen_expr(node[1])
            ltrue = self.new_label("n")
            lend = self.new_label("ne")
            self.emit("cmpi r0, 0")
            self.emit(f"jz {ltrue}")
            self.emit("movi r0, 0")
            self.emit(f"jmp {lend}")
            self.label(ltrue)
            self.emit("movi r0, 1")
            self.label(lend)

        elif kind in ("logand", "logor"):
            ltrue = self.new_label("lt")
            lend = self.new_label("le")
            self.gen_cond(node, ltrue, True)
            self.emit("movi r0, 0")
            self.emit(f"jmp {lend}")
            self.label(ltrue)
            self.emit("movi r0, 1")
            self.label(lend)

        elif kind == "cmp":
            ltrue = self.new_label("ct")
            lend = self.new_label("ce")
            self.gen_cond(node, ltrue, True)
            self.emit("movi r0, 0")
            self.emit(f"jmp {lend}")
            self.label(ltrue)
            self.emit("movi r0, 1")
            self.label(lend)

        elif kind == "bin":
            _, op, a, b = node
            # Zeigerarithmetik:  p + 1  ->  p + sizeof(*p)
            scale = 1
            ta = self.type_of(a)
            if op in ("+", "-") and self.is_pointer(ta):
                scale = self.elem_size(ta)
            self.gen_expr(a)
            self.emit("push r0")
            self.gen_expr(b)
            if scale > 1:
                self.emit(f"muli r0, r0, {scale}")
            self.emit("mov r10, r0")
            self.emit("pop r0")
            ops = {"+": "add", "-": "sub", "*": "mul", "/": "div", "%": "mod",
                   "&": "and", "|": "or", "^": "xor", "<<": "shl", ">>": "shr"}
            self.emit(f"{ops[op]} r0, r0, r10")

        elif kind == "ternary":
            _, cond, a, b = node
            lelse = self.new_label("t")
            lend = self.new_label("te")
            self.gen_cond(cond, lelse, False)
            self.gen_expr(a)
            self.emit(f"jmp {lend}")
            self.label(lelse)
            self.gen_expr(b)
            self.label(lend)

        elif kind == "assign":
            _, op, target, value = node
            if op == "=":
                self.gen_expr(value)
            else:
                self.gen_expr(target)
                self.emit("push r0")
                self.gen_expr(value)
                self.emit("mov r10, r0")
                self.emit("pop r0")
                ops = {"+=": "add", "-=": "sub", "*=": "mul", "/=": "div",
                       "%=": "mod", "&=": "and", "|=": "or", "^=": "xor",
                       "<<=": "shl", ">>=": "shr"}
                self.emit(f"{ops[op]} r0, r0, r10")
            self.store_to(target)

        elif kind in ("preinc", "postinc"):
            target, delta = node[1], node[2]
            typ = self.type_of(target)
            step = self.elem_size(typ) if self.is_pointer(typ) else 1
            self.gen_expr(target)
            if kind == "postinc":
                self.emit("push r0")
            self.emit(f"addi r0, r0, {delta * step}")
            self.store_to(target)
            if kind == "postinc":
                self.emit("pop r0")

        elif kind == "call":
            self.gen_call(node)

        elif kind == "comma":
            self.gen_expr(node[1])
            self.gen_expr(node[2])

        else:
            raise CompileError(f"unbekannter Ausdruck {kind}")

    def gen_call(self, node):
        _, target, args = node
        for a in reversed(args[5:]):
            self.gen_expr(a)
            self.emit("push r0")
        # die ersten fuenf Argumente in r1..r5
        for i, a in enumerate(args[:5]):
            self.gen_expr(a)
            self.emit("push r0")
        for i in range(len(args[:5]) - 1, -1, -1):
            self.emit(f"pop r{i+1}")

        if target[0] == "var" and target[1] in self.funcs:
            self.emit(f"call {target[1]}")
        else:
            self.gen_expr(target)
            self.emit("mov r10, r0")
            self.emit("callr r10")
        if len(args) > 5:
            self.emit(f"addi sp, sp, {4 * (len(args) - 5)}")

    def gen_addr(self, node):
        """Adresse eines Ziels nach r0."""
        if node[0] == "var":
            name = node[1]
            if name in self.locals:
                self.emit(f"addi r0, fp, {self.locals[name]}")
            elif name in self.globals:
                self.emit(f"li r0, {name}")
            elif name in self.funcs:
                self.emit(f"li r0, {name}")
            else:
                raise CompileError(f"unbekannte Variable '{name}'")
        elif node[0] == "index":
            base, idx = node[1], node[2]
            typ = self.type_of(base)
            esz = self.elem_size(typ)
            self.gen_expr(base)
            self.emit("push r0")
            self.gen_expr(idx)
            if esz > 1:
                self.emit(f"muli r0, r0, {esz}")
            self.emit("mov r10, r0")
            self.emit("pop r0")
            self.emit("add r0, r0, r10")
        elif node[0] == "deref":
            self.gen_expr(node[1])
        else:
            raise CompileError("dieser Ausdruck hat keine Adresse")

    def store_to(self, target):
        """Speichert r0 in das Ziel (r0 bleibt erhalten)."""
        if target[0] == "var":
            name = target[1]
            if name in self.locals:
                typ = self.local_types[name]
                st = "stb" if self.var_size(typ) == 1 else "stw"
                self.emit(f"{st} [fp{self.locals[name]:+d}], r0")
                return
            if name in self.globals:
                typ, n, is_array = self.globals[name]
                st = "stba" if self.var_size(typ) == 1 else "stwa"
                self.emit(f"{st} {name}, r0")
                return
            raise CompileError(f"unbekannte Variable '{name}'")

        # Ziel ist ein Speicherplatz: Adresse berechnen, ohne r0 zu verlieren
        self.emit("push r0")
        self.gen_addr(target)
        self.emit("mov r11, r0")
        self.emit("pop r0")
        typ = self.type_of(target[1]) if target[0] in ("index", "deref") else None
        esz = self.elem_size(typ)
        st = "stb" if esz == 1 else "stw"
        self.emit(f"{st} [r11], r0")

    # -- ganz einfache Typermittlung (reicht fuer Zeigerarithmetik) --------

    def type_of(self, node):
        if not isinstance(node, tuple):
            return None
        if node[0] == "var":
            name = node[1]
            if name in self.locals:
                return self.local_types.get(name)
            if name in self.globals:
                typ, n, is_array = self.globals[name]
                return (typ[0], typ[1] + 1) if is_array else typ
            return None
        if node[0] == "str":
            return ("char", 1)
        if node[0] == "cast":
            return node[1]
        if node[0] == "index":
            t = self.type_of(node[1])
            return (t[0], t[1] - 1) if t and t[1] > 0 else ("int", 0)
        if node[0] == "deref":
            t = self.type_of(node[1])
            return (t[0], t[1] - 1) if t and t[1] > 0 else ("int", 0)
        if node[0] == "addr":
            t = self.type_of(node[1])
            return (t[0], t[1] + 1) if t else ("int", 1)
        if node[0] == "bin":
            ta = self.type_of(node[2])
            return ta if self.is_pointer(ta) else self.type_of(node[3])
        if node[0] == "assign":
            return self.type_of(node[2])
        if node[0] == "call":
            # Rückgabetyp der gerufenen Funktion -- sonst würde  f()[i]
            # den Index falsch skalieren, wenn f() ein char* liefert.
            t = node[1]
            if t[0] == "var" and t[1] in self.func_types:
                return self.func_types[t[1]]
        return None


# ---------------------------------------------------------------------------

def zeilen_im_kommentar(text):
    """Fuer jede Zeile: faengt sie mitten in einem /* ... */ an?

    Der Praeprozessor arbeitet zeilenweise und wuerde sonst ein '#' auch dann
    fuer eine Anweisung halten, wenn es nur in einem Kommentar steht. Genau das
    hat einmal eine Zeile geloescht, in der das schliessende '*/' stand -- der
    Kommentar blieb offen und hat den nachfolgenden echten Quelltext gefressen.
    """
    drin = []
    block = False
    for ln in text.splitlines():
        drin.append(block)
        i = 0
        n = len(ln)
        while i < n:
            c = ln[i]
            if block:
                if c == "*" and i + 1 < n and ln[i + 1] == "/":
                    block = False
                    i += 2
                else:
                    i += 1
                continue
            if c == "/" and i + 1 < n and ln[i + 1] == "/":
                break                            # Zeilenkommentar
            if c == "/" and i + 1 < n and ln[i + 1] == "*":
                block = True
                i += 2
                continue
            if c == '"' or c == "'":              # Text- oder Zeichenkonstante
                ende = c
                i += 1
                while i < n:
                    if ln[i] == "\\":
                        i += 2
                        continue
                    if ln[i] == ende:
                        i += 1
                        break
                    i += 1
                continue
            i += 1
    return drin


def compile_source(text, includes_dir=None):
    # #include "datei.c" einfach hineinkopieren -- ein echter Praeprozessor
    # ist fuer unsere Zwecke nicht noetig.
    if includes_dir:
        seen = set()
        changed = True
        while changed:
            changed = False
            lines = []
            kommentar = zeilen_im_kommentar(text)
            for nr, ln in enumerate(text.splitlines()):
                m = None if kommentar[nr] else re.match(r'^\s*#include\s+"([^"]+)"', ln)
                if m and m.group(1) not in seen:
                    seen.add(m.group(1))
                    with open(os.path.join(includes_dir, m.group(1)), encoding="utf-8") as f:
                        lines.append(f.read())
                    changed = True
                elif m:
                    pass
                else:
                    lines.append(ln)
            text = "\n".join(lines)

    # Sehr einfacher Präprozessor: #define NAME wert  (reine Textersetzung)
    defines = {}
    lines = []
    kommentar = zeilen_im_kommentar(text)
    for nr, ln in enumerate(text.splitlines()):
        if kommentar[nr]:                        # '#' im Kommentar ist keins
            lines.append(ln)
            continue
        m = re.match(r"^\s*#define\s+([A-Za-z_]\w*)\s*(.*)$", ln)
        if m:
            # Einen abschliessenden Kommentar NICHT mit in den Wert nehmen.
            #
            #   #define SRC_BUF 0x280000    /* Quelltext, bis 64 KB */
            #
            # Ohne diese Zeile ist der Wert von SRC_BUF die Zahl *samt*
            # Kommentar. Wer den Namen dann irgendwo in einem Kommentar
            # erwaehnt, bekommt ein `*/` mitten hineingesetzt -- der
            # Kommentar endet dort, und der Rest des Satzes wird als
            # Quelltext gelesen. Der Fehler zeigt dann auf eine voellig
            # harmlose Prosazeile.
            wert = m.group(2)
            schnitt = wert.find("/*")
            if schnitt >= 0:
                wert = wert[:schnitt]
            schnitt = wert.find("//")
            if schnitt >= 0:
                wert = wert[:schnitt]
            defines[m.group(1)] = wert.strip()
            lines.append("")
        elif re.match(r"^\s*#", ln):
            lines.append("")
        else:
            lines.append(ln)
    text = "\n".join(lines)
    for _ in range(4):                       # verschachtelte Makros auflösen
        before = text
        for name, val in defines.items():
            text = re.sub(rf"\b{re.escape(name)}\b", val, text)
        if text == before:
            break

    tokens = tokenize(text)
    ast = Parser(tokens).parse()
    return Codegen().compile(ast)


def main():
    if len(sys.argv) < 3:
        print("Aufruf: tcc.py quelle.c ziel.asm")
        return 1
    src, dst = sys.argv[1], sys.argv[2]
    with open(src, encoding="utf-8") as f:
        text = f.read()
    try:
        asm = compile_source(text, os.path.dirname(os.path.abspath(src)))
    except CompileError as e:
        print(f"FEHLER in {src}: {e}")
        return 1
    with open(dst, "w", encoding="utf-8") as f:
        f.write(asm)
    print(f"{src} -> {dst}  ({len(asm.splitlines())} Zeilen Assembler)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
