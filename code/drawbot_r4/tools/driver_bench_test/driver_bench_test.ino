// Temporary, belt-disconnected A4988 test. Do not flash this for normal use.
// Set STEP_PIN and DIR_PIN for one empty shield socket, install only that
// driver's module and motor while power is OFF, then apply motor power.

constexpr uint8_t STEP_PIN = 2;  // X socket; use 3 for Y
constexpr uint8_t DIR_PIN = 5;   // X socket; use 6 for Y
constexpr uint8_t ENABLE_PIN = 8;
constexpr uint8_t MS1_PIN = 7;
constexpr uint8_t MS2_PIN = 12;
constexpr uint8_t MS3_PIN = 13;
constexpr int STEPS_EACH_WAY = 400;
constexpr uint16_t STEP_INTERVAL_US = 1500;

void setup() {
  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  pinMode(ENABLE_PIN, OUTPUT);
  pinMode(MS1_PIN, OUTPUT);
  pinMode(MS2_PIN, OUTPUT);
  pinMode(MS3_PIN, OUTPUT);
  // Full-step mode for this simple electrical test.
  digitalWrite(MS1_PIN, LOW);
  digitalWrite(MS2_PIN, LOW);
  digitalWrite(MS3_PIN, LOW);
  digitalWrite(ENABLE_PIN, LOW);
}

void moveSteps(bool direction) {
  digitalWrite(DIR_PIN, direction ? HIGH : LOW);
  delay(10);
  for (int i = 0; i < STEPS_EACH_WAY; ++i) {
    digitalWrite(STEP_PIN, HIGH);
    delayMicroseconds(3);
    digitalWrite(STEP_PIN, LOW);
    delayMicroseconds(STEP_INTERVAL_US - 3);
  }
}

void loop() {
  moveSteps(true);
  delay(1000);
  moveSteps(false);
  delay(3000);
}
