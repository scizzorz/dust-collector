#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <EEPROM.h>
#include <SPI.h>
#include <Servo.h>
#include <Wire.h>

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
  int pin;
  int openPosition;
  int closePosition;
  Servo servo;
  bool status = CLOSED;

public:
  Gate(int pin) : pin(pin), openPosition(NEUTRAL), closePosition(NEUTRAL) {}

  void init(int openPosition, int closePosition) {
    this->setOpenPosition(openPosition);
    this->setClosePosition(closePosition);

    this->servo.attach(this->pin, 400, 2500);
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

  bool isOpen() { return this->status; }
};

class Button {
private:
  int pin;
  unsigned long lastDebounce;
  int buttonState;
  int lastButtonState;

public:
  Button(int pin)
      : pin(pin), lastDebounce(0), buttonState(LOW), lastButtonState(HIGH) {}

  void init() { pinMode(this->pin, INPUT); }

  /// this does *not* do any debouncing!
  bool read() { return digitalRead(this->pin); }

  /// This returns true if the button state CHANGED. It does *not* return the
  /// button state! If you want to react to presses, use
  /// `.checkPress() && .isPressed()`
  bool checkPress() {
    bool change = false;
    int currentReading = this->read();
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
};

// pins 2 + 3
Adafruit_SSD1306 display(128, 64, &Wire, -1);

Knob knob(A2);
Beeper beeper(A3);
Collector collector(A4, A5);
Gate gates[NUM_GATES] = {
    Gate(5), Gate(6), Gate(9), Gate(10), Gate(11), Gate(12),
};
Button butts[NUM_GATES] = {
    Button(4), Button(7), Button(8), Button(13), Button(A0), Button(A1),
};

int programmingGate = -1;
int programmingState = STATE_NORMAL;

void prepStatus() {
  display.setCursor(0, 8);
  display.println("collector:");

  for (int i = 0; i < NUM_GATES; i++) {
    display.setCursor(0, 16 + 8 * i);
    display.print("gate ");
    display.print(i);
    display.print(": ");
  }
}

void dispCollector(String x) {
  display.setCursor(66, 8);
  display.print(x);
}

void dispCollectorCurrent() { dispCollector(collector.isOn() ? "on" : "off"); }

void dispGate(int i, String x) {
  display.setCursor(48, 16 + 8 * i);
  display.print(x);
}

void dispGatesCurrentExcept(int j, String x) {
  for (int i = 0; i < NUM_GATES; i++) {
    if (i == j) {
      dispGate(i, x);
    } else {
      dispGate(i, gates[i].isOpen() ? "open" : "closed");
    }
  }
}

void dispGatesCurrent() {
  for (int i = 0; i < NUM_GATES; i++) {
    dispGate(i, gates[i].isOpen() ? "open" : "closed");
  }
}

/// what happens when a button is pressed in normal state
void pressNormal(int i) {
  beeper.beep(200);

  // if this gate is already open, just toggle the dust collector.
  if (gates[i].isOpen()) {
    collector.toggle();

    display.clearDisplay();
    prepStatus();
    dispCollectorCurrent();
    dispGatesCurrent();
    display.display();

    return;
  }

  // if the gate isn't already open, we need to open it.
  // do this before turning on the dust collector to avoid having no open gates
  // with the collector on.
  display.clearDisplay();
  prepStatus();
  dispCollectorCurrent();
  dispGatesCurrentExcept(i, "opening");
  display.display();

  gates[i].open();

  // turn on the collector if we need to
  if (!collector.isOn()) {
    collector.on();
  }

  display.clearDisplay();
  prepStatus();
  dispCollectorCurrent();
  dispGatesCurrent();
  display.display();

  // close all other gates. do this after opening the target gate to avoid
  // having no open gates with the collector on.
  for (int j = 0; j < NUM_GATES; j++) {
    if (i != j && gates[j].isOpen()) {
      display.clearDisplay();
      prepStatus();
      dispCollectorCurrent();
      dispGatesCurrentExcept(j, "closing");
      display.display();

      gates[j].close();
    }
  }

  display.clearDisplay();
  prepStatus();
  dispCollectorCurrent();
  dispGatesCurrent();
  display.display();

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

    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("programming mode");
    display.print("open gate ");
    display.println(i);
    display.display();

    return;
  }

  if (programmingState == STATE_OPEN) {
    // save open position
    EEPROM[OPEN_ADDR(programmingGate)] = knobToMem(knob.read());

    // move to next state
    programmingState = STATE_CLOSED;

    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("programming mode");
    display.print("close gate ");
    display.println(i);
    display.display();

    // beep one more time
    delay(100);
    beeper.beep(200);

    return;
  }

  if (programmingState == STATE_CLOSED) {
    // save close position
    EEPROM[CLOSE_ADDR(i)] = knobToMem(knob.read());

    // move to next state
    programmingState = STATE_PROGRAMMING;
    programmingGate = -1;

    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("programming mode");
    display.display();

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
    return;
  }

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
  initMemory();

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.display();
  delay(1000);

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println("booting...");
  display.display();

  beeper.init();
  knob.init();
  collector.init();

  // beep to indicate boot start (a lil late into the routine, but... whatever)
  beeper.beep(200);

  prepStatus();
  dispCollector("ready");
  display.display();

  // initialize gates and set the appropriate indicator as we go
  for (int i = 0; i < NUM_GATES; i++) {
    dispGate(i, "ready");
    display.display();

    butts[i].init();
    gates[i].init(memToPosition(EEPROM[OPEN_ADDR(i)]),
                  memToPosition(EEPROM[CLOSE_ADDR(i)]));

    // delay to ensure we don't turn on all servos at once
    delay(500);
  }

  // check for a held button to enter programming mode
  for (int i = 0; i < NUM_GATES; i++) {
    if (butts[i].read()) {
      programmingState = STATE_PROGRAMMING;
    }
  }

  // beep twice to indicate boot complete
  beeper.beep(200);
  delay(100);
  beeper.beep(200);

  display.clearDisplay();
  prepStatus();
  dispCollectorCurrent();
  dispGatesCurrent();
  display.display();

  // beep a third time to indicate programming mode
  if (programmingState == STATE_PROGRAMMING) {
    display.clearDisplay();
    display.setCursor(0, 0);
    display.print("programming mode");
    display.display();

    beeper.beep(200);
    delay(100);
  }
}

void loop() {
  // check for button presses
  for (int i = 0; i < NUM_GATES; i++) {
    if (butts[i].checkPress() && butts[i].isPressed()) {
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
