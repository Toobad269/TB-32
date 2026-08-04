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

| Befehl | Aufruf | was er tut |
|---|---|---|
| `DIR` | `DIR` | Inhalt des aktuellen Ordners |
| `CD` / `CHDIR` | `CD name` \| `CD ..` | Ordner wechseln |
| `MD` / `MKDIR` | `MD name` | Ordner anlegen |
| `RD` / `RMDIR` | `RD name` | leeren Ordner löschen |
| `COPY` | `COPY quelle ziel` | Datei kopieren |
| `REN` | `REN alt neu` | umbenennen |
| `DEL` / `ERASE` | `DEL name` | **in den Papierkorb** `\RECYCLED`; wer dort löscht, löscht endgültig |
| `TYPE` | `TYPE name` | Datei ausgeben |
| `MORE` | `MORE name` | seitenweise |
| `FC` | `FC a b` | zwei Dateien vergleichen |
| `DUMP` | `DUMP name` | Hexdump |
| `FORMAT` | `FORMAT` | Laufwerk neu formatieren |
| `CHKDSK` | `CHKDSK` | Dateisystem prüfen |
| `VOL` | `VOL` | Laufwerksname |
| `VER` | `VER` | Version von System und BIOS |
| `MEM` | `MEM` | Speicherbelegung |
| `SYSTEMINFO` | `SYSTEMINFO` | alles über die Maschine |
| `TEMP` | `TEMP` | Temperatur, Lüfter, Drosselung |
| `DATE` / `TIME` | | Datum / Uhrzeit |
| `CLS` | | Bildschirm löschen |
| `COLOR` | `COLOR nn` | Farbattribut setzen |
| `ECHO` | `ECHO text` | Text ausgeben |
| `START` | `START prog.tbx` | Programm starten |
| `TASKLIST` | | laufende Prozesse |
| `TASKKILL` | `TASKKILL nr` | Prozess beenden |
| `EDIT` | `EDIT name` | Editor (der Coder im Textmodus) |
| `DISPTEST` | | Bildschirmtest |
| `WIN` / `DESKTOP` | | **Schreibtisch starten** |
| `SHUTDOWN` / `REBOOT` / `EXIT` | | ausschalten / neu starten / zurück |
| `HELP` | | dieselbe Liste im System |

Ein Programm startet man auch **ohne** `START`, einfach mit seinem Namen.

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

## 7. Architektur — die vollständigen Tabellen

### Register

| | |
|---|---|
| `r0` | Rückgabewert **und Arbeitsregister des Compilers** — jeder Ausdruck landet hier |
| `r1`–`r5` | Argumente 1–5 (mehr gibt es nicht, der Compiler kann keine sechs) |
| `r6`–`r9` | muss die gerufene Funktion sichern |
| `r10`–`r12` | Kratzregister, dürfen jederzeit zerstört werden |
| `r13` (`at`) | Hilfsregister des Assemblers — nach `ldwa`/`stwa` immer futsch |
| `r14` (`fp`) | Framepointer |
| `r15` (`sp`) | Stackpointer |

**Weil der Compiler alles in `r0` rechnet, muss jeder Interrupthandler `r0`
sichern.**

### Speicherkarte

| Adresse | was |
|---|---|
| `0x00000000` | Interruptvektoren, 256 × 4 Byte |
| `0x00000400` | BIOS-Datenbereich (Cursor, Farbe, Ticks, Tastaturpuffer) |
| `0x00007C00` | hierhin lädt der Bootsektor |
| `0x00008000` | der Bootsektor liest das Verzeichnis hierhin |
| `0x00010000` | **Kernel** |
| `0x0007FFF0` | Stack der Firmware |
| `0x00100000` | Bildschirmhistorie, 512 Zeilen |
| `0x000B0000` | feste Puffer des Dateisystems — **der Kernel darf nicht bis hierher wachsen** |
| `0x000D0000` | Textpuffer des Coders, 60 KB |
| `0x00120000` | Terminalfenster |
| `0x00130000` | Zwischenablage von TOOBAD-OS |
| `0x00200000` | hierhin lädt das OS Programme |
| `0x00600008` | Leinwand von Paint |
| `0x00720000` | Text von Word |
| `0x00770000` | Fenstertext für `Strg`+`K` |
| `0x02000000` | Textbildspeicher, 80 × 25 × 2 Byte |
| `0x02100000` | Grafikbildspeicher, 640 × 400, ein Byte je Punkt |
| `0x0F000000` | **BIOS-ROM**, 64 KB, nur lesbar |

