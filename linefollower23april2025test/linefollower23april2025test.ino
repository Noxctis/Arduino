#include <QTRSensors.h>

// ---------------------
// QTR-8A Sensor Setup
// ---------------------
#define NUM_SENSORS 8
// Sensor pins for analog input (A0 through A7)
uint8_t sensorPins[NUM_SENSORS] = { A0, A1, A2, A3, A4, A5, A6, A7 };
QTRSensors qtrrc;                   // QTR sensor object (analog type)
unsigned int sensorValues[NUM_SENSORS];  // Array to store raw sensor readings

// ---------------------
// Motor Driver Pin Definitions (TB6612FNG)
// ---------------------
const int STBY      = 10;  // Standby pin; set HIGH to enable driver.
const int motorAPWM = 5;   // PWM pin for Motor A (left motor)
const int motorAIn1 = 3;   // Motor A direction pin 1
const int motorAIn2 = 4;   // Motor A direction pin 2
const int motorBPWM = 11;  // PWM pin for Motor B (right motor)
const int motorBIn1 = 12;  // Motor B direction pin 1
const int motorBIn2 = 13;  // Motor B direction pin 2

// ---------------------
// PID Controller Parameters & Variables
// ---------------------
// We require a high Kp for sharp turns, but that same high gain leads to excessive jitter
// when the robot is nearly centered. To address this, we use gain scheduling:
//
//   - When |error| > errorThreshold, use highKp (and if |error| > extremeErrorThreshold, further increase gain)
//   - Otherwise, use a lower Kp for fine adjustments.
// Ki is kept at 0 unless you experience drift, and Kd adds damping.
float highKp    = 0.9;    // Kp for normal sharp turn correction
float lowKp     = 0.4;    // Kp for fine adjustments near center
float extremeKp = 1.4;    // An even higher gain for extremely large errors
float effectiveKp;        // This variable holds the dynamically chosen Kp

float Ki = 0.0;           // Integral gain (set > 0 only if consistent offset appears)
float Kd = 1.1;           // Derivative gain provides damping

//Tune based on performance (zig-zagging = too little D, slow response = not enough P)

long lastError = 0;       // Stores the previous error value
long integral = 0;        // Accumulates the error over time

// The sensor array computes a weighted position from 0 to (NUM_SENSORS-1)*1000.
// For 8 sensors, a setpoint of ~3500 is considered centered.
long setpoint = ((NUM_SENSORS - 1) * 1000) / 2;

// ---------------------
// Base Speed and Lost-Line/Recovery Parameters
// ---------------------
int defaultSpeed = 200;   // Default forward speed when following the line
int baseSpeed = defaultSpeed;  // Base speed that may be adjusted dynamically

// Lost-line timing thresholds (in milliseconds)
const unsigned long briefLostThreshold  = 700;   // Up to 500ms of lost line = brief gap
const unsigned long searchDelayThreshold = 1400;  // Beyond 1000ms, consider the line fully lost

unsigned long lostLineCounter = 0;  // Accumulates the time (in ms) since the line was last detected
const int searchSpeed = 100;        // Speed used in SEARCHING mode (pivoting behavior)

// For overriding regular gains during extreme turns:
const int extremeErrorThreshold = 700;  // If |error| > this, assume a very sharp turn is needed

// Gain scheduling and deadband thresholds:
const int errorThreshold = 500;     // For |error| > 500, use highKp; otherwise, use lowKp
const int deadbandThreshold = 20;     // If |error| < 20, treat error as 0 to reduce jitter

// ---------------------
// Robot State Definitions
// ---------------------
enum RobotState { FOLLOWING, BRIEFLY_LOST, SEARCHING };
RobotState robotState = FOLLOWING;

// ---------------------
// Setup Function
// ---------------------
void setup() {
  Serial.begin(115200);
  
  // Initialize motor driver pins.
  pinMode(STBY, OUTPUT);
  pinMode(motorAPWM, OUTPUT);
  pinMode(motorAIn1, OUTPUT);
  pinMode(motorAIn2, OUTPUT);
  pinMode(motorBPWM, OUTPUT);
  pinMode(motorBIn1, OUTPUT);
  pinMode(motorBIn2, OUTPUT);
  digitalWrite(STBY, HIGH);  // Enable the motor driver.
  
  // Configure QTRSensors for analog inputs and assign sensor pins.
  qtrrc.setTypeAnalog();
  qtrrc.setSensorPins(sensorPins, NUM_SENSORS);
  
  // Calibrate the sensors: Move the array over both black and white surfaces during calibration.
  Serial.println("Calibrating sensors...");
  for (uint16_t i = 0; i < 400; i++) {
    qtrrc.calibrate();
    delay(10);
  }
  Serial.println("Calibration complete.");
}

