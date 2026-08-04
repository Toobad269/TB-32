# TOOBAD TB-32

> **Beta.** Das Projekt ist in Arbeit und wird ohne jede Gewähr
> veröffentlicht. Es gibt keine Zusicherung, dass es bei dir läuft, und
> keine Haftung für irgendetwas.

Ein vollständiger virtueller PC. Der Grundsatz: **Python emuliert nur die
Chips.** BIOS, Betriebssystem, Oberfläche und alle Programme sind echter
TB-32-Maschinencode, geschrieben in einer eigenen Assemblersprache und einem
eigenen C-Dialekt.

```bash
python3 build.py     # BIOS, Kernel, Programme und Laufwerk bauen
python3 pc.py        # den Rechner einschalten
```

Beim Start läuft die Bedenkzeit, dann `DEL` fürs Setup. `ü` schaltet den
Rechner wieder ein, wenn er aus ist.

## Was hier drin steckt

| | |
|---|---|
| **CPU** | TB-32 — 32 Bit, 16 Register, feste 4-Byte-Befehle, 57 Opcodes |
| **BIOS** | eigene Firmware mit Setup, Secure Boot und austauschbarem Chip |
| **OS** | TOOBAD-OS mit Dateisystem, Multitasking, Fenstern, Paint, Word, Coder |
| **Werkzeuge** | Assembler und C-Compiler — **jeweils einmal für den Mac und einmal für den TB-32 selbst** |
| **Emulator** | einmal in Python (Referenz), einmal in C (~150× schneller) |

Der Rechner baut seinen eigenen Compiler **und seine eigene Firmware** — das
Ergebnis ist Byte für Byte dasselbe wie vom Mac-Werkzeug.

## Worauf es läuft

| | |
|---|---|
| **macOS** | hier entwickelt und getestet |
| **Linux / Windows** | sollte laufen — es ist nur Python und pygame. Zwei Sachen sind aber macOS-gebunden und tun dort schlicht nichts: die Zwischenablage zum Wirtsrechner (`pbcopy`/`pbpaste`) und der Dateidialog zum Flashen des BIOS (`osascript`) |
| **Raspberry Pi ohne Linux** | **geht noch nicht.** Der C-Emulator ist der Weg dorthin und rechnet nachweislich identisch, aber Startbild, Tastatur und Bildausgabe stecken bis heute in `pc.py`. Siehe `Doku/15 Weg zum Raspberry Pi` |

Gebraucht wird Python 3 und `pygame`.

## Was das hier eigentlich ist

Ein **CPU-Emulator**, ein **eigener Befehlssatz**, ein **Assembler**, ein
**C-Compiler**, ein **BIOS** und ein **Betriebssystem mit Fenstern** — alles
von Grund auf selbst gebaut, nichts übernommen. Wer sich für *osdev*,
Retro-Computing, Compilerbau oder schlicht dafür interessiert, wie ein
Rechner unter der Oberfläche funktioniert, findet hier jede Schicht
einzeln und lesbar.

Der Rechner baut seinen eigenen Compiler und seine eigene Firmware selbst —
das nennt man Bootstrapping, und es ist der eigentliche Prüfstein.

## Doku

Die Arbeitsreferenz liegt in [`Doku/`](Doku/) als Obsidian-Vault. Anfangen
bei `00 START HIER`. Besonders lesenswert: `07 Fallstricke` (teuer erkaufte
Erkenntnisse) und `16 Eigenes BIOS schreiben`.

## Tests

```bash
python3 tools/selftest.py       # 62 Prüfungen vom Einschalten bis zum Desktop
python3 tools/ctest.py          # Sprachtests für den Compiler
python3 tools/bootstrap.py      # der Compiler übersetzt sich selbst
python3 tools/emu_vergleich.py  # C gegen Python, Befehl für Befehl
```

## Lizenz

MIT — siehe [LICENSE](LICENSE). Mach damit, was du willst.
