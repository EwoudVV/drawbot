// Driver-state probe for the drawbot Uno R4 + CNC Shield V3.00.
//
// Purpose: incremental, low-risk verification of what actually drives the
// A4988s, with motor power (24 V) applied. The main firmware's M100/M101
// assume a lot at once; this sketch exposes the individual pin groups
// (ENABLE, MS1/2/3, DIR, STEP) so each polarity question can be answered
// in isolation before any long motion test.
//
// It runs over the same Nordic UART BLE service as the main firmware (so
// it works while 24 V is on and USB is disconnected) AND over USB Serial
// (handy for multimeter level checks with 24 V off). Reply goes to both
// when available. Use tools/ble_test.py to talk to it.
//
// Commands (one per line, '\n' or '\r' terminated):
//   ?            report every pin state and the limit switches
//   E<0|1>       set D8 (driver ENABLE net) level. E0 = LOW = drivers
//                ENABLED on this shield (direct net, active-low, R1
//                pull-up; see the verified note below)
//   M<0..7>      set MS1/MS2/MS3 as bits (bit2=MS1, bit1=MS2, bit0=MS3):
//                M0=000 full, M1=001 half, M2=010 quarter, M4=100 eighth,
//                M7=111 sixteenth
//   D<0|1>       set both DIR pins
//   A<0|1>       set DIR A (D5) only
//   B<0|1>       set DIR B (D6) only
//   S[<n>]       burst n STEP pulses on BOTH motors, 2 ms period (n
//                defaults to 400)
//   SA[<n>]      burst n STEP pulses on motor A (D2) only
//   SB[<n>]      burst n STEP pulses on motor B (D3) only
//   R            reset to boot defaults (ENABLE off, MS 000, DIR LOW)
//   H            help
//
// Boot state is safe: ENABLE HIGH (drivers off), MS 000 (full step),
// DIR LOW, STEP LOW.
//
// Verified hardware facts (Protoneer V3.00 schematic + grbl 0.8 + on-device
// probing): pinMode(OUTPUT) resets the output data register on this core,
// so every state change below is pinMode-first-then-digitalWrite. D8 is the
// A4988 ENABLE net, DIRECT (no inverter), pulled high by R1 (10K): D8 LOW =
// drivers enabled. D12/D13 are the SpnEn/SpnDir headers; there is NO
// D13-to-RESET "R10" rail in the official design.
#include <ArduinoBLE.h>

static constexpr uint8_t PIN_STEP_A = 2;
static constexpr uint8_t PIN_STEP_B = 3;
static constexpr uint8_t PIN_DIR_A = 5;
static constexpr uint8_t PIN_DIR_B = 6;
static constexpr uint8_t PIN_MS1 = 7;
static constexpr uint8_t PIN_ENABLE = 8;
static constexpr uint8_t PIN_HOME_X = 9;
static constexpr uint8_t PIN_HOME_Y = 10;
static constexpr uint8_t PIN_PROBE = 11;
static constexpr uint8_t PIN_MS2 = 12;
static constexpr uint8_t PIN_MS3 = 13;

static constexpr const char* BLE_SERVICE_UUID = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
static constexpr const char* BLE_TX_CHAR_UUID = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E";
static constexpr const char* BLE_RX_CHAR_UUID = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E";

BLEService bleService(BLE_SERVICE_UUID);
BLECharacteristic bleTxChar(BLE_TX_CHAR_UUID, BLERead | BLENotify, 20);
BLECharacteristic bleRxChar(BLE_RX_CHAR_UUID, BLEWrite, 20);

class BlePrint : public Print {
 public:
  size_t write(uint8_t c) override {
    if (pos_ < sizeof(buf_) - 1) buf_[pos_++] = c;
    return 1;
  }
  void flush() {
    uint8_t* p = (uint8_t*)buf_;
    while (pos_ > 20) {
      bleTxChar.writeValue(p, 20);
      p += 20;
      pos_ -= 20;
    }
    if (pos_ > 0) {
      bleTxChar.writeValue(p, pos_);
      pos_ = 0;
    }
  }

