# Eigenes BIOS schreiben

Das BIOS des TB-32 ist austauschbar. Man kann ein eigenes schreiben, es
flashen, und der Rechner startet damit — oder eben nicht mehr, wenn es
falsch ist.

Diese Seite ist der **Vertrag**: alles, was ein BIOS liefern muss, damit
TOOBAD-OS darauf läuft. Wer sich daran hält, hat ein funktionierendes BIOS,
egal wie es innen aussieht.

Fertige Vorlage zum Umbauen: **`firmware/minimal.asm`** (3324 Byte, kann
nichts außer starten — und genau das reicht).

---

## Der Kopf: die ersten 48 Byte

Ohne ihn nimmt das Mainboard das Abbild gar nicht erst an.

| Position | Inhalt |
|---|---|
| `0x00` | ein Sprung über den Kopf (`jmp start`) — hier landet die CPU nach dem Einschalten |
| `0x04` | die vier Zeichen `TBBI` |
| `0x08` | Länge des Abbildes in Byte |
| `0x0C` | Prüfsumme |
| `0x10` | **Name, 32 Byte, mit Nullbyte abgeschlossen** |
| `0x30` | ab hier der Code |

```asm
.org ROM_BASE
reset:
    jmp start
    .db "TBBI"
    .dw 0                  ; Laenge     -- build.py traegt sie ein
    .dw 0                  ; Pruefsumme -- build.py traegt sie ein
    .db "MEIN BIOS", 0     ; 0x10 -- der Name im Startbild
    .space 22              ;         auf genau 32 Byte auffuellen
start:                     ; 0x30
    li sp, BIOS_STACK
```

**Der Name ist kein Schmuck.** Das Mainboard liest ihn und schreibt ihn beim
Einschalten in die Bildmitte — *bevor* die CPU überhaupt läuft. Deshalb sieht
das Startbild bei jedem BIOS gleich aus, und trotzdem steht dort der eigene
Name. Fehlt das Feld (ältere Abbilder), zeigt das Board `UNNAMED BIOS`.

**Länge und Prüfsumme trägt `build.py` ein**, nicht der Assembler — beide
hängen vom fertigen Abbild ab. Die Rechnung steht in `bios_kopf_stempeln`:
Länge auf vier aufrunden, das Prüfsummenfeld auf null setzen, dann

```
summe = 0x1234
für jedes 32-Bit-Wort:  summe = summe * 31 + wort
```

Geprüft wird **an drei Stellen zu drei Zeitpunkten**:

1. **Beim Flashen** — die Firmware lehnt ein Abbild ohne Kennung oder mit
   falscher Prüfsumme ab und schreibt gar nicht erst (`bios_pruefen` in
   `firmware/setup.asm`)
2. **Beim Einschalten** — das Mainboard prüft nach und greift sonst zur
   Sicherung (`Machine.rom_pruefen` in `hardware/machine.py`)
3. **Secure Boot**, falls eingeschaltet — dann muss auch noch die gemerkte
   Summe stimmen, siehe [[13 BIOS-Dienste und was fehlt]]

Punkt 2 ist der wichtige: **eine kaputte Firmware kann sich nicht selbst
prüfen.** Deshalb sitzt diese Prüfung im Board.

---

## Was beim Einschalten passiert

Die CPU beginnt bei `ROM_BASE` = `0x0F000000`. Ab da ist alles unsere Sache:

1. **Stack setzen** (`BIOS_STACK` = `0x0007FFF0`)
2. **BIOS-Datenbereich leeren** (ab `0x400`) — dort liegen Cursorposition,
   Farbe, Tickzähler und der Tastaturpuffer
3. **Interrupttabelle füllen** (ab Adresse 0, je Vektor 4 Byte). Ohne diese
   Einträge springt jeder Interrupt nach Adresse 0
4. **Timer anwerfen**: `out P_TIMER_HZ, 100`
5. **Interrupts freigeben** (`sti`)
6. **Sektor 0 nach `0x7C00` laden**, die Signatur `55 AA` an Position 510
   prüfen und hineinspringen