RAM: 16 MB.

### Befehlssatz — alle Opcodes

Jeder Befehl ist **genau 4 Byte**. Formate: `n` ohne Operand, `r` ein
Register, `rr`, `rrr`, `ri` Register+Konstante, `rri`, `mem` `[Basis+Offset]`,
`j` Sprung, `c` Aufruf, `i` Konstante, `ir` Port+Register.

| Opcode | Mnemonic | Format |
|---|---|---|
| `0x00` | `nop` | n |
| `0x01` | `hlt` | n |
| `0x02` | `cli` | n |
| `0x03` | `sti` | n |
| `0x04` | `iret` | n |
| `0x05` | `ret` | n |
| `0x06` | `brk` | n |
| `0x10` | `mov` | rr |
| `0x11` | `movi` | ri |
| `0x13` | `movh` | ri |
| `0x18` | `ldb` | mem |
| `0x19` | `ldsb` | mem |
| `0x1A` | `ldh` | mem |
| `0x1B` | `ldw` | mem |
| `0x1C` | `stb` | mem |
| `0x1D` | `sth` | mem |
| `0x1E` | `stw` | mem |
| `0x20` | `add` | rrr |
| `0x21` | `sub` | rrr |
| `0x22` | `mul` | rrr |
| `0x23` | `div` | rrr |
| `0x24` | `mod` | rrr |
| `0x25` | `and` | rrr |
| `0x26` | `or` | rrr |
| `0x27` | `xor` | rrr |
| `0x28` | `shl` | rrr |
| `0x29` | `shr` | rrr |
| `0x2A` | `sar` | rrr |
| `0x2B` | `not` | rr |
| `0x2C` | `neg` | rr |
| `0x2D` | `cmp` | rr |
| `0x2E` | `tst` | rr |
| `0x2F` | `udiv` | rrr |
| `0x30` | `addi` | rri |
| `0x31` | `subi` | rri |
| `0x32` | `muli` | rri |
| `0x33` | `divi` | rri |
| `0x34` | `modi` | rri |
| `0x35` | `andi` | rri |
| `0x36` | `ori` | rri |
| `0x37` | `xori` | rri |
| `0x38` | `shli` | rri |
| `0x39` | `shri` | rri |
| `0x3A` | `sari` | rri |
| `0x3D` | `cmpi` | ri |
| `0x3E` | `tsti` | ri |
| `0x3F` | `umod` | rrr |
| `0x40` | `push` | r |
| `0x41` | `pop` | r |
| `0x42` | `call` | c |
| `0x43` | `callr` | r |
| `0x44` | `pushf` | n |
| `0x45` | `popf` | n |
| `0x50` | `ja` `jae` `jb` `jbe` `jc` `jeq` `jg` `jge` `jl` `jle` `jmp` `jn` `jnc` `jne` `jnn` `jnv` `jnz` `jv` `jz` | j |
| `0x51` | `jmpr` | r |
| `0x60` | `in` | ri |
| `0x61` | `inr` | rr |
| `0x62` | `out` | ir |
| `0x63` | `outr` | rr |
| `0x64` | `int` | i |

**Kodierung:** `r`-Formate `(op<<24)|(rd<<20)|(ra<<16)|(rb<<12)`,
`i`-Formate `(op<<24)|(rd<<20)|(ra<<16)|(imm&0xFFFF)`,
Sprünge `(op<<24)|(cond<<20)|(off&0xFFFFF)` mit `off = (ziel-pc)/4`.

