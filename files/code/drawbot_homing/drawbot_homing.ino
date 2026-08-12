/*
  ESP32 CoreXY pen plotter: minimal G-code interpreter (absolute, mm)
  Updated:
    - On M2/M30: pen up, HOME (left then down), reset modal state, keep accepting more jobs.
    - Still stops permanently on "error:" until reset.

  Supported:
    G21                ; set mm units (required before motion)
    G90                ; absolute mode (required before motion)
    G0 X.. Y.. [F..]   ; travel (forces pen up)
    G1 X.. Y.. [F..]   ; draw   (forces pen down)
    M3                 ; pen down
    M5                 ; pen up
    M2 / M30           ; end job (pen up, home, ready)

    - F is interpreted as mm/min.
    - Coordinates are clamped to the workspace.
    - Executes line-by-line over Serial and replies "ok" per line.
    - Homes on boot: left until X switch, then down until Y switch.

  Pins:
    MS1=18 MS2=19 MS3=21
    Left motor:  STEP_L=32 DIR_L=33
    Right motor: STEP_R=26 DIR_R=25
    Limit X-left: GPIO34 (pressed -> GND, needs external pullup to 3.3V)
    Limit Y-down: GPIO35 (pressed -> GND, needs external pullup to 3.3V)
    Servo: GPIO27 (powered from 5 V buck, common GND)

  Limit switch pullups:
    GPIO34/35 do NOT have internal pullups.
    Add 10k from GPIO34 -> 3.3V and 10k from GPIO35 -> 3.3V.
    Switch between GPIO pin and GND. Pressed reads LOW.
*/

#include <Arduino.h>
#include <ESP32Servo.h>
#include <math.h>

// ---------------- Pins ----------------
constexpr int MS1_PIN = 18;
constexpr int MS2_PIN = 19;
constexpr int MS3_PIN = 21;

constexpr int STEP_L = 32;
constexpr int DIR_L = 33;

constexpr int STEP_R = 26;
constexpr int DIR_R = 25;

constexpr int LIMIT_X_LEFT = 34;
constexpr int LIMIT_Y_DOWN = 35;

constexpr int SERVO_PIN = 27;

// ---------------- Workspace ----------------
constexpr float X_MAX_MM = 400.0f;
constexpr float Y_MAX_MM = 320.0f;

// Calibration base (axis-step units per mm at full step)
constexpr float BASE_STEPS_PER_MM_FULLSTEP = 10.0f;

// Fixed microstepping for v1
constexpr int MICROSTEP_FACTOR = 8;

// ---------------- Motor capability model ----------------
constexpr float MOTOR_MAX_RPM = 200.0f;
constexpr float MOTOR_FULL_STEPS_PER_REV = 200.0f;
constexpr float MOTOR_MAX_FULLSTEP_SPS =
    (MOTOR_MAX_RPM * MOTOR_FULL_STEPS_PER_REV) / 60.0f;  // ~666.7 full-steps/s
constexpr float MOTOR_MAX_MICROSTEP_SPS =
    MOTOR_MAX_FULLSTEP_SPS * MICROSTEP_FACTOR;

// ---------------- Timing ----------------
constexpr uint16_t PULSE_US = 8;

// Default feedrates (mm/min)
constexpr float FEED_TRAVEL_MM_MIN = 3000.0f;
constexpr float FEED_DRAW_MM_MIN = 1200.0f;
constexpr float FEED_HOME_MM_MIN = 600.0f;

constexpr uint32_t MAX_HOME_STEPS = 400000;

// ---------------- Servo ----------------
constexpr int SERVO_HZ = 50;
constexpr int SERVO_MIN_US = 500;
constexpr int SERVO_MAX_US = 2500;

// Your calibration
constexpr int PEN_UP_US = 1000;
constexpr int PEN_DOWN_US = 2000;

// ---------------- State ----------------
Servo penServo;

bool units_mm = false;
bool absolute_mode = false;
bool stopped = false;

float cur_x_mm = 0.0f;
float cur_y_mm = 0.0f;

float current_feed_mm_min = FEED_TRAVEL_MM_MIN;
uint32_t step_low_delay_us = 2000;

// ---------------- Parsing ----------------
struct ParsedLine {
  bool hasG = false;
  int g = -1;

  bool hasM = false;
  int m = -1;

  bool hasX = false;
  float x = 0.0f;

