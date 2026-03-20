# THERMOSTAT TX TM1621B

This copy of the TX project is dedicated to the TM1621B LCD driver migration.

## nRF52832 -> TM1621B serial pins

- `P0.26` -> `TM1621B /CS`
- `P0.27` -> `TM1621B /WR`
- `P0.28` -> `TM1621B DATA`

## TM1621B hardware assumptions

- package: `SSOP48`
- `TM1621B /RD` tied to `3V3`
- `TM1621B VDD` tied to `3V3`
- `TM1621B VSS` tied to `GND`
- `TM1621B VLCD` comes from a contrast trimmer
- `TM1621B OSCI/OSCO` left open, internal RC selected in firmware

## LCD glass mapping

The LCD glass remains the same 8-pin panel used by the old direct COM/SEG drive:

- glass pin 1 -> `COM0`
- glass pin 2 -> `COM1`
- glass pin 3 -> `COM2`
- glass pin 4 -> `COM3`
- glass pin 5 -> `SEG0`
- glass pin 6 -> `SEG1`
- glass pin 7 -> `SEG2`
- glass pin 8 -> `SEG3`

## Notes

- The TX UI logic stays the same: temperature, setpoint, `CO`, and pairing `P`.
- The passive glass is no longer multiplexed directly by the nRF.
- `TM1621B` is now the only LCD driver layer.
