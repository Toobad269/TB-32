# BIOS-Dienste und was fehlt

Alles in `firmware/bios.asm`. Aufrufweg immer gleich: **Funktionsnummer in
`r0`, Argumente in `r1`–`r5`, Ergebnis in `r0`** — genau wie beim echten PC,
nur dass der `ah` statt `r0` benutzt.

## Was das BIOS heute kann

| Interrupt | Funktionen | |
|---|---|---|
| `INT 0x10` Bildschirm | 17 | 0 putc, 1 puts, 2 setcursor, 3 cls, 4 getcursor, 5 putat, 6 putn, 7 puthex, 8 setmode, 9 box, 10 fillrect, 11 hline, 12 scroll, 13 clearrow, 14 putsat, 15 sbcount, 16 sbline |
| `INT 0x13` Platte | 3 | 0 lesen, 1 schreiben, 2 Größe in Sektoren |
| `INT 0x16` Tastatur | 3 | 0 auf Taste warten, 1 nachsehen, 2 Puffer leeren |
| `INT 0x1A` Zeit | 3 | 0 Ticks seit Start, 1 Uhrzeit, 2 Datum |
| `IRQ 0x08` | — | Timer, zählt `BDA_TICKS` hoch |
| `IRQ 0x09` | — | Tastatur, füllt den Ringpuffer im BDA |
| `INT 0x00` / `INT 0x06` | — | Division durch Null und ungültiger Befehl → Panik-Bildschirm |

Dazu: POST, Bootvorgang, Panik-Bildschirm — und das **Setup mit vier
Reitern** (siehe unten).

## Das Setup

Mit `DEL` oder `F2` beim Start. **Links/Rechts wechselt den Reiter**,
Hoch/Runter die Zeile, ENTER oder `+`/`-` ändert den Wert, `F10` speichert,
`F5` lädt Standardwerte, `ESC` verwirft.

| Reiter | Inhalt |
|---|---|
| **Main** | Uhrzeit, Datum, Quick Boot, POST-Piepser, POST-Meldungen, Standardwerte laden |
| **Hardware** | Takt, Bootgerät, Speichergröße, Platte, Grafikkarte (die letzten drei nur zur Anzeige) |
| **Cooling** | Lüftersteuerung, Drosselgrenze, dazu Temperatur, Lüfter, Drosselung und Höchstwert **live vom Chipsatz** |
| **Security** | Secure Boot, Prüfsumme, „Trust Current Boot Image" |

**Uhrzeit und Datum stellen:** ENTER auf der Zeile öffnet einen kleinen
Feldeditor — Hoch/Runter ändert, Links/Rechts wechselt zwischen
Stunde/Minute/Sekunde bzw. Tag/Monat/Jahr, ENTER beendet. Welches Feld dran
ist, steht unten im Hilfekasten.

Damit das überhaupt möglich wurde, brauchte der **Uhrenbaustein eine eigene
Zeit**: vorher las `CMOS._refresh_clock()` einfach die Uhr des Macs, jeder
Schreibversuch war beim nächsten Lesen wieder weg. Jetzt merkt sich das CMOS
einen **Versatz in Sekunden** (vier Byte ab Register `0x30`), und Schreiben
auf ein Uhrenregister rechnet den Versatz neu aus — genau wie das Drehen an
einem echten RTC-Baustein.

## Ein eigenes BIOS

Das BIOS ist austauschbar: **DEL → Firmware → Flash BIOS from File** nimmt
eine `.bin` vom Mac und brennt sie in den Chip. Was ein BIOS dafür liefern
muss — Kopf, Interruptvektoren, alle vier Dienste mit Registern — steht
vollständig in [[16 Eigenes BIOS schreiben]]. Fertige Vorlage:
`firmware/minimal.asm`, 3324 Byte.

## Secure Boot

Der Gedanke ist derselbe wie beim echten PC: Vor dem Start wird nachgerechnet,
ob das Startabbild noch das bekannte ist. Nur ist die Rechnung hier eine
**Prüfsumme über Bootsektor, Kernel und die ersten 16 KB des ROM** — keine
Unterschrift mit Schlüssel. Das Prinzip „erst prüfen, dann starten" ist
dasselbe, die Fälschungssicherheit nicht.

Den Kernel sucht die Firmware dabei **als Datei** `\SYSTEM\KERNEL.BIN`,
über `kernel_finden` in `firmware/setup.asm` — dieselbe Suche, die auch der
Bootsektor macht. Sie muss messen, was wirklich startet, sonst wäre die
Prüfung eine Attrappe; siehe [[07 Fallstricke]].

- Die gemerkte Summe liegt in vier CMOS-Plätzen (`CM_SUM0`–`CM_SUM3`), also
  in der Knopfzelle
- Stimmt sie nicht, **bootet der Rechner nicht** — es kommt ein roter
  Bildschirm mit dem Hinweis, dass `DEL` ins Setup führt
- Dort merkt *Trust Current Boot Image* die aktuelle Summe