 private:
  char buf_[128];
  size_t pos_ = 0;
};

BlePrint bleOut;

// Reported state (what the firmware INTENDS the pins to be).
int enable_level_ = HIGH;  // boot: ENABLE net high = drivers off
int ms1_ = LOW;
int ms2_ = LOW;
int ms3_ = LOW;
int dir_a_ = LOW;
int dir_b_ = LOW;

// pinMode(OUTPUT) must come BEFORE digitalWrite on this core: calling it
// after a write resets the output data register to 0 and clobbers the value.
void drive(uint8_t pin, uint8_t value) {
  pinMode(pin, OUTPUT);
  digitalWrite(pin, value);
}

void report(Print& out) {
  out.print(F("probe: ENABLE D8="));
  out.print(enable_level_ == HIGH ? F("1 (OFF)") : F("0 (ENABLED)"));
  out.print(F(" MS1="));
  out.print(ms1_ == HIGH ? 1 : 0);
  out.print(F(" MS2="));
  out.print(ms2_ == HIGH ? 1 : 0);
  out.print(F(" MS3="));
  out.print(ms3_ == HIGH ? 1 : 0);
  const int ms_bits = (ms1_ == HIGH ? 4 : 0) | (ms2_ == HIGH ? 2 : 0) |
                      (ms3_ == HIGH ? 1 : 0);
  out.print(F(" (mode bits "));
  out.print(ms_bits);
  out.print(F(") DIR A="));
  out.print(dir_a_ == HIGH ? 1 : 0);
  out.print(F(" B="));
  out.print(dir_b_ == HIGH ? 1 : 0);
  out.print(F(" | x_home:"));
  out.print(digitalRead(PIN_HOME_X) == LOW ? F("TRIGGERED") : F("open"));
  out.print(F(" y_home:"));
  out.print(digitalRead(PIN_HOME_Y) == LOW ? F("TRIGGERED") : F("open"));
  out.print(F(" probe:"));
  out.println(digitalRead(PIN_PROBE) == LOW ? F("TRIGGERED") : F("open"));
}

void help(Print& out) {
  out.println(F("commands: ? report | E0/E1 enable level (E0=ON) | M0..M7 "
                "MS bits (M7=1/16) | D0/D1 both DIR | A0/A1 DIR A | B0/B1 "
                "DIR B | S[n] step both | SA[n] step A | SB[n] step B | R "
                "reset | H help"));
}

void setEnable(int level) {
  enable_level_ = level ? HIGH : LOW;
  drive(PIN_ENABLE, enable_level_);
}

void setMsBits(int bits) {
  ms1_ = (bits & 4) ? HIGH : LOW;
  ms2_ = (bits & 2) ? HIGH : LOW;
  ms3_ = (bits & 1) ? HIGH : LOW;
  drive(PIN_MS1, ms1_);
  drive(PIN_MS2, ms2_);
  drive(PIN_MS3, ms3_);
}

void setDirA(int level) {
  dir_a_ = level ? HIGH : LOW;
  drive(PIN_DIR_A, dir_a_);
}

void setDirB(int level) {
  dir_b_ = level ? HIGH : LOW;
  drive(PIN_DIR_B, dir_b_);
}

void resetDefaults() {
  setEnable(HIGH);  // drivers off
  setMsBits(0);     // full step
  setDirA(LOW);
  setDirB(LOW);
  digitalWrite(PIN_STEP_A, LOW);
  digitalWrite(PIN_STEP_B, LOW);
}

void burst(bool step_a, bool step_b, long count) {
  for (long i = 0; i < count; i++) {
    if (step_a) digitalWrite(PIN_STEP_A, HIGH);
    if (step_b) digitalWrite(PIN_STEP_B, HIGH);
    delayMicroseconds(50);
    if (step_a) digitalWrite(PIN_STEP_A, LOW);
    if (step_b) digitalWrite(PIN_STEP_B, LOW);
    delayMicroseconds(1950);
  }
}

