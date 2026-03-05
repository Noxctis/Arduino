/*******************************************************************
 * Teensy 4.1: Robot Brain (TB6612FNG, Encoders, QTRX-HD-15A)
 * FEATURES: Live UI Tuning, Acceleration Ramp, Sharp Turn Overrides
 * UPGRADED: Single-string "ALL:" parsing for 0-lag Wi-Fi telemetry
 *******************************************************************/

#include <QTRSensors.h>
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

// --- QTRX Control Pins ---
const uint8_t ODD_CTRL = 10;
const uint8_t EVEN_CTRL = 11;
const uint8_t NUM_SENSORS = 15;
const uint8_t SENSOR_PINS[NUM_SENSORS] = { A14, A13, A12, A11, A10, A9, A8, A7, A6, A5, A4, A3, A2, A1, A0 };
QTRSensors qtr;
uint16_t sensorValues[NUM_SENSORS];

// --- Encoders ---
Encoder leftEnc(0, 1);
Encoder rightEnc(2, 3);

// ==========================================
// 2. GLOBALS: TELEMETRY & PID
// ==========================================
// PID gains
float Kp = 0.36, Ki = 0.01, Kd = 7.99;
int lastError = 0;
long integralError = 0;
const long INTEGRAL_CAP = 50000; // Anti-windup limit

int uiBaseSpeed = 90; 
int currentBaseSpeed = 100;
int maxSpeedLimit = 255; 

// --- Broken Line / Gap Handling ---
bool lineDetected = true;
unsigned long lineLostTime = 0;
const unsigned long GAP_COAST_MS = 500;   // Coast straight for up to 250ms through gaps
const unsigned long LINE_LOST_MS = 1000;   // Stop motors after 800ms with no line
const uint16_t WHITE_THRESHOLD = 200;     // Below this = sensor sees white (adjust for your surface)
int lastGoodLeftSpeed = 0;
int lastGoodRightSpeed = 0;

// --- Acceleration Timers ---
unsigned long startTime = 0;
unsigned long elapsedTime = 0;
const unsigned long accelerationTime = 500; // 500ms soft start

// --- Telemetry Timers & Buffers ---
unsigned long lastTeleTime = 0;
const unsigned long TELE_MS = 100;  // 10Hz System State
unsigned long lastQtrSendTime = 0;
const unsigned long QTR_SEND_MS = 50;  // 20Hz QTR visualizer (optimized from 50Hz)
const int MAX_RX_LEN = 64;
char rxBuffer[MAX_RX_LEN];
int rxIndex = 0;

// ==========================================
// 3. SETUP & CALIBRATION
// ==========================================
void setup() {
  Serial8.begin(460800);
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

  // --- AUTO-CALIBRATE ---
  digitalWrite(13, HIGH);
  unsigned long calibStart = millis();
  while (millis() - calibStart < 5000) {
    qtr.calibrate();
  }
  digitalWrite(13, LOW);

  startTime = millis(); 
}

