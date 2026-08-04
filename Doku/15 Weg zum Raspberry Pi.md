# Weg zum Raspberry Pi

**Ziel:** TOOBAD-OS läuft auf einem echten Gerät — ohne Linux dahinter, und
**ohne dass der TB-32 verschwindet**.

Diese Seite hält den Plan, die Entscheidung dahinter und den Stand fest.
Stand: Schritt 1 fertig, alles Weitere wartet auf Colins Rückkehr aus dem
Urlaub.

---

## Die Entscheidung: emulieren, nicht portieren

Es gab zwei Wege, und die Wahl ist gefallen.

| | **Emulator in C, bare metal** (gewählt) | OS auf ARM portieren |
|---|---|---|
| Bildschirm, Timer, SD, USB selbst schreiben | ja | ja |
| Compiler bekommt ARM64-Rückende | — | ja |
| BIOS und `start.asm` in ARM-Assembler neu | — | ja, komplett |
| Blitter in Software nachbauen | — | ja, alle sieben Kommandos |
| Jedes `portout` in `gui.c`, `paint.c`, `word.c` neu | — | ja |
| TOOBAD-OS muss geändert werden | **kein einziges Byte** | überall |

Die schwere Stelle (Bare-Metal-Treiber, vor allem USB) ist bei **beiden**
Wegen dieselbe. Der Portierungsweg ist derselbe Berg **plus** vier weitere.

Und der entscheidende Punkt: Bei einer ARM-Portierung wäre der **TB-32 weg**
— der eigene Befehlssatz, der Assembler, **CC** (der Compiler, der auf dem
Gerät selbst läuft und sich selbst übersetzt), das Bootstrapping. Übrig
bliebe „ein OS auf fremder Hardware".

Beim Emulator-Weg bleibt alles erhalten: **der Emulator ist dann das
Mainboard.** Er nimmt die echte Pi-Hardware und stellt dem TB-32 seine
gewohnten Ports hin. Die OS-Seite merkt nichts.

---

## Die Schritte

### Schritt 1 — Emulator in C ✅ **fertig**

`emu/` neben `hardware/`. Siehe [[14 Aenderungsjournal]] und
[[06 Bauen und Testen]].

- `emu/cpu.c` — alle 57 Befehle
- `emu/machine.c` — Bus, Grafikkarte mit Blitter, Platte, Tastatur, Timer,
  CMOS, Blockkopierer, Wärme
- `emu/main.c` — kopfloser Start zum Vergleichen
- `tools/emu_vergleich.py` — prüft C gegen Python, Befehl für Befehl

Gemessen: **1,8 → 287 Millionen Befehle je Sekunde**, Faktor 160.
TOOBAD-OS bootet, der Bildschirm ist Zeichen für Zeichen gleich.

### Schritt 1b — Fenster für die C-Fassung (offen)

SDL2 statt pygame, damit man den C-Emulator wie `pc.py` benutzen kann.
Tastatur, Maus, Bildausgabe. Danach ist die Python-Fassung nur noch
Referenz zum Vergleichen.

### Schritt A — Pi mit Linux (offen, ohne Risiko)

Über SSH das Projekt auf den Pi, dort übersetzen, im Vollbild auf HDMI
starten. Colins Linux bleibt völlig unberührt (`rm -rf` und es ist weg).

**Warum diese Zwischenstufe nicht übersprungen wird:** Sonst debuggt man
zwei unbekannte Sachen gleichzeitig — den Emulator auf ARM *und* Bare
Metal. So sieht man erst, ob der Emulator auf ARM richtig rechnet.

### Schritt B — Bare Metal (offen)

Der Pi bootet `kernel8.img` direkt: darin steckt der TB-32-Emulator als
nativer ARM-Code. Kein Linux.

---

## Wie ein Pi startet (kein Imager nötig)

Beim Einschalten läuft nicht die ARM-CPU zuerst, sondern der Grafikchip. Er
liest die **erste Partition der SD-Karte (FAT32)** und sucht feste
Dateinamen. Findet er `kernel8.img`, lädt er die Datei und startet die
ARM-Kerne hinein. Fertig — kein eigener Bootloader, kein Image-Format.

Auf einem Pi 4 sähe die Karte so aus:

| Datei | Woher |
|---|---|
| `start4.elf`, `fixup4.dat` | offizielle Pi-Firmware, unverändert |
| `config.txt` | schreiben wir, drei Zeilen |
| **`kernel8.img`** | **unser Emulator** |

**Der Pi 5 startet anders** (EEPROM-Bootloader, andere Firmware-Dateien) —
siehe unten.

---

## Der Ablauf mit Colins Karte

Colin hat **eine** SD-Karte, auf der Baronie, der Toobad-Server und SideEye
laufen. Er will nichts kaufen. Deshalb:

