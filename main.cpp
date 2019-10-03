#include <Arduino.h>
#include <Servo.h>

#define NUM_GATES 6
#define MOVETIME 750
#define NEUTRAL 90
#define OPEN true
#define CLOSED false
#define ON true
#define OFF false
#define COLLECTOR_WAIT 300
#define DEBOUNCE 50

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
    this->servo.attach(this->servoPin);
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
    Gate(13, 11, 40, 105), Gate(12, 10, 45, 120), Gate(8, 9, 80, 100),
    Gate(7, 6, 80, 100),   Gate(4, 5, 80, 100),   Gate(2, 3, 80, 100),
};

void press(int i) {
  // if this gate is already open, just toggle the dust collector.
  if (gates[i].isOpen()) {
    collector.toggle();
    return;
  }

  // if the gate isn't already open, we need to open it.
  // do this before turning on the dust collector to avoid having no open gates
  // with the collector on.
  gates[i].open();

  // turn on the collector if we need to
  if (!collector.isOn()) {
    collector.on();
  }

  // close all other gates. do this after opening the target gate to avoid
  // having no open gates with the collector on.
  for (int j = 0; j < NUM_GATES; j++) {
    if (i != j) {
      gates[j].close();
    }
  }
}

void setup() {
  collector.init();
  for (int i = 0; i < NUM_GATES; i++) {
    gates[i].init();
    /*
    delay(500);
    gates[i].open();
    gates[i].close();
    */
  }
}

void loop() {
  for (int i = 0; i < NUM_GATES; i++) {
    if (gates[i].checkPress() && gates[i].isPressed()) {
      press(i);
    }
  }

  delay(10);
}

int main(void) {
  init();
  setup();
  while (true) {
    loop();
  }
}
