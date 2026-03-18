mediasmartserverd
=================

Linux daemon that controls the drive bay status LEDs on HP MediaSmart Server
EX48X and compatible hardware (Acer H340/H341/H342, Acer Altos easyStore M2,
Lenovo IdeaCentre D400). Monitors disk presence and activity via udev/sysfs,
provides update notifications via LED colors, and supports light show
animations.

Also supports an **Arduino-based USB serial LED driver** for custom hardware
builds where the original motherboard chipset is no longer available.

USE AT YOUR OWN RISK.

## Credits

Originally created by **Chris Byrne** and **Brian Teague**.
Arduino USB serial driver and continued development by **Alejandro Mora**.

This is an altered version of the original software. See
[LICENSE](LICENSE) for terms (zlib license).

-----------------------------------------------------------------------------

## Usage

```
mediasmartserverd [-D] [-v] [-V] [--brightness <level>]
```

| Option | Description |
|--------|-------------|
| `-D, --daemon` | Run in the background as a daemon |
| `-a, --activity` | Use bay lights as disk activity indicators |
| `-u, --update-monitor` | Use system LED as update notification light |
| `--brightness <level>` | Set LED brightness (0=off to 10=full) |
| `--light-show=N` | Run LED animation pattern N |
| `-v, --verbose` | Verbose output (use twice for more) |
| `--debug` | Print debug messages |
| `-V, --version` | Print version number |
| `-A, --arduino` | Use Arduino USB serial LED driver |
| `-P, --serial-port=DEV` | Serial port for Arduino (default: /dev/ttyACM0) |
| `-M, --bay-map=H,H,...` | Map SCSI host numbers to LED bays (see below) |

```bash
# run as a daemon with disk activity monitoring
sudo ./mediasmartserverd -D --activity

# run with Arduino LED driver
sudo ./mediasmartserverd --arduino --activity

# Arduino on a specific serial port
sudo ./mediasmartserverd --arduino --serial-port=/dev/ttyACM1 --activity

# test LEDs with light show
sudo ./mediasmartserverd --light-show=1

# set brightness
sudo ./mediasmartserverd --brightness=5
```

-----------------------------------------------------------------------------

## Building

Requires: `g++`, `libudev-dev`, `make`

```bash
make
```

-----------------------------------------------------------------------------

## Bay Mapping

On the original HP hardware, SCSI host numbers map directly to physical bay
positions (host 0 = bottom bay, host 3 = top bay). On different hardware with
a different SATA controller, these numbers may not match the physical bays.

Use `--bay-map` to specify which SCSI host numbers correspond to which LED
bays. The argument is a comma-separated list of SCSI host numbers, in bay
order (bay 0 first, bay 3 last):

```bash
# First, discover your SCSI host numbers:
sudo ./mediasmartserverd --arduino --activity --verbose --debug
# Look for lines like "scsi_host: 2 led: 2"

# Then set the mapping: hosts 2,3,4,5 → bays 0,1,2,3
sudo ./mediasmartserverd --arduino --activity --bay-map=2,3,4,5
```

The mapping is stable across reboots as long as the SATA controller and port
assignments don't change. Set it permanently in the systemd service file:

```ini
ExecStart=/usr/sbin/mediasmartserverd --arduino --activity --bay-map=2,3,4,5
```

When `--bay-map` is not specified, the original behavior is preserved (SCSI
host index is used directly as the LED bay index).

-----------------------------------------------------------------------------

## Arduino LED Driver

For hardware where the original motherboard (SCH5127 + ICH9 chipset) is no
longer available, the LED daughter board can be wired to a microcontroller
and controlled over USB serial. Two firmware variants are provided:

The daughter board has 4 dual-color (blue/red) LEDs with inline resistors —
each LED wire connects directly to a microcontroller GPIO pin.

### Supported Boards

| Board | Firmware | Voltage | PWM Brightness | Notes |
|-------|----------|---------|----------------|-------|
| Arduino Pro Micro | `arduino/mediasmartserver_leds/` | 5V | System LED only | ATmega32U4, native USB |
| Raspberry Pi Pico | `arduino/mediasmartserver_leds_pico/` | 3.3V | All LEDs | RP2040, all GPIOs have PWM |

Both use the same serial protocol — the daemon doesn't need any changes
between boards. See each board's `README.md` for pin mapping, wiring, and
flash instructions.

### Setup

1. Flash the firmware for your board (see links above)
2. Wire the LED daughter board to the GPIO pins
3. Connect the board to your server via USB
4. Run the daemon with `--arduino`:

```bash
./mediasmartserverd --arduino --activity --verbose
```

The driver communicates over USB serial at 115200 baud using a simple
ASCII protocol. Both boards show up as `/dev/ttyACM*` on Linux.

-----------------------------------------------------------------------------

## Original Hardware

On the EX485, LED brightness, fan control, and voltage sensors are connected
to an SMSC SCH5127-NW chip (Super I/O with Temperature Sensing, Auto Fan
Control and Glue Logic). The actual LEDs are controlled via General Purpose
I/O on the Intel ICH9 (I/O Controller Hub 9).

LEDs are numbered from BOTTOM (1) to TOP (4) to match the SCSI bay
assignments.

### Supported Hardware

| Hardware | LED Driver | Chipset |
|----------|-----------|---------|
| HP MediaSmart Server EX48X | LedHpEx48X | SCH5127 + ICH9 |
| Acer Aspire easyStore H340 | LedAcerH340 | SCH5127 + ICH7 |
| Acer Aspire easyStore H341/H342 | LedAcerH341 | SCH5127 + ICH9 |
| Acer Altos easyStore M2 | LedAcerAltosM2 | SCH5127 + ICH7 |
| Lenovo IdeaCentre D400 | LedAcerH340 | SCH5127 + ICH7 |
| Arduino Pro Micro (custom) | LedArduino | USB serial |
| Raspberry Pi Pico (custom) | LedArduino | USB serial |

The chipset-based drivers are auto-detected via DMI vendor/product strings.
The USB serial driver is selected explicitly with `--arduino`.
