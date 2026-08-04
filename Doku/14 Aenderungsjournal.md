# Änderungsjournal

Jede Änderung, jeder gefundene Fehler und jede neue Funktion — **neueste
Einträge oben**. Bei Fehlern steht die *Ursache* dabei, nicht nur das
Symptom; das Symptom hilft beim nächsten Mal nicht weiter.

Die tiefer liegenden Fallen haben zusätzlich einen ausführlichen Eintrag in
[[07 Fallstricke]].

---

## TOOBAD-OS 2.5.2

Aus 1.0 wird **2.5.2** -- System und BIOS gemeinsam, an allen sechs Stellen
(Startbild, Kopfzeile, `ver`, `about`, Setup, Kopf des BIOS-Abbildes).

**Beinahe eine Falle:** das Namensfeld im Kopf ist **genau 32 Byte** lang.
`TOOBAD BIOS v2.5.2` ist vier Zeichen laenger als vorher, und mit dem alten
`.space 17` waere der Kopf 36 Byte gross geworden -- der Code haette bei
0x34 statt 0x30 begonnen und der Rechner waere gar nicht mehr gestartet.
Jetzt `.space 13`, nachgerechnet: 19 + 13 = 32. Geprueft: Name wird gelesen,
Code steht wieder bei 0x30, Abbild gueltig.

**Und der Coder ruckelte.** `edg_ist_bios()` durchsuchte bei JEDEM
Neuzeichnen 3000 Byte -- rund 30.000 Befehle, waehrend ein ganzes Bild bei
2 MHz nur etwa 33.000 hat. Allein diese Suche ass die Bildzeit auf. Jetzt
wird das Ergebnis gemerkt und nur bei geaenderter Laenge neu gesucht, und
nur in den ersten 400 Byte -- weiter hinten darf die Kennung ohnehin nicht
stehen.

---

## Klicks, Fenstergrenzen und ein abbrechbares "New"

**Zwei weitere Ueberlaeufe, von Colin auf Bildern gezeigt:**

- Im Dateidialog ragte **`Cancel`** aus seinem Knopf: sechs Zeichen sind
  48 Punkte Text, der Knopf war 44 breit. `g_button` zentriert nur, es
  kuerzt nichts. Jetzt nachgerechnet statt geschaetzt: 8 Punkte Rand,
  Cancel 56, OK 44 -- und das Namensfeld entsprechend kuerzer. Zeichnen und
  Klicken auf dieselben Zahlen umgestellt.
- In der Statuszeile des Coders stand das gruene **`saved`** ab fester
  Spalte 560. Bei 588 Punkten Platz sind das drei Zeichen -- der Rest lief
  unter dem `?`-Knopf hindurch aus dem Fenster. Jetzt rechtsbuendig vor dem
  Knopf, und die Byte-Zahl weicht, solange eine Meldung steht. Beide teilen
  sich denselben Platz, wie es im Kommentar schon immer stand.

Ein Skript haelt jetzt ausserdem jede feste Knopfbeschriftung gegen ihre
Breite -- danach war keine weitere zu breit.

**Der Command Prompt liess sich nicht mehr anklicken**, sobald ein Fenster
dahinter lag. Ursache: `draw_desktop()` malt nach Fensternummer und
`win_top` zuletzt -- die hoehere Nummer liegt also vorn. Die Klicksuche lief
dagegen **vorwaerts** und nahm den ersten Treffer, mithin das Fenster
DAHINTER. Jetzt laeuft sie rueckwaerts, in derselben Reihenfolge wie das
Malen. Nachgestellt: drei Fenster, das mittlere oben, Klick in die
Ueberlappung der beiden anderen -- vorher gewann das hintere, jetzt das
sichtbar vordere.

**Text ragte aus den Meldungsfenstern heraus.** Compilerzeilen sind gut
50 Zeichen lang, das Fortschrittsfenster war 320 Punkte breit. Neu ist
`g_text_max()`, das auf die Fensterbreite kuerzt und mit zwei Punkten endet;
benutzt in den Compilermeldungen, im Hilfefenster und in der
Firmware-Rueckfrage. Und aus dem schmalen Balkenfenster wird beim Fehler ein
richtiges Meldungsfenster (520x240 statt 320x90).

**Eine falsche Klickflaeche gefunden**, maschinell: ein Skript hat alle
`g_button()` gegen alle `treffer()` gehalten. Der Knopf *Save to CMOS* im
Control Panel ist 96 Punkte breit, geprueft wurde aber **nur die Zeile, kein
`mx`** -- ein Klick auf die Temperaturanzeige daneben schrieb das CMOS.

**"New" laesst sich jetzt abbrechen.** Vorher legte es sofort ein leeres
Blatt an und fragte erst danach nach dem Platz; wer abbrach, hatte seine
Arbeit verloren und eine namenlose Datei offen. Jetzt fragt es zuerst, und
das Dokument entsteht erst, wenn ein Platz gewaehlt ist -- in Paint, Word
und im Coder. Im Coder geht das Schreibfenster ohne Platz gar nicht erst
auf, die Startseite bleibt stehen.

---

## Neue Datei fragt sofort nach dem Platz

Colins Wunsch: beim Anlegen einer neuen Datei -- in Paint, Word oder im
Coder -- kommt gleich das Fenster fuer den Speicherort. Danach ist man
"direkt drin", und "Save" speichert einfach den neuen Stand.

Genau so ist es jetzt. `New` oeffnet den Dateidialog im Speichern-Modus mit
einem Vorschlag (`PICTURE.TBI`, `DOCUMENT.TBW`, `NEW.C` ...). Ab da merkt
sich das Programm, dass Name und Ordner feststehen (`pt_ort`, `wd_ort`,
`edg_ort`), und `Save` schreibt ohne weitere Nachfrage.

Auch nach dem **Oeffnen** steht der Platz fest -- danach speichert `Save`
dorthin zurueck, statt jedes Mal wieder zu fragen. Vorher fragte er
ausnahmslos immer, was bei jedem zweiten Tastendruck ein Fenster aufgehen
liess.

---

## Der TB-32 baut jetzt seine eigene Firmware

Colins Wunsch: im Coder ein BIOS schreiben, mit Vorlage und Anleitung,
einmal testen ohne Risiko, und erst wenn es passt dauerhaft flashen -- mit
Rueckfrage vom BIOS selbst.

**Der Blocker sass tiefer als gedacht.** `ASM.TBX` konnte kein BIOS bauen:
kein `.org`, kein `.equ`, kein `.include`, 256 Symbole (const.inc allein hat
158). Dazu kamen beim Ausprobieren drei Sachen heraus, die niemand geahnt
hatte:

- **Ausdruecke wurden an Leerzeichen zerschnitten.** `next_token()` lieferte
  von `IVT_BASE + IRQ_TIMER*4` nur `IVT_BASE`; der Rest fiel lautlos weg und
  die Interrupttabelle stand voller Nullen. Jetzt holt `next_arg()` das
  ganze Argument bis zum Komma.
- **Punkt vor Strich fehlte**, und Klammern auch -- `(SCR_H-1)*SCR_W*2/4`
  kommt in video.asm wirklich vor.
- **Lokale Marken hatten keinen Gueltigkeitsbereich.** `.copy` war ueberall
  dieselbe; der Assembler uebersetzte anstandslos, und die Spruenge landeten
  in einer anderen Funktion. Genau die Sorte Fehler, die erst auffaellt,
  wenn ein fertiges BIOS nicht mehr startet.

Dazu `ldwa`/`stwa` und 512 Symbole. **Ergebnis:** das Geraet baut
`minimal.asm` zu 3356 Byte -- **Byte fuer Byte dasselbe wie der Mac**, bis
auf die acht Kopfbytes, die der Coder selbst stempelt.

**Neu im Coder:** *New -> BIOS* mit fertiger Vorlage, ein `?`-Knopf mit der
Kurzfassung von [[16 Eigenes BIOS schreiben]] auf dem Geraet, und unten
**Test** und **Flash**.

**Der Einmal-Start** ist der Kern. Das Abbild liegt im Board, nicht auf der
Platte: der naechste Start nimmt es, der uebernaechste wieder den echten
Chip. Im Startbild steht `TEST IMAGE -- runs once`. Ein Testabbild, das
haengenbleibt, kostet damit einen Druck auf Strg+R und sonst nichts.

**Beim dauerhaften Flashen fragt die Firmware selbst**, in Rot, vor dem
Selbsttest. Der Coder kann ein Abbild nur anmelden -- ein Programm darf
nicht allein entscheiden, dass der Chip ueberschrieben wird.

**Vier Fehler auf dem Weg, alle in [[07 Fallstricke]]:**

1. `#define NAME wert /* Kommentar */` nahm den Kommentar in den Wert. Wer
   `NAME` in einem Kommentar erwaehnte, bekam ein `*/` hineingesetzt und der
   Rest wurde als Quelltext gelesen. Behoben in `tools/tcc.py`.
2. **Ich habe `programs/asm.c` selbst zerstoert** -- `s[:i] + neu + s[j:]`
   mit `j == -1` schnitt die Datei von 646 auf 434 Zeilen. Kein Backup, kein
   git. Der fehlende Teil wurde aus `tools/assembler.py` und `hardware/isa.py`
   neu geschrieben; die Befehlstabelle war zum Glueck heil.
3. 308 Byte Zeichenketten mitten im BIOS -- ohne `.align 4` lag jeder Befehl
   danach schief, und der Rechner starb 15 Befehle nach dem Reset.
4. Die C-Fassung des Emulators meldete auf die Frage "liegt ein Flashwunsch
   an?" eine 2 statt 0 -- und zeigte beim Start die rote Rueckfrage, ohne
   dass jemand etwas angemeldet hatte.

**Nachtrag nach Colins Blick auf den fertigen Coder:** die BIOS-Vorlage war
auf Deutsch (`einstieg`, `hochfahren`, `; BIOS-Daten leeren`) -- sie steht
auf dem Bildschirm des TB-32 und gehoert damit auf Englisch. Uebersetzt,
Marken inklusive. Beim Suchen sind noch sechs weitere deutsche Texte
aufgefallen: `"Datei"` als Fenstertitel des Dateidialogs, `-- Taste
druecken --` in lib.c und vier in Word (`Bild einfuegen`, `Bild loeschen`,
`Als Text speichern`, `Seite`). Alle englisch.

**Die Knopfleiste richtet sich jetzt nach der Art des Quelltextes.** Erst
hatte ich nur zwischen BIOS und Rest getrennt; Colin hat den schaerferen
Schnitt gesehen: eine `.PY` wird gar nicht uebersetzt, die laeuft direkt.
`Build` haette dort den **C-Compiler auf Python-Quelltext** losgelassen --
dabei kann nichts als eine Fehlerliste herauskommen.

| | Build | Run | Test | Flash |
|---|---|---|---|---|
| C / Assembler | ja | ja | -- | -- |
| Python | **nein** | ja | -- | -- |
| BIOS | nein | nein | ja | ja |

Die Firmware wird an der Kennung `TBBI` im Kopf erkannt, die ein BIOS
ohnehin braucht -- so gibt es keine Betriebsart, die man vergessen kann
umzuschalten. Die Knoepfe ruecken zusammen, wenn einer fehlt; Zeichnen und
Klicken fragen dieselbe Funktion `cb_pos()`, damit die beiden nicht wieder
auseinanderlaufen. Nachgerechnet passt die Leiste in allen drei Faellen:
436, 382 und 454 von 588 Punkten.

