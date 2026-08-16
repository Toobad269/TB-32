# Eigene Betriebssysteme fuer den TB-32

Jeder Unterordner hier ist ein bootbares OS. `build.py` uebersetzt

    os/<name>/kernel.c   +   os/<name>/start.asm

zu  \SYSTEM\<NAME>.BIN  auf der Platte. Beim Start zeigt das BIOS ein
Auswahlmenue mit allen gefundenen Systemen (plus TOOBAD-OS als KERNEL.BIN).

Ein eigenes OS dazunehmen:
1. Ordner  os/meinos/  anlegen,
2. kernel.c (main() + Befehle) und start.asm (laedt bei 0x00010000) hineinlegen,
3. python3 build.py   -- fertig, es steht im Bootmenue.

Vorlage: os/tbos/  (TBOS 0.1).
