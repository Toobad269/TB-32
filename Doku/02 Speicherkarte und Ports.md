# Speicherkarte und Ports

**Die wichtigste Seite.** Fast jeder schwer zu findende Fehler im Projekt kam
von überlappenden Speicherbereichen. Vor jedem neuen Puffer hier nachsehen.

Quelle der Wahrheit: `hardware/isa.py`, `firmware/const.inc`, die `#define`s
in `system/*.c` und `programs/*.c`.

## Adressraum

| Bereich | Was | Wer |
|---|---|---|
| `0x00000000`–`0x000003FF` | Interruptvektoren, 256 × 4 Byte | BIOS |
| `0x00000400`–`0x000004FF` | BIOS-Datenbereich (BDA) | BIOS |
| `0x00007C00` | Bootsektor wird hierhin geladen | BIOS |
| `0x00008000` | Eingabezeile der Shell | Kernel |
| `0x00008200` | **Argumente für Programme** | `prog_setargs` |
| `0x00010000` | Kernel (aktuell ~157 KB, **Platz bis `0xB0000`**) | Bootloader |
| `0x0007FFF0` | BIOS-Stack | BIOS |
| `0x0009FFF0` | Kernel-Stack (= Prozess 0) | `start.asm` |
| `0x000A0000`–`0x000B0000` | Prozess-Stacks, je 8 KB | `proc.c` |
| `0x000B0000` | Sektorpuffer | `fs.c` |
| `0x000B1000` | Verzeichnis im RAM (4 KB) | `fs.c` |
| `0x000C0000` | FILEBUF, 64 KB | `fs.c` |
| `0x000D0000` | ED_BUF — Editortext, 60 KB | `edit.c`, GUI-Editor |
| `0x00100000`–`0x00114000` | Scrollback-Ringpuffer, 512 Zeilen | BIOS `video.asm` |
| `0x00114000` | Bildsicherung für den Scrollback-Betrachter | `lib.c` |
| `0x00120000` | **Terminalfenster-Puffer** 70×22×2 | `term.c` |
| `0x00124000` | Zurückblätter-Ring des Terminalfensters, 200 Zeilen | `term.c` |
| `0x00128000` | Mitschnitt der Compilerausgabe, 40 Zeilen | `term.c` (`cap_*`) |
| `0x00130000` | **Zwischenablage**, max 8 KB (`clip_len`) | `gui.c`, `pc.py` |
| `0x00200000` | **PROG_ADDR** — hierhin lädt das OS Programme | `syscall.c` |
| `0x00240000` | DATA_ADDR — globale Variablen erzeugter Programme | `cc.c` |
| `0x00280000` | SRC_BUF — Quelltext in CC/ASM/PY | Werkzeuge |
| `0x00292000` | OUT_BUF des Assemblers | `asm.c` |
| `0x00300000` | OUT_BUF des Compilers / Heap von PY | `cc.c`, `py.c` |
| `0x00380000` | STR_BUF des Compilers | `cc.c` |
| `0x003A0000` | INC_BUF (`#include`) | `cc.c` |
| `0x00400000`, `0x00500000` | FC_BUF1/2 für den Dateivergleich | `kernel.c` |
| `0x00FFFFFF` | Ende des RAM (16 MB) | |
| `0x02000000` | Textbildspeicher 80×25×2 | Grafikkarte |
| `0x02100000` | Grafikbildspeicher 640×400×1 | Grafikkarte |
| `0x0F000000` | BIOS-ROM, 64 KB, nur lesbar | |

**Regel:** Ein Programm bei `0x200000` darf höchstens bis `0x280000` wachsen
(PROG_MAX = 512 KB), sonst frisst es die Werkzeugpuffer. CC.TBX ist mit ~170 KB
der größte Brocken.

## BIOS-Datenbereich

| Adresse | Inhalt |
|---|---|
| `0x400` / `0x404` | Cursor x / y |
| `0x408` | Textattribut |
| `0x40C` | Timer-Ticks (100/s) |
| `0x414` / `0x418` | Tastaturpuffer Kopf / Ende |
| `0x420` | Tastaturpuffer, 32 × 4 Byte |
| `0x4A0` | **Speichergröße in KB** (vom POST ermittelt) |
| `0x4A4` | Plattengröße in Sektoren |
| `0x4A8` / `0x4AC` | Scrollback: Schreibzeiger / Füllstand |
| `0x4B0` | Kratzpapier für Zahlenausgabe |