**Bedingungen** für `0x50`: `al`=0 `z`/`eq`=1 `nz`/`ne`=2 `c`/`b`=3
`nc`/`ae`=4 `n`=5 `nn`=6 `v`=7 `nv`=8 `be`=9 `a`=10 `l`=11 `ge`=12 `le`=13
`g`=14.

**Falle:** `cmp`, `cmpi`, `tst`, `tsti`, `jmpr`, `callr` benutzen **`rd`**,
nicht `ra`. Wer das verwechselt, baut einen Emulator, der fast richtig ist.

**Pseudo-Befehle** des Assemblers: `li rd, wert32` (wird `movi`+`movh`),
`ldwa/ldha/ldba/stwa/stha/stba rd, ADRESSE` (wird `li at, ADRESSE` plus
Zugriff über `at`).

**Direktiven:** `.org` `.equ` `.include` `.db` `.dw` `.space` `.align`.
Ausdrücke können `+ - * /` und Klammern, Punkt vor Strich.

### Ports

| Port | wofür |
|---|---|
| `0x00`/`0x01` | Interruptcontroller: quittieren / Maske |
| `0x10`/`0x11` | Timer: Frequenz setzen / Ticks lesen |
| `0x20`/`0x21` | Tastatur: Zeichen holen / liegt eins bereit |
| `0x30`–`0x35` | Platte: LBA, Anzahl, Adresse, Befehl (1 lesen, 2 schreiben), Status, Größe |
| `0x40`–`0x43` | Grafikkarte: Modus (0 Text, 1 Grafik), Cursor, Palette |
| `0x44`–`0x4C` | **Blitter**: x, y, w, h, Farbe, Kommando, Zeichen, Quelle, Hintergrund |
| `0x4D`–`0x4F` | Hardware-Mauszeiger: x, y, an |
| `0x50`/`0x51` | Lautsprecher: Frequenz / an |
| `0x52`/`0x53` | Doppelpufferung an / Bild sichtbar machen |
| `0x54` | Zoom für Blitter-Kommando 3 |
| `0x56`–`0x5A` | **Blockkopierer**: Quelle, Ziel, Länge, Füllbyte, Kommando |
| `0x60`–`0x63` | Maus: x, y, Tasten (Bit 0 links, 1 Mitte, **2 rechts**), Rad |
| `0x70`/`0x71` | CMOS: Adresse / Wert |
| `0x80` | Entwickler-Log |
| `0x90` | Netzteil: 1 aus, 2 Neustart |
| `0xA0`–`0xA5` | Temperatur, Lüfter, Drosselung, Grenze, Lüftermodus, Höchstwert |
| `0xB0`–`0xB2` | **BIOS-Chip**: Befehl, Puffergröße, Adresse |

**Blitter-Kommandos** (Port `0x49`): 1 Fläche, 2 Rahmen, 3 Zeichen,
4 Bild, 5 kopieren, 6 Zeichenkette, 7 Bild skaliert.
**Blockkopierer** (Port `0x5A`): 1 kopieren, 2 füllen, 3/4/5 suchen.
**BIOS-Chip** (Port `0xB0`): 1 Datei vom Wirt holen, 2 Puffer in den RAM,
3 brennen, 4 Sicherung zurück, 5 Puffer aus dem RAM, 6 für einen Start
anmelden, 7 abmelden, 8 dauerhaft anmelden, 9 liegt ein Wunsch an.

**Ein neuer Port braucht drei Einträge:** Konstante in `hardware/isa.py`,
Behandlung im Gerät, Registrierung in `hardware/machine.py`. Fehlt der
dritte, tut er nichts — ohne jede Meldung. `m.bus.unknown_ports` verrät es.

### BIOS-Dienste

Funktionsnummer in `r0`, Argumente ab `r1`, Ergebnis in `r0`.