**Nachtraeglich bestaetigt:** der Suchlauf im Hintergrund hat doch noch
Kopien von `asm.c` gefunden (in `~/Desktop/Projekte/PyPC Kopie` und im
iCloud-Papierkorb). Der Vergleich mit dem Original zeigt: keine Funktion
fehlt, nur `parse_mem` heisst jetzt `mem_operand`, und Kodierung wie
Sprungrechnung sind verhaltensgleich. Zur Sicherheit wurde HELLO.ASM auf
dem Geraet uebersetzt und gegen den Mac gehalten -- **171 Byte, Byte fuer
Byte gleich.**

Tests: **62/62** statt 55/55 -- neu sind Puffer aus dem RAM holen,
Einmal-Start laeuft und der Chip bleibt unangetastet, der Start danach nimmt
wieder das echte BIOS, die Firmware fragt in Rot nach, sie schreibt bis
dahin nichts, und ENTER brennt. Dazu 11/11 Compilertests und beide
Emulatoren wieder Befehl fuer Befehl gleich.

---

## Ein BIOS, das die Ruecktaste nicht kennt -- und der deutsche Papierkorb

**Colins Bild:** `A:\> fff` und dahinter drei graue Kaestchen. Jeder Druck
auf die Loeschtaste legte eins dazu, die Buchstaben blieben stehen.

**Erst nicht reproduzierbar, und das war der Hinweis.** Mit dem grossen BIOS
loescht die Taste sauber -- durch `pc.py` genauso wie kopflos. Erst mit
**seinem** BIOS im Chip kam der Fehler, und zwar auf Anhieb: drei Drucke,
drei Kaestchen.

**Ursache:** Colin hat seine Bildschirmroutinen selbst geschrieben statt
`video.asm` einzubinden -- deshalb ist sein BIOS auch nur 3 KB statt 3,3.
Sein `scr_putc` faengt genau ein Steuerzeichen ab, die 10. Die 8, die
`readline` zum Loeschen schickt, lief in den normalen Zweig und landete als
Zeichen im Bildspeicher. CP437 stellt die 8 als "◘" dar.

**Das Tueckische:** der Puffer im Speicher war die ganze Zeit richtig. Nach
den drei Ruecktasten und ENTER kam keine Fehlermeldung, sondern ein frischer
Prompt -- die Shell hatte brav den leeren Befehl ausgefuehrt. **Nur der
Bildschirm log.**

**Und die Doku war schuld.** In [[16 Eigenes BIOS schreiben]] stand jede
Funktionsnummer und jedes Register -- aber nirgends, dass `putc` 8, 9, 10
und 13 abfangen muss. Colin konnte es nicht wissen. Steht jetzt als eigene
Tabelle drin, und als Fallstrick in [[07 Fallstricke]].

**Behoben in Colins `colinbios.asm`** (auf seine Bitte): Ruecktaste,
Wagenruecklauf und Tabulator dazu. Dabei gleich das Namensfeld im Kopf
nachgetragen -- im Startbild steht jetzt `ColinBIOS 0.2` statt
`UNNAMED BIOS`. Nachgeprueft: drei Ruecktasten, drei Buchstaben weg.

**Der Papierkorb heisst jetzt `\RECYCLED`.** Er hiess `\PAPIERKORB`, und das
war der einzige deutsche Name, der auf dem Bildschirm des TB-32 stand --
gegen die eigene Regel aus [[05 Konventionen]]. Windows 95 nannte seinen
genauso. Der vorhandene Ordner auf Colins Platte wurde mitsamt Inhalt
umbenannt, nicht neu angelegt.

---

## Das Startbild gehoert dem Board -- nur der Name kommt aus dem BIOS

Colin wollte einen festen Ablauf beim Einschalten, **bei jedem BIOS gleich**:
Blau laeuft langsam von oben nach unten, in der Mitte erscheint der Name,
darunter der Hinweis auf DEL, dann faehrt der Rechner hoch. Und der Name
soll der sein, den *jedes BIOS fuer sich* festlegt.

Genau so ist es jetzt -- und die Aufteilung ist der interessante Teil:

| | wer macht es |
|---|---|
| Blau von oben nach unten, Bildmitte, DEL-Zeile, die fuenf Sekunden | **das Mainboard** (`pc.py`) |
| der Name in der Mitte | **das BIOS**, im Kopf seines Abbildes |

**Der Kopf ist von 16 auf 48 Byte gewachsen.** Auf 0x10 stehen jetzt 32 Byte
Name, mit Nullbyte abgeschlossen; der Code faengt bei 0x30 an. Das Board
liest ihn, bevor die CPU ueberhaupt Strom hat (`Machine.rom_name`). Fehlt
das Feld -- so wie in Colins bisherigem ColinBIOS -- zeigt das Board
`UNNAMED BIOS` statt zu raten. Geprueft wird streng: druckbar, mit Nullbyte
beendet, danach nur noch Nullbytes.

**Warum das Bild nicht in die Firmware gehoert:** ein Startbild im BIOS ist
genau dann weg, wenn jemand sein eigenes flasht -- und dann gibt es auch
keine Stelle mehr, an der man DEL druecken koennte. Im Board kann es kein
BIOS verlieren.

Der Ablauf, nachgemessen im echten `pc.py`:

| Zeit | Bild |
|---|---|
| 0,4 s | 7 von 25 Zeilen blau |
| 0,9 s | 18 Zeilen |
| 1,7 s | voll blau, in der Mitte `TOOBAD BIOS v1` |
| 2,4 s | darunter `Press DEL to enter SETUP` |
| 5,0 s | Strom aufs Board |

Mit Colins BIOS steht an derselben Stelle `UNNAMED BIOS` -- alles andere
identisch. Genau das war der Wunsch.

**Die Hinweiszeilen unten links sind weg**, wie gewuenscht. Ausgeschaltet
heisst jetzt wirklich nur schwarz.

**Was weiterhin nicht geht, und zwar grundsaetzlich:** Das Board kann Zeit
verschaffen, aber kein Menue herbeizaubern. Was beim DEL passiert,
entscheidet die Firmware. Colins BIOS hat kein Setup, also tut das DEL dort
nichts -- nachgeprueft. Dafuer braucht es ein Setup im BIOS selbst.

---

## Die Bedenkzeit gehoert ins Gehaeuse, nicht in die Firmware

Colin wollte fuenf Sekunden Bedenkzeit **bei jedem BIOS**, und zwar in
Python hinterlegt. Der Grund dahinter ist gut: eine Bedenkzeit, die in der
Firmware steht, ist genau dann weg, wenn man ein eigenes BIOS flasht -- und
dann kommt man nicht mehr ins Setup. Sein ColinBIOS sprang sofort in den
Bootsektor, und er landete ohne Zwischenstopp in der Konsole.

**Jetzt haelt das Gehaeuse den Rechner fuenf Sekunden an**
(`EINSCHALT_HALT_S` in `pc.py`), bevor ueberhaupt Strom auf das Board kommt.
Kein BIOS kann das ueberspringen, auch nicht aus Versehen. Das gilt fuer den
Programmstart genauso wie fuer den `ü`-Knopf.

**Tasten aus der Bedenkzeit gehen nicht verloren.** Sie werden aufgehoben
und dem Rechner gereicht, sobald die CPU laeuft. Erst dann -- ein
Tastatur-Interrupt bei stehender CPU verpufft, die Taste laege im Baustein
und niemand holte sie ab.

Nachgeprueft mit beiden BIOSen im echten `pc.py`: nach 1 s und nach 4 s
noch aus, nach 6 s laeuft er. Mit dem grossen BIOS oeffnet ein DEL aus der
Bedenkzeit **das Setup**. Mit ColinBIOS passiert nichts -- richtig so, denn
dort gibt es keins. Eine Bedenkzeit kann Zeit verschaffen, aber kein Menue
herbeizaubern.

---

## Ausschalten heisst jetzt aus, und Hochfahren dauert

**Zwei Wuensche von Colin, beide vom selben Gedanken:** der Rechner soll
sich anfuehlen wie einer.

**Der Bildschirm wird beim Ausschalten schwarz.** Vorher blieb das letzte
Bild stehen und darueber lag ein roter Balken "Rechner ausgeschaltet". Das
war praktisch, aber falsch -- ein Monitor an einem ausgeschalteten Rechner
zeigt nichts. Jetzt ist er wirklich schwarz, unten steht nur klein
`aus -- ü = einschalten`.

**Das `ü` ist der Einschaltknopf.** Es wirkt nur, wenn der Rechner aus ist,
und **nicht sofort**: erst bleibt es 1,1 Sekunden dunkel, so wie bei einem
echten Geraet zwischen Knopfdruck und erstem Bild. Waehrend dieser Sekunde
steht auch der Hinweis nicht da -- der Schirm ist einfach schwarz.

**Der Selbsttest laesst sich Zeit.** Das war der eigentliche Punkt: der
ganze POST war nach **16 Millisekunden** fertig. Alle sechs Zeilen standen
schon im ersten Bild -- vom Hochfahren sah man schlicht nichts. Jetzt:

| | vorher | nachher |
|---|---|---|
| POST komplett | 0,02 s | 1,5 s |
| bis zur Eingabeaufforderung | 2,3 s | 3,95 s |
| mit Quick Boot | | 0,58 s |

Der Speichertest **zaehlt sichtbar hoch** -- 512 KB alle 15 Millisekunden,
von 512 bis 16384 -- und zwischen den Pruefungen liegt je eine Fuenftel-
sekunde (`post_pause` in `firmware/bios.asm`).

**Abschaltbar, und zwar ueber die Einstellung, die es dafuer schon gab.**
Bei eingeschaltetem *Quick Boot* entfaellt jede Pause. Genau dafuer ist der
Schalter da, und genau so macht es jedes echte BIOS.

**Fast in eine dokumentierte Falle getreten.** Die `ü`-Abfrage stand
zuerst bei den Tastendruecken (`KEYDOWN`, `event.unicode`) -- und keine
zwanzig Zeilen darueber steht in `pc.py` der Kommentar, dass `unicode` dort
je nach Layout leer ist oder noch das Zeichen des vorigen Anschlags traegt.
Umlaute kommen nur ueber das Text-Ereignis zuverlaessig an. Jetzt steht sie
bei `TEXTINPUT`, wo sie hingehoert.

**Nachgeprueft mit gefaelschten Ereignissen im echten `pc.py`:** laeuft nach
dem Start, ist nach `power_off` aus, bleibt waehrend des Kaltstarts dunkel,
laeuft nach dem `ü` wieder, und bootet erneut bis zur Eingabeaufforderung.
Fuenf von fuenf.

Die Wartezeiten im Selbsttest mussten mit -- er prueft den POST nach 2,0 s
statt 1,2 s und den Bootvorgang nach 3,0 s statt 2,5 s. Danach wieder
**55/55**, und beide Emulatoren rechnen weiterhin Befehl fuer Befehl gleich.

---

## Das BIOS ist austauschbar geworden

