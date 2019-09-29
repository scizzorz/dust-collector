#include <Arduino.h>
#include <Servo.h>

#define NUM_GATES 6
#define MOVETIME 2000
#define OPEN true
#define CLOSED false
#define NEUTRAL 90

Servo gates[NUM_GATES];
int servoPins[NUM_GATES] = {11, 10, 9, 6, 5, 3};
int buttonPins[NUM_GATES] = {13, 12, 8, 7, 4, 2};
int openPosition[NUM_GATES] = {0, 0, 0, 0, 0, 0};
int closePosition[NUM_GATES] = {180, 180, 180, 180, 180, 180};
bool status[NUM_GATES] = {CLOSED, CLOSED, CLOSED, CLOSED, CLOSED, CLOSED};

void setGate(int i, bool open) {
  if(open != status[i]) {
    if(open) {
      gates[i].write(openPosition[i]);
    } else {
      gates[i].write(closePosition[i]);
    }
    status[i] = open;
    delay(MOVETIME);
  }
}

int findLimit(int i, bool open) {
  int adjust;
  if(open) {
    adjust = -1; // lower degrees -> more open
  } else {
    adjust = 1; // higher degrees -> more closed
  }

  int target = NEUTRAL;
  gates[i].write(target);
  delay(1000);

  while(true) {
    target += adjust;
    gates[i].write(target);
    delay(150);
    if(target == 0 || target == 180) {
      return target;
    }
    if(gates[i].read() != target) {
      return target - adjust;
    }
  }

  return NEUTRAL;
}

void setup() {
  for (int i = 0; i < NUM_GATES; i++) {
    gates[i].attach(servoPins[i]);
    pinMode(buttonPins[i], INPUT);
  }

  for (int i = 0; i < 1; i++) { // FIXME
    openPosition[i] = findLimit(i, OPEN);
    closePosition[i] = findLimit(i, CLOSED);
    setGate(i, OPEN);
    setGate(i, CLOSED);
  }
}

void loop() {
  for (int i = 0; i < NUM_GATES; i++) {
    int pressed = digitalRead(buttonPins[i]);
    if (pressed) {
      gates[i].write(OPEN);
      delay(MOVETIME);
      anyOpen = true;
      if (anyOpen && openGate != i) {
        gates[openGate].write(CLOSED);
        delay(MOVETIME);
      }
      openGate = i;
    }
  }
  delay(10);
}

int main(void) {
  init();
  setup();
  for (;;) {
    loop();
  }
}
