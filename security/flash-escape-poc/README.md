# Flash-Port Sandbox-Escape — Proof of Concept

Ein ganz normales, **unprivilegiertes** `.TBX`, das innerhalb der emulierten
Maschine (also in der Sandbox) läuft, legt beim Ausführen eine **echte Datei**
auf dem Desktop des Host-Rechners an:

```
~/Desktop/TB32_PWNED.txt
```

Kein Dialog, kein Klick, keine Rückfrage.

> ⚠️ **Bewusster Demonstrator auf dem eigenen Rechner.** Der Payload ist
> harmlos (eine Textdatei mit kurzem Text). Zweck ist, die Schwere des
> ungeschützten BIOS-Flash-Ports sichtbar zu machen — und den Bug danach zu
> fixen.

---

## Warum das geht

Der BIOS-Flash-Chip hängt an den Ports `0xB0`–`0xB2`. Auf dem TB-32 sind Ports
**nicht geschützt**: `IN`/`OUT` sind gewöhnliche Instruktionen ohne
Ring-/Kernel-Prüfung (`hardware/cpu.py`, Opcodes `0x60`–`0x63`), und es gibt
kein Port-Rechtesystem. `prog_start.asm` sagt es selbst:

> „Ports auf dem TB-32 sind nicht geschützt — ein Programm darf sie direkt
> bedienen.“

Jedes `.TBX` kann also `portout(0xB0, …)` ausführen. Der Flash-Chip kennt u. a.:

| Befehl | Wirkung |
|---|---|
| `1` | Host nach einer Datei fragen (`osascript choose file`) → Inhalt in den Puffer |
| `2` | Puffer in den Gast-RAM kopieren |
| `5` | Gast-RAM in den Puffer kopieren |
| `3` | Puffer auf die **Host-Datei** `firmware/bios.bin` brennen |

Daraus ergeben sich zwei reale Escapes (Details siehe unten, „Der echte Bug“).

---

## Was dieser PoC hinzufügt — und was ehrlich dazugehört

Die **echten** Escapes können Host-Dateien nur *lesen* (Befehl 1+2, mit
Datei-Dialog) oder die *feste* Datei `firmware/bios.bin` *überschreiben*
(Befehl 5+3). **Keinen freien Pfad.** Eine Datei genau auf `~/Desktop/…`
ist mit dem Bug *wie er ist* nicht erreichbar.

Damit der PoC die **Worst-Case-Schwere** zeigt, wurde dem Flash-Chip **ein
Befehl hinzugefügt** — bewusst in einer eigenen Datei, als Unterklasse, damit
das echte Gerät (`hardware/devices.py`) **unangetastet** bleibt und der
Demonstrator in einem Stück löschbar ist:

- **`hardware/flash_poc.py`** *(neu)* — `FlashPoC(Flash)` mit dem neuen Port
  `PORT_FLASH_PATH = 0x00B3` (RAM-Adresse eines NUL-terminierten Host-Pfads)
  und **Befehl 12**: „schreibe den Puffer an den angegebenen Host-Pfad“
  (`open(expanduser(pfad), "wb").write(puffer)`).
- **`hardware/machine.py`** — nutzt `FlashPoC` statt `Flash` und registriert
  den neuen Port. Sonst nichts.

`hardware/devices.py` und `hardware/isa.py` bleiben **im Auslieferungszustand**.

> **Wichtig, damit nichts überzeichnet wird:** Befehl 12 ist **kein**
> echtes Hardware-Feature und **nicht** das, was der gefundene Bug kann. Er
> demonstriert, wie schlimm der ungeschützte Flash-Port *wäre*, wenn das Gerät
> nur eine Idee mächtiger wäre. Der reale, heute vorhandene Schreib-Bug ist auf
> `firmware/bios.bin` beschränkt.

Das eigentliche Angreiferprogramm **`programs/escape.c`** braucht dagegen
**keinen** Sondercode — nur die schon vorhandenen `portout`/`portin`:

