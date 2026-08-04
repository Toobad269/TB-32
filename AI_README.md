# AI_README — alles, was ein Assistent über dieses Projekt wissen muss

Diese Datei ist für eine KI geschrieben, die einem Menschen bei TOOBAD TB-32
helfen soll. Sie enthält den Aufbau, jeden Befehl, jede Oberfläche und die
Fallen, die hier schon jemanden Stunden gekostet haben.

**Die eine Regel, aus der alles folgt:** Python emuliert **nur die Chips**.
BIOS, Betriebssystem, Fenster und alle Programme sind echter
TB-32-Maschinencode. Wer eine Funktion vermisst, baut sie in TB-32-Code —
nicht in Python. Wer das umgeht, hat das Projekt verfehlt.

---

## 1. Starten

```bash
python3 build.py     # BIOS, Kernel, Programme und Laufwerk bauen — IMMER zuerst
python3 pc.py        # den Rechner einschalten
python3 pc.py --scale 3      # größeres Fenster
python3 pc.py --turbo        # so schnell wie der Wirt kann
python3 reset.py     # Werkszustand (fragt nach; --bios nur der Chip, --ja ohne Frage)
```

Nach dem Start laufen **fünf Sekunden Bedenkzeit** mit blauem Startbild.
Erst danach bekommt die CPU Strom. Wer in dieser Zeit `DEL` drückt, landet im
BIOS-Setup — der Tastendruck wird aufgehoben und später ausgeliefert.

Ohne Fenster, für Tests und für dich als Assistent:

```bash
python3 tools/headless.py 8                       # 8 s booten, Bildschirm als Text
python3 tools/headless.py 8 --keys "DIR,ENTER"    # dabei tippen
python3 tools/headless.py 8 --after 0.5 --keys "DEL"   # ins Setup
python3 tools/screenshot.py /tmp/x.png 12         # PNG, mit --keys und --mouse
```

## 2. Tasten am Gehäuse

Sie gehören dem **Fenster**, nicht dem virtuellen Rechner — sie wirken
deshalb überall, auch im BIOS, wo noch kein Betriebssystem läuft.

| Taste | Wirkung |
|---|---|
| `ü` | Einschaltknopf, wenn der Rechner aus ist |
| `DEL` (macOS `fn`+`⌫`) oder `F2` | BIOS-Setup; schon während der Bedenkzeit drückbar |
| `Strg`+`K` | **alles kopieren, ohne Rückmeldung.** Textmodus: der ganze Bildschirm. Grafikmodus: das System liefert den Text des obersten Fensters |
| `Strg`+`V` / `Cmd`+`V` | vom Wirtsrechner einfügen |
| `Cmd`+`C` | Auswahl aus TOOBAD-OS zum Wirt |
| `Strg`+`R` | Reset (kein Neustart des Netzteils, also ohne Bedenkzeit) |
| `F11` / `F12` | Vollbild / Einblendung mit Takt, Temperatur, Bildrate |
| `Strg`+`Q` | beenden |
| `Bild↑` / `Bild↓` | im Textmodus zurückblättern |

## 3. Kommandozeile

```
DIR  CD  MD  RD  COPY  REN  DEL  TYPE  MORE  FC  DUMP
FORMAT  CHKDSK  VOL
VER  MEM  SYSTEMINFO  TEMP  DATE  TIME  CLS  COLOR  ECHO
START  TASKLIST  TASKKILL
WIN            startet den Schreibtisch
SHUTDOWN  REBOOT  EXIT
HELP           zeigt dieselbe Liste im System
```

Namen sind **maximal 15 Zeichen**, Groß-/Kleinschreibung ist beim Suchen
egal. Programme werden gesucht in: aktueller Ordner → `\SYSTEM` → `\PROGS`.

Ordner auf dem Laufwerk: `\SYSTEM` (Werkzeuge und die Systemdateien),
`\PROGS` (Programme), `\SOURCE` (Quelltexte), `\RECYCLED` (Papierkorb),
`\DESKTOP` (Symbole).

## 4. Programme

