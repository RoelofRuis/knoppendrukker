#!/usr/bin/env python3
"""
Live viewer and logger for calibrate_mcp3008.ino.

Every sample from the Pico is written to a CSV file and every event (touch, tap,
hold, release) to a second CSV file, while a rolling plot shows the voltage per
sensor, the events as markers and the current reading per sensor.

Usage:
  python3 calibrate/live_view.py
  python3 calibrate/live_view.py --port /dev/ttyACM0 --window 5 --r-fixed 4700 22000
"""

import argparse
import csv
import glob
import os
import threading
import time
from collections import deque
from datetime import datetime

import matplotlib

# Force a real GUI window: IDEs like PyCharm set MPLBACKEND to their own plot
# panel, which renders a single static image and makes plt.show() return at once.
matplotlib.use("TkAgg")

import matplotlib.pyplot as plt
import serial
from matplotlib.animation import FuncAnimation

VREF = 3.3
ADC_STEPS = 1024
SAMPLE_INTERVAL_US = 1000  # must match the sketch
THRESHOLD_V = 0.10  # must match the sketch, only used to draw the threshold line

# marker, filled, size
EVENT_STYLES = {
    "touch": ("o", True, 8),
    "tap": ("^", True, 8),
    "hold": (".", True, 3),
    "release": ("v", False, 8),
}
# hold arrives every 10 ms per pressed sensor; keep it out of the terminal and event list
QUIET_EVENTS = {"hold"}


def find_port():
    ports = sorted(glob.glob("/dev/ttyACM*") + glob.glob("/dev/cu.usbmodem*"))
    return ports[0] if ports else "/dev/ttyACM0"


def to_volts(raw):
    return raw * VREF / ADC_STEPS


def sensor_resistance(volts, r_fixed):
    """Sensor between 3V3 and the ADC input, fixed resistor from the input to GND."""
    if volts <= 0:
        return float("inf")
    return r_fixed * (VREF - volts) / volts


class SerialReader(threading.Thread):
    def __init__(self, port, out_file, events_file, window_samples):
        super().__init__(daemon=True)
        self.port = port
        self.out_file = out_file
        self.events_file = events_file
        self.window_samples = window_samples
        self.lock = threading.Lock()
        self.stopped = threading.Event()
        self.error = None

        self.num_channels = None
        self.times = deque(maxlen=window_samples)
        self.channels = []
        self.count = 0
        self.missed = 0
        self.events = deque(maxlen=window_samples + 1000)  # (time_us, channel, type, value)
        self.event_count = 0

    def run(self):
        try:
            self._read_loop()
        except Exception as e:
            self.error = e

    def _read_loop(self):
        with serial.Serial(self.port, timeout=1) as ser, \
                open(self.out_file, "w", newline="") as f, \
                open(self.events_file, "w", newline="") as ef:
            writer = csv.writer(f)
            event_writer = csv.writer(ef)
            event_writer.writerow(["time_us", "channel", "event", "value"])
            time.sleep(0.1)  # let the Pico notice the connection first
            ser.write(b"samples on\n")  # the Pico sends only events unless asked
            ser.reset_input_buffer()
            ser.readline()  # drop a possibly partial line

            wraps = 0
            last_raw_t = None
            last_t = None

            while not self.stopped.is_set():
                line = ser.readline().decode("ascii", errors="replace").strip()

                if line.startswith("E,"):
                    parts = line.split(",")
                    if len(parts) != 5:
                        continue
                    try:
                        t = int(parts[1]) + wraps * 2**32
                        channel = int(parts[2])
                        value = int(parts[4])
                    except ValueError:
                        continue
                    if last_t is not None and t - last_t > 2**31:
                        t -= 2**32  # stamped just before micros() wrapped
                    event = (t, channel, parts[3], value)
                    event_writer.writerow(event)
                    if parts[3] not in QUIET_EVENTS:
                        print(f"{parts[3]:7s} ch{channel}  {to_volts(value):.3f} V")
                    with self.lock:
                        self.events.append(event)
                        self.event_count += 1
                    continue

                try:
                    values = [int(p) for p in line.split(",")]
                except ValueError:
                    continue  # header or empty line
                if len(values) < 2:
                    continue

                n = len(values) - 1
                if self.num_channels is None:
                    self.channels = [deque(maxlen=self.window_samples) for _ in range(n)]
                    self.num_channels = n
                    writer.writerow(["time_us"] + [f"ch{i}" for i in range(n)])
                elif n != self.num_channels:
                    continue

                # micros() on the Pico wraps every ~71 minutes
                raw_t = values[0]
                if last_raw_t is not None and raw_t < last_raw_t:
                    wraps += 1
                last_raw_t = raw_t
                t = raw_t + wraps * 2**32
                values[0] = t

                if last_t is not None and t - last_t > 1.5 * SAMPLE_INTERVAL_US:
                    self.missed += round((t - last_t) / SAMPLE_INTERVAL_US) - 1
                last_t = t

                writer.writerow(values)

                with self.lock:
                    self.times.append(t)
                    for channel, value in zip(self.channels, values[1:]):
                        channel.append(value)
                    self.count += 1