**Die Frage davor:** Colin hatte `\SYSTEM\BIOS.BIN` geloescht und wollte
wissen, warum nichts passiert. Antwort: weil das BIOS auf einem Chip liegt
und nicht auf der Platte -- die Datei ist eine sichtbare Kopie. So ist es
auch bei einem echten PC, und so soll es bleiben.

Aber es gibt einen Weg, ein BIOS wirklich zu zerstoeren: **flashen**. Colins
Vorschlag war, es genau so zu machen -- im Setup eine Datei vom echten Mac
aussuchen. Das ist die richtige Variante, und zwar aus einem Grund, der
nicht offensichtlich ist: der Chip ist Hardware. Eine Datei vom Wirt
hereinzureichen heisst "ein anderer Chip kommt aufs Board" -- das darf der
Emulator, weil er das Mainboard ist. Ein Programm *im* TB-32, das den Chip
beschreibt, aus dem es gerade selbst seine Befehle holt, waere die schlechte
Variante.

**Neu -- Setup, Reiter "Firmware".** Zeigt Groesse und Pruefsumme des Chips,
der gerade laeuft, und hat zwei Knoepfe: *Flash BIOS from File* und *Restore
Backup BIOS*. Der erste oeffnet den Dateidialog von macOS
(`osascript`, in `pc.py`).

**Neu -- der Chip als Geraet** (`Flash` in `hardware/devices.py`, Ports
0xB0-0xB2). Absichtlich dumm: er prueft **nichts** und nimmt jedes Byte, wie
ein echter Flash-Baustein. Was taugt und was nicht, entscheidet die
Firmware.

**Neu -- ein Kopf im Abbild.** Die ersten 16 Byte: Sprung, Kennung `TBBI`,
Laenge, Pruefsumme. `build.py` traegt die letzten beiden ein (`bios_kopf_stempeln`).

**Drei Netze, an drei verschiedenen Zeitpunkten:**

1. Beim Flashen prueft die Firmware Kennung und Pruefsumme und schreibt
   sonst gar nicht erst (`bios_pruefen` in `firmware/setup.asm`).
2. Beim Einschalten prueft das **Mainboard** nach und spielt sonst von
   selbst die Sicherung zurueck (`Machine.rom_pruefen`). Das ist Dual BIOS,
   und es sitzt bewusst dort: **eine kaputte Firmware kann sich nicht selbst
   pruefen.**
3. Was formal gueltig ist und trotzdem haengt, holt *Restore Backup BIOS*
   oder `python3 build.py` zurueck.

**Neu -- `firmware/minimal.asm`.** Ein vollstaendiges BIOS in **3324 Byte**
gegen 12216 beim vollen: kein Startbild, kein Setup, kein Secure Boot,
kein Speichertest. Es startet den Rechner, und TOOBAD-OS laeuft darauf --
nachgeprueft. Das ist die Vorlage zum Umbauen. Es geht so klein, weil das
System fast alles selbst macht: Grafik, Blitter, Maus, Ton und Waerme laufen
ueber `inr`/`outr` direkt an die Ports, ganz ohne BIOS.

**Neu -- [[16 Eigenes BIOS schreiben]].** Der vollstaendige Vertrag: die 16
Byte Kopf, die Interruptvektoren, alle vier Dienste mit jeder
Funktionsnummer und jedem Register. Genau die Datei, nach der Colin gefragt
hat.

**Behoben -- ein Semikolon in einer Zeichenkette frass die halbe Zeile.**
Der Assembler warf Kommentare mit `zeile.split(";")[0]` weg, ohne auf
Anfuehrungszeichen zu achten. Aus
`.db "A bad image is refused; keeps a backup", 0` wurde stillschweigend
`.db "A bad image is refused` -- kein Nullbyte, keine Fehlermeldung, und die
Ausgabe lief in die naechste Zeichenkette weiter. Auf dem Bildschirm sah das
aus, als haette sich der Rechner eine Frage selbst gestellt. Behoben in
`tools/assembler.py` (`ohne_kommentar`).

**Behoben -- `vid_puthex` ohne Stellenzahl in `r3`** druckte den Wert
hunderte Male und fuellte den ganzen Bildschirm mit `5B03E1E0`. Sah aus wie
eine Endlosschleife, war ein fehlendes Argument.

**Getestet, nicht behauptet.** Sechs neue Pruefungen im Selbsttest, alle auf
einer **Kopie** des Chips: beschaedigtes Abbild wird abgelehnt und der Chip
bleibt unangetastet, gutes Abbild wird gebrannt, Sicherung wird angelegt,
der Rechner startet mit dem selbst geflashten BIOS, und ein vernichteter
Chip wird beim Einschalten automatisch aus der Sicherung ersetzt.
**55/55** statt vorher 45/45.

Die drei Ports stehen auch in `emu/` -- die C-Fassung kann keinen
Dateidialog oeffnen und meldet immer "keine Datei", aber die Portnummern
muessen dieselben sein. Beide Emulatoren rechnen weiterhin Befehl fuer
Befehl gleich.

---

## Der Bootsektor liest jetzt das Dateisystem

**Die Frage, aus der das wurde:** Colin hatte alle Dateien geloescht, auch
die in `\SYSTEM` -- und der Rechner startete weiter. Zu Recht misstrauisch:
*"sollte er nach einem reboot nicht abstuerzen dann?"*

**Warum er weiterlief.** Der Kernel lag auf festen Sektoren ab 1, ausserhalb
des Dateisystems. Die Kernelgroesse stand im Bootsektor an Position 506, und
mehr brauchte er nicht zu wissen. Die `KERNEL.BIN` in `\SYSTEM` war blosse
Dekoration -- man konnte sie ansehen, kopieren, loeschen, es aenderte nichts.
Ein Betriebssystem, dessen Systemdateien Attrappen sind.

**Jetzt sucht der Bootsektor die Datei wirklich.** `system/boot.asm` liest
das Verzeichnis (Sektor 513..520), sucht den Ordner `SYSTEM` im
Hauptverzeichnis, darin `KERNEL.BIN`, nimmt Startsektor und Groesse aus dem
Eintrag und laedt genau diese Sektoren. Feste Kernelsektoren gibt es nicht
mehr; `build.py` schreibt nur noch Sektor 0.

Nachgeprueft, beides:

- `KERNEL.BIN` geloescht, Neustart → `\SYSTEM\KERNEL.BIN fehlt`, der Rechner
  bleibt stehen. Genau wie ein echter Rechner ohne Betriebssystem.
- `python3 build.py`, Neustart → startet wieder.

**Das Kunststueck sind die 512 Byte.** Ein Verzeichnis durchsuchen, mit
Namensvergleich und Ordnerpruefung, in einem einzigen Sektor: 483 Byte
belegt, 29 frei. Moeglich ist es nur, weil TBFS Dateien **am Stueck** ablegt
-- Startsektor und Groesse genuegen, es gibt keine Blockketten zu verfolgen.
Gespart wurde an zwei Stellen bewusst:

- Die Namen werden als **drei 32-Bit-Woerter** verglichen statt Byte fuer
  Byte. Zwei Woerter waeren zu wenig gewesen: `KERNEL.BIN` und `KERNEL.BAK`
  sind in den ersten acht Zeichen gleich.
- Der **Superblock wird nicht geprueft**. Die Magie haette sieben Befehle und
  eine eigene Fehlermeldung gekostet, und ohne Dateisystem findet die
  Namenssuche gleich darauf ohnehin nichts.

**Secure Boot musste mitwandern.** Die Firmware rechnete ihre Pruefsumme
ueber Bootsektor + feste Kernelsektoren. Waere das so geblieben, haette sie
Bytes gemessen, die niemand mehr startet -- eine Pruefung, die nie anschlaegt
und trotzdem nach Sicherheit aussieht. `secure_summe` in `firmware/setup.asm`
sucht deshalb ueber `kernel_finden` **dieselbe Datei** wie der Bootsektor.
Dass die Suche zweimal dasteht, ist unvermeidbar: der Bootsektor kann keine
BIOS-Innereien aufrufen.

Nachgeprueft mit drei Durchlaeufen: richtige Summe → startet durch; ein Byte
der gemerkten Summe verdreht → rotes SECURE-BOOT-Bild; **ein Byte in der
Kerneldatei verdreht → rotes Bild.** Der letzte Fall ist der eigentliche
Zweck. Die erwartete Summe wurde unabhaengig in Python nachgerechnet und war
Bit fuer Bit dieselbe (`0xF61B29C2`).

**Wenn geloescht wird, wandert der Kernel in den Papierkorb** -- der
Bootsektor findet ihn dort nicht, weil er den Elternordner mitprueft. Der
Rechner startet also nicht mehr, die Bytes sind aber noch da.

**Nachtrag -- die Meldungen waren deutsch.** Colin hat es auf dem Bild
gesehen: `Bootsektor: lade Kernel ... \SYSTEM\KERNEL.BIN fehlt` mitten in
einem sonst englischen Startbild. Jetzt `Boot sector: loading kernel ... OK`
bzw. `... \SYSTEM\KERNEL.BIN missing`, passend zur Zeile `Booting from Hard
Disk 0 ... OK` darueber. 488 Byte von 512.

**Merke:** Kommentare und Doku auf Deutsch, alles was auf dem Bildschirm des
TB-32 landet auf Englisch. Bei einem neu geschriebenen Programmstueck faellt
das leicht durch.

Tests danach: 45/45 Selbsttest, 11/11 Compilertests, Bootstrapping bestanden,
C- und Python-Emulator Befehl fuer Befehl gleich.

---

## Dateiauswahl-Fenster, Papierkorb -- und zwei sichtbare Fehler

**Neu -- ein Dateiauswahl-Fenster fuer alle** (`system/dialog.c`). Bis jetzt
hatte jedes Programm ein Textfeld fuer den Dateinamen: man musste wissen,
wie die Datei heisst und wo sie liegt. Das ist der Stand von 1981.

Jetzt gibt es EIN Fenster, das Coder, Paint und Word benutzen. Es zeigt den
Ordner, laesst hineinklicken, hat einen Up-Knopf, ein Namensfeld und
OK/Cancel. Der Rueckweg laeuft ueber `dlg_ziel`: das Fenster merkt sich, wer
gefragt hat, und ruft dort die passende Funktion auf -- kein Programm muss
auf ein Ergebnis warten.

**Mit Filter.** Paint sieht nur `.TBI`, Word nur `.TBW`, und *Bild einfuegen*
in Word nur Bilder. Ordner werden immer gezeigt -- man muss ja hinnavigieren
koennen.

**Neu -- der Papierkorb.** `DEL` verschiebt ab jetzt nach `\PAPIERKORB`
statt zu vernichten. Erst wer DORT loescht, loescht wirklich. Der Ordner
entsteht beim ersten Bedarf. Das ist keine Bequemlichkeit: Colin hatte an
einem Abend versehentlich die ganze Platte geleert, und eigene Quelltexte
holt kein `build.py` zurueck.

**Neu -- Bilder in Word loeschen.** Ein Bild ist ein ganzer Absatz; mit der
Ruecktaste haette man Buchstaben seines Dateinamens abgeknabbert. Jetzt
loescht Entf oder Ruecktaste bei angeklicktem Bild den ganzen Absatz,
Dateiname und Umbruch inklusive.

