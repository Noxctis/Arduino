#include <QTRSensors.h>

#define NUM_SENSORS 15
const uint8_t SENSOR_PINS[NUM_SENSORS] = {
  A0, A1, A2, A3, A4, A5, A6,
  A7, A8, A9, A10, A11, A12,
  A13, A14
};

QTRSensors qtr;
uint16_t sensorValues[NUM_SENSORS];

void setup() {
  // LED on = power OK
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);

  // USB Serial for debugging
  Serial.begin(115200);
  while (!Serial) ;  // wait for monitor

  // Serial7 → ESP8266
  Serial7.begin(115200);

  // QTR setup
  qtr.setTypeAnalog();
  qtr.setSensorPins(SENSOR_PINS, NUM_SENSORS);
  qtr.setEmitterPin(QTRNoEmitterPin);

  // Optional: calibrate by sweeping
  Serial.println("Calibrating QTR sensors...");
  for (uint16_t i = 0; i < 300; i++) {
    qtr.read(sensorValues, QTRReadMode::On);
    delay(10);
  }
  Serial.println("Calibration complete.");
}

void loop() {
  // 1) read sensors
  qtr.read(sensorValues, QTRReadMode::On);

  // 2) print to USB Serial Monitor
  for (uint8_t i = 0; i < NUM_SENSORS; i++) {
    Serial.print(sensorValues[i]);
    Serial.print(i < NUM_SENSORS - 1 ? '\t' : '\n');
  }

  // 3) send same line to ESP8266 over Serial7
  for (uint8_t i = 0; i < NUM_SENSORS; i++) {
    Serial7.print(sensorValues[i]);
    Serial7.print(i < NUM_SENSORS - 1 ? '\t' : '\n');
  }
  // (Serial7.println() of last value already printed the newline)

  delay(20);  // ~50 updates/sec
}
