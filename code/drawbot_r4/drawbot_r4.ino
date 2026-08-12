#include "command_processor.h"
#include "config.h"
#include "gcode_parser.h"
#include "motion_controller.h"
#include "pen_controller.h"
#include "serial_line_reader.h"

#include <ArduinoBLE.h>

drawbot::MotionController motion;
drawbot::PenController pen;
drawbot::CommandProcessor commands(motion, pen);
drawbot::SerialLineReader line_reader;

// Nordic UART Service (NUS) — the standard BLE serial profile.
// macOS pairs and creates a virtual serial port automatically.
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

void setup() {
  Serial.begin(drawbot::config::SERIAL_BAUD);
  const uint32_t wait_started = millis();
  while (!Serial && millis() - wait_started < 5000U) {
  }

  motion.begin();
  motion.setIdleCallback([] { BLE.poll(); });
  pen.begin();

  if (!BLE.begin()) {
    Serial.println(F("ble: init failed"));
  } else {
    BLE.setLocalName("Drawbot");
    BLE.setAdvertisedService(bleService);
    bleService.addCharacteristic(bleTxChar);
    bleService.addCharacteristic(bleRxChar);
    BLE.addService(bleService);
    BLE.advertise();
  }

  Serial.println(F("ready"));
}

static drawbot::LineReadStatus pollBle() {
  if (!bleRxChar.written()) return drawbot::LineReadStatus::kNone;
  int len = bleRxChar.valueLength();
  const uint8_t* buf = bleRxChar.value();
  for (int i = 0; i < len; i++) {
    drawbot::LineReadStatus s = line_reader.feed((char)buf[i]);
    if (s != drawbot::LineReadStatus::kNone) return s;
  }
  return drawbot::LineReadStatus::kNone;
}

void loop() {
  BLE.poll();
  bool ble_connected = BLE.connected();

  drawbot::LineReadStatus line_status;
  if (Serial.available() > 0) {
    line_status = line_reader.poll(Serial);
  } else {
    line_status = pollBle();
  }
  if (line_status == drawbot::LineReadStatus::kNone) return;
  if (line_status == drawbot::LineReadStatus::kOverflow) {
    pen.forceSafeUp();
    Serial.println(F("error: line is too long"));
    if (ble_connected) { bleOut.print("error: line is too long\n"); bleOut.flush(); }
    return;
  }

  drawbot::GcodeCommand command;
  const drawbot::ParseStatus parse_status =
      drawbot::parseGcodeLine(line_reader.line(), command);
  if (parse_status == drawbot::ParseStatus::kEmpty) return;
  if (parse_status != drawbot::ParseStatus::kOk) {
    pen.forceSafeUp();
    Serial.print(F("error: "));
    Serial.println(drawbot::parseStatusMessage(parse_status));
    if (ble_connected) { bleOut.print("error: "); bleOut.println(drawbot::parseStatusMessage(parse_status)); bleOut.flush(); }
    return;
  }

  char error[112];
  if (ble_connected) {
    bleOut.flush();
    if (commands.execute(command, bleOut, error, sizeof(error))) {
      bleOut.println("ok");
    } else {
      pen.forceSafeUp();
      bleOut.print("error: ");
      bleOut.println(error);
    }
    bleOut.flush();
  } else {
    if (commands.execute(command, Serial, error, sizeof(error))) {
      Serial.println(F("ok"));
    } else {
      pen.forceSafeUp();
      Serial.print(F("error: "));
      Serial.println(error);
    }
  }
}