**`INT 0x10` Bildschirm** — die Reihenfolge ist Pflicht:

| r0 | Name | Argumente |
|---|---|---|
| 0 | putc | r1 Zeichen, r2 Attribut |
| 1 | puts | r1 Zeiger, r2 Attribut |
| 2 | setcursor | r1 x, r2 y |
| 3 | clear | r1 Attribut |
| 4 | getcursor | → `y<<16 \| x` |
| 5 | putat | r1 x, r2 y, r3 Zeichen, r4 Attribut |
| 6 | putn | r1 Zahl, r2 Attribut |
| 7 | puthex | r1 Wert, r2 Attribut, **r3 Stellen** |
| 8 | setmode | r1 = 0 Text, 1 Grafik |
| 9 | box | r1 x, r2 y, r3 w, r4 h, r5 Attribut |
| 10 | fillrect | dito |
| 11 | hline | r1 x, r2 y, r3 Länge, r4 Zeichen, r5 Attribut |
| 12 | scroll | — |
| 13 | clearrow | r1 y, r2 Attribut |
| 14 | putsat | r1 x, r2 y, r3 Text, r4 Attribut |
| 15/16 | sbcount / sbline | Bildschirmhistorie |

`putc` **muss die Steuerzeichen 8, 9, 10 und 13 selbst behandeln.** Fehlt
die 8, druckt die Rücktaste ein Kästchen statt zu löschen.

**`INT 0x13` Platte:** 0 lesen (r1 Sektor, r2 Anzahl, r3 Adresse → r0
Status), 1 schreiben, 2 Größe.
**`INT 0x16` Tastatur:** 0 warten (→ `Scancode<<8 \| ASCII`), 1 nachsehen,
2 leeren. Ins Warten gehört ein `hlt`.
**`INT 0x1A` Zeit:** 0 Ticks (100/s), 1 Uhrzeit `h<<16\|m<<8\|s`,
2 Datum `j<<16\|m<<8\|t`.

### Systemaufrufe des OS — `INT 0x40`

Nummer in `r0`, Argumente `r1`–`r4`.

| Nr | | Nr | |
|---|---|---|---|
| 0 | putc | 17 | setmode |
| 1 | puts | 18 | out(port, wert) |
| 2 | getkey | 19 | in(port) |
| 3 | cls | 20 | box |
| 4 | exit | 21 | hline |
| 5 | ticks | 22 | memkb |
| 6 | putn | 23 | flushkeys |
| 7 | setcursor | 24–27 | Verzeichnis abfragen |
| 8 | putat | 28 | Fortschritt melden (0–100) |
| 9 | haskey | 29 | Statustext melden |
| 10 | fileread | 30 | Adresse des Zeichensatzes |
| 11 | filewrite | 31 | Fläche/Rahmen malen |
| 12 | clock | 32 | Zeichen malen |
| 13 | date | 33 | fileread mit Suchpfad |
| 14 | sleep | | |
| 15 | beep | | |
| 16 | disksize | | |

`INT 0x41` gibt die Rechenzeit freiwillig ab.

### Dateisystem TBFS

| | |
|---|---|
| Superblock | Sektor 512, Kennung `TBFS` = `0x54424653` |
| Verzeichnis | Sektoren 513–520, 128 Einträge à 32 Byte |
| Daten | ab Sektor 576 |
| Eintrag | Name 16 Byte, Start `+16`, Größe `+20`, Info `+24`, Zeit `+28` |
| Info | Art im untersten Byte (1 Datei, 2 Ordner), **Elternordner+1** in Bit 16–31 |

**Dateien liegen am Stück.** Nur deshalb passt ein Lader in 512 Byte.
Der Aufbau steht an **vier** Stellen: `system/fs.c`, `tools/tbfs.py`,
`system/boot.asm`, `firmware/setup.asm` — wer eine ändert, ändert alle.

Eigene Formate: `.TBX` Programm (lädt nach `0x200000`), `.TBI` Bild
(Breite, Höhe, dann ein Byte je Punkt), `.TBW` Word-Dokument.

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

