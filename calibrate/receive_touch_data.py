
import serial
import csv
import time

PORT = "/dev/ttyACM0"
BAUD_RATE = 115200
OUTPUT_FILE = "touch_readings_15102026.csv"

with serial.Serial(PORT, BAUD_RATE, timeout=2) as ser:
    time.sleep(2)
    ser.reset_input_buffer()

    with open(OUTPUT_FILE, "w", newline="") as file:
        writer = csv.writer(file)
        writer.writerow(["time_ms", "touch_raw", "trigger_state"])

        print("Logging both sensors. Press Ctrl+C to stop.")

        try:
            while True:
                line = ser.readline().decode(
                    "utf-8", errors="replace"
                ).strip()

                parts = line.split(",")

                if len(parts) != 3:
                    continue

                try:
                    timestamp = int(parts[0])
                    touch = int(parts[1])
                    trigger = int(parts[2])
                except ValueError:
                    continue

                writer.writerow([timestamp, touch, trigger])
                file.flush()

                print(timestamp, touch, trigger)

        except KeyboardInterrupt:
            print("\nLogging stopped.")

print(f"Saved data to {OUTPUT_FILE}")