**Behoben -- die Uhr malte ueber andere Fenster.** Einmal je Sekunde zeichnete
`app_clock()` ihren Inhalt direkt auf den Schirm, ohne die Fensterreihenfolge
zu beachten. Die Uhrzeit stand dann mitten im Control Panel. Jetzt fordert
sie ein normales Neuzeichnen an -- und der Schreibtisch kennt die
Reihenfolge. Dasselbe galt fuer den System Monitor.

**Behoben -- nach dem Dialog kam keine Taste mehr an.** `win_top` zeigte auf
das geschlossene Dialogfenster. Beim Schliessen bekommt jetzt das Programm
den Fokus zurueck, das gefragt hat.

**Umbenannt -- Paints "Pic" heisst jetzt "Get".** Es war nie fuer Bilder,
sondern die Pipette: Farbe aus dem Bild aufnehmen und damit weitermalen.
Der alte Name las sich wie "Picture".

---

## Word: Listen, Seiten und "Drucken" -- und drei Fehler nebenbei

**Listen.** Aufzaehlung (gemaltes Kaestchen) und Nummerierung. Die Nummer
zaehlt zurueck bis zum ersten Absatz ohne Nummer -- so faengt jede neue Liste
wieder bei eins an. Ein neuer Absatz erbt die Liste des vorigen: man tippt
eine Liste einfach durch. Der Umbruch rechnet die Einrueckung mit, die Marke
steht nur an der **ersten** Zeile eines Absatzes.

Technisch: ein **zweites Formbyte je Absatz** (`WD_ABS2` bei `0x00730400`).
Im Dateiformat haengt es ganz hinten -- aeltere Dokumente haben dort nichts
und werden einfach ohne Listen geladen, statt kaputtzugehen.

**Echte Seiten.** Der Umbruch teilt in einem zweiten Durchgang die Zeilen
auf Seiten auf (`WD_SEITE_H` = 620 Punkte). Beim Malen erscheint an der
Grenze eine Trennlinie mit der Seitenzahl. Die Umbruchliste hat dafuer eine
vierte Spalte bekommen.

**"Drucken".** Der Rechner hat keinen Drucker -- also gibt es die Datei.
`Als Text speichern` im Rechtsklick-Menue schreibt reinen Text: Formen fallen
weg, Listenmarken werden ausgeschrieben (`- ` und `1. `), Bilder erscheinen
als `[Bild: NAME]`. Damit laesst sich ein Dokument mit `TYPE` anzeigen oder
im Coder oeffnen.

**Behoben -- Blaettern lief ins Leere.** PgDn schob den Anfang um eine feste
Zahl Zeilen weiter und konnte hinter dem Text landen: leere Seite. Jetzt
zaehlt es die **echten Zeilenhoehen** (eine Ueberschrift in Groesse 3 nimmt
dreimal so viel Platz wie normaler Text, ein Bild noch mehr) und stoppt so,
dass die letzte Zeile unten steht.

**Behoben -- der Rechtsklick kam nie beim TB-32 an.** `pc.py` las die
Maustasten mit `pygame.mouse.get_pressed()` statt aus dem Ereignis. Das
liefert je nach Plattform beim Loslassen noch den alten Stand -- und die
rechte Taste fiel ganz durch. Jetzt wird der Zustand aus den Ereignissen
gefuehrt (`e.button`: 1 links, 2 Mitte, 3 rechts). Dazu gilt auf dem Mac
**Ctrl+Klick als Rechtsklick**, weil das dort ohnehin ueblich ist und bei
manchen Trackpads der einzige Weg.

**Neu -- das System liegt jetzt sichtbar in `\SYSTEM`.** `KERNEL.BIN`,
`BIOS.BIN` und `KERNEL.SYM` werden beim Bauen mit auf die Platte gelegt.
Es sind dieselben Bytes wie in den reservierten Sektoren; gebootet wird
weiterhin von dort, nicht aus diesen Dateien. Aber man kann sie jetzt
sehen, mit `DUMP` ansehen und kopieren.

**Nachgeprueft -- was passiert, wenn man alles loescht?** Colin hatte alle
Dateien einschliesslich `\SYSTEM` geloescht, und der Rechner lief weiter.
Versuch mit leerem Dateisystem: **er bootet auch nach einem Neustart** --
der Kernel liegt in den Sektoren 1-318, das Dateisystem faengt erst bei
Sektor 512 an. Loeschen kann ihn gar nicht erwischen. Weg sind nur die
Dateien; `python3 build.py` holt das System zurueck, eigene Quelltexte
nicht. Genau dafuer kommt als Naechstes der Papierkorb.

---

## Der Schreibtisch flackert nicht mehr -- und heisst jetzt Coder

**Behoben -- alles flackerte.** Der Schreibtisch malte direkt in den
*angezeigten* Bildspeicher. Jede halbfertige Zeichnung war sofort zu sehen:
beim Fensterziehen, beim Menueoeffnen, am schlimmsten beim Malen in Paint.

Die Loesung lag schon in der Hardware -- die zweite Bildseite, die Flappy
seit heute Mittag benutzt. Der Schreibtisch bekommt sie jetzt auch. Dafuer
brauchte Port `0x53` eine zweite Betriebsart:

| Wert | Was |
|---|---|
| 1 | Seiten **tauschen** -- schnell, aber die neue Rueckseite hat das vorletzte Bild. Fuer Spiele, die ohnehin alles neu malen. |
| 2 | Rueckseite auf die Vorderseite **kopieren** -- die Rueckseite bleibt stehen. Genau das braucht der Schreibtisch, der meist nur EIN Fenster neu malt. |

Eingebaut in **beide** Emulatoren (Python und C). Der Schreibtisch schaltet
die zweite Seite beim Start ein, kopiert am Ende jeder Runde nach vorn und
schaltet sie beim Verlassen wieder aus. Waehrend ein Vollbildprogramm laeuft,
gehoert die Bildseite dem Programm -- danach holt sich der Schreibtisch sie
zurueck.

**Behoben -- Paint malte auf den Schreibtisch.** Die Vorschau eines grossen
Kreises ragte ueber den Fensterrand hinaus. Die Figur selbst war korrekt
beschnitten (`pt_tupfen` prueft die Grenzen), ihre **Vorschau** aber nicht --
die ging direkt mit `g_frame` auf den Schirm. Jetzt gibt es
`pt_rahmen_begrenzt()`, das an der Leinwandkante endet.

**Beschleunigt -- Paint zeichnete beim Ziehen das ganze Fenster neu.**
Werkzeugleiste, Knoepfe, Palette und Dateiname bei jeder Mausbewegung.
Jetzt geht nur noch die Leinwand nach vorn -- ein einziger Blitterbefehl
(`pt_leinwand_malen`).

**Umbenannt -- aus "Editor" wird "Coder".** Colins Wunsch: der Coder ist
zum Programmieren da, Texte und Notizen schreibt man in Word. Der Punkt
"Notes / text .MD" ist aus dem Startbildschirm verschwunden, es bleiben
C, Assembler und Python. Bestehende Dateien lassen sich weiterhin oeffnen --
man will beim Programmieren ja auch mal in eine README schauen.

**Geprueft:** 45/45, 11/11, Emulator-Vergleich (Bildschirm Zeichen fuer
Zeichen gleich).

---

## Der Emulator in C -- Schritt 1 auf dem Weg zum Pi

`emu/` neben `hardware/`: derselbe Rechner, nur nicht mehr in Python.
`emu/cpu.c` (alle 57 Befehle), `emu/machine.c` (Bus, Grafikkarte mit
Blitter, Platte, Tastatur, Timer, CMOS, Blockkopierer, Waerme),
`emu/main.c` (kopfloser Start zum Vergleichen).

**TOOBAD-OS hat sich dabei um kein Byte geaendert.** Der TB-32 bleibt der
Prozessor -- getauscht wird nur, was die Chips nachbaut. Genau das ist der
Unterschied zu einer Portierung auf ARM, bei der der TB-32 verschwaenden
wuerde.

**Tempo:** 1,8 -> **287 Millionen Befehle je Sekunde**, Faktor **160**.

**Neuer Test: `tools/emu_vergleich.py`.** Zwei Emulatoren desselben Rechners
sind nur etwas wert, wenn sie genau dasselbe rechnen. Der Test laesst beide
Fassungen einzelne Befehle ausfuehren und vergleicht nach jedem
Programmzaehler und Flags; danach den ganzen Bootvorgang Zeichen fuer
Zeichen. Er hat sich sofort bezahlt gemacht -- drei Portierungsfehler, alle
in Sekunden gefunden:

| Fehler | gefunden bei |
|---|---|
| `cmp`/`cmpi`/`tst`/`tsti` vergleichen **rd**, nicht ra | Schritt 13 |
| `jmpr`/`callr` springen ueber **rd**, nicht ra | beim Sprung in den Bootsektor |
| `IRQ_TIMER` ist **0x08**, nicht 0 | "Division durch null" beim Booten |

Der letzte ist der lehrreichste: die IRQ-Nummern in `isa.py` **sind bereits
die Interrupt-Vektoren**. Wer dort 0/1/2 einsetzt, laesst den Timer auf
Vektor 0 springen -- und der heisst "Division durch null". Das BIOS hat
brav genau das gemeldet.

**Stand:** Der C-Emulator bootet TOOBAD-OS, der Bildschirm ist Zeichen fuer
Zeichen gleich, `dir` laeuft. Noch kein Fenster -- das kommt mit SDL im
naechsten Schritt, zusammen mit dem Pi.

---

## Word: Zwischenablage -- und dieselbe wie ueberall sonst

Kopieren, Ausschneiden, Einfuegen mit Strg+C/X/V oder ueber das
Rechtsklick-Menue. Word benutzt **dieselbe Ablage wie der Editor**
(`CLIP_BUF` bei `0x130000`) -- Text wandert also zwischen beiden Programmen.
Und weil `pc.py` die Mac-Zwischenablage dorthin spiegelt, auch zwischen dem
TB-32 und dem Mac.

Die **Farben** kommen in eine eigene kleine Ablage daneben (`0x00760000`).
Bleibt die Laenge gleich, faerbt sich der eingefuegte Text wieder wie vorher
-- ein Kopieren innerhalb von Word behaelt also die Farbe, ein Einfuegen vom
Mac kommt schwarz herein. Genau das erwartet man auch.

**Geprueft:** "ABC " markiert, Strg+C, zweimal Strg+V -> "ABC ABC ABC ".

---

## Word kann jetzt markieren, faerben und Bilder einbauen

Colins Wunsch: Text markieren, Rechtsklick, faerben -- und Paint-Bilder
einfuegen und in der Groesse aendern. Alles vier drin.

**Markieren** mit der Maus: aus der Mausposition wird erst die
Bildschirmzeile gesucht, dann ueber die Schriftgroesse die Spalte. Die
Markierung wird invers gemalt, Tippen ersetzt sie, Ruecktaste loescht sie.

**Rechtsklick.** Die Maus liefert Bit 0 links, Bit 1 Mitte, **Bit 2 rechts**
-- der Schreibtisch hat das bisher weggeworfen und jeden Klick gleich
behandelt. Jetzt merkt er sich in `gui_taste`, welche Taste es war. Word
klappt darauf ein Menue auf: sechs Farben, Alles markieren, Markierung weg,
Bild einfuegen.

