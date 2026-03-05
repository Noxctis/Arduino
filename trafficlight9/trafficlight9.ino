// Define pin numbers
int RedN = 2;
int YellowN = 3;
int GreenN = 4;

int RedS = 5;
int YellowS = 6;
int GreenS = 7;

int RedW = 8;
int YellowW = 9;
int GreenW = 10;

int RedE = 11;
int YellowE = 12;
int GreenE = 13;

void setup() {
  // Set all pins as OUTPUT
  pinMode(RedN, OUTPUT);
  pinMode(YellowN, OUTPUT);
  pinMode(GreenN, OUTPUT);
  pinMode(RedS, OUTPUT);
  pinMode(YellowS, OUTPUT);
  pinMode(GreenS, OUTPUT);
  pinMode(RedW, OUTPUT);
  pinMode(YellowW, OUTPUT);
  pinMode(GreenW, OUTPUT);
  pinMode(RedE, OUTPUT);
  pinMode(YellowE, OUTPUT);
  pinMode(GreenE, OUTPUT);
}

void loop() {
  digitalWrite(RedN, LOW);
  digitalWrite(YellowN, LOW);
  digitalWrite(GreenN, HIGH);
  
  digitalWrite(RedS, LOW);
  digitalWrite(YellowS, LOW);
  digitalWrite(GreenS, HIGH);
  
  digitalWrite(RedW, HIGH);
  digitalWrite(YellowW, LOW);
  digitalWrite(GreenW, LOW);
  
  digitalWrite(RedE, HIGH);
  digitalWrite(YellowE, LOW);
  digitalWrite(GreenE, LOW);

  delay(5000);
  
  digitalWrite(GreenN, LOW);
  digitalWrite(YellowN, HIGH);

  digitalWrite(GreenS, LOW);
  digitalWrite(YellowS, HIGH);

  delay(2000);

  digitalWrite(YellowN, LOW);
  digitalWrite(RedN, HIGH);

  digitalWrite(YellowS, LOW);
  digitalWrite(RedS, HIGH);

  delay(1000);

  digitalWrite(RedW, LOW);
  digitalWrite(YellowW, LOW);
  digitalWrite(GreenW, HIGH);

  digitalWrite(RedE, LOW);
  digitalWrite(YellowE, LOW);
  digitalWrite(GreenE, HIGH);

  delay(5000); 

  digitalWrite(GreenW, LOW);
  digitalWrite(YellowW, HIGH);

  digitalWrite(GreenE, LOW);
  digitalWrite(YellowE, HIGH);

  delay(2000);

  digitalWrite(YellowW, LOW);
  digitalWrite(RedW, HIGH);

  digitalWrite(YellowE, LOW);
  digitalWrite(RedE, HIGH);

  delay(1000);
}
