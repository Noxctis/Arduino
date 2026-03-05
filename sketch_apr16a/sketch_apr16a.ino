int red = 5;
int yellow = 6;
int green = 9;
void setup()
{
  pinMode(red, OUTPUT);
  pinMode(yellow, OUTPUT);
  pinMode(green, OUTPUT);
}

void loop()
{
  digitalWrite(red, HIGH);
  delay(1000); // Wait for 1000 millisecond(s)
  digitalWrite(red, LOW);
  delay(1000); // Wait for 1000 millisecond(s)
  digitalWrite(yellow, HIGH);
  delay(1000); // Wait for 1000 millisecond(s)
  digitalWrite(yellow, LOW);
  delay(1000); // Wait for 1000 millisecond(s)
  digitalWrite(green, HIGH);
  delay(1000); // Wait for 1000 millisecond(s)
  digitalWrite(green, LOW);
  delay(1000); // Wait for 1000 millisecond(s)
}