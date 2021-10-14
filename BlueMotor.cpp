#include <RBE1001Lib.h>
#include "BlueMotor.h"

Timer armTimer(10);

int encoderTable[4][4] = {
  {0,-1,1,0,},
  {1,0,0,-1,},
  {-1,0,0,1,},
  {0,1,-1,0},
};
int oldVal = 0;
int newVal = 0;
volatile int out = 0;
float errorTotal = 0;
float lastError = 0; 
int newPositionArm=0;
int oldPositionArm=0;
int rpm=0;

long count = 0;  // encoder counter
// Mutex for the count critical variable
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

/**
 * Interrupt service routine (ISR) for one of the encoder
 * channels. It simply counts how many interrupts occured
 */
void IRAM_ATTR isr() {
  portENTER_CRITICAL_ISR(&mux);
  oldVal = newVal;
  newVal = (digitalRead(18) << 1) | digitalRead(19);

  out = encoderTable[oldVal][newVal];
  count -= out;
  portEXIT_CRITICAL_ISR(&mux);
}

/**
 * Set up all the variables for the blue motor
 * This is not the same as the ESP32 setup() function, although
 * that would be a good place to call this setup function
 */
void BlueMotor::setup() {
  pinMode(ENCA, INPUT);
  pinMode(ENCB, INPUT);
  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);
  digitalWrite(AIN1, LOW);
  digitalWrite(AIN2, HIGH);
  pinMode(PWM, OUTPUT);
  attachInterrupt(ENCA, isr, CHANGE);
  attachInterrupt(ENCB, isr, CHANGE);
}

/**
 * Get the current encoder count
 * Returns the encoder count value
 */
long BlueMotor::getPosition() {
  long value;
  portENTER_CRITICAL(&mux);
  value = count;
  portEXIT_CRITICAL(&mux);
  return value;
}

/**
 * Reset the encoder count to zero
 */
void BlueMotor::reset() {
  portENTER_CRITICAL(&mux);
  count = 0;
  portEXIT_CRITICAL(&mux);
}

/**
 * Set the motor effort
 * effort value ranges from -255 to +255
 * Negative values turn one way, positive values turn the other way
 */
void BlueMotor::setEffort(int effort) {
  if (effort < 0) {
    setEffort(-effort, true);
  } else {
    setEffort(effort, false);
  }
}

/**
 * Set the motor effort
 * effort values range from 0-255
 * clockwise is true for one direction, false for the other
 */
void BlueMotor::setEffort(int effort, bool clockwise) {
  if (clockwise) {
    digitalWrite(AIN1, HIGH);
    digitalWrite(AIN2, LOW);
  } else {
    digitalWrite(AIN1, LOW);
    digitalWrite(AIN2, HIGH);
  }
  int value = constrain(effort, 0, 255);
  analogWrite(PWM, value);
}

float BlueMotor::setEffortWihtoutDB(int effort){
  float motorEffort = 0.0;
  float effortG = effort;//set effort as float
  if (effortG < 0){
    motorEffort = ((effortG/255)*(255-107))-107;//down calc
    setEffort(-motorEffort, true);
  }
  else{
    motorEffort = ((effortG/255)*(255-120))+120;//up calc
    setEffort(motorEffort, false);
  }
  return motorEffort;//return for print
}

void BlueMotor::moveTo(int pos){
  int readVal = getPosition();
  float error = pos-readVal;
  while(error >= 25 || error <= -25){
      newPositionArm = getPosition();//new postion
      readVal = getPosition();
      error = pos-readVal;
      readVal = getPosition();//get current position
      error = pos-readVal;//get error of arm
      errorTotal += error;//tally total
      if ((error >= 25 || error <= -25)) {
        setEffortWihtoutDB((int) (error*kpA + errorTotal*kiA + (error - lastError) * kdA));//set effort
        lastError = error;//last error is current error
      }
      oldPositionArm  = newPositionArm;//record current posision
  }
}
