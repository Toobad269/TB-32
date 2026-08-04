# Desktop-Aufbau

Alles in `system/gui.c`. Grafikmodus 640×400, gezeichnet über den Blitter der
Grafikkarte (Ports `0x44`–`0x4C`) — nie Pixel für Pixel über den Bus, das ist
hundertfach langsamer.

## Zeichenfunktionen

`g_fill(x,y,w,h,farbe)`, `g_frame`, `g_char(x,y,zeichen,farbe,bg)`,
`g_text`, `g_num`, `g_num2`, `g_panel(…,gedrueckt)`, `g_button`.
`bg = 256` bedeutet durchsichtig.

Der Zeichensatz kennt nur 32–127. Blockzeichen selbst als Rechteck malen.

## Fenster

`win_type[]` `win_x[] win_y[] win_w[] win_h[]`, `win_top` = vorderstes.
`MAXWIN` = 6. `starte(typ, titel, w, h)` öffnet oder holt nach vorn und
**begrenzt die Position auf den Bildschirm**.

Jedes Fenster hat in der Titelleiste zwei Knöpfe: **Vollbild** (12 px breit,
bei `win_w - 30`) und **Schließen** (bei `win_w - 16`). `win_vollbild(i)`
merkt sich die alten Maße in `win_ax/ay/aw/ah` und setzt `win_voll[i]`.

Unten rechts sitzt der **Anfasser** zum Ziehen (12×12, nur wenn nicht
Vollbild). Er wird **nach** dem Anwendungsinhalt gezeichnet, sonst malt die
Anwendung darüber. Beim Ziehen hält `groesse_zieht` die Fensternummer;
Mindestmaß 160×80, begrenzt auf Bildschirm und Startleiste.

Wichtig: Die Abfrage der Ecke steht **vor** allen anderen Treffern in der
Klickkette — sonst fängt die Anwendung den Klick ab.