Was das BIOS **nicht** mehr selbst machen muss: das Startbild und die
Bedenkzeit. Beides gehört dem Board (siehe unten). Ein BIOS, das die
DEL-Taste auswerten will, findet sie nach dem Start ganz normal im
Tastaturpuffer.

Alles andere — Startbild, Speichertest, Setup, Secure Boot — ist Kür. Der
Rechner läuft auch ohne.

---

## Die Interruptvektoren

| Vektor | wofür | Pflicht? |
|---|---|---|
| `0x08` | Timer-IRQ | ja — sonst zählt keine Uhr und `hlt` weckt nie auf |
| `0x09` | Tastatur-IRQ | ja |
| `0x10` | Dienst Bildschirm | ja |
| `0x13` | Dienst Festplatte | ja |
| `0x16` | Dienst Tastatur | ja |
| `0x1A` | Dienst Zeit | ja |
| `0x00` | Division durch null | nein (dann stürzt der Rechner unschön ab) |
| `0x06` | unbekannter Befehl | nein |

Beide Hardware-Interrupts müssen dem Controller quittieren
(`out P_PIC_ACK, …`), sonst kommt nie wieder einer.

**Und der Tastatur-Handler muss ALLE wartenden Tasten holen**, nicht nur
eine — der Controller kennt je Quelle nur ein Bit. Wer nach der ersten
aufhört, hinkt bei schnellem Tippen dauerhaft einen Anschlag hinterher.
Das war ein echter Fehler, siehe [[07 Fallstricke]].

---

## Die vier Dienste

Aufgerufen mit `int <nummer>`, Funktionsnummer in `r0`, Argumente ab `r1`,
Ergebnis in `r0`. Das Gegenstück auf der Systemseite steht in
`system/start.asm` — dort sieht man jeden Aufruf.

### INT 0x10 — Bildschirm

Die **Reihenfolge ist Pflicht**, das System ruft über die Nummer auf.

| r0 | Name | Argumente |
|---|---|---|
| 0 | putc | r1 Zeichen, r2 Attribut |
| 1 | puts | r1 Zeiger auf 0-terminierten Text, r2 Attribut |
| 2 | setcursor | r1 x, r2 y |
| 3 | clear | r1 Attribut |
| 4 | getcursor | — → r0 = `y<<16 \| x` |
| 5 | putat | r1 x, r2 y, r3 Zeichen, r4 Attribut |
| 6 | putn | r1 Zahl, r2 Attribut (dezimal, ohne Vorzeichen) |
| 7 | puthex | r1 Wert, r2 Attribut, **r3 Stellen** |
| 8 | setmode | r1 = 0 Text, 1 Grafik |
| 9 | box | r1 x, r2 y, r3 Breite, r4 Höhe, r5 Attribut |
| 10 | fillrect | r1 x, r2 y, r3 Breite, r4 Höhe, r5 Attribut |
| 11 | hline | r1 x, r2 y, r3 Länge, r4 Zeichen, r5 Attribut |
| 12 | scroll | — (alles eine Zeile hoch) |
| 13 | clearrow | r1 y, r2 Attribut |
| 14 | putsat | r1 x, r2 y, r3 Text, r4 Attribut |
| 15 | sbcount | — → r0 = Zeilen in der Bildschirmhistorie |
| 16 | sbline | r1 Zeilennummer, r2 Zieladresse |

**15 und 16 darf man weglassen** — dann geben sie einfach 0 zurück, und die
Historie im Terminal ist leer. So macht es `minimal.asm`. Was man nicht
weglassen darf, ist der Eintrag selbst: fehlt er in der Tabelle, springt
das System ins Leere.

**putc muss Steuerzeichen kennen.** Das ist die Anforderung, die man beim
Abschreiben am ehesten übersieht — sie steht in keiner Funktionsnummer:

| Code | | was putc tun muss |
|---|---|---|
| 10 | `\n` | Zeilenumbruch, am unteren Rand scrollen |
| **8** | **Rücktaste** | **Cursor eins zurück und das Zeichen dort löschen** |
| 13 | `\r` | Cursor an den Zeilenanfang |
| 9 | Tab | auf das nächste Vielfache von 8 |

