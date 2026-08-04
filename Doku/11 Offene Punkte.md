# Offene Punkte

## Als Nächstes (Stand August 2026, nach Colins Urlaub)

1. **Fenster für den C-Emulator** (SDL2 statt pygame) — Schritt 1b in
   [[15 Weg zum Raspberry Pi]]
2. **Pi mit Linux**: über SSH aufspielen, im Vollbild starten. Ohne Risiko
   für Colins Server.
3. **Bare Metal auf dem Pi 5** — über Circle, weil USB selbst zu schreiben
   die Wand ist. Details und Stolpersteine: [[15 Weg zum Raspberry Pi]]

Ausdrücklich **nicht** gewünscht: Pac-Man (Colin hat abgelehnt), GPIO-LEDs.

## Ideen, die im Gespräch aufkamen

- **Netzwerkkarte + einfacher Browser.** HTTP ist ein Textprotokoll, einfaches
  HTML ist darstellbar. HTTPS, JavaScript und CSS nicht.
- **Realistischer Speicher.** Zurzeit ist jeder Zugriff sofort fertig — kein
  Cache, keine Wartetakte. Wäre ein eigenes Stück Hardware zum Nachbauen.
- **Proportionalschrift für Word.** Bräuchte einen zweiten, selbst
  gezeichneten Zeichensatz mit Breitentabelle.
- **FPGA** — den TB-32 als echten Chip bauen.

## Naheliegend


- **Kernelgröße**: ~250 KB. Feste Kernelsektoren gibt es nicht mehr — der
  Kernel liegt als Datei im Dateisystem, die Grenze ist der freie Platz am
  Stück. Der Superblock liegt bei Sektor **512**; wer das Layout verschiebt,
  muss `fs.c`, `tools/tbfs.py`, `system/boot.asm` **und** `firmware/setup.asm`
  gemeinsam ändern.
- **Editor**: keine Suchen-Funktion, kein Rückgängig.
- **Auswahl** nur mit der Maus, nicht mit Shift+Pfeiltasten.
- **Zwischenablage** gilt nur im Editor; Terminalfenster und File Manager
  können noch nichts einfügen.
- **Symbole auf dem Schreibtisch** ordnen sich nicht selbst neu, wenn eines
  gelöscht wird — es bleibt eine Lücke, bis man sie von Hand zuzieht.

- **BIOS-Dienste**: Speichergröße, Ausstattungsliste, Warten, Piepser und vor
  allem die **Maus** fehlen als Dienst — das OS greift dafür an der Firmware
  vorbei direkt auf Ports und den BIOS-Datenbereich zu. Ganze Liste in
  [[13 BIOS-Dienste und was fehlt]]

## ~~`#include` findet nur den aktuellen Ordner~~ — erledigt

Der Suchpfad ist in Benutzung: die **Hauptdatei** wird im aktuellen Ordner
gesucht, **eingebundene Dateien** zusätzlich in `\SOURCE` (`fs_read_lib` in
`fs.c`, **Syscall 33**, `fileread_lib` in `proglib.c`).

Dass der Aufruf vorher immer −1 lieferte, lag nicht an ihm: ein `#include`
in einem Kommentar hatte den Präprozessor dazu gebracht, die Zeile mit dem
`*/` zu löschen — der offene Kommentar fraß den `if (fn == 33)`. Beide
Präprozessoren prüfen das jetzt. Siehe [[07 Fallstricke]].

## ~~Grafik ist durch die Systemaufrufe gebremst~~ — erledigt

Programme sprechen den Blitter jetzt **direkt** über die Ports an, ohne
`int 0x40`. Ports sind auf dem TB-32 nicht geschützt — das darf ein
Programm. Dazu wurde der Blitter im Emulator schneller gemacht (Zeichen
6,5×, Portzugriff 5,8×). Gemessen an Flappy: **9 → 53 fps**. Einzelheiten
im [[14 Aenderungsjournal]].

Was hier offen bleibt, falls es noch schneller werden soll:

1. **Mehrere Malbefehle in einer Liste** im Speicher, die der Blitter am
   Stück abarbeitet — spart auch die verbleibenden Portzugriffe
2. `syscall_asm` sichert 15 Register und verteilt über eine `if`-Kette.
   Eine Sprungtabelle würde jedem *anderen* Systemaufruf helfen

## Tempo (gemessen, siehe [[01 Architektur TB-32]])

Die Emulation schafft ~3,1 Mio Befehle/s; für die eingestellten 8 MHz fehlt
Faktor 2,6. Colin hat sich bewusst gegen einen C-Kern entschieden. Was in
Python bliebe:

1. **Neuere Python-Version** — das System-Python ist 3.9.6, seit 3.11 gibt es
   den spezialisierenden Interpreter. 1,4–1,8× ohne eine Zeile Code
2. **Weniger Befehle erzeugen**: `tcc.py` rechnet alles über `r0` und schiebt
   ständig auf den Stack — daher die 40 % `push`/`pop`. Ein einfacher
   Registerzuteiler wirkt wie ein schnellerer Emulator
3. **Mini-JIT**: Basisblöcke einmal zu Python-Funktionen übersetzen und
   merken. 3–8×, aber selbstmodifizierender Code (unser Compiler!) braucht
   eine Ungültigmachung

## Größer

- **Netzwerkkarte**: zwei laufende Instanzen miteinander reden lassen. Wäre
  ein sichtbares neues Kapitel (Ports + Treiber + einfaches Protokoll).
- **Sound-Chip** mit mehreren Stimmen statt nur Rechteckton.
- **Speicherschutz**: aktuell kann jedes Programm alles überschreiben. Eine
  einfache Bereichsprüfung in der CPU wäre machbar und lehrreich.
- **Python-Interpreter**: keine Wörterbücher, keine Klassen, keine
  Zeichenketten-Methoden außer `len`/`+`.

## Bewusst nicht gemacht

- Echtes CPython portieren — unmöglich, siehe README.
- Farbige Syntaxhervorhebung im Editor — kostet viel Zeichenzeit für wenig.
- Frei umbrechender Text im Terminalfenster: der Puffer der Shell bleibt
  70×22, das Fenster zeigt beim Verkleinern nur einen Ausschnitt.

Verwandt: [[00 START HIER]]
