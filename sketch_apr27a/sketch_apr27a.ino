#include "DualVNH5019MotorShield.h"
#include <Encoder.h>

const int led = LED_BUILTIN;

// Motor driver pins
DualVNH5019MotorShield md(
  8,   // M1INA
  9,   // M1INB
  10,  // M1PWM
  7,   // M1EN/DIAG
  39,  // M1CS (current sense on A15)
  0,0,0,0,0  // M2 unused
);

// Encoder on Motor 1
Encoder motorEncoder(5, 6); // D5=A, D6=B

// —— USER SETTINGS ——
// Measured battery voltage
const float batteryVoltage   = 9.2;     
// Your wheel diameter in mm
const float WHEEL_DIAMETER_MM = 43.0;   
// PWM command
const int   pwmValue          = 300;   

// Derived constants
const float VOLTAGE_SCALE     = abs(pwmValue) / 400.0f;
const float WHEEL_CIRC_MM     = WHEEL_DIAMETER_MM * 3.14159;
const float COUNTS_PER_REV    = 12.0 * 50.0;  // 12 CPR × 50:1 gearbox

// Timing
const unsigned long INTERVAL_MS = 100;
unsigned long lastTime    = 0;
long          lastCount   = 0;

// State
float totalDistanceM = 0;    // accumulated distance in meters

void stopIfFault() {
  if (md.getM1Fault()) {
    Serial.println("M1 fault!");
    while (1);
  }
}

void setup() {
  pinMode(led, OUTPUT);
  digitalWrite(led, HIGH);
  Serial.begin(115200);
  Serial.println("Forward + RPM + Speed + Dist + Vbat + PWM%");
  md.init();
  motorEncoder.write(0);
  lastCount = 0;
  lastTime  = millis();
  // run motor
  md.setM1Speed(-pwmValue);  // invert if needed
}

void loop() {
  unsigned long now = millis();
  if (now - lastTime >= INTERVAL_MS) {
    // 1) RPM & speed & distance
    long rawCount   = -motorEncoder.read();  // correct direction
    long deltaCount = rawCount - lastCount;
    float dtSec     = (now - lastTime) / 1000.0f;
    float revs      = deltaCount / COUNTS_PER_REV;
    float rpm       = revs / dtSec * 60.0f;
    float speed_mm_s= revs * WHEEL_CIRC_MM / dtSec;
    totalDistanceM += (revs * WHEEL_CIRC_MM) / 1000.0f;
    lastCount = rawCount;
    lastTime  = now;

    // 2) Motor current
    int current_mA = md.getM1CurrentMilliamps();

    // 3) PWM% & applied voltage
    float pwmPct      = VOLTAGE_SCALE * 100.0f;
    float appliedVolt = batteryVoltage * VOLTAGE_SCALE;

    // — print everything —
    Serial.print("PWM: ");
    Serial.print(pwmValue);
    Serial.print(" (");
    Serial.print(pwmPct,1);
    Serial.print("%) | Vbat: ");
    Serial.print(appliedVolt,2);
    Serial.print("V | RPM: ");
    Serial.print(rpm,1);
    Serial.print(" | Speed: ");
    Serial.print(speed_mm_s,1);
    Serial.print(" mm/s | Dist: ");
    Serial.print(totalDistanceM,3);
    Serial.print(" m | Current: ");
    Serial.print(current_mA);
    Serial.println(" mA");
  }

  stopIfFault();
  delay(10);
}
