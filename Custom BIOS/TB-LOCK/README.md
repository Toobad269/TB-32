# TB-LOCK

Das serienmäßige TOOBAD BIOS, erweitert um genau eine Sache: **ein Passwort
vor dem Setup.** Ohne das Passwort kommt niemand mehr an Takt, Bootgerät,
Secure Boot oder den Flash-Knopf.

```bash
python3 "Custom BIOS/TB-LOCK/bauen.py"      # baut TB-LOCK.bin
python3 "Custom BIOS/TB-LOCK/pruefen.py"    # 27 Prüfungen auf der echten Maschine
```

Aufspielen: **DEL → Firmware → Flash BIOS from File**, dann `TB-LOCK.bin`
auswählen und den Rechner aus- und wieder einschalten.

## Wie es sich bedient

Im Setup gibt es einen neuen Reiter **Password** zwischen *Security* und
*Firmware*:

| Zeile | |
|---|---|
| **Supervisor Password** | zeigt `Installed` oder `Not Installed` |
| **Set / Change Password** | einrichten oder ändern |
| **Clear Password** | wieder abschaffen |

**Einrichten:** ENTER auf *Set / Change Password*, das neue Passwort eintippen,
ENTER, dasselbe noch einmal, ENTER. Stimmen die beiden Eingaben nicht überein,
passiert nichts — man sperrt sich also nicht mit einem Tippfehler aus. Ein
leeres Passwort wird abgelehnt.

**Ändern:** dieselbe Zeile. Es kommt zuerst das alte Passwort dran, danach
zweimal das neue.

**Löschen:** *Clear Password*, dann das aktuelle Passwort. Danach steht das
Setup wieder offen.

Ein gesetztes Passwort gilt **sofort und dauerhaft** — es wird nicht erst mit
F10 bestätigt, und ESC („Exit Without Saving") nimmt es nicht zurück. Wer es
gerade zweimal eingetippt hat, erwartet, dass es gilt.

**Beim Start:** `DEL` oder `F2` führen nicht mehr direkt ins Setup, sondern an
ein Fenster *BIOS Setup is locked*. Drei Fehlversuche, dann bleibt das Setup
zu und der Rechner startet normal weiter. Das Passwort erscheint als Sterne.

## Was geändert wurde

Vier der fünf Dateien sind Kopien aus `firmware/`. Neu ist nur `passwort.asm`;
in den anderen stehen wenige Zeilen mehr.

| Datei | |
|---|---|
| `passwort.asm` | **neu** — Eingabe, Prüfsumme, Tor, die beiden Knöpfe |
| `bios.asm` | drei Zeilen: Name im Kopf, zweimal `setup_tor` statt `setup_main`, ein `.include` |
| `setup.asm` | der Reiter *Password*, drei `REG_`-Nummern, zwei Verteilerzweige |
| `const.inc` | `CM_PWFLAG`, `CM_PWSUM0..3`, `PW_BUF1/2` |
| `video.asm` | unverändert |

### Warum das Tor an **zwei** Stellen steht

`setup_main` wurde im Original an zwei Stellen gerufen: beim normalen `DEL`
während der Bedenkzeit — und vom **roten Secure-Boot-Bildschirm** aus, wenn
das Startabbild nicht mehr das bekannte ist. Genau dort liegt der Knopf
*Trust Current Boot Image*.

Wäre nur der erste Weg bewacht, gäbe es eine offene Hintertür: Startabbild
absichtlich kaputtmachen, roter Bildschirm, `DEL`, Setup ohne Passwort. Beide
Wege gehen deshalb über `setup_tor`. `pruefen.py` prüft genau das, indem es
Secure Boot einschaltet, den Bootsektor verändert und dann versucht, über den
roten Bildschirm hereinzukommen.

### Wo das Passwort liegt

In der Knopfzelle: `CM_PWFLAG` (0x20) sagt, ob eines gesetzt ist,
`CM_PWSUM0..3` (0x21–0x24) halten die Prüfsumme.

Die Plätze liegen **oberhalb von 0x1F** — das ist kein Zufall. `setup_backup`
sichert 0x10–0x1F und spielt es bei ESC zurück; ein Passwort dort wäre beim
Verlassen ohne Speichern stillschweigend wieder weg. Sie liegen aber
**unterhalb von 0x2E**, damit die Knopfzelle sie in ihre eigene Prüfsumme
einrechnet.

`Load Setup Defaults` (F5) rührt sie nicht an — wie bei einem echten Board.

## Zwei ehrliche Einschränkungen

**Die Knopfzelle.** Das CMOS ist die Datei `disk/cmos.bin`. Wer sie löscht,
ist das Passwort los. Beim echten Mainboard zieht man dafür die Batterie oder
steckt den Jumper um — es ist also Originaltreue und kein Fehler. Es heißt
aber: Das Passwort schützt gegen jemanden **am TB-32**, nicht gegen jemanden
**am Wirtsrechner**.

**Die Prüfsumme.** Gerechnet wird `h = h * 31 + Zeichen`, angefangen bei
`0x1234` — dieselbe Rechnung, die TOOBAD-OS für die Anmeldung benutzt und
`build.py` für den BIOS-Kopf. Das ist eine **Prüfsumme, keine
kryptografische Hash-Funktion**: wer die vier Byte aus `cmos.bin` liest,
findet in Sekunden ein anderes Passwort mit derselben Summe. Dieselbe
Ehrlichkeit gilt in `Doku/13` für Secure Boot, und aus demselben Grund: das
Prinzip ist echt, die Fälschungssicherheit nicht.

Was es **verlässlich** tut: verhindern, dass jemand im Vorbeigehen den Takt
verstellt, das Bootgerät ändert, Secure Boot abschaltet oder ein fremdes BIOS
brennt.

## Eine Falle beim Nachbauen

Befehle sind auf dem TB-32 fest vier Byte breit und werden ab einer durch vier
teilbaren Adresse geholt. `passwort.asm` wird **nach** `setup.asm` eingebunden,
und `setup.asm` hört mit seiner Zeichenkettentabelle auf — die endet auf einer
krummen Adresse. Ohne ein `.align 4` vor dem ersten Befehl fängt der ganze
Code zwei Byte versetzt an. Der Rechner startet dann noch, der POST läuft
sauber durch, und beim ersten `DEL` zerlegt es ihn mit *Invalid opcode*.

Genau so ist es beim ersten Bau passiert. Die Zeile steht jetzt drin und ist
kommentiert.
