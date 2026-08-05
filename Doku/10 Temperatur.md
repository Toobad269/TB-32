# Temperature and Throttling

Lives in the **hardware** (`class Thermal` in `hardware/devices.py`), not in
the operating system — like the chipset of a real mainboard.

## Model

```
Heat in  = 2.5 × clock(MHz) × load + 0.4
Heat out = (temperature - 22 °C) × (0.05 + fan% × 0.0025)
```

Load is `instructions executed / budget` per time slice. Heating uses the
**effective** clock (after throttling) — otherwise throttling wouldn't
accomplish anything.

Automatic fan: `25 + (temp - 40) × 2.5`, clamped to 20–100%.

## Measured steady state under sustained load

| Clock | Temperature | Fan | Throttling |
|---|---|---|---|
| 0.4 MHz | 36 °C | 20% | – |
| 1 MHz | 44 °C | 33% | – |
| 2 MHz | 52 °C | 53% | – |
| 4 MHz | 63 °C | 82% | – |
| 8 MHz | 86 °C | 100% | kicks in |
| Idle | 27 °C | 20% | – |

From 85 °C (`limit`) onward, throttling rises up to 80%; at 105 °C the
machine shuts down. The clock used for time slices is
`ips_soll × (100 - throttle) / 100`.

## Controls

- Ports `0xA0`–`0xA5`, see [[02 Speicherkarte und Ports]]
- Shell: `TEMP`, `TEMP AUTO | QUIET | FULL | RESET`
- Desktop: System Monitor (bars), Control Panel (fan mode switchable)
- Emulator: **F12** shows temperature, fan, and throttling

Related: [[01 Architektur TB-32]]


## Configurable in the BIOS

The **Cooling** tab in Setup (`DEL` at startup) sets the fan mode
(Automatic / Quiet / Full speed) and the throttle limit (60–100 °C in steps
of five), and shows temperature, fan, throttling, and the peak value live.
The values live in CMOS (`CM_FANMODE`, `CM_TEMPLIMIT`) and get passed on to
ports `0xA3`/`0xA4` during POST by `kuehlung_anwenden` — without that they'd
sit in Setup but have no effect at all.