| Typ | Anwendung |
|---|---|
| 1 | File Manager (Up/Move/Delete/**Open/Run**, Doppelklick öffnet) |
| 2 | Clock |
| 3 | System Monitor (Prozesse, Platte, **Temperatur**) |
| 4 | About |
| 6 | Control Panel (CMOS + Lüftermodus) |
| 7 | **Command Prompt** (siehe unten) |
| 8 | **Editor** |
| 9 | **Compiling** — Fortschrittsfenster, öffnet und schließt sich selbst |
| 10 | **Paint** — Zeichenprogramm, `system/paint.c` |
| 11 | **Word** — Textverarbeitung, `system/word.c` |

## Startleiste und Menü

Links `Start` (öffnet das Menü), daneben je ein Knopf für jedes offene
Fenster (vorderstes erscheint gedrückt), rechts die Uhr. Menüeinträge in
`menu_text()`.

**Reihenfolge (MENU_ANZ = 10):**

| Nr | Eintrag |
|---|---|
| 0 | File Manager |
| 1 | Command Prompt |
| 2 | Editor |
| 3 | System Monitor |
| 4 | Control Panel |
| 5 | Paint |
| 6 | Word |
| 7 | Clock |
| 8 | About TOOBAD-OS |
| 9 | Exit desktop |

**Achtung bei Klicktests:** Das Menü wächst nach **oben**. Die Position des
*n*-ten Eintrags ist

```
MENU_TOP = BAR_Y - (MENU_ANZ * MENU_ZH + 10)      # 378 - (10*14 + 10) = 228
y        = MENU_TOP + 6 + n * 14
```

Jeder neue Menüpunkt verschiebt **alle** Klickkoordinaten um eine Zeile.
`tools/selftest.py` rechnet sie deshalb aus `MENU_ANZ` aus, statt sie fest
einzutragen — genau daran sind schon zweimal Tests gescheitert.

## Rechte Maustaste

Die Maus liefert in Port `0x62` Bit 0 links, Bit 1 Mitte, **Bit 2 rechts**.
Der Schreibtisch merkt sich in `gui_taste`, welche Taste den letzten Klick
ausgelöst hat. Word wertet das aus und klappt sein Farbmenü auf.

## Zurückblättern im Terminalfenster

Die Zeilen, die oben herauslaufen, wandern in einen Ringpuffer bei
`0x00124000` (200 Zeilen à 70×2 Byte). `term_sb_push()` sichert die oberste
Zeile, bevor `term_scroll()` sie überschreibt.

`term_sicht(i, view)` liefert die Adresse der *i*-ten sichtbaren Zeile.
Die Rechnung läuft über einen gedachten Gesamtstrom aus Ringpuffer + aktuellem
Bild — deshalb ist der Übergang nahtlos und es braucht keine Sonderfälle.
Mausrad blättert (`term_view`), jede Taste holt nach vorn.

## Terminalfenster

Die Shell läuft als **eigener Prozess** (`term_main` in `system/term.c`).

- Ausgabe: `term_aktiv` schaltet in `lib.c` und `syscall.c` die Ausgabe in den
  Puffer bei `0x120000` um
- Eingabe: Die GUI-Schleife holt Tasten und schiebt sie mit `term_push_key()`
  weiter, wenn das Terminalfenster vorn ist
- Beim Verlassen des Desktops **muss** der Prozess beendet und `term_aktiv`
  zurückgesetzt werden
- Der Puffer ist fest 70×22. `app_term` malt nur so viele Zeilen und Spalten,
  wie ins Fenster passen — sonst schreibt der Text über den Rand hinaus
- `WIN` im Terminalfenster startet **keinen** zweiten Desktop: `kernel.c`
  fragt `gui_running` ab. Deshalb muss `gui_main()` `gui_running = 0` auch
  beim Verlassen mit ESC setzen, nicht nur beim Menüpunkt *Exit*

## Editorfenster

Nutzt die Editierfunktionen aus `system/edit.c` (`ed_insert`, `ed_backspace`,
`ed_line_of` …). Eigen sind nur Zeichnen und Tastenverteilung.

## Symbole auf dem Schreibtisch

**„Auf dem Schreibtisch liegen" heißt: im Ordner `\DESKTOP` liegen.** Damit
ist es kein Sonderfall — die Kommandozeile sieht den Ordner, die
Dateiverwaltung sieht ihn, und Verschieben ist dieselbe `fs_move`-Funktion
wie sonst. `desk_ordner()` legt ihn beim ersten Zeichnen an, falls er fehlt.

- `desk_index(n)` liefert den n-ten Eintrag, `desk_symbol` malt je nach Typ:
  **Blatt** mit Eselsohr und farbigem Endungsstreifen (`.C` rot, `.ASM` gelb,
  `.PY` grün, `.MD` türkis), **Ordner** mit Reiter und hellerer Vorderseite,
  **Programm** als kleines Fenster mit Titelleiste und grünem Startpfeil.
  Schräge Kanten entstehen als Treppe aus `g_fill`-Rechtecken
- Der Name wird auf **11 Zeichen gekürzt** (`kurzname`) und links wie rechts
  auf den Bildschirm geklemmt. Vorher rechnete nur die Mitte mit 10 Zeichen,
  gezeichnet wurde der ganze Name — bei einem Symbol am linken Rand stand
  dann „EADME.TXT" da
- **Position:** `icon_pos[]` hat ein Wort je Verzeichniseintrag — unten `x`,
  oben `y`, **0 heißt „nie angefasst"** und ergibt den Rasterplatz
  (`desk_raster_x/y`, 7 Spalten à 84×62). `desk_setzen()` begrenzt auf den
  Bildschirm und weicht dem Wert 0/0 aus, damit „gesetzt" eindeutig bleibt
- Gespeichert wird die Tabelle als `\DESKTOP\ICONS.DAT` (512 Byte).
  `icon_laden`/`icon_speichern` schalten `cwd` kurz auf den Schreibtischordner
  um, weil `fs_read`/`fs_write` immer im aktuellen Ordner arbeiten. Die Datei
  selbst wird in `desk_index` übersprungen, sonst läge sie als Symbol herum
- `desk_treffer(mx,my)` sagt, auf welchem Symbol die Maus liegt
- Symbole hinter einem Fenster sind nicht anklickbar — dafür fragt die
  Klickkette zuerst `win_unter(mx,my)`

## Grafische Programme bekommen den ganzen Schirm

Ein Programm, das den Bildschirmmodus umschaltet, kann nicht in ein Fenster
passen — es malt auf dieselbe Fläche wie die Oberfläche. Deshalb tritt der
Schreibtisch für die Dauer ab:

- `syscall.c`, Funktion 17: schaltet ein Programm auf Grafik, während
  `gui_running` gilt, wird `gui_fremd = 1` gesetzt und `term_aktiv = 0`.
  Das zweite ist entscheidend — sonst holt sich das Programm seine Tasten aus
  der Warteschlange des Terminalfensters, die aber die schlafende Oberfläche
  füllt: es käme keine einzige Taste an
- zurück in den Textmodus heißt „fertig" → `gui_fremd = 2`
- die Hauptschleife zeichnet bei 1 gar nichts und liest keine Tasten, bei 2
  stellt sie Grafikmodus, Zeichensatz und Mauszeiger wieder her und malt neu
- `gui_selbst` schützt die eigenen Moduswechsel der Oberfläche davor, sich
  selbst zu melden (`gui_ausfuehren` setzt es für seine ganze Dauer)

## Doppelklick und Ziehen

`klick_was` + `klick_zeit` merken den letzten Klick; zwei Klicks auf dasselbe
Ziel innerhalb von **50 Ticks (0,5 s)** sind ein Doppelklick. Zeilen der
Dateiverwaltung zählen ab 0, Schreibtischsymbole ab 1000 — so können sich
die beiden nicht verwechseln.

`eintrag_oeffnen(idx)` ist die *eine* Stelle, die entscheidet, was „öffnen"
heißt: Ordner betreten, `.TBX`/`.PY` **im Vollbild** starten (`gui_ausfuehren`),
alles andere in den Editor. Im Fenster läuft nur, was man in der
Kommandozeile selbst eintippt — dort *ist* das Fenster die Shell. **Vorher wird `cwd` auf den Ordner der Datei gesetzt** — der
Programm-Suchpfad ist aktueller Ordner → `\SYSTEM` → `\PROGS`, und
`\DESKTOP` steht nicht darin. Doppelklick in der Liste und auf dem Symbol rufen beide sie
auf — es gibt keinen zweiten Weg, der auseinanderlaufen könnte.

**Ziehen:** `files_click` gibt **2** zurück, wenn der Klick auf einer Zeile
lag; dann merkt sich die Hauptschleife `zieh_idx`. Auf dem Schreibtisch merkt
sie zusätzlich `zieh_sym` und den Griffpunkt (`zieh_dx/dy`), damit das Symbol
unter dem Zeiger bleibt und nicht springt. Beim Loslassen entscheidet
`win_unter(mx,my)`:

| losgelassen über | Wirkung |
|---|---|
| freiem Schreibtisch, Datei kam aus einem Fenster | `fs_move` nach `\DESKTOP` + Position setzen |
| freiem Schreibtisch, Symbol lag schon dort | nur neue Position, `icon_speichern()` |
| einem File-Manager-Fenster | `fs_move` in dessen Ordner, Position löschen |

**Knopfleiste der Dateiverwaltung** steht in *einer* Tabelle
(`fb_breite()`, `fb_x()`, `fb_text()`) — Zeichnen und Klicken lesen dieselbe,
also können sie nicht auseinanderlaufen. Nur noch vier, nach Häufigkeit
sortiert: `Up`, `Move`/`Drop`, `Delete`, `Open/Run` (letzterer nimmt den
ganzen Bildschirm, für grafische Programme). Öffnen und Starten macht
der Doppelklick; ein eigener Knopf dafür wäre eine Doppelung.
Der **Text Viewer** (Fenstertyp 5) ist dabei weggefallen — er war nur noch
über „Open" auf einer `.TBX` erreichbar und zeigte Maschinencode als Text. Knöpfe, die nicht mehr ins Fenster
passen, werden weggelassen statt über den Rand gemalt.

**Programme im Fenster** (`gui_im_fenster`): Das Terminalfenster *ist* eine
echte Shell mit eigenem Bildspeicher und eigener Tastatur. Statt eine zweite
Ausführungsumgebung zu bauen, öffnet die Funktion das Fenster und **tippt den
Befehl hinein** (`term_push_key`). Die Shell startet das Programm als ihr
Kind, die Ausgabe landet über `term_aktiv` von selbst im Fenster. Wichtig:
Die Eingabetaste muss als `13 + (K_ENTER << 8)` kommen — die Shell prüft den
**Tastencode**, ein blankes 13 erkennt sie nicht.
Grafische Programme gehören *nicht* hierhin, die schalten den Bildschirmmodus
um; dafür bleibt `Run` mit `gui_ausfuehren`.

**Verschieben** (`fs_move` in `fs.c`): Elternordner im Verzeichniseintrag
ändern, sonst nichts — keine Sektoren werden angefasst. Zwei Klicks:
*Move* nimmt auf (`move_quelle`, die Pfadzeile zeigt „Moving: …", der Knopf
heißt jetzt *Drop*), im Zielordner legt *Drop* ab. Schlägt es fehl (Name
belegt, Ordner in sich selbst), bleibt die Datei aufgenommen.

**Dateiverwaltung:** `file_masse()` rechnet `file_rows` aus der Fensterhöhe
aus, `file_top` ist die erste sichtbare Zeile, `file_sel` der ausgewählte
Eintrag (Index in der *ganzen* Liste, nicht in der Anzeige — beim Zeichnen und
beim Klicken also immer `file_top + zeile` rechnen). Mausrad blättert, rechts
erscheint ein Rollbalken, sobald mehr Einträge da sind als Platz. Vorher
standen dort fest 11 Zeilen ohne Blättern: alles darunter war unsichtbar,
`\SOURCE` mit 14 Dateien zeigte die letzten drei nie an.

**Startbildschirm:** `edg_screen` = 0 zeigt statt des Textes die Frage
*„What do you want to do?"* — links vier Knöpfe für eine neue Datei (`.C`,
`.ASM`, `.PY`, `.MD`, jeweils mit kleiner Vorlage), rechts die Liste des
aktuellen Ordners (Ordner führen hinein, der `Up`-Knopf **direkt über der
Liste** wieder heraus — unten in der Ecke hatte ihn niemand gefunden).
`edg_liste_top` wird beim Rad **nach oben und nach unten** begrenzt, sonst
scrollt man endlos in ein leeres Feld, unten der
Ordner, in dem gespeichert wird. Der Knopf `< Back` in der Knopfleiste führt dorthin
zurück — ohne ihn kam man aus einer offenen Datei nicht mehr heraus. `New`
leert dagegen nur das Blatt und setzt den Namen auf `NEW.<Endung>`; den Namen
mitzunehmen wäre gefährlich, weil `Save` dann die geöffnete Datei leeren
würde. Es ist derselbe `cwd` wie in Dateiverwaltung und Kommandozeile —
wer hier den Ordner wechselt, wechselt ihn überall.

**Größe passt sich an:** `edg_masse(w)` rechnet `edg_cols`/`edg_rows` aus
`win_w`/`win_h` aus und muss vor jedem Zeichnen *und* vor jeder
Mausumrechnung laufen. `EDG_COLS`/`EDG_ROWS` sind nur noch Aliase darauf.

**Maus:** `edg_pos_aus_maus()` rechnet Bildschirmpunkte in eine Textstelle um
(`zeile = (my - texty - 3)/9 + edg_top`, `spalte = (mx - textx - 3)/8`) und
begrenzt auf das Zeilenende. Klick setzt die Schreibmarke und beginnt eine
Auswahl, `edg_zieht` hält sie beim Ziehen nach.

**Auswahl und Zwischenablage:** `ed_sel_von`/`ed_sel_bis` (−1 = keine),
invers gezeichnet. Puffer bei `CLIP_BUF = 0x130000`, Länge in `clip_len`.
Strg+C kopieren, Strg+X ausschneiden, Strg+V einfügen (ersetzt die Auswahl),
Strg+A alles. **`Strg+V` und `Cmd+V` sind dasselbe**: Liegt auf dem Mac etwas
in der Zwischenablage, schreibt `pc.py` es vorher direkt in `CLIP_BUF` und
setzt `clip_len` (`gast_clipboard_setzen`, Adressen aus `kernel.sym`). Tippen bei bestehender Auswahl ersetzt sie.

**Mausrad:** Port `0x63` liefert die aufgelaufenen Rasten und setzt sich beim
Lesen zurück. Beim Blättern wird `edg_folgen = 0` gesetzt — sonst zöge
`app_editor` den Ausschnitt sofort wieder zur Schreibmarke zurück. Tippen und
Klicken schalten das Nachführen wieder ein.

- Knopfleiste: `< Back  New  Save  Rename  Compile  Run`, rechts daneben das
  Statusfeld (Ladebalken, „Saved.", „Compiled: …" oder der Kürzel-Hinweis)
- `Compile` speichert, startet `CC`/`ASM` als **Hintergrundprozess** und
  öffnet das Fenster `APP_BUILD`: Quell- und Zielname, Balken aus
  `build_progress` (Syscall 28) und Statuszeile aus `build_status`
  (Syscall 29, `cc.c` meldet dort seine drei Phasen). Die Hauptschleife
  schließt das Fenster, sobald der Prozess weg ist — **außer bei Fehlern**:
  dann bleibt es stehen, heißt „Compiler messages" und zeigt die
  mitgeschriebene Ausgabe des Compilers (`cap_*` in `term.c`). Der
  Mitschnitt sitzt in `syscall.c`, weil Programme über Systemaufrufe
  ausgeben, nicht über die `print`-Funktionen des Kernels
- `Run` übersetzt bei Quelltexten erst und startet danach automatisch
  (`edg_run_danach`); `.PY` und `.TBX` laufen sofort
- Programme laufen **im Textmodus** (`gui_ausfuehren`), danach zurück zum
  Desktop — Textprogramme und Oberfläche vertragen sich sonst nicht

Verwandt: [[07 Fallstricke]], [[03 Dateien und Zustaendigkeiten]]
