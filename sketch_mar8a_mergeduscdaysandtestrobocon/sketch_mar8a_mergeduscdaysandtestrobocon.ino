#include <QTRSensors.h>

// --- Motor Pins (TB6612FNG) ---
const int PWMA_PIN = 7;
const int AIN1_PIN = 8;
const int AIN2_PIN = 9;
const int PWMB_PIN = 4;
const int BIN1_PIN = 5;
const int BIN2_PIN = 6;  

// --- QTRX Control Pins ---
const uint8_t ODD_CTRL = 10;
const uint8_t EVEN_CTRL = 11;
const uint8_t NUM_SENSORS = 15;
const uint8_t SENSOR_PINS[NUM_SENSORS] = { A14, A13, A12, A11, A10, A9, A8, A7, A6, A5, A4, A3, A2, A1, A0 };
QTRSensors qtr;
uint16_t sensorValues[NUM_SENSORS];

// ==========================================
// PID & MOTION CONSTANTS
// ==========================================
// QTRX HD 15-A analog: after calibration the library scales every sensor
// reading to 0 (fully white) to 1000 (fully black).
// readLineBlack() returns a weighted average position:
//   0     = line is under sensor 0  (far left)
//   7000  = line is centred (sensor 7 of 15)
//   14000 = line is under sensor 14 (far right)

const long  setpoint         = 7000;   // target position = dead centre of the array

// --- PID GAINS ---
// These values came from a previous working run and are a good starting point.
// HOW TO RE-TUNE if the robot behaviour changes:
//
//  STEP 1 – Kp only (set Ki=0, Kd=0):
//    Raise Kp until the robot follows the line but oscillates (wiggles).
//    Then back it off ~20% so it just barely stops oscillating.
//
//  STEP 2 – Kd (derivative, dampens oscillation):
//    With the Kp you found, raise Kd until the wiggling smooths out.
//    Too much Kd makes the robot jerky/twitchy on straight lines.
//
//  STEP 3 – Ki (integral, fixes steady-state drift):
//    Only needed if the robot drifts slightly to one side on long straights.
//    Keep Ki very small; too much causes slow growing oscillations ("integral windup").
//
//  QUICK REFERENCE for this track (sharp U-turns + dashed lines):
//    If it cuts corners  → raise Kp or lower pivotThreshold
//    If it oscillates    → lower Kp or raise Kd
//    If it drifts slowly → tiny increase to Ki
//    If it is sluggish   → raise defaultSpeed, then re-tune Kp
const float effectiveKp      = 0.043f; // proportional gain — tune this first
const float Ki               = 0.0001f;// integral gain    — fixes long-straight drift
const float Kd               = 0.340f; // derivative gain  — dampens oscillation

// --- SPEED ---
const int   defaultSpeed     = 220;    // base forward PWM (0–255); lower = easier to tune
const int   pivotBaseSpeed   = 150;    // PWM used during in-place pivot on sharp corners
const int   searchSpeed      = 140;    // PWM used when rotating to find a lost line

// --- THRESHOLDS ---
// blackThreshold: QTRSensors calibrated range is 0 (white) to 1000 (black).
//   500 is the midpoint. Raise to 600–700 if bright ambient light causes
//   false "black" readings on white surface.
const int   blackThreshold   = 500;

// pivotThreshold: maximum error before switching from PID to a full in-place pivot.
//   Max possible error = 7000 (line at far edge). 4500 ≈ 64% of max, meaning
//   the line has reached roughly the outer 3rd of the sensor array.
//   Lower (e.g. 3500) if sharp U-turns are being missed; raise if pivots trigger too early.
const long  pivotThreshold   = 4500;

// deadbandThreshold: error smaller than this is treated as zero (no correction applied).
//   Suppresses motor jitter from sensor noise when already well-centred.
//   150 ≈ 15% of one sensor pitch (1000 units). Raise if motors buzz on straights.
const int   deadbandThreshold= 150;

// dashDuration: how long (ms) to coast on the last known correction when the line
//   disappears — handles dashed-line gaps on the track.
//   Estimate: gap_length_mm / (defaultSpeed_fraction * robot_speed_mm_per_s).
//   400 ms gives ~40–60 mm of coasting at typical competition speeds.
//   Increase if the robot stops mid-dash; decrease if it overshoots past the next solid line.
const unsigned long dashDuration = 400;

// State
long  lastError    = 0;
long  integral     = 0;
unsigned long lostLineStart = 0;
unsigned long startTime     = 0;

// Forward declaration (needed for calibration sweep)
void setMotors(int leftSpeed, int rightSpeed);

void setup() {
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

  // --- QTR Wake Up & Calibration ---
  pinMode(ODD_CTRL, OUTPUT);
  pinMode(EVEN_CTRL, OUTPUT);
  digitalWrite(ODD_CTRL, HIGH);
  digitalWrite(EVEN_CTRL, HIGH);
  qtr.setTypeAnalog();
  qtr.setSensorPins(SENSOR_PINS, NUM_SENSORS);
  qtr.setEmitterPin(QTRNoEmitterPin);

  // --- AUTO-CALIBRATE (with motor sweep for proper min/max learning) ---
  digitalWrite(13, HIGH);
  unsigned long calibStart = millis();
  int sweepPhase = 0;
  while (millis() - calibStart < 5000) {
    qtr.calibrate();
    // Sweep left-right so sensors see both black and white
    unsigned long elapsed = millis() - calibStart;
    if (elapsed < 1000) {          // Turn left
      setMotors(-80, 80);
    } else if (elapsed < 2500) {   // Turn right
      setMotors(80, -80);
    } else if (elapsed < 4000) {   // Turn left again
      setMotors(-80, 80);
    } else {                       // Return to center
      setMotors(60, -60);
    }
  }
  setMotors(0, 0);  // Stop after calibration
  digitalWrite(13, LOW);

  integral       = 0;
  lastError      = 0;
  lostLineStart  = 0;
  startTime      = millis();

  Serial.begin(115200);
}