## I/O-Ports

| Port | Gerät | Bedeutung |
|---|---|---|
| `0x00` / `0x01` | PIC | Interrupt bestätigen / Maske |
| `0x10` / `0x11` | Timer | Frequenz setzen / Ticks lesen |
| `0x20` / `0x21` | Tastatur | Daten / Status |
| `0x30`–`0x35` | Platte | LBA, Anzahl (**16 Bit**), Adresse, Kommando, Status, Größe |
| `0x40`–`0x43` | Grafik | Modus, Cursor, Palettenindex, Palettenwert |
| `0x44`–`0x4C` | **Blitter** | X, Y, W, H, Farbe, Kommando, Zeichen, Quelle, Hintergrund |
| `0x4D`–`0x4F` | Mauszeiger | X, Y, sichtbar |
| `0x50` / `0x51` | Lautsprecher | Frequenz / an |
| `0x52` | **Doppelpufferung** | 1 = an, 0 = aus |
| `0x53` | **Bild zeigen** | 1 = Seiten tauschen, 2 = Rückseite nach vorn kopieren |
| `0x54` | **Vergrößerung** | Faktor für Blitter-Kommando 3 (1 = normal, bis 16) |
| `0x56`–`0x5A` | **Blockkopierer (DMA)** | Quelle, Ziel, Länge, Wert, Kommando |
| `0x60`–`0x62` | Maus | X, Y, Tasten |
| `0x63` | Mausrad | Rasten seit dem letzten Lesen; **Lesen setzt zurück** |
| `0x70` / `0x71` | CMOS | Register wählen / lesen+schreiben |
| `0x80` | Debug | Zeichen ins Entwicklerlog des Mac |
| `0x90` | Netzteil | 1 = aus, 2 = Neustart |
| `0xA0`–`0xA5` | Thermik | Temperatur, Lüfter, Drosselung, Grenze, Lüftermodus, Höchstwert |
| `0xB0`–`0xB2` | BIOS-Chip | Befehl / Puffergröße / Zieladresse — den ROM neu beschreiben, siehe [[16 Eigenes BIOS schreiben]] |

Der Bildspeicher des Grafikmodus liegt ab `0x02100000`, ein Byte je Punkt.
Programme dürfen direkt hineinschreiben (`gx_punkt` in `programs/gfxlib.c`) —
für einzelne Punkte ist das schneller als ein Blitterbefehl je Punkt, weil
kein Systemaufruf dazwischen liegt.

**Ports sind nicht geschützt.** Es gibt keine Privilegstufen auf dem TB-32 —
ein Programm darf `outr`/`inr` genauso benutzen wie der Kernel. `gfxlib.c`
macht davon Gebrauch und schreibt die Blitter-Ports selbst, statt über
`int 0x40` zu gehen. In C heißen die beiden `portout(port, wert)` und
`portin(port)` — TCC findet sie in `prog_start.asm`, CC auf dem Gerät setzt
den Befehl direkt an der Aufrufstelle ein.

Merke zur Reihenfolge: `outr <Wert>, <Port>` — die **Portnummer steht in
`ra`**, also im zweiten Operanden.

Blitter-Kommandos (Port `0x49`): 1 = Fläche, 2 = Rahmen, 3 = Zeichen aus dem
Zeichensatz, 4 = Bild aus dem RAM, 5 = Bereich kopieren, **7 = Bild skaliert**
(Quellgröße im CHR-Register als `breite | höhe<<16`, Zielgröße in W und H,
Nächster-Nachbar), **6 = Zeichenkette
aus dem RAM** (Adresse im CHR-Register `0x4A`, Länge im W-Register `0x46`,
Zeichensatz bleibt in SRC). Ein Befehl statt einem je Buchstabe — eine
Editorseite sind 1600 Stück.

Puffer des Coders: Farben je sichtbarem Zeichen ab `0x00700000`.
Puffer von Word: Text ab `0x00720000`, **Farbe je Zeichen** ab `0x00728000`,
Formbytes je Absatz ab `0x00730000`, **zweites Formbyte (Listen)** ab
`0x00730400`, Bildgrößen ab `0x00730800` und `0x00731800`, Umbruchliste ab
`0x00733000` (**vier Worte je Zeile**: Anfang, Länge, Absatz, Seite),
Dateipuffer ab `0x00739000`, geladenes Bild ab `0x00750000`.

