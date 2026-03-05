/*******************************************************************
 * Teensy 4.1: MOTOR TEST MODE
 * Tests: Forward, Turn Right, Turn Left
 * Prints: Direction, PWM speed, Encoder speed
 *******************************************************************/

#include <Encoder.h>

// ==========================================
// 1. PIN DEFINITIONS
// ==========================================
// --- Motor Pins (TB6612FNG) ---
const int PWMA_PIN = 7;
const int AIN1_PIN = 9;
const int AIN2_PIN = 8;
const int PWMB_PIN = 4;
const int BIN1_PIN = 5;
const int BIN2_PIN = 6;  

// --- Encoders ---
Encoder leftEnc(0, 1);
Encoder rightEnc(2, 3);

// ==========================================
// 2. TEST VARIABLES
// ==========================================
int testPWM = 220;  // PWM speed for testing (0-255)

// Test states: 0=STOP, 1=FORWARD, 2=TURN_RIGHT, 3=TURN_LEFT
int testState = 0;
const char* stateNames[] = {"STOP", "FORWARD", "TURN RIGHT", "TURN LEFT"};

// Timing
unsigned long stateStartTime = 0;
const unsigned long STATE_DURATION = 3000;  // 3 seconds per state
unsigned long lastPrintTime = 0;
const unsigned long PRINT_INTERVAL = 100;   // Print every 100ms

// Encoder tracking
long lastLeftEnc = 0;
long lastRightEnc = 0;
unsigned long lastEncTime = 0;

// Current motor speeds (for display)
int currentLeftPWM = 0;
int currentRightPWM = 0;

// ==========================================
// 3. SETUP
// ==========================================
void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000);  // Wait for Serial (max 3s)
  
  pinMode(13, OUTPUT);
  
  // --- Motor Setup ---
  pinMode(PWMA_PIN, OUTPUT);
  pinMode(AIN1_PIN, OUTPUT);
  pinMode(AIN2_PIN, OUTPUT);
  pinMode(PWMB_PIN, OUTPUT);
  pinMode(BIN1_PIN, OUTPUT);
  pinMode(BIN2_PIN, OUTPUT);
  analogWriteFrequency(PWMA_PIN, 20000);
  analogWriteFrequency(PWMB_PIN, 20000);

  // Reset encoders
  leftEnc.write(0);
  rightEnc.write(0);
  
  Serial.println("==========================================");
  Serial.println("    MOTOR TEST - PIN VERIFICATION");
  Serial.println("==========================================");
  Serial.println("Test PWM: " + String(testPWM));
  Serial.println("Each state runs for 3 seconds");
  Serial.println("States: STOP -> FORWARD -> RIGHT -> LEFT -> repeat");
  Serial.println("==========================================");
  Serial.println();
  Serial.println("DIR\t\tL_PWM\tR_PWM\tL_ENC\tR_ENC\tL_SPD\tR_SPD");
  Serial.println("----------------------------------------------------------");
  
  stateStartTime = millis();
  lastEncTime = millis();
}

// ==========================================
// 4. MAIN LOOP
// ==========================================
void loop() {
  unsigned long now = millis();
  
  // --- State Machine: Cycle through test states ---
  if (now - stateStartTime >= STATE_DURATION) {
    testState = (testState + 1) % 4;  // Cycle 0->1->2->3->0
    stateStartTime = now;
    
    // Reset encoders at each state change for cleaner comparison
    leftEnc.write(0);
    rightEnc.write(0);
    lastLeftEnc = 0;
    lastRightEnc = 0;
    
    Serial.println();
    Serial.println(">>> STATE CHANGE: " + String(stateNames[testState]) + " <<<");
    Serial.println();
  }
  
  // --- Execute current test state ---
  switch (testState) {
    case 0:  // STOP
      setMotors(0, 0);
      currentLeftPWM = 0;
      currentRightPWM = 0;
      break;
      
    case 1:  // FORWARD (both motors forward)
      setMotors(testPWM, testPWM);
      currentLeftPWM = testPWM;
      currentRightPWM = testPWM;
      break;
      
    case 2:  // TURN RIGHT (left forward, right backward)
      setMotors(testPWM, -testPWM);
      currentLeftPWM = testPWM;
      currentRightPWM = -testPWM;
      break;
      
    case 3:  // TURN LEFT (left backward, right forward)
      setMotors(-testPWM, testPWM);
      currentLeftPWM = -testPWM;
      currentRightPWM = testPWM;
      break;
  }
  
  // --- Print telemetry data ---
  if (now - lastPrintTime >= PRINT_INTERVAL) {
    lastPrintTime = now;
    
    // Read encoders
    long leftEncValue = leftEnc.read();
    long rightEncValue = rightEnc.read();
    
    // Calculate encoder speed (ticks per second)
    unsigned long dt = now - lastEncTime;
    float leftSpeed = 0;
    float rightSpeed = 0;
    if (dt > 0) {
      leftSpeed = (leftEncValue - lastLeftEnc) * 1000.0 / dt;
      rightSpeed = (rightEncValue - lastRightEnc) * 1000.0 / dt;
    }
    lastLeftEnc = leftEncValue;
    lastRightEnc = rightEncValue;
    lastEncTime = now;
    
    // Blink LED to show activity
    digitalWrite(13, (now / 500) % 2);
    
    // Print: Direction, Left PWM, Right PWM, Left Encoder, Right Encoder, Left Speed, Right Speed
    Serial.print(stateNames[testState]);
    Serial.print("\t");
    if (testState == 0) Serial.print("\t");  // Extra tab for alignment
    Serial.print(currentLeftPWM);
    Serial.print("\t");
    Serial.print(currentRightPWM);
    Serial.print("\t");
    Serial.print(leftEncValue);
    Serial.print("\t");
    Serial.print(rightEncValue);
    Serial.print("\t");
    Serial.print(leftSpeed, 0);
    Serial.print("\t");
    Serial.println(rightSpeed, 0);
  }
}

// ==========================================
// 5. TB6612FNG MOTOR DRIVE (BI-DIRECTIONAL)
// ==========================================
void setMotors(int leftSpeed, int rightSpeed) {
  // Cap max PWM to 255
  leftSpeed = constrain(leftSpeed, -255, 255);
  rightSpeed = constrain(rightSpeed, -255, 255);

  // LEFT MOTOR (Motor A)
  if (leftSpeed >= 0) {
    digitalWrite(AIN1_PIN, HIGH);
    digitalWrite(AIN2_PIN, LOW);
  } else {
    digitalWrite(AIN1_PIN, LOW);
    digitalWrite(AIN2_PIN, HIGH);
  }
  analogWrite(PWMA_PIN, abs(leftSpeed));

  // RIGHT MOTOR (Motor B)
  if (rightSpeed >= 0) {
    digitalWrite(BIN1_PIN, HIGH);
    digitalWrite(BIN2_PIN, LOW);
  } else {
    digitalWrite(BIN1_PIN, LOW);
    digitalWrite(BIN2_PIN, HIGH);
  }
  analogWrite(PWMB_PIN, abs(rightSpeed));
}