/*
  =============================================================================
  Raspberry Pi Pico (RP2040) -> Raspberry Pi USB Serial Communication
  =============================================================================

  DESCRIPTION:
  This Arduino sketch configures a Raspberry Pi Pico (RP2040) to communicate
  with a Raspberry Pi host exclusively over a standard USB cable (USB CDC /
  Virtual COM Port).

  Operation:
    1. Pico boots and initializes USB CDC Serial.
    2. Every 1 second (1000 ms), Pico generates a random test message.
    3. Pico transmits the formatted message string over USB Serial (ending with '\n').
    4. Pico blinks its onboard LED on each transmission for visual feedback.
    5. The connected Raspberry Pi reads the incoming stream via /dev/ttyACM0
       (or /dev/ttyACM*) using Python.

  -----------------------------------------------------------------------------
  HARDWARE SETUP & WIRING:
  -----------------------------------------------------------------------------
  No jumper wires required! Simply connect a standard Micro-USB to USB-A cable:
  
    Raspberry Pi Pico (Micro-USB) <====== USB Cable ======> Raspberry Pi (USB Port)

  Benefits of USB-only setup:
    - Single cable supplies both power (5V VBUS -> Pico 3.3V SMPS) and data.
    - Automatic common ground.
    - No GPIO header pins or external pull-up resistors needed.
    - Native USB 1.1 Full Speed (12 Mbps) data transfer.

  -----------------------------------------------------------------------------
  FLASHING INSTRUCTIONS:
  -----------------------------------------------------------------------------
  1. Open this file in Arduino IDE.
  2. Select Board: "Raspberry Pi Pico" (from Earle Philhower or Official RP2040 core).
  3. Select Port: The corresponding Serial / UF2 port.
  4. Hold the BOOTSEL button while connecting USB to enter bootloader if needed.
  5. Click Upload.
  =============================================================================
*/

// ---------------------------------------------------------------------------
// Configuration Constants
// ---------------------------------------------------------------------------
const unsigned long SERIAL_BAUD_RATE    = 115200; // Standard baud rate (USB CDC uses full USB speed regardless)
const unsigned long MESSAGE_INTERVAL_MS = 1000;   // Transmit message every 1 second
const int LED_PIN                       = LED_BUILTIN; // Onboard status LED

// ---------------------------------------------------------------------------
// State Variables
// ---------------------------------------------------------------------------
unsigned long lastMessageTime = 0;
unsigned long messageCounter  = 0;

void setup() {
  // Initialize onboard LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Initialize USB Serial interface
  Serial.begin(SERIAL_BAUD_RATE);

  // Seed random generator using noise from unconnected analog pin A0
  randomSeed(analogRead(A0));
}

void loop() {
  unsigned long now = millis();

  // Send a test message every 1 second
  if (now - lastMessageTime >= MESSAGE_INTERVAL_MS) {
    lastMessageTime = now;
    messageCounter++;

    // Generate random test payload (e.g. simulated sensor reading or random integer)
    long randomValue = random(1000, 9999);

    // Turn LED ON to indicate data transmission
    digitalWrite(LED_PIN, HIGH);

    // Send formatted message string followed by newline
    // Format: "MSG:<counter>,VAL:<random_number>"
    Serial.print("MSG:");
    Serial.print(messageCounter);
    Serial.print(",VAL:");
    Serial.println(randomValue);

    // Keep LED on briefly for visibility (non-blocking in next cycle or quick blink)
    delay(20);
    digitalWrite(LED_PIN, LOW);
  }
}
