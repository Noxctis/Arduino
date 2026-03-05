#include <QTRSensors.h>
#include "DualVNH5019MotorShield.h"
#include <Encoder.h>

// ================= USER CONFIGURATION =================
// Motor PWM values (–400…+400). Change these to adjust speed/direction.
const int pwmM1               = 300;  // Motor 1 PWM magnitude
const int pwmM2               = 300;  // Motor 2 PWM magnitude

// Battery voltage (for applied-voltage calculation)
const float batteryVoltage   = 9.2;

// How often (ms) to send motor telemetry
const unsigned long TELE_MS  = 100;

// QTR sensor pins (you rarely need to touch these)
#define NUM_SENSORS 15
const uint8_t SENSOR_PINS[NUM_SENSORS] = {
  A0, A1, A2, A3, A4, A5, A6,
  A7, A8, A9, A10, A11, A12,
  A13, A14
};

// ======================================================

// Motor driver (M1INA, M1INB, M1PWM, M1EN_DIAG, M1CS,  M2INA, M2INB, M2PWM, M2EN_DIAG, M2CS)
DualVNH5019MotorShield md(
  8,  9, 10,  7, 39,
  1,  2, 28,  0, 40
);

// Encoders
Encoder enc1(5, 6);
Encoder enc2(11,12);

// Wheel & gearbox constants (usually don’t change)
const float WHEEL_D_MM = 43.0;
const float WHEEL_CIRC = WHEEL_D_MM * 3.14159;
const float CPR50      = 12.0 * 50.0;  // 12 CPR × 50:1 gearbox

QTRSensors qtr;
uint16_t sensorValues[NUM_SENSORS];

// Telemetry state
unsigned long lastTime1 = 0, lastTime2 = 0;
long lastCnt1 = 0, lastCnt2 = 0;
float dist1 = 0, dist2 = 0;

void stopIfFault() {
  if (md.getM1Fault()) {
    const char* msg = "ERROR: M1 fault!";
    Serial.println(msg);
    Serial8.println(msg);
    while (1);
  }
  if (md.getM2Fault()) {
    const char* msg = "ERROR: M2 fault!";
    Serial.println(msg);
    Serial8.println(msg);
    while (1);
  }
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  // Start USB Serial + Serial8 (to ESP8266) immediately
  Serial.begin(115200);
  Serial8.begin(115200);

  // QTR sensor setup + calibration
  qtr.setTypeAnalog();
  qtr.setSensorPins(SENSOR_PINS, NUM_SENSORS);
  qtr.setEmitterPin(QTRNoEmitterPin);
  for (uint16_t i = 0; i < 200; i++) {
    qtr.read(sensorValues, QTRReadMode::On);
    delay(10);
  }

  // Motor driver + encoders
  md.init();
  enc1.write(0);
  enc2.write(0);
  lastCnt1 = lastCnt2 = 0;
  lastTime1 = lastTime2 = millis();

  // Apply the PWM values you set above.
  // If a wheel spins the wrong way, **invert** the sign here:
  md.setM1Speed(-pwmM1);  // ← change to +pwmM1 if direction is flipped
  md.setM2Speed(-pwmM2);  // ← change to +pwmM2 if direction is flipped
}

void loop() {
  unsigned long now = millis();

  // —— SENSOR BROADCAST ——
  qtr.read(sensorValues, QTRReadMode::On);
  for (uint8_t i = 0; i < NUM_SENSORS; i++) {
    Serial.print(sensorValues[i]);
    Serial.print(i < NUM_SENSORS-1 ? '\t' : '\n');
    Serial8.print(sensorValues[i]);
    Serial8.print(i < NUM_SENSORS-1 ? '\t' : '\n');
  }

  // —— MOTOR 1 TELEMETRY ——
  if (now - lastTime1 >= TELE_MS) {
    long cnt1  = -enc1.read();     // invert if your encoder counts backward
    long dcnt1 = cnt1 - lastCnt1;
    float dt1  = (now - lastTime1) / 1000.0f;
    float rev1 = dcnt1 / CPR50;
    float rpm1 = rev1 / dt1 * 60.0f;
    float spd1 = rev1 * WHEEL_CIRC / dt1;      // mm/s
    dist1     += rev1 * WHEEL_CIRC / 1000.0f;  // meters
    lastCnt1   = cnt1;
    lastTime1  = now;

    int   cur1  = md.getM1CurrentMilliamps();
    float vs1   = fabs(pwmM1) / 400.0f;        // dynamic volt scale
    float vout1 = batteryVoltage * vs1;
    String t1   = String("M1: ") +
                  "RPM="  + rpm1   +
                  ", spd=" + spd1   + "mm/s" +
                  ", dist=" + dist1 + "m" +
                  ", Vbat=" + vout1 + "V" +
                  ", PWM%=" + (vs1*100.0f) +
                  ", Cur=" + cur1  + "mA";
    Serial.println(t1);
    Serial8.println(t1);
  }

  // —— MOTOR 2 TELEMETRY ——
  if (now - lastTime2 >= TELE_MS) {
    long cnt2  = -enc2.read();
    long dcnt2 = cnt2 - lastCnt2;
    float dt2  = (now - lastTime2) / 1000.0f;
    float rev2 = dcnt2 / CPR50;
    float rpm2 = rev2 / dt2 * 60.0f;
    float spd2 = rev2 * WHEEL_CIRC / dt2;
    dist2     += rev2 * WHEEL_CIRC / 1000.0f;
    lastCnt2   = cnt2;
    lastTime2  = now;

    int   cur2  = md.getM2CurrentMilliamps();
    float vs2   = fabs(pwmM2) / 400.0f;
    float vout2 = batteryVoltage * vs2;
    String t2   = String("M2: ") +
                  "RPM="  + rpm2   +
                  ", spd=" + spd2   + "mm/s" +
                  ", dist=" + dist2 + "m" +
                  ", Vbat=" + vout2 + "V" +
                  ", PWM%=" + (vs2*100.0f) +
                  ", Cur=" + cur2  + "mA";
    Serial.println(t2);
    Serial8.println(t2);
  }

  stopIfFault();
  delay(20);  // ~50 Hz sensor updates
}
