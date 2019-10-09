#include <Adafruit_NeoPixel.h>
#include <Arduino.h>
#include <EEPROM.h>
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
#define EEPROM_START 4

#define OPEN_ADDR(x) (EEPROM_START + x * 2)
#define CLOSE_ADDR(x) (EEPROM_START + x * 2 + 1)

#define COLOR_CLOSED 0x220000
#define COLOR_CLOSING 0x221100
#define COLOR_NONE 0x000000
#define COLOR_OFF 0x220000
#define COLOR_ON 0x002200
#define COLOR_OPEN 0x002200
#define COLOR_OPENING 0x112200
#define COLOR_READY 0x222200

#define STATE_NORMAL 0
#define STATE_PROGRAMMING 1
#define STATE_CLOSED 2
#define STATE_OPEN 3

#define MAGIC_0 'S'
#define MAGIC_1 'C'
#define MAGIC_2 'I'
#define MAGIC_3 'Z'

unsigned char knobToMem(int knobReading) { return knobReading / 4; }

int memToPosition(unsigned char mem) { return NEUTRAL + (mem - 128) / 2; }

class Knob {
private:
  int pin;

public:
  Knob(int pin) : pin(pin) {}

  void init() { pinMode(this->pin, INPUT); }

  int read() { return analogRead(pin); }
};

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
  Gate(int buttonPin, int servoPin)
      : buttonPin(buttonPin), servoPin(servoPin), openPosition(NEUTRAL),
        closePosition(NEUTRAL) {}

  void init(int openPosition, int closePosition) {
    this->setOpenPosition(openPosition);
    this->setClosePosition(closePosition);

    pinMode(this->buttonPin, INPUT);
    this->servo.attach(this->servoPin, 400, 2500);
    this->servo.write(this->closePosition);
  }

  void writePosition(int position) { this->servo.write(position); }

  void setOpenPosition(int position) { this->openPosition = position; }

  void setClosePosition(int position) { this->closePosition = position; }

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

  /// this does *not* do any debouncing!
  bool readButton() { return digitalRead(this->buttonPin); }

  /// This returns true if the button state CHANGED. It does *not* return the
  /// button state! If you want to react to presses, use
  /// `.checkPress() && .isPressed()`
  bool checkPress() {
    bool change = false;
    int currentReading = this->readButton();
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
    Gate(13, 11), Gate(12, 10), Gate(8, 9), Gate(7, 6), Gate(4, 5), Gate(2, 3),
};
Adafruit_NeoPixel strip(12, A3, NEO_GRB + NEO_KHZ800);
Beeper beeper(A2);
Knob knob(A0);

int programmingGate = -1;
int programmingState = STATE_NORMAL;

