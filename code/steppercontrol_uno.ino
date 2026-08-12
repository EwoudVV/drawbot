// coreXY drawing machine
// commands: w,s,a,d,ul,ur,dl,dr [distance_cm]; c [diameter_mm]

#include <math.h>

// pin mapping
const int stepPin1 = 5;   // Motor A STEP
const int dirPin1  = 6;   // Motor A DIR
const int stepPin2 = 8;   // Motor B STEP
const int dirPin2  = 7;   // Motor B DIR
const int ms1 = 2, ms2 = 3, ms3 = 4;  // shared microstep pins

// mechanical calibration
float stepsPerMM = 40.4;   // calibrated for gt6 timing belt

// motion params
int baseDelay = 150;       // ms between step edges
int segments  = 360;       // circle smoothness

// ------------------------

void setMicrostep(int mode) {
  // mode = 1,2,4,8,16
  switch (mode) {
    case 1:  digitalWrite(ms1, LOW);  digitalWrite(ms2, LOW);  digitalWrite(ms3, LOW);  break;
    case 2:  digitalWrite(ms1, HIGH); digitalWrite(ms2, LOW);  digitalWrite(ms3, LOW);  break;
    case 4:  digitalWrite(ms1, LOW);  digitalWrite(ms2, HIGH); digitalWrite(ms3, LOW);  break;
    case 8:  digitalWrite(ms1, HIGH); digitalWrite(ms2, HIGH); digitalWrite(ms3, LOW);  break;
    case 16: digitalWrite(ms1, HIGH); digitalWrite(ms2, HIGH); digitalWrite(ms3, HIGH); break;
  }
}

void setup() {
  pinMode(stepPin1, OUTPUT);
  pinMode(dirPin1, OUTPUT);
  pinMode(stepPin2, OUTPUT);
  pinMode(dirPin2, OUTPUT);
  pinMode(ms1, OUTPUT);
  pinMode(ms2, OUTPUT);
  pinMode(ms3, OUTPUT);
  setMicrostep(8); // default ms mode

  Serial.begin(115200);
  Serial.println(F("Commands: w,a,s,d,ul,ur,dl,dr [mm]; c [diameter_mm]"));
  Serial.print(F("Steps/mm = ")); Serial.println(stepsPerMM, 3);
}

void stepMotor(int stepPin, int dirPin, bool dir, int delayUs) {
  digitalWrite(dirPin, dir);
  digitalWrite(stepPin, HIGH);
  delayMicroseconds(delayUs);
  digitalWrite(stepPin, LOW);
  delayMicroseconds(delayUs);
}

void stepCoreXY(float dx, float dy, int delayUs) {
  float aSteps = dx + dy;
  float bSteps = dx - dy;
  bool dirA = aSteps >= 0;
  bool dirB = bSteps >= 0;

  int stepsA = abs((int)aSteps);
  int stepsB = abs((int)bSteps);
  int maxSteps = max(stepsA, stepsB);

  for (int i = 0; i < maxSteps; i++) {
    if (i < stepsA) stepMotor(stepPin1, dirPin1, dirA, delayUs);
    if (i < stepsB) stepMotor(stepPin2, dirPin2, dirB, delayUs);
  }
}

void stepBoth(bool dir1, bool dir2, long steps, int delayUs) {
  digitalWrite(dirPin1, dir1);
  digitalWrite(dirPin2, dir2);
  for (long i = 0; i < steps; i++) {
    digitalWrite(stepPin1, HIGH);
    digitalWrite(stepPin2, HIGH);
    delayMicroseconds(delayUs);
    digitalWrite(stepPin1, LOW);
    digitalWrite(stepPin2, LOW);
    delayMicroseconds(delayUs);
  }
}

void stepSingle(int stepPin, int dirPin, bool dir, long steps, int delayUs) {
  digitalWrite(dirPin, dir);
  for (long i = 0; i < steps; i++) {
    digitalWrite(stepPin, HIGH);
    delayMicroseconds(delayUs);
    digitalWrite(stepPin, LOW);
    delayMicroseconds(delayUs);
  }
}

void moveLine(char axis, float distMM) {
  long steps = lround(distMM * stepsPerMM);
  setMicrostep(1); // full step for straight moves
  Serial.print(F("Line ")); Serial.print(axis);
  Serial.print(F(" ")); Serial.print(distMM, 2);
  Serial.print(F(" mm (" )); Serial.print(steps); Serial.println(F(" steps)"));

  if (axis == 'w')      stepBoth(true,  false, steps, baseDelay * 5);
  else if (axis == 's') stepBoth(false, true,  steps, baseDelay * 5);
  else if (axis == 'a') stepBoth(false, false, steps, baseDelay * 5);
  else if (axis == 'd') stepBoth(true,  true,  steps, baseDelay * 5);
  else if (axis == 'u') stepSingle(stepPin1, dirPin1, true,  steps, baseDelay * 5);
  else if (axis == 'U') stepSingle(stepPin2, dirPin2, true,  steps, baseDelay * 5);
  else if (axis == 'D') stepSingle(stepPin2, dirPin2, false, steps, baseDelay * 5);
  else if (axis == 'L') stepSingle(stepPin1, dirPin1, false, steps, baseDelay * 5);
}

void drawCircle(float diameterMM) {
  int mode;
  if (diameterMM <= 20)      mode = 16;
  else if (diameterMM <= 50) mode = 8;
  else                       mode = 4;
  setMicrostep(mode);

  float stepsPerMMScaled = stepsPerMM * mode;
  float radiusSteps = (diameterMM * stepsPerMMScaled) / 2.0;
  const float twoPi = 6.2831853;

  float prevX = radiusSteps, prevY = 0;
  Serial.print(F("Circle: dia="));
  Serial.print(diameterMM, 1);
  Serial.print(F(" mm (mode 1/")); Serial.print(mode); Serial.println(F(")"));

  for (int i = 1; i <= segments; i++) {
    float theta = twoPi * i / segments;
    float x = radiusSteps * cos(theta);
    float y = radiusSteps * sin(theta);
    float dx = x - prevX;
    float dy = y - prevY;
    stepCoreXY(dx, dy, baseDelay);
    prevX = x;
    prevY = y;
  }
  setMicrostep(1);
}

void loop() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd.length() == 0) return;

    int spaceIdx = cmd.indexOf(' ');
    String arg = (spaceIdx > 0) ? cmd.substring(spaceIdx + 1) : "";
    float val = arg.toFloat();

    // movement commands
    if (cmd.startsWith("w")) moveLine('w', val);
    else if (cmd.startsWith("s")) moveLine('s', val);
    else if (cmd.startsWith("a")) moveLine('a', val);
    else if (cmd.startsWith("d")) moveLine('d', val);
    else if (cmd.startsWith("ul")) moveLine('u', val);
    else if (cmd.startsWith("ur")) moveLine('U', val);
    else if (cmd.startsWith("dl")) moveLine('L', val);
    else if (cmd.startsWith("dr")) moveLine('D', val);
    else if (cmd.startsWith("c"))  drawCircle(val);
    else Serial.println(F("Invalid command"));
  }
}
