#include <AccelStepper.h>

// Driver 1: X axis
#define X_STEP_PIN   2   // D2 → STEP
#define X_DIR_PIN    3   // D3 → DIR
// Driver 2: Y axis
#define Y_STEP_PIN   4   // D4 → STEP
#define Y_DIR_PIN    5   // D5 → DIR

// Microstep pins
#define X_MS1_PIN    8
#define X_MS2_PIN    9
#define X_MS3_PIN   10

#define Y_MS1_PIN   11
#define Y_MS2_PIN   12
#define Y_MS3_PIN   13

// Endstops
#define X_MIN_PIN   12
#define Y_MIN_PIN   13

// Motion config
// 200 steps/rev × full‑step / (20 teeth × 2 mm) = 80 steps/mm
const float STEPS_PER_MM = 80.0;
const float MAX_SPEED    = 8000.0;  // steps/sec
const float ACCEL        = 400.0;   // steps/sec^2
const float HOME_SPEED   = 2000.0;  // steps/sec for homing

AccelStepper motorX(AccelStepper::DRIVER, X_STEP_PIN, X_DIR_PIN);
AccelStepper motorY(AccelStepper::DRIVER, Y_STEP_PIN, Y_DIR_PIN);

void setup() {
  // microstep pin setup
  pinMode(X_MS1_PIN, OUTPUT);
  pinMode(X_MS2_PIN, OUTPUT);
  pinMode(X_MS3_PIN, OUTPUT);
  pinMode(Y_MS1_PIN, OUTPUT);
  pinMode(Y_MS2_PIN, OUTPUT);
  pinMode(Y_MS3_PIN, OUTPUT);
  // default to 1/16‑step
  digitalWrite(X_MS1_PIN, HIGH);
  digitalWrite(X_MS2_PIN, HIGH);
  digitalWrite(X_MS3_PIN, HIGH);
  digitalWrite(Y_MS1_PIN, HIGH);
  digitalWrite(Y_MS2_PIN, HIGH);
  digitalWrite(Y_MS3_PIN, HIGH);

  // endstops
  pinMode(X_MIN_PIN, INPUT_PULLUP);
  pinMode(Y_MIN_PIN, INPUT_PULLUP);

  // steppers
  motorX.setMaxSpeed(MAX_SPEED);
  motorX.setAcceleration(ACCEL);
  motorY.setMaxSpeed(MAX_SPEED);
  motorY.setAcceleration(ACCEL);

  // serial for G‑code
  Serial.begin(115200);
  while (!Serial);

  homeAll();
  Serial.println("ready");
}

void loop() {
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length()) parseGcode(line);
  }
}

void parseGcode(const String &line) {
  if (line.startsWith("G28")) {
    homeAll();
    Serial.println("ok");
    return;
  }
  if (line.startsWith("G1")) {
    float X = NAN, Y = NAN;
    int i;
    if ((i = line.indexOf('X')) != -1) X = line.substring(i+1).toFloat();
    if ((i = line.indexOf('Y')) != -1) Y = line.substring(i+1).toFloat();

    long tX = motorX.currentPosition();
    long tY = motorY.currentPosition();

    if (!isnan(X) && !isnan(Y)) {
      // Core XY kinematics
      tX = long(( X + Y ) * STEPS_PER_MM);
      tY = long(( X - Y ) * STEPS_PER_MM);
    } else if (!isnan(X)) {
      tX = tY = long(X * STEPS_PER_MM);
    } else if (!isnan(Y)) {
      tX =  long(Y * STEPS_PER_MM);
      tY = -long(Y * STEPS_PER_MM);
    }

    motorX.moveTo(tX);
    motorY.moveTo(tY);
    while (motorX.distanceToGo() || motorY.distanceToGo()) {
      motorX.run();
      motorY.run();
    }
    Serial.println("ok");
  }
}

void homeAll() {
  // Home X
  motorX.setMaxSpeed(HOME_SPEED);
  motorY.setMaxSpeed(HOME_SPEED);
  motorX.moveTo(-100000);
  motorY.moveTo(-100000);
  while (digitalRead(X_MIN_PIN)) {
    motorX.run(); motorY.run();
  }
  motorX.stop(); motorY.stop();
  while (motorX.isRunning()||motorY.isRunning()) {
    motorX.run(); motorY.run();
  }
  
  motorX.moveTo(+10*STEPS_PER_MM);
  motorY.moveTo(+10*STEPS_PER_MM);
  while (motorX.isRunning()||motorY.isRunning()) {
    motorX.run(); motorY.run();
  }
  motorX.setCurrentPosition(0);
  motorY.setCurrentPosition(0);

  // Home Y
  motorX.moveTo(-100000);
  motorY.moveTo(+100000);
  while (digitalRead(Y_MIN_PIN)) {
    motorX.run(); motorY.run();
  }
  motorX.stop(); motorY.stop();
  while (motorX.isRunning()||motorY.isRunning()) {
    motorX.run(); motorY.run();
  }
  // back off 10 mm
  motorX.moveTo(+10*STEPS_PER_MM);
  motorY.moveTo(-10*STEPS_PER_MM);
  while (motorX.isRunning()||motorY.isRunning()) {
    motorX.run(); motorY.run();
  }
  motorX.setCurrentPosition(0);
  motorY.setCurrentPosition(0);

  // restore travel params
  motorX.setMaxSpeed(MAX_SPEED);
  motorX.setAcceleration(ACCEL);
  motorY.setMaxSpeed(MAX_SPEED);
  motorY.setAcceleration(ACCEL);

  Serial.println("Homing complete");
}
