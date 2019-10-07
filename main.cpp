#include <Adafruit_NeoPixel.h>
#include <Arduino.h>
#include <Servo.h>

#define NUM_PIX 12
#define NUM_GATES 6
#define MOVETIME 750
#define NEUTRAL 90
#define OPEN true
#define CLOSED false
#define ON true
#define OFF false
#define COLLECTOR_WAIT 500
#define DEBOUNCE 30

#define COLOR_CLOSED 0x220000
#define COLOR_CLOSING 0x221100
#define COLOR_NONE 0x000000
#define COLOR_OFF 0x220000
#define COLOR_ON 0x002200
#define COLOR_OPEN 0x002200
#define COLOR_OPENING 0x112200
#define COLOR_READY 0x222200

class Beeper {
private:
  int pin;

public:
  Beeper(int pin) : pin(pin) {}

  void init() { pinMode(this->pin, OUTPUT); }

  void beep(int len) {
    digitalWrite(this->pin, HIGH);
    delay(len);
    digitalWrite(this->pin, LOW);
  }
};

class Collector {
private:
  int onPin;
  int offPin;
  bool status;

public:
  Collector(int onPin, int offPin)
      : onPin(onPin), offPin(offPin), status(OFF) {}

  void init() {
    pinMode(this->onPin, OUTPUT);
    pinMode(this->offPin, OUTPUT);
  }

  void on() { this->set(ON); }

  void off() { this->set(OFF); }

  void toggle() { this->set(!this->status); }

  void set(bool to) {
    int pin = to ? this->onPin : this->offPin;
    digitalWrite(pin, HIGH);
    delay(COLLECTOR_WAIT);
    digitalWrite(pin, LOW);
    this->status = to;
  }

  bool isOn() { return this->status; }
};

class Gate {
private:
  unsigned long lastDebounce = 0;
  int lastButtonState = HIGH;
  int buttonPin;
  int servoPin;
  int openPosition;
  int closePosition;
  Servo servo;
  bool status = CLOSED;
  int buttonState = LOW;

public:
  Gate(int buttonPin, int servoPin, int openPosition, int closePosition)
      : buttonPin(buttonPin), servoPin(servoPin), openPosition(openPosition),
        closePosition(closePosition) {}

  void init() {
    pinMode(this->buttonPin, INPUT);
    this->servo.attach(this->servoPin, 400, 2500);
    this->servo.write(this->closePosition);
  }

  void open() { this->set(OPEN); }

  void close() { this->set(CLOSED); }

  void toggle() { this->set(!this->status); }

  void set(bool open) {
    if (open != this->status) {
      this->servo.write(open ? this->openPosition : this->closePosition);
      this->status = open;
      delay(MOVETIME);
    }
  }

  /// This returns true if the button state CHANGED. It does *not* return the
  /// button state! If you want to react to presses, use
  /// `.checkPress() && .isPressed()`
  bool checkPress() {
    bool change = false;
    int currentReading = digitalRead(this->buttonPin);
    if (currentReading != this->lastButtonState) {
      this->lastDebounce = millis();
    }

    if ((millis() - this->lastDebounce) > DEBOUNCE) {
      if (currentReading != this->buttonState) {
        this->buttonState = currentReading;
        change = true;
      }
    }

    this->lastButtonState = currentReading;
    return change;
  }

  bool isPressed() { return this->buttonState; }

  bool isOpen() { return this->status; }
};

Collector collector(A4, A5);
Gate gates[NUM_GATES] = {
    Gate(13, 11, 62, 100), Gate(12, 10, 35, 102), Gate(8, 9, 49, 125),
    Gate(7, 6, 66, 102),   Gate(4, 5, 65, 104),   Gate(2, 3, 65, 100),
};
Adafruit_NeoPixel strip(12, A3, NEO_GRB + NEO_KHZ800);
Beeper beeper(A2);

void press(int i) {
  beeper.beep(200);

  // if this gate is already open, just toggle the dust collector.
  if (gates[i].isOpen()) {
    collector.toggle();
    strip.setPixelColor(NUM_GATES + 1, collector.isOn() ? COLOR_ON : COLOR_OFF);
    strip.show();
    return;
  }

  // if the gate isn't already open, we need to open it.
  // do this before turning on the dust collector to avoid having no open gates
  // with the collector on.
  strip.setPixelColor(i, COLOR_OPENING);
  strip.show();

  gates[i].open();

  strip.setPixelColor(i, COLOR_OPEN);
  strip.show();

  // turn on the collector if we need to
  if (!collector.isOn()) {
    collector.on();
  }

  strip.setPixelColor(NUM_GATES + 1, collector.isOn() ? COLOR_ON : COLOR_OFF);
  strip.show();

  // close all other gates. do this after opening the target gate to avoid
  // having no open gates with the collector on.
  for (int j = 0; j < NUM_GATES; j++) {
    if (i != j) {
      strip.setPixelColor(j, gates[j].isOpen() ? COLOR_CLOSING : COLOR_CLOSED);
      strip.show();

      gates[j].close();

      strip.setPixelColor(j, COLOR_CLOSED);
      strip.show();
    }
  }

  beeper.beep(200);
  delay(100);
  beeper.beep(200);
}

void setup() {
  strip.begin();
  strip.show();

  beeper.init();
  beeper.beep(200);

  strip.setPixelColor(NUM_GATES + 1, COLOR_READY);
  collector.init();

  for (int i = 0; i < NUM_GATES; i++) {
    strip.setPixelColor(i, COLOR_READY);
    strip.show();
    gates[i].init();
    delay(500);
  }

  beeper.beep(200);
  delay(100);
  beeper.beep(200);
}

void loop() {
  for (int i = 0; i < NUM_GATES; i++) {
    if (gates[i].checkPress() && gates[i].isPressed()) {
      press(i);
    }
  }
}

int main(void) {
  init();
  setup();
  while (true) {
    loop();
  }
}
