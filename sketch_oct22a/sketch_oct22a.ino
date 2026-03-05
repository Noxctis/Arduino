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

// Memory Array to track past movements
int movementMemory[5];  // 0 = straight, 1 = left, 2 = right, 3 = hard left, 4 = hard right
int memoryIndex = 0;

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

  // Memory-based pattern handling: check for zig-zag or U-bend
  if (checkUBend()) {
    performUTurn(); // Execute U-turn if U-bend is detected
    return;
  }

  if (checkZigZag()) {
    performZigZag(); // Execute zig-zag adjustments
    return;
  }

  // Minor adjustments for 2nd and 4th sensors
  if (sensorValues[2] > threshold && sensorValues[1] > threshold && sensorValues[3] > threshold) {
    moveStraight();
    updateMemory(0);  // 0 for straight
  }
  else if (sensorValues[1] > threshold) { 
    turnLeft(); // Minor left adjustment
    updateMemory(1);  // 1 for left
  }
  else if (sensorValues[3] > threshold) { 
    turnRight(); // Minor right adjustment
    updateMemory(2);  // 2 for right
  }
  // Hard turns for 1st and 5th sensors
  else if (sensorValues[0] > threshold) {
    turnHARDLeft(); // Hard left turn
    updateMemory(3);  // 3 for hard left
  }
  else if (sensorValues[4] > threshold) {
    turnHARDRight(); // Hard right turn
    updateMemory(4);  // 4 for hard right
  }
  else {
    stop(); // Stop if no sensor detects the line
  }

  delay(10); // Small delay to stabilize
}

// Function to update the memory of past movements
void updateMemory(int move) {
  movementMemory[memoryIndex] = move;
  memoryIndex = (memoryIndex + 1) % 5;  // Circular buffer, always store the last 5 movements
}

// Function to check for a U-bend using memory
bool checkUBend() {
  // If the last 3 movements were minor adjustments or hard turns (left/right), a U-bend is likely
  if ((movementMemory[0] == 1 || movementMemory[0] == 3) &&  // Left or hard left
      (movementMemory[1] == 2 || movementMemory[1] == 4) &&  // Right or hard right
      (movementMemory[2] == 1 || movementMemory[2] == 3)) {  // Left or hard left
    return true;
  }
  return false;
}

// Function to check for zig-zag using memory
bool checkZigZag() {
  // Zig-zag detection: alternating left and right movements in the last 4 movements
  if ((movementMemory[0] == 1 && movementMemory[1] == 2) ||
      (movementMemory[0] == 2 && movementMemory[1] == 1)) {
    return true;
  }
  return false;
}

// Function to perform a U-turn
void performUTurn() {
  setMotor(AIN1, AIN2, PWMA, uTurnSpeed);     // Motor A forward
  setMotor(BIN1, BIN2, PWMB, -uTurnSpeed);    // Motor B backward
  delay(1000);  // Delay for a full U-turn
}

// Function to perform zig-zag adjustments
void performZigZag() {
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