---

# 11. Die Oberfläche, Fenster für Fenster

Dieser Abschnitt beschreibt jedes Fenster so genau, dass du einem Nutzer
sagen kannst, wo er klicken muss, ohne selbst hinzusehen.

Grundmaße: Bildschirm **640 × 400**, Zeichen **8 × 8** im Grafikmodus.
Aufgabenleiste ab **y = 378**. Titelleiste jedes Fensters **14 Punkte** hoch.

## 11.1 Der Schreibtisch

**Aufgabenleiste unten.** Ganz links der Knopf **Start** (x 2, Breite 52).
Rechts daneben je ein Knopf pro offenem Fenster (Breite 64, ab x 90) — ein
Klick holt es nach vorn. Rechts außen die **Uhr**.

**Startmenü** (Klick auf *Start*, Einträge 14 Punkte hoch, ab y 262):

| # | Eintrag | öffnet |
|---|---|---|
| 0 | File Manager | Dateiverwaltung |
| 1 | Command Prompt | Kommandozeile im Fenster |
| 2 | Coder | Editor |
| 3 | System Monitor | Prozesse und Messwerte |
| 4 | Control Panel | Einstellungen |
| 5 | Paint | Malprogramm |
| 6 | Word | Textverarbeitung |
| 7 | Clock | Uhr |
| 8 | About TOOBAD-OS | Systeminfo |
| 9 | Exit desktop | zurück zur Kommandozeile |

**Symbole auf dem Schreibtisch.** Dateien aus `\DESKTOP` liegen als Symbole
da und lassen sich mit der Maus verschieben; die Positionen merkt sich
`ICONS.DAT`. Doppelklick startet oder öffnet.

**Fensterrahmen.** In der Titelleiste rechts: **Vollbild** (x = Breite−30,
12 × 11) und **Schließen** (x = Breite−16). Unten rechts ein 12 × 12 großer
Anfasser zum Vergrößern. Ziehen an der Titelleiste verschiebt.

**Reihenfolge:** gemalt wird nach Fensternummer, `win_top` zuletzt — wer die
höhere Nummer hat, liegt vorn. Die Klicksuche läuft **rückwärts**, damit sie
zur Malreihenfolge passt.

## 11.2 File Manager

Spalten **Name / Size / Type**. Ein Klick wählt, Doppelklick öffnet: Ordner
wechselt hinein, `.TBX` startet, Textdateien gehen in den Coder, `.TBI` nach
Paint, `.TBW` nach Word. Der Knopf **Up** rechts oben geht eine Ebene hoch.
Dateien lassen sich per Maus auf den Schreibtisch ziehen.
Blättern mit `Bild↑` / `Bild↓`, löschen mit `Entf` (in den Papierkorb).

## 11.3 Command Prompt

Die Kommandozeile in einem Fenster, **70 × 22** Zeichen. Alles aus Abschnitt
3 funktioniert hier. Eigene Historie mit `Bild↑` / `Bild↓`. Ausgaben von
Programmen, die man aus dem Coder startet, landen ebenfalls hier.

## 11.4 Coder

**Kopfzeile:** `File: NAME  in PFAD  Ln n  Col n  Bytes n`. Steht rechts eine
Meldung (`saved`, `built`, `errors`, `building ...`, `not found`), weicht die
Byte-Zahl — beide teilen sich den Platz. Ganz rechts das **`?`** (20 × 14),
das die BIOS-Anleitung öffnet.

**Knopfleiste** — sie richtet sich nach der Art des Quelltextes, erkannt an
der Kennung `TBBI` im Kopf. Die Knöpfe rücken zusammen, wenn einer fehlt:

| Art | Knöpfe (von links) |
|---|---|
| C / Assembler | `< Back` `New` `Save` `Name` `Build` `Run` `Find` + Suchfeld |
| Python | dasselbe **ohne** `Build` |
| BIOS | `< Back` `New` `Save` `Name` `Find` + Suchfeld + `Test` `Flash` |

