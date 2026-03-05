#include "Arduino_LED_Matrix.h"

ArduinoLEDMatrix matrix;

uint8_t frame[8][12];
bool startedHeartbeatLine = false;
bool finishedHeartbeatLine = false;

void clearFrame() {
  for(int i=0;i<8;i++){
    for(int j=0;j<12;j++){
      frame[i][j] = 0;
    }
  }
}

void drawBitmap(const byte bmp[8]) {
  clearFrame();
  for(int row=0;row<8;row++) {
    for(int col=0;col<8;col++) {
      if((bmp[row] >> (7-col)) & 0x01) {
        frame[row][col+2] = 1;
      }
    }
  }
  matrix.renderBitmap(frame,8,12);
}

byte eye[8]     ={B00000000,B00111000,B00010000,B00010000,B00010000,B00010000,B00111000,B00000000};
byte bheart[8]  ={B01100110,B11111111,B11111111,B11111111,B01111110,B00111100,B00011000,B00000000};
byte sheart[8]  ={B00000000,B00000000,B01100110,B01111110,B00111100,B00011000,B00000000,B00000000};
const uint32_t smallheart[] {
  0x19,
	0x81f81f80,
	0xf0060000,
	66
};

byte youbmp[8]  ={B00000000,B01000010,B01000010,B01000010,B01000010,B01000010,B00111100,B00000000};

void setup() {
  Serial.begin(115200);
  matrix.begin();
}

void loop() {


  // 3. After the heartbeat line, run the beating heart + YOU
  matrix.loadFrame(smallheart);
  
  //drawBitmap(youbmp);  delay(2000);

  // 4. reset state so the whole process starts again
  startedHeartbeatLine  = false;
  finishedHeartbeatLine = false;
}
