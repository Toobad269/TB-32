# COMPANY-OS BIOS — was es können soll

Ein BIOS für Firmenrechner. Diese Datei ist das **Pflichtenheft**: was schon
fertig ist, was noch fehlt, wo es hingehört und in welche Fallen man dabei
tritt. Gebaut wird es hier:

```bash
python3 "Custom BIOS/COMPANY-OS/bauen.py"
```

---

## Zuerst: warum dein geflashtes BIOS immer wieder verschwunden war

Das ist kein Fehler im BIOS. **`build.py` überschreibt den Chip.**

Der ROM-Baustein ist die Datei `firmware/bios.bin`. Genau diese Datei baut
`build.py` bei jedem Durchlauf neu aus `firmware/bios.asm`:

```python
bios, syms = asm_file(os.path.join(fw, "bios.asm"),
                      os.path.join(fw, "bios.bin"), ...)
```

Also: du flashst COMPANY-OS → es ist da. Dann ändert einer von uns etwas am
System und lässt `build.py` laufen → der Chip trägt wieder das Serien-BIOS.
Für dich sah das aus wie „mal ist das Geflashte da, mal nicht" — in
Wirklichkeit war es jedes Mal derselbe Handgriff, der es weggewischt hat.

Bei einem echten Mainboard kann das nicht passieren, weil der Compiler nicht
an den Flash-Chip kommt. Bei uns liegt beides im selben Ordner.

**Drei Wege heraus, such dir einen aus:**

1. **`build.py` lässt den Chip in Ruhe, wenn ein fremdes BIOS drauf ist.**
   Vor dem Schreiben den Namen im Kopf lesen (`BIOSHDR_NAME`, Byte 0x10).
   Steht dort nicht der Name des Serien-BIOS, dann nicht überschreiben,
   sondern melden: *„firmware/bios.bin trägt COMPANY-OS — nicht angefasst.
   Zum Zurücksetzen: `python3 build.py --bios-neu`."* Das ist der ehrlichste
   Weg, weil er den Zustand des Rechners respektiert.
2. **Ein Startschalter `--bios <datei>`** in `pc.py`. `Machine(ROOT, rom=...)`
   kann das schon, `pc.py` reicht es nur nicht durch. Dann testest du
   COMPANY-OS, ohne überhaupt zu flashen.
3. **Nur Weg 1 und 2 zusammen** sind wirklich bequem: bauen, mit `--bios`
   ausprobieren, und wenn es gefällt, im Setup flashen — wo es dann auch
   liegen bleibt.

Prüf danach auch die zweite Stelle: `flash.einmal` (Flash-Befehl 6) ist ein
Abbild für **genau einen Start** und wird in `Machine.power_on()` verbraucht.
Das ist so gewollt, aber wenn ein Werkzeug versehentlich Befehl 6 statt 3
schickt, sieht das Ergebnis wieder genauso aus. Der Setup-Knopf *Flash BIOS
from File* macht es richtig (Befehl 3).

---

## Was schon fertig ist

| Was | Wo |
|---|---|
| Setup mit Passwort gesperrt, eigener Reiter *Password* | `passwort.asm`, `tab_password` |
| Eigentümer-Eintrag im Speicher (`BDA_FIRMA`) | `bios.asm`, Zeile `s_firma:` |
| Anzeige oben rechts auf dem Schreibtisch und im Anmeldeschirm | `system/gui.c`, `firma_da()` / `firma_text()` |
| Serien-BIOS leert die Felder beim Start | `firmware/bios.asm` |

Der Reiter *Password* ist damit schon das, was du wolltest: eigener Reiter,
nichts anderes darin.

**Noch nicht einstellbar** ist alles andere: Der Text steht fest im Abbild,
die Schalter in `BDA_POLICY` schreibt das BIOS blind auf 1, und blockieren
kann man gar nichts.

---

## Der neue Reiter *Company*

