# Temperatur und Drosselung

Sitzt in der **Hardware** (`class Thermal` in `hardware/devices.py`), nicht im
Betriebssystem — wie der Chipsatz eines echten Mainboards.

## Modell

```
Wärme rein  = 2.5 × Takt(MHz) × Auslastung + 0.4
Wärme raus  = (Temperatur - 22 °C) × (0.05 + Lüfter% × 0.0025)
```

Die Auslastung ist `ausgeführte Befehle / Budget` je Zeitscheibe. Geheizt wird
mit dem **effektiven** Takt (nach Drosselung) — sonst brächte Drosseln nichts.

Lüfterautomatik: `25 + (Temp - 40) × 2.5`, begrenzt auf 20–100 %.

## Gemessene Ruhelage bei Dauerlast

| Takt | Temperatur | Lüfter | Drosselung |
|---|---|---|---|
| 0,4 MHz | 36 °C | 20 % | – |
| 1 MHz | 44 °C | 33 % | – |
| 2 MHz | 52 °C | 53 % | – |
| 4 MHz | 63 °C | 82 % | – |
| 8 MHz | 86 °C | 100 % | greift ein |
| Leerlauf | 27 °C | 20 % | – |

Ab 85 °C (`limit`) steigt die Drosselung auf bis zu 80 %, bei 105 °C schaltet
der Rechner ab. Der Takt für die Zeitscheiben ist
`ips_soll × (100 - throttle) / 100`.

## Bedienung

- Ports `0xA0`–`0xA5`, siehe [[02 Speicherkarte und Ports]]
- Shell: `TEMP`, `TEMP AUTO | QUIET | FULL | RESET`
- Desktop: System Monitor (Balken), Control Panel (Lüftermodus umschaltbar)
- Emulator: **F12** zeigt Temperatur, Lüfter und Drosselung

Verwandt: [[01 Architektur TB-32]]


## Im BIOS einstellbar

Der Reiter **Cooling** im Setup (`DEL` beim Start) stellt Lüftersteuerung
(Automatik / Leise / Volle Drehzahl) und die Drosselgrenze (60–100 °C in
Fünferschritten) ein und zeigt Temperatur, Lüfter, Drosselung und Höchstwert
live. Die Werte liegen im CMOS (`CM_FANMODE`, `CM_TEMPLIMIT`) und werden beim
POST von `kuehlung_anwenden` an die Ports `0xA3`/`0xA4` weitergereicht —
ohne das stünden sie im Setup, würden aber nichts bewirken.