// ---------------------
// Helper Function: setMotor
// Sets a motor's speed and direction. A positive speed drives forward, negative drives reverse.
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
// Main Loop: PID with State-Based Lost-Line Handling and Gain Scheduling
// ---------------------
void loop() {
  // Read sensor values and calculate the weighted average position.
  // (Your calibration shows that black produces high values.)
  unsigned int position = qtrrc.readLineBlack(sensorValues);

  // Print sensor readings for debugging.
  Serial.print("Sensors: ");
  for (int i = 0; i < NUM_SENSORS; i++) {
    Serial.print(sensorValues[i]);
    Serial.print("\t");
  }
  Serial.print("Pos: ");
  Serial.println(position);
  
  // ----------- Determine if the Line Is Detected -----------
  // Since black returns high values, we consider the line detected if any sensor value > 700.
  bool lineDetected = false;
  for (int i = 0; i < NUM_SENSORS; i++) {
    if (sensorValues[i] > 700) {
      lineDetected = true;
      break;
    }
  }
  
  long error;
  // ----------- State Machine: FOLLOWING, BRIEFLY_LOST, SEARCHING -----------
  if (lineDetected) {
    // Normal operation: line is detected.
    robotState = FOLLOWING;
    lostLineCounter = 0;
    error = setpoint - position;  // Compute error based on center.
  } else {
    // No line detected: increment lostLineCounter by the loop delay (assumed 10ms).
    lostLineCounter += 10;
    error = lastError;  // Use last known error.
    
    // Determine state based on how long the line has been lost.
    if (lostLineCounter < briefLostThreshold) {
      robotState = BRIEFLY_LOST;
    } else if (lostLineCounter < searchDelayThreshold) {
      robotState = BRIEFLY_LOST;
    } else {
      robotState = SEARCHING;
    }
  }
  
  // ----------- Deadband Filtering -----------
  // If the absolute error is very small, set it to zero to avoid unnecessary oscillations.
  if (abs(error) < deadbandThreshold) {
    error = 0;
  }
  
  int leftMotorSpeed = 0, rightMotorSpeed = 0;
  
  // ----------- PID and Driving Logic for FOLLOWING and BRIEFLY_LOST States -----------
  if (robotState == FOLLOWING || robotState == BRIEFLY_LOST) {
    // Adjust base speed: when line is briefly lost, lower the speed for sharper control.
    if (robotState == BRIEFLY_LOST) {
      baseSpeed = 160;
    } else {
      baseSpeed = defaultSpeed;
    }
    
    // Additional override for very sharp turns: if error exceeds extremeErrorThreshold,
    // use an even higher proportional gain and reduce base speed to enable a tighter pivot.
    if (abs(error) > extremeErrorThreshold) {
      effectiveKp = extremeKp;
      baseSpeed = 120;  // Lower speed for a more aggressive turn.
    } else if (abs(error) > errorThreshold) {
      effectiveKp = highKp;
    } else {
      effectiveKp = lowKp;
    }
    
    // PID Calculations:
    integral += error;
    integral = constrain(integral, -10000, 10000);  // Prevent windup.
    long derivative = error - lastError;
    lastError = error;
    long correction = effectiveKp * error + Ki * integral + Kd * derivative;
    
    // Differential drive: adjust motor speeds based on the correction.
    leftMotorSpeed = baseSpeed + correction;
    rightMotorSpeed = baseSpeed - correction;
    
    leftMotorSpeed = constrain(leftMotorSpeed, -255, 255);
    rightMotorSpeed = constrain(rightMotorSpeed, -255, 255);
    
    // Debug output.
    Serial.print("State: ");
    if (robotState == FOLLOWING)
      Serial.print("FOLLOWING ");
    else
      Serial.print("BRIEFLY_LOST ");
    Serial.print("| Error: ");
    Serial.print(error);
    Serial.print(" | Correction: ");
    Serial.print(correction);
    Serial.print(" | BaseSpeed: ");
    Serial.print(baseSpeed);
    Serial.print(" | EffectiveKp: ");
    Serial.print(effectiveKp);
    Serial.print(" | LostCounter: ");
    Serial.println(lostLineCounter);
    
  } else {  // SEARCHING state.
    // In SEARCHING mode, pivot aggressively in the direction suggested by the last error.
    // If lastError is positive, pivot right; if negative, pivot left.
    if (lastError > 0) {
      // Pivot right: left motor forward, right motor reverse.
      leftMotorSpeed = searchSpeed;
      rightMotorSpeed = -searchSpeed;
    } else {
      // Pivot left: left motor reverse, right motor forward.
      leftMotorSpeed = -searchSpeed;
      rightMotorSpeed = searchSpeed;
    }
    
    // Debug output.
    Serial.print("State: SEARCHING | Pivoting. LostCounter: ");
    Serial.println(lostLineCounter);
  }
  
  // ----------- Drive the Motors -----------
  setMotor(motorAPWM, motorAIn1, motorAIn2, leftMotorSpeed);
  setMotor(motorBPWM, motorBIn1, motorBIn2, rightMotorSpeed);
  
  delay(10);  // Loop delay (10 ms).
}