Sechs Zeilen, aufgebaut wie jeder andere Reiter (`setup_tabs` erweitern,
`SET_TABS` auf 7, Tabelle `tab_company`, Name `s_tab_comp`):

| Zeile | Verhalten | Speicher |
|---|---|---|
| `Owner Tag` | On / Off | `CM_POLICY` Bit 0 |
| `Owner Text` | ENTER öffnet einen Texteditor, bis 31 Zeichen | siehe unten |
| `Block Compiler` | On / Off | `CM_POLICY` Bit 1 |
| `Block Network` | On / Off | `CM_POLICY` Bit 2 |
| `Require Login Password` | On / Off | `CM_POLICY` Bit 3 |
| `Blocked Programs` | ENTER öffnet eine Liste zum Abhaken | `CM_BLOCK0/1` |
| `Item Help` | reine Erklärzeile | `REG_INFO` |

Die On/Off-Zeilen brauchen **keinen** Sonderfall: ein CMOS-Platz mit zwei
Werten und `opts_onoff` genügt, das macht `setup_change` von allein. Nur
`Owner Text` und `Blocked Programs` sind Knöpfe (Register ab `0xE0`, im
Sprungverteiler von `setup_change` eintragen — genau wie `REG_PWSET`).

Ein Bit pro Schalter ist umständlicher als ein Byte pro Schalter. Wenn dir
das lieber ist, nimm für jeden Schalter einen eigenen CMOS-Platz — Platz ist
knapp, aber für drei bis vier reicht er (siehe Tabelle unten).

### Owner Text — der Editor

`passwort.asm` hat den Editor schon fast: `pw_eingabe` liest eine Zeile,
zeichnet aber Sterne statt der Buchstaben (`pw_sterne`). Für den Firmentext
brauchst du dieselbe Schleife mit sichtbarer Ausgabe. Am saubersten wäre eine
gemeinsame Routine `text_eingabe` mit einem Schalter „sichtbar / Sterne",
dann gibt es die Tipp-Logik nur einmal.

### Blocked Programs — die Liste

