#include <QTRSensors.h>

// --- Pin Definitions ---
const uint8_t ODD_CTRL = 10;
const uint8_t EVEN_CTRL = 11;
const uint8_t NUM_SENSORS = 15;

// Your analog pins as defined in your PCB/Teensy setup
const uint8_t SENSOR_PINS[NUM_SENSORS] = {
  A0, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14
};

QTRSensors qtr;
uint16_t sensorValues[NUM_SENSORS];

void setup() {
  Serial.begin(115200);
  
  // 1. Power up the IR LED banks
  pinMode(ODD_CTRL, OUTPUT);
  pinMode(EVEN_CTRL, OUTPUT);
  digitalWrite(ODD_CTRL, HIGH);
  digitalWrite(EVEN_CTRL, HIGH);

  // 2. Initialize QTR Object
  qtr.setTypeAnalog();
  qtr.setSensorPins(SENSOR_PINS, NUM_SENSORS);
  
  // We use the manual pins above, so tell the library not to manage emitters
  qtr.setEmitterPin(QTRNoEmitterPin);

  Serial.println("--- QTRX-HD-15A Raw Value Test ---");
  delay(1000);
}

void loop() {
  // Read raw values (0 to 1023 on Teensy 4.1 by default)
  qtr.read(sensorValues);

  // Print values in a single row
  for (uint8_t i = 0; i < NUM_SENSORS; i++) {
    Serial.print(sensorValues[i]);
    Serial.print("\t");
  }
  Serial.println();

  delay(100); 
}