Fehlt die **8**, legt das BIOS das Byte 8 als ganz normales Zeichen in den
Bildspeicher — und CP437 stellt 8 als „◘" dar. Die Eingabezeile sammelt dann
bei jedem Druck auf die Rücktaste ein Kästchen ein, statt zu löschen.
Verwirrend daran: der Text *im Speicher* stimmt trotzdem, nur der Bildschirm
lügt. Genau das ist Colin mit seinem ersten eigenen BIOS passiert.

Das System schickt die 8 an zwei Stellen: `readline` in `system/lib.c` und
die Eingabe des Python-Interpreters in `programs/py.c`.

`r3 Stellen` bei **puthex** ist eine echte Falle. Wer es vergisst, druckt
den Wert hunderte Male und füllt den ganzen Bildschirm — genau das ist beim
Bau des Firmware-Reiters passiert.

### INT 0x13 — Festplatte

| r0 | Name | Argumente | Ergebnis |
|---|---|---|---|
| 0 | lesen | r1 Sektor (LBA), r2 Anzahl, r3 Zieladresse | r0 = Status, 0 = gut |
| 1 | schreiben | dito | r0 = Status |
| 2 | Größe | — | r0 = Sektoren |

### INT 0x16 — Tastatur

| r0 | Name | Ergebnis |
|---|---|---|
| 0 | warten | r0 = `Scancode<<8 \| ASCII` |
| 1 | nachsehen | 0 oder der Code — die Taste bleibt im Puffer |
| 2 | Puffer leeren | — |

Beim Warten gehört ein **`hlt`** in die Schleife. Ohne dreht die CPU im
Leerlauf mit voller Last, wird heiß und drosselt sich selbst — auch das ist
schon passiert, siehe [[10 Temperatur]].

### INT 0x1A — Zeit

| r0 | Name | Ergebnis |
|---|---|---|
| 0 | Ticks seit dem Start | r0 (100 je Sekunde) |
| 1 | Uhrzeit | `h<<16 \| m<<8 \| s` |
| 2 | Datum | `j<<16 \| m<<8 \| t` |

---

## Was das BIOS **nicht** machen muss

Fast alles. Das System redet mit der Hardware weitgehend selbst — Grafik,
Blitter, Maus, Lautsprecher, Blockkopierer und Temperatur gehen über
`inr`/`outr` direkt an die Ports, ganz ohne BIOS. Die Portliste steht in
[[02 Speicherkarte und Ports]].

Deshalb ist ein brauchbares BIOS hier so klein: **3324 Byte** gegen 12216
beim vollen.

---

## Ein BIOS auf dem Gerät selbst schreiben

Seit dem Ausbau des Assemblers kann der TB-32 **seine eigene Firmware
bauen** — nachgemessen: das Ergebnis ist Byte für Byte dasselbe wie vom Mac,
bis auf Länge und Prüfsumme im Kopf, die der Coder selbst einträgt.

**Coder → New → BIOS** legt eine Quelle mit fertiger Vorlage an. Der
`?`-Knopf oben rechts öffnet die Kurzfassung dieser Seite auf dem Gerät.

Unten zwei Knöpfe:

| | |
|---|---|
| **Test** | baut, prüft, fragt nach — und startet den Rechner **einmal** damit. Der Chip bleibt, wie er ist; der nächste Neustart bringt das normale BIOS zurück. Im Startbild steht `TEST IMAGE -- runs once`. |
| **Flash** | dasselbe, aber dauerhaft. Nach der Rückfrage im Coder startet der Rechner neu, und die **Firmware** fragt in Rot ein zweites Mal, bevor irgendetwas geschrieben wird. |

Die zweite Rückfrage stellt bewusst das BIOS und nicht der Coder: ein
Programm darf nicht allein entscheiden, dass der Chip überschrieben wird.

**Was der Assembler auf dem Gerät dafür gelernt hat:** `.org`, `.equ`,
`.include`, Ausdrücke mit Punkt-vor-Strich und Klammern, `ldwa`/`stwa`,
512 statt 256 Symbole — und **lokale Marken** (`.loop`, `.done`) gehören
jetzt zur zuletzt genannten globalen Marke. Ohne das war `.copy` überall
dieselbe, und die Sprünge landeten in einer anderen Funktion.

## Flashen von einer Datei

