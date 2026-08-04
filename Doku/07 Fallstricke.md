# Fallstricke — teuer erkaufte Erkenntnisse

Jeder Eintrag hat mich echte Suchzeit gekostet. Wenn etwas Ähnliches auftritt:
zuerst hier nachsehen.

## Schlafen hat nicht geschlafen — der Rechner wurde 65 Grad heiß

**Symptom:** Sobald ein Spiel mit Bildtakt lief, ging die Temperatur von 23
auf **65 Grad**, der Chipsatz drosselte auf **60 %**, und das ganze System
wurde zäh — es fühlte sich an wie eingefroren. Auslastung: 100 %, obwohl das
Spiel den größten Teil jeder Bildzeit schlafen wollte.

**Ursache:** `proc_next()` gibt den **eigenen** Prozess zurück, wenn sonst
niemand rechenbereit ist — sonst hätte der Umschalter gar keinen zum Nehmen.
Damit war `proc_sleep()` wirkungslos: der Prozess legte sich schlafen, wurde
im selben Interrupt wieder geweckt, legte sich schlafen … Bei einem einzigen
laufenden Programm schlief also nie jemand, und der Prozessor lief Volllast.

Gefunden durch Abtasten des Befehlszählers gegen `kernel.sym`: die Spitze lag
nicht im Spiel, sondern in `proc_next` / `proc_schedule` / `proc_sleep`.

**Behoben in:** `system/proc.c`. `proc_sleep()` wartet den Rest der Zeit
selbst ab, mit **angehaltenem Prozessor** (`hlt`), und gibt vorher per
`int 0x41` den anderen die Gelegenheit. Das `sti` davor ist Pflicht — wir
stecken in einem Systemaufruf, dort sind die Interrupts gesperrt.

| | vorher | nachher |
|---|---|---|
| Temperatur bei Flappy | 65,1 °C | 26,6 °C |
| Drosselung | 60 % | 0 % |
| Auslastung | 100 % | 10 % |

**Merke:** Ein „Leerlauf" ist erst dann einer, wenn der Prozessor wirklich
`hlt` ausführt. Ob das passiert, sieht man an der Temperatur — das
Wärmemodell ist hier ein ehrlicherer Messfühler als jede Zählung.

## Wer alleine malt, malt über alles drüber

**Symptom:** Die Uhrzeit stand mitten im Control Panel, obwohl das
Uhrfenster dahinter lag.
**Ursache:** Die Uhr frischte sich einmal je Sekunde selbst auf —
`app_clock(i)` malt aber nur ihren Inhalt, ohne zu wissen, welche Fenster
davor liegen. Dasselbe galt für den System Monitor.
**Behoben in:** `system/gui.c` — sie fordern jetzt ein normales
Neuzeichnen an (`neu = 1`), und `draw_desktop()` kennt die Reihenfolge.
**Merke:** In einem Fenstersystem darf **nur** die Stelle malen, die die
Z-Reihenfolge kennt. Jede Abkürzung „ich zeichne schnell nur mein Fenster"
ist genau dann falsch, wenn etwas davor liegt.

## Mauszustand gehört in die Ereignisse, nicht in eine Abfrage

**Symptom:** Der Rechtsklick kam beim TB-32 nie an — das Menü in Word ging
im Test auf, auf Colins Rechner nicht.
**Ursache:** `pc.py` las die Tastenlage mit `pygame.mouse.get_pressed()`,
ausgelöst durch ein Klick-Ereignis. Das liefert je nach Plattform beim
Loslassen noch den alten Stand, und bei manchen Trackpads gar nichts für
die rechte Taste.
**Behoben in:** `pc.py` — der Zustand wird aus den Ereignissen selbst
geführt (`e.button`: 1 links, 2 Mitte, 3 rechts). Dazu gilt Ctrl+Klick als
Rechtsklick, wie auf dem Mac üblich.
**Merke:** Wer Ereignisse bekommt, soll sie auch auswerten. Eine
Zustandsabfrage im Ereignishandler ist immer einen Takt zu spät.

## Mauszustand gehört in die Ereignisse, nicht in eine Abfrage

## „Kaputt" hiess in Wahrheit „noch nicht fertig"