  bool hasY = false;
  float y = 0.0f;

  bool hasF = false;
  float f = 0.0f;
};

// ---------------- Helpers ----------------
static inline float clampf(float v, float lo, float hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

static inline int32_t iround(float x) {
  return (int32_t)lroundf(x);
}

static inline bool swPressed(int pin) {
  return digitalRead(pin) == LOW;
}

static inline void setMotorDirs(bool dirL_forward, bool dirR_forward) {
  // L+: up-right, R+: down-right
  digitalWrite(DIR_L, dirL_forward ? HIGH : LOW);
  digitalWrite(DIR_R, dirR_forward ? HIGH : LOW);
}

static inline void pulseOne(int stepPin) {
  digitalWrite(stepPin, HIGH);
  delayMicroseconds(PULSE_US);
  digitalWrite(stepPin, LOW);
  delayMicroseconds(step_low_delay_us);
}

static inline void pulseBoth() {
  digitalWrite(STEP_L, HIGH);
  digitalWrite(STEP_R, HIGH);
  delayMicroseconds(PULSE_US);
  digitalWrite(STEP_L, LOW);
  digitalWrite(STEP_R, LOW);
  delayMicroseconds(step_low_delay_us);
}

static inline float stepsPerMmEffective() {
  return BASE_STEPS_PER_MM_FULLSTEP * (float)MICROSTEP_FACTOR;
}

void setMicrosteppingFixed() {
  // A4988:
  // 1    : L L L
  // 1/2  : H L L
  // 1/4  : L H L
  // 1/8  : H H L
  // 1/16 : H H H
  bool ms1 = LOW, ms2 = LOW, ms3 = LOW;

  if (MICROSTEP_FACTOR == 1) { ms1 = LOW; ms2 = LOW; ms3 = LOW; }
  else if (MICROSTEP_FACTOR == 2) { ms1 = HIGH; ms2 = LOW; ms3 = LOW; }
  else if (MICROSTEP_FACTOR == 4) { ms1 = LOW; ms2 = HIGH; ms3 = LOW; }
  else if (MICROSTEP_FACTOR == 8) { ms1 = HIGH; ms2 = HIGH; ms3 = LOW; }
  else if (MICROSTEP_FACTOR == 16) { ms1 = HIGH; ms2 = HIGH; ms3 = HIGH; }

  digitalWrite(MS1_PIN, ms1);
  digitalWrite(MS2_PIN, ms2);
  digitalWrite(MS3_PIN, ms3);
}

void setFeedMmMin(float f_mm_min) {
  if (f_mm_min <= 0.0f) return;

  current_feed_mm_min = f_mm_min;

  float v_mm_s = f_mm_min / 60.0f;
  float axis_sps = v_mm_s * stepsPerMmEffective();

  // Conservative clamp to motor capability
  float axis_sps_cap = 2.0f * MOTOR_MAX_MICROSTEP_SPS;
  if (axis_sps > axis_sps_cap) axis_sps = axis_sps_cap;

  float period_us = 1000000.0f / axis_sps;
  float low_us = period_us - (float)PULSE_US;
  if (low_us < 50.0f) low_us = 50.0f;

  step_low_delay_us = (uint32_t)lroundf(low_us);
}

void penUp() {
  penServo.writeMicroseconds(PEN_UP_US);
  delay(200);
}

void penDown() {
  penServo.writeMicroseconds(PEN_DOWN_US);
  delay(200);
}

// ---------------- CoreXY diagonal move ----------------
// L+ => (+x,+y), R+ => (+x,-y)
//
// dx_steps = sL + sR
// dy_steps = sL - sR
// => sL = (dx+dy)/2, sR = (dx-dy)/2
void moveByAxisSteps(int32_t dxSteps, int32_t dySteps) {
  float sLf = 0.5f * (dxSteps + dySteps);
  float sRf = 0.5f * (dxSteps - dySteps);

  int32_t sL = iround(sLf);
  int32_t sR = iround(sRf);

  bool dirL = (sL >= 0);
  bool dirR = (sR >= 0);

  uint32_t nL = (uint32_t)labs(sL);
  uint32_t nR = (uint32_t)labs(sR);

  setMotorDirs(dirL, dirR);

  if (nL == 0 && nR == 0) return;

  uint32_t nMax = (nL > nR) ? nL : nR;
  int32_t errL = 0;
  int32_t errR = 0;

  for (uint32_t i = 0; i < nMax; i++) {
    bool stepL = false;
    bool stepR = false;

    errL += (int32_t)nL;
    errR += (int32_t)nR;

    if (errL >= (int32_t)nMax) { errL -= (int32_t)nMax; stepL = true; }
    if (errR >= (int32_t)nMax) { errR -= (int32_t)nMax; stepR = true; }

    if (stepL && stepR) pulseBoth();
    else if (stepL) pulseOne(STEP_L);
    else if (stepR) pulseOne(STEP_R);
    else delayMicroseconds(step_low_delay_us);
  }

  int32_t dxAch = sL + sR;
  int32_t dyAch = sL - sR;

  float spmm = stepsPerMmEffective();
  cur_x_mm += (float)dxAch / spmm;
  cur_y_mm += (float)dyAch / spmm;

  cur_x_mm = clampf(cur_x_mm, 0.0f, X_MAX_MM);
  cur_y_mm = clampf(cur_y_mm, 0.0f, Y_MAX_MM);
}

void moveToMm(float xMm, float yMm) {
  float tx = clampf(xMm, 0.0f, X_MAX_MM);
  float ty = clampf(yMm, 0.0f, Y_MAX_MM);

  float dxMm = tx - cur_x_mm;
  float dyMm = ty - cur_y_mm;

  float spmm = stepsPerMmEffective();
  int32_t dxSteps = iround(dxMm * spmm);
  int32_t dySteps = iround(dyMm * spmm);

  moveByAxisSteps(dxSteps, dySteps);
}

// ---------------- Homing ----------------
// Left: DIR_L=LOW, DIR_R=LOW, step both until X switch pressed
// Down: DIR_L=LOW, DIR_R=HIGH, step both until Y switch pressed
bool homeLeft() {
  setFeedMmMin(FEED_HOME_MM_MIN);
  setMotorDirs(false, false);

  for (uint32_t i = 0; i < MAX_HOME_STEPS; i++) {
    if (swPressed(LIMIT_X_LEFT)) return true;
    pulseBoth();
  }
  return false;
}

bool homeDown() {
  setFeedMmMin(FEED_HOME_MM_MIN);
  setMotorDirs(false, true);

  for (uint32_t i = 0; i < MAX_HOME_STEPS; i++) {
    if (swPressed(LIMIT_Y_DOWN)) return true;
    pulseBoth();
  }
  return false;
}

bool doHome() {
  penUp();

  if (!homeLeft()) return false;
  if (!homeDown()) return false;

  cur_x_mm = 0.0f;
  cur_y_mm = 0.0f;

  setFeedMmMin(FEED_TRAVEL_MM_MIN);
  return true;
}

// ---------------- G-code parsing ----------------
String stripComments(const String &in) {
  int semi = in.indexOf(';');
  String s = (semi >= 0) ? in.substring(0, semi) : in;

  String out;
  out.reserve(s.length());
  bool inParens = false;
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '(') { inParens = true; continue; }
    if (c == ')') { inParens = false; continue; }
    if (!inParens) out += c;
  }
  out.trim();
  return out;
}