| Aufruf | was es tut |
|---|---|
| `CC quelle.c ziel.tbx` | C-Compiler **auf dem Gerät**. Übersetzt sich selbst |
| `ASM quelle.asm ziel.tbx` | Assembler auf dem Gerät. Kann `.org`, `.equ`, `.include`, Ausdrücke mit Klammern, `ldwa`/`stwa` — genug für ein **komplettes BIOS** |
| `PY datei.py` | kleiner Python-Interpreter |
| `CALC` | Taschenrechner |
| `FLAPPY` | Spiel im Grafikmodus |
| `BENCH` `MEMTEST` `KELLERTEST` | Messwerkzeuge |
| `CRASH` | löst absichtlich Fehler aus, um die Behandlung zu prüfen |

## 5. Der Schreibtisch

`WIN` startet ihn, *Exit desktop* im Startmenü führt zurück. Startmenü:
File Manager, Command Prompt, Coder, System Monitor, Control Panel, Paint,
Word, Clock, About, Exit desktop.

### Coder

Editor mit Syntaxfarben und Suche. **Die Knopfleiste richtet sich nach der
Art des Quelltextes** — erkannt an der Kennung `TBBI` im Kopf:

| Quelltext | Knöpfe |
|---|---|
| C / Assembler | `< Back  New  Save  Name  Build  Run  Find` |
| Python | dasselbe **ohne Build** — eine `.PY` wird nicht übersetzt |
| BIOS | `< Back  New  Save  Name  Find  Test  Flash` — kein Build, kein Run |

`New` fragt **zuerst** nach dem Speicherort; bricht man ab, entsteht keine
Datei. Danach speichert `Save` ohne weitere Nachfrage. Das `?` oben rechts
öffnet die Anleitung zum BIOS-Schreiben auf dem Gerät.

### Paint und Word

Paint: Werkzeuge, Strichstärke, Füllen, Rückgängig, Format `.TBI`.
Word: Auswahl, Rechtsklickmenü, Textfarben, Listen, Seitenumbruch,
eingebettete Paint-Bilder mit Größenänderung, Format `.TBW`.
Beide fragen bei `New` zuerst nach dem Speicherort.

### Dateidialog

Alle Programme benutzen dasselbe Fenster (`system/dialog.c`), gefiltert nach
Endung. `DEL` verschiebt nach `\RECYCLED`; wer **dort** löscht, löscht
endgültig.

## 6. BIOS und Firmware

Setup mit `DEL`, fünf Reiter: **Main, Hardware, Cooling, Security, Firmware**.

Der BIOS-Chip ist austauschbar. Ein Abbild hat einen **48-Byte-Kopf**:

| Position | Inhalt |
|---|---|
| `0x00` | Sprung über den Kopf |
| `0x04` | die vier Zeichen `TBBI` |
| `0x08` | Länge in Byte |
| `0x0C` | Prüfsumme (`h = 0x1234`, je Wort `h = h*31 + wort`) |
| `0x10` | Name, 32 Byte, mit Nullbyte — **das Mainboard zeigt ihn im Startbild** |
| `0x30` | ab hier Code |

Länge und Prüfsumme trägt `build.py` ein (bzw. der Coder beim `Test`/`Flash`).

**Drei Netze, drei Zeitpunkte:** die Firmware prüft vor dem Brennen, das
Mainboard prüft beim Einschalten und greift sonst zur Sicherung (Dual BIOS),
und *Restore Backup BIOS* holt ein gültiges, aber hängendes Abbild zurück.
Der **Einmal-Start** (`Test`) läuft nur für den nächsten Start; das Abbild
liegt im Board, nicht auf der Platte.

Vollständige Anleitung mit allen Diensten: `Doku/16 Eigenes BIOS schreiben`.

## 7. Architektur in Stichworten

- 32-Bit-RISC, 16 Register, **feste 4-Byte-Befehle**, 57 Opcodes
- `r0` Rückgabe und Arbeitsregister, `r1`–`r5` Argumente, `r6`–`r9` gesichert,
  `r10`–`r12` Kratzregister, `r13` Assembler-Hilfsregister, `r14` fp, `r15` sp
- BIOS-Dienste: `INT 0x10` Bildschirm (17 Funktionen), `0x13` Platte,
  `0x16` Tastatur, `0x1A` Zeit. Systemaufrufe des OS: `INT 0x40`
