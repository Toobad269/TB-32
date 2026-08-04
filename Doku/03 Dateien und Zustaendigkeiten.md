# Dateien und Zuständigkeiten

Wer macht was — damit Änderungen an der richtigen Stelle landen.

## Auf dem Mac (Python)

| Datei | Zuständig für |
|---|---|
| `pc.py` | Fenster, Tastatur, Maus, Ton, **Scrollen im Fenster**, F12-Anzeige, Zeitbudget je Bild, **Fenstergröße/Vollbild**, **Brücke zur macOS-Zwischenablage** |
| `hardware/isa.py` | **Befehlssatz, Speicherkarte, Portnummern** — einzige Wahrheit für CPU *und* Assembler |
| `hardware/cpu.py` | Die CPU. Hauptschleife hält PC und Flags in lokalen Variablen (Geschwindigkeit) |
| `hardware/bus.py` | Adressdekodierung, ROM-Schreibschutz, Portverteilung |
| `hardware/devices.py` | Grafikkarte inkl. **Blitter**, Tastatur, Platte, Timer, CMOS, Lautsprecher, Maus, **Thermal**, Netzteil |
| `hardware/machine.py` | Alles zusammenstecken, Zeitscheiben, Takt und Drosselung |
| `tools/assembler.py` | Assembler (zwei Durchgänge, Labels, Direktiven, Pseudobefehle) |
| `tools/tcc.py` | **C-Compiler auf dem Mac** — erzeugt den Kernel |
| `tools/mkfont.py` | 8×8-Zeichensatz aus handgezeichneten 5×7-Mustern |
| `tools/tbfs.py` | Dateisystem von außen (auch Ordner) |
| `tools/opstat.py` | misst die Befehlshäufigkeit — Grundlage für die Reihenfolge der Ausführungskette |
| `build.py` | Baut alles; schreibt **nur** Sektor 0 roh, der Kernel kommt als Datei `\SYSTEM\KERNEL.BIN` |

## Firmware (TB-32-Assembler)

| Datei | Inhalt |
|---|---|
| `firmware/const.inc` | Konstanten für alle Assemblerdateien |
| `firmware/bios.asm` | Reset, Interruptvektoren, POST, Bootvorgang, BIOS-Dienste, Panik-Bildschirm |
| `firmware/video.asm` | Bildschirmroutinen, **Scrollback-Ringpuffer** |
| `firmware/setup.asm` | BIOS-Setup: vier Reiter, Feldeditor für die Uhr, Secure Boot |
| `system/boot.asm` | Bootsektor, 512 Byte — **liest TBFS** und lädt `\SYSTEM\KERNEL.BIN` |
| `firmware/minimal.asm` | Das kleinste BIOS, das den Rechner startet (3324 Byte) — Vorlage für ein eigenes, siehe [[16 Eigenes BIOS schreiben]] |
| `system/start.asm` | Kernel-Einsprung, **Brücke C → BIOS**, Prozessumschalter, Systemaufruf-Eingang |

## Betriebssystem (TC)

| Datei | Inhalt |
|---|---|
| `system/kernel.c` | Befehlsinterpreter, alle Shell-Befehle, `main()` |
| `system/lib.c` | Ausgabe (**Weiche Text/Terminalfenster**), Zeichenketten, Eingabe, Bildschirmsperre, Scrollback-Ansicht |
| `system/fs.c` | TBFS: Superblock, Verzeichnis, **Ordner**, Suchpfad, **Verschieben** |
| `system/edit.c` | Texteditor im Textmodus — die Editierlogik nutzt auch der GUI-Editor |
| `system/proc.c` | Prozesse, Scheduler-Hälfte in C, `mt_enable` |
| `system/syscall.c` | Gegenseite von `INT 0x40`, Programmlader, Fortschrittsmeldung |
| `system/term.c` | Bildspeicher und Tastatur des **Terminalfensters** |
| `system/diag.c` | Anzeigetest |
| `emu/cpu.c` | TB-32-Prozessor in echtem C — alle 57 Befehle |
| `emu/machine.c` | Bus und Geräte in C: Grafik, Blitter, Platte, Timer, DMA |
| `emu/main.c` | kopfloser Start der C-Fassung zum Vergleichen |
| `tools/emu_vergleich.py` | prüft C gegen Python, Befehl für Befehl |
| `system/dialog.c` | Dateiauswahl-Fenster, von Coder, Paint und Word benutzt |
| `system/word.c` | Textverarbeitung mit Absätzen, Formaten und Wortumbruch |
| `system/coder.c` | Syntaxfarben, Zeilennummern, Suchen, Einrücken für den Editor |
| `system/paint.c` | Zeichenprogramm als Fenster im Schreibtisch |
| `system/gui.c` | **Desktop**: Fenster, Startmenü, alle Anwendungen |
| `system/font8.c` | erzeugter Zeichensatz — nicht von Hand ändern |

## Programme für den TB-32 (TC)

| Datei | Wird zu | Inhalt |
|---|---|---|
| `programs/proglib.c` | — | Bibliothek für Programme (Syscall-Verpackungen) |
| `programs/gfxlib.c` | — | **Grafik für Programme**: Blitter, Schrift, große Schrift, Knöpfe, Maus |
| `programs/prog_start.asm` | — | Startcode jedes Programms |
| `programs/cc.c` | `\SYSTEM\CC.TBX` | **C-Compiler, der sich selbst übersetzt** |
| `programs/asm.c` | `\SYSTEM\ASM.TBX` | Assembler |
| `programs/py.c` | `\SYSTEM\PY.TBX` | Python-Interpreter |
| `programs/memtest.c` | `\PROGS\MEMTEST.TBX` | Speichertest |
| `programs/bench.c` | `\PROGS\BENCH.TBX` | Leistungsmessung |
| `programs/calc.c` | `\PROGS\CALC.TBX` | **Taschenrechner**, grafisch, Festkomma |
| `programs/flappy.c` | `\PROGS\FLAPPY.TBX` | **Flappy Bird**, Physik in Sechzehnteln, mit Bildratenanzeige |
| `programs/crash.c` | nur `\SOURCE\CRASH.C` | **Stresstest und Fehlerinjektor** — Burn-in bis zur Drosselung, Farbchaos, Flackern, dazu fünf echte Abstürze. Wird **absichtlich nicht** mitgebaut: Colin übersetzt ihn auf dem TB-32 selbst (`NUR_QUELLTEXT` in `build.py`) |

## Sonstiges

- `diskfiles/` — wird beim Bauen 1:1 auf die virtuelle Platte gespiegelt
- `disk/hd0.img` — die virtuelle Festplatte (Dateien überleben Neustarts)
- `disk/cmos.bin` — die Knopfzelle; löschen setzt das BIOS-Setup zurück
- `README.md` — Doku für Colin (erzählend)
- `Doku/` — dieser Vault (Arbeitsreferenz)

Verwandt: [[00 START HIER]], [[02 Speicherkarte und Ports]]