// ==========================================
// MAIN LOOP
// ==========================================
void loop() {
  // 1) Read calibrated sensors and compute line position (0–14000)
  unsigned int position = qtr.readLineBlack(sensorValues);

  // 2) Detect line presence (calibrated value ≥ blackThreshold means black)
  bool anyBlack = false;
  bool allBlack = true;
  for (int i = 0; i < NUM_SENSORS; i++) {
    if (sensorValues[i] >= (uint16_t)blackThreshold) {
      anyBlack = true;
    } else {
      allBlack = false;
    }
  }
  bool lineDetected = anyBlack;

  // 3) Track lost-line start time
  if (!lineDetected) {
    if (lostLineStart == 0) lostLineStart = millis();
  } else {
    lostLineStart = 0;
  }
  unsigned long lostDuration = lostLineStart ? millis() - lostLineStart : 0;

  // 4) Compute error (freeze at lastError when line is lost)
  long error = lineDetected ? ((long)setpoint - (long)position) : lastError;
  if (abs(error) < deadbandThreshold) error = 0;

  int leftMotorSpeed  = 0;
  int rightMotorSpeed = 0;
  const char* motionState = "";

  // 5a) All sensors see black → crossing or intersection → drive straight
  if (allBlack) {
    leftMotorSpeed  = defaultSpeed;
    rightMotorSpeed = defaultSpeed;
    motionState = "ALL_BLACK_FORWARD";

  // 5b) Error large enough for an in-place pivot (very sharp corners)
  } else if (abs(error) > pivotThreshold) {
    int dir = (error > 0) ? +1 : -1;   // positive error = line is right of centre
    leftMotorSpeed  =  pivotBaseSpeed * dir;
    rightMotorSpeed = -pivotBaseSpeed * dir;
    motionState = (dir > 0) ? "PIVOT_RIGHT" : "PIVOT_LEFT";

  // 5c) Brief loss → dashed-line gap: coast with last-known correction
  } else if (!lineDetected && lostDuration < dashDuration) {
    long correction = (long)(effectiveKp * lastError);
    leftMotorSpeed  = constrain(defaultSpeed + correction, -255, 255);
    rightMotorSpeed = constrain(defaultSpeed - correction, -255, 255);
    motionState = "DASH_FORWARD";

  // 5d) Long loss → rotate to search for the line
  } else if (!lineDetected) {
    int dir = (lastError > 0) ? +1 : -1;
    leftMotorSpeed  =  searchSpeed * dir;
    rightMotorSpeed = -searchSpeed * dir;
    motionState = (dir > 0) ? "SEARCH_RIGHT" : "SEARCH_LEFT";

  // 5e) Normal line-following with full PID
  } else {
    integral += error;
    integral  = constrain(integral, -10000L, 10000L);
    long derivative = error - lastError;
    lastError = error;

    long correction = (long)(effectiveKp * error)
                    + (long)(Ki          * integral)
                    + (long)(Kd          * derivative);
    leftMotorSpeed  = constrain(defaultSpeed + correction, -255, 255);
    rightMotorSpeed = constrain(defaultSpeed - correction, -255, 255);

    if (error == 0)      motionState = "FORWARD";
    else if (error > 0)  motionState = "TURN_RIGHT";
    else                 motionState = "TURN_LEFT";
  }

  setMotors(leftMotorSpeed, rightMotorSpeed);

  // Debug output over Serial
  Serial.print("Pos:"); Serial.print(position);
  Serial.print(" Err:"); Serial.print(error);
  Serial.print(" L:"); Serial.print(leftMotorSpeed);
  Serial.print(" R:"); Serial.print(rightMotorSpeed);
  Serial.print(" "); Serial.println(motionState);
}

// ==========================================
// TB6612FNG MOTOR DRIVE
// ==========================================
void setMotors(int leftSpeed, int rightSpeed) {
  leftSpeed = constrain(leftSpeed, -255, 255);
  rightSpeed = constrain(rightSpeed, -255, 255);

  if (leftSpeed >= 0) {
    digitalWrite(AIN1_PIN, HIGH);
    digitalWrite(AIN2_PIN, LOW);
  } else {
    digitalWrite(AIN1_PIN, LOW);
    digitalWrite(AIN2_PIN, HIGH);
  }
  analogWrite(PWMA_PIN, abs(leftSpeed));

  if (rightSpeed >= 0) {
    digitalWrite(BIN1_PIN, HIGH);
    digitalWrite(BIN2_PIN, LOW);
  } else {
    digitalWrite(BIN1_PIN, LOW);
    digitalWrite(BIN2_PIN, HIGH);
  }
  analogWrite(PWMB_PIN, abs(rightSpeed));
}