def parse_args():
    default_out = f"readings_{datetime.now():%Y%m%d_%H%M%S}.csv"
    parser = argparse.ArgumentParser(description="Live view and log MCP3008 pressure sensors.")
    parser.add_argument("-p", "--port", default=None, help="Serial port (default: auto-detect)")
    parser.add_argument("-o", "--out", default=default_out, help="CSV output file")
    parser.add_argument("-w", "--window", type=float, default=5.0, help="Seconds shown in the plot")
    parser.add_argument("-r", "--r-fixed", type=float, nargs="+", default=[10000],
                        help="Fixed divider resistor in ohms per channel, used for R_sensor. "
                             "One value applies to all channels (default: 10000)")
    return parser.parse_args()


def main():
    args = parse_args()
    port = args.port or find_port()
    window_samples = int(args.window * 1e6 / SAMPLE_INTERVAL_US)
    events_out = os.path.splitext(args.out)[0] + "_events.csv"

    reader = SerialReader(port, args.out, events_out, window_samples)
    reader.start()
    print(f"Reading from {port}, logging to {args.out} and {events_out}")

    waiting_since = time.monotonic()
    warned = False
    while reader.num_channels is None:
        if not reader.is_alive():
            raise SystemExit(f"Serial reader stopped: {reader.error}")
        if not warned and time.monotonic() - waiting_since > 3:
            print(f"No samples from {port} yet. Is the current sketch running "
                  "(older ones ignore 'samples on')? Try unplugging and replugging the Pico.")
            warned = True
        time.sleep(0.1)
    n = reader.num_channels
    print(f"Receiving {n} channel(s). Close the plot window to stop.")

    if len(args.r_fixed) == 1:
        r_fixed = args.r_fixed * n
    elif len(args.r_fixed) == n:
        r_fixed = args.r_fixed
    else:
        reader.stopped.set()
        raise SystemExit(f"--r-fixed got {len(args.r_fixed)} values, expected 1 or {n}")

    fig, ax = plt.subplots(figsize=(12, 6))
    lines = [ax.plot([], [], linewidth=0.8, label=f"ch{i}")[0] for i in range(n)]
    ax.set_xlim(-args.window, 0)
    ax.set_ylim(-0.05, VREF + 0.05)
    ax.set_xlabel("Time (s, 0 = now)")
    ax.set_ylabel("Voltage (V)")
    ax.grid(True, alpha=0.3)
    ax.axhline(THRESHOLD_V, color="gray", linestyle="--", linewidth=0.8)

    event_markers = {}
    for i, line in enumerate(lines):
        for kind, (marker, filled, size) in EVENT_STYLES.items():
            color = line.get_color()
            event_markers[(i, kind)] = ax.plot(
                [], [], linestyle="none", marker=marker, markersize=size,
                color=color, markerfacecolor=color if filled else "none")[0]
    event_legend = [plt.Line2D([], [], linestyle="none", marker=marker, markersize=size,
                               color="black", markerfacecolor="black" if filled else "none",
                               label=kind)
                    for kind, (marker, filled, size) in EVENT_STYLES.items()]
    ax.legend(handles=lines + event_legend, loc="upper right")

    readout = ax.text(0.01, 0.98, "", transform=ax.transAxes, va="top",
                      family="monospace", fontsize=9,
                      bbox=dict(facecolor="white", alpha=0.8))
    fig.subplots_adjust(left=0.07, right=0.72)
    fig.text(0.79, 0.88, "Latest events", fontsize=10, weight="bold")
    event_log = fig.text(0.79, 0.86, "", va="top", family="monospace", fontsize=9)

    def update(_frame):
        with reader.lock:
            times = list(reader.times)
            channels = [list(c) for c in reader.channels]
            count, missed, event_count = reader.count, reader.missed, reader.event_count
            if not times:
                return
            now = times[-1]
            cutoff = now - args.window * 1e6
            events = []  # newest first, only those inside the plot window
            for e in reversed(reader.events):
                if e[0] < cutoff:
                    break
                events.append(e)

        xs = [(t - now) / 1e6 for t in times]
        for line, raw in zip(lines, channels):
            line.set_data(xs, [to_volts(v) for v in raw])

        shown = {key: ([], []) for key in event_markers}
        for t, i, kind, value in events:
            if (i, kind) in shown:
                shown[(i, kind)][0].append((t - now) / 1e6)
                shown[(i, kind)][1].append(to_volts(value))
        for key, markers in event_markers.items():
            markers.set_data(*shown[key])

        listed = [e for e in events if e[2] not in QUIET_EVENTS][:20]
        event_log.set_text("\n".join(
            f"{(e[0] - now) / 1e6:7.2f}s  ch{e[1]}  {e[2]:7s} {to_volts(e[3]):.2f} V"
            for e in listed))

        rows = ["      raw   volt    R_sensor"]
        for i, raw in enumerate(channels):
            volts = to_volts(raw[-1])
            r = sensor_resistance(volts, r_fixed[i])
            r_text = "open" if r > 10e6 else f"{r / 1000:8.1f} kΩ"
            rows.append(f"ch{i}  {raw[-1]:4d}  {volts:5.3f}  {r_text}")
        readout.set_text("\n".join(rows))

        ax.set_title(f"{count} samples  |  {event_count} events  |  missed: {missed}"
                     f"  |  logging to {os.path.basename(args.out)}")

    anim = FuncAnimation(fig, update, interval=50, cache_frame_data=False)
    try:
        plt.show()
    finally:
        reader.stopped.set()
        reader.join(timeout=2)
        print(f"Saved {reader.count} samples to {args.out}")
        print(f"Saved {reader.event_count} events to {events_out}")


if __name__ == "__main__":
    main()
