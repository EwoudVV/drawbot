// Pin-drive probe for Uno R4: does the handoff's "digitalWrite-then-pinMode"
// recipe actually drive a pin HIGH? Or does pinMode(OUTPUT) clobber the
// output data register back to LOW?
#include <Arduino.h>

// DIR pins used by M100/M101 plus STEP pins for comparison.
const uint8_t kPins[] = {2, 3, 5, 6, 7, 12, 13};
const int kNumPins = sizeof(kPins) / sizeof(kPins[0]);

void report(const char* label, int pin, bool high) {
  int level = digitalRead(pin);
  Serial.print(label);
  Serial.print(" pin ");
  Serial.print(pin);
  Serial.print(" -> readback ");
  Serial.print(level == HIGH ? "HIGH" : "LOW");
  Serial.print("  expected ");
  Serial.println(high ? "HIGH" : "LOW");
}

// Sequence 1: digitalWrite(val) THEN pinMode(OUTPUT)  (the handoff recipe)
void sequenceRecipe(uint8_t pin, bool high) {
  digitalWrite(pin, high ? HIGH : LOW);
  pinMode(pin, OUTPUT);
}

// Sequence 2: pinMode(OUTPUT) THEN digitalWrite(val)
void sequenceReverse(uint8_t pin, bool high) {
  pinMode(pin, OUTPUT);
  digitalWrite(pin, high ? HIGH : LOW);
}

// Sequence 3: plain digitalWrite on an already-OUTPUT pin
void sequencePlain(uint8_t pin, bool high) {
  pinMode(pin, OUTPUT);
  delayMicroseconds(10);
  digitalWrite(pin, high ? HIGH : LOW);
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {}
  delay(300);
}

void loop() {
  Serial.println("=== pin drive probe ===");
  for (int i = 0; i < kNumPins; i++) {
    uint8_t pin = kPins[i];
    // Reset pin to input first so every sequence starts clean.
    pinMode(pin, INPUT);
    delayMicroseconds(20);

    sequenceRecipe(pin, true);
    report("recipe dw+pm HIGH", pin, true);
    pinMode(pin, INPUT);
    delayMicroseconds(20);

    sequenceReverse(pin, true);
    report("reverse pm+dw HIGH", pin, true);
    pinMode(pin, INPUT);
    delayMicroseconds(20);

    sequencePlain(pin, true);
    report("plain dw HIGH", pin, true);
    pinMode(pin, INPUT);
    delayMicroseconds(20);

    // The critical one: flip an already-driven pin, recipe style, twice.
    sequenceRecipe(pin, true);
    report("recipe set HIGH", pin, true);
    sequenceRecipe(pin, false);
    report("recipe flip LOW", pin, false);
    sequenceRecipe(pin, true);
    report("recipe flip HIGH again", pin, true);
    pinMode(pin, INPUT);
    delayMicroseconds(20);

    // Same flip via plain digitalWrite.
    sequencePlain(pin, true);
    report("plain set HIGH", pin, true);
    sequencePlain(pin, false);
    report("plain flip LOW", pin, false);
    sequencePlain(pin, true);
    report("plain flip HIGH again", pin, true);
    pinMode(pin, INPUT);
  }
  Serial.println("=== done ===");
  delay(1000);
}
