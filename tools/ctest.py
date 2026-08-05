#!/usr/bin/env python3
"""
Quick test for the C compiler: compiles a C program, loads it directly into
the virtual CPU's RAM and runs main(). Prints the return value and
everything the program wrote via putchar().

    python3 tools/ctest.py test.c
    python3 tools/ctest.py --selftest        (built-in test cases)
"""

import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)

from tools.tcc import compile_source, CompileError
from tools.assembler import assemble_text, AsmError
from hardware.bus import Bus
from hardware.cpu import CPU
from hardware.devices import VGA

PROLOG = """
.equ P_DEBUG, 0x0080
.org 0x00001000
__start:
    li sp, 0x0009FFF0
    call main
    brk

; --- Minimal runtime library for the tests ---
putchar:
    out P_DEBUG, r1
    ret
"""


class TestBus(Bus):
    def __init__(self, vga):
        super().__init__(vga)
        self.output = []
        self.line = ""

    def port_out(self, port, value):
        if port == 0x0080:
            ch = value & 0xFF
            if ch == 10:
                self.output.append(self.line)
                self.line = ""
            else:
                self.line += chr(ch)
        else:
            super().port_out(port, value)

    def port_in(self, port):
        return 0


def run_c(source, max_steps=20_000_000, dump_asm=False):
    asm = compile_source(source)
    full = PROLOG + "\n" + asm
    if dump_asm:
        print(full)
    data, symbols, base = assemble_text(full)

    bus = TestBus(VGA())
    bus.ram[base:base + len(data)] = data
    cpu = CPU(bus)
    cpu.reset()
    cpu.pc = base
    cpu.r[15] = 0x0009FFF0

    steps = 0
    while not cpu.halted and steps < max_steps:
        steps += cpu.run(100000)
    if bus.line:
        bus.output.append(bus.line)
    return cpu.r[0], bus.output, cpu, steps


TESTS = [
    ("Arithmetic", """
     int main() { return 2 + 3 * 4 - 6 / 2; }
     """, 11),

    ("Loop", """
     int main() { int i; int s; s = 0; for (i = 1; i <= 10; i++) s += i; return s; }
     """, 55),

    ("Functions + Recursion", """
     int fak(int n) { if (n <= 1) return 1; return n * fak(n - 1); }
     int main() { return fak(7); }
     """, 5040),

    ("while + break + continue", """
     int main() {
        int i; int s; i = 0; s = 0;
        while (1) { i++; if (i > 100) break; if (i % 2) continue; s += i; }
        return s;
     }
     """, 2550),

    ("Pointers and Arrays", """
     int arr[8];
     int main() {
        int i; int* p;
        for (i = 0; i < 8; i++) arr[i] = i * i;
        p = arr;
        return p[5] + *(p + 3) + arr[7];
     }
     """, 25 + 9 + 49),

    ("Strings", """
     char buf[16];
     int strlen(char* s) { int n; n = 0; while (*s) { n++; s++; } return n; }
     int main() {
        char* t; int i;
        t = "Hallo Welt";
        for (i = 0; i < 5; i++) buf[i] = t[i];
        buf[5] = 0;
        return strlen(t) * 100 + strlen(buf);
     }
     """, 1005),

    ("Logic and Comparisons", """
     int main() {
        int a; int b; a = 5; b = 12;
        return (a < b) * 1 + (a == 5 && b > 10) * 10 + (a > b || b == 12) * 100
             + (!0) * 1000 + (a != b ? 10000 : 0);
     }
     """, 11111),

    ("Bit Operations", """
     int main() {
        int x; x = 0xF0;
        return ((x >> 4) | 0x100) + (x & 0x30) + (x ^ 0xFF) + (~0 & 7) + (1 << 5);
     }
     """, (0xF0 >> 4 | 0x100) + (0xF0 & 0x30) + (0xF0 ^ 0xFF) + 7 + 32),

    ("Global Variables with Initial Value", """
     int zaehler = 100;
     char text[4] = {65, 66, 67, 0};
     int main() { zaehler += text[0] + text[2]; return zaehler; }
     """, 100 + 65 + 67),

    ("Pointer to Pointer / Function Pointer", """
     int verdopple(int x) { return x * 2; }
     int anwenden(int f, int wert) { int* q; return wert; }
     int main() {
        int a; int* p; int** pp;
        a = 21; p = &a; pp = &p;
        **pp = verdopple(**pp);
        return a;
     }
     """, 42),

    ("Output", """
     int putchar(int c);
     int main() {
        char* s; s = "Compiler laeuft!";
        while (*s) { putchar(*s); s++; }
        putchar(10);
        return 7;
     }
     """, 7),
]


def selftest():
    ok = 0
    for name, src, expect in TESTS:
        try:
            r0, out, cpu, steps = run_c(src)
            got = r0 if r0 < 0x80000000 else r0 - 0x100000000
            if got == expect:
                print(f"  [OK]     {name:38s} = {got}"
                      + (f"   Output: {out}" if out else ""))
                ok += 1
            else:
                print(f"  [FAILED] {name:38s} = {got}, expected {expect}")
        except (CompileError, AsmError) as e:
            print(f"  [FAILED] {name:38s} {e}")
        except Exception as e:
            print(f"  [CRASH]  {name:38s} {type(e).__name__}: {e}")
    print(f"\n{ok}/{len(TESTS)} tests passed")
    return 0 if ok == len(TESTS) else 1


def main():
    if "--selftest" in sys.argv or len(sys.argv) == 1:
        return selftest()
    with open(sys.argv[1], encoding="utf-8") as f:
        src = f.read()
    r0, out, cpu, steps = run_c(src, dump_asm="--asm" in sys.argv)
    for line in out:
        print(line)
    print(f"--- Return value: {r0} ({steps:,} instructions)")
    if cpu.last_fault:
        print("Error:", cpu.last_fault)
    return 0


if __name__ == "__main__":
    sys.exit(main())
