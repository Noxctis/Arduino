// TEAM: Complexity Robotics
// Members: Wouter Wessels & Martin Ferreira
// Coach: Mr. Paul Ntsinyi

#include <QTRSensors.h>

// Number of sensors used (8 sensors)
const uint8_t SensorCount = 8;
// Array to hold the calibrated sensor readings
uint16_t sensorValues[SensorCount];

// PID Control Constants
const double KP = 3.5;       // Proportional constant: scales the error directly.
const double KI = 0.01;      // Integral constant: accumulates past errors to counter steady-state issues.
const double KD = 3.0;       // Derivative constant: responds to the rate at which error changes.
double lastError = 0;        // Stores the error from the previous loop cycle (for derivative calculation).
double integral = 0;         // Accumulates the error over time (integral term).
// Set the target value to 3500: For an 8-sensor QTR array, this value represents a line
// that is between sensor 3 and sensor 4 (i.e. between the 4th and 5th sensor if counting starts at 1).
const int GOAL = 3500;
const int INTEGRAL_LIMIT = 500; // Limit for the integral term to prevent excessive accumulation

// Motor properties
const int MAX_SPEED = 100;   // Maximum motor speed (adjustable)

// TB6612FNG Motor Driver Pins
const int STBY = 10;         // Standby pin (must be HIGH for the driver to work)
const int motorAPWM = 5;     // PWM pin for Motor A (left motor)
const int motorAIn1 = 3;     // Direction pin 1 for Motor A
const int motorAIn2 = 4;     // Direction pin 2 for Motor A
const int motorBPWM = 11;    // PWM pin for Motor B (right motor)
const int motorBIn1 = 12;    // Direction pin 1 for Motor B
const int motorBIn2 = 13;    // Direction pin 2 for Motor B

// Create an instance of the QTRSensors object.
QTRSensors qtr;

void setup() {
  // Start Serial communication for debugging.
  Serial.begin(9600);
  
  // Configure QTR sensor to analog mode with the specified sensor pins (A0 to A7).
  qtr.setTypeAnalog();
  qtr.setSensorPins((const uint8_t[]){A0, A1, A2, A3, A4, A5, A6, A7}, SensorCount);

  // Initialize motor driver pins as outputs.
  pinMode(STBY, OUTPUT);
  digitalWrite(STBY, HIGH);  // Take the motor driver out of standby
  
  pinMode(motorAPWM, OUTPUT);
  pinMode(motorAIn1, OUTPUT);
  pinMode(motorAIn2, OUTPUT);
  pinMode(motorBPWM, OUTPUT);
  pinMode(motorBIn1, OUTPUT);
  pinMode(motorBIn2, OUTPUT);

  // Calibrate the sensors to determine the range of values when exposed to the line and the background.
  calibrateLineSensor();
  
  // Notify that calibration is complete and that the line-following routine is starting.
  Serial.println("Calibration complete. Starting line following...");
}

void loop() {
  // Use readLineBlack() to read the sensors, automatically applying the calibration data.
  // It returns a weighted average of the sensor indices (each multiplied by 1000) based on the sensor values.
  // For example:
  //   - A return value of 3000 means the line is directly below sensor 3.
  //   - A return value of 4000 means the line is directly below sensor 4.
  //   - A return value of 3500 indicates the line is exactly between sensor 3 and sensor 4.
  uint16_t position = qtr.readLineBlack(sensorValues);

  // Debug: Print the calibrated sensor values.
  Serial.print("Sensors: ");
  for (uint8_t i = 0; i < SensorCount; i++) {
    Serial.print(sensorValues[i]);
    Serial.print("\t");
  }
  
  // Calculate the error as the difference between the current position and the target GOAL.
  int error = position - GOAL;
  
  // Update the integral term, accumulating the error over time.
  integral += error;
  // Constrain the integral term to prevent it from growing too large (integral windup).
  integral = constrain(integral, -INTEGRAL_LIMIT, INTEGRAL_LIMIT);
  
  // Determine the derivative (how much the error has changed since the last reading).
  int derivative = error - lastError;
  
  // Compute the PID adjustment: the combination of proportional, integral, and derivative corrections.
  int adjustment = KP * error + KI * integral + KD * derivative;
  
  // Store the current error for the next loop iteration.
  lastError = error;
  
  // Debug: Print the error and adjustment values.
  Serial.print("  Error: ");
  Serial.print(error);
  Serial.print("  Adj: ");
  Serial.print(adjustment);

  // Update motor speeds based on the PID adjustment.
  controlMotors(adjustment);
  
  Serial.println();
  // Delay to control the update frequency; adjust this value as needed.
  delay(250);
}

// Function to adjust the motor speeds using the computed adjustment from the PID controller.
void controlMotors(int adjustment) {
  // Calculate the motor speeds by subtracting the adjustment from one motor and adding it to the other.
  // This creates a differential steering effect.
  int leftSpeed = MAX_SPEED - adjustment;
  int rightSpeed = MAX_SPEED + adjustment;

  // Ensure the motor speeds remain within the allowed range.
  leftSpeed = constrain(leftSpeed, 0, MAX_SPEED);
  rightSpeed = constrain(rightSpeed, 0, MAX_SPEED);

  // Debug: Print the calculated speeds for both motors.
  Serial.print(" L_Speed: ");
  Serial.print(leftSpeed);
  Serial.print("  R_Speed: ");
  Serial.print(rightSpeed);

  // Set motor A (left motor) direction and speed.
  digitalWrite(motorAIn1, leftSpeed > 0 ? HIGH : LOW);
  digitalWrite(motorAIn2, leftSpeed < 0 ? HIGH : LOW);
  analogWrite(motorAPWM, abs(leftSpeed));

  // Set motor B (right motor) direction and speed.
  digitalWrite(motorBIn1, rightSpeed < 0 ? HIGH : LOW);
  digitalWrite(motorBIn2, rightSpeed > 0 ? HIGH : LOW);
  analogWrite(motorBPWM, abs(rightSpeed));
}

// Function to calibrate the QTR sensors.
// ------------------------------------------------------
// Calibration Process Details:
// 1. A delay lets initial transients settle.
// 2. The built-in LED is activated to signal that calibration is in progress.
// 3. The calibration loop calls qtr.calibrate() 400 times so that each sensor records its minimum and maximum values,
//    both when over the dark line and over the light background.
// 4. This calibration allows the sensor readings to be normalized, ensuring accurate line detection.
// 5. Once calibration is complete, the LED is turned off.
void calibrateLineSensor() {
  delay(500);  // Wait for any initial transients to settle.
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH); // Turn on the LED to indicate calibration

  // Perform 400 iterations of calibration.
  for (uint16_t i = 0; i < 400; i++) {
    qtr.calibrate();
  }
  
  // Turn off the LED after calibration.
  digitalWrite(LED_BUILTIN, LOW); // Calibration complete
}
