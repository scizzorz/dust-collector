#include <Arduino.h>
#include <Servo.h>

#define NUM_GATES 6
#define MOVETIME 750
#define NEUTRAL 90
#define OPEN true
#define CLOSED false

Servo gates[NUM_GATES];
int servoPins[NUM_GATES] = {11, 10, 9, 6, 5, 3};
int buttonPins[NUM_GATES] = {13, 12, 8, 7, 4, 2};
int openPosition[NUM_GATES] = {40, 45, 80, 80, 80, 80};
int closePosition[NUM_GATES] = {115, 120, 100, 100, 100, 100};
bool status[NUM_GATES] = {CLOSED, CLOSED, CLOSED, CLOSED, CLOSED, CLOSED};

void setGate(int i, bool open) {
  if (open != status[i]) {
    gates[i].write(open ? openPosition[i] : closePosition[i]);
    status[i] = open;
    delay(MOVETIME);
  }
}

void setup() {
  for (int i = 0; i < NUM_GATES; i++) {
    pinMode(buttonPins[i], INPUT);
    gates[i].attach(servoPins[i]);
    gates[i].write(closePosition[i]);
    delay(500);
    setGate(i, OPEN);
    setGate(i, CLOSED);
  }
}

void loop() {
  for (int i = 0; i < NUM_GATES; i++) {
    int pressed = digitalRead(buttonPins[i]);
    if (pressed) {
      setGate(i, OPEN);
      for (int j = 0; j < NUM_GATES; j++) {
        if (i != j) {
          setGate(j, CLOSED);
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
