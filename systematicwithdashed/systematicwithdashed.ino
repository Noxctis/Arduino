#include <QTRSensors.h>

// ---------------------
// QTR-8A Sensor Setup
// ---------------------
#define NUM_SENSORS 8
uint8_t sensorPins[NUM_SENSORS] = { A0, A1, A2, A3, A4, A5, A6, A7 };
QTRSensors qtrrc;
unsigned int sensorValues[NUM_SENSORS];

// ---------------------
// Motor Driver Pins (TB6612FNG)
// ---------------------
const int STBY      = 10;
const int motorAPWM = 5;
const int motorAIn1 = 3;
const int motorAIn2 = 4;
const int motorBPWM = 11;
const int motorBIn1 = 12;
const int motorBIn2 = 13;

// ---------------------
// PID Controller Params
// ---------------------
float effectiveKp = 0.395;
float Ki           = 0.00;
float Kd           = 1.0;
long  lastError    = 0;
long  integral     = 0;
long  setpoint     = ((NUM_SENSORS - 1) * 1000) / 2;  // equals 3500

// ---------------------
// Speed Settings
// ---------------------
const int defaultSpeed    = 150;
const int searchSpeed     = 100;

// ---------------------
// Pivot (dynamic) Params
// ---------------------
const int   pivotThreshold   = 1500;   // |error| > this → pivot
const float pivotKp          = 0.2;   // error → pivot speed scaling
const int   pivotMinSpeed    = 120;    // min pivot PWM
const int   pivotMaxSpeed    = 255;    // max pivot PWM

// ---------------------
// Dash-gap & Deadband
// ---------------------
const int   deadbandThreshold = 20;     // tiny errors → zero
const unsigned long dashDuration = 300; // ms: treat shorter white gaps as dashed-lines

// ---------------------
// Lost-line timing
// ---------------------
unsigned long lostLineStart = 0;

// ---------------------
// Setup
// ---------------------
void setup() {
  Serial.begin(115200);

  pinMode(STBY,      OUTPUT);
  pinMode(motorAPWM, OUTPUT);
  pinMode(motorAIn1, OUTPUT);
  pinMode(motorAIn2, OUTPUT);
  pinMode(motorBPWM, OUTPUT);
  pinMode(motorBIn1, OUTPUT);
  pinMode(motorBIn2, OUTPUT);
  digitalWrite(STBY, HIGH);

  qtrrc.setTypeAnalog();
  qtrrc.setSensorPins(sensorPins, NUM_SENSORS);

  Serial.println("Calibrating sensors...");
  for (uint16_t i = 0; i < 200; i++) {
    qtrrc.calibrate();
    delay(10);
  }
  Serial.println("Calibration complete.");
}

// ---------------------
// Motor Helper
// ---------------------
void setMotor(int pwmPin, int in1, int in2, int speed) {
  if (speed > 0) {
    digitalWrite(in1, HIGH);
    digitalWrite(in2, LOW);
    analogWrite(pwmPin, speed);
  } else if (speed < 0) {
    digitalWrite(in1, LOW);
    digitalWrite(in2, HIGH);
    analogWrite(pwmPin, -speed);
  } else {
    digitalWrite(in1, LOW);
    digitalWrite(in2, LOW);
    analogWrite(pwmPin, 0);
  }
}

// ---------------------
// Main Loop
// ---------------------
void loop() {
  // 1) Read interpolated position (0–7000)
  unsigned int position = qtrrc.readLineBlack(sensorValues);

  // 2) Detect if any sensor sees the line
  bool lineDetected = false;
  for (int i = 0; i < NUM_SENSORS; i++) {
    if (sensorValues[i] > 700) {
      lineDetected = true;
      break;
    }
  }

  // 3) Track how long we've been off the line
  if (!lineDetected) {
    if (lostLineStart == 0) lostLineStart = millis();
  } else {
    lostLineStart = 0;
  }
  unsigned long lostDuration = lostLineStart ? (millis() - lostLineStart) : 0;

  // 4) Always compute error based on actual position reading
  long error = (long)setpoint - (long)position;
  if (abs(error) < deadbandThreshold) {
    error = 0;
  }

  // 5) Decide speeds
  int leftSpeed  = 0;
  int rightSpeed = 0;

  // a) Dynamic pivot for very large errors (highest priority)
  if (abs(error) > pivotThreshold) {
    long pivotCorrection = (long)(pivotKp * error);
    int pivotSpeed = abs(pivotCorrection);
    pivotSpeed = constrain(pivotSpeed, pivotMinSpeed, pivotMaxSpeed);
    int dir = (pivotCorrection > 0) ? +1 : -1;
    leftSpeed  = pivotSpeed * dir;
    rightSpeed = -pivotSpeed * dir;

  // b) Brief white gap → dashed-line straight-through
  } else if (!lineDetected && lostDuration < dashDuration) {
    leftSpeed  = defaultSpeed;
    rightSpeed = defaultSpeed;

  // c) Full lost → slow search spin
  } else if (!lineDetected) {
    int dir = (error > 0) ? +1 : -1;
    leftSpeed  = searchSpeed * dir;
    rightSpeed = -searchSpeed * dir;

  // d) Normal PID follow
  } else {
    integral += error;
    integral  = constrain(integral, -10000, 10000);
    long derivative = error - lastError;
    long correction = (long)(effectiveKp * error + Ki * integral + Kd * derivative);
    leftSpeed  = constrain(defaultSpeed + correction, -255, 255);
    rightSpeed = constrain(defaultSpeed - correction, -255, 255);
  }

  lastError = error;

  // 6) Drive motors
  setMotor(motorAPWM, motorAIn1, motorAIn2, leftSpeed);
  setMotor(motorBPWM, motorBIn1, motorBIn2, rightSpeed);
}
