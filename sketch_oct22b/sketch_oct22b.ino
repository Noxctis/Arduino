// Define sensor pins
const uint8_t sensorCount = 5; // Using 5 sensors
const uint8_t sensorPins[sensorCount] = {A4, A3, A2, A1, A0}; // SENSOR_1 to SENSOR_5 (left to right)
uint16_t sensorValues[sensorCount];

// Define motor control pins for TB6612FNG
#define AIN1 8      // Input 1 for Motor A check
#define AIN2 9     // Input 2 for Motor A check
#define PWMA 3      // PWM control for Motor A check
#define BIN1 11     // Input 1 for Motor B check 
#define BIN2 12     // Input 2 for Motor B check 
#define PWMB 5      // PWM control for Motor B check
#define STBY 10      // Standby pin check

#define LED_CTRL A5 // Optional LED control pin

// Motor speed control
int baseSpeed = 200; // Base speed for motors
int uTurnSpeed = 150; // Speed for U-turns
int zigZagSpeed = 180; // Speed for zig-zag adjustments

void setup() {
  Serial.begin(9600);

  // Initialize motor control pins
  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);
  pinMode(PWMA, OUTPUT);
  pinMode(BIN1, OUTPUT);
  pinMode(BIN2, OUTPUT);
  pinMode(PWMB, OUTPUT);
  pinMode(STBY, OUTPUT);

  // Set standby to HIGH to enable the motors
  digitalWrite(STBY, HIGH);

  // Optional LED control
  pinMode(LED_CTRL, OUTPUT);
}

void loop() {
  // Read the values of the 5 sensors
  for (uint8_t i = 0; i < sensorCount; i++) {
    sensorValues[i] = analogRead(sensorPins[i]);
  }

  // Debugging: print the sensor values
  for (uint8_t i = 0; i < sensorCount; i++) {
    Serial.print(sensorValues[i]);
    Serial.print("\t");
  }
  Serial.println();

  // Define threshold to detect the black line (adjust based on your track)
  int threshold = 500; // Adjust this value based on your track's lighting

  // Check for U-bend
  if (detectUBend()) {
    performUTurn(); // Execute U-turn if U-bend is detected
    return;
  }

  // Check for zig-zag lines
  if (detectZigZag()) {
    performZigZag(); // Execute zig-zag adjustments
    return;
  }

  // Minor adjustments for 2nd and 4th sensors
  if (sensorValues[2] > threshold && sensorValues[1] > threshold && sensorValues[3] > threshold) {
    moveStraight(); // Middle sensors detect the line
  }
  else if (sensorValues[1] > threshold) { 
    // 2nd sensor detects black: minor left adjustment
    turnLeft(); // Minor left adjustment
  }
  else if (sensorValues[3] > threshold) { 
    // 4th sensor detects black: minor right adjustment
    turnRight(); // Minor right adjustment
  } 
  // Hard turns for 1st and 5th sensors
  else if (sensorValues[0] > threshold) {
    // 1st sensor detects black: hard left turn
    turnHARDLeft(); // Hard left turn
  }
  else if (sensorValues[4] > threshold) {
    // 5th sensor detects black: hard right turn
    turnHARDRight(); // Hard right turn
  } 
  else {
    stop(); // Stop if no sensor detects the line
  }

  delay(10); // Small delay to stabilize
}

// Function to detect a U-bend
bool detectUBend() {
  int threshold = 500;
  // Detect U-bend when middle three sensors detect black and outer sensors detect white
  return (sensorValues[1] > threshold && sensorValues[2] > threshold && sensorValues[3] > threshold &&
          sensorValues[0] < threshold && sensorValues[4] < threshold);
}

// Function to perform a U-turn
void performUTurn() {
  setMotor(AIN1, AIN2, PWMA, uTurnSpeed);     // Motor A forward
  setMotor(BIN1, BIN2, PWMB, -uTurnSpeed);    // Motor B backward
  delay(1000);  // Delay for a full U-turn
}

// Function to detect zig-zag patterns
bool detectZigZag() {
  int threshold = 500;
  // Check for zig-zag patterns: alternating left and right sensors detecting black
  return ((sensorValues[0] > threshold && sensorValues[4] > threshold) || 
          (sensorValues[1] > threshold && sensorValues[3] > threshold));
}

// Function to perform zig-zag adjustments
void performZigZag() {
  // Adjust left then right in quick succession
  turnLeft();
  delay(200);  // Small delay for left turn
  turnRight();
  delay(200);  // Small delay for right turn
}

// Function to move the robot straight
void moveStraight() {
  setMotor(AIN1, AIN2, PWMA, baseSpeed); // Motor A forward
  setMotor(BIN1, BIN2, PWMB, baseSpeed); // Motor B forward
  digitalWrite(LED_CTRL, LOW); // LED off when on track
}

// Function to turn the robot left (minor adjustment)
void turnLeft() {
  setMotor(AIN1, AIN2, PWMA, baseSpeed / 2); // Motor A slower
  setMotor(BIN1, BIN2, PWMB, baseSpeed);     // Motor B full speed
  digitalWrite(LED_CTRL, HIGH); // Turn LED on when turning
}

// Function to turn the robot right (minor adjustment)
void turnRight() {
  setMotor(AIN1, AIN2, PWMA, baseSpeed);     // Motor A full speed
  setMotor(BIN1, BIN2, PWMB, baseSpeed / 2); // Motor B slower
  digitalWrite(LED_CTRL, HIGH); // Turn LED on when turning
}

// Function to turn the robot hard left
void turnHARDLeft() {
  setMotor(AIN1, AIN2, PWMA, 0); // Motor A stopped
  setMotor(BIN1, BIN2, PWMB, baseSpeed);     // Motor B full speed
  digitalWrite(LED_CTRL, HIGH); // Turn LED on when turning
}

// Function to turn the robot hard right
void turnHARDRight() {
  setMotor(AIN1, AIN2, PWMA, baseSpeed);     // Motor A full speed
  setMotor(BIN1, BIN2, PWMB, 0); // Motor B stopped
  digitalWrite(LED_CTRL, HIGH); // Turn LED on when turning
}

// Function to stop the robot
void stop() {
  setMotor(AIN1, AIN2, PWMA, 0);
  setMotor(BIN1, BIN2, PWMB, 0);
  digitalWrite(LED_CTRL, HIGH); // LED on when stopping
}

// Function to control the motor direction and speed
void setMotor(int in1, int in2, int pwm, int speed) {
  if (speed >= 0) {
    digitalWrite(in1, HIGH);
    digitalWrite(in2, LOW);
    analogWrite(pwm, speed);
  } else {
    digitalWrite(in1, LOW);
    digitalWrite(in2, HIGH);
    analogWrite(pwm, -speed);
  }
}
