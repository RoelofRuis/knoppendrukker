#!/usr/bin/env python3
"""
=============================================================================
Raspberry Pi Host: Read I2C Data from Raspberry Pi Pico on IRQ Notification
=============================================================================

DESCRIPTION:
This Python script runs on the host Raspberry Pi. It monitors the IRQ pin
connected to the Raspberry Pi Pico. When the Pico pulls the IRQ pin LOW
(signaling that a new random test message is available), this script reads the
data over I2C (bus 1, address 0x17) and displays the received message.

WIRING:
  Raspberry Pi Pico (RP2040)           Raspberry Pi (3 / 4 / 5 / Zero 2W)
  =================================    ==================================
  GP4  (Physical Pin 6)  - I2C0 SDA -> GPIO 2 / SDA1 (Physical Pin 3)
  GP5  (Physical Pin 7)  - I2C0 SCL -> GPIO 3 / SCL1 (Physical Pin 5)
  GP14 (Physical Pin 19) - IRQ Line -> GPIO 22       (Physical Pin 15)
  GND  (Physical Pin 8 or any GND)  -> GND            (Physical Pin 6 or any GND)

REQUIREMENTS:
  - I2C enabled on Raspberry Pi: `sudo raspi-config` -> Interface Options -> I2C
  - Python libraries: `gpiozero`, `smbus2` (or standard Linux /dev/i2c fallback)
=============================================================================
"""

import sys
import time
import os
import struct

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------
I2C_BUS_NUMBER = 1          # Standard I2C bus on Raspberry Pi (/dev/i2c-1)
I2C_SLAVE_ADDR = 0x17       # Must match I2C_SLAVE_ADDR in Arduino sketch (0x17 = 23)
IRQ_GPIO_PIN   = 22         # BCM GPIO 22 (Physical Pin 15 on Raspberry Pi 40-pin header)
MAX_BUFFER_LEN = 32         # Max bytes to read per transaction

# ---------------------------------------------------------------------------
# I2C Interface Class (Supports smbus2 with fallback to raw /dev/i2c)
# ---------------------------------------------------------------------------
class PicoI2CReader:
    def __init__(self, bus_num=I2C_BUS_NUMBER, slave_addr=I2C_SLAVE_ADDR):
        self.bus_num = bus_num
        self.slave_addr = slave_addr
        self.use_smbus = False
        self.smbus_obj = None
        self.i2c_fd = None

        # Try initializing smbus2 first
        try:
            import smbus2
            self.smbus_obj = smbus2.SMBus(self.bus_num)
            self.use_smbus = True
            print(f"[I2C] Using smbus2 on /dev/i2c-{self.bus_num}")
        except (ImportError, Exception):
            # Fallback to direct /dev/i2c-X ioctl
            import fcntl
            I2C_SLAVE = 0x0703
            dev_path = f"/dev/i2c-{self.bus_num}"
            try:
                self.i2c_fd = os.open(dev_path, os.O_RDWR)
                fcntl.ioctl(self.i2c_fd, I2C_SLAVE, self.slave_addr)
                print(f"[I2C] Using direct OS device {dev_path} (address 0x{self.slave_addr:02X})")
            except Exception as e:
                print(f"[ERROR] Could not open {dev_path}: {e}")
                print("Make sure I2C is enabled in raspi-config and you have appropriate permissions.")

    def read_message(self, length=MAX_BUFFER_LEN) -> str:
        """Reads raw bytes from Pico I2C slave and decodes to ASCII string."""
        raw_bytes = b""
        if self.use_smbus and self.smbus_obj is not None:
            import smbus2
            try:
                # Direct I2C read transaction
                msg = smbus2.i2c_msg.read(self.slave_addr, length)
                self.smbus_obj.i2c_rdwr(msg)
                raw_bytes = bytes(list(msg))
            except Exception as e:
                # Fallback to read_i2c_block_data if i2c_rdwr fails
                try:
                    data = self.smbus_obj.read_i2c_block_data(self.slave_addr, 0, length)
                    raw_bytes = bytes(data)
                except Exception as inner_e:
                    print(f"[I2C Read Error] {inner_e}")
                    return ""
        elif self.i2c_fd is not None:
            try:
                raw_bytes = os.read(self.i2c_fd, length)
            except Exception as e:
                print(f"[I2C Read Error] {e}")
                return ""

        # Decode null-terminated ASCII string
        # Strip trailing null bytes (\x00) and whitespace
        clean_text = raw_bytes.split(b"\x00")[0].decode("utf-8", errors="replace").strip()
        return clean_text

    def close(self):
        if self.smbus_obj:
            try:
                self.smbus_obj.close()
            except Exception:
                pass
        if self.i2c_fd:
            try:
                os.close(self.i2c_fd)
            except Exception:
                pass


# ---------------------------------------------------------------------------
# Main IRQ Listener & Event Loop
# ---------------------------------------------------------------------------
def main():
    print("=" * 60)
    print(" Raspberry Pi Pico I2C + IRQ Reader")
    print(f" I2C Target Address: 0x{I2C_SLAVE_ADDR:02X}")
    print(f" IRQ GPIO Pin:       GPIO {IRQ_GPIO_PIN} (Physical Pin 15)")
    print("=" * 60)

    reader = PicoI2CReader(I2C_BUS_NUMBER, I2C_SLAVE_ADDR)

    def on_interrupt():
        timestamp = time.strftime("%Y-%m-%d %H:%M:%S")
        msg = reader.read_message()
        if msg:
            print(f"[{timestamp}] [IRQ TRIGGERED] Received: \"{msg}\"")
        else:
            print(f"[{timestamp}] [IRQ TRIGGERED] Empty or invalid response from Pico.")

    # Set up IRQ Pin listener using gpiozero or RPi.GPIO
    has_gpio = False
    try:
        from gpiozero import Button
        # Active-LOW interrupt: when_pressed triggers when pin goes LOW (connected to Pico GP14)
        irq_button = Button(IRQ_GPIO_PIN, pull_up=True, bounce_time=None)
        irq_button.when_pressed = on_interrupt
        has_gpio = True
        print(f"[GPIO] Listening for IRQ (Active-LOW) on GPIO {IRQ_GPIO_PIN} using gpiozero...")
    except (ImportError, Exception):
        try:
            import RPi.GPIO as GPIO
            GPIO.setmode(GPIO.BCM)
            GPIO.setup(IRQ_GPIO_PIN, GPIO.IN, pull_up_down=GPIO.PUD_UP)

            def rpi_gpio_callback(channel):
                on_interrupt()

            GPIO.add_event_detect(IRQ_GPIO_PIN, GPIO.FALLING, callback=rpi_gpio_callback, bouncetime=10)
            has_gpio = True
            print(f"[GPIO] Listening for IRQ (Active-LOW) on GPIO {IRQ_GPIO_PIN} using RPi.GPIO...")
        except (ImportError, Exception) as e:
            print(f"[WARN] No hardware GPIO library available ({e}).")
            print("Falling back to periodic polling for test mode...")

    print("Waiting for messages from Raspberry Pi Pico. Press Ctrl+C to exit.\n")

    try:
        while True:
            if not has_gpio:
                # If running in environment without GPIO hardware, poll every second
                on_interrupt()
                time.sleep(1.0)
            else:
                time.sleep(0.1)
    except KeyboardInterrupt:
        print("\nExiting listener...")
    finally:
        reader.close()
        print("Done.")


if __name__ == "__main__":
    main()