void handleLine(const char* line, Print& out) {
  char letter = line[0];
  char second = line[1];
  long number = 0;
  bool has_number = false;
  for (int i = (second >= '0' && second <= '9') ? 1 : 2; line[i]; i++) {
    if (line[i] >= '0' && line[i] <= '9') {
      number = number * 10 + (line[i] - '0');
      has_number = true;
    }
  }

  switch (letter) {
    case '?':
      report(out);
      break;
    case 'H':
      help(out);
      break;
    case 'E':
      setEnable(has_number ? (number != 0) : 1);
      report(out);
      break;
    case 'M':
      if (has_number && number >= 0 && number <= 7) {
        setMsBits((int)number);
      }
      report(out);
      break;
    case 'D':
      setDirA(has_number ? (number != 0) : 1);
      setDirB(has_number ? (number != 0) : 1);
      report(out);
      break;
    case 'A':
      setDirA(has_number ? (number != 0) : 1);
      report(out);
      break;
    case 'B':
      setDirB(has_number ? (number != 0) : 1);
      report(out);
      break;
    case 'R':
      resetDefaults();
      report(out);
      break;
    case 'S': {
      if (!has_number) number = 400;
      if (number < 0) number = 0;
      if (number > 200000) number = 200000;
      out.print(F("burst "));
      out.print(number);
      out.println(second == 'A' ? F(" on motor A")
                                : second == 'B' ? F(" on motor B")
                                                : F(" on both motors"));
      burst(second != 'B', second != 'A', number);
      out.println(F("burst done"));
      report(out);
      break;
    }
    default:
      out.print(F("unknown command '"));
      out.print(line);
      out.println(F("'; H for help"));
      break;
  }
}

class LineAccumulator {
 public:
  void feed(char c, Print& out) {
    if (c == '\n' || c == '\r') {
      if (len_ > 0) {
        buf_[len_] = '\0';
        handleLine(buf_, out);
        len_ = 0;
      }
      return;
    }
    if (len_ < sizeof(buf_) - 1) buf_[len_++] = c;
  }

 private:
  char buf_[40];
  size_t len_ = 0;
};

LineAccumulator ble_lines;
LineAccumulator serial_lines;

void setup() {
  Serial.begin(115200);
  pinMode(PIN_STEP_A, OUTPUT);
  pinMode(PIN_STEP_B, OUTPUT);
  pinMode(PIN_HOME_X, INPUT_PULLUP);
  pinMode(PIN_HOME_Y, INPUT_PULLUP);
  pinMode(PIN_PROBE, INPUT_PULLUP);
  resetDefaults();

  BLE.setLocalName("Drawbot Probe");
  BLE.setAdvertisedService(bleService);
  bleService.addCharacteristic(bleTxChar);
  bleService.addCharacteristic(bleRxChar);
  BLE.addService(bleService);
  if (!BLE.begin()) {
    Serial.println(F("ble: init failed (USB-only mode)"));
  } else {
    BLE.advertise();
    Serial.println(F("ble: advertising"));
  }

  Serial.println(F("=== driver state probe (drawbot) ==="));
  Serial.println(F("24 V may be ON. Boot state: drivers OFF (ENABLE high)."));
  Serial.println(F("E0 enables the drivers; E1 disables. H for help."));
  report(Serial);
}

void loop() {
  BLE.poll();
  const bool ble_connected = BLE.connected();

  if (Serial.available() > 0) {
    while (Serial.available() > 0) {
      serial_lines.feed((char)Serial.read(), Serial);
    }
  }

  if (bleRxChar.written()) {
    Print& out = ble_connected ? static_cast<Print&>(bleOut)
                               : static_cast<Print&>(Serial);
    int len = bleRxChar.valueLength();
    const uint8_t* buf = bleRxChar.value();
    for (int i = 0; i < len; i++) {
      ble_lines.feed((char)buf[i], out);
    }
    if (ble_connected) bleOut.flush();
  }
}