**Textfarben** waren die eigentliche Modelaenderung. Die Form sass **je
Absatz** -- fuer eine eingefaerbte Markierung braucht es aber **ein Byte je
Zeichen**. Also ein zweiter Puffer neben dem Text (`0x00728000`), der bei
jedem Einfuegen und Loeschen mitwandert. Beim Malen wird die Zeile in
gleichfarbige Abschnitte zerlegt -- dieselbe Technik wie im Coder.

**Bilder als eigener Absatz.** Sein Text ist der Dateiname, sein Formbyte
traegt die Marke `WF_BILD`, seine Groesse steht in zwei eigenen Feldern. Der
Umbruch behandelt ihn wie eine sehr hohe Zeile, der Text laeuft darueber und
darunter. Anklicken markiert das Bild, am Anfasser unten rechts zieht man
die Groesse. Fehlt die Datei, steht das ehrlich im Rahmen statt eines
Absturzes.

**Neu in der Hardware -- Blitter-Kommando 7: Bild skaliert.** Kommando 4
malt Bilder nur 1:1. Fuer freie Groessen rechnet die Karte jetzt nach dem
Nachster-Nachbar-Verfahren: die Schrittweite steht als Bruch aus Quell- und
Zielgroesse fest, ganz ohne Kommazahlen -- genau so skalieren Grafikkarten
seit jeher. Quellgroesse im CHR-Register, Zielgroesse in W und H. Gemessen:
**1,7 ms** fuer 320x240.

**Geprueft:** Satz getippt, mit der Maus markiert, ueber das Rechtsklick-Menue
rot gefaerbt, Bild eingefuegt (160x100 auf 200x130 skaliert), am Anfasser auf
gut 300x200 gezogen -- Text laeuft weiter darum herum. 45/45, 11/11,
Bootstrapping.

---

## Word -- Textverarbeitung als Fenster im Schreibtisch

Start -> Word. `system/word.c`.

**Der Unterschied zum Editor ist das Modell, nicht die Bedienung.** Der
Editor kennt Zeilen, so wie sie in der Datei stehen. Eine Textverarbeitung
kennt **Absaetze** -- wo eine Zeile umbricht, entscheidet die Seitenbreite,
nicht die Eingabetaste. Deshalb liegen hier zwei Ebenen uebereinander:

* der Text als durchgehender Puffer, Absaetze durch Zeilenumbruch getrennt
* je Absatz ein **Formbyte**: Groesse (1-3), fett, unterstrichen, Ausrichtung

Daraus wird bei jeder Aenderung ein **Umbruch** gerechnet -- eine Liste von
Bildschirmzeilen mit Anfang, Laenge und Absatz. Gebrochen wird an
Leerzeichen, nicht mitten im Wort.

**Was geht:** drei Schriftgroessen (8, 16, 24 Punkte), fett, unterstrichen,
links/mittig/rechts, Wortumbruch, Blaettern, Neu, Speichern und Oeffnen im
eigenen Format **TBW** (Laenge, Anzahl Absaetze, Formbytes, Text).

**Wie fett gemacht wird:** derselbe Text noch einmal einen Punkt versetzt
gemalt -- genau der Trick, mit dem Nadeldrucker frueher fett gedruckt haben.
Einen zweiten, fetten Zeichensatz gibt es nicht.

**Ehrliche Grenze:** Der Zeichensatz hat **feste Breite** und laesst sich nur
ganzzahlig vergroessern. Echte Proportionalschrift waere ein eigenes Projekt
-- dafuer braeuchte es einen selbst gezeichneten zweiten Zeichensatz mit
Breitentabelle.

**Erweitert:** Blitter-Kommando 6 (Zeichenkette) kann jetzt auch
vergroessern -- eine Ueberschrift in 24 Punkt ist damit ein einziger
Malbefehl.

**Geprueft:** Ueberschrift in Groesse 3 zentriert, Absatz in Groesse 1 mit
automatischem Umbruch, Zwischenueberschrift in Groesse 2 unterstrichen;
Speichern, Leeren, Wiederladen -- alle Formate kommen zurueck. 45/45, 11/11,
Bootstrapping.

---

## Coder -- aus dem Editor wird ein Werkzeug fuer Programme

Der Editor im Schreibtisch kann jetzt, was ein Code-Editor koennen muss:

* **Zeilennummern** in einer eigenen Spalte links
* **Syntaxfarben** fuer C, Assembler und Python: Schluesselwoerter blau,
  Zeichenketten gruen, Kommentare grau, Zahlen magenta, Praeprozessor braun.
  Die Sprache ergibt sich aus der Dateiendung.
* **Suchen** (Knopf *Find*, Strg+F, weitersuchen mit F3 oder Eingabetaste),
  ohne Gross-/Kleinschreibung, mit Umbruch am Dateiende
* **Automatisches Einruecken**: die neue Zeile uebernimmt die Einrueckung der
  vorigen, nach einer offenen geschweiften Klammer zwei Stellen mehr
* **Sprung zur Fehlerzeile**: schlaegt das Uebersetzen fehl, springt der
  Cursor auf die Zeile, die der Compiler gemeldet hat

Quelltext: `system/coder.c`.

**Neu in der Hardware -- Blitter-Kommando 6 "Zeichenkette".** Vorher schickte
das Betriebssystem je Buchstabe einen eigenen Malbefehl; eine Editorseite
sind 1600 Stueck. Jetzt holt sich der Blitter den Text selbst aus dem
Speicher: Adresse im CHR-Register, Laenge im W-Register, Zeichensatz bleibt
wo er ist. `g_text` im ganzen Schreibtisch geht darueber, der Editor malt
je Zeile nur noch die **Farbabschnitte**.

**Wie die Faerbung schnell blieb.** Der erste Versuch kostete je Tastendruck
**476.000 Befehle** -- eine Sechstelsekunde, das Tippen fuehlte sich tot an.
Drei Sachen:

1. Farben haengen nur am Text und am Sichtbereich, nicht am Cursor. Der
   Farbpuffer wird deshalb nur neu gebaut, wenn sich einer von beiden
   aendert -- Pfeiltasten, Mausbewegungen und Klicks kosten gar nichts mehr.
2. Die Schluesselwortsuche pruefte fuer *jedes* Wort die ganze Liste. Jetzt
   entscheidet erst der Anfangsbuchstabe.
3. Zeiger statt Funktionsaufruf je Bildpunkt, und Zeichenketten statt
   Einzelbuchstaben ueber den Blitter.

Ergebnis: **476.000 -> 190.000** Befehle je Neuzeichnen.

**Zwei Fehler unterwegs:**

* Ich habe wieder zu frueh gemessen. Das Fenster blieb leer, alle Zaehler
  sahen falsch aus -- in Wahrheit war das Bild noch nicht fertig gemalt.
  Derselbe Fehler wie beim Fuellwerkzeug in Paint, zwei Stunden spaeter.
  Steht in [[07 Fallstricke]].
* Der Farbabschnitt wurde ueber `spalte - laenge` platziert. Bei Zeilen, die
  breiter als das Fenster sind, zaehlt `spalte` aber weiter -- der letzte
  Abschnitt landete dadurch immer weiter rechts, Zeile um Zeile. Jetzt wird
  die Startspalte direkt gemerkt.

**Geprueft:** CALC.C geoeffnet (Farben, Zeilennummern), Suche nach "rechne"
findet die Stelle, Einruecken nach `{` setzt zwei Stellen, 45/45, 11/11,
Bootstrapping.

---

## Paint -- das erste der drei neuen Programme

Ein Zeichenprogramm als **Fenster im Schreibtisch** (Startmenue -> Paint).
Werkzeuge: Stift, Radierer, Linie, Rechteck, gefuelltes Rechteck, Kreis,
Fuellen, Pipette. Dazu drei Strichstaerken, 32 Farben, Neu, Rueckgaengig,
Speichern und Oeffnen im eigenen Format **TBI** (Breite, Hoehe, dann ein Byte
je Punkt). Quelltext: `system/paint.c`.

Die Leinwand liegt **nicht** im Bildspeicher, sondern im RAM bei `0x600008`.
Ein Fenster kann verschoben oder ueberdeckt werden -- laege das Bild direkt
auf dem Schirm, waere es danach kaputt. Auf den Schirm kommt es mit einem
einzigen Blitterbefehl.

**Neu in der Hardware -- Blockkopierer (DMA), Ports 0x56-0x5A.** Er schaufelt
Speicher am Prozessor vorbei: 256 KB in 0,03 ms statt einer Drittelsekunde.
Rueckgaengig und "Neu" laufen darueber. Dazu drei **Suchbefehle** ueber ganze
Bloecke, das Gegenstueck zu den Zeichenkettenbefehlen echter Prozessoren.
Einzelheiten in [[02 Speicherkarte und Ports]].

**Beschleunigt -- Bild aus dem RAM (Blitter-Kommando 4).** Lief Punkt fuer
Punkt durch Python: **9,5 ms** fuer 400x300, also die halbe Bildzeit. Jetzt
zeilenweise, und nur Zeilen mit einem durchsichtigen Punkt werden einzeln
behandelt: **0,06 ms**, 158-mal schneller.

**Der Schreibtisch kennt jetzt das Ziehen.** Bisher gab es nur den Klick.
Ein Malprogramm braucht die ganze Bewegung und den Moment des Loslassens --
Linie, Rechteck und Kreis werden waehrend des Ziehens nur auf den Schirm
gemalt und erst beim Loslassen in die Leinwand uebernommen.

**Zwei eigene Patzer, aufgeschrieben damit sie nicht wiederkommen:**

* Der Blitter liest die Bildquelle aus **demselben Register wie den
  Zeichensatz**. Ohne `sys_out(P_BLT_SRC, ...)` davor malte Paint den
  Zeichensatz als Leinwand -- schwarz mit Rauschen. Und danach muss man es
  zuruecksetzen, sonst schreibt der ganze Schreibtisch seine Schrift aus dem
  Bild.
* Eine Textersetzung ohne Anzahlgrenze hat die Doppelpuffer-Methoden in
  **jede** Geraeteklasse kopiert (neun Stueck). Gefunden beim Suchen nach
  etwas ganz anderem.

**Und die teuerste Lehre:** Das Fuellwerkzeug sah kaputt aus -- es fuellte
nur drei Zeilen. Ich habe eine Stunde lang nach einem Fehler in der
Warteschlange gesucht, Zaehler mitgeschrieben, die Funktion aufgeteilt, den
Keller ins RAM verlegt. Es war **nie kaputt, nur langsam**: beim Messen
rechnete es noch. Steht in [[07 Fallstricke]].

**Geprueft:** Paint von Hand bedient (Stift, Linie, Rechteck, Kreis, Fuellen,
Farben, Strichstaerken), 45/45, 11/11, Bootstrapping.

---

## Der Rechner wurde bei Flappy 65 Grad heiss und drosselte sich selbst

Colins Meldung: „Der PC ist gerade gefreezt bei Flappy." Ein hartes Einfrieren
liess sich in 200 Sekunden Spiel nicht ausloesen — die Uhr lief durch, das
Bild aenderte sich. Gefunden wurde aber etwas anderes, das sich genau so
anfuehlt:

