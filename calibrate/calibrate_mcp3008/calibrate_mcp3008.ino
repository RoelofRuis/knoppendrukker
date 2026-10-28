
/*
  Raspberry Pi Pico + MCP3008 - Multi-sensor pressure logger

  MCP3008 on SPI0: GP16 (MISO), GP17 (CS), GP18 (SCK), GP19 (MOSI), powered from 3V3
  Output over USB serial, two kinds of lines:
    samples: time_us,ch0,...,chN-1
    events:  E,time_us,channel,type,value   (type: touch, tap, hold, release)

  See calibrate/README.md for the wiring and the event rules.
*/

#include <SPI.h>

const int NUM_CHANNELS = 2;  // set to 8 once all sensors are connected

const unsigned long SAMPLE_INTERVAL_US = 1000;  // 1 kHz, all channels per sample

// Event detection
const float VREF = 3.3;
const float THRESHOLD_V = 0.10;               // a press starts above this
const float RELEASE_V = 0.08;                 // a press ends below this (hysteresis)
const float TAP_RISE_V_PER_MS = 0.08;         // tap: rises at least this fast in one sample
const unsigned long ONSET_WINDOW_US = 2000;   // no fast rise within this time: touch
const unsigned long TAP_PEAK_MAX_US = 10000;  // send the tap by now even if still rising
const unsigned long HOLD_INTERVAL_US = 10000; // hold event interval while pressed

const int THRESHOLD_RAW = THRESHOLD_V / VREF * 1024;
const int RELEASE_RAW = RELEASE_V / VREF * 1024;
const int TAP_RISE_RAW = TAP_RISE_V_PER_MS / VREF * 1024 * SAMPLE_INTERVAL_US / 1000.0;

const int MISO_PIN = 16;
const int CS_PIN = 17;
const int SCK_PIN = 18;
const int MOSI_PIN = 19;

// MCP3008 is rated up to 1.35 MHz at 3.3V; 1 MHz leaves margin for the sensor impedance
SPISettings mcpSettings(1000000, MSBFIRST, SPI_MODE0);

unsigned long nextSampleTime;
bool hostConnected = false;

enum class PressState { Idle, Onset, Rising, Pressed };  // scoped: Arduino already defines RISING

struct Press {
  PressState state = PressState::Idle;
  unsigned long startTime;
  unsigned long lastHoldTime;
  int previous = 0;
  int peak;
};

Press presses[8];

int readMcp3008(int channel) {
  digitalWrite(CS_PIN, LOW);
  SPI.transfer(0x01);                                  // start bit
  uint8_t high = SPI.transfer(0x80 | (channel << 4));  // single-ended + channel
  uint8_t low = SPI.transfer(0x00);
  digitalWrite(CS_PIN, HIGH);
  return ((high & 0x03) << 8) | low;
}

void printHeader() {
  Serial.print("time_us");
  for (int ch = 0; ch < NUM_CHANNELS; ch++) {
    Serial.print(",ch");
    Serial.print(ch);
  }
  Serial.println();
  Serial.println("# events: E,time_us,channel,type,value");
}

void sendEvent(unsigned long time, int channel, const char *type, int value) {
  if (!hostConnected) {
    return;
  }
  char line[48];
  int len = snprintf(line, sizeof(line), "E,%lu,%d,%s,%d\n", time, channel, type, value);
  Serial.write(line, len);
}

// touch/tap are timestamped at the start of the press, hold and release at the
// moment they are sent. A press that rises fast (the start of a spike) within
// ONSET_WINDOW_US is a tap, otherwise a touch. When in doubt it's a tap.
// A tap is sent once its spike stops rising, with the top of the spike as value.
void detectEvents(int channel, int value, unsigned long now) {
  Press &p = presses[channel];
  int rise = value - p.previous;
  p.previous = value;

  if (p.state == PressState::Idle) {
    if (value <= THRESHOLD_RAW) {
      return;
    }
    p.state = PressState::Onset;
    p.startTime = now;
    p.peak = value;
  }

  if (value < RELEASE_RAW) {
    if (p.state == PressState::Onset || p.state == PressState::Rising) {
      sendEvent(p.startTime, channel, "tap", p.peak);  // released before it could be classified
    }
    sendEvent(now, channel, "release", value);
    p.state = PressState::Idle;
    return;
  }

  if (p.state == PressState::Onset) {
    p.peak = max(p.peak, value);
    if (rise >= TAP_RISE_RAW) {
      p.state = PressState::Rising;
    } else if (now - p.startTime >= ONSET_WINDOW_US) {
      sendEvent(p.startTime, channel, "touch", value);
      p.state = PressState::Pressed;
      p.lastHoldTime = now;
    }
  } else if (p.state == PressState::Rising) {
    if (value > p.peak && now - p.startTime < TAP_PEAK_MAX_US) {
      p.peak = value;
    } else {
      sendEvent(p.startTime, channel, "tap", p.peak);
      p.state = PressState::Pressed;
      p.lastHoldTime = now;
    }
  } else if (now - p.lastHoldTime >= HOLD_INTERVAL_US) {
    sendEvent(now, channel, "hold", value);
    p.lastHoldTime += HOLD_INTERVAL_US;
  }
}

void setup() {
  pinMode(CS_PIN, OUTPUT);
  digitalWrite(CS_PIN, HIGH);

  SPI.setRX(MISO_PIN);
  SPI.setSCK(SCK_PIN);
  SPI.setTX(MOSI_PIN);
  SPI.begin();

  Serial.begin(115200);  // baud rate is ignored for USB serial
  nextSampleTime = micros();
}

void loop() {
  // Re-send the CSV header whenever a host opens the serial port
  bool connected = Serial;
  if (connected && !hostConnected) {
    printHeader();
  }
  hostConnected = connected;

  if ((long)(micros() - nextSampleTime) < 0) {
    return;
  }
  nextSampleTime += SAMPLE_INTERVAL_US;

  unsigned long now = micros();
  if ((long)(now - nextSampleTime) > (long)(10 * SAMPLE_INTERVAL_US)) {
    nextSampleTime = now;  // fell far behind, resync instead of bursting
  }

  int values[8];
  SPI.beginTransaction(mcpSettings);
  for (int ch = 0; ch < NUM_CHANNELS; ch++) {
    values[ch] = readMcp3008(ch);
  }
  SPI.endTransaction();

  if (hostConnected) {
    char line[96];
    int len = snprintf(line, sizeof(line), "%lu", now);
    for (int ch = 0; ch < NUM_CHANNELS; ch++) {
      len += snprintf(line + len, sizeof(line) - len, ",%d", values[ch]);
    }
    line[len++] = '\n';
    Serial.write(line, len);
  }

  for (int ch = 0; ch < NUM_CHANNELS; ch++) {
    detectEvents(ch, values[ch], now);
  }
}