```
python3 build.py                          # -> firmware/minimal.bin
```

Dann im TB-32: **DEL** beim Start → Reiter **Firmware** → *Flash BIOS from
File* → im Mac-Dialog die `.bin` aussuchen → mit ENTER bestätigen.

Der Reiter zeigt außerdem Größe und Prüfsumme des Chips, der **gerade
läuft** — daran sieht man sofort, ob wirklich das eigene BIOS drin ist.

Gebrannt wird die Chipdatei, **nicht der laufende Chip**: das neue BIOS gilt
ab dem nächsten Einschalten. Wer den Speicher überschreibt, aus dem die CPU
gerade ihre Befehle holt, stürzt im selben Augenblick ab; echte
Flash-Programme kopieren sich dafür erst ins RAM.

### Wenn es schiefgeht

Drei Netze, in dieser Reihenfolge:

1. Ein Abbild ohne `TBBI` oder mit falscher Prüfsumme wird **gar nicht erst
   geschrieben**.
2. Vor jedem Brennen wandert das alte Abbild nach `firmware/bios.backup.bin`.
   Enthält der Chip beim Einschalten Unsinn, spielt das Board die Sicherung
   **von selbst** zurück (Dual BIOS) und sagt es im Terminal.
3. Ein Abbild, das die Prüfung besteht und trotzdem hängenbleibt, holt man
   über *Restore Backup BIOS* im selben Reiter zurück — oder über
   `python3 build.py`, das den Auslieferungszustand wieder herstellt.

Netz 2 greift nur bei einem **kaputten** Abbild. Ein formal gültiges BIOS,
das einfach nicht funktioniert, startet den Rechner nicht — dafür ist Netz 3
da.

---

## Die Hardware dahinter

Drei Ports, siehe `hardware/devices.py`, Klasse `Flash`:

| Port | Richtung | Bedeutung |
|---|---|---|
| `0xB0` | schreiben | 1 Datei vom Wirt holen, 2 Puffer in den RAM, 3 brennen, 4 Sicherung zurück |
| `0xB0` | lesen | Ergebnis des letzten Befehls, 0 = gut |
| `0xB1` | lesen | Bytes im Puffer, 0 = keine Datei |
| `0xB2` | schreiben | Zieladresse für Befehl 2 |

Befehl 1 öffnet den **Dateidialog des Macs** (`pc.py`, `bios_datei_waehlen`).
Das ist der USB-Stick beim BIOS-Flashback eines echten Boards: die Datei
kommt von außen, nicht aus dem laufenden System.

Der Baustein prüft **nichts**. Er nimmt jedes Byte, das man ihm gibt — genau
wie ein echter Flash-Chip. Ob ein Abbild taugt, entscheidet die Firmware.

---

Verwandt: [[13 BIOS-Dienste und was fehlt]], [[02 Speicherkarte und Ports]],
[[06 Bauen und Testen]], [[07 Fallstricke]]

---

## Das Startbild gehört dem Board

Beim Einschalten laufen fünf Sekunden, in denen die CPU noch keinen Strom
hat (`EINSCHALT_HALT_S` in `pc.py`):

| Zeit | was passiert |
|---|---|
| 0,0–1,2 s | Blau füllt den Bildschirm von oben nach unten |
| ab 1,5 s | der **Name aus dem BIOS-Kopf** steht in der Mitte |
| ab 2,0 s | darunter `Press DEL to enter SETUP` |
| 5,0 s | Strom aufs Board, das BIOS startet |

Das steht bewusst im Gehäuse und nicht in der Firmware. Ein Startbild im
BIOS wäre genau dann weg, wenn jemand sein eigenes flasht — und dann gäbe
es auch keine Stelle mehr, an der man DEL drücken könnte.

**Tasten aus dieser Zeit gehen nicht verloren.** Sie werden aufgehoben und
dem Rechner gereicht, sobald die CPU läuft — erst dann, weil ein
Tastatur-Interrupt bei stehender CPU verpufft.

**Aber:** Das Board kann Zeit verschaffen, kein Menü herbeizaubern. Was beim
DEL passiert, entscheidet die Firmware. Ein BIOS ohne Setup tut nichts.
