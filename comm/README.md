# Raspberry Pi Pico <-> Raspberry Pi Communication Examples

This directory provides two complete communication architectures between a **Raspberry Pi Pico (RP2040)** and a **Raspberry Pi** (3 / 4 / 5 / Zero 2W):

1. **Option A: USB Serial Communication** (`pico_usb_serial.ino` + `read_pico_usb.py`)
   - Communicates exclusively over a single USB cable (USB CDC / Virtual Serial Port).
   - Single cable provides both power and high-speed data. No jumper wires required.
2. **Option B: I2C Slave with IRQ Pin** (`pico_i2c_slave.ino` + `read_pico_i2c.py`)
   - Communicates over hardware I2C bus with a dedicated hardware interrupt (IRQ) pin.
   - Ideal for embedded bus topologies or when USB ports are unavailable.

---

## Option A: USB Serial Communication (Recommended for Simplicity)

### 1. Hardware Setup & Connection
Connect a standard **Micro-USB to USB-A cable** between the Raspberry Pi Pico and any USB port on the Raspberry Pi:

```
Raspberry Pi Pico (Micro-USB) <================ USB Cable ================> Raspberry Pi (USB Port)
```

- **Power**: The Raspberry Pi powers the Pico via the USB 5V VBUS rail.
- **Data**: Full-speed USB CDC serial connection appears as `/dev/ttyACM0` (or `/dev/ttyACM*`).
- **Wiring**: No extra jumper wires or pull-up resistors needed.

### 2. Flashing the Pico (`pico_usb_serial.ino`)
1. Open Arduino IDE and install the **Raspberry Pi Pico/RP2040** board core (by Earle F. Philhower, III).
2. Open `comm/pico_usb_serial.ino`.
3. Select board: `Raspberry Pi Pico`.
4. Hold the `BOOTSEL` button on the Pico while plugging into USB, then click **Upload**.

### 3. Running the Host Python Script (`read_pico_usb.py`)
On the Raspberry Pi:
```bash
python3 comm/read_pico_usb.py
```
*(Optional flags: `--port /dev/ttyACM0`, `--baud 115200`)*

**Expected Output:**
```
=================================================================
 Raspberry Pi Pico USB Serial Message Reader
 Target Port: /dev/ttyACM0
 Baud Rate:   115200
 Timeout:     2.0s
=================================================================
Connecting to Pico over USB... (Press Ctrl+C to stop)

[CONNECTED] Successfully opened serial port /dev/ttyACM0
Listening for incoming messages...

[2026-10-17 19:20:01] [USB RX] Received: "MSG:1,VAL:5832"
[2026-10-17 19:20:02] [USB RX] Received: "MSG:2,VAL:2104"
[2026-10-17 19:20:03] [USB RX] Received: "MSG:3,VAL:8745"
```

---

## Option B: I2C Communication with IRQ Notification

### 1. Hardware Wiring

Both the Raspberry Pi Pico and Raspberry Pi operate at **3.3V logic levels**. Signals are wired directly with **no level shifters needed**.

#### Wiring Diagram
```
 Raspberry Pi Pico (RP2040)                    Raspberry Pi (3 / 4 / 5 / Zero 2W)
 +-------------------------+                  +----------------------------------+
 |                         |                  |                                  |
 | GP4  (Pin 6)   [SDA] ---+----------------->+ Pin 3  (GPIO 2 / I2C1 SDA)       |
 | GP5  (Pin 7)   [SCL] ---+----------------->+ Pin 5  (GPIO 3 / I2C1 SCL)       |
 | GP14 (Pin 19)  [IRQ] ---+----------------->+ Pin 15 (GPIO 22 / Interrupt Pin) |
 | GND  (Pin 8/13/18/etc) -+----------------->+ Pin 6  (GND / Ground)            |
 |                         |                  |                                  |
 +-------------------------+                  +----------------------------------+
```

#### Wiring Table

| Signal | Pico RP2040 Pin | Pico Physical Pin | Raspberry Pi GPIO | Pi Physical Pin | Notes |
|---|---|---|---|---|---|
| **I2C SDA** | `GP4` | Pin 6 | `GPIO 2 (SDA1)` | Pin 3 | Shared I2C data line |
| **I2C SCL** | `GP5` | Pin 7 | `GPIO 3 (SCL1)` | Pin 5 | Shared I2C clock line |
| **IRQ** | `GP14` | Pin 19 | `GPIO 22` | Pin 15 | Active-LOW interrupt signal |
| **GND** | `GND` | Pin 8 (or 3, 13, 18, 23, 28, 33, 38) | `GND` | Pin 6 (or 9, 14, 20, 25, 30, 34, 39) | **Mandatory** common ground |

*(To power the Pico from the Pi's GPIO header without USB, connect Pi 5V [Pin 2 or 4] to Pico `VSYS` [Pin 39]).*

### 2. Raspberry Pi I2C Configuration
1. Enable I2C:
   ```bash
   sudo raspi-config
   ```
   Navigate to: `Interface Options` -> `I2C` -> `Enable` -> `Finish`.
2. Verify I2C detection on bus 1 (Slave address `0x17`):
   ```bash
   sudo apt-get install -y i2c-tools
   i2cdetect -y 1
   ```

### 3. Flashing the Pico (`pico_i2c_slave.ino`)
1. Open `comm/pico_i2c_slave.ino` in Arduino IDE.
2. Select board: `Raspberry Pi Pico`.
3. Upload to Pico via USB.

### 4. Running the Host Python Script (`read_pico_i2c.py`)
On the Raspberry Pi:
```bash
python3 comm/read_pico_i2c.py
```

**Expected Output:**
```
============================================================
 Raspberry Pi Pico I2C + IRQ Reader
 I2C Target Address: 0x17
 IRQ GPIO Pin:       GPIO 22 (Physical Pin 15)
============================================================
[I2C] Using smbus2 on /dev/i2c-1
[GPIO] Listening for IRQ (Active-LOW) on GPIO 22 using gpiozero...
Waiting for messages from Raspberry Pi Pico. Press Ctrl+C to exit.

[2026-10-17 19:15:01] [IRQ TRIGGERED] Received: "MSG:1,VAL:4821"
[2026-10-17 19:15:02] [IRQ TRIGGERED] Received: "MSG:2,VAL:7912"
```

---

## Summary Comparison

| Feature | Option A: USB Serial | Option B: I2C + IRQ |
|---|---|---|
| **Cabling** | 1x USB Cable | 4–5x Jumper wires |
| **Power Source** | USB VBUS (5V -> internal 3.3V SMPS) | USB or Pin 2/4 (5V -> `VSYS`) |
| **Logic Voltage** | Handled internally via USB | 3.3V direct |
| **Notification Method** | Streaming serial / line buffered | Active-LOW hardware IRQ pin |
| **Best For** | Fast prototyping, debugging, simple setup | Multi-drop bus, low-level GPIO pin integration |
