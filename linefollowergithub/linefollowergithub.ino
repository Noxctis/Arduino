// TEAM: Complexity Robotics
// Members: Wouter Wessels & Martin Ferreira
// Coach: Mr. Paul Ntsinyi

#include <QTRSensors.h>

// Line sensor properties
const uint8_t SensorCount = 8;
uint16_t sensorValues[SensorCount];

// PID Control Constants
const double KP = 1.5;       // Proportional Constant
const double KI = 0.01;      // Integral Constant (adjust as needed)
const double KD = 3.0;       // Derivative Constant
double lastError = 0;
double integral = 0;
const int GOAL = 1700;       // Ideal sensor reading when centered on the line
const int INTEGRAL_LIMIT = 500; // Prevent integral windup

// Motor properties
const int MAX_SPEED = 100;   // Maximum speed (adjustable)

// TB6612FNG Motor Driver Pins
const int STBY = 10;         // Standby pin (must be set HIGH to enable driver)
const int motorAPWM = 5;     // PWM pin for Motor A
const int motorAIn1 = 3;     // Direction pin 1 for Motor A
const int motorAIn2 = 4;     // Direction pin 2 for Motor A
const int motorBPWM = 11;    // PWM pin for Motor B
const int motorBIn1 = 12;    // Direction pin 1 for Motor B (BIN1)
const int motorBIn2 = 13;    // Direction pin 2 for Motor B (BIN2)

// QTR sensor library declaration
QTRSensors qtr;

void setup() {
  // Initialize Serial Monitor for debugging
  Serial.begin(9600);
  
  // Configure QTR sensor (analog mode for QTR8A)
  qtr.setTypeAnalog();
  qtr.setSensorPins((const uint8_t[]){A0, A1, A2, A3, A4, A5, A6, A7}, SensorCount);

  // Configure TB6612FNG motor driver pins
  pinMode(STBY, OUTPUT);
  digitalWrite(STBY, HIGH); // Enable the motor driver
  
  pinMode(motorAPWM, OUTPUT);
  pinMode(motorAIn1, OUTPUT);
  pinMode(motorAIn2, OUTPUT);
  pinMode(motorBPWM, OUTPUT);
  pinMode(motorBIn1, OUTPUT);
  pinMode(motorBIn2, OUTPUT);

  // Run calibration process for the line sensor
  calibrateLineSensor();
  
  // Print an initialization message to the Serial Monitor
  Serial.println("Calibration complete. Starting line following...");
}

void loop() {
  // Read the line position using white threshold detection
  uint16_t position = qtr.readLineWhite(sensorValues);

  // Debug: Print sensor readings
  Serial.print("Sensors: ");
  for (uint8_t i = 0; i < SensorCount; i++) {
    Serial.print(sensorValues[i]);
    Serial.print("\t");
  }
  
  // Check if all sensors detect white (i.e. the line is missing)
  bool lineLost = true;
  for (uint8_t i = 0; i < SensorCount; i++) {
    if (sensorValues[i] < 1000) {  // If any sensor detects black, the line is present
      lineLost = false;
      break;
    }
  }

  int error = position - GOAL;
  int adjustment;

  if (lineLost) {
    // No line detected: assume a broken line, move forward with no turning adjustment
    adjustment = 0;
    integral = 0;  // Reset the integral term to prevent windup
    Serial.print("  Line Lost  ");
  } else {
    // PID calculation: compute the adjustment for motor speeds
    integral += error;
    integral = constrain(integral, -INTEGRAL_LIMIT, INTEGRAL_LIMIT); // Prevent windup
    int derivative = error - lastError;
    adjustment = KP * error + KI * integral + KD * derivative;
    lastError = error;
    Serial.print("  Error: ");
    Serial.print(error);
    Serial.print("  Adj: ");
    Serial.print(adjustment);
  }

  // Adjust motor speeds based on PID output
  controlMotors(adjustment);

  Serial.println();
  delay(250);  // Adjust loop delay as needed for your application
}

void controlMotors(int adjustment) {
  // Calculate left and right motor speeds based on the adjustment
  int leftSpeed = MAX_SPEED - adjustment;
  int rightSpeed = MAX_SPEED + adjustment;

  // Constrain speeds to remain within 0 and MAX_SPEED
  leftSpeed = constrain(leftSpeed, 0, MAX_SPEED);
  rightSpeed = constrain(rightSpeed, 0, MAX_SPEED);

  // Debug: Print motor speeds
  Serial.print("L_Speed: ");
  Serial.print(leftSpeed);
  Serial.print("  R_Speed: ");
  Serial.print(rightSpeed);

  // Set motor directions and speeds for Motor A (left motor)
  digitalWrite(motorAIn1, leftSpeed > 0 ? HIGH : LOW);
  digitalWrite(motorAIn2, leftSpeed < 0 ? HIGH : LOW);
  analogWrite(motorAPWM, abs(leftSpeed));

  // Set motor directions and speeds for Motor B (right motor)
  digitalWrite(motorBIn1, rightSpeed < 0 ? HIGH : LOW);
  digitalWrite(motorBIn2, rightSpeed > 0 ? HIGH : LOW);
  analogWrite(motorBPWM, abs(rightSpeed));
}

void calibrateLineSensor() {
  delay(500);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH); // Indicate calibration mode

  // Calibrate sensors 400 times (~10 seconds)
  for (uint16_t i = 0; i < 400; i++) {
    qtr.calibrate();
  }

  digitalWrite(LED_BUILTIN, LOW); // End calibration mode
}
