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

## Tasten am Gehäuse

Diese Tasten gehören dem Fenster, nicht dem virtuellen Rechner — sie
funktionieren deshalb überall, auch im BIOS, wo noch gar kein
Betriebssystem läuft.

| Taste | Wirkung |
|---|---|
| `ü` | **Einschaltknopf**, wenn der Rechner aus ist. Danach fünf Sekunden Bedenkzeit mit Startbild |
| `DEL` (auf dem Mac `fn`+`⌫`) oder `F2` | ins BIOS-Setup — schon während der Bedenkzeit drückbar |
| **`Strg`+`K`** / `Cmd`+`K` | **alles kopieren, ohne Rückmeldung.** Im Textmodus der ganze Bildschirm (auch im BIOS und im Setup), im Grafikmodus der *vollständige* Text des Coders — nicht nur der sichtbare Ausschnitt |
| `Strg`+`V` / `Cmd`+`V` | Text vom Wirtsrechner einfügen |
| `Cmd`+`C` | die Auswahl aus TOOBAD-OS zurück zum Wirtsrechner |
| `Strg`+`R` | Reset — der Knopf am Gehäuse |
| `F11` / `F12` | Vollbild / Einblendung mit Takt, Temperatur und Bildrate |
| `Strg`+`Q` | beenden |

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

## Was drauf ist

**Auf der Kommandozeile** (`HELP` zeigt sie im System selbst):

| | |
|---|---|
| Dateien | `DIR` `CD` `MD` `RD` `COPY` `REN` `DEL` `TYPE` `MORE` `FC` `DUMP` |
| Laufwerk | `FORMAT` `CHKDSK` `VOL` |
| System | `VER` `MEM` `SYSTEMINFO` `TEMP` `DATE` `TIME` `CLS` `COLOR` `ECHO` |
| Prozesse | `START` `TASKLIST` `TASKKILL` |
| Oberfläche | `WIN` startet den Schreibtisch |
| Ende | `SHUTDOWN` `REBOOT` `EXIT` |

**Programme auf dem Laufwerk:**

| | |
|---|---|
| `CC` | C-Compiler — **läuft auf dem TB-32 selbst und übersetzt sich selbst** |
| `ASM` | Assembler, ebenfalls auf dem Gerät. Kann auch ein BIOS bauen |
| `PY` | kleiner Python-Interpreter |
| `CALC` | Taschenrechner |
| `FLAPPY` | Spiel, zeigt die Grafikleistung |
| `BENCH` `MEMTEST` `KELLERTEST` `CRASH` | Messen und Kaputtmachen zum Prüfen |

**Auf dem Schreibtisch** (Startmenü):

| | |
|---|---|
| **File Manager** | Dateien ansehen, umbenennen, löschen, per Maus ziehen |
| **Command Prompt** | die Kommandozeile in einem Fenster |
| **Coder** | Editor mit Syntaxfarben, Suche, Übersetzen, Starten — und **Firmware bauen** (`Test` / `Flash`) |
| **Paint** | Malen mit Werkzeugen, Füllen, Rückgängig, eigenes Format `.TBI` |
| **Word** | Textverarbeitung mit Auswahl, Farben, Listen, Seiten und **eingebetteten Bildern** |
| **System Monitor** | Prozesse, Speicher, Takt, Temperatur |
| **Control Panel** | CPU-Takt, Lüfter, POST-Einstellungen — schreibt ins CMOS |
| **Clock** | Uhr und Laufzeit |

## Doku

**Für KI-Assistenten:** [`AI_README.md`](AI_README.md) — Aufbau, jeder
Befehl, jede Oberfläche und die Fallen, an einem Ort.

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
