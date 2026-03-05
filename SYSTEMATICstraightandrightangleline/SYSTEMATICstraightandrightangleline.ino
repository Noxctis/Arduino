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
float Kd           = 1.0;
float effectiveKp  = 0.395;
float Ki           = 0.00;
long  lastError    = 0;
long  integral     = 0;
long  setpoint     = ((NUM_SENSORS - 1) * 1000) / 2;

// ---------------------
// Speed Settings
// ---------------------
int defaultSpeed       = 170;
const int pivotBaseSpeed = 120;   // speed when doing in-place pivot
const int searchSpeed    = 100;   // speed when line is lost and searching

// ---------------------
// Tuning & Thresholds
// ---------------------
const int pivotThreshold       = 1500;  // if |error| > this, do in-place pivot
const int deadbandThreshold    = 20;    // treat very small errors as zero
const unsigned long dashDuration = 300; // ms threshold for treating as dashed-line

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
  // 1) Read sensors
  unsigned int position = qtrrc.readLineBlack(sensorValues);

  // 2) Detect line presence
  bool lineDetected = false;
  for (int i = 0; i < NUM_SENSORS; i++) {
    if (sensorValues[i] > 700) {
      lineDetected = true;
      break;
    }
  }

  // 3) Track lost-line start time
  if (!lineDetected) {
    if (lostLineStart == 0) {
      lostLineStart = millis();
    }
  } else {
    lostLineStart = 0;
  }
  unsigned long lostDuration = (lostLineStart == 0) ? 0 : (millis() - lostLineStart);

  // 4) Compute error (reuse lastError if lost)
  long error = lineDetected ? (setpoint - position) : lastError;
  if (abs(error) < deadbandThreshold) {
    error = 0;
  }

  int leftMotorSpeed  = 0;
  int rightMotorSpeed = 0;

  // 5a) Pivot in-place on very sharp turns
  if (abs(error) > pivotThreshold) {
    int dir = (error > 0) ? +1 : -1;
    leftMotorSpeed  =  pivotBaseSpeed * dir;
    rightMotorSpeed = -pivotBaseSpeed * dir;

  // 5b) Brief loss (dashed-line): go straight through
  } else if (!lineDetected && lostDuration < dashDuration) {
    leftMotorSpeed  = defaultSpeed;
    rightMotorSpeed = defaultSpeed;

  // 5c) Full search after gap too long
  } else if (!lineDetected) {
    int dir = (error > 0) ? +1 : -1;
    leftMotorSpeed  =  searchSpeed * dir;
    rightMotorSpeed = -searchSpeed * dir;

  // 5d) Normal follow with PID
  } else {
    integral += error;
    integral  = constrain(integral, -10000, 10000);
    long derivative = error - lastError;
    lastError = error;

    long correction = effectiveKp * error + Ki * integral + Kd * derivative;

    leftMotorSpeed  = constrain(defaultSpeed + correction, -255, 255);
    rightMotorSpeed = constrain(defaultSpeed - correction, -255, 255);
  }

  // 6) Drive motors
  setMotor(motorAPWM, motorAIn1, motorAIn2, leftMotorSpeed);
  setMotor(motorBPWM, motorBIn1, motorBIn2, rightMotorSpeed);
}
