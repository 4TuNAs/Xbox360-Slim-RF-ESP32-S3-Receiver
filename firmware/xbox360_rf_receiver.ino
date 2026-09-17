#include <Arduino.h>

static constexpr uint8_t RF_CLOCK_PIN = 3;
static constexpr uint8_t RF_DATA_PIN  = 4;
static constexpr uint8_t SYNC_PIN     = 5;

static constexpr uint32_t CLOCK_TIMEOUT_US = 200000;

static const uint8_t START_COMMAND[10] = {
  0, 0, 0, 0, 0, 1, 0, 0, 1, 0
};

static const uint8_t POWER_COMMAND[10] = {
  0, 0, 1, 0, 0, 0, 0, 1, 0, 1
};

static const uint8_t SYNC_COMMAND[11] = {
  0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1
};

bool waitClockLevel(uint8_t requiredLevel)
{
  uint32_t started = micros();

  while (digitalRead(RF_CLOCK_PIN) != requiredLevel) {
    if ((uint32_t)(micros() - started) > CLOCK_TIMEOUT_US) {
      return false;
    }
    delayMicroseconds(2);
  }

  return true;
}

void releaseDataLine()
{
  // Release DATA and keep the ESP32 internal pull-up enabled.
  // External 10 kOhm pull-ups on DATA and CLOCK are still required.
  pinMode(RF_DATA_PIN, INPUT_PULLUP);
}

bool sendCommand(const uint8_t *bits, size_t count)
{
  Serial.printf("Sending %u bits...\n", (unsigned)count);

  /*
   * Pulling DATA low tells the RF board that a command is starting.
   * The RF board should then begin generating CLOCK.
   */
  pinMode(RF_DATA_PIN, OUTPUT);
  digitalWrite(RF_DATA_PIN, LOW);

  delayMicroseconds(20);

  for (size_t i = 0; i < count; i++) {
    // Wait for CLOCK to go LOW.
    if (!waitClockLevel(LOW)) {
      releaseDataLine();
      Serial.printf("Error: no CLOCK falling edge, bit %u\n", (unsigned)i);
      return false;
    }

    delayMicroseconds(1000);
    digitalWrite(RF_DATA_PIN, bits[i] ? HIGH : LOW);

    // Wait for CLOCK to go HIGH.
    if (!waitClockLevel(HIGH)) {
      releaseDataLine();
      Serial.printf("Error: no CLOCK rising edge, bit %u\n", (unsigned)i);
      return false;
    }
  }

  /*
   * On the Slim RF board, DATA must be driven HIGH immediately after
   * the last bit and then released.
   */
  digitalWrite(RF_DATA_PIN, HIGH);
  delayMicroseconds(10);
  releaseDataLine();

  Serial.println("Command sent");
  return true;
}

bool initializeRf()
{
  Serial.println("Initializing RF board...");

  if (!sendCommand(START_COMMAND, 10)) {
    Serial.println("START_COMMAND failed");
    return false;
  }

  delay(50);

  if (!sendCommand(POWER_COMMAND, 10)) {
    Serial.println("POWER_COMMAND failed");
    return false;
  }

  delay(50);
  Serial.println("RF board initialized");
  return true;
}

void startSync()
{
  Serial.println("Sending SYNC command...");

  if (sendCommand(SYNC_COMMAND, 11)) {
    Serial.println("Turn on the controller and press its Sync button");
  } else {
    Serial.println("SYNC command failed");
  }
}

void setup()
{
  Serial.begin(115200);
  delay(1000);

  pinMode(RF_CLOCK_PIN, INPUT_PULLUP);
  pinMode(RF_DATA_PIN, INPUT_PULLUP);
  pinMode(SYNC_PIN, INPUT_PULLUP);

  Serial.println();
  Serial.println("Xbox 360 Slim RF + ESP32-S3");
  Serial.printf("DATA idle level: %d\n", digitalRead(RF_DATA_PIN));
  Serial.printf("CLOCK idle level: %d\n", digitalRead(RF_CLOCK_PIN));

  delay(1000);
  initializeRf();

  Serial.println("Press BOOT or send S to start Sync");
}

void loop()
{
  static bool previousButton = HIGH;
  bool button = digitalRead(SYNC_PIN);

  if (previousButton == HIGH && button == LOW) {
    delay(30);

    if (digitalRead(SYNC_PIN) == LOW) {
      startSync();

      while (digitalRead(SYNC_PIN) == LOW) {
        delay(10);
      }
    }
  }

  previousButton = button;

  while (Serial.available()) {
    char command = Serial.read();

    if (command == 's' || command == 'S') {
      startSync();
    }

    if (command == 'i' || command == 'I') {
      initializeRf();
    }
  }

  delay(5);
}