Die Maus liefert in Port `0x62` **Bit 0 links, Bit 1 Mitte, Bit 2 rechts**.
Der Schreibtisch merkt sich in `gui_taste`, welche Taste einen Klick
ausgelöst hat — daran hängt das Rechtsklick-Menü.

## Blockkopierer und Blocksuche

Der Baustein an `0x56`–`0x5A` schaufelt Speicher **am Prozessor vorbei** —
er sieht denselben Adressraum, Quelle und Ziel dürfen also auch der
Bildspeicher sein. Kommandos (Port `0x5A`):

| Cmd | Was |
|---|---|
| 1 | kopieren (Quelle → Ziel, Länge Bytes) |
| 2 | füllen (Ziel bekommt `Länge` mal den Wert) |
| 3 | **suchen**: wie viele Bytes ab Quelle sind gleich dem Wert |
| 4 | **suchen**: an welcher Stelle ab Quelle steht das erste gleiche (−1 = keins) |
| 5 | **suchen rückwärts**: wie viele Bytes vor Quelle (einschließlich) sind gleich |

Das Ergebnis der Suchbefehle steht danach im **Längenregister** (`0x58`) und
wird von dort gelesen.

Warum es das gibt: 256 KB Byte für Byte umzuschaufeln kostet den Prozessor
eine Million Befehle — eine Drittelsekunde. Kein Rückgängig, keine
Bildablage wäre damit flüssig. Mit dem Baustein sind es 0,03 ms. Die
Suchbefehle sind das Gegenstück zu den Zeichenkettenbefehlen echter
Prozessoren: das Füllwerkzeug in Paint braucht sonst je Bildpunkt einen
eigenen Lesebefehl.

Adressen der Paint-Puffer: Leinwand `0x00600008` (480×260), Rückgängig-Kopie
`0x00640000`, Warteschlange des Füllwerkzeugs `0x00680000`.

## Zwei Bildseiten

Die Karte hat zwei gleich große Bildspeicher. Angezeigt wird immer der eine,
gemalt wird immer in den anderen; Port `0x53` tauscht sie. Solange
Doppelpufferung aus ist (`0x52` = 0), sind beide dasselbe Feld und jeder
Malbefehl ist sofort sichtbar — so arbeitet der Schreibtisch.

Das ist der Unterschied zwischen „es flackert" und „es flackert nicht": ohne
zweite Seite liest der Bildschirm mit, während gemalt wird, und man sieht
halb gezeichnete Bilder. Ein Spiel schaltet also am Anfang `gx_doppelpuffer(1)`
ein, malt jedes Bild komplett neu und ruft am Ende `gx_zeigen()`.

**Zwei Betriebsarten**, und die Wahl hängt daran, ob man alles oder nur
Teile neu malt:

| Port `0x53` | Was passiert | Für wen |
|---|---|---|
| 1 | Seiten tauschen (zwei Zeiger, sofort) | Spiele — sie malen jedes Bild komplett |
| 2 | Rückseite nach vorn kopieren | **der Schreibtisch** — er malt meist nur ein Fenster neu, der Rest muss stehen bleiben |

Der Schreibtisch benutzt seit August 2026 durchgehend Betriebsart 2. Deshalb
flackert dort nichts mehr.

Ein Moduswechsel (Port `0x40`) setzt die Vergrößerung auf 1 zurück — sonst
schriebe der Schreibtisch in Riesenschrift weiter, wenn ein Programm mit
gesetztem Zoom abstürzt.

## CMOS-Register

| Reg | Inhalt |
|---|---|
| `0x00`–`0x09` | Uhrzeit und Datum (binär, nicht BCD) |
| `0x10` | Startreihenfolge |
| `0x11` | Schnellstart |
| `0x12` | Signalton beim Start |
| `0x13` | **Prozessortakt-Index** (0–4 → 0.4/1/2/4/8 MHz) |
| `0x15` | Startmeldungen ausführlich |
| `0x3F` | Schreiben darauf sichert die Knopfzelle in `disk/cmos.bin` |

Verwandt: [[01 Architektur TB-32]], [[07 Fallstricke]]
