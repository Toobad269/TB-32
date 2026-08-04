# Selbst-Compilierung (Bootstrapping)

**Bewiesen.** `programs/cc.c` liegt als `\SOURCE\CC.C` auf der virtuellen
Platte und wird vom eigenen Compiler übersetzt.

## Die Kette

| Stufe | Wer baut | Ergebnis |
|---|---|---|
| 1 | `tools/tcc.py` auf dem Mac | `\SYSTEM\CC.TBX` |
| 2 | `CC.TBX` auf dem TB-32 | `CC2.TBX` |
| 3 | `CC2.TBX` auf dem TB-32 | `CC3.TBX` |

Stufe 2 und 3 sind **byte-identisch** (66224 Byte). Damit ist der Compiler ein
Fixpunkt. Stufe 1 darf abweichen — sie stammt von einem anderen Compiler.

Nachprüfen: `python3 tools/bootstrap.py` (~5 min), oder von Hand:

```
CD SOURCE
CC  CC.C CC2.TBX
CC2 CC.C CC3.TBX
FC  CC2.TBX CC3.TBX      -> "no differences encountered"
```

## Was CC dafür können musste

- `#define` (Makrotabelle im Lexer) und `#include` (lädt **vom eigenen
  Dateisystem**, eine Ebene tief)
- Typumwandlungen `(char*)x` — erkannt am Typwort direkt nach der Klammer
- konstante Ausdrücke in Arraygrößen (`char n[MAX * LEN]`)
- globale Variablen mit Startwert — die Zuweisungen laufen als erzeugter Code
  vor `main()`, aufgerufen über eine nachgetragene Sprungmarke
- `sc()` als eingebauter Systemaufruf, damit `proglib.c` unverändert
  funktioniert
- `portout()` / `portin()` ebenfalls eingebaut (Nummern 98 und 97): sie
  erzeugen `outr` bzw. `inr` **direkt an der Aufrufstelle**, ohne Kernel.
  Auf dem Mac liefert `prog_start.asm` dieselben zwei Funktionen — beide
  Compiler kommen also aufs Gleiche, und `gfxlib.c` bleibt eine Datei für
  beide
- größere Tabellen: 256 Globale, 192 Funktionen, 3000 offene Sprünge

## Bauart von CC

Ein-Durchgang-Compiler mit direkter Codeerzeugung, ohne Syntaxbaum. Vier
Dinge werden nachgetragen (*Backpatching*): Vorwärtssprünge, Aufrufe später
definierter Funktionen, Adressen der Zeichenketten, Größe des Stackrahmens.

Der **lvalue-Trick**: Beim Parsen eines Namens ist noch unklar, ob gleich
`x = 5` (Adresse gebraucht) oder `y = x` (Wert gebraucht) folgt. Also bleibt
immer die *Adresse* in `r0` und ein Flag merkt sich das; `rvalue()` lädt den
Wert erst nach, wenn er wirklich gebraucht wird.

## Wenn `cc.c` geändert wird

Danach **beides** prüfen:

```bash
python3 build.py                  # TCC muss es noch übersetzen
python3 tools/bootstrap.py        # und es muss sich selbst noch übersetzen
```

Verwandt: [[04 Compiler TCC Grenzen]], [[06 Bauen und Testen]]