**Der Bildtakt hat den Prozessor nicht schlafen gelegt.** `proc_next()` gibt
den eigenen Prozess zurueck, wenn sonst niemand rechenbereit ist. Damit war
`proc_sleep()` bei einem einzigen laufenden Programm wirkungslos: schlafen,
sofort wieder geweckt, schlafen ... 100 % Auslastung. Der TB-32 heizte auf
65 Grad und drosselte auf 40 % Takt — und ein gedrosseltes System reagiert
traege auf alles, auch auf ESC.

Gefunden durch Abtasten des Befehlszaehlers: die Spitze lag in
`proc_next`/`proc_schedule`, nicht im Spiel. Ausfuehrlich in [[07 Fallstricke]].

**Behoben:** `proc_sleep()` wartet den Rest der Zeit selbst mit `hlt` ab.

| | vorher | nachher |
|---|---|---|
| Temperatur bei Flappy | 65,1 °C | **26,6 °C** |
| Drosselung | 60 % | **0 %** |
| Auslastung | 100 % | **10 %** |
| Bildrate | 50 | 50 |

Das gilt fuer **jedes** Programm, das schlaeft — der ganze Rechner bleibt im
Leerlauf jetzt kalt.

**Ausserdem behoben — Doppelpufferung ging nur beim ersten Start.** Beim
Ausschalten zeigte die Merkvariable fuer die Rueckseite anschliessend auf
dieselbe Seite; beim zweiten Start eines Spiels waren beide Bildseiten
dasselbe Feld und es flackerte wieder. Die Karte haelt jetzt zwei feste
Seiten und waehlt die jeweils andere.

---

## Die Grafik-Engine kann jetzt, was eine Engine koennen muss

Colins Befund: „die count zahl flackert und legt sich nicht ueber irgendwas,
es ist ein blauer kasten drumherum". Beides stimmte, und beides waren keine
Fehler in Flappy, sondern Loecher in der Engine.

**Zwei Bildseiten (Doppelpufferung).** Bisher las der Bildschirm mit,
waehrend gemalt wurde — man sah halb gezeichnete Ziffern. Die Karte hat
jetzt zwei Bildspeicher: gemalt wird in den unsichtbaren, `gx_zeigen()`
tauscht sie. Ports `0x52` (an/aus) und `0x53` (tauschen).

Damit faellt der ganze alte Eiertanz weg: nur das Bewegte loeschen, die
Reihenfolge der Malbefehle abzaehlen, Anzeigen ein Kaestchen freiraeumen
lassen. Genau dieses Kaestchen war Colins blauer Kasten — die Punkteanzeige
musste ihren alten Stand ueberdecken und stanzte dabei ein himmelblaues Loch
in jedes Rohr dahinter. Jetzt malt Flappy jedes Bild vollstaendig neu, von
hinten nach vorn, und die Ziffern liegen einfach obendrauf (mit Schatten,
damit sie auch vor Gruen lesbar bleiben).

**Vergroessern kann die Karte selbst.** Port `0x54` setzt den Faktor fuer
Blitter-Kommando 3. Vorher schrieb `gx_gross` bei Zoom 3 fuer **eine
Ziffer 576 Punkte einzeln** — das war der eigentliche Flaschenhals des
ganzen Spiels, nicht die Rohre.

**Ein Bildtakt.** `gx_takt(2)` haelt 50 Bilder je Sekunde ein, unabhaengig
davon, wie schnell die Maschine gerade ist. Ohne Bremse lief das Spiel nach
den Verbesserungen mit 500 fps — und damit zehnmal zu schnell zum Spielen.
Zwei Details, die es gebraucht hat: der naechste Zeitpunkt wird vom vorigen
aus gerechnet (sonst geht der Rest der angebrochenen Hundertstelsekunde
verloren: 40 statt 50), und gewartet wird mit `sleep(0)` statt `sleep(1)` —
eine ganze Hundertstelsekunde waere die halbe Bildzeit.

Bildrate von Flappy im Verlauf dieser Sitzung:

| | fps |
|---|---|
| Anfang | 9 |
| Blitter in einem Systemaufruf | 39 |
| Ports direkt + schnellerer Blitter | 53 |
| alles neu malen (noch mit Punkt-fuer-Punkt-Schrift) | 29 |
| Vergroessern im Blitter | **500** |
| mit Bildtakt gebremst | **50, gleichmaessig** |

**Neu in `gfxlib.c`:** `gx_doppelpuffer(an)`, `gx_zeigen()`, `gx_takt(hundertstel)`.
`gx_gross` und `gx_text_gross` gehen jetzt ueber den Blitter.

**Gefunden dabei:** Ein Schreibvorgang auf einen unbekannten Port wird still
verschluckt (`bus.unknown_ports`). Die neuen Ports waren beim ersten Versuch
nicht in `machine.py` eingetragen — die Doppelpufferung blieb einfach aus,
ohne jede Meldung. Wer einen Port hinzufuegt, muss ihn an **drei** Stellen
eintragen: `isa.py`, das Geraet und die Geraeteliste in `machine.py`.

**Sicherung:** Ein Moduswechsel setzt die Vergroesserung auf 1 zurueck, sonst
schriebe der Schreibtisch in Riesenschrift weiter, wenn ein Programm mit
gesetztem Zoom abstuerzt.

**Geprueft:** Flappy (50 fps, sauberes Bild, Anzeige ueber den Rohren), der
Taschenrechner, der Schreibtisch, 45/45, 11/11, Bootstrapping — und `CC
CALC.C` **auf dem Geraet**, damit auch der Compiler dort die neuen Ports
richtig erzeugt.

---

## Grafik: Programme malen jetzt direkt, ohne den Kernel

**Neu — der Blitter gehoert auch den Programmen.** Ports sind auf dem TB-32
nicht geschuetzt, ein Programm darf sie selbst bedienen. `gx_fill`,
`gx_frame`, `gx_char` und `gx_text` in `gfxlib.c` schreiben deshalb
unmittelbar an 0x44–0x4C statt ueber `int 0x40`. Der Sprung in den Kernel
kostete das Sichern von 15 Registern und eine lange Fallunterscheidung —
sechs Portschreibvorgaenge sind zusammen billiger als dieser eine Sprung.

Damit das in **beiden** Compilern gleich funktioniert:

* `programs/prog_start.asm` bekommt `portout:` / `portin:` (fuer TCC auf dem Mac)
* `programs/cc.c` kennt sie als eingebaute Funktionen 98 und 97 und setzt
  `outr` bzw. `inr` direkt an der Aufrufstelle ein (`e_outr`, `e_inr`)
* `programs/proglib.c` deklariert sie nur noch, ohne Rumpf

Achtung bei `outr`: die Reihenfolge ist `outr <Wert>, <Port>` — der Port
steht in `ra`. Mein erster Versuch hatte sie vertauscht, das Bild blieb
komplett schwarz.

**Neu — `gx_text` schreibt Farbe und Zeile nur einmal.** Bei zwanzig
Buchstaben spart das vierzig Portzugriffe.

**Beschleunigt — der Blitter im Emulator.** Drei Sachen:

| | vorher | nachher |
|---|---|---|
| ein Portzugriff | 0,58 µs | **0,10 µs** |
| Zeichen 8×8, durchsichtig | 9,90 µs | **1,52 µs** |
| Zeichen 8×8, mit Hintergrund | 9,90 µs | **2,53 µs** |
| Flaeche 52×120 | 17,08 µs | 12,51 µs |
| Vollbild 640×360 | — | 8,98 µs |

1. `port_out` hatte eine `import`-Zeile **im Rumpf** — sie lief bei jedem
   einzelnen Portzugriff, und ein Malbefehl schreibt sechs davon. Dasselbe
   stand in elf weiteren Methoden (Maus, Timer, Tastatur, Platte …), alle
   nach oben gezogen.
2. Der Zeichen-Blit lief 64-mal durch eine Python-Schleife. Liegt der
   Buchstabe ganz auf dem Schirm, werden jetzt acht fertige Acht-Byte-Folgen
   gesetzt; sie wiederholen sich staendig und liegen im Zwischenspeicher.
   Durchsichtig werden nur die gesetzten Punkte geschrieben (`_GESETZT`).
3. Fuellen ueber die ganze Breite ist ein einziger Schreibvorgang statt
   einer Zeile je Bildzeile.

**Ergebnis, gemessen an Flappy:** 9 fps → 39 fps → **53 fps**.

**Geprueft:** Flappy aus der Kommandozeile, der Schreibtisch (unveraendertes
Bild), und — der eigentliche Beweis — `CC CALC.C` **auf dem Geraet**: der
Taschenrechner uebersetzt sich mit dem neuen `cc.c` und laeuft. Dazu 45/45,
11/11 und das Bootstrapping.

---

## Ein Prozessplatz hat die Marke seines Vormieters behalten

**Symptom:** Nach dem Uebersetzen im Editor kam im gestarteten Programm
**keine einzige Taste** an. Der Vogel in Flappy fiel sofort herunter, ESC
half nicht. Aus der Textkonsole gestartet lief dasselbe Programm normal.

**Ursache:** `p_bg[pid]` merkt sich „im Hintergrund gestartet"; solche
Prozesse bekommen absichtlich keine Tastatur ([[07 Fallstricke]]). Gesetzt
wurde die Marke beim Start, **geloescht nie**. Der Compiler laeuft im
Hintergrund, gibt seinen Platz frei — und die danach gestartete
Kommandozeile bekam denselben Platz samt alter Marke. Damit galt sie als
Hintergrundprogramm, und jedes `getkey()` schlief fuer immer.

Messbar war es am Tastenpuffer: `tail` wuchs bei jedem Druck, `head` blieb
stehen — die Taste kam an, niemand holte sie ab.

**Behoben in:** `system/proc.c`, `proc_start()` setzt `p_bg[i] = 0` beim
Vergeben eines Platzes, wie schon `p_wake` und `p_ticks`.

**Merke:** Wer einen Platz wiederverwendet, muss *alle* Felder
zuruecksetzen, nicht die meisten.

---

## Der Schreibtisch hat ins laufende Vollbildprogramm gemalt

**Symptom:** Startet man ein Programm ueber **Run** im Editor, lagen
Command-Prompt-Fenster und Knopfleiste mitten im Bild des Spiels. Per
Doppelklick im File Manager passierte es nicht.

**Ursache:** Schreibtisch und Programm laufen gleichzeitig. Die Regel
„solange ein Programm den ganzen Schirm hat, malt der Schreibtisch nichts"
wurde nur **oben in der Hauptschleife** geprueft. Schaltet das Programm
mitten in einer Runde um, malt der Schreibtisch den Rest dieser Runde
trotzdem — und weil ein Spiel den Hintergrund nur beim Rundenstart einmal
fuellt, bleibt das Fensterbild danach stehen.

**Zwei Anlaeufe, die nicht reichten:** die Pruefung ans Schleifenende zu
setzen half gar nicht; sie an den Anfang von `draw_desktop`/`draw_window`/
`draw_taskbar` zu setzen half nur halb — `draw_desktop` malt viele Fenster
nacheinander, das Umschalten passiert mittendrin.

**Behoben in:** `system/gui.c`, `gui_fremd` wird in den Malbefehlen selbst
geprueft (`g_fill`, `g_frame`, `g_char`). So hoert der Schreibtisch mitten
im Satz auf.

---

## `#include` findet jetzt auch `\SOURCE` — und ein Präprozessor-Fehler

