# Architektur TB-32

Quelle der Wahrheit: `hardware/isa.py`. CPU und Assembler lesen beide von dort.
Was die Kürzel bedeuten, steht in [[12 Abkuerzungen und Namen]].

## Grundzüge

- 32 Bit, 16 Universalregister, **jeder Befehl genau 4 Byte**, RISC-artig
- Flags Z, N, C, V; Interrupt-Freigabe als Bit 9
- Interruptvektortabelle ab Adresse 0, 256 Einträge à 4 Byte (wie beim 8086)
- 16 MB RAM, ROM ab `0x0F000000`, Reset springt dorthin

## Wie schnell die Emulation ist

Der Solltakt aus dem BIOS-Setup ist ein **Wunsch**, kein Versprechen: wie
viele Befehle wirklich durchgehen, hängt am Python-Interpreter des Wirts.
Gemessen mit `tools/messen` bzw. dem Muster unten (Stand: nach der
Optimierung, Compilerlauf als Last):

| | |
|---|---|
| Rohdurchsatz der Emulation | **~3,0 Mio Befehle/s** |
| davon im Fenster nutzbar | **~2,9 Mio/s bei 60 Bildern/s** |
| 2 MHz (Standard) und 4 MHz (Turbo) | werden voll erreicht |
| 8 MHz | wird **nicht** erreicht (~36 %) |

Höhere Taktstufen ins Setup zu schreiben brächte deshalb nichts — die Zahl
würde steigen, die Maschine nicht.

**Was den Emulator schnell macht** (alles in `hardware/cpu.py`):

1. `self.words` — eine 32-Bit-Sicht auf den Arbeitsspeicher
   (`memoryview(ram).cast("I")`). Ein Befehl ist damit **ein** Zugriff statt
   vier Bytes plus Schieben und Verodern. Krumme Adressen fallen auf den
   alten Weg zurück, damit auch ein verirrter Sprung noch stimmt
2. Die Ausführungskette steht nach **gemessener** Häufigkeit — `push` und
   `pop` sind zusammen 40 % aller Befehle. Nachmessen: `tools/opstat.py`
3. `rb`, `imm` und `simm` holt sich nur der Zweig, der sie braucht
4. `pc`, `flags`, anstehende Interrupts und die Haltepunktmenge liegen in
   **lokalen** Variablen; jeder `self.x`-Zugriff kostet in Python ein
   Vielfaches
5. Ob die CPU angehalten ist, wird nicht mehr bei jedem Befehl geprüft,
   sondern dort, wo ein Halt entstehen kann (`hlt`, `brk`, Interrupt ohne
   Handler, ungültiger Befehl)

**Und die andere Hälfte** steckt in `pc.py`: die CPU bekommt nicht mehr feste
8 ms je Bild, sondern alles, was das Zeichnen übrig lässt (gemessen und
geglättet, gedeckelt auf 14 ms). Zeichnen kostet real ~1 ms.

## Befehlsformate

```
R-Typ:  [31:24] op | [23:20] rd | [19:16] ra | [15:12] rb | Rest frei
I-Typ:  [31:24] op | [23:20] rd | [19:16] ra | [15:0]  imm16
J-Typ:  [31:24] op | [23:20] cond | [19:0] Sprungweite in Wörtern
C-Typ:  [31:24] op | [23:0] Aufrufweite in Wörtern
```

Sprungziel = Adresse des Sprungbefehls + Weite × 4.

## Befehle

| Gruppe | Befehle |
|---|---|
| Steuerung | `nop hlt cli sti iret ret brk` |
| Transfer | `mov movi movh`, Pseudo `li rd, 32bit` |
| Speicher | `ldb ldsb ldh ldw stb sth stw` — immer `[reg + off16]` |
| Rechnen | `add sub mul div mod and or xor shl shr sar not neg cmp tst udiv umod` |
| mit Konstante | `addi subi muli divi modi andi ori xori shli shri sari cmpi tsti` |
| Stack | `push pop call callr pushf popf` |
| Sprünge | `jmp` und `jz jnz jc jnc jn jbe ja jl jge jle jg`, `jmpr` |
| Ein-/Ausgabe | `in out inr outr`, `int n` |

**Wichtig:** `add`/`sub` setzen Flags, `addi`/`subi` **nicht**. Nach einer
Zählschleife also `cmpi` einsetzen, sonst springt es falsch.

## Assembler-Besonderheiten

- Lokale Label mit führendem Punkt gelten innerhalb des letzten globalen Labels
- `ldwa`/`stwa`/`ldba`/`stba` sind Pseudobefehle für absolute Adressen und
  zerstören dabei `r13` (`at`)
- Direktiven: `.org .equ .db .dh .dw .string .space .align .fill .include`
- Zwei Durchgänge; im ersten sind unbekannte Label 0

## Geschwindigkeit

Die Emulation schafft 1,5–3,5 Mio Befehle/s (abhängig vom Programm-Mix).
Der im BIOS eingestellte Takt ist deshalb ein Wunsch. Details und Folgen:
[[07 Fallstricke]], [[10 Temperatur]].

Verwandt: [[02 Speicherkarte und Ports]], [[05 Konventionen]]
