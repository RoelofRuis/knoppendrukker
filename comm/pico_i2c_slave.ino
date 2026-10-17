/*
  =============================================================================
  Raspberry Pi Pico (RP2040) -> Raspberry Pi I2C Slave + IRQ Notification
  =============================================================================

  DESCRIPTION:
  This Arduino sketch configures a Raspberry Pi Pico (RP2040) as an I2C Slave
  (Target) device that periodically generates test messages and notifies a host
  Raspberry Pi via an active-LOW Interrupt Request (IRQ) pin.

  When new data is ready (every 1 second):
    1. Pico generates a random test message and loads it into the I2C tx buffer.
    2. Pico pulls the IRQ pin LOW to signal the Raspberry Pi.
    3. The Raspberry Pi detects the falling edge / LOW level on its GPIO pin.
    4. Raspberry Pi initiates an I2C READ transaction to the Pico address (0x17).
    5. In the I2C request handler, Pico transmits the data buffer and releases
       the IRQ pin (sets it back HIGH / Idle).

  -----------------------------------------------------------------------------
  WIRING & PIN CONNECTIONS
  -----------------------------------------------------------------------------
  
  Raspberry Pi Pico (RP2040)           Raspberry Pi (3 / 4 / 5 / Zero 2W)
  =================================    ==================================
  GP4  (Physical Pin 6)  - I2C0 SDA -> GPIO 2 / SDA1 (Physical Pin 3)
  GP5  (Physical Pin 7)  - I2C0 SCL -> GPIO 3 / SCL1 (Physical Pin 5)
  GP14 (Physical Pin 19) - IRQ Line -> GPIO 22       (Physical Pin 15)
  GND  (Physical Pin 8 or any GND)  -> GND            (Physical Pin 6 or any GND)

  -----------------------------------------------------------------------------
  ELECTRICAL & HARDWARE NOTES:
  -----------------------------------------------------------------------------
  1. LOGIC VOLTAGE: Both Raspberry Pi and Raspberry Pi Pico use 3.3V logic.
     Connections are direct; NO logic level shifters are needed.
     WARNING: Do not connect any 5V pins directly to the Pico or Pi GPIOs!

  2. COMMON GROUND (GND): You MUST connect at least one GND pin between the
     Pico and the Raspberry Pi to establish a common voltage reference.

  3. I2C PULL-UP RESISTORS: The Raspberry Pi already has built-in 1.8 kOhm
     hardware pull-up resistors to 3.3V on GPIO 2 (SDA) and GPIO 3 (SCL).
     No external pull-up resistors are required for short wire runs (<30 cm).

  4. IRQ PIN LOGIC:
     - Idle state: HIGH (3.3V)
     - Active / Data Ready state: LOW (0V)
     - The Raspberry Pi GPIO pin should have internal pull-up enabled and
       listen for falling edge (or LOW level).
     - Once the host reads the I2C data, the Pico resets the IRQ pin to HIGH.

  -----------------------------------------------------------------------------
  RASPBERRY PI SETUP:
  -----------------------------------------------------------------------------
  1. Enable I2C on Raspberry Pi:
       sudo raspi-config -> Interface Options -> I2C -> Enable
       (or add 'dtparam=i2c_arm=on' to /boot/config.txt or /boot/firmware/config.txt)
  2. Verify device detection on bus 1:
       sudo apt-get install -y i2c-tools
       i2cdetect -y 1
       (Address 0x17 should appear in the table)
  =============================================================================
*/

#include <Wire.h>

// ---------------------------------------------------------------------------
// Configuration Constants
// ---------------------------------------------------------------------------
const uint8_t I2C_SLAVE_ADDR = 0x17;  // 7-bit I2C Slave Address (decimal 23)
const int SDA_PIN            = 4;     // Pico GP4 (Physical Pin 6) -> I2C0 SDA
const int SCL_PIN            = 5;     // Pico GP5 (Physical Pin 7) -> I2C0 SCL
const int IRQ_PIN            = 14;    // Pico GP14 (Physical Pin 19) -> IRQ Out
const int LED_PIN            = LED_BUILTIN; // Onboard LED for status indication

const unsigned long MESSAGE_INTERVAL_MS = 1000; // Send test message every 1 second
const size_t BUFFER_SIZE = 32;                  // Standard I2C payload buffer size

