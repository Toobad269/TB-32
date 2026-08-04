# Custom BIOS

Eigene Firmware für den TB-32. Jeder Unterordner ist ein vollständiges
BIOS-Abbild mit eigenem Namen, eigener Quelle und eigenem Test.

| | |
|---|---|
| [`TB-LOCK/`](TB-LOCK/) | Das serienmäßige BIOS plus **Setup-Passwort**. Ohne Passwort kommt niemand mehr ins Setup |

## Warum das geht

Der BIOS-Chip des TB-32 ist austauschbar. Das Mainboard prüft beim
Einschalten nur zwei Dinge im Kopf des Abbildes — die Kennung `TBBI` und eine
Prüfsumme — und greift sonst zur Sicherung. Was dahinter steht, ist dem Board
egal. Wie ein eigenes BIOS aufgebaut sein muss, steht vollständig in
[`Doku/16 Eigenes BIOS schreiben.md`](../Doku/16%20Eigenes%20BIOS%20schreiben.md);
die kleinste lauffähige Vorlage ist `firmware/minimal.asm`.

## Ein eigenes BIOS bauen und benutzen

```bash
python3 "Custom BIOS/TB-LOCK/bauen.py"      # erzeugt TB-LOCK.bin
python3 "Custom BIOS/TB-LOCK/pruefen.py"    # startet den Rechner damit und prüft es
```

Aufgespielt wird es im laufenden Rechner: **DEL → Firmware → Flash BIOS from
File**. Das alte BIOS legt das Board vorher als Sicherung ab, und *Restore
Backup BIOS* holt es zurück — wer sich aussperrt, kommt also wieder heraus.

Die `.bin`- und `.sym`-Dateien sind **nicht** eingecheckt. Das ist im ganzen
Projekt so: was bei jedem Bau neu entsteht, gehört nicht ins Verzeichnis.
Ein Klon plus der Bauaufruf oben ergibt dasselbe Abbild, Byte für Byte.

## Ein weiteres dazulegen

Neuer Ordner, Quelldateien hinein, `bauen.py` daneben. Die Ordner sind
voneinander unabhängig: `TB-LOCK` hat seine eigene Kopie von `bios.asm`,
`setup.asm`, `video.asm` und `const.inc` und fasst `firmware/` nicht an. Wer
etwas am Serien-BIOS ändert, muss es also bewusst herüberholen — dafür kann
kein Versuch hier drin den normalen Rechner kaputtmachen.