**Symptom:** Das Fuellwerkzeug in Paint fuellte nur drei Zeilen und hoerte
dann auf. Jede Messung bestaetigte es: Zeilen = 3, Warteschlange = 4 Eintrage,
und beim Auslesen der Zaehler standen sie auf 0.

**Ursache:** Nichts davon war ein Fehler. Die Funktion **rechnete noch**,
als ich gemessen habe. Der Klick wartete 0,6 Sekunden, das Fuellen brauchte
aber eine halbe Minute -- ein Funktionsaufruf mit vier Bereichspruefungen je
Bildpunkt, 26.000-mal. Die scheinbar widerspruechlichen Zahlen (Zaehler im
Speicher = 3 und 5, meine mitgeschriebenen Kopien = 2 und 4) waren einfach
Momentaufnahmen aus verschiedenen Augenblicken.

**Behoben durch Messen statt Raten:** Ein Blick auf den Befehlszaehler zeigte
die Schleife munter weiterlaufen. Danach: Zeilen fuellt der Blockkopierer,
die Laufgrenzen findet die Blocksuche der Hardware. Aus einer halben Minute
wurde etwa eine Sekunde.

**Und noch einmal, zwei Stunden spaeter:** Beim Coder blieb das
Editorfenster leer, die Statuszeile fehlte, alle Zaehler sahen falsch aus.
Wieder dasselbe -- das Bild war schlicht noch nicht fertig gemalt, weil die
Syntaxfaerbung anfangs 476.000 Befehle je Neuzeichnen kostete. Dieser
Fallstrick ist teuer genug, dass er zweimal zugeschlagen hat.

**Merke:** Bevor man einen Fehler sucht, pruefen ob die Sache ueberhaupt
fertig ist. Ein „falsches" Zwischenergebnis, das sich bei jeder Messung
aendert, ist meistens kein Fehler, sondern eine laufende Rechnung. Und wenn
sich mitgeschriebene Werte und der Speicherinhalt widersprechen, ist der
Speicher aktueller -- nicht die Kopie.

## Ein unbekannter Port wird still verschluckt

**Symptom:** Die neu eingebaute Doppelpufferung blieb einfach aus. Kein
Fehler, keine Meldung — `gx_doppelpuffer(1)` tat nichts.
**Ursache:** `bus.port_out` schlägt den Port in `port_devices` nach; steht er
nicht drin, landet er in `unknown_ports` und wird verworfen. Ich hatte den
Port in `isa.py` und im Gerät eingetragen, aber nicht in der Geräteliste in
`machine.py`.
**Merke:** Ein neuer Port braucht **drei** Einträge: Konstante in `isa.py`,
Behandlung im Gerät, und die Registrierung in `machine.py`. Wenn eine neue
Hardwarefunktion „nichts tut", zuerst `m.bus.unknown_ports` ansehen.

## Ein wiederverwendeter Prozessplatz erbt alte Marken

**Symptom:** Nach dem Übersetzen im Editor bekam das gestartete Programm
keine einzige Taste. Aus der Textkonsole lief dasselbe Programm normal.
**Ursache:** `p_bg[pid]` („im Hintergrund gestartet", bekommt absichtlich
keine Tastatur) wurde gesetzt, aber nie gelöscht. Der Compiler läuft im
Hintergrund und gibt seinen Platz frei; die danach gestartete Kommandozeile
bekam denselben Platz **samt alter Marke**.
**Behoben in:** `system/proc.c`, `proc_start()` setzt `p_bg[i] = 0`.
**Merke:** Wer einen Platz wiederverwendet, muss *alle* Felder
zurücksetzen — nicht die meisten. Und: wenn Tasten „nicht ankommen", erst am
Tastenpuffer messen (`BDA_KEYHEAD` / `BDA_KEYTAIL`). Wächst `tail`, aber
`head` steht still, dann liegt es nicht an der Tastatur, sondern daran, dass
niemand liest.

## Wer nebenläufig malt, muss beim Malbefehl prüfen, nicht in der Schleife