Breiten: Back 50, New 38, Save 44, Name 46, Build 50, Run 40, Find 40,
Suchfeld 100, Test 52, Flash 56 — je 4 Punkte Abstand.

| Knopf | was er tut |
|---|---|
| `< Back` | zurück zur Startseite mit Dateiliste und Vorlagen |
| `New` | fragt **zuerst** nach dem Speicherort. Abbrechen legt nichts an |
| `Save` | speichert ohne Nachfrage, sobald der Platz feststeht |
| `Name` | Dateinamen im Kopf bearbeiten |
| `Build` | übersetzt: `.ASM` mit `ASM.TBX`, sonst mit `CC.TBX`. Fortschrittsfenster, bei Fehlern wird es zum Meldungsfenster (520 × 240) und `ENTER` springt in die erste Fehlerzeile |
| `Run` | speichert, übersetzt bei Bedarf, startet — Ausgabe im Terminalfenster |
| `Find` | Suchfeld; `ENTER` springt zum nächsten Treffer, `not found` erscheint rechts |
| `Test` | baut ein BIOS, prüft es, fragt einmal — und startet den Rechner **einmal** damit |
| `Flash` | dasselbe dauerhaft; danach fragt die **Firmware** in Rot ein zweites Mal |

**Startseite** (nach `< Back` oder beim ersten Öffnen): links die Vorlagen
**C program .C**, **Assembler .ASM**, **Python script .PY**, **BIOS .ASM** —
rechts die Dateiliste des aktuellen Ordners mit `Up`-Knopf.

**Im Text:** Syntaxfarben je nach Sprache, Auswahl mit der Maus, Rollen mit
`Bild↑`/`Bild↓`, `Pos1`/`Ende`, `Strg`+`A/C/X/V`.

## 11.5 Paint

**Werkzeuge** (zwei Spalten, je 24 × 20 Punkte):

| | | | |
|---|---|---|---|
| `Pen` Stift | `Era` Radierer | `Lin` Linie | `Box` Rechteck |
| `Bx*` gefüllt | `Cir` Kreis | `Fil` Füllen | `Get` Pipette |

Darunter **Size** mit 1, 2, 4 Punkten Strichstärke, dann die **Farbpalette**,
dann die Knöpfe **New**, **Undo**, **Save**, **Open** (je 47 × 14, untereinander).

`New` fragt zuerst nach dem Speicherort. `Undo` macht einen Schritt
rückgängig. Format `.TBI`: Breite und Höhe als Wort, dann ein Byte je Punkt.
Beim Ziehen von Linie, Rechteck und Kreis erscheint eine Vorschau, die
**auf die Leinwand begrenzt** ist.

## 11.6 Word

**Knopfleiste oben:** `B` fett, `U` unterstrichen, `A` Schriftfarbe,
`A+`/`A*` Größe, `1.` nummerierte Liste, `*` Aufzählung, `<`/`>` Einzug,
`><` Umbruch, dann `New`, `Save`, `Open`.

**Rechtsklick** öffnet ein Menü mit 14 Einträgen (166 Punkte breit, Zeilen
14 hoch):

| | |
|---|---|
| Black, Red, Green, Blue, Orange, Grey | Textfarbe der Auswahl |
| Copy `^C`, Cut `^X`, Paste `^V` | Zwischenablage |
| Select all, Deselect | Auswahl |
| Insert picture | Paint-Bild einfügen (Dateidialog, nur `.TBI`) |
| Delete picture | angeklicktes Bild samt Absatz löschen |
| Save as text | als reine Textdatei ausgeben |

**Tasten:** Pfeile, `Pos1`/`Ende`, `Bild↑`/`Bild↓`, `Entf`, `Rücktaste`,
`ENTER`. Ein Bild ist ein ganzer Absatz — angeklickt löschen `Entf` oder
`Rücktaste` es vollständig. Seitenumbruch alle **620 Punkte** Höhe, die
Seitenzahl steht am Rand. Format `.TBW`.

