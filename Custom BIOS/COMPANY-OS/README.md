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
3. **Beides zusammen** ist der bequeme Weg: bauen, mit `--bios` ausprobieren,
   und wenn es gefällt, im Setup flashen — wo es dann auch liegen bleibt.

Prüf danach auch die zweite Stelle: `flash.einmal` (Flash-Befehl 6) ist ein
Abbild für **genau einen Start** und wird in `Machine.power_on()` verbraucht.
Das ist so gewollt, aber wenn ein Werkzeug versehentlich Befehl 6 statt 3
schickt, sieht das Ergebnis wieder genauso aus. Der Setup-Knopf *Flash BIOS
from File* macht es richtig (Befehl 3).

---

## Was schon fertig ist

| Was | Wo |
|---|---|
| Setup mit Supervisor-Passwort gesperrt, eigener Reiter *Password* | `passwort.asm`, `tab_password` |
| Drei Fehlversuche, dann abgewiesen | `pw_tor` |
| F5 „Load Defaults" lässt das Passwort in Ruhe | `setup_load_defaults` |
| Secure Boot (Prüfsumme über Bootsektor, Kernel und ROM) | `secure_summe` |
| BIOS aus einer Datei flashen, Sicherung zurückspielen | `flash_bios`, `flash_restore` |
| Eigentümer-Eintrag im Speicher (`BDA_FIRMA`) | `bios.asm`, `s_firma:` |
| Anzeige oben rechts und im Anmeldeschirm | `system/gui.c`, `firma_da()` |

Der Reiter *Password* ist damit schon das, was du wolltest: eigener Reiter,
nichts anderes darin.

---

# Die Funktionsliste

Sortiert nach Wichtigkeit, nicht nach Aufwand. Jede Zeile sagt, wo sie
hingehört und wo die Einstellung liegt.

## A — Pflicht: das macht ein Firmen-BIOS aus

### A1 Zwei Passwörter statt einem
*Reiter: Password · CMOS 0x27–0x2B*

Heute gibt es nur das **Supervisor**-Passwort, und das bewacht nur das Setup.
Ein Firmenrechner braucht zwei:

* **Supervisor** — öffnet das Setup. Gibt es.
* **Power-On (User)** — wird **vor dem Booten** verlangt. Ohne das startet
  der Rechner überhaupt nicht.

Das ist der Kern der ganzen Sache. Ein BIOS-Passwort, das nur das Setup
schützt, hält niemanden davon ab, den Rechner einzuschalten und zu benutzen.

Zu bauen ist wenig: `pw_tor` gibt es schon, es braucht nur eine zweite
Prüfsumme (Flag + 4 Byte) und einen Aufruf in `bios.asm` vor dem Bootversuch.
Das Supervisor-Passwort muss dabei **auch** durchs Power-On-Tor kommen — der
Chef sperrt sich sonst selbst aus, wenn er das User-Passwort vergisst.

### A2 Fehlversuche im CMOS zählen
*Reiter: Password · CMOS 0x2C*

`pw_tor` zählt heute in einem Register: drei Versuche, dann abgewiesen — aber
ein Druck auf Reset fängt bei drei wieder an. Also raten in Ruhe, beliebig
oft.

Echte BIOSe zählen deshalb **in der Knopfzelle**. Nach drei Fehlversuchen
bleibt der Rechner stehen, und der Zähler überlebt den Neustart; erst die
richtige Eingabe setzt ihn zurück. Ein Byte, und es ändert alles.

### A3 Der Eigentümer-Eintrag, einstellbar
*Reiter: Company · siehe „Wo die Einstellungen liegen"*

Text bis 31 Zeichen und ein Schalter An/Aus. Steht heute fest im Abbild.

### A4 Programme sperren
*Reiter: Company · CMOS 0x25/0x26*