**Symptom:** Der Schreibtisch malte seine Fenster ins Bild eines laufenden
Vollbildspiels.
**Ursache:** Die Prüfung „hat ein Programm den Schirm?" stand am Anfang der
Hauptschleife. Schaltet das Programm mitten in einer Runde um, läuft der
Rest dieser Runde trotzdem durch. Auch ein Wächter am Anfang von
`draw_desktop()` reicht nicht — die Funktion malt viele Fenster
nacheinander, und das Umschalten passiert mittendrin.
**Behoben in:** `system/gui.c`, `gui_fremd` wird in `g_fill`, `g_frame` und
`g_char` selbst geprüft.
**Merke:** Bei zwei nebenläufigen Malern gehört die Prüfung so weit nach
unten wie möglich — an die Stelle, die tatsächlich schreibt.

## Ein `#include` **im Kommentar** hat echten Quelltext gefressen

**Symptom:** `fs_read_lib` und Systemaufruf 33 verhielten sich, als gäbe es sie
nicht — jeder Aufruf lieferte −1. Direkt daneben stehender, offensichtlich
richtiger Code hatte keinerlei Wirkung. Eine Messung zeigte, dass sogar
`fs_find_in("SOURCE", 0 - 1)` fehlschlug, obwohl derselbe Aufruf mit einer
Zeichenkette *aus einem Programm* den Ordner problemlos fand.

**Ursache:** Der Präprozessor arbeitet zeilenweise und prüfte nur, ob eine
Zeile mit `#` beginnt. In `syscall.c` stand dieser Kommentar:

```c
/* Datei lesen mit Suchpfad: aktueller Ordner, dann \SOURCE. Fuer
   #include im Compiler auf dem Geraet. */
if (fn == 33) return fs_read_lib((char*)a1, a2, a3);
```

Die zweite Zeile beginnt (nach Leerzeichen) mit `#` → der Präprozessor hielt
sie für eine Anweisung und **ersetzte sie durch eine leere Zeile**. Damit war
das schließende `*/` weg, der Kommentar blieb offen und verschlang alles bis
zum nächsten `*/` — also den `if (fn == 33)` und den Anfang des nächsten
Blocks. Die Klammernzählung zeigte es dann eindeutig: die Funktion `syscall`
endete an einer Stelle, an der im Quelltext noch gar kein Ende steht.

**Behoben in:** `tools/tcc.py` (`zeilen_im_kommentar()` — merkt sich für jede
Zeile, ob sie mitten in einem Blockkommentar anfängt; beide Durchgänge,
`#include` und `#define`, überspringen solche Zeilen) und genauso in
`programs/cc.c` (`komm_folge()`), damit der Compiler auf dem Gerät denselben
Schutz hat.

**Merke:** Ein zeilenweiser Präprozessor darf niemals Zeilen löschen, ohne zu
wissen, ob sie in einem Kommentar stehen. Und: Wenn Code „keine Wirkung hat",
obwohl er offensichtlich richtig ist, zuerst nachzählen, ob der Compiler ihn
überhaupt sieht — Klammern zählen ist billiger als tagelang die Logik zu
prüfen.

## Der Prozessumschalter muss R0 sichern

**Symptom:** Programme rechnen falsch oder stürzen ab, sobald Multitasking
läuft.
**Ursache:** `sched_irq_asm` sicherte r1–r14. Der Compiler benutzt aber **r0**
als Arbeitsregister für *jeden* Ausdruck. Jeder Timerinterrupt zerstörte also
mitten in der Rechnung ein Zwischenergebnis.
**Behoben in:** `system/start.asm` (15 statt 14 Register) und `proc.c`
(Stackaufbau neuer Prozesse).

## Wer im Systemaufruf hängenbleibt, muss `sti` machen

**Symptom:** Nach dem Start eines Hintergrundprogramms fror das ganze System
ein. `p_switches` blieb stehen.
**Ursache:** Ein Programm beendet sich mit `int 0x40`. Beim Interrupt sperrt
die CPU die Interrupts; freigegeben werden sie erst durch `iret`. `proc_exit()`
kehrt aber nie zurück, sondern wartet in einer Schleife — **mit gesperrten
Interrupts**. Kein Timer mehr, kein Scheduler, tot.
**Behoben in:** `system/proc.c`, `asm("sti")` vor der Warteschleife.
**Merke:** Jede Funktion, die aus einem Interrupt heraus nicht zurückkehrt,
muss die Interrupts selbst freigeben.

## `funktion()[i]` skalierte den Index falsch