// ---------------------------------------------------------------------------
// State Variables
// ---------------------------------------------------------------------------
volatile char txBuffer[BUFFER_SIZE];
volatile uint8_t txLength = 0;
volatile bool dataReady = false;

unsigned long lastMessageTime = 0;
unsigned long messageCounter  = 0;

// ---------------------------------------------------------------------------
// I2C Event Handlers (Called in Interrupt Context)
// ---------------------------------------------------------------------------

/**
 * Called when the I2C Master (Raspberry Pi) requests data from this device.
 * Sends the current message buffer over I2C and clears the IRQ line.
 */
void requestEvent() {
  if (txLength > 0) {
    Wire.write((const uint8_t*)txBuffer, txLength);
  } else {
    // Send a null byte if no message is available
    Wire.write((uint8_t)0x00);
  }

  // Deassert IRQ (set HIGH = Idle) to signal data has been consumed
  digitalWrite(IRQ_PIN, HIGH);
  digitalWrite(LED_PIN, LOW);
  dataReady = false;
}

/**
 * Called when the I2C Master writes data to this device.
 * (Optional handler for bidirectional command handling)
 */
void receiveEvent(int numBytes) {
  while (Wire.available()) {
    char c = Wire.read();
    // Optional: process incoming commands from Raspberry Pi
    (void)c;
  }
}

// ---------------------------------------------------------------------------
// Arduino Setup & Loop
// ---------------------------------------------------------------------------

void setup() {
  // Initialize onboard LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Initialize IRQ pin (Active-LOW: default HIGH / idle)
  pinMode(IRQ_PIN, OUTPUT);
  digitalWrite(IRQ_PIN, HIGH);

  // Initialize optional USB Serial for local debugging
  Serial.begin(115200);

  // Initialize pseudo-random generator with noise from floating analog pin
  randomSeed(analogRead(A0));

  // Configure I2C Hardware Pins (Earle Philhower / standard RP2040 core)
  #if defined(ARDUINO_ARCH_RP2040)
  Wire.setSDA(SDA_PIN);
  Wire.setSCL(SCL_PIN);
  #endif

  // Join I2C bus as Slave / Target device
  Wire.begin(I2C_SLAVE_ADDR);
  Wire.onRequest(requestEvent);
  Wire.onReceive(receiveEvent);

  Serial.println("==================================================");
  Serial.println("Raspberry Pi Pico I2C Slave + IRQ Initialized");
  Serial.print("I2C Address: 0x");
  Serial.println(I2C_SLAVE_ADDR, HEX);
  Serial.print("SDA Pin: GP"); Serial.println(SDA_PIN);
  Serial.print("SCL Pin: GP"); Serial.println(SCL_PIN);
  Serial.print("IRQ Pin: GP"); Serial.println(IRQ_PIN);
  Serial.println("==================================================");
}

void loop() {
  unsigned long now = millis();

  // Generate and prepare a random test message every second
  if (now - lastMessageTime >= MESSAGE_INTERVAL_MS) {
    lastMessageTime = now;
    messageCounter++;

    // Generate random test payload (e.g. simulated sensor reading or random integer)
    long randomValue = random(1000, 9999);

    // Prepare temporary string message (format: "MSG:<count>,VAL:<rand>")
    char tempBuf[BUFFER_SIZE];
    snprintf(tempBuf, sizeof(tempBuf), "MSG:%lu,VAL:%ld", messageCounter, randomValue);

    // Atomically update txBuffer
    noInterrupts();
    strncpy((char*)txBuffer, tempBuf, BUFFER_SIZE - 1);
    txBuffer[BUFFER_SIZE - 1] = '\0';
    txLength = strlen((char*)txBuffer);
    dataReady = true;

    // Assert IRQ (Active LOW: pull LOW to notify Raspberry Pi)
    digitalWrite(IRQ_PIN, LOW);
    digitalWrite(LED_PIN, HIGH);
    interrupts();

    // Debug output over USB Serial
    Serial.print("[Pico TX] Ready: \"");
    Serial.print((char*)txBuffer);
    Serial.print("\" (len: ");
    Serial.print(txLength);
    Serial.println(") -> IRQ Asserted (LOW)");
  }
}