**Behoben — der Präprozessor hat Quelltext verschluckt.** Ein `#include`, das
nur *in einem Kommentar* stand, wurde für eine Anweisung gehalten; die Zeile
verschwand samt dem `*/`, und der offene Kommentar fraß die nächsten
Codezeilen. Betroffen waren beide Compiler: `tools/tcc.py` (`zeilen_im_kommentar()`)
und `programs/cc.c` (`komm_folge()`). Ausführlich in [[07 Fallstricke]].

Das war die wahre Ursache dafür, dass `fileread_lib` immer −1 lieferte: der
Systemaufruf 33 stand zwar im Quelltext, war aber nie im Kernel gelandet.
`fs_find_in` war die ganze Zeit in Ordnung.

**Neu — Suchpfad für `#include`.** Bisher musste eine Quelldatei im selben
Ordner liegen wie `proglib.c`, sonst brach das Übersetzen ab. Jetzt gilt:

* Die **Hauptdatei** wird dort gesucht, wo man gerade steht (`fileread`).
* **Eingebundene Dateien** zusätzlich in `\SOURCE` (`fileread_lib`,
  Systemaufruf 33, `fs_read_lib()` in `fs.c`).

Damit lässt sich ein Programm in jedem beliebigen Ordner — auch auf dem
Schreibtisch — übersetzen, solange es `#include "proglib.c"` schreibt.
Die Fehlermeldung sagt jetzt auch, wo gesucht wurde:

```
  cannot include: proglib.c -- not in this folder and not in \SOURCE
```

**Geprüft:** beide Fälle. `CC CRASH.C` in `\SOURCE` (der bisher einzige
funktionierende Weg) und `CC AUSSEN.C` im Wurzelverzeichnis — beide
übersetzen und laufen. Dazu 45/45 Selbsttest, 11/11 Compilertest und das
Bootstrapping (Stufe 2 und 3 Byte für Byte gleich).

---

## Fehlermeldungen: sichtbar und mit richtiger Zeilennummer

**Neu — Meldungen erscheinen im Editor.** Der Compiler läuft als eigener
Prozess; seine Ausgabe ging in den unsichtbaren Textbildschirm, im Editor
stand bloß „Errors". Jetzt schreibt das System die Ausgabe während des
Übersetzens in einen Puffer mit (`cap_*` in `term.c`, Puffer bei
`0x128000`, 40 Zeilen), und das Übersetzungsfenster bleibt bei Fehlern
stehen und zeigt sie als **„Compiler messages"**.

Wichtig dabei: Der Mitschnitt musste **in `syscall.c`** sitzen, nicht nur in
`lib.c`. Programme geben über die Systemaufrufe aus, nicht über die
`print`-Funktionen des Kernels — der erste Versuch fing deshalb nichts ein.
Und `printn` gehört dazu, sonst fehlen genau die Zahlen (Zeilennummern!).

**Neu — Zeilennummern stimmen wieder.** Sie zählten den eingebundenen Text
mit: eine acht Zeilen lange Datei meldete Fehler in „line 146", weil
`proglib.c` davorgeklebt wird. `cc.c` merkt sich jetzt, welcher Bereich der
zusammengeklebten Fassung aus welcher Datei stammt (`inc_start`, `inc_len`,
`melde_ort`) und rechnet zurück. Fehler in einer eingebundenen Datei melden
deren Namen mit.

**Neu:** „unknown function" nennt jetzt die Zeile des Aufrufs — die
Zeilennummer wird beim Erzeugen des Aufrufs mitgeschrieben (`fix_zeile`).
Vorher stand da nur der Name, und man suchte in einer langen Datei.

Vorher/nachher bei derselben Datei:

```
  undefined function: gibtsnicht          (vorher)
  line 4: unknown function gibtsnicht     (jetzt)
```

Und „cannot include" sagt jetzt dazu, dass nur im aktuellen Ordner gesucht
wurde — genau die Falle, in die man mit einer Datei außerhalb von `\SOURCE`
läuft.

---

## Löschtaste wiederholt jetzt — und ein Fehlversuch beim Compiler

**Neu:** Gehaltene Sondertasten wiederholen sich. Nach **0,4 s** Halten
löst die Taste alle **30 ms** erneut aus — für Rücktaste, Entfernen, Pfeile
und Bild auf/ab. Umgesetzt in `pc.py` (`WIEDERHOLBAR`, `halten`), nicht über
`pygame.key.set_repeat`: so wiederholen sich genau die Tasten, bei denen es
Sinn ergibt, und nicht F12 oder Strg+R.

**Erkannt:** Wer eine `.C`-Datei außerhalb von `\SOURCE` übersetzt, bekommt
lauter „undefined function". Ursache: `CC` sucht `#include`-Dateien **nur im
aktuellen Ordner**, `proglib.c` und `gfxlib.c` liegen aber in `\SOURCE`.
Wer das nicht weiß, sucht den Fehler im eigenen Code.

**Fehlversuch, wieder zurückgenommen:** Ich habe dafür einen Suchpfad gebaut
(`fs_read_lib`, Syscall 33: erst aktueller Ordner, dann `\SOURCE`) und `cc.c`
darauf umgestellt. Der Aufruf lieferte aber **immer −1**, auch für Dateien im
aktuellen Ordner — damit ging **gar kein** `#include` mehr, also kein einziges
Programm mehr zu übersetzen. Sofort zurückgebaut, `cc.c` liest Includes
wieder mit `fileread`.

`fs_read_lib` und Syscall 33 stehen weiter im Kernel, werden aber von
niemandem benutzt. Warum sie −1 liefern, ist **ungeklärt** — das gehört
gedebuggt, bevor es jemand wieder anfasst. Steht in [[11 Offene Punkte]].

> **Nachtrag — geklärt und erledigt.** Syscall 33 war gar nicht im Kernel: ein
> `#include` im *Kommentar* darüber hatte den Präprozessor dazu gebracht, die
> Zeile mit dem `*/` zu löschen. Der offene Kommentar hat den Code gefressen.
> Siehe den Eintrag ganz oben und [[07 Fallstricke]].

**Merke:** Eine Änderung am Compiler trifft *jedes* Programm. So etwas erst
mit einer echten Übersetzung prüfen, bevor es weggeht — ich hatte nur den
neuen Fall getestet, nicht den alten.

---

## Einfügen vom Mac ging nur mit Cmd+V

**Behoben.** Wer auf dem Mac etwas kopiert hatte und im Editor `Strg+V`
drückte, bekam nichts.

**Ursache:** Es gab **zwei** Ablagen und zwei Tasten. `Strg+V` fügte die
interne Zwischenablage von TOOBAD-OS ein (die war leer), nur `Cmd+V` holte
vom Mac. Eine Unterscheidung, die kein Mensch im Kopf behalten will — und
die auch technisch keinen Grund hatte.

**Jetzt:** Beide Tasten tun dasselbe. Liegt auf dem Mac etwas in der
Zwischenablage, schreibt `pc.py` es **direkt in den Puffer von TOOBAD-OS**
(`gast_clipboard_setzen`, über die Symboltabelle `system/kernel.sym`) und
schickt dann `Strg+V` an das System — das fügt von da an selbst ein.

Nebenbei zwei Verbesserungen: Vorher wurden die Zeichen als **Tastendrücke**
eingeschleust, also einzeln durch den Tastaturpuffer — langsam, nur im
Editor brauchbar und ohne Tabulatoren. Jetzt geht es in einem Rutsch, samt
Zeilenumbrüchen, überall wo das System einfügt. Und die Symboltabelle wird
nur noch **einmal** gelesen statt bei jedem Tastendruck.

Geprüft: Text mit zwei Zeilen vom Mac eingefügt (50 Zeichen rein, 50 im
Editor angekommen), danach mit `Strg+A`/`Strg+C` markiert und wieder
zurückgeholt.

---

## Flappy, und was dabei über die Grafik herauskam

**Neu:** `programs/flappy.c` — Flappy Bird für den TB-32. Physik in
**Sechzehnteln** eines Bildpunktes (der TB-32 kann kein Fließkomma), Vogel,
drei wandernde Rohre, Punkte, Bestwert, Bildratenanzeige.

**Neu — und viel wichtiger als das Spiel:** Systemaufruf **31 und 32** nehmen
einen ganzen Blitter-Befehl auf **einmal** entgegen. Vorher brauchte eine
gefüllte Fläche **sechs** Systemaufrufe (x, y, w, h, Farbe, Kommando).
Koordinaten stecken zu zweit in einem Wort, jeweils 16 Bit im
Zweierkomplement — der Blitter rechnet Werte ab `0x8000` selbst wieder ins
Negative.

Gemessen auf dem Gerät (200 gefüllte Flächen):

| | |
|---|---|
| über sechs Ports (alt) | 85 Ticks → **4,25 ms je Fläche** |
| über einen Systemaufruf | 30 Ticks → **1,5 ms je Fläche** |
| ein Systemaufruf allein (`ticks()`) | **0,4 ms** |

Das ist der Grund, warum bewegte Grafik auf dem TB-32 zäh ist: **nicht das
Rechnen kostet, sondern der Sprung in den Kernel.** Das Spiel braucht rund 25
Malbefehle je Bild und kommt damit auf **9 Bilder/s** — vorher waren es 5.
Der Vogel selbst verbraucht dabei nur etwa 20.000 Befehle je Bild; die CPU
ist zu über 90 % mit Warten beschäftigt.

Wer es schneller will, muss an dieser Stelle ansetzen, nicht am Spiel:
weniger Systemaufrufe je Bild, oder ein billigerer Weg in den Kernel.
Steht in [[11 Offene Punkte]].

**Behoben beim Bauen des Spiels**

- Falsche Farbnummern: der Farbwürfel ist `16 + rot*36 + grün*6 + blau`, ich
  hatte Grün und Braun verwechselt
- **Flackern**: Das Spiel löschte erst den ganzen Vogel, malte dann die Rohre
  und erst danach den Vogel neu — dazwischen lagen zwei Dutzend Malbefehle,
  und der Bildschirm liest ja währenddessen schon mit. Jetzt wird der Vogel
  unmittelbar vor dem Neuzeichnen gelöscht
- **Der Bestwert verschwand**: die Spur hinter den Rohren wischt oben mit
  durch, die Anzeige wurde aber nur bei Änderung neu gemalt

---

## Tastatur hinkte einen Anschlag hinterher

**Behoben.** Im BIOS-Setup passierte beim ersten Pfeil nichts, der nächste
Druck führte dann die vorige Bewegung aus — die Steuerung fühlte sich
komplett verdreht an.

**Ursache:** `irq_kbd` in `firmware/bios.asm` holte pro Interrupt **genau
eine** Taste. Der Interruptcontroller kennt je Quelle aber nur ein Bit:
Kommen zwei Tasten an, bevor der Handler läuft, gibt es trotzdem nur einen
Interrupt — die zweite blieb im Baustein liegen, bis irgendwann die nächste
Taste einen neuen Interrupt auslöste. Im Setup fiel es besonders auf, weil
dort nach jedem Tastendruck der ganze Bildschirm neu gezeichnet wird.

Derselbe Fehler steckte früher im **Timer** und steht schon in
[[07 Fallstricke]] — ich habe ihn beim Nachbau der Tastatur nicht
mitgedacht.

**Behoben:** Der Handler räumt den Baustein jetzt in einer Schleife leer,
solange der Statusport eine Taste meldet.