Ein BIOS kennt keine Dateien. Es kennt eine **feste Liste** von Programmen,
so wie ein echtes BIOS eine feste Liste von Anschlüssen kennt („USB Ports:
Enabled/Disabled"). Also im BIOS eine Tabelle:

```asm
sperr_namen:  .dw s_p_coder, s_p_prompt, s_p_browser, s_p_monitor, ...
```

Pro Programm ein Bit in `CM_BLOCK0` / `CM_BLOCK1` — sechzehn Programme in
zwei CMOS-Plätzen. Der Reiter zeigt die Liste, Hoch/Runter wählt, ENTER
hakt ab.

**Das Wichtige daran:** Weil das BIOS die Namen kennt, kann es sie beim Start
im Klartext in den Speicher legen. Das System muss die Tabelle dann nicht
kennen und keine Bits deuten — es liest einfach eine Liste von Namen. Wenn
später ein Programm dazukommt, ändert sich nur das BIOS.

---

## Wo die Einstellungen liegen

Das ist die eigentliche Entscheidung, und sie fällt beim Text.

**Freie CMOS-Plätze (64 Byte insgesamt, `hardware/devices.py`):**

| Bereich | Zustand |
|---|---|
| `0x00`–`0x09` | Uhr |
| `0x10`–`0x1D` | Einstellungen |
| **`0x1E`–`0x1F`** | **frei, in der Prüfsumme** |
| `0x20`–`0x24` | Passwort (TB-LOCK) |
| **`0x25`–`0x2D`** | **frei, in der Prüfsumme** |
| `0x2E` / `0x2F` | Prüfsumme über `0x10`–`0x2D`, Kennung |
| `0x30`–`0x33` | Gangunterschied der Uhr |
| `0x34`–`0x3E` | frei, **außerhalb** der Prüfsumme |
| `0x3F` | Schreiben sichert die Knopfzelle |

Elf freie Bytes unter der Prüfsumme. **Die Schalter und die Sperrliste passen
da bequem hinein** (`CM_POLICY` = `0x1E`, `CM_BLOCK0/1` = `0x25`/`0x26`).

**Die 32 Byte Firmentext passen nicht.** Dafür zwei Wege:

**A — ein NVRAM-Baustein (empfohlen).** In `hardware/devices.py` einen
zweiten kleinen Speicher anlegen, 256 Byte, eigene Datei `disk/nvram.bin`,
eigenes Portpaar (0x72 Index, 0x73 Daten — direkt neben dem CMOS). Echte
Mainboards machen genau das: die Uhr-CMOS blieb bei 64 Byte, alles Weitere
zog in einen extra Baustein um. Kostet dich zwanzig Zeilen Python und im BIOS
nur eine Lese- und eine Schreibschleife.

**B — das BIOS beschreibt seinen eigenen Chip.** Ist reiner: Der Text liegt
dann wirklich in der Firmware und überlebt sogar das Ziehen der Knopfzelle.
Der Weg dorthin steht schon bereit — Flash-Befehl 5 („Puffer aus dem RAM"),
dann 3 („brennen"). Ablauf: ROM in den RAM kopieren, die Textbytes ändern,
**Prüfsumme neu rechnen**, brennen.

Bei B sind zwei Dinge lebenswichtig:

* Die Kopfprüfsumme (`BIOSHDR_SUM`, Byte 0x0C) muss neu gerechnet werden —
  Feld auf Null, dann `h = h*31 + Wort` über das ganze Abbild, Startwert
  `0x1234`, danach eintragen. Vergisst du das, weist das Mainboard den Chip
  beim nächsten Start zurück und greift zur Sicherung. Das sähe dann wieder
  genau aus wie „mal ist es da, mal nicht".
* Secure Boot rechnet über die **ersten 16 KB** des ROMs (`secure_summe`,
  `li r8, 4096` Wörter). Liegt dein Textfeld darin, macht jede Änderung des
  Firmentexts den gemerkten Fingerabdruck ungültig und der Rechner bleibt
  beim Start stehen. Also den Firmenblock **hinter 16 KB** legen, zum
  Beispiel auf Abbild-Position `0x8000`.

Nimm A, wenn du es diese Woche fertig haben willst. Nimm B, wenn dir wichtig
ist, dass der Eintrag auch das Ziehen der Knopfzelle übersteht — das ist die
Sorte Aufgabe, an der man wirklich versteht, was Firmware ist.

---

## Wie es beim Start ins System kommt

Wie bisher über den BIOS-Datenbereich — das ist unser SMBIOS:

| Adresse | Inhalt | Zustand |
|---|---|---|
| `0x00000500` | 32 Byte Eigentümer-Eintrag, mit Null abgeschlossen | da |
| `0x00000524` | Schalterwort | da, aber blind auf 1 |
| `0x00000528` | **neu:** 8 gesperrte Programme à 16 Byte, leerer Name = Ende | fehlt |

Die Sperrliste endet bei `0x5A8`, das Setup fängt erst bei `0x600` an — es
passt, aber knapp. Wer mehr will, geht nach `0x580` und verschiebt `SETUP_TAB`.

Schalterwort, wie in `firmware/const.inc` schon festgeschrieben:

| Bit | Bedeutung |
|---|---|
| 0 | Eintrag anzeigen |
| 1 | kein Compiler |
| 2 | kein Netz |
| 3 | Passwort verlangt |
| 4 | nur von der eigenen Platte starten |

**Wichtig:** Das Serien-BIOS muss **alle drei** Bereiche beim Start leeren,
auch den neuen. Es leert heute nur die ersten beiden (`bios.asm`, Zeile 584).
Sonst bliebe nach dem Zurückflashen die Sperrliste eines Firmen-BIOS stehen,
und das System sperrte Programme, für die es längst keine Firmware mehr gibt.

---

## Was das System dazu noch tun muss

Das ist die andere Hälfte, und ohne sie ist das BIOS wirkungslos. Alles in
`system/gui.c`:

1. **`firma_policy()` auswerten.** Die Funktion gibt es schon, sie wird nur
    nirgends benutzt. Bit 0 statt `firma_da()` über die Anzeige entscheiden
    lassen.
2. **Gesperrte Programme abfangen** — eine Funktion `gesperrt(char* name)`,
    die die Liste ab `0x528` durchgeht, und **ein** Aufruf in
    `gui_prog_starten()` (Zeile 1901). Dort läuft jeder Start durch, auch der
    aus der Dateiverwaltung.
3. **Im Startmenü grau zeichnen**, nicht verstecken. Ein Programm, das
    unsichtbar ist, sieht nach einem Fehler aus; ein graues sieht nach einer
    Regel aus. Beim Klick eine Meldung: *„Blocked by system policy."*
4. **Compiler und Netz** an denselben Punkt hängen (Bit 1 und 2).

---

## Die Fallen, in die ich beim Bauen getreten bin

* **Der Name im Kopf muss genau 32 Byte sein.** „COMPANY-OS BIOS v1.0" sind
  20 Zeichen, mit der Null 21 — also `.space 11`. Mit einem Byte zu viel
  fängt der Code bei `0x31` statt `0x30` an, der Rechner springt beim
  Einschalten mitten in einen Befehl und läuft ins Nichts, **ohne eine
  einzige Meldung**.
* **`.align 4` nach jeder Zeichenkette**, bevor eine Worttabelle kommt. Ein
  einziges Zeichen mehr im Text verschiebt sonst die ganze Tabelle.
* **`cmp`, `cmpi`, `tst`, `jmpr`, `callr` benutzen `rd`, nicht `ra`.**
* **Neue Zeile im Reiter, aber `setup_tabs` nicht mitgezählt** — dann malt
  das Setup die Zeile, aber Hoch/Runter kommt nicht hin. Die Zahl hinter dem
  Tabellenzeiger ist die Zeilenzahl.

---

## Abnahme

Nimm dir `Custom BIOS/TB-LOCK/pruefen.py` als Vorlage — dort läuft der
Rechner wirklich, mit einer eigenen Knopfzelle im Temp-Ordner, und der Test
drückt dieselben Tasten wie ein Mensch. Fertig ist der Reiter, wenn das
durchläuft:

1. Setup öffnen, auf *Company*, Text auf etwas Eigenes setzen, F10.
2. Neu starten → der neue Text steht oben rechts auf dem Schreibtisch.
3. *Owner Tag* auf Off, F10, neu starten → nichts steht mehr da.
4. Ein Programm sperren, neu starten → es ist im Startmenü grau und lässt
   sich nicht starten.
5. `build.py` laufen lassen → COMPANY-OS ist **immer noch da** (das ist der
   Punkt aus dem ersten Kapitel).
6. Serien-BIOS zurückflashen → Text weg, Sperren weg.

---

## Die ehrliche Grenze

Der TB-32 hat **keinen Speicherschutz**. Eine Richtlinie ist damit eine
Regel, keine Mauer: wer den Coder hat, schreibt sich ein Programm, das die
Ports selbst anspricht, und die Sperre ist ihm egal. Deshalb gehört „Compiler
sperren" in so ein BIOS zwingend dazu — nicht als Schikane, sondern weil ohne
das alles andere nur Zierde wäre.

Und wer an `disk/cmos.bin` kommt, hat die Knopfzelle gezogen. Bei einem
echten Mainboard ist das derselbe Handgriff, und deshalb steht in jedem
Handbuch derselbe Satz: physischer Zugang schlägt jede Firmware-Sperre.