/// what happens when a button is pressed in normal state
void pressNormal(int i) {
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

/// what happens when a button is pressed in programming state
void pressProgramming(int i) {
  beeper.beep(200);
  // if this button wasn't the currently gate we were currently programming,
  // make it the new one we are and restart with the open position.
  if (i != programmingGate) {
    // move to next state
    programmingGate = i;
    programmingState = STATE_OPEN;

    // set the gate's indicator so we know that we're adjusting the open
    // position
    strip.clear();
    strip.setPixelColor(programmingGate, COLOR_OPEN);
    strip.show();

    return;
  }

  if (programmingState == STATE_OPEN) {
    // save open position
    EEPROM[OPEN_ADDR(programmingGate)] = knobToMem(knob.read());

    Serial.print("Saving open position for gate #");
    Serial.print(i, DEC);
    Serial.print(" => ");
    Serial.println(EEPROM[OPEN_ADDR(i)], DEC);

    // move to next state
    programmingState = STATE_CLOSED;

    // set the gate's indicator so we know that we're adjusting the close
    // position
    strip.clear();
    strip.setPixelColor(i, COLOR_CLOSED);
    strip.show();

    // beep one more time
    delay(100);
    beeper.beep(200);

    return;
  }

  if (programmingState == STATE_CLOSED) {
    // save close position
    EEPROM[CLOSE_ADDR(i)] = knobToMem(knob.read());

    Serial.print("Saving close position for gate #");
    Serial.print(i, DEC);
    Serial.print(" => ");
    Serial.println(EEPROM[CLOSE_ADDR(i)], DEC);

    // move to next state
    programmingState = STATE_PROGRAMMING;
    programmingGate = -1;

    // clear pixels to show that it's in idle mode
    strip.clear();
    strip.show();

    // reinitialize the gate (just for fun)
    gates[i].init(memToPosition(EEPROM[OPEN_ADDR(i)]),
                  memToPosition(EEPROM[CLOSE_ADDR(i)]));

    // beep two more times
    delay(100);
    beeper.beep(200);
    delay(100);
    beeper.beep(200);

    return;
  }
}

void initMemory() {
  // check for the magic header at the start of EEPROM. if it's not set, this
  // memory is (very likely) uninitialized and needs to be initialized
  if (EEPROM[0] == MAGIC_0 && EEPROM[1] == MAGIC_1 && EEPROM[2] == MAGIC_2 &&
      EEPROM[3] == MAGIC_3) {
    Serial.println("EEPROM is initialized.");

    for (int i = 0; i < NUM_GATES; i++) {
      Serial.print("gate #");
      Serial.print(i, DEC);
      Serial.print(" open[");
      Serial.print(OPEN_ADDR(i), DEC);
      Serial.print("] = ");
      Serial.print(EEPROM[OPEN_ADDR(i)], DEC);
      Serial.print(", close[");
      Serial.print(CLOSE_ADDR(i), DEC);
      Serial.print("] = ");
      Serial.println(EEPROM[CLOSE_ADDR(i)], DEC);
    }
    return;
  }

  Serial.println("Initializing EEPROM...");

  // set the magic header
  EEPROM[0] = MAGIC_0;
  EEPROM[1] = MAGIC_1;
  EEPROM[2] = MAGIC_2;
  EEPROM[3] = MAGIC_3;

  // just initializing a bunch of spots for some futureproofing.
  for (int i = EEPROM_START; i < 255; i++) {
    EEPROM[i] = 128;
  }
}

void setup() {
  // initialize everything
  Serial.begin(9600);

  initMemory();

  strip.begin();
  strip.show();

  beeper.init();
  knob.init();
  collector.init();

  // beep to indicate boot start (a lil late into the routine, but... whatever)
  beeper.beep(200);

  // set indicator for dust collector
  strip.setPixelColor(NUM_GATES + 1, COLOR_READY);

  // initialize gates and set the appropriate indicator as we go
  for (int i = 0; i < NUM_GATES; i++) {
    strip.setPixelColor(i, COLOR_READY);
    strip.show();
    gates[i].init(memToPosition(EEPROM[OPEN_ADDR(i)]),
                  memToPosition(EEPROM[CLOSE_ADDR(i)]));

    // delay to ensure we don't turn on all servos at once
    delay(500);
  }

  // check for a held button to enter programming mode
  for (int i = 0; i < NUM_GATES; i++) {
    if (gates[i].readButton()) {
      programmingState = STATE_PROGRAMMING;
      Serial.println("Entering programming mode");
    }
  }

  // beep twice to indicate boot complete
  beeper.beep(200);
  delay(100);
  beeper.beep(200);

  // beep a third time to indicate programming mode
  if (programmingState == STATE_PROGRAMMING) {
    strip.clear();
    strip.show();
    beeper.beep(200);
    delay(100);
  }
}

void loop() {
  // check for button presses
  for (int i = 0; i < NUM_GATES; i++) {
    if (gates[i].checkPress() && gates[i].isPressed()) {
      if (programmingState == STATE_NORMAL) {
        pressNormal(i);
      } else {
        pressProgramming(i);
      }
    }
  }

  // if we're adjusting a position, physically show the position
  if (programmingState == STATE_OPEN || programmingState == STATE_CLOSED) {
    gates[programmingGate].writePosition(memToPosition(knobToMem(knob.read())));
  }
}

int main(void) {
  init();
  setup();
  while (true) {
    loop();
  }
}
