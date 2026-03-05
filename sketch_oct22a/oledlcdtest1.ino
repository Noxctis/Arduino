#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Define the display width and height
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// Define the I2C address for the OLED (0x3C or 0x3D, depending on your module)
#define OLED_ADDR 0x3C

// Initialize the OLED display with I2C address
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

void setup() {
  // Start the display
  if(!display.begin(SSD1306_I2C_ADDRESS, OLED_ADDR)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  
  // Clear the buffer
  display.clearDisplay();
  
  // Set text color
  display.setTextColor(SSD1306_WHITE);
  
  // Set text size and cursor position
  display.setTextSize(1);    // Text size multiplier (1 = small, 2 = medium, etc.)
  display.setCursor(0, 0);   // Starting position (x, y)

  // Print text to display buffer
  display.println("Hello, World!");

  // Display the buffer
  display.display();
}

void loop() {
  // Nothing to do here
}
