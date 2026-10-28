# Pressure sensor calibration

Read up to 8 pressure sensors at 1 kHz with a Raspberry Pi Pico and an MCP3008
ADC, detect touch / tap / release events on the Pico, log samples and events
to CSV, and watch both live on the computer.

```
 sensors ──> MCP3008 (8ch, 10-bit ADC) ──SPI──> Pico ──USB──> laptop: live_view.py (plot + CSV)
```

| File | What it is |
|---|---|
| `calibrate_mcp3008/calibrate_mcp3008.ino` | Pico sketch: samples all channels every 1 ms, detects events, streams both over USB |
| `live_view.py` | Live plot + CSV logger (samples and events) on the laptop |
| `calibrate_touch/calibrate_touch.ino`, `receive_touch_data.py`, `view_touch_data.py` | Previous single-sensor setup (Pico's own ADC on A0) |

## What you need

- Raspberry Pi Pico + micro-USB cable
- MCP3008 (DIP-16)
- Pressure sensors (assumed resistive, e.g. FSRs: resistance drops when pressed)
- One fixed resistor per sensor (start with 10 kΩ, see [Choosing the resistor](#choosing-the-resistor))
- 100 nF ceramic capacitor (decoupling for the MCP3008)
- Breadboard and jumper wires

## Wiring

### MCP3008 pinout

The notch (or dot) on the chip marks the top; pin 1 is top left.

```
            +---\_/---+
      CH0 --| 1    16 |-- VDD
      CH1 --| 2    15 |-- VREF
      CH2 --| 3    14 |-- AGND
      CH3 --| 4    13 |-- CLK
      CH4 --| 5    12 |-- DOUT
      CH5 --| 6    11 |-- DIN
      CH6 --| 7    10 |-- CS/SHDN
      CH7 --| 8     9 |-- DGND
            +---------+
```

### MCP3008 → Pico

| MCP3008 pin | Name | Pico pin | Pico physical pin |
|---|---|---|---|
| 16 | VDD | 3V3(OUT) | 36 |
| 15 | VREF | 3V3(OUT) | 36 |
| 14 | AGND | GND | 38 |
| 13 | CLK | GP18 (SPI0 SCK) | 24 |
| 12 | DOUT | GP16 (SPI0 RX / MISO) | 21 |
| 11 | DIN | GP19 (SPI0 TX / MOSI) | 25 |
| 10 | CS/SHDN | GP17 (SPI0 CSn) | 22 |
| 9 | DGND | GND | 23 |

Put the **100 nF capacitor between VDD (16) and AGND (14)**, as close to the chip
as possible.

> **Power the MCP3008 from 3V3, not from VBUS/VSYS (5V).** The chip would then
> output 5V on DOUT, and the Pico's GPIO pins are not 5V tolerant.

```
  Pico                                   MCP3008
  ┌─────────────────────┐                ┌─────────────┐
  │ 3V3(OUT)  pin 36 ───┼──┬─────────────┤16 VDD       │
  │                     │  ├─────────────┤15 VREF      │
  │                     │  │   100nF     │             │
  │                     │  └───┤├───┐    │             │
  │ GND       pin 38 ───┼──────────┴─────┤14 AGND      │
  │ GP18 SCK  pin 24 ───┼────────────────┤13 CLK       │
  │ GP16 MISO pin 21 ───┼────────────────┤12 DOUT      │
  │ GP19 MOSI pin 25 ───┼────────────────┤11 DIN       │
  │ GP17 CS   pin 22 ───┼────────────────┤10 CS/SHDN   │
  │ GND       pin 23 ───┼────────────────┤ 9 DGND      │
  │                     │                │             │
  │                     │                │ 1-8 CH0-CH7 ├── sensors
  └─────────────────────┘                └─────────────┘
```

Tip: run a 3V3 rail and a GND rail along the breadboard. The MCP3008 and all
sensors hang off those two rails.

### Sensor → MCP3008 (repeat per sensor)

Each sensor forms a voltage divider with a fixed resistor:

```
  3V3 rail ──── sensor ────┬──── CHx  (MCP3008 pin 1..8)
                           │
                         10 kΩ
                           │
  GND rail ────────────────┘
```

- Sensor 0 → CH0 (pin 1), sensor 1 → CH1 (pin 2), ..., sensor 7 → CH7 (pin 8).
- With no pressure the sensor is (nearly) open, so CHx reads ~0V. Pressing
  lowers the sensor resistance and the voltage rises towards 3.3V.
- Tie unused channels to GND so they don't float (optional; they are not read
  unless `NUM_CHANNELS` includes them).

Because the sensors and VREF share the same 3V3 supply, the readings are
*ratiometric*: drift in the 3V3 supply cancels out.

### Choosing the resistor

Each sensor type has its own resistance range, so give each its own resistor.
Start with 10 kΩ, then:

1. In the live viewer, read the sensor's `R_sensor` at the **lightest** press
   you want to measure (`R_light`) and at **full** force (`R_full`).
2. Pick `R_fixed = sqrt(R_light × R_full)` and round to the nearest standard
   value (1.0, 1.5, 2.2, 3.3, 4.7, 6.8 × 10ⁿ). This gives the largest voltage
   swing between those two forces.
3. Pass the resistors to the viewer in channel order:
   `--r-fixed 4700 22000`.

Trade-off: a larger resistor pushes full force closer to 3.3V but squeezes
light presses together near 0V; a smaller one does the opposite. The full
0-3.3V can't be reached, since the sensor never drops to 0 Ω. If a sensor
jumps high on a light touch and then barely moves, its resistor is too large.

## Software

### 1. Flash the Pico

1. Arduino IDE with the **Raspberry Pi Pico/RP2040** core (Earle F. Philhower, III).
2. Open `calibrate/calibrate_mcp3008/calibrate_mcp3008.ino`.
3. Set `NUM_CHANNELS` to the number of connected sensors (2 now, up to 8 later).
   Sensors must be connected to CH0 upwards without gaps.
4. Board: `Raspberry Pi Pico`; hold BOOTSEL while plugging in the first time, then **Upload**.

The sketch lives in its own folder because the Arduino IDE compiles every
`.ino` in a folder as one sketch; next to `calibrate_touch/calibrate_touch.ino` it would not build.

### 2. Live view and logging

```bash
pip install pyserial matplotlib
python3 calibrate/live_view.py
```

Options:

| Flag | Default | Meaning |
|---|---|---|
| `--port` | auto (`/dev/ttyACM*`) | Serial port of the Pico |
| `--out` | `readings_<date>_<time>.csv` | CSV file that receives every sample; events go to `<name>_events.csv` |
| `--window` | `5` | Seconds of history shown in the plot |
| `--r-fixed` | `10000` | Divider resistor in ohms per channel (one value = all channels), for the R_sensor column |

The window shows one voltage line per sensor, the 0.1V threshold (dashed), the
events as markers in the sensor's colour (● touch, ▲ tap, · hold, ▽ release),
a live readout per channel (raw ADC value, voltage, computed sensor resistance)
and a list of the latest events. Events are also printed in the terminal
(`hold` is left out of the list and the terminal, it's too frequent). The title
shows the number of samples and events received and how many samples were
**missed**; that should stay at 0. Close the window to stop; the CSVs are then
complete.

Only one program can have the serial port open at a time, so close the Arduino
Serial Monitor first. On Linux your user needs to be in the `dialout` group
(`sudo usermod -aG dialout $USER`, then log in again).

### CSV format

```
time_us,ch0,ch1
16486123,0,412
16487123,0,415
```

- `time_us`: Pico timestamp in microseconds (unwrapped, so it keeps increasing).
- `ch0..chN`: raw 10-bit ADC value, 0-1023. Volts = `raw * 3.3 / 1024`.

Events file (`<name>_events.csv`):

```
time_us,channel,event,value
794723473,1,tap,408
794735473,1,hold,233
794745473,1,hold,240
794771472,1,release,20
```

- `time_us`: for `touch`/`tap` the moment the press **started** (crossed the
  threshold), for `hold`/`release` the moment it was sent. Same clock as the
  samples.
- `value`: raw ADC value. For `tap` the top of the spike, for the others the
  level at the moment the event was sent.

## Events

The sample stream is unchanged; on top of it the Pico detects events per
channel and sends them as extra lines (`E,time_us,channel,type,value`).

| Event | Rule | Sent after the press starts |
|---|---|---|
| `tap` | Above 0.1V, and rises ≥ 0.08V within one 1 ms sample, at the threshold crossing or within 2 ms after. Sent once the spike stops rising, with its top as value | ~3 ms (2-7 ms) |
| `touch` | Above 0.1V, but no such fast rise within 2 ms | 2 ms |
| `hold` | Still pressed: every 10 ms after the `tap`/`touch`, with the current level | every 10 ms |
| `release` | Drops below 0.08V | immediately |

Every press gives exactly one `tap` or `touch`, then a `hold` every 10 ms while
it stays pressed, and one `release`. A press released within 10 ms has no
`hold`. The release level is a bit below the start level (hysteresis) so noise
around 0.1V doesn't cause repeated events.

Why rise speed: fast hits start with a spike (peak in 1-3 ms, then fall back),
and that steep rise is already visible at the first sample above the
threshold, while the fall-back shows only after 2-6 ms. Slow presses rise
< 0.05V per ms. Ambiguous presses, in between, count as `tap`. Measured on two
recordings (4k7 on ch0, 1k on ch1): a tap is recognised on average 0.3-1.4 ms
after the press starts, never more than 2 ms.

The `tap` itself waits for the top of the spike, so it carries the right
height: at the moment of recognition the signal is only ~30% of the way up.
The top follows 1-5 ms later; on a recording with 94 taps the tap was sent a
median 3-4 ms after the press started, and the value was the spike top every
time. If a press keeps rising (no clear spike), the tap is sent after 10 ms
with the highest level so far.

Watch out for 50 Hz mains hum (bursts every 20 ms, e.g. from a loose wire or a
floating input): each burst looks like a short tap.

Tuning constants at the top of the sketch:

| Constant | Default | Effect |
|---|---|---|
| `THRESHOLD_V` | `0.10` | Level where a press starts |
| `RELEASE_V` | `0.08` | Level where a press ends |
| `TAP_RISE_V_PER_MS` | `0.08` | Rise speed that makes a press a `tap`; lower = more taps |
| `ONSET_WINDOW_US` | `2000` | How long to look for a fast rise; also the `touch` latency |
| `TAP_PEAK_MAX_US` | `10000` | Longest wait for the spike top before the `tap` is sent anyway |
| `HOLD_INTERVAL_US` | `10000` | Interval between `hold` events |

If you change `THRESHOLD_V`, change it in `live_view.py` too (only used for
the dashed line).

## Timing and precision

- Every 1 ms the Pico reads all channels and timestamps the sample with its own
  microsecond clock. Timestamps are made on the Pico, so USB latency and
  laptop scheduling do not affect them.
- One MCP3008 conversion at 1 MHz SPI takes ~25 µs, so 8 channels take ~0.2 ms.
  Channels within one sample are therefore at most ~0.2 ms apart.
- The live plot lags the data by ~50-100 ms (screen refresh); the CSV does not.
- Resolution is 10 bit: 3.3V / 1024 ≈ 3.2 mV per step.

If neighbouring channels seem to influence each other (a press on CH0 shows up
slightly on CH1), the sensor impedance is too high for the ADC's sampling time.
Lower the SPI clock in the sketch (e.g. 500 kHz) or add a 10 nF capacitor from
each CHx input to GND.

## Calibrating

1. Start `live_view.py` with an `--out` name that describes the session.
2. Place known weights on one sensor at a time (e.g. 0, 50, 100, 200, 500 g),
   holding each for a few seconds.
3. Per weight, average the readings over the steady part of the hold, and fit a
   curve from voltage (or sensor resistance) to force. FSRs are strongly
   non-linear; a fit of `1/R_sensor` against force is usually close to linear.
