# Konventionen

## Register

| Register | Rolle |
|---|---|
| `r0` | Rückgabewert **und Arbeitsregister des Compilers** — jeder Ausdruck landet hier |
| `r1`–`r5` | Argumente 1–5 |
| `r6`–`r9` | müssen von der gerufenen Funktion gesichert werden |
| `r10`–`r12` | Kratzregister, dürfen jederzeit zerstört werden |
| `r13` (`at`) | Hilfsregister des Assemblers — nach `ldwa`/`stwa` immer futsch |
| `r14` (`fp`) | Framepointer |
| `r15` (`sp`) | Stackpointer |

Weil der Compiler alles in `r0` rechnet, **muss jeder Interrupthandler r0
sichern** — siehe [[07 Fallstricke]].

## Aufruf einer Funktion

Der Compiler wertet die Argumente aus, legt sie auf den Stack und holt sie
rückwärts nach `r1`…`r5`. Prolog jeder Funktion:

```
push fp
mov fp, sp
subi sp, sp, <rahmen>     ; Größe wird nachgetragen
push r6 … r9
[Parameter in die Rahmenplätze speichern]
```

Epilog: `mov sp, fp` / `pop fp` / `ret`.

## BIOS-Dienste (Firmware)

Vollständige Liste und die bekannten Lücken: [[13 BIOS-Dienste und was fehlt]]

Funktionsnummer in `r0`, Argumente in `r1`–`r5`, Ergebnis in `r0`.

| Interrupt | Dienst |
|---|---|
| `INT 0x10` | Bildschirm: 0 putc, 1 puts, 2 setcursor, 3 cls, 4 getcursor, 5 putat, 6 putn, 7 puthex, 8 setmode, 9 box, 10 fillrect, 11 hline, 12 scroll, 13 clearrow, 14 putsat, **15 sbcount, 16 sbline** |
| `INT 0x13` | Platte: 0 lesen, 1 schreiben, 2 Größe |
| `INT 0x16` | Tastatur: 0 warten, 1 nachsehen, 2 leeren |
| `INT 0x1A` | Zeit: 0 Ticks, 1 Uhrzeit, 2 Datum |
| `INT 0x40` | **Systemaufruf des Betriebssystems** |
| `INT 0x41` | Rechenzeit freiwillig abgeben (Scheduler) |
| `IRQ 0x08` | Timer (100 Hz) — trägt bei aktivem Multitasking den Umschalter |
| `IRQ 0x09` | Tastatur |

## Systemaufrufe (`INT 0x40`)

Nummer in `r0`, Argumente `r1`–`r4`, Ergebnis in `r0`.

| Nr | Bedeutung | Nr | Bedeutung |
|---|---|---|---|
| 0 | putc(ch, attr) | 14 | sleep(ticks) |
| 1 | puts(str, attr) | 15 | beep(freq, dauer) |
| 2 | getkey() | 16 | disksize() |
| 3 | cls(attr) | 17 | setmode(m) |
| 4 | exit() | 18 | out(port, wert) |
| 5 | ticks() | 19 | in(port) |
| 6 | putn(n, attr) | 20 | box(x,y,w,h) |
| 7 | setcursor(x, y) | 21 | hline(x,y,len,ch) |
| 8 | putat(x,y,ch,attr) | 22 | memkb() |
| 9 | haskey() | 23 | flushkeys() |
| 10 | fileread(name, adr, max) | 24–27 | Verzeichnis abfragen |
| 11 | filewrite(name, adr, len) | **28** | **Fortschritt melden (0–100)** |
| 12 | clock() | **29** | **Statustext melden** |
| 13 | date() | **30** | **Adresse des Zeichensatzes** |
| | | **31** | **Fläche/Rahmen malen** (x\|y<<16, w\|h<<16, Farbe, Kommando) |
| | | **32** | **Zeichen malen** (x\|y<<16, Farbe\|Zeichen<<16, Hintergrund) |

Syscall 28 und 29 füttern das Übersetzungsfenster; `cc.c` meldet darüber
Fortschritt und Phase.

Die Ausgabe-Aufrufe (0, 1, 2, 3, 6, 9) gehen automatisch ins **Terminalfenster**,
wenn `term_aktiv` gesetzt ist. Programme merken davon nichts.

## Tastenkürzel

| Taste | Wirkung |
|---|---|
| `F11` | Vollbild des **Emulatorfensters** |
| `Strg`+`Q` / `Cmd`+`Q` | Beenden |
| `Strg`+`R` | Reset (Knopf am Gehäuse) |
| `F12` | Einblendung mit Takt, Temperatur, Bildrate |
| `Cmd`+`V` / `Strg`+`V` | Text vom Mac in TOOBAD-OS einfügen |
| Taste halten | Rücktaste, Entfernen und Pfeile wiederholen nach 0,4 s alle 30 ms |
| `Cmd`+`C` | Auswahl aus TOOBAD-OS zum Mac |
| `Strg`+`A/C/X/V` | im Gast: alles / kopieren / ausschneiden / einfügen |
| `ü` | **Einschaltknopf** — wirkt nur, wenn der Rechner aus ist |
| `Strg`+`K` / `Cmd`+`K` | **alles kopieren**, still: im Textmodus der ganze Bildschirm (auch im BIOS und im Setup), im Grafikmodus der vollständige Text des Coders |
| beim Einschalten | **5 s Bedenkzeit** (`EINSCHALT_HALT_S` in `pc.py`), Tasten daraus werden aufgehoben |

`Strg`+*Buchstabe* wird in `pc.py` allgemein als Steuerzeichen 1–26 an den
Gast durchgereicht — die Abfrage steht **nach** Strg+Q und Strg+R.

`Strg`+`K` sitzt im **Gehäuse** (`pc.py`, `alles_kopieren`) und nicht im
System. Im BIOS und im Setup läuft gar kein Betriebssystem, das eine Taste
auswerten könnte — von dort aus geht es überall. Es meldet bewusst nichts:
wer die Taste drückt, weiß, was er wollte.

Das `ü` steht bewusst **nicht** bei den Tastendrücken, sondern beim
Text-Ereignis (`TEXTINPUT`). Bei `KEYDOWN` ist `event.unicode` je nach
Layout leer oder trägt noch das Zeichen des vorigen Anschlags — Umlaute
kommen nur über das Text-Ereignis zuverlässig an.

## Namensgebung

- Oberfläche des Systems: **englisch** — und zwar restlos alles, was auf dem
  Bildschirm des TB-32 landet, auch Firmware- und Bootsektormeldungen. Der
  neue Bootsektor hatte deutsche Texte und stach sofort heraus.
- Quelltext-Kommentare: **deutsch**
- Variablen im OS-Quelltext: deutsch oder englisch gemischt, gewachsen —
  nicht vereinheitlichen, das erzeugt nur Diffs ohne Nutzen.

## Dateisystem

Namen **max. 15 Zeichen**, Groß-/Kleinschreibung egal beim Suchen, in der
Anzeige groß. Programme werden gesucht: aktueller Ordner → `\SYSTEM` → `\PROGS`.

Verwandt: [[01 Architektur TB-32]], [[04 Compiler TCC Grenzen]]