bool parseNumber(const String &s, int startIdx, float &value, int &endIdx) {
  int i = startIdx;
  while (i < (int)s.length() && s[i] == ' ') i++;

  int j = i;
  bool seenDigit = false;
  bool seenDot = false;

  if (j < (int)s.length() && (s[j] == '+' || s[j] == '-')) j++;

  for (; j < (int)s.length(); j++) {
    char c = s[j];
    if (c >= '0' && c <= '9') { seenDigit = true; continue; }
    if (c == '.' && !seenDot) { seenDot = true; continue; }
    break;
  }

  if (!seenDigit) return false;

  value = s.substring(i, j).toFloat();
  endIdx = j;
  return true;
}

bool parseLine(const String &lineIn, ParsedLine &pl) {
  String s = stripComments(lineIn);
  if (s.length() == 0) return true;

  s.toUpperCase();

  int i = 0;
  while (i < (int)s.length()) {
    char c = s[i];

    if (c == ' ' || c == '\t') { i++; continue; }

    if (c == 'G' || c == 'M' || c == 'X' || c == 'Y' || c == 'F') {
      float v = 0.0f;
      int endIdx = i + 1;
      if (!parseNumber(s, i + 1, v, endIdx)) return false;

      if (c == 'G') { pl.hasG = true; pl.g = (int)lroundf(v); }
      else if (c == 'M') { pl.hasM = true; pl.m = (int)lroundf(v); }
      else if (c == 'X') { pl.hasX = true; pl.x = v; }
      else if (c == 'Y') { pl.hasY = true; pl.y = v; }
      else if (c == 'F') { pl.hasF = true; pl.f = v; }

      i = endIdx;
      continue;
    }

    i++;
  }

  return true;
}