```c
portout(F_ADDR, (int)text);     /* Quelle im RAM            */
portout(F_SIZE, strlen(text));  /* Länge                    */
portout(F_CMD, 5);              /* RAM  -> Flash-Puffer      */

portout(F_PATH, (int)pfad);     /* Ziel-Host-Pfad           */
portout(F_CMD, 12);             /* Puffer -> Host-Datei      */
```

---

## Bauen & Ausführen (macOS)

`build.py` kompiliert jedes `programs/*.c` automatisch zu einem `.TBX` und
legt es in `\PROGS` ab — es taucht dann im Start-Menü auf.

```bash
python3 build.py
python3 pc.py
```

In der Maschine:

- **Start ▸ ESCAPE PoC** anklicken (oder in der Command Prompt `START ESCAPE.TBX`),

und danach auf dem Mac nachsehen:

```bash
cat ~/Desktop/TB32_PWNED.txt
```

Erwartet: die Datei existiert und enthält den kurzen Text — geschrieben aus der
Sandbox heraus.

> **Hinweis (C-Emulator):** Der schnelle C-Emulator (`emu/`) kennt Befehl 12
> nicht — er wurde bewusst nur im Python-Pfad ergänzt, den `pc.py` benutzt.
> Der PoC läuft über `python3 pc.py`.

---

## Der echte Bug (ohne PoC-Erweiterung)

Auch ohne Befehl 12 ist der Flash-Port ein Loch:

1. **Host-Datei lesen (Confused Deputy).** Befehl 1 öffnet auf dem Mac einen
   echten `choose file`-Dialog; wählt der Nutzer eine Datei, landet ihr
   kompletter Inhalt im Puffer und via Befehl 2 im Gast-RAM. Ein harmlos
   wirkendes TBX kann so an eine echte Host-Datei kommen. Diese Variante
   überlebt sogar die `FLASH_LOCK`-Sperre des COMPANY-OS-BIOS, weil die Sperre
   nur die Schreib-Befehle (3/4/6/8) prüft, nicht 1/2.
2. **Host-Datei schreiben (ohne Interaktion).** Befehl 5 füllt den Puffer aus
   beliebigem Gast-RAM, Befehl 3 brennt ihn auf `firmware/bios.bin` — kein
   Dialog, kein Klick. Beim Standard- und beim TB-LOCK-BIOS ist das offen
   (nur COMPANY-OS sperrt vorm Boot).

---

## Der Fix (nächster Schritt)

Kernproblem: Der Host-Wähler ist die ganze Session aktiv, der Flash-Port ist
unprivilegiert, und die Sperre deckt nur den Schreib-Pfad. Mögliche Maßnahmen:

- Den `waehler` (Datei-Dialog) **nur im Setup-Kontext** setzen, nicht im
  laufenden OS (`pc.py`).
- Auch die Puffer-/Lese-Befehle (1/2/5) **hinter das `FLASH_LOCK`-Latch**
  legen, sodass ein einmal gesperrter Chip komplett dicht ist.
- Den gesamten Flash-Port als **privilegiert** behandeln (nur BIOS/Setup darf
  ihn bedienen), statt ihn jedem `.TBX` offen zu lassen.

## Aufräumen

Diesen PoC nach dem Test wieder entfernen:

- `programs/escape.c` und `hardware/flash_poc.py` löschen,
- in `hardware/machine.py` wieder `Flash` statt `FlashPoC` verwenden und
  `PORT_FLASH_PATH` aus der Port-Registrierung + dem Import nehmen.

`hardware/devices.py` und `hardware/isa.py` müssen nicht angefasst werden — sie
wurden nie verändert.

---

## Status dieser Session

Der Code wurde geschrieben und durch Lesen gegen Compiler und Toolchain
geprüft, aber in **dieser** Session **nicht gebaut oder ausgeführt** (die
Ausführungsumgebung hat das Starten von Shell-Befehlen unterbunden). Der erste
echte Lauf passiert bei dir auf dem Mac.
