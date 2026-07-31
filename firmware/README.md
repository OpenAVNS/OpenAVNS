# OpenAVNS Bring-Up Shell

Interactive shell firmware for board bring-up and testing of the OpenAVNS nRF52840 PCBA. Runs on Zephyr RTOS (via the nRF Connect SDK) and provides a command-line interface over **J-Link RTT** or **USB-C serial** for toggling GPIOs, scanning I2C buses, driving the stim outputs, and testing the BLE antenna — no code changes needed to test the board.

## Prerequisites

- [nRF Connect SDK (NCS) toolchain](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/nrf/installation.html) — provides `west`, the Zephyr SDK (`arm-zephyr-eabi-gcc`), and CMake/Ninja
- J-Link Software Pack installed (`JLinkExe`, `JLinkRTTClient`) — needed for SWD flashing and the RTT shell backend
- A J-Link probe (e.g. J-Link EDU Mini) wired to the board's SWD header (SWDIO, SWDCLK, GND, VTref)

## Getting the SDK

This firmware is a standalone Zephyr application — the nRF Connect SDK itself (several GB) is **not** vendored in this repo. `firmware/west.yml` is a [west manifest](https://developer.nordicsemi.com/nRF_Connect_SDK/doc/latest/zephyr/guides/west/index.html) that lets you bootstrap a matching SDK workspace from scratch:

```bash
west init -m https://github.com/<org>/OpenAVNS --mf firmware/west.yml --mr main workspace
cd workspace
west update
```

This pulls the exact NCS/Zephyr/nrfxlib revisions this firmware was last built and tested against (pinned in `west.yml`). It's a development snapshot rather than a numbered NCS release — see the comments at the top of `west.yml` if you'd rather move to a stable release tag.

After `west update`, the repo itself is checked out at `workspace/openavns/`, so this application lives at `workspace/openavns/firmware`.

**Already have an NCS workspace set up?** You don't need the `west init` step — just copy or symlink this `firmware/` directory in as an application alongside your existing `zephyr`/`nrf` checkout, and build directly (see below).

## Build

```bash
west build -b nrf52840dk/nrf52840 -p always openavns/firmware   # from the workspace root
```

(If you're already inside `firmware/` within a west workspace, `west build -b nrf52840dk/nrf52840 -p always` works too — west walks up to find the workspace root automatically.)

## Flash

Connect your J-Link to the board's SWD header (SWDIO, SWDCLK, GND, VTref), then plug the J-Link into your computer via USB.

```bash
west flash --runner jlink
```

If the flash succeeds, your MCU is alive and soldered correctly.

## Connect to the Shell

Two shell backends run simultaneously and independently — use whichever is convenient.

### Option A: USB-C Serial (simpler)

Plug the board into your computer via USB-C. A serial device will appear:

```bash
ls /dev/tty.usbmodem*
```

Connect with any serial terminal at any baud rate (USB CDC-ACM ignores baud):

```bash
screen /dev/tty.usbmodem1101 115200
```

You should see the shell prompt. Press Enter if it doesn't appear immediately. To exit `screen`: `Ctrl-A` then `K`, then `y`.

### Option B: J-Link RTT

Requires the J-Link connected via SWD. Use two terminals:

**Terminal 1** — start the J-Link GDB server (keeps the RTT connection alive):
```bash
JLinkExe -device NRF52840_XXAA -if SWD -speed 4000 -autoconnect 1
```

**Terminal 2** — connect to the shell:
```bash
JLinkRTTClient
```

Press Enter if the prompt doesn't appear immediately.

> **Note:** JLinkRTTClient does not support paste — all commands must be typed character by character. USB-C serial does not have this limitation.

Both options show:
```
nRF52840 bring-up shell ready
Type 'help' for commands
uart:~$
```

## Shell Commands

### Square-Wave Generator — Frequency Sweep

Use this to characterize how fast your analog circuits can respond. The pin toggles symmetrically (equal high and low times) at the specified half-period.

```
sq start <gpio_dev> <pin> <half_period_us>   # start toggling
sq stop                                       # stop and drive pin LOW
sq status                                     # show current state
```

The `half_period_us` argument is the time spent HIGH (and LOW) in microseconds. The full cycle period is `2 × half_period_us`, so the output frequency is `500000 / half_period_us` Hz.

**Examples:**
```
sq start gpio0 15 1        # P0.15 at ~500 kHz (1 us half-period)
sq start gpio0 15 10       # P0.15 at ~50 kHz  (10 us half-period)
sq start gpio0 15 500      # P0.15 at ~1 kHz   (500 us half-period)
sq start gpio0 15 1000     # P0.15 at ~500 Hz  (1 ms half-period)
sq start gpio0 15 100000   # P0.15 at ~5 Hz    (100 ms half-period)
sq stop
```

To change frequency or pin, just call `sq start` again — it reconfigures on the fly.

**Timing notes:**
- Half-periods **< 1 ms** use a CPU busy-wait loop for ~1 µs accuracy. The shell remains responsive (you can still type `sq stop`) because the shell thread runs at a higher priority.
- Half-periods **≥ 1 ms** yield the CPU between toggles, so the shell stays fully snappy.
- On `sq stop`, the pin is driven LOW immediately.

**Typical workflow (frequency sweep):**
1. Connect an oscilloscope or your analog circuit to the pin.
2. Start at a low frequency and increase until the output degrades: `sq start gpio0 15 1000` → `sq start gpio0 15 100` → `sq start gpio0 15 10` → ...
3. `sq stop` when done.

### Stim — Stimulation Output

Combines DAC voltage control and antiphase GPIO toggling into a single command for mimicking real device operation. Both GPIO pins switch simultaneously in opposite states (one HIGH, one LOW) at the specified frequency.

```
stim start <channel> <voltage_mv> <freq_hz>   # start stimulation
stim stop                                      # stop all channels
```

| Channel | DAC address | Positive pin | Negative pin |
|---------|-------------|--------------|--------------|
| 1       | 0x62        | P0.15        | P0.16        |
| 2       | 0x63        | P0.17        | P0.19        |

Voltage is specified in **millivolts** (0–3300). Frequency in **Hz**.

**Examples:**
```
stim start 1 1000 10      # ch1: 1.0V, 10 Hz
stim start 1 2500 100     # ch1: 2.5V, 100 Hz
stim start 2 1650 50      # ch2: 1.65V (mid-rail), 50 Hz
stim stop                 # stop, drive all pins LOW, zero both DACs
```

**Notes:**
- `stim stop` zeros **both** DACs and drives all four pins LOW regardless of which channel was active.
- Calling `stim start` while already running restarts cleanly on the new channel/voltage/frequency.
- Timing uses `k_busy_wait` at all frequencies for ~1 µs accuracy. The shell stays responsive since the stim thread runs at a lower priority and the shell can preempt between toggles.

### General

```
help                    # List all available commands
device list             # List all enabled peripherals
kernel version          # Show Zephyr version
kernel uptime           # Show time since boot
```

### GPIO — Toggle Pins

The nRF52840 has two GPIO ports:
- `gpio0` — Port 0 (pins P0.00–P0.31)
- `gpio1` — Port 1 (pins P1.00–P1.15)

```
gpio conf <device> <pin> <flags>        # Configure pin
gpio set <device> <pin> <0|1>           # Set pin HIGH or LOW
gpio get <device> <pin>                 # Read pin state
gpio toggle <device> <pin>              # Toggle pin
gpio blink <device> <pin>               # Blink pin
gpio devices                            # List all GPIO devices
gpio info [device]                      # Show GPIO info
```

**Configuration flags** for `gpio conf`:
- `i` or `o` — input or output (required)
- `u` or `d` — pull up or pull down (optional, open if omitted)
- `h` or `l` — active high or active low (optional, defaults to active high)
- `0` or `1` — initial logic level (optional, defaults to 0)

Flags are combined into a single string, e.g. `oh1` = output, active high, initial level 1.

**Examples:**
```
gpio conf gpio0 13 o                    # Configure P0.13 as output
gpio set gpio0 13 1                     # P0.13 HIGH
gpio set gpio0 13 0                     # P0.13 LOW
gpio toggle gpio0 13                    # Toggle P0.13
gpio conf gpio0 15 iud                  # P0.15 as input with pull-up, active low
gpio get gpio0 15                       # Read P0.15
gpio conf gpio1 2 oh1                   # P1.02 as output, active high, initial HIGH
```

### I2C — Scan and Send Bytes

The enabled I2C bus is `i2c@40003000` (I2C0).

**Scan for devices:**
```
i2c scan i2c@40003000
```
This prints a table of all responding 7-bit addresses. Use this to verify your DACs and other I2C peripherals are connected and powered.

**Read bytes from a device:**
```
i2c read_byte i2c@40003000 <addr> <reg>
```

**Write bytes to a device (e.g. set a DAC value):**
```
i2c write_byte i2c@40003000 <addr> <reg> <value>
```

All values are in **hex**. Addresses are **7-bit** (not left-shifted).

**Important:** JLinkRTTClient does not support pasting commands. You must type each command character by character. This limitation does not apply when using the USB-C serial shell.

**Examples:**
```
i2c scan i2c@40003000                       # Confirm devices show up
i2c read_byte i2c@40003000 0x60 0x00        # Read register 0x00 from addr 0x60
i2c write_byte i2c@40003000 0x60 0x40 0xFF  # Write 0xFF to register 0x40
```

### BLE — Antenna Testing

Broadcasts a non-connectable BLE advertisement so you can verify the antenna using the **nRF Connect** app (iOS or Android).

```
ble start      # begin advertising as "nRF52840 Bringup"
ble stop       # stop advertising
```

**How to test:**
1. `ble start` in the RTT shell
2. Open nRF Connect → **Scanner** tab
3. Look for `nRF52840 Bringup` in the device list
4. Check RSSI — at ~1m distance, a healthy antenna reads roughly **−50 to −70 dBm**
5. `ble stop` when done

If the device doesn't appear at all, suspect the antenna connection (open circuit). If RSSI is unusually weak (below −80 dBm at close range), suspect an impedance mismatch or poor solder joint on the antenna feed.

> For deeper RF characterization (gain, radiation pattern, return loss), use Nordic's **Direct Test Mode** sample flashed separately with a spectrum analyzer or VNA.

### MCP4725 DAC — Set Output Voltage

The board has two MCP4725 12-bit DACs on I2C0:
- Address `62` (0x62)
- Address `63` (0x63)

The MCP4725 supports a "fast write" where the first byte contains the command/power-down bits and the upper 4 bits of the 12-bit value, and the second byte contains the lower 8 bits. Using `write_byte`, the `<reg>` argument becomes the first byte and `<value>` becomes the second.

**Output voltage formula:** Vout = VDD × (D / 4096)

**Common values (assuming VDD = 3.3V):**

| Target Voltage | D (decimal) | D (hex) | Byte 1 (reg) | Byte 2 (value) | Command |
|----------------|-------------|---------|---------------|-----------------|---------|
| 0.0V | 0 | 000 | `0` | `0` | `i2c write_byte i2c@40003000 62 0 0` |
| 0.5V | 621 | 26D | `2` | `6D` | `i2c write_byte i2c@40003000 62 2 6D` |
| 1.0V | 1241 | 4D9 | `4` | `D9` | `i2c write_byte i2c@40003000 62 4 D9` |
| 1.65V | 2048 | 800 | `8` | `0` | `i2c write_byte i2c@40003000 62 8 0` |
| 2.5V | 3103 | C1F | `C` | `1F` | `i2c write_byte i2c@40003000 62 C 1F` |
| 3.3V | 4095 | FFF | `F` | `FF` | `i2c write_byte i2c@40003000 62 F FF` |

Replace `62` with `63` for the second DAC.

**How to compute the bytes for any voltage:**
1. D = round(Vtarget / VDD × 4096)
2. Byte 1 (reg) = upper nibble of D (D >> 8)
3. Byte 2 (value) = lower 8 bits of D (D & 0xFF)

### LIS2DH12 IMU — Read Accelerometer

The board has a LIS2DH12 3-axis accelerometer on I2C0 at address `18` (0x18).

**Verify the device is alive:**
```
i2c read_byte i2c@40003000 0x18 0x0F
```
The WHO_AM_I register (0x0F) should return `0x33`.

**Enable the accelerometer:**
```
i2c write_byte i2c@40003000 0x18 0x20 0x57
```
This writes 0x57 to CTRL_REG1 (0x20): 100Hz data rate, normal mode, X/Y/Z axes enabled.

**Read acceleration data:**
```
i2c read_byte i2c@40003000 0x18 0x28       # OUT_X_L
i2c read_byte i2c@40003000 0x18 0x29       # OUT_X_H
i2c read_byte i2c@40003000 0x18 0x2A       # OUT_Y_L
i2c read_byte i2c@40003000 0x18 0x2B       # OUT_Y_H
i2c read_byte i2c@40003000 0x18 0x2C       # OUT_Z_L
i2c read_byte i2c@40003000 0x18 0x2D       # OUT_Z_H
```
Each axis is a signed 16-bit value (little-endian, left-justified). Combine the high and low bytes: `(OUT_H << 8) | OUT_L`, then interpret as a signed int16. At the default ±2g range, 1 mg/digit (in normal mode, 10-bit resolution: 4 mg/LSB).

**Check if new data is ready:**
```
i2c read_byte i2c@40003000 0x18 0x27
```
STATUS_REG (0x27) — bit 3 (ZYXDA) is set when new data is available on all axes.

**Key registers:**

| Register | Address | Description |
|----------|---------|-------------|
| WHO_AM_I | 0x0F | Device ID (reads 0x33) |
| CTRL_REG1 | 0x20 | Data rate, power mode, axis enable |
| CTRL_REG4 | 0x23 | Full-scale selection (±2g/4g/8g/16g) |
| STATUS_REG | 0x27 | Data ready flags |
| OUT_X_L/H | 0x28/0x29 | X-axis acceleration |
| OUT_Y_L/H | 0x2A/0x2B | Y-axis acceleration |
| OUT_Z_L/H | 0x2C/0x2D | Z-axis acceleration |

## Pin Mapping

I2C pins are remapped to the custom board layout via `app.overlay`. GPIO commands work with any pin number directly.

| Function | Port | Pin | Node |
|----------|------|-----|------|
| I2C0 SDA | P0 | 13 | `i2c@40003000` |
| I2C0 SCL | P0 | 14 | `i2c@40003000` |

To change I2C pins in the future, edit `app.overlay`.

## Troubleshooting

| Problem | Fix |
|---------|-----|
| `west flash` fails with "No J-Link found" | Check USB connection, install/update J-Link software |
| `west flash` fails with "Could not connect to target" | Check SWD wiring (SWDIO, SWDCLK, GND, VTref). Verify VTref reads board voltage. If it still fails, try `west -v flash` for verbose output — a misleading "Flashing file: ..." message can appear even when the underlying JLink connection failed. |
| RTTClient shows nothing | Make sure JLinkExe is running in another terminal first. Press Enter a few times. |
| USB-C serial device doesn't appear (`/dev/tty.usbmodem*`) | Check D+/D- continuity from connector to nRF52840. Verify 5.1kΩ pull-downs on CC1 and CC2. Try flipping the cable (USB-C orientation). |
| USB-C serial appears but shell is unresponsive | Press Enter once to wake the shell. The CDC-ACM device enumerates before the shell is fully ready. |
| `i2c scan` shows no devices | Check wiring, pull-up resistors, and power to I2C peripherals. Verify you're using the right pins (P0.13/P0.14 per `app.overlay`). |
| GPIO toggle has no effect | Double-check pin number matches your schematic. Remember Port 1 uses `gpio1`. |
| Commands fail with "wrong parameter count" | JLinkRTTClient does not support paste. Type commands manually character by character. |