## 11.7 System Monitor

Zeigt die Prozesstabelle (Nummer, Zustand, Name), Speicherbelegung, den
eingestellten Takt, die gemessene Temperatur, die Lüfterdrehzahl und die
Drosselung. Frischt sich einmal je Sekunde auf — **ohne selbst zu malen**,
er fordert ein normales Neuzeichnen an, damit er nicht über andere Fenster
schreibt.

## 11.8 Control Panel

Fünf Zeilen, ein Klick auf eine Zeile ändert den Wert (er läuft im Kreis):

| Zeile | Werte |
|---|---|
| CPU Clock Speed | 0.4 / 1 / 2 / 4 / 8 MHz |
| POST Beep | an / aus |
| Quick Boot | an / aus — **überspringt die Pausen im Selbsttest** |
| POST Messages | kurz / ausführlich |
| Fan Control | automatisch / leise / volle Drehzahl |

Darunter der Knopf **Save to CMOS** (96 × 16) — erst er macht die
Einstellungen dauerhaft. Rechts daneben die aktuelle Temperatur.

## 11.9 Clock, About

**Clock:** Uhrzeit, Datum und Betriebszeit. **About:** Systemname, Version,
CPU, Speicher, Grafikkarte.

## 11.10 Dateidialog

Ein Fenster für alle Programme (`system/dialog.c`), 380 × 250.

Oben links **Save as** / **Open** / **Picture**, daneben der aktuelle Pfad,
rechts der Knopf **Up**. Darunter die Liste (Ordner mit `DIR`, Dateien mit
Größe), gefiltert nach Endung — Paint sieht nur `.TBI`, Word nur `.TBW`,
*Insert picture* nur Bilder. **Ordner werden immer gezeigt**, sonst käme man
nicht hin.

Unten das Feld **Name:** und rechts **OK** (44 × 18) und **Cancel** (56 × 18).

**Tasten:** `↑`/`↓` wählen, `ENTER` bestätigt, `ESC` bricht ab,
`Rücktaste` löscht im Namensfeld. Ein Klick auf einen Ordner geht hinein,
ein Klick auf eine Datei ist beim Öffnen schon die Antwort.

## 11.11 Die Firmware-Fenster im Coder

**Rückfrage** (420 × 150): Name, Größe und Prüfsumme des Abbildes, darunter
drei Zeilen Erklärung — beim Testen „läuft einmal, der Chip bleibt", beim
Flashen „die Firmware fragt gleich noch einmal in Rot". Knöpfe
**Test once** / **Continue** und **Cancel**.

**Hilfe** (`?`, 460 × 300): 33 Zeilen über den Kopf, die Interruptvektoren,
alle Bildschirmfunktionen, die Steuerzeichen und das Laden des Bootsektors.
`Bild↑`/`Bild↓` blättert, `ESC` schließt.

## 11.12 BIOS-Setup

`DEL` oder `F2`. Fünf Reiter, mit `←`/`→` gewechselt:

| Reiter | Inhalt |
|---|---|
| **Main** | Uhrzeit, Datum, Quick Boot, POST-Piepser, POST-Meldungen, Standardwerte laden |
| **Hardware** | CPU-Takt, Startgerät, Speicher, Platte, Grafikkarte |
| **Cooling** | Lüftermodus, Drosselgrenze, Temperatur, Lüfter, Drosselung, Höchstwert |
| **Security** | Secure Boot, Prüfsumme, *Trust Current Boot Image* |
| **Firmware** | Größe und Prüfsumme des Chips, *Flash BIOS from File*, *Restore Backup BIOS* |

**Tasten:** `↑`/`↓` Zeile, `←`/`→` Reiter, `ENTER` oder `+`/`−` ändern,
`F5` Standardwerte, `F10` sichern und raus, `ESC` verwerfen und raus.