// ==========================================
// 4. MAIN LOOP
// ==========================================
void loop() {
  unsigned long now = millis();

  // --- A. RX: ESP32 Telemetry Parsing ---
  while (Serial8.available() > 0) {
    char c = Serial8.read();
    if (c == '\n') {
      rxBuffer[rxIndex] = '\0'; 
      parseCommand(rxBuffer);
      rxIndex = 0; 
    } else if (c != '\r' && rxIndex < MAX_RX_LEN - 1) {
      rxBuffer[rxIndex++] = c;
    }
  }

  // --- B. ACCELERATION RAMP ---
  elapsedTime = millis() - startTime;
  if (elapsedTime <= accelerationTime) {
    currentBaseSpeed = map(elapsedTime, 0, accelerationTime, 0, uiBaseSpeed);
  } else {
    currentBaseSpeed = uiBaseSpeed;
  }

  // --- C. READ SENSORS ---
  uint16_t position = qtr.readLineBlack(sensorValues);
  int error = position - 7000;
  char currentAction[64] = "";
  int leftSpeed = 0;
  int rightSpeed = 0;

  // --- GAP / BROKEN LINE DETECTION ---
  // Check if ALL sensors see white (= gap in a dashed line)
  bool allWhite = true;
  for (uint8_t i = 0; i < NUM_SENSORS; i++) {
    if (sensorValues[i] > WHITE_THRESHOLD) {
      allWhite = false;
      break;
    }
  }

  if (allWhite) {
    // First moment we lose the line — record the timestamp
    if (lineDetected) {
      lineLostTime = now;
      lineDetected = false;
    }

    unsigned long lostDuration = now - lineLostTime;

    if (lostDuration < GAP_COAST_MS) {
      // COAST: keep last known good speeds to glide through the gap
      leftSpeed = lastGoodLeftSpeed;
      rightSpeed = lastGoodRightSpeed;
      snprintf(currentAction, sizeof(currentAction), "~~ GAP COAST %lums ~~", lostDuration);
    } else if (lostDuration < LINE_LOST_MS) {
      // SLOW SEARCH: reduce speed, drift in last known direction
      leftSpeed = lastGoodLeftSpeed / 2;
      rightSpeed = lastGoodRightSpeed / 2;
      snprintf(currentAction, sizeof(currentAction), "?? SEARCHING %lums ??", lostDuration);
    } else {
      // TRULY LOST: stop motors
      leftSpeed = 0;
      rightSpeed = 0;
      snprintf(currentAction, sizeof(currentAction), "XX LINE LOST XX");
    }

    setMotors(leftSpeed, rightSpeed);

  } else {
    // Line is visible — reset gap tracker
    lineDetected = true;

    // --- PID CALCULATION (with Integral + Anti-windup) ---
    integralError += error;
    integralError = constrain(integralError, -INTEGRAL_CAP, INTEGRAL_CAP);
    int motorSpeed = (Kp * error) + (Ki * integralError) + (Kd * (error - lastError));
    lastError = error;

    // --- PROGRESSIVE SPEED ZONES (smoother deceleration into curves) ---
    int activeBase = currentBaseSpeed;
    int absError = abs(error);
    if (absError > 4000) {        // Extreme curve
      activeBase = constrain(activeBase * 0.45, 40, activeBase);
    } else if (absError > 3000) { // Sharp curve
      activeBase = constrain(activeBase * 0.55, 45, activeBase);
    } else if (absError > 2000) { // Moderate curve
      activeBase = constrain(activeBase * 0.70, 50, activeBase);
    } else if (absError > 1000) { // Gentle curve
      activeBase = constrain(activeBase * 0.85, 50, activeBase);
    }

    leftSpeed = activeBase + motorSpeed;
    rightSpeed = activeBase - motorSpeed;

    // --- SMOOTHER SHARP TURN OVERRIDES (reduced aggression) ---
    if (position <= 3000) {         // Very far left — hard pivot
      leftSpeed = constrain(-activeBase * 1.2, -180, -80);
      rightSpeed = constrain(activeBase * 1.4, 80, 200);
      integralError = 0; // Reset integral on sharp turns
      snprintf(currentAction, sizeof(currentAction), "!! HARD LEFT !!");
    } else if (position >= 11000) { // Very far right — hard pivot
      leftSpeed = constrain(activeBase * 1.4, 80, 200);
      rightSpeed = constrain(-activeBase * 1.2, -180, -80);
      integralError = 0;
      snprintf(currentAction, sizeof(currentAction), "!! HARD RIGHT !!");
    } else if (position <= 4500) {  // Moderate left — proportional turn
      leftSpeed = constrain(leftSpeed, -120, activeBase / 2);
      rightSpeed = constrain(rightSpeed, activeBase / 2, activeBase);
      snprintf(currentAction, sizeof(currentAction), "< TURN LEFT | L:%d R:%d", leftSpeed, rightSpeed);
    } else if (position >= 9500) {  // Moderate right — proportional turn
      leftSpeed = constrain(leftSpeed, activeBase / 2, activeBase);
      rightSpeed = constrain(rightSpeed, -120, activeBase / 2);
      snprintf(currentAction, sizeof(currentAction), "TURN RIGHT > | L:%d R:%d", leftSpeed, rightSpeed);
    } else {
      if (error < -500) {
        snprintf(currentAction, sizeof(currentAction), "<< LEFT | L:%d R:%d", leftSpeed, rightSpeed);
      } else if (error > 500) {
        snprintf(currentAction, sizeof(currentAction), "RIGHT >> | L:%d R:%d", leftSpeed, rightSpeed);
      } else {
        snprintf(currentAction, sizeof(currentAction), "^ STRAIGHT ^ | L:%d R:%d", leftSpeed, rightSpeed);
      }
    }

    // Save last good speeds for gap coast-through
    lastGoodLeftSpeed = leftSpeed;
    lastGoodRightSpeed = rightSpeed;

    setMotors(leftSpeed, rightSpeed);
  }
  // --- D. TX: Send QTR Arrays to ESP32 (Optimized to 20Hz) ---
  if (now - lastQtrSendTime >= QTR_SEND_MS) {
    for (uint8_t i = 0; i < NUM_SENSORS; i++) {
      Serial8.print(sensorValues[i]);
      Serial8.print(i < NUM_SENSORS - 1 ? '\t' : '\n');
    }
    lastQtrSendTime = now;
  }

  // --- E. TX: Send System State (10Hz) ---
  if (now - lastTeleTime >= TELE_MS) {
    lastTeleTime = now;

    char sysMsg[128];
    snprintf(sysMsg, sizeof(sysMsg), "SYS: Pos: %d | Err: %d | %s", position, error, currentAction);
    Serial8.println(sysMsg);
  }
}

// ==========================================
// 5. COMMAND PARSER (Efficient Bundled String)
// ==========================================
void parseCommand(char* cmd) {
  // Now intercepts the single payload: ALL:bs|Kp|Ki|Kd
  if (strncmp(cmd, "ALL:", 4) == 0) {
    int b; float p, i, d;
    if (sscanf(cmd + 4, "%d|%f|%f|%f", &b, &p, &i, &d) == 4) {
      uiBaseSpeed = b;
      Kp = p;
      Ki = i;
      Kd = d;
    }
  } 
  // Keep the old fallbacks just in case
  else if (strncmp(cmd, "bs:", 3) == 0) {
    uiBaseSpeed = atoi(cmd + 3);
  }
}

// ==========================================
// 6. TB6612FNG MOTOR DRIVE
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