**Symptom:** Der Python-Tokenizer schrieb in fremden Speicher.
**Ursache:** `tools/tcc.py` kannte den Rückgabetyp von Funktionen nicht und
nahm für `f()[i]` immer 4 Byte Elementgröße — auch wenn `f()` ein `char*`
liefert.
**Behoben in:** `tcc.py`, `self.func_types` und `type_of()` für `call`.

## Der Sektorzähler der Platte war 8 Bit breit

**Symptom:** Ab ~128 KB Programmgröße „Invalid opcode" beim Start.
**Ursache:** `PORT_DISK_COUNT` maskierte mit `0xFF`. Bei 327 Sektoren wurden
71 geladen, der Rest war Müll.
**Behoben in:** `hardware/devices.py`, jetzt 16 Bit.

## Tastatureingabe hinkte einen Anschlag hinterher

**Symptom:** `w` tippen → nichts, `i` tippen → `w` erscheint.
**Ursache:** `event.unicode` bei `KEYDOWN` ist bei SDL je nach Layout leer;
das Zeichen kommt erst mit dem folgenden Text-Ereignis.
**Behoben in:** `pc.py` — Zeichen über `pygame.TEXTINPUT`, Sondertasten über
`KEYDOWN`.

## Das Fenster darf nicht auf die CPU warten

**Symptom:** Oberfläche wird zäh, wenn ein Programm rechnet.
**Ursache:** CPU-Emulation und Zeichnen liefen in derselben Schleife ohne
Zeitgrenze. Die Emulation schafft 1,5–3,5 Mio Befehle/s; bei 2 MHz Solltakt
fraß sie das ganze Bild.
**Behoben in:** `machine.run_slice(dt, max_ms)` — höchstens 8 ms echte
Rechenzeit je Bild, in Häppchen von 4000 Befehlen (gröber greift die Frist
nicht genau genug). Reicht es nicht, läuft die virtuelle Uhr langsamer.

## Textprogramme und Oberfläche teilen sich keinen Bildschirm

**Symptom:** „Run" im File Manager → Desktop reagiert nicht mehr.
**Ursache:** Das Programm lief im Hintergrund, schrieb in den unsichtbaren
Textbildspeicher und fing mit `getkey()` alle Tasten ab — auch ESC.
**Gelöst:** `gui_ausfuehren()` verlässt den Grafikmodus, lässt das Programm
sichtbar laufen und kehrt danach zurück (wie Windows 3.1 mit DOS-Programmen).

## Terminalprozess muss beim Verlassen sterben

**Symptom:** Nach dem Desktop war die normale Kommandozeile stumm.
**Ursache:** Der cmd-Prozess lief weiter, `term_aktiv` blieb 1 — alle
Ausgaben landeten im unsichtbaren Fensterpuffer.
**Behoben in:** `gui.c`, Prozess beenden und `term_aktiv = 0` beim Verlassen
und beim Schließen des Fensters.

## Ein Interrupt kann für mehrere Ereignisse stehen

Der Interruptcontroller hat je Quelle **ein Bit**. Treffen zwei Ereignisse
ein, bevor der Handler läuft, gibt es trotzdem nur einen Interrupt. Ein
Handler, der genau *ein* Ereignis abholt, verliert deshalb das zweite —
bis zufällig ein weiteres nachkommt.

Dieser Fehler ist mir in diesem Projekt **zweimal** passiert:

- **Timer**: Uhr lief zu langsam (unten ausführlich)
- **Tastatur**: `irq_kbd` holte eine Taste je Interrupt. Im BIOS-Setup
  passierte beim ersten Pfeil nichts, der nächste Druck führte dann die
  vorige Bewegung aus. Behoben, indem der Handler den Baustein in einer
  Schleife leerräumt, solange `P_KBD_STATUS` etwas meldet

**Regel:** Ein Interrupthandler fragt den Baustein, *wie viel* anliegt — er
nimmt nie an, dass es genau eins ist.

## Der Timer-Tick darf nicht selbst gezählt werden

**Symptom:** Uhr lief zu langsam, Wartezeiten zu lang.
**Ursache:** Mehrere Ticks pro Zeitscheibe setzen nur *ein* Interrupt-Bit; der
Handler zählte aber nur um eins hoch.
**Behoben in:** `firmware/bios.asm` — der Handler liest den Zählerstand
direkt vom Baustein (`in r1, P_TIMER_TICKS`).

