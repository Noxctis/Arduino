#include <QTRSensors.h>

#define NUM_SENSORS 15

// Change these to the pins you wired your 0–14 outputs to:
const uint8_t SENSOR_PINS[NUM_SENSORS] = {
  A0, A1, A2, A3, A4, A5, A6,
  A7, A8, A9, A10, A11, A12,
  A13, A14
};

// If you wired the board’s EMITTER pin, put it here; otherwise set to QTRNoEmitterPin
//#define EMITTER_PIN  2

QTRSensors qtr;
uint16_t sensorValues[NUM_SENSORS];

void setup() {
  Serial.begin(115200);
  while (!Serial);

  // 1) tell the library we have analog outputs
  qtr.setTypeAnalog();

  // 2) give it your sensor pins
  qtr.setSensorPins(SENSOR_PINS, NUM_SENSORS);

  // 3) (optional) control the IR LEDs via the EMITTER pin
  //qtr.setEmitterPin(EMITTER_PIN);

  // 4) calibrate: sweep the sensor over your line/white surface for ~2 s
  for (uint16_t i = 0; i < 200; i++) {
    qtr.calibrate();      // calls read(QTRReadMode::On) internally
    delay(10);
  }
  Serial.println(F("Calibration complete"));
}

void loop() {
  // read raw reflectance (0–1023)
  qtr.read(sensorValues);

  // if you prefer calibrated (0–1000) instead:
  // qtr.readCalibrated(sensorValues);

  // print 15 values tab-separated
  for (uint8_t i = 0; i < NUM_SENSORS; i++) {
    Serial.print(sensorValues[i]);
    if (i < NUM_SENSORS - 1) Serial.print('\t');
  }
  Serial.println();

  delay(100);
}