- Grafik 640×400, 256 Farben, Blitter mit sieben Kommandos, Blockkopierer
- Dateisystem TBFS: Superblock 512, Verzeichnis 513–520, Daten ab 576,
  **Dateien liegen am Stück**

## 8. Bauen und Prüfen

```bash
python3 tools/selftest.py       # 62 Prüfungen vom Einschalten bis zum Desktop
python3 tools/ctest.py          # Sprachtests für den Compiler
python3 tools/bootstrap.py      # der Compiler übersetzt sich selbst
python3 tools/emu_vergleich.py  # C gegen Python, Befehl für Befehl
```

Nach **jeder** Änderung an `hardware/cpu.py` oder `emu/cpu.c` gehört
`emu_vergleich.py` gelaufen. Nach Änderungen am System `selftest.py`.

## 9. Fallen — hier zuerst nachsehen, bevor du einen Fehler suchst

1. **`cmp`, `cmpi`, `tst`, `jmpr`, `callr` benutzen `rd`, nicht `ra`.**
2. **Text mitten im Code braucht `.align 4`.** Feste 4-Byte-Befehle: ohne
   Auffüllung liegt jeder folgende Befehl schief, und der Rechner stirbt
   noch vor dem ersten Bild.
3. **„Kaputt" hieß hier schon zweimal „noch nicht fertig".** Prüfe erst, ob
   die Rechnung überhaupt durch ist — ein Zwischenergebnis, das sich bei
   jeder Messung ändert, ist meistens kein Fehler.
4. **Kosten pro Bild nachrechnen.** Bei 2 MHz hat ein Bild rund 33.000
   Befehle. Eine Schleife über 3000 Byte je Neuzeichnen frisst das allein
   auf und sieht aus wie ein Hänger.
5. **Ein Leerlauf ist erst dann einer, wenn die CPU `hlt` ausführt** — sonst
   wird die Maschine heiß und drosselt sich selbst.
6. **Zeichenreihenfolge und Trefferreihenfolge sind dasselbe Wissen.** Wer
   die eine ändert, ändert die andere mit, sonst sind Fenster nicht mehr
   anklickbar.
7. **`g_button` zentriert nur, es kürzt nichts.** Beschriftungen gegen die
   Breite rechnen — `g_text_max()` kürzt, wo die Länge unbekannt ist.
8. **Ein neuer Port braucht drei Einträge:** Konstante in `isa.py`,
   Behandlung im Gerät, Registrierung in `machine.py`. Sonst tut er nichts,
   ohne jede Meldung (`m.bus.unknown_ports` verrät es).
9. **`#define NAME wert /* Kommentar */`** nahm früher den Kommentar in den
   Wert. Behoben, aber die Familie solcher Fehler bleibt.
10. **Der TBFS-Aufbau steht an vier Stellen** (`fs.c`, `tbfs.py`,
    `boot.asm`, `setup.asm`). Wer Sektornummern verschiebt, ändert alle vier.

Ausführlich mit Symptom, Ursache und Fundstelle: `Doku/07 Fallstricke`.

## 10. Wo was liegt

| | |
|---|---|
| `pc.py` | **das Gehäuse**: Monitor, Tastatur, Maus, Ton, Startbild, Bedenkzeit. Keine Logik des Rechners |
| `hardware/` | CPU, Bus, Geräte — die Chips |
| `firmware/` | BIOS und Setup in Assembler, dazu `minimal.asm` als Vorlage |
| `system/` | das Betriebssystem in C und Assembler |
| `programs/` | Programme fürs Laufwerk, inklusive Compiler und Assembler |
| `tools/` | Compiler, Assembler und Tests für den Wirtsrechner |
| `emu/` | derselbe Emulator in C, ~150× schneller, Weg zum Raspberry Pi |
| `Doku/` | **die Arbeitsreferenz** als Obsidian-Vault. Bei Unklarheit zuerst `00 START HIER`, Änderungen ins `14 Aenderungsjournal` |

**Wenn du hier mitarbeitest:** lies zuerst `Doku/00 START HIER`, halte dich
an `Doku/05 Konventionen` (Oberfläche englisch, Kommentare deutsch), und
trage jede Änderung ins `14 Aenderungsjournal` ein — mit der *Ursache*, nicht
nur dem Symptom.