## `gui_running` wurde nur beim Menüpunkt „Exit" zurückgesetzt

**Symptom:** Nach dem ersten Verlassen des Desktops mit ESC startete `WIN`
den Schreibtisch nie wieder — der Selbsttest fiel von 41 auf 39.
**Ursache:** Der neue Schutz gegen einen zweiten Desktop fragt `gui_running`
ab. Aus der Hauptschleife kommt man aber auch mit ESC (`break`) heraus, und
dort blieb die Variable auf 1 stehen.
**Behoben in:** `gui.c`, `gui_running = 0` am **Ende von `gui_main()`**.
**Merke:** Ein Zustandsmerker gehört an die Stelle, an der der Zustand
tatsächlich endet — nicht an jeden einzelnen Ausgang.

## Der Ausschnitt sprang beim Blättern sofort zurück

**Symptom:** Mausrad im Editor bewegte nichts.
**Ursache:** `app_editor` führt den Ausschnitt bei jedem Zeichnen der
Schreibmarke nach. Das Rad verschob `edg_top`, der nächste Bildaufbau zog es
wieder zurück.
**Behoben in:** `gui.c`, `edg_folgen` — Rad aus, Tippen und Klicken ein.

## Der Kernel ist in seine eigenen Puffer hineingewachsen

**Symptom:** Die Dateiverwaltung zeigte statt `PROGS` und `SOURCE` plötzlich
`@`, `ager` und `Filem` an. Die Kommandozeile (`DIR`) war dagegen richtig.
**Ursache:** Die festen Puffer lagen ab `0x30000`, direkt hinter dem Kernel.
Als der Kernel über 128 KB wuchs, überschrieb er das Verzeichnis im RAM mit
seinen eigenen Daten — `wtitle` („File Manager") und `gui_pfad` („A:\")
standen mitten in der Verzeichnistabelle. `DIR` las neu von der Platte und
sah deshalb nichts davon.
**Behoben in:** `fs.c` und `edit.c` — Puffer nach `0xB0000` verlegt; `build.py`
bricht jetzt ab, wenn der Kernel bis dorthin reicht.
**Merke:** Wer eine Größengrenze anhebt (hier: 255 → 511 Sektoren), muss
prüfen, ob die **RAM**-Aufteilung das auch hergibt. Die Platte war nicht das
Limit — der Speicher war es.

## Ein Programm darf nicht im Kernel schlafen gehen

**Symptom:** Der Taschenrechner fror beim ersten Klick ein, die ganze
Maschine stand.
**Ursache:** `sleep()` (und damit `beep()`) wartet mit `hlt` auf den Timer.
Ruft ein Programm das über `INT 0x40` auf, sind die Interrupts gesperrt — der
Timer kommt nie, das `hlt` wacht nie auf. Derselbe Fehler wie oben bei
`proc_exit()`.
**Behoben in:** `lib.c`, `asm("sti")` am Anfang von `sleep()`.

## Klicks landeten im falschen Fenster

**Symptom:** Auf einen Knopf im vorderen Fenster geklickt — reagiert hat das
Fenster darunter.
**Ursache:** Gezeichnet wird nach Stapelreihenfolge (`win_top` zuletzt),
geprüft wurde aber stur nach Fensternummer rückwärts. Lag das vorderste
Fenster auf einem Platz mit kleinerer Nummer, gewann das falsche.
**Behoben in:** `gui.c` — erst `win_top` prüfen, dann den Rest.

## `continue` sprang am Neuzeichnen vorbei

**Symptom:** Startmenü → *Editor*: das Fenster wurde geöffnet (der Knopf in
der Leiste erschien), aber der Bildschirm zeigte weiter das Menü.
**Ursache:** Der Menüzweig setzt `neu = 1` und macht `continue` — das
`if (neu) draw_desktop()` steht aber am **Schleifenende**. Beim nächsten
Durchlauf wird `neu` sofort wieder auf 0 gesetzt.
**Behoben in:** `gui.c`, der Zweig zeichnet selbst.

## Ein Hintergrundprogramm klaute die Tastatur

**Symptom:** Nach `START BENCH.TBX /B` kam von `TASKLIST` nur `ASKLIST` an.
**Ursache:** `getkey()` liest den globalen Tastaturpuffer. Wer zuerst fragt,
gewinnt — auch ein Programm im Hintergrund.
**Behoben in:** `syscall.c` — mit `/B` gestartete Prozesse bekommen `p_bg = 1`
und werden bei `getkey()` schlafen gelegt statt bedient.

## `START X.TBX ARG /B` lief im Vordergrund

**Symptom:** `START CRASH.TBX COLORS /B` blockierte die Kommandozeile, es kam
nicht einmal die Meldung „Started in background".
**Ursache:** `cmd_start` prüfte nur das **zweite Wort** auf `/B`. Stand ein
Argument davor, war `/B` nur noch ein Argument.
**Behoben in:** `kernel.c` — alle Wörter werden durchgegangen, `/B` darf
überall stehen und fällt aus der Argumentliste heraus.

## Doppelklick auf ein Programm im Schreibtischordner scheiterte

**Symptom:** `'CALC.TBX' is not recognized as a command or program.` im
Terminalfenster — obwohl das Symbol sichtbar auf dem Schreibtisch lag.
**Ursache:** Das Fenster tippt den Dateinamen in die Shell, und deren
Suchpfad ist *aktueller Ordner → `\SYSTEM` → `\PROGS`*. Die Datei lag in
`\DESKTOP`, der Prompt stand in `A:\PROGS`.
**Behoben in:** `gui.c`, `eintrag_oeffnen` setzt `cwd` auf den Ordner der
Datei, bevor es den Befehl abschickt.

## Bauen loeschte die Dateien des laufenden PCs

**Symptom:** Colin übersetzt Programme im PC, schließt den Emulator, startet
neu — alles weg.
**Ursache:** `build.py` las das **ganze** Plattenabbild ein, tauschte
Bootsektor und Kernel aus und schrieb alles zurück. Lief nebenher der
Emulator (der seine Sektoren sofort in dieselbe Datei schreibt), überschrieb
der Rückschreibvorgang dessen Dateien mit dem alten Stand von vor dem Bauen.
Genau dasselbe galt für `tools/tbfs.py`, dessen `save()` das Abbild komplett
hinausschrieb.
**Behoben in:** `build.py` schreibt jetzt **nur Sektor 0 und die
Kernelsektoren** (`r+b`, gezielte `seek`s) und fasst das Dateisystem ab
Sektor 512 gar nicht mehr an. `tbfs.py` merkt sich in `self.dirty`, welche
Sektoren es geändert hat, und schreibt ausschließlich diese zurück.
**Merke:** Ein Werkzeug, das eine Datei ändert, die ein anderes Programm
offen hat, darf sie nie komplett neu schreiben — nur die Stellen, die es
wirklich betrifft.

## Am Takt drehen hilft nichts, wenn der Wirt die Bremse ist

**Symptom:** „Können wir die CPU auf mehr als 8 MHz bringen?"
**Befund:** Die Emulation schaffte 1,7 Mio Befehle/s — **21 %** der
eingestellten 8 MHz. Eine größere Zahl im BIOS hätte nur die Anzeige
verändert.
**Gelöst:** Erst messen, dann optimieren. `hardware/cpu.py` (32-Bit-Sicht auf
den Speicher, Kette nach gemessener Häufigkeit, faules Dekodieren, lokale
Variablen statt `self.x`) und `pc.py` (Zeitbudget statt fester 8 ms) —
zusammen etwa **3,4×** mehr Durchsatz im Fenster.
**Merke:** Wer am Emulator schraubt, misst vorher mit `tools/opstat.py` und
prüft danach mit Selbsttest **und Bootstrapping** — letzteres vergleicht zwei
selbst erzeugte Compiler Byte für Byte und findet jeden Rechenfehler der CPU.

## Der Setup-Zustand lag im Zahlen-Kritzelblock

**Symptom:** Beim Wechsel auf den Reiter *Security* füllte sich der ganze
Bildschirm mit Nullen.
**Zwei Ursachen auf einmal**, beide lehrreich:

1. Ich hatte den aktiven Reiter in `BDA_SCRATCH` abgelegt — genau dort
   formatiert `vid_putn` aber seine Ziffern hin. Nach der ersten ausgegebenen
   Zahl war der Reiter Datenmüll und die Zeichenschleife lief endlos.
   Behoben: eigener Platz `SETUP_TAB`/`SETUP_ROW`/`SETUP_SAVE` ab `0x600`.
2. `vid_puthex` erwartet die **Stellenzahl in `r3`** — die hatte ich nicht
   gesetzt, also lief die Ziffernschleife über zufälligen Registerinhalt.

**Merke:** Wer einen fremden BIOS-Dienst aufruft, sieht sich seine Signatur
an. Und ein Zwischenspeicher, der „gerade frei aussieht", gehört meist schon
jemandem.

## Zeilenzahl in einem Kratzregister gehalten

**Symptom:** Endlosschleife beim Zeichnen des Setups.
**Ursache:** Ich hatte die Anzahl Zeilen in `r11` gehalten. `r10`–`r12` sind
laut [[05 Konventionen]] **Kratzregister** — jeder Unterprogrammaufruf darf
sie zerstören, und `vid_hline` tat das auch prompt.
**Behoben:** Wert sofort nach dem Holen vergleichen, nicht zwischenlagern.

## Kleinigkeiten, die trotzdem Zeit kosten

- **Zeichensatz kennt nur 32–127.** Blockzeichen (219, 176) im Grafikmodus
  selbst als Rechtecke malen, sonst erscheinen Balken nicht.
- **Fenster können aus dem Bild ragen** — `starte()` begrenzt die Position,
  neue Fenstergrößen trotzdem prüfen.
- **Zahlen ohne Hintergrund überlagern sich** beim Auffrischen. Entweder
  `bg` setzen oder das ganze Fenster neu zeichnen.
- **`#include` wird auch in Kommentaren gefunden** — von `tools/tcc.py` *und*
  von `cc.c`, bei letzterem auch eingerückt. `gfxlib.c` hatte in seinem
  Kopfkommentar ein Beispiel `#include "gfxlib.c"` stehen und band sich damit
  selbst ein: neun Syntaxfehler in einer Zeile, die es gar nicht gab.
- **Der Anfasser zum Ziehen muss nach dem Fensterinhalt gezeichnet werden**,
  sonst malt die Anwendung ihn zu.
- **Beschriftungen kürzen, nicht nur die Mitte rechnen.** In der Startleiste
  stand „Compiling" (9 Zeichen = 72 Punkte) in einem 64 Punkte breiten Knopf
  und ragte links und rechts heraus; bei den Schreibtischsymbolen lief der
  Name aus dem Bild. `g_button` zentriert nur — es kürzt nichts.
- **Zahl über Beschriftung gemalt.** Im Uhrfenster begann die Betriebszeit
  bei `x+36`, das Wort „Up time" reichte aber bis `x+56` — die Ziffern lagen
  im Text. Beschriftung links, Werte in einer festen Spalte, dann passiert
  das nicht.
- **Rollen ohne obere Grenze.** `if (top < 0) top = 0;` allein reicht nicht —
  ohne `if (top > anzahl - zeilen)` scrollt man endlos ins Leere. Beide
  Grenzen, immer.
- **Feste Zeilenzahlen in Listen** halten nur, bis der Ordner voll genug ist.
  Die Dateiverwaltung zeigte hart 11 Einträge ohne Blättern — die 14 Dateien
  in `\SOURCE` passten nicht, und die fehlenden sahen aus, als gäbe es sie
  gar nicht. Jede Liste braucht Zeilenzahl aus der Fenstergröße **und** einen
  Ausschnitt zum Blättern.
- **Tastaturpuffer läuft über**, wenn Testskripte während langer Rechenläufe
  weiter tippen. In Tests auf den Prompt warten (`tools/bootstrap.py` macht es
  richtig).
- **Der Aufbau von TBFS steht jetzt an vier Stellen** — `system/fs.c`,
  `tools/tbfs.py`, `system/boot.asm` und `firmware/setup.asm`. Die beiden
  letzten sind Absicht und nicht wegzukürzen: der Bootsektor kann keine
  BIOS-Routine aufrufen, und die Firmware läuft, bevor es einen Bootsektor
  gibt. Wer Sektornummern oder Feldabstände verschiebt, muss **alle vier**
  anfassen — sonst startet nichts mehr, und die Meldung zeigt auf den Kernel
  statt auf das Dateisystem.
- **Ein Semikolon in einer Zeichenkette schnitt die halbe Zeile ab.** Der
  Assembler warf Kommentare mit `zeile.split(";")[0]` weg, ohne auf
  Anführungszeichen zu achten. Aus
  `.db "A bad image is refused; keeps a backup", 0` wurde stillschweigend
  `.db "A bad image is refused` — Text ohne Ende, ohne Nullbyte, ohne
  Fehlermeldung, und die Ausgabe lief in die nächste Zeichenkette weiter.
  Behoben in `tools/assembler.py` (`ohne_kommentar`), aber die Lehre bleibt:
  **naives Kommentar-Abschneiden ist ein Textzerstörer.**
- **`vid_puthex` braucht die Stellenzahl in `r3`.** Vergessen heißt: der Wert
  wird hunderte Male gedruckt, bis der ganze Bildschirm voll ist. Sieht aus
  wie eine Endlosschleife, ist aber ein fehlendes Argument.
- **Ein `putc` ohne Steuerzeichen macht die Rücktaste sichtbar.** Colins
  erstes eigenes BIOS behandelte nur `\n`. Die 8, die `readline` zum Löschen
  schickt, landete deshalb als Zeichen im Bildspeicher — CP437 stellt sie als
  „◘" dar. Bei jedem Druck kam ein Kästchen dazu, der Text blieb stehen.
  Das Tückische: der Puffer im Speicher war die ganze Zeit richtig, ENTER
  führte brav den leeren Befehl aus. **Nur der Bildschirm log.** Wer eine
  Ausgabefunktion neu schreibt, muss 8, 9, 10 und 13 abfangen, bevor er ein
  Zeichen ablegt.
- **Text mitten im Code muss auf vier Byte aufgefüllt werden.** Die neue
  Flash-Rückfrage brachte 308 Byte Zeichenketten mitten ins BIOS — eine Zahl
  ohne Rest durch 4. Jeder Befehl danach lag schief, und der Rechner starb
  15 Befehle nach dem Reset, noch vor jedem Bild. Der TB-32 hat feste
  4-Byte-Befehle: hinter `.db` gehört ein `.align 4`, sobald wieder Code
  folgt. Bisher standen alle Texte am Dateiende, deshalb ist es nie
  aufgefallen.
- **`#define NAME wert /* Kommentar */` nahm den Kommentar in den Wert.**
  Wer `NAME` dann irgendwo in einem Kommentar erwähnte, bekam ein `*/`
  hineingesetzt — der Kommentar endete dort, und die Prosa dahinter wurde
  als Quelltext gelesen. Der Fehler zeigte auf eine völlig harmlose Zeile.
  Behoben in `tools/tcc.py`; `cc.c` war nie betroffen, es speichert
  `#define`-Werte als Zahl.
- **`s[:i] + neu + s[j:]` mit `j == -1` verschluckt die halbe Datei.**
  `find` liefert −1, wenn es nichts findet, und `s[-1:]` ist das letzte
  Zeichen. So habe ich `programs/asm.c` von 646 auf 434 Zeilen gekürzt und
  musste den Rest neu schreiben. Bei jedem `find` prüfen, ob es −1 ist.
- **Die Klicksuche muss dieselbe Reihenfolge haben wie das Malen.**
  `draw_desktop()` malt die Fenster nach Nummer (0, 1, 2 …) und `win_top`
  zuletzt — wer die höhere Nummer hat, liegt sichtbar weiter vorn. Die
  Klicksuche lief aber **vorwärts** und nahm den ersten Treffer, also das
  Fenster *dahinter*. Colin konnte den Command Prompt nicht mehr anklicken,
  sobald ein Fenster mit kleinerer Nummer darunterlag. Jetzt läuft sie
  rückwärts. **Merke:** Zeichenreihenfolge und Trefferreihenfolge sind
  dasselbe Wissen — sie gehören zusammen geändert.
- **Eine Prüfsumme muss messen, was wirklich startet.** Als der Kernel vom
  festen Sektor 1 in die Datei `\SYSTEM\KERNEL.BIN` wanderte, hätte Secure
  Boot weiter die alten Sektoren gerechnet: eine Prüfung, die nie anschlägt.
  Das ist schlimmer als gar keine, weil es nach Sicherheit aussieht.
  `secure_summe` sucht deshalb dieselbe Datei wie der Bootsektor.

Verwandt: [[04 Compiler TCC Grenzen]], [[06 Bauen und Testen]]
