#include <Wire.h>
#include <U8g2lib.h>

#define I2C_ADDRESS   0x3C
#define SCREEN_W      128
#define LINE_HEIGHT   20       // matches a ~6×18 font
#define MAX_WORDS     32
#define MAX_LINES      3       // up to 3 lines total

// full‐buffer SH1106 over hardware I²C
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// measure a C-string’s pixel width
inline uint8_t textW(const char *s) {
  return u8g2.getStrWidth(s);
}

// justify & draw one line of wc words at vertical position y
void justifyLine(char *words[], uint8_t wc, int16_t y) {
  int16_t totalW = 0;
  for (uint8_t i = 0; i < wc; i++) {
    totalW += textW(words[i]);
  }
  // single word → left-align
  if (wc < 2) {
    u8g2.drawStr(0, y, words[0]);
    return;
  }
  // full justify across the whole width
  int16_t gaps  = wc - 1;
  int16_t space = SCREEN_W - totalW;
  int16_t extra = space / gaps;
  int16_t rem   = space % gaps;

  int16_t x = 0;
  for (uint8_t i = 0; i < wc; i++) {
    u8g2.drawStr(x, y, words[i]);
    x += textW(words[i]);
    if (i < wc - 1) {
      x += extra + (i < rem ? 1 : 0);
    }
  }
}

// animated, word-by-word justified display with dynamic wrapping
void displayJustifiedAnimated(const char *msg, uint16_t wordDelay) {
  // tokenize
  static char buf[256];
  strncpy(buf, msg, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';

  static char* words[MAX_WORDS];
  uint8_t wcount = 0;
  for (char *tok = strtok(buf, " "); tok && wcount < MAX_WORDS; tok = strtok(nullptr, " ")) {
    words[wcount++] = tok;
  }
  if (!wcount) return;

  // reveal words one at a time
  for (uint8_t shown = 1; shown <= wcount; shown++) {
    // determine wrapping for the first shown words
    uint8_t lineStart[MAX_LINES] = {0};
    uint8_t lineCount[MAX_LINES] = {0};
    uint8_t linesUsed = 1;
    int16_t curW = 0;

    for (uint8_t w = 0; w < shown; w++) {
      uint8_t lw = textW(words[w]);
      // if first word on this line
      if (lineCount[linesUsed-1] == 0) {
        lineStart[linesUsed-1] = w;
        lineCount[linesUsed-1] = 1;
        curW = lw;
      } else {
        // would adding this word + 2px gap overflow?
        if (curW + 2 + lw <= SCREEN_W) {
          curW += 2 + lw;
          lineCount[linesUsed-1]++;
        } else if (linesUsed < MAX_LINES) {
          // wrap to next line
          linesUsed++;
          lineStart[linesUsed-1] = w;
          lineCount[linesUsed-1] = 1;
          curW = lw;
        } else {
          break;  // no more lines available
        }
      }
    }

    // draw all lines
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_helvR14_tr);

    for (uint8_t ln = 0; ln < linesUsed; ln++) {
      char **lnWords = &words[lineStart[ln]];
      uint8_t wc = lineCount[ln];
      int16_t y = LINE_HEIGHT + ln * LINE_HEIGHT;
      justifyLine(lnWords, wc, y);
    }

    u8g2.sendBuffer();
    delay(wordDelay);
  }
}

void setup() {
  u8g2.begin();
}

void loop() {
  displayJustifiedAnimated(
    "hindi ko naman yata ikamamatay",
    400   // ms between each word
  );
  delay(2000);  // pause before repeating
}