#include <Arduino.h>
#include <Servo.h>

#define NUM_GATES 6
#define CLOSED 180
#define OPEN 115
#define MOVETIME 1000

Servo gates[NUM_GATES];
int servoPins[NUM_GATES] = {11, 10, 9, 6, 5, 3};
int buttonPins[NUM_GATES] = {13, 12, 8, 7, 4, 2};
int openGate = 0;
int open = false;

void setup() {
  for(int i=0; i<NUM_GATES; i++) {
    gates[i].attach(servoPins[i]);
    gates[i].write(OPEN);
    delay(MOVETIME);
    gates[i].write(CLOSED);
    delay(MOVETIME);
    pinMode(buttonPins[i], INPUT);
  }
}


void loop() {
  for(int i=0; i<NUM_GATES; i++) {
    int pressed = digitalRead(buttonPins[i]);
    if(pressed) {
      gates[i].write(OPEN);
      delay(MOVETIME);
      open = true;
      if(open && openGate != i) {
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
