
/*
  Raspberry Pi Pico - Touch + Trigger Data Logger

  A0: Analog touch sensor (voltage divider)
  GP15: Trigger pad
  Output: CSV over USB serial at 115200 baud
*/

const int SENSOR_PIN = A0;
const int TRIGGER_PIN = 15;

const unsigned long SAMPLE_INTERVAL_MS = 2;
unsigned long lastSampleTime = 0;

void setup() {
  pinMode(TRIGGER_PIN, INPUT);

  Serial.begin(115200);
  delay(1000);

  // CSV column names
  Serial.println("time_ms,touch_raw,trigger_state");
}

void loop() {
  unsigned long now = millis();

  if (now - lastSampleTime >= SAMPLE_INTERVAL_MS) {
    lastSampleTime = now;

    int touchRaw = analogRead(SENSOR_PIN);
    int triggerState = digitalRead(TRIGGER_PIN);

    Serial.print(now);
    Serial.print(",");
    Serial.print(touchRaw);
    Serial.print(",");
    Serial.println(triggerState);
  }
}
