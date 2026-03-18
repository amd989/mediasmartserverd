# Raspberry Pi Pico LED Controller for mediasmartserverd

Firmware for a Raspberry Pi Pico that drives the HP MediaSmart Server LED
daughter board via USB serial. Uses the arduino-pico core
(earlephilhower/arduino-pico).

The Pico variant is functionally identical to the Pro Micro version. The
daemon doesn't need any changes — use `--arduino` the same way.

## Pin Mapping

| Function     | Pico GP Pin | Notes                 |
|--------------|-------------|-----------------------|
| Bay 0 Blue   | GP2         | PWM                   |
| Bay 1 Blue   | GP3         | PWM                   |
| Bay 2 Blue   | GP4         | PWM                   |
| Bay 3 Blue   | GP5         | PWM                   |
| Bay 0 Red    | GP6         | PWM                   |
| Bay 1 Red    | GP7         | PWM                   |
| Bay 2 Red    | GP8         | PWM                   |
| Bay 3 Red    | GP9         | PWM                   |
| System Blue  | GP10        | PWM                   |
| System Red   | GP11        | PWM                   |
| USB LED      | GP12        |                       |
| Onboard LED  | GP25        | Blinks once on boot   |

All Pico GPIO pins support hardware PWM, so brightness control works on
every LED pin (unlike the Pro Micro where only a few pins have PWM).

Pin assignments can be changed at the top of the `.ino` file.

## Wiring the Daughter Board

Same as the Pro Micro version — the daughter board has inline resistors.
Connect each LED wire to the corresponding Pico GPIO pin.

Check `ACTIVE_LOW` setting based on your daughter board type:
- **Common cathode** (shared ground): `ACTIVE_LOW = false`, common pin → GND
- **Common anode** (shared VCC): `ACTIVE_LOW = true`, common pin → 3V3

**Note:** The Pico runs at 3.3V (not 5V like the Pro Micro). The LEDs will
be slightly dimmer but should still be visible with the existing resistors.
If too dim, you can reduce the resistor values on the daughter board.

## Installing the arduino-pico Core

1. In Arduino IDE, go to **File → Preferences**
2. Add this URL to **Additional Boards Manager URLs**:
   ```
   https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json
   ```
3. Go to **Tools → Board → Boards Manager**, search for "pico", install
   **Raspberry Pi Pico/RP2040** by Earle F. Philhower, III

## Flashing

### Arduino IDE

1. Select board: **Raspberry Pi Pico** (Tools → Board → Raspberry Pi
   Pico/RP2040 → Raspberry Pi Pico)
2. Hold the BOOTSEL button on the Pico, plug it into USB, then release
3. Select the serial port
4. Upload `mediasmartserver_leds_pico.ino`

### arduino-cli

```bash
# Install the core
arduino-cli core install rp2040:rp2040 \
  --additional-urls https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json

# Compile
arduino-cli compile --fqbn rp2040:rp2040:rpipico mediasmartserver_leds_pico

# Upload (hold BOOTSEL on first flash)
arduino-cli upload --fqbn rp2040:rp2040:rpipico --port /dev/ttyACM0 mediasmartserver_leds_pico
```

## Usage with mediasmartserverd

Identical to the Pro Micro — the daemon talks the same serial protocol:

```bash
./mediasmartserverd --arduino --activity --verbose
./mediasmartserverd --arduino --serial-port=/dev/ttyACM0 --activity
```

## Differences from Pro Micro Version

| Feature | Pro Micro | Pico |
|---------|-----------|------|
| Voltage | 5V | 3.3V |
| PWM pins | 6 of 18 | All 26 |
| Brightness | System LED only | All LEDs |
| USB reset on connect | Yes (DTR) | No |
| Boot flash method | Auto (via serial) | Hold BOOTSEL |
| Price | ~$4-8 | ~$4 |

## Testing with Serial Monitor

```bash
screen /dev/ttyACM0 115200
# Type: P<Enter>       → should print "OK"
# Type: S B 0 1<Enter> → bay 0 blue LED on
# Type: B 3<Enter>     → dim all LEDs
# Type: R<Enter>       → all LEDs off
```
