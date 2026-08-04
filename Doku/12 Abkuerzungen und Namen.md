# Abkürzungen und Namen

Alle Namen in diesem Projekt sind selbst erfunden — sie stehen nirgends
sonst. Damit sie nicht doch irgendwann verschieden ausgelegt werden, sind
sie hier **verbindlich** festgehalten.

## Die Maschine

| Kürzel | Steht für | Was es ist |
|---|---|---|
| **TB** | **T**oo**b**ad | Das Kürzel der ganzen Familie, nach Colins Namen für seine Projekte |
| **TB-32** | Toobad, **32** Bit | Der Prozessor und damit der ganze Rechner. 32 Bit breit, 16 Register, jeder Befehl 4 Byte |
| **TOOBAD-OS** | — | Das Betriebssystem, das auf dem TB-32 läuft |
| **TOOBAD BIOS** | **B**asic **I**nput **O**utput **S**ystem | Die Firmware im ROM. Der Begriff ist von echten PCs übernommen |
| **TB-VGA** | **V**ideo **G**raphics **A**rray | Die Grafikkarte. Name ebenfalls vom echten PC übernommen |

## Dateien und Formate

| Kürzel | Steht für | Was es ist |
|---|---|---|
| **.TBX** | **TB**-32 E**x**ecutable | Ein fertiges Programm für den TB-32. Reiner Maschinencode, wird ab `0x200000` geladen und angesprungen — das Gegenstück zu `.exe` |
| **TBFS** | **TB**-32 **F**ile **S**ystem | Das Dateisystem auf der virtuellen Platte. Superblock, Verzeichnis, Daten — siehe [[02 Speicherkarte und Ports]] |
| **.C** | — | Quelltext in **TC** (siehe unten) |
| **.ASM** | **Ass**e**m**bler | Quelltext in TB-32-Assembler |
| **.PY** | **Py**thon | Skript für den eingebauten Python-Interpreter `PY.TBX` |
| **.MD** | **M**ark**d**own | Notizen und Text, so wie diese Seite hier |

## Die Werkzeuge

| Kürzel | Steht für | Was es ist |
|---|---|---|
| **TC** | **T**oobad **C** | Die Sprache: sieht aus wie C, kann aber weniger. Was genau, steht in [[04 Compiler TCC Grenzen]] |
| **TCC** | **T**oobad **C** **C**ompiler | Der Compiler **auf dem Mac** (`tools/tcc.py`). Er baut den Kernel |
| **CC** | **C** **C**ompiler | Derselbe Compiler, aber **auf dem TB-32 selbst** (`programs/cc.c` → `SYSTEM\CC.TBX`). Er kann sich selbst übersetzen, siehe [[09 Selbst-Compilierung]] |
| **ASM** | Assembler | `SYSTEM\ASM.TBX`, übersetzt `.ASM` nach `.TBX` |
| **PY** | Python | `SYSTEM\PY.TBX`, der Python-Interpreter auf dem Gerät |

Warum zwei Compiler mit fast gleichem Namen? Weil sie dieselbe Sprache
übersetzen, aber an verschiedenen Orten laufen. **TCC läuft auf dem Mac und
kennt den TB-32 nur als Ziel; CC läuft auf dem TB-32 und kennt den Mac gar
nicht.** Wer „der Compiler" sagt, muss dazusagen, welcher gemeint ist.

## Begriffe aus der Hardware

Die sind *nicht* erfunden — sie kommen aus der echten Rechnertechnik und
bedeuten hier dasselbe wie überall:

| Begriff | Steht für | Bedeutung hier |
|---|---|---|
| **POST** | **P**ower **O**n **S**elf **T**est | Der Selbsttest beim Einschalten: Speicher zählen, Platte suchen, Grafikkarte melden |
| **CMOS** | ursprünglich die Chip-Bauart | Die 64 Byte Einstellungen, die die Knopfzelle am Leben hält (`disk/cmos.bin`) |
| **IVT** | **I**nterrupt **V**ector **T**able | 256 Adressen ab Speicherstelle 0. Bei Interrupt *n* springt die CPU dorthin |
| **BDA** | **B**IOS **D**ata **A**rea | Der Kritzelblock des BIOS ab `0x400`: Tastaturpuffer, Tickzähler, Cursorposition |
| **IRQ** | **I**nterrupt **Re****q**uest | Ein Baustein meldet sich: Timer (8), Tastatur (9), Maus (12) |
| **LBA** | **L**ogical **B**lock **A**ddress | Sektoren werden schlicht durchnummeriert, 0, 1, 2 … — kein Zylinder/Kopf/Sektor wie bei alten Platten |
| **DMA** | **D**irect **M**emory **A**ccess | Die Platte schreibt selbst in den Arbeitsspeicher, ohne dass die CPU jedes Byte anfasst |
| **Blitter** | von *block image transfer* | Der 2D-Beschleuniger der Grafikkarte: ein Kommando füllt eine ganze Fläche |
| **Throttling** | Drosselung | Wird der Chip zu heiß, nimmt der Chipsatz den Takt zurück — siehe [[10 Temperatur]] |
| **Bootstrapping** | von *sich am eigenen Schnürsenkel hochziehen* | Der Compiler übersetzt sich selbst, siehe [[09 Selbst-Compilierung]] |

Verwandt: [[00 START HIER]], [[01 Architektur TB-32]]

## Später dazugekommen

| Kürzel | Bedeutung |
|---|---|
| **TBI** | *TB-32 Image* — Bildformat von Paint: Breite, Höhe, dann ein Byte je Punkt |
| **TBW** | *TB-32 Word* — Dokumentformat: Länge, Absätze, Formbytes, Bildgrößen, Farben, Text |
| **Coder** | der ausgebaute Editor: Zeilennummern, Syntaxfarben, Suchen, Einrücken |
| **`emu/`** | der Emulator in **echtem** C für den Wirtsrechner — nicht zu verwechseln mit `system/*.c` (TC für den TB-32) |
