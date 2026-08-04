# Compiler TCC — was die Sprache kann und was nicht

**Vor jeder Zeile in `system/*.c` oder `programs/*.c` lesen.** Diese Dateien
sehen aus wie C, werden aber von `tools/tcc.py` übersetzt. Was echtes C kann,
kann TCC noch lange nicht — und der Compiler meldet manches nicht als Fehler,
sondern erzeugt stillschweigend falschen Code.

## Es gibt NICHT

| Fehlt | Ersatz |
|---|---|
| `struct`, `union`, `enum`, `typedef` | parallele Arrays + Indexrechnung |
| `float`, `double`, `long`, `short`, `unsigned` als eigener Typ | alles ist `int` (32 Bit) oder `char` |
| Mehrdimensionale Arrays `a[y][x]` | `a[y * BREITE + x]` |
| String-Arrays `char* t[] = {"a","b"}` | Funktion mit `if`-Kette, die Zeiger zurückgibt |
| `switch` / `case` | `if`/`else if`-Kette |
| `do…while` | `while (1)` mit `break` |
| `&&=`, Komma-Operator in `for` | einzeln schreiben |
| `#ifdef`, Funktionsmakros | nur `#define NAME zahl` |
| Standardbibliothek | eigene Fassungen in `lib.c` bzw. `proglib.c` |

## Es gibt (und funktioniert zuverlässig)

`int`, `char`, `int*`, `char*`, Arrays, globale und lokale Variablen,
Funktionen mit **bis zu 5 Parametern**, Rekursion, `if`/`else`, `while`,
`for`, `break`, `continue`, `return`, alle Rechen- und Vergleichsoperatoren,
`&&` `||` `!`, `& | ^ ~ << >>`, `=` `+=` `-=` `*=` `/=`, `++` `--`,
`&x`, `*p`, `a[i]`, Zeichenketten, `'A'`, `?:`, `asm("...")`,
`#include "datei.c"`, `#define NAME zahl`, Kommentare beider Art.

## Stolperfallen, die stillschweigend falschen Code erzeugen

**Negative Zahlen** — `-1` funktioniert, aber im Projekt steht überall
`0 - 1`. Das ist historisch und schadet nicht; nicht „aufräumen".

**`#` am Zeilenanfang in einem Kommentar** — beide Präprozessoren wissen
jetzt, ob eine Zeile in einem Blockkommentar steht (`zeilen_im_kommentar()`
in `tcc.py`, `komm_folge()` in `cc.c`). Vorher wurde so eine Zeile gelöscht,
das `*/` verschwand und der Kommentar fraß echten Code — der teuerste Fehler
des Projekts, siehe [[07 Fallstricke]].

**Mehr als 5 Argumente** — TCC legt sie auf den Stack, die Assembler-Brücke
in `start.asm` erwartet sie aber in `r1`–`r5`. Bei `sys_*`-Funktionen also
**nie mehr als 5**. **`cc.c` auf dem Gerät kann überhaupt nur 5** (es macht
`for (i = argn; i >= 1; i--) e_pop(i)` und würde bei sechs in `r6` schreiben).
Programme unter `programs/`, die sich selbst übersetzen können sollen, bleiben
deshalb bei höchstens fünf.

**Globale Startwerte** funktionieren (`int x = 5;`), aber nur mit
konstantem Ausdruck. `char text[4] = {65,66,67,0}` geht, `char* s = "x"` nicht.

**Lokale Arrays** liegen im Stackrahmen. Große lokale Puffer (`char buf[4096]`)
sprengen ihn — solche Puffer gehören an feste Adressen, siehe
[[02 Speicherkarte und Ports]].

**Sichtbarkeit über Dateigrenzen**: Der Compiler sammelt *alle* Funktions- und
Variablennamen aus allen `#include`-Dateien **vorab**. Deshalb braucht es keine
Vorwärtsdeklarationen — und deshalb ist eine Deklaration wie
`int term_aktiv;` in einer Datei plus `int term_aktiv = 0;` in einer anderen
ein **doppeltes Label** und bricht den Assembler ab. Nie doppelt deklarieren.

**Reihenfolge der `#include`** in `kernel.c` spielt für Namen keine Rolle, für
`#define` aber schon: Makros gelten erst ab ihrer Zeile.

## Wie man Compilerfehler erkennt

Der erzeugte Assembler liegt nach dem Bauen in `system/kernel.asm`. Bei
merkwürdigem Verhalten dort nachsehen — der Code ist gut lesbar, jede
C-Funktion beginnt mit `; ===== name() =====`.

`python3 tools/ctest.py --selftest` prüft 11 Sprachmerkmale direkt auf der
emulierten CPU. Wer am Compiler etwas ändert, lässt das **immer** laufen.

Ein einzelnes C-Schnipsel testen, ohne das ganze System zu bauen:

```bash
python3 tools/ctest.py meinprogramm.c --asm
```

## Der zweite C-Compiler

`programs/cc.c` ist ein **weiterer** C-Compiler, der auf dem TB-32 läuft und
sich selbst übersetzen kann. Er versteht ungefähr dieselbe Teilmenge wie TCC,
plus Typumwandlungen `(char*)x` und konstante Ausdrücke in Arraygrößen.
Wer `cc.c` ändert, muss danach **beides** prüfen: dass TCC es noch übersetzt
*und* dass es sich selbst noch übersetzt ([[09 Selbst-Compilierung]]).

Verwandt: [[05 Konventionen]], [[07 Fallstricke]]