**Neu:** Selbsttest prüft den Fall (zwei Tasten im selben Bild), 44 → **45**.

---

## BIOS-Setup mit Reitern, stellbare Uhr, Secure Boot

**Neu**

- Setup hat vier Reiter: **Main, Hardware, Cooling, Security**.
  Links/Rechts wechselt, `SET_TABS` und `setup_tabs` in `firmware/setup.asm`
- **Uhrzeit und Datum stellbar** über einen Feldeditor
  (`setup_edit_felder`) — Hoch/Runter ändert, Links/Rechts wechselt das Feld
- **Cooling**: Lüftersteuerung und Drosselgrenze im CMOS
  (`CM_FANMODE`, `CM_TEMPLIMIT`), beim POST von `kuehlung_anwenden` an die
  Ports gegeben. Temperatur, Lüfter, Drosselung und Höchstwert live
- **Secure Boot**: Prüfsumme über Bootsektor, Kernel und die ersten 16 KB
  ROM, gemerkt in `CM_SUM0`–`CM_SUM3`. Stimmt sie nicht, hält der Rechner an
  und bietet `DEL` als Weg ins Setup. Ab Werk aus
- Neue Ports in `const.inc`: `P_TEMP`, `P_FAN`, `P_THROTTLE`, `P_TEMPLIMIT`,
  `P_FANMODE`, `P_TEMPMAX`
- Selbsttest um drei Prüfungen erweitert: 41 → **44**

**Behoben**

- **Die Uhr des TB-32 ließ sich nicht stellen.** `CMOS._refresh_clock()` las
  bei jedem Zugriff die Uhr des Wirts — jeder geschriebene Wert war sofort
  wieder überschrieben. Der Baustein merkt sich jetzt einen **Versatz in
  Sekunden** (CMOS-Register `0x30`–`0x33`); Schreiben auf ein Uhrenregister
  rechnet ihn neu aus, wie das Drehen an einem echten RTC
- **Bildschirm voller Nullen beim Reiter Security.** Zwei Ursachen: der
  aktive Reiter lag in `BDA_SCRATCH`, wo `vid_putn` seine Ziffern
  formatiert — und `vid_puthex` erwartet die Stellenzahl in `r3`, die ich
  nicht gesetzt hatte. Eigener Platz `SETUP_TAB`/`SETUP_ROW`/`SETUP_SAVE`
  ab `0x600`
- **Endlosschleife beim Zeichnen**: Zeilenzahl lag in `r11`, einem
  Kratzregister, das jeder Unterprogrammaufruf zerstören darf

---

## CPU-Optimierung: rund 3,4× schneller

**Geändert** — alles in `hardware/cpu.py` und `pc.py`, gemessen mit
`tools/opstat.py`:

| | vorher | jetzt |
|---|---|---|
| Rohdurchsatz | 1,74 Mio Befehle/s | **3,11 Mio/s** |
| im Fenster nutzbar | ~0,83 Mio/s | **2,82 Mio/s bei 63 Bildern/s** |

1. Speicher als 32-Bit-Sicht (`memoryview(ram).cast("I")`) — ein Zugriff
   statt vier Bytes plus Schieben; krumme Adressen fallen auf den alten Weg
   zurück
2. Ausführungskette nach **gemessener** Häufigkeit sortiert (`push`/`pop`
   sind zusammen 40 % aller Befehle und standen an Stelle 13 und 14)
3. `rb`, `imm`, `simm` holt nur noch der Zweig, der sie braucht
4. Anstehende Interrupts und Haltepunktmenge in lokalen Variablen; die
   Halt-Prüfung nur noch dort, wo ein Halt entstehen kann
5. `pc.py` gibt der CPU das ganze Bild abzüglich Zeichenzeit statt fester
   8 ms

**Neu:** `tools/opstat.py` misst die Befehlshäufigkeit.

---

## Datenverlust beim Bauen

**Behoben — der schwerste Fehler dieser Runde.** `build.py` las das *ganze*
Plattenabbild, tauschte Bootsektor und Kernel und schrieb alles zurück. Lief
nebenher der Emulator (der seine Sektoren sofort in dieselbe Datei
schreibt), waren dessen Dateien danach auf dem Stand von vor dem Bauen —
Colins übersetzte Programme also weg. `tools/tbfs.py` hatte denselben
Fehler in `save()`.

Jetzt schreibt `build.py` nur Sektor 0 und die Kernelsektoren (`r+b` mit
gezielten `seek`s) und fasst das Dateisystem ab Sektor 512 nicht mehr an.
`tbfs.py` merkt sich in `self.dirty`, welche Sektoren es geändert hat.

---

## Schreibtisch: Symbole, Doppelklick, Ziehen

**Neu**

- **`\DESKTOP` als echter Ordner** trägt die Symbole — kein Sonderfall, die
  Kommandozeile sieht ihn wie jeden anderen
- Symbole **frei verschiebbar**, Anordnung in `\DESKTOP\ICONS.DAT`
  (`icon_pos[]`, ein Wort je Verzeichniseintrag)
- **Doppelklick öffnet** (Liste wie Symbol), `eintrag_oeffnen` ist die eine
  Stelle, die entscheidet was das heißt
- Ziehen: aus einem Fenster auf den Schreibtisch verschiebt nach `\DESKTOP`,
  vom Schreibtisch auf ein Dateifenster in dessen Ordner
- Symbole neu gezeichnet: Blatt mit Eselsohr und farbigem Endungsstreifen,
  Ordner mit Reiter, Programm als Fenster mit Startpfeil
- Dateien **verschieben** über `fs_move` — im TBFS nur ein geändertes Feld
  im Verzeichniseintrag, kein Sektor wird bewegt
- Knopfleiste der Dateiverwaltung von sechs auf vier Knöpfe, nach
  Häufigkeit sortiert, Positionen in *einer* Tabelle (`fb_x`, `fb_breite`)

**Behoben**

- **Klicks landeten im falschen Fenster**: gezeichnet wurde nach
  Stapelreihenfolge, geprüft nach Fensternummer
- **Dateiverwaltung zeigte hart 11 Zeilen ohne Blättern** — in `\SOURCE`
  waren die letzten drei Dateien unsichtbar
- **Endloses Scrollen** im Editor-Auswahlbildschirm: nur die untere Grenze
  war geprüft
- **Doppelklick auf ein Programm im Schreibtischordner** scheiterte mit
  „is not recognized" — der Suchpfad ist aktueller Ordner → `\SYSTEM` →
  `\PROGS`, `\DESKTOP` steht nicht darin. `eintrag_oeffnen` setzt jetzt
  vorher `cwd`
- **Grafische Programme übermalten den Schreibtisch**: wer den
  Bildschirmmodus umschaltet, bekommt jetzt den ganzen Schirm (`gui_fremd`),
  und `term_aktiv` wird dabei abgeschaltet — sonst kam keine Taste an
- **`New` behielt den Dateinamen** — `Save` hätte die geöffnete Datei
  geleert
- **Text lief aus den Knöpfen**: `g_button` zentriert nur, es kürzt nicht
- **Symbolname lief aus dem Bild**: gekürzt wurde nur die Mittenrechnung
- **Das × saß nicht mittig**: die Zeichensatzmuster sind 5×7 in einer
  8×8-Zelle; Kreuz und Vollbildzeichen werden jetzt selbst gemalt
- **Zahl über Beschriftung** im Uhrfenster

---

## Editor, Terminalfenster, Übersetzungsfenster

**Neu**

- **Startbildschirm** des Editors: neue Datei (`.C`, `.ASM`, `.PY`, `.MD`
  mit Vorlage) oder vorhandene öffnen, `Up` direkt an der Liste,
  `< Back` in der Knopfleiste
- **Maus im Editor**: Klick setzt die Schreibmarke, Ziehen markiert,
  Mausrad blättert (neuer Port `0x63` fürs Rad)
- **Zwischenablage** bei `0x130000`, `Strg+C/X/V/A`; `Cmd+C`/`Cmd+V`
  tauschen mit der macOS-Zwischenablage
- **Fenstergröße und Vollbild** für TOOBAD-OS-Fenster *und* das
  Emulatorfenster
- **Übersetzungsfenster** mit Balken, Prozent und Statuszeile; `cc.c` meldet
  seine drei Phasen über Syscall 29
- **Zurückblättern im Terminalfenster**: Ringpuffer bei `0x124000`,
  200 Zeilen, `term_sicht()` rechnet über einen gedachten Gesamtstrom

**Behoben**

- **`WIN` im Terminalfenster** startete einen zweiten Schreibtisch — und
  `gui_running` wurde nur beim Menüpunkt *Exit* zurückgesetzt, nicht bei ESC
- **`continue` sprang am Neuzeichnen vorbei**: Startmenü → Editor öffnete
  das Fenster, der Bildschirm zeigte weiter das Menü

---

## Programme und Werkzeuge

**Neu**

- **`programs/gfxlib.c`** — Grafik für eigene Programme: Blitter, Schrift,
  vergrößerte Schrift direkt in den Bildspeicher, Knöpfe, Maus
- **Syscall 30** gibt die Adresse des Zeichensatzes
- **`programs/calc.c`** — Taschenrechner, rechnet in Tausendsteln, weil der
  TB-32 kein Fließkomma hat
- **`programs/crash.c`** — Stresstest und Fehlerinjektor: Burn-in bis zur
  Drosselung, Farbchaos, Flackern, dazu fünf echte Abstürze. Läuft mit `/B`
  im Hintergrund, während der Schreibtisch weiterläuft. Absichtlich **nur
  als Quelltext** ausgeliefert (`NUR_QUELLTEXT` in `build.py`)
- **`.PY` startet unter eigenem Namen** — die Shell hängt `PY.TBX` davor
- `screenshot.py` kann mit `--type "10.0:text|ENTER"` zu bestimmten Zeiten
  tippen

**Behoben**

- **`sleep()`/`beep()` froren die ganze Maschine ein**, wenn ein Programm
  sie über einen Systemaufruf erreichte: `hlt` wartet auf den Timer, aber
  bei `INT 0x40` sind die Interrupts gesperrt. `asm("sti")` in `lib.c`
- **Ein Hintergrundprogramm klaute die Tastatur** — `TASKLIST` kam als
  `ASKLIST` an. Mit `/B` gestartete Prozesse werden bei `getkey()` schlafen
  gelegt
- **`START X.TBX ARG /B` lief im Vordergrund**: `/B` wurde nur als zweites
  Wort erkannt
- **Der Kernel wuchs in die Puffer des Dateisystems** und überschrieb sein
  eigenes Verzeichnis — Fenstertitel standen als Dateinamen da. Puffer nach
  `0xB0000`, `build.py` prüft den Abstand
- Grenzen von `cc.c` gefunden und dokumentiert: kein `?:`, kein `asm()`,
  keine Variablen mitten im Block, höchstens 5 Argumente — und `#include`
  wird **auch in Kommentaren** gefunden

---

## Doku

- [[12 Abkuerzungen und Namen]] — was TBX, TBFS, TC, TCC, CC heißen sollen
- [[13 BIOS-Dienste und was fehlt]] — Dienstliste, Setup, Secure Boot und
  die bekannten Lücken
- Diese Seite

Verwandt: [[00 START HIER]], [[07 Fallstricke]]