**Wichtig:** Secure Boot ist ab Werk **aus**, und das aus gutem Grund — jedes
`python3 build.py` ändert Kernel oder BIOS und macht die gemerkte Summe
ungültig. Wer es einschaltet, muss nach jedem Neubauen einmal ins Setup und
neu vertrauen. Genau das ist der Sinn der Sache.

## Was fehlt — und ob es sich lohnt

### Lohnt sich, ist wenig Arbeit

**Speichergröße als Dienst** (beim PC `INT 0x12`). Der Wert steht im BDA bei
`0x4A0`, und das OS **liest ihn direkt aus dem Speicher** (`syscall.c`,
Funktion 22). Das ist eine Abkürzung an der Firmware vorbei: Ein Programm
sollte nicht wissen müssen, wo das BIOS seine Notizen macht. Dasselbe gilt
für die Plattengröße bei `0x4A4`.

**Ausstattungsliste** (beim PC `INT 0x11`). Eine Bitmaske: Ist eine Maus da?
Ein Lautsprecher? Wie viele Platten? Heute muss das OS die Ports einzeln
abfragen und raten.

**Warten** (beim PC `INT 0x15`, Funktion `86h`). Die Routine `delay` gibt es
im BIOS längst — sie ist nur nicht nach außen gelegt. Das OS baut sich in
`lib.c` seine eigene Warteschleife.

**Piepser.** Der Lautsprecher hängt an den Ports `0x50`/`0x51`, das OS
schreibt direkt hinein. Ein BIOS-Dienst dafür wäre die saubere Ebene.

### Lohnt sich, ist mehr Arbeit

**Maus.** Der TB-32 *hat* eine Maus mit Hardware-Zeiger (Ports `0x60`–`0x63`,
`IRQ 12`), aber **das BIOS kennt sie überhaupt nicht.** Der Desktop spricht
die Ports direkt an. Beim echten PC macht das ein Treiber über `INT 0x33` —
hier wäre ein BIOS-Dienst der natürliche Ort.

**Zeichensatz-Adresse.** Ein Programm im Grafikmodus braucht sie für den
Blitter. Gelöst ist das über **Systemaufruf 30 des Kernels** — der gibt die
Adresse einer Tabelle im *Kernel* zurück. Sauberer wäre ein Zeichensatz im
ROM und ein BIOS-Dienst dafür (beim PC: `INT 0x10`, `AX=1130h`). Dann könnte
ein Programm auch ohne TOOBAD-OS malen.

**Zeichen vom Bildschirm lesen** (beim PC `INT 0x10`, Funktion `08h`). Wir
können schreiben, aber nicht zurücklesen. Für Dinge wie „was steht da
eigentlich" muss das OS seinen eigenen Bildspeicher mitführen.

**Tastenzustand** (beim PC `INT 0x16`, Funktion `02h`): Ist gerade Shift,
Strg oder Alt gedrückt? In `hardware/devices.py` gibt es dafür sogar schon
die Felder `self.ctrl` und `self.alt` — **sie werden aber nirgends gesetzt
und nirgends gelesen.** Tote Anschlüsse: Die Hardware müsste sie beim
Tastendruck füllen und über den Statusport melden, dann könnte das BIOS sie
weiterreichen.

**Neu starten** (beim PC `INT 0x19`). Es gibt den Portbefehl `P_POWER`, aber
keinen Dienst „lad den Bootsektor nochmal".

### Bewusst nicht nötig

| Fehlt | Warum es hier keinen Sinn hat |
|---|---|
| Drucker (`INT 0x17`) | Es gibt keinen Drucker |
| Serielle Schnittstelle (`INT 0x14`) | Keine Hardware — wäre aber der natürliche Anfang für ein Netzwerkkapitel, siehe [[11 Offene Punkte]] |
| ROM-BASIC (`INT 0x18`) | Reines Erbe der 80er |
| Zylinder/Kopf/Sektor | Der TB-32 spricht von Anfang an in **LBA**, also durchnummerierten Sektoren — die Umrechnerei alter Platten fällt weg |
| Erweiterte Speicherkarte (`E820`) | Die Aufteilung liegt fest, siehe [[02 Speicherkarte und Ports]] |

## Woran man beim Ergänzen denken muss

Ein neuer Dienst ist **drei** Änderungen, nicht eine:

1. Eintrag in der Sprungtabelle in `firmware/bios.asm` (und der Vektor in
   `bios_init`, falls es ein neuer Interrupt ist)
2. Konstante in `firmware/const.inc`
3. Falls das OS ihn nutzen soll: Brücke in `system/start.asm` und
   Verpackung in `system/lib.c` — und für Programme zusätzlich eine
   Syscall-Nummer, siehe [[05 Konventionen]]

Und: Der Kernel darf **nicht größer als bis `0xB0000`** werden, sonst frisst
er die Puffer des Dateisystems — die Falle steht in [[07 Fallstricke]].

Verwandt: [[05 Konventionen]], [[02 Speicherkarte und Ports]], [[11 Offene Punkte]]
