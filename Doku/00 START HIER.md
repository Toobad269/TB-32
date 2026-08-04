# TOOBAD TB-32 — Einstieg

Arbeitsreferenz für Claude. Bei Kontextverlust **zuerst diese Seite**, dann
gezielt die verlinkte Detailseite. Alles hier ist Stand des laufenden Systems,
nicht Wunschdenken — geprüft über `tools/selftest.py` (45/45 grün).

## Arbeitsregel

Nach jeder fertigen Änderung: **erst den Startbefehl nennen, dann sofort die
Doku nachziehen** — und jeden gefundenen Fehler, jede Änderung und jede neue
Funktion ins [[14 Aenderungsjournal]] eintragen, mit **Ursache**, nicht nur
mit Symptom. Colins ausdrücklicher Wunsch; nachgereichte Doku zählt nicht.

## Was das Projekt ist

Ein vollständiger virtueller PC in `~/Desktop/Projekte/PyPC/`. Colins Challenge
gegen andere KIs. **Kernprinzip: Python emuliert nur die Chips.** Alles Sichtbare
— BIOS, Bootvorgang, OS, Editor, Desktop — ist echter Maschinencode auf der
emulierten CPU. Wer das aufweicht, zerstört den Sinn des Projekts.

Der Compiler **übersetzt sich selbst** (Bootstrapping bewiesen, siehe
[[09 Selbst-Compilierung]]).

Seit August 2026 gibt es den Emulator **zusätzlich in echtem C** (`emu/`) —
160-mal schneller und die Grundlage dafür, den Rechner auf einem Raspberry
Pi ohne Linux zu starten. Der TB-32 bleibt dabei der Prozessor; getauscht
wird nur, was die Chips nachbaut. Plan und Stand: [[15 Weg zum Raspberry Pi]].

**Anwendungen im Schreibtisch:** File Manager, Command Prompt, Editor
(= „Coder", mit Syntaxfarben), System Monitor, Control Panel, **Paint**,
**Word**, Clock, About.

## Die drei Ebenen (nie verwechseln)

| Ebene | Sprache | Läuft auf |
|---|---|---|
| `hardware/`, `pc.py`, `tools/` | Python | dem Mac |
| `emu/` | **echtes C** | dem Mac (später dem Pi) |
| `firmware/*.asm`, `system/start.asm` | TB-32-Assembler | dem TB-32 |
| `system/*.c`, `programs/*.c` | TC (eigene C-Variante) | dem TB-32 |

**`emu/` ist echtes C für den Wirtsrechner, `system/*.c` ist TC für den
TB-32.** Beide heißen „C" und haben nichts miteinander zu tun. Verwechseln
kostet Stunden.

`system/*.c` und `programs/*.c` sehen aus wie C, werden aber von
**`tools/tcc.py`** übersetzt — dessen Grenzen stehen in
[[04 Compiler TCC Grenzen]]. **Das ist die häufigste Fehlerquelle beim Coden.**

## Sofort-Befehle

```bash
cd ~/Desktop/Projekte/PyPC
python3 build.py            # BIOS + Kernel + Programme + Platte
python3 pc.py               # starten (Fenster)
python3 tools/selftest.py   # 41 Prüfungen, ~2 min
python3 tools/ctest.py --selftest   # 11 Compilertests
python3 tools/bootstrap.py  # Selbst-Compilierung, ~5 min
python3 tools/emu_vergleich.py      # C-Emulator gegen Python-Fassung
```

Die C-Fassung des Emulators:

```bash
cd emu && make && cd ..
./emu/tb32 4.0 "dir"        # kopflos booten und einen Befehl tippen
```

Ohne Fenster testen (das Arbeitspferd):

```bash
python3 tools/headless.py 12 --keys "DIR,ENTER,TEMP,ENTER"
python3 tools/screenshot.py /tmp/x.png 10 --keys "WIN,ENTER" --mouse "6:25:387:click"
```

## Seiten

- [[01 Architektur TB-32]] — Register, Befehlssatz, Kodierung
- [[02 Speicherkarte und Ports]] — **alle Adressen**, Kollisionen vermeiden
- [[03 Dateien und Zustaendigkeiten]] — wer macht was
- [[04 Compiler TCC Grenzen]] — **vor jedem Coden lesen**
- [[05 Konventionen]] — Register, Aufrufe, Syscalls
- [[06 Bauen und Testen]] — Werkzeuge, GUI-Koordinaten für Klicktests
- [[07 Fallstricke]] — teuer erkaufte Erkenntnisse, nicht wiederholen
- [[08 Desktop Aufbau]] — Fenster, Menü, Knopfpositionen
- [[09 Selbst-Compilierung]] — Bootstrap-Kette
- [[10 Temperatur]] — Wärmemodell und Drosselung
- [[11 Offene Punkte]] — was als Nächstes ansteht
- [[12 Abkuerzungen und Namen]] — was TBX, TBFS, TC, CC … heißen sollen
- [[13 BIOS-Dienste und was fehlt]] — Dienstliste, Setup, Secure Boot
- [[14 Aenderungsjournal]] — **jede Änderung, jeder Fehler, jede neue Funktion**
- [[15 Weg zum Raspberry Pi]] — Plan, Entscheidung und Stand für echte Hardware

## Arbeitsweise mit Colin

Deutsch. Vor einem Fix **erst die verstandene Ursache schildern** und
reproduzieren, nicht drauflosreparieren. Die Oberfläche des Systems ist
**englisch**, die Quelltext-Kommentare **deutsch** — das ist so gewollt.