1. **Wir fassen die Linux-Partition nie an.** macOS mountet ohnehin nur die
   FAT32-Bootpartition — an ext4 kommt es gar nicht heran. Das ist die
   Rettung: kein rohes Schreiben, kein `sudo`, kein Risiko.
2. **Backup = die Bootpartition** (ein paar hundert MB statt 32 GB), gezogen
   wenn die Karte im Mac steckt. Nicht über SSH — das dauert Stunden.
3. Unser `kernel8.img` kommt **neben** das Vorhandene, umgeschaltet wird über
   eine Zeile in `config.txt`. Zurück zu Linux = diese Zeile zurückändern.
   Von `config.txt` vorher eine Kopie.

**Grenze:** Auf eine rohe Karte schreiben bräuchte `sudo` — Claude kann kein
Passwort eingeben. Auf die gemountete FAT-Partition kopieren geht ohne.

---

## Der Pi 5 ist der schwierigste Pi (Stand August 2026)

Colin hat einen **Pi 5**. Das ist ausgerechnet das Modell mit der dünnsten
Bare-Metal-Unterstützung, und der Grund ist der **RP1**: USB, Netzwerk und
GPIO hängen nicht mehr am Hauptchip, sondern an einem eigenen Chip hinter
PCIe. Für Bare Metal heißt das: PCIe hochfahren, RP1 ansprechen,
xHCI-Treiber, dann erst USB-Tastatur.

Recherchierter Stand von [Circle](https://circle-rpi.readthedocs.io/en/50.0/appendices/raspberry-pi-5.html)
(Bare-Metal-Treibersammlung für den Pi, **kein** Betriebssystem):

- **Bildschirm:** Firmware-Unterstützung für Framebuffer „weniger
  komfortabel" — keine Konfiguration über `config.txt`, Auflösung aus dem
  Programm heraus nicht setzbar
- **USB:** „sollte funktionieren", aber Berichte über Erkennungsprobleme
  beim Start — **besonders wenn HDMI angeschlossen ist**. Genau unsere
  Kombination.
- **Netzwerk:** ~~gar nicht~~ **geht inzwischen.** Circles README (Stand
  04.08.2026) führt für den Pi 5 sowohl „MACB / GEM Gigabit Ethernet NIC of
  Raspberry Pi 5" als auch „Wireless LAN access" als unterstützt. Die
  Appendix-Seite zum Pi 5 sagt dazu nichts und verweist auf das README --
  wer hier nachschaut, muss also das README lesen, nicht die Doku-Seite.
  Getestet laut Circle nur mit den BCM2712-Steppings C1 und D0.

### Was das praktisch heißt

- Ein **Pi 4** würde Schritt B von einem Monat mit ungewissem Ausgang auf
  etwa eine Woche bringen. Colin will keinen kaufen — akzeptiert.
- Auf dem Pi 5 gehen wir es über **Circle** an: damit müssen wir USB *nicht
  selbst* schreiben, und das war die Wand.
- Falls Circle zu wackelig ist: eigene Treiber. Dafür braucht es aber einen
  **USB-Seriell-Adapter (~10 €)**. Ohne serielle Ausgabe debuggt man bare
  metal blind — sobald der Pi ohne Linux startet, ist SSH weg.

---

## Was NICHT geht (mehrfach gefragt)

- **Vom USB-Stick auf dem Ryzen 7600X booten.** Der Ryzen ist x86-64, der
  TB-32 ist eine andere CPU. Kein Byte unseres Codes läuft dort. Das eigene
  BIOS „auf das Mainboard laden" scheitert zusätzlich an signiertem UEFI.
- **Claude Code oder brew auf dem TB-32.** Node.js-Programme für
  macOS/Linux auf x86/ARM; sie bräuchten POSIX, TCP/IP, TLS und eine
  JavaScript-Laufzeit — und selbst dann stimmte der Befehlssatz nicht.
- **Realistischer RAM-Speed im Emulator.** Gibt es nicht: jeder
  Speicherzugriff ist sofort fertig, kein Cache, keine Wartetakte.
  Gedrosselt wird nur die CPU über ihr Befehlsbudget. Wäre ein eigenes
  Stück Hardware zum Nachbauen.

## Was ginge, aber ein eigenes Projekt wäre

- **Netzwerk und ein einfacher Browser.** Netzwerkkarte als Gerät, eigener
  TCP/IP-Stapel in TB-32-Code, HTTP holen, einfaches HTML darstellen. HTTPS,
  JavaScript und CSS nicht.
- **FPGA.** Der TB-32 ist ein richtiger Prozessorentwurf — auf einem
  FPGA-Board (50–150 €) könnte man den Chip **wirklich bauen**. Das ist der
  ehrliche Weg für eine selbstgebaute CPU.

---

Verwandt: [[14 Aenderungsjournal]], [[06 Bauen und Testen]],
[[02 Speicherkarte und Ports]]
