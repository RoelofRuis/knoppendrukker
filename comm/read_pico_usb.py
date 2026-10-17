#!/usr/bin/env python3
"""
=============================================================================
Raspberry Pi Host: Read USB Serial Messages from Raspberry Pi Pico
=============================================================================

DESCRIPTION:
This Python script runs on the host Raspberry Pi (or any Linux/macOS/Windows PC).
It connects to a Raspberry Pi Pico plugged in via USB and reads the stream of
test messages transmitted every second.

HARDWARE SETUP:
  Raspberry Pi Pico (Micro-USB) <====== USB Cable ======> Raspberry Pi (USB Port)

REQUIREMENTS:
  - Python 3
  - pyserial (`pip install pyserial` or system package `python3-serial`)

DEVICE PERMISSIONS ON LINUX:
  Ensure your user belongs to the `dialout` group to access serial ports:
    sudo usermod -aG dialout $USER
  (Re-login or reboot for changes to take effect).

USAGE:
  python3 comm/read_pico_usb.py
  python3 comm/read_pico_usb.py --port /dev/ttyACM0 --baud 115200
=============================================================================
"""

import argparse
import glob
import os
import sys
import time

def find_default_serial_port() -> str:
    """
    Attempts to auto-detect the serial port assigned to the Raspberry Pi Pico.
    On Linux / Raspberry Pi OS, USB CDC devices typically appear as /dev/ttyACM0.
    """
    candidates = []

    # Typical Linux USB CDC ports (Raspberry Pi Pico / RP2040)
    candidates.extend(glob.glob("/dev/ttyACM*"))
    candidates.extend(glob.glob("/dev/ttyUSB*"))

    # macOS ports
    candidates.extend(glob.glob("/dev/cu.usbmodem*"))
    candidates.extend(glob.glob("/dev/tty.usbmodem*"))

    if candidates:
        return sorted(candidates)[0]

    # Default fallback
    return "/dev/ttyACM0"


def parse_arguments():
    parser = argparse.ArgumentParser(
        description="Read periodic USB Serial messages from Raspberry Pi Pico."
    )
    parser.add_argument(
        "-p", "--port",
        default=None,
        help="Serial port device (e.g. /dev/ttyACM0, COM3). Defaults to auto-detection.",
    )
    parser.add_argument(
        "-b", "--baud",
        type=int,
        default=115200,
        help="Baud rate (default: 115200). Note: USB CDC runs at full USB speed regardless.",
    )
    parser.add_argument(
        "-t", "--timeout",
        type=float,
        default=2.0,
        help="Serial read timeout in seconds (default: 2.0).",
    )
    return parser.parse_args()


def main():
    args = parse_arguments()

    try:
        import serial
    except ImportError:
        print("[ERROR] 'pyserial' is not installed.")
        print("Please install it using: pip install pyserial (or sudo apt install python3-serial)")
        sys.exit(1)

    port = args.port or find_default_serial_port()
    baud = args.baud
    timeout = args.timeout

    print("=" * 65)
    print(" Raspberry Pi Pico USB Serial Message Reader")
    print(f" Target Port: {port}")
    print(f" Baud Rate:   {baud}")
    print(f" Timeout:     {timeout}s")
    print("=" * 65)
    print("Connecting to Pico over USB... (Press Ctrl+C to stop)\n")

    while True:
        try:
            with serial.Serial(port, baudrate=baud, timeout=timeout) as ser:
                print(f"[CONNECTED] Successfully opened serial port {port}")
                time.sleep(1.0)  # Allow port settling time
                ser.reset_input_buffer()
                print("Listening for incoming messages...\n")

                while True:
                    raw_line = ser.readline()
                    if not raw_line:
                        # Timeout occurred without receiving a newline
                        continue

                    # Decode bytes to UTF-8 string
                    message = raw_line.decode("utf-8", errors="replace").strip()
                    if message:
                        timestamp = time.strftime("%Y-%m-%d %H:%M:%S")
                        print(f"[{timestamp}] [USB RX] Received: \"{message}\"")

        except serial.SerialException as e:
            print(f"[DISCONNECTED] Error accessing {port}: {e}")
            print("Retrying connection in 2 seconds... (Check if Pico is plugged in)")
            try:
                time.sleep(2.0)
                # Re-check port in case device node changed
                if not args.port:
                    port = find_default_serial_port()
            except KeyboardInterrupt:
                break
        except KeyboardInterrupt:
            print("\n[STOPPED] User requested exit.")
            break

    print("Done.")


if __name__ == "__main__":
    main()
