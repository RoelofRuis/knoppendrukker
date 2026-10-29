
/*
  Raspberry Pi Pico + MCP3008 - Multi-sensor pressure event detector

  MCP3008 on SPI0: GP16 (MISO), GP17 (CS), GP18 (SCK), GP19 (MOSI), powered from 3V3
  Output over USB serial:
    events:  E,time_us,channel,type,value   (type: touch, tap, hold, release)
    samples: time_us,ch0,...,chN-1          (only after the "samples on" command)

  Commands from the host, one per line:
    samples on    also stream every sample (calibration mode)
    samples off   events only (default for every new connection)

  Event detection lives in detector.h. See calibrate/README.md for the wiring
  and the event rules.
*/

#include <SPI.h>
#include "detector.h"

const int NUM_CHANNELS = 4;  // set to 8 once all sensors are connected

const unsigned long SAMPLE_INTERVAL_US = 1000;  // 1 kHz, all channels per sample

const int MISO_PIN = 16;
const int CS_PIN = 17;
const int SCK_PIN = 18;
const int MOSI_PIN = 19;

// MCP3008 is rated up to 1.35 MHz at 3.3V; 1 MHz leaves margin for the sensor impedance
SPISettings mcpSettings(1000000, MSBFIRST, SPI_MODE0);

unsigned long nextSampleTime;
bool hostConnected = false;
bool streamSamples = false;

Detector detectors[8];

char commandBuffer[32];
unsigned int commandLength = 0;

int readMcp3008(int channel) {
  digitalWrite(CS_PIN, LOW);
  SPI.transfer(0x01);                                  // start bit
  uint8_t high = SPI.transfer(0x80 | (channel << 4));  // single-ended + channel
  uint8_t low = SPI.transfer(0x00);
  digitalWrite(CS_PIN, HIGH);
  return ((high & 0x03) << 8) | low;
}

void printSampleHeader() {
  Serial.print("time_us");
  for (int ch = 0; ch < NUM_CHANNELS; ch++) {
    Serial.print(",ch");
    Serial.print(ch);
  }
  Serial.println();
}

void handleCommand(const char *command) {
  if (strcmp(command, "samples on") == 0) {
    streamSamples = true;
    printSampleHeader();
  } else if (strcmp(command, "samples off") == 0) {
    streamSamples = false;
  } else {
    Serial.print("# unknown command: ");
    Serial.println(command);
  }
}

void readCommands() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      commandBuffer[commandLength] = '\0';
      if (commandLength > 0) {
        handleCommand(commandBuffer);
      }
      commandLength = 0;
    } else if (commandLength < sizeof(commandBuffer) - 1) {
      commandBuffer[commandLength++] = c;
    }
  }
}

void sendEvent(int channel, const Event &event) {
  if (!hostConnected) {
    return;
  }
  char line[48];
  int len = snprintf(line, sizeof(line), "E,%lu,%d,%s,%d\n",
                     (unsigned long)event.time, channel, eventName(event.type), event.value);
  Serial.write(line, len);
}

void setup() {
  pinMode(CS_PIN, OUTPUT);
  digitalWrite(CS_PIN, HIGH);

  SPI.setRX(MISO_PIN);
  SPI.setSCK(SCK_PIN);
  SPI.setTX(MOSI_PIN);
  SPI.begin();

  DetectorConfig config;
  config.sampleIntervalUs = SAMPLE_INTERVAL_US;
  for (int ch = 0; ch < NUM_CHANNELS; ch++) {
    detectors[ch].configure(config);
  }

  Serial.begin(115200);  // baud rate is ignored for USB serial
  nextSampleTime = micros();
}

void loop() {
  // Every new host connection starts in events-only mode
  bool connected = Serial;
  if (connected && !hostConnected) {
    streamSamples = false;
    Serial.println("# events: E,time_us,channel,type,value");
  }
  hostConnected = connected;

  if (hostConnected) {
    readCommands();
  }

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

  if (hostConnected && streamSamples) {
    char line[96];
    int len = snprintf(line, sizeof(line), "%lu", now);
    for (int ch = 0; ch < NUM_CHANNELS; ch++) {
      len += snprintf(line + len, sizeof(line) - len, ",%d", values[ch]);
    }
    line[len++] = '\n';
    Serial.write(line, len);
  }

  for (int ch = 0; ch < NUM_CHANNELS; ch++) {
    Event events[Detector::MAX_EVENTS];
    int count = detectors[ch].update(values[ch], now, events);
    for (int i = 0; i < count; i++) {
      sendEvent(ch, events[i]);
    }
  }
}
