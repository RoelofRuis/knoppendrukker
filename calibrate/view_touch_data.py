
import csv
import matplotlib.pyplot as plt

CSV_FILE = "touch_readings_16102026.csv"

times = []
touch = []
trigger = []

# Read CSV using Python's built-in csv module
with open(CSV_FILE, newline="") as f:
    reader = csv.DictReader(f)

    for row in reader:
        try:
            times.append(float(row["time_ms"]))
            touch.append(float(row["touch_raw"]))
            trigger.append(int(row["trigger_state"]))
        except (ValueError, KeyError):
            continue

if not times:
    raise ValueError("No valid readings found.")

# Convert timestamps to elapsed seconds
start_time = times[0]
times = [(t - start_time) / 1000 for t in times]

fig, ax = plt.subplots(figsize=(12, 6))

# Raw touch sensor readings
ax.plot(times, touch, linewidth=0.8, label="Touch sensor (raw)")
ax.set_xlabel("Elapsed time (seconds)")
ax.set_ylabel("Raw touch reading")
ax.grid(True, alpha=0.3)

# Shade periods when trigger is active (state == 0)
active = [state == 1 for state in trigger]
i = 0

while i < len(active):
    if active[i]:
        start = i
        while i + 1 < len(active) and active[i + 1]:
            i += 1

        # Extend boundaries halfway between adjacent samples
        left = times[start]
        right = times[i]

        if start > 0:
            left = (times[start - 1] + times[start]) / 2
        if i < len(times) - 1:
            right = (times[i] + times[i + 1]) / 2

        ax.axvspan(
            left, right,
            color="red", alpha=0.2,
            label="Trigger active" if start == active.index(True)
            else None
        )
    i += 1

# Trigger state on a separate right-hand axis
ax2 = ax.twinx()
ax2.step(
    times, trigger, where="post",
    color="darkorange", linewidth=0.8,
    label="Trigger state"
)
ax2.set_ylabel("Trigger state (0 = active)")
ax2.set_yticks([0, 1])
ax2.set_ylim(-0.15, 1.15)

# Combine legends from both axes
lines1, labels1 = ax.get_legend_handles_labels()
lines2, labels2 = ax2.get_legend_handles_labels()
ax.legend(lines1 + lines2, labels1 + labels2, loc="upper right")

ax.set_title("Touch Sensor and Trigger Activity")
fig.tight_layout()

print(f"Loaded {len(times)} samples.")
print(f"Duration: {times[-1]:.2f} seconds.")
print("Toolbar: zoom, pan, and reset the view.")

plt.show()