Ein BIOS kennt keine Dateien. Es kennt eine **feste Liste** von Programmen,
so wie ein echtes BIOS eine feste Liste von Anschlüssen kennt („USB Ports:
Enabled/Disabled"):

```asm
sperr_namen:  .dw s_p_coder, s_p_prompt, s_p_browser, s_p_monitor, ...
```

Ein Bit je Programm, sechzehn Programme in zwei CMOS-Plätzen. Weil das BIOS
die Namen kennt, legt es sie beim Start im Klartext in den Speicher — das
System muss dann keine Bits deuten und keine Tabelle mitführen. Kommt später
ein Programm dazu, ändert sich nur das BIOS.

### A5 Compiler und Netz sperren
*Reiter: Company · Schalterwort Bit 1 und 2*

Der Compiler gehört zwingend dazu, und zwar nicht aus Schikane: Der TB-32 hat
keinen Speicherschutz. Wer den Coder hat, schreibt sich ein Programm, das die
Ports selbst anspricht — und dann ist jede andere Sperre nur noch Zierde.

### A6 Nur von der eigenen Platte starten
*Reiter: Company oder Hardware · Schalterwort Bit 4*

Der klassische erste Angriff auf einen fremden Rechner ist: ein eigenes
System von woanders starten und die Platte in Ruhe auslesen. Sperrt man das
nicht, waren alle anderen Sperren umsonst — sie stehen ja im System, das gar
nicht erst hochkommt.

Bei uns heißt das: Boot-Reihenfolge festnageln und `Network`/`Floppy` als
Startquelle verweigern, solange das Bit gesetzt ist.

### A7 Schreibschutz für den Chip — und der ist eine echte Lücke
*Reiter: Firmware · Baustein, nicht CMOS*

`P_FLASH_CMD` ist ein ganz normaler Port. Der TB-32 kennt keine Portrechte —
**jedes Programm im laufenden System kann den BIOS-Chip überschreiben.**
Deine ganze Firmware-Sperre hängt an einem `portout`.

Ein Schalter im Setup allein hilft dagegen nicht, denn den liest ja nur das
Setup. Die Sperre muss im **Baustein** sitzen, in `hardware/devices.py`:

* `Flash` bekommt ein Sperr-Latch, etwa `self.gesperrt = False`.
* Ein neuer Befehl (z. B. 10) setzt es. Löschen kann es **nur ein Neustart**.
* Ist es gesetzt, verweigern Befehl 3 (brennen) und 4 (Sicherung) den Dienst.
* Das BIOS setzt es als **letzten Schritt vor dem Booten** — bis dahin kann
  es selbst noch flashen, danach niemand mehr.

Genau so machen es echte Chipsätze: ein Lock-Bit, das die Firmware setzt und
das nur ein Reset wieder löst. Das ist die einzige Stelle in dieser Liste, an
der du am Bauteil arbeiten musst — und die wichtigste.

### A8 Merken, dass die Knopfzelle gezogen wurde
*Reiter: Company (Anzeige) · CMOS-Kennung 0x2F*

Wer an `disk/cmos.bin` kommt, löscht alle Sperren. Das kannst du nicht
verhindern — bei einem echten Mainboard genauso wenig. Aber du kannst es
**sichtbar** machen: Ist die Kennung beim Start ungültig, meldet das BIOS in
Rot *„Configuration was cleared — contact your administrator"* und schreibt
es in den Ereignisspeicher.

Echte Firmenrechner nennen das *Chassis Intrusion*. Der Gedanke dahinter ist
wichtiger als die Technik: Wo man nicht verhindern kann, sorgt man dafür,
dass es auffällt.

## B — Sehr sinnvoll

### B1 Ereignisspeicher
*Eigener Reiter „Event Log" · braucht NVRAM*

Die letzten acht Ereignisse mit Datum und Uhrzeit: falsches Passwort, CMOS
geleert, BIOS geflasht, Secure Boot hat angehalten, Standardwerte geladen.
Acht Einträge à 8 Byte reichen.

Für einen Firmenrechner ist das oft nützlicher als jede Sperre — Sperren
sagen dir, was nicht passieren darf, das Protokoll sagt dir, was passiert
ist. Echte BIOSe haben das als *SMBIOS Type 15 Event Log*.

### B2 Secure Boot: anhalten oder nur warnen
*Reiter: Security · CMOS 0x18 auf drei Werte erweitern*

Heute nur An/Aus. Echte Firmware kennt drei Stufen: *Enforce* (anhalten),
*Audit* (starten, aber melden und protokollieren), *Off*. Die mittlere
brauchst du selbst am meisten — beim Entwickeln willst du gewarnt werden,
nicht ausgesperrt.

### B3 Inventarangaben, die das System auslesen kann
*Reiter: Company (nur Anzeige) · NVRAM*

Modell, Seriennummer, BIOS-Fassung, Datum der Inbetriebnahme, Anzahl der
Starts, Betriebsstunden. Im Setup nur lesbar, im Speicher für das System
abgelegt — der Systemmonitor zeigt sie dann an.

Das ist genau, wofür SMBIOS bei echten PCs da ist, und es ist wenig Arbeit:
zwei Zähler hochzählen und ein paar Texte hinlegen.

### B4 Startmenü mit F12
*kein Reiter, ein Tastendruck beim Start*

Einmal von woanders starten, **ohne** die Einstellung zu ändern. Steht bei
A6 die Sperre, verlangt F12 das Supervisor-Passwort — dann ist es das
Werkzeug des Administrators und nicht das Schlupfloch.

### B5 Netzwerkstart
*Reiter: Hardware · groß*

Im Setup steht heute „Network (not installed)". Das stimmt nicht mehr — die
Karte gibt es, mit ARP, IP, UDP und TCP darüber. Ein Firmenrechner holt sein
System vom Server, das ist bei echten Firmen der Normalfall (PXE).

Das ist das größte Stück auf dieser Liste und es passt zum Pi-Ziel: Ein
Rechner, der sein System übers Netz holt, braucht auf der Platte gar nichts
mehr.

### B6 Startbild der Firma
*Reiter: Company*

Den BIOS-Namen malt das Mainboard schon in die Bildmitte. Drei Zeilen
Firmentext darunter, aus demselben Speicher wie der Eigentümer-Eintrag —
kleine Arbeit, große Wirkung.

## C — Nett, wenn Zeit ist

* **Numlock beim Start**, **Startton an/aus** (halb da), **POST-Zeit
  anzeigen**.
* **Automatisch wieder an nach Stromausfall** — bei echten Servern *AC Power
  Recovery*. Bei uns eher Zierde, aber leicht.
* **Startverzögerung** — Sekunden warten, bevor gebootet wird, damit man DEL
  sicher trifft. Auf langsamen Anzeigen hilft das wirklich.
* **Eigener Reiter „Exit"** mit *Save & Exit*, *Discard & Exit*, *Load
  Defaults*. Rein kosmetisch, aber echte BIOSe haben ihn, und er macht die
  F-Tasten für Ungeübte sichtbar.

## D — Was ich weglassen würde, und warum

* **Verschlüsselte Platte / TPM.** Ohne Speicherschutz und ohne echte
  Schlüssel wäre das eine Behauptung, keine Sicherheit — und eine
  Sicherheitsanzeige, die lügt, ist schlimmer als gar keine.
* **Master- oder Wiederherstellungspasswort.** Ein zweiter Schlüssel ist ein
  zweites Loch. Wer sich aussperrt, zieht die Knopfzelle; das ist bei echten
  Rechnern derselbe Weg.
* **Fernwartung (wie vPro/AMT).** Das ist ein eigener kleiner Rechner im
  Rechner — ein Projekt für sich, kein Reiter.
* **Fingerabdruck, Smartcard.** Keine Hardware da, und nachgebaut wäre es
  nur ein zweites Passwortfeld mit hübscherem Namen.

## Reihenfolge zum Bauen

1. **A7 Flash-Sperre** und der `build.py`-Schutz von ganz oben. Solange der
   Chip von außen überschreibbar ist, testest du im Sand.
2. **A3 Company-Reiter mit Owner Tag** — die kleinste Sache, an der der ganze
   neue Reiter einmal durchläuft.
3. **A1/A2 Power-On-Passwort und Zähler.**
4. **A6 Startquelle sperren.**
5. **A4/A5 Programme, Compiler, Netz** — zusammen mit der Systemseite unten.
6. **A8 und B1** — erst merken, dann protokollieren.
7. Alles Weitere nach Lust.

---

## Der Reiter *Company*

Aufgebaut wie jeder andere Reiter: `setup_tabs` erweitern, `SET_TABS` auf 7,
Tabelle `tab_company`, Name `s_tab_comp`.

| Zeile | Verhalten | Speicher |
|---|---|---|
| `Owner Tag` | On / Off | `CM_POLICY` Bit 0 |
| `Owner Text` | ENTER öffnet einen Texteditor, bis 31 Zeichen | siehe unten |
| `Block Compiler` | On / Off | `CM_POLICY` Bit 1 |
| `Block Network` | On / Off | `CM_POLICY` Bit 2 |
| `Require Login Password` | On / Off | `CM_POLICY` Bit 3 |
| `Boot From Internal Disk Only` | On / Off | `CM_POLICY` Bit 4 |
| `Blocked Programs` | ENTER öffnet eine Liste zum Abhaken | `CM_BLOCK0/1` |
| `Configuration Cleared` | nur Anzeige: Yes / No | Kennung 0x2F |
| `Item Help` | reine Erklärzeile | `REG_INFO` |

Die On/Off-Zeilen brauchen **keinen** Sonderfall: ein CMOS-Platz mit zwei
Werten und `opts_onoff` genügt, das macht `setup_change` von allein. Nur
`Owner Text` und `Blocked Programs` sind Knöpfe (Register ab `0xE0`, im
Sprungverteiler von `setup_change` eintragen — genau wie `REG_PWSET`).

Ein Bit pro Schalter ist umständlicher als ein Byte pro Schalter. Wenn dir
das lieber ist, nimm für jeden Schalter einen eigenen CMOS-Platz — Platz ist
knapp, aber für vier reicht er (siehe Tabelle unten).

### Owner Text — der Editor

`passwort.asm` hat ihn fast: `pw_eingabe` liest eine Zeile, zeichnet aber
Sterne statt Buchstaben (`pw_sterne`). Für den Firmentext brauchst du
dieselbe Schleife mit sichtbarer Ausgabe. Am saubersten: eine gemeinsame
Routine `text_eingabe` mit einem Schalter „sichtbar / Sterne", dann gibt es
die Tipp-Logik nur einmal.

---

## Wo die Einstellungen liegen

Das ist die eigentliche Entscheidung, und sie fällt beim Text.

**Freie CMOS-Plätze (64 Byte insgesamt, `hardware/devices.py`):**

| Bereich | Zustand |
|---|---|
| `0x00`–`0x09` | Uhr |
| `0x10`–`0x1D` | Einstellungen |
| **`0x1E`–`0x1F`** | **frei, in der Prüfsumme** |
| `0x20`–`0x24` | Supervisor-Passwort (TB-LOCK) |
| **`0x25`–`0x2D`** | **frei, in der Prüfsumme** |
| `0x2E` / `0x2F` | Prüfsumme über `0x10`–`0x2D`, Kennung |
| `0x30`–`0x33` | Gangunterschied der Uhr |
| `0x34`–`0x3E` | frei, **außerhalb** der Prüfsumme |
| `0x3F` | Schreiben sichert die Knopfzelle |

Elf freie Bytes unter der Prüfsumme, und die Liste oben braucht genau elf:
Schalterwort (1), Sperrliste (2), Power-On-Passwort (5), Fehlversuche (1) —
bleiben zwei übrig. Es passt, aber ohne Reserve.

**Die 32 Byte Firmentext passen nicht.** Dafür zwei Wege:

**A — ein NVRAM-Baustein (empfohlen).** In `hardware/devices.py` einen
zweiten kleinen Speicher anlegen, 256 Byte, eigene Datei `disk/nvram.bin`,
eigenes Portpaar (0x72 Index, 0x73 Daten — direkt neben dem CMOS). Echte
Mainboards machen genau das: die Uhr-CMOS blieb bei 64 Byte, alles Weitere
zog in einen extra Baustein um. Kostet dich zwanzig Zeilen Python und im BIOS
je eine Lese- und Schreibschleife. Der Ereignisspeicher (B1) und die
Inventarangaben (B3) hätten damit auch gleich ein Zuhause.

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
Sorte Aufgabe, an der man wirklich versteht, was Firmware ist. Und wenn du
B nimmst: **A7 zuerst**, sonst schreibt dir jedes Programm im System den
Chip wieder um.

---

## Wie es beim Start ins System kommt

Wie bisher über den BIOS-Datenbereich — das ist unser SMBIOS:

| Adresse | Inhalt | Zustand |
|---|---|---|
| `0x00000500` | 32 Byte Eigentümer-Eintrag, mit Null abgeschlossen | da |
| `0x00000524` | Schalterwort | da, aber blind auf 1 |
| `0x00000528` | **neu:** 8 gesperrte Programme à 16 Byte, leerer Name = Ende | fehlt |
| `0x000005A8` | **neu:** Inventar — Seriennummer, Starts, Betriebsstunden | fehlt |

Die Sperrliste endet bei `0x5A8`, das Setup fängt erst bei `0x600` an. Für
das Inventar bleiben also 88 Byte; wer mehr braucht, verschiebt `SETUP_TAB`
nach oben.

Schalterwort, wie in `firmware/const.inc` schon festgeschrieben:

| Bit | Bedeutung |
|---|---|
| 0 | Eintrag anzeigen |
| 1 | kein Compiler |
| 2 | kein Netz |
| 3 | Passwort verlangt |
| 4 | nur von der eigenen Platte starten |

**Wichtig:** Das Serien-BIOS muss **alle** diese Bereiche beim Start leeren,
auch die neuen. Es leert heute nur die ersten beiden (`bios.asm`, Zeile 584).
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
5. **Bit 3 „Passwort verlangt"** — die Anmeldung darf dann kein Konto ohne
    Passwort mehr durchlassen.
6. **Inventar anzeigen** im Systemmonitor, sobald B3 steht.

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
* **Neue CMOS-Plätze über `0x2D` hinaus** fallen aus der Prüfsumme heraus.
  Sie überleben zwar den Neustart, aber ein leergeräumtes CMOS merkt niemand.

---

## Abnahme

Nimm dir `Custom BIOS/TB-LOCK/pruefen.py` als Vorlage — dort läuft der
Rechner wirklich, mit einer eigenen Knopfzelle im Temp-Ordner, und der Test
drückt dieselben Tasten wie ein Mensch. Fertig ist es, wenn das durchläuft:

1. Setup öffnen, auf *Company*, Text auf etwas Eigenes setzen, F10.
2. Neu starten → der neue Text steht oben rechts auf dem Schreibtisch.
3. *Owner Tag* auf Off, F10, neu starten → nichts steht mehr da.
4. Power-On-Passwort setzen, neu starten → der Rechner fragt, **bevor** er
   bootet. Dreimal falsch → er bleibt stehen. Reset → er zählt nicht von
   vorn.
5. Ein Programm sperren, neu starten → es ist im Startmenü grau und lässt
   sich nicht starten.
6. Im System ein Programm schreiben, das `portout` auf `P_FLASH_CMD` macht →
   der Chip bleibt heil (A7).
7. `build.py` laufen lassen → COMPANY-OS ist **immer noch da**.
8. Serien-BIOS zurückflashen → Text weg, Sperren weg.

---

## Die ehrliche Grenze

Der TB-32 hat **keinen Speicherschutz**. Eine Richtlinie ist damit eine
Regel, keine Mauer: wer den Coder hat, schreibt sich ein Programm, das die
Ports selbst anspricht, und die Sperre ist ihm egal. Deshalb steht „Compiler
sperren" so weit oben, und deshalb muss die Flash-Sperre im Bauteil sitzen
und nicht im Setup.

Und wer an `disk/cmos.bin` kommt, hat die Knopfzelle gezogen. Bei einem
echten Mainboard ist das derselbe Handgriff, und deshalb steht in jedem
Handbuch derselbe Satz: physischer Zugang schlägt jede Firmware-Sperre. Was
man dagegen tun kann, ist nicht verhindern, sondern **sichtbar machen** —
darum A8.
