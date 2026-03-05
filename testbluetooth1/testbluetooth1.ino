void setup() {
  pinMode(9, OUTPUT);        // Pull HC-05 Key Pin HIGH
  digitalWrite(9, HIGH);

  Serial.begin(9600);        // Connection to your Computer (USB)
  Serial1.begin(38400);      // Connection to HC-05 (Pins 0 & 1)
  
  Serial.println("Enter AT Commands:");
}

void loop() {
  // Read from Computer, Send to HC-05
  if (Serial.available()) {
    Serial1.write(Serial.read());
  }

  // Read from HC-05, Send to Computer
  if (Serial1.available()) {
    Serial.write(Serial1.read());
  }
}