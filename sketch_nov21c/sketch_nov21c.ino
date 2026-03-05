#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 20, 4);

void setup() {
  lcd.begin(20, 4);
  lcd.backlight();
}

void type(const String& str, bool clearAfterPrinting) {
  int typespeed = 90;

  for (size_t i = 0; i < str.length(); i++) {
    lcd.print(str[i]);
    delay(typespeed);
  }

  if (clearAfterPrinting) {
    delay(1000);
    lcd.clear();
  }
}

void loop() {
  type("10 PM na pala shet", true);
  type("Tulong", true);
  type("/* Nag iiba ng anyo* Arrrggghhhhh", true);
  type("Dii mapigilanggg", false);
  lcd.setCursor(0, 1);
  type("magg-isippppppp", false);
  lcd.setCursor(0, 2);
  type("</3 </3 ://", true);
  type("Na bakaa saa tagal", false);
  lcd.setCursor(0, 1);
  type("mahulog ang loob moo", false);
  lcd.setCursor(0, 2);
  type("sa ibaaaaaaa :( :(", false);
  lcd.setCursor(0, 3);
  type("imissyou - CpEC", false);
  delay(2000);
  exit(0);  // This will stop the entire program
}
