# COMPANY-OS BIOS v1.0

Ein BIOS für Firmenrechner. Es kann zwei Dinge mehr als das serienmäßige:

1. **Das Setup ist mit einem Passwort gesperrt** (von [TB-LOCK](../TB-LOCK/)
   übernommen: Reiter *Password*, ENTER auf *Set / Change Password*).
2. **Es trägt einen Eigentümer-Eintrag** und legt ihn beim Start in den
   Speicher. Das Betriebssystem liest ihn dort und zeigt ihn an — oben rechts
   auf dem Schreibtisch und im Anmeldeschirm.

```
Controlled by Microsoft
```

## Warum das im BIOS steht und nicht im System

Weil es **unter** dem System liegt. Der Benutzer kann TOOBAD-OS zurücksetzen,
die Platte löschen oder neu aufsetzen — der Eintrag bleibt. Wegbekommen kann
ihn nur, wer ein anderes BIOS einspielt, und davor steht das Setup-Passwort.

Genau so machen es echte Firmenrechner: Dell und HP haben im BIOS ein Feld
„Asset Tag" / „Ownership Tag", das beim Start erscheint und das das
Betriebssystem auslesen kann.

## Wie das BIOS es dem System sagt

Bei echten PCs heißt das **SMBIOS**: die Firmware legt eine Tabelle in den
Speicher, das System liest sie aus. Hier ist es dasselbe, nur kleiner:

| Adresse | Inhalt |
|---|---|
| `0x00000500` | 32 Byte Eigentümer-Eintrag, mit Null abgeschlossen |
| `0x00000524` | Schalter; Bit 0 = Eintrag anzeigen |

Das serienmäßige BIOS **leert** diese Felder beim Start. Sonst bliebe der
Eintrag eines früher geflashten Firmen-BIOS stehen, und das System zeigte ihn
weiter an, obwohl die Firmware längst eine andere ist.

## Den eigenen Namen eintragen

In `bios.asm` die Zeile ändern und neu bauen — höchstens 31 Zeichen:

```asm
s_firma:      .db "Controlled by Microsoft", 0
```

```bash
python3 "Custom BIOS/COMPANY-OS/bauen.py"
```

Einspielen: im TB-32 `DEL` → *Firmware* → *Flash BIOS from File*.

## Die Falle, in die ich beim Bauen getreten bin

Der Name im Kopf des Abbildes muss **genau 32 Byte** lang sein. „COMPANY-OS
BIOS v1.0" sind 20 Zeichen, mit der Null 21 — also `.space 11`. Mit einem
Byte zu viel fängt der Code bei `0x31` statt `0x30` an: der Rechner springt
beim Einschalten mitten in einen Befehl und läuft ins Nichts, **ohne eine
einzige Meldung**. Genau dieselbe Falle wie beim Umbenennen auf v2.5.2.

## Was noch fehlt

Die Richtlinien (kein Compiler, kein Netz, Programme sperren, Passwortzwang)
sind im Schalterwort schon vorgesehen, aber noch nicht im Setup einstellbar
und im System noch nicht ausgewertet.

Und die ehrliche Grenze: Der TB-32 hat **keinen Speicherschutz**. Eine
Richtlinie ist damit eine Regel, keine Mauer — wer den Coder hat, kann sich
ein Programm schreiben, das die Ports selbst anspricht. Deshalb gehört
„Compiler sperren" in so ein BIOS zwingend dazu. Und wer an die Datei
`disk/cmos.bin` kommt, hat die Knopfzelle gezogen; bei einem echten
Mainboard ist das derselbe Handgriff.
