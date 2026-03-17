# Arduino LED Controller for mediasmartserverd

Firmware for an Arduino Pro Micro (ATmega32U4) that drives the HP MediaSmart
Server LED daughter board via USB serial, replacing the original SCH5127 chipset
GPIO control.

## Pin Mapping

| Function     | Arduino Pin | Notes                    |
|--------------|-------------|--------------------------|
| Bay 0 Blue   | 2           |                          |
| Bay 1 Blue   | 3           |                          |
| Bay 2 Blue   | 4           |                          |
| Bay 3 Blue   | 5           |                          |
| Bay 0 Red    | 6           |                          |
| Bay 1 Red    | 7           |                          |
| Bay 2 Red    | 8           |                          |
| Bay 3 Red    | 9           |                          |
| System Blue  | 10          | PWM-capable (brightness) |
| System Red   | 16          |                          |
| USB LED      | 14          |                          |

Pin assignments can be changed at the top of the `.ino` file.

## Wiring the Daughter Board

The original LED daughter board has resistors already in-line. Connect each LED
wire from the daughter board to the corresponding Arduino pin listed above.

**Important:** Before wiring, determine if the LEDs are common-anode or
common-cathode using a multimeter:

- **Common cathode** (shared ground): Set `ACTIVE_LOW = false` in the sketch.
  Connect the common pin to Arduino GND.
- **Common anode** (shared VCC): Set `ACTIVE_LOW = true` in the sketch.
  Connect the common pin to Arduino VCC (5V).

## Flashing

1. Install the Arduino IDE or `arduino-cli`
2. Select board: **Arduino Leonardo** (Pro Micro uses the same ATmega32U4)
3. Select the serial port (e.g., `/dev/ttyACM0`)
4. Upload `mediasmartserver_leds.ino`

With `arduino-cli`:
```bash
arduino-cli compile --fqbn arduino:avr:leonardo mediasmartserver_leds
arduino-cli upload --fqbn arduino:avr:leonardo --port /dev/ttyACM0 mediasmartserver_leds
```

## Serial Protocol

115200 baud, 8N1. Commands are newline-terminated ASCII. The daemon sends
commands; the Arduino only responds to Ping.

| Command    | Format                    | Example              |
|------------|---------------------------|----------------------|
| Set bay LED| `S <B\|R> <0-3> <0\|1>`  | `S B 2 1` (bay 2 blue on) |
| Brightness | `B <0-9>`                 | `B 5`                |
| System LED | `Y <B\|R\|A> <0\|1\|2>`  | `Y B 2` (sys blue blink) |
| USB LED    | `U <0\|1>`                | `U 1`                |
| Ping       | `P`                       | Response: `OK`       |
| Reset      | `R`                       | All LEDs off         |

System LED states: 0=off, 1=on, 2=blink (500ms interval, handled by Arduino).

## Usage with mediasmartserverd

```bash
# Basic usage
./mediasmartserverd --arduino --verbose

# Specify a different serial port
./mediasmartserverd --arduino --serial-port=/dev/ttyACM1 --activity

# Full daemon mode with activity and update monitoring
./mediasmartserverd --arduino --daemon --activity --update-monitor

# Test with light show
./mediasmartserverd --arduino --light-show=1 --verbose
```

## Troubleshooting

- **"failed to open" error**: Check that the serial port exists (`ls /dev/ttyACM*`)
  and that your user is in the `dialout` group (`sudo usermod -aG dialout $USER`).
- **"ping timeout" error**: The Pro Micro resets when the serial port opens.
  The driver waits 2.5s for the bootloader. If this isn't enough, try pressing
  the reset button on the Pro Micro, or disable auto-reset by connecting a
  10µF capacitor between RESET and GND.
- **LEDs are inverted**: Toggle the `ACTIVE_LOW` constant in the sketch.

## Testing with a Serial Monitor

You can test the Arduino independently using any serial terminal:

```bash
screen /dev/ttyACM0 115200
# Type: P<Enter>       → should print "OK"
# Type: S B 0 1<Enter> → bay 0 blue LED should light up
# Type: R<Enter>       → all LEDs off
```
