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

int collectorOn = A4;
int collectorOff = A5;

class Collector {
public:
  int onPin;
  int offPin;
  bool status;

  Collector(int onPin, int offPin)
      : onPin(onPin), offPin(offPin), status(OFF) {}

  void init() {
    pinMode(this->onPin, OUTPUT);
    pinMode(this->offPin, OUTPUT);
  }

  void on() { this->set(ON); }

  void off() { this->set(OFF); }

  void set(bool to) {
    int pin = to ? this->onPin : this->offPin;
    digitalWrite(pin, HIGH);
    delay(COLLECTOR_WAIT);
    digitalWrite(pin, LOW);
  }
};

class Gate {
public:
  int buttonPin;
  int servoPin;
  int openPosition;
  int closePosition;
  bool status;
  Servo servo;

  Gate(int buttonPin, int servoPin, int openPosition, int closePosition)
      : buttonPin(buttonPin), servoPin(servoPin), openPosition(openPosition),
        closePosition(closePosition), status(CLOSED) {}

  void init() {
    pinMode(this->buttonPin, INPUT);
    this->servo.attach(this->servoPin);
    this->servo.write(this->closePosition);
  }

  void open() { this->set(true); }

  void close() { this->set(false); }

  void set(bool open) {
    if (open != this->status) {
      this->servo.write(open ? this->openPosition : this->closePosition);
      this->status = open;
      delay(MOVETIME);
    }
  }

  bool isPressed() { return digitalRead(this->buttonPin); }
};

Gate gates[NUM_GATES] = {
    Gate(13, 11, 40, 105), Gate(12, 10, 45, 120), Gate(8, 9, 80, 100),
    Gate(7, 6, 80, 100),   Gate(4, 5, 80, 100),   Gate(2, 3, 80, 100),
};

Collector collector(A4, A5);

void setup() {
  collector.init();
  for (int i = 0; i < NUM_GATES; i++) {
    gates[i].init();
    delay(500);
    gates[i].open();
    gates[i].close();
  }
}

void loop() {
  for (int i = 0; i < NUM_GATES; i++) {
    if (gates[i].isPressed()) {
      gates[i].open();
      for (int j = 0; j < NUM_GATES; j++) {
        if (i != j) {
          gates[j].close();
        }
      }
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