void hardStop(const char *msg) {
  penUp();
  stopped = true;
  Serial.print("error: ");
  Serial.println(msg);
}

// Reset modal state between jobs (after M2/M30)
void resetJobState() {
  units_mm = false;
  absolute_mode = false;
  setFeedMmMin(FEED_TRAVEL_MM_MIN);
  penUp();
}

// ---------------- Execute one parsed line ----------------
void executeParsed(const ParsedLine &pl) {
  // Unsupported meaning-changing modes
  if (pl.hasG && pl.g == 20) { hardStop("G20 inches unsupported"); return; }
  if (pl.hasG && pl.g == 91) { hardStop("G91 relative unsupported"); return; }

  // End job: home and keep going
  if (pl.hasM && (pl.m == 2 || pl.m == 30)) {
    penUp();
    if (!doHome()) {
      hardStop("homing failed after M2/M30");
      return;
    }
    resetJobState();
    Serial.println("ok");
    Serial.println("ready");
    return;
  }

  // Modal setup
  if (pl.hasG && pl.g == 21) units_mm = true;
  if (pl.hasG && pl.g == 90) absolute_mode = true;

  // Tool
  if (pl.hasM && pl.m == 3) penDown();
  if (pl.hasM && pl.m == 5) penUp();

  // Feedrate
  if (pl.hasF) setFeedMmMin(pl.f);

  // Motion
  bool isG0 = (pl.hasG && pl.g == 0);
  bool isG1 = (pl.hasG && pl.g == 1);

  if (isG0 || isG1) {
    if (!units_mm) { hardStop("missing G21"); return; }
    if (!absolute_mode) { hardStop("missing G90"); return; }

    if (isG0) {
      penUp();
      if (!pl.hasF && current_feed_mm_min <= 0.0f) setFeedMmMin(FEED_TRAVEL_MM_MIN);
    } else {
      penDown();
      if (!pl.hasF && current_feed_mm_min <= 0.0f) setFeedMmMin(FEED_DRAW_MM_MIN);
    }

    float tx = cur_x_mm;
    float ty = cur_y_mm;
    if (pl.hasX) tx = pl.x;
    if (pl.hasY) ty = pl.y;

    tx = clampf(tx, 0.0f, X_MAX_MM);
    ty = clampf(ty, 0.0f, Y_MAX_MM);

    moveToMm(tx, ty);
  }

  Serial.println("ok");
}

// ---------------- Arduino ----------------
void setup() {
  Serial.begin(115200);

  pinMode(MS1_PIN, OUTPUT);
  pinMode(MS2_PIN, OUTPUT);
  pinMode(MS3_PIN, OUTPUT);

  pinMode(STEP_L, OUTPUT);
  pinMode(DIR_L, OUTPUT);
  pinMode(STEP_R, OUTPUT);
  pinMode(DIR_R, OUTPUT);

  pinMode(LIMIT_X_LEFT, INPUT);
  pinMode(LIMIT_Y_DOWN, INPUT);

  setMicrosteppingFixed();

  penServo.setPeriodHertz(SERVO_HZ);
  penServo.attach(SERVO_PIN, SERVO_MIN_US, SERVO_MAX_US);

  setFeedMmMin(FEED_TRAVEL_MM_MIN);
  penUp();

  if (!doHome()) {
    hardStop("homing failed on boot");
    return;
  }

  resetJobState();

  Serial.println("ok");
  Serial.println("ready");
}

void loop() {
  if (stopped) {
    while (Serial.available()) Serial.read();
    delay(50);
    return;
  }

  if (!Serial.available()) return;

  String line = Serial.readStringUntil('\n');
  line.trim();

  ParsedLine pl;
  if (!parseLine(line, pl)) {
    hardStop("parse error");
    return;
  }

  executeParsed(pl);
}