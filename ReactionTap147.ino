#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <Wire.h>
#include <Seeed_GFX.h>
#include "board/boards/XIAO_LCD_Board.h"
#include "driver/tft/Driver_JD9853A.h"
#include "panel/Panel_TFT.h"
#include "touch/Touch_AXS5106L.h"

static constexpr int8_t LCD_RST_PIN = 38;
static constexpr int8_t LCD_BL_PIN = 37;
static constexpr int16_t LCD_W = 172;
static constexpr int16_t LCD_H = 320;

Seeed_GFX display;
Touch_AXS5106L touch(-1, D7, Wire, LCD_W, LCD_H);

enum GameState : uint8_t { READY, WAITING, GO, RESULT, FALSE_START };
static GameState state = READY;
static bool wasTouching = false;
static volatile bool goTimingActive = false;
static volatile bool goTouchLatched = false;
static volatile uint32_t goTouchAtUs = 0;
static uint32_t goAtUs = 0;
static uint32_t waitUntil = 0;
static uint32_t reactionMs = 0;
static uint32_t bestMs = 0;
static uint16_t rounds = 0;

static uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return display.color565(r, g, b);
}

// The touch controller's active-low INT line can timestamp a tap while the
// LCD is busy drawing the full-screen green cue.
static void onTouchInterrupt() {
  if (goTimingActive && !goTouchLatched) {
    goTouchAtUs = micros();
    goTouchLatched = true;
  }
}

static void centeredText(const char *text, int16_t y, uint8_t size,
                         uint16_t color, uint16_t background) {
  display.setTextFont(1);
  display.setTextSize(size);
  display.setTextColor(color, background);
  const int16_t x = (LCD_W - display.textWidth(text)) / 2;
  display.setCursor(x, y);
  display.print(text);
}

static void drawFrame(uint16_t accent) {
  display.drawRect(5, 5, LCD_W - 10, LCD_H - 10, accent);
  display.drawRect(9, 9, LCD_W - 18, LCD_H - 18, rgb(11, 34, 49));
  display.fillRect(12, 18, 4, 18, accent);
  display.setTextFont(1);
  display.setTextSize(1);
  display.setTextColor(accent, TFT_BLACK);
  display.setCursor(22, 20);
  display.print("REFLEX // XIAO LAB");
  display.drawLine(12, 46, LCD_W - 12, 46, rgb(15, 52, 68));
}

static void drawReady() {
  const uint16_t cyan = rgb(0, 218, 255);
  display.fillScreen(TFT_BLACK);
  drawFrame(cyan);
  centeredText("REACTION", 91, 3, TFT_WHITE, TFT_BLACK);
  centeredText("SPEED TEST", 124, 2, cyan, TFT_BLACK);

  display.drawRoundRect(25, 176, 122, 55, rgb(0, 112, 143), 8);
  centeredText("TAP TO START", 197, 1, rgb(139, 242, 255), TFT_BLACK);
  centeredText("Wait for the screen", 258, 1, rgb(76, 120, 137), TFT_BLACK);
  centeredText("to turn GREEN", 274, 1, rgb(76, 120, 137), TFT_BLACK);

  display.setTextFont(1);
  display.setTextSize(1);
  display.setTextColor(rgb(54, 92, 108), TFT_BLACK);
  display.setCursor(18, 298);
  display.print("BEST: ");
  if (bestMs == 0) display.print("---");
  else { display.print(bestMs); display.print(" ms"); }
}

static void drawWaiting() {
  const uint16_t amber = rgb(255, 170, 0);
  display.fillScreen(rgb(8, 7, 3));
  drawFrame(amber);
  centeredText("WAIT...", 115, 3, amber, rgb(8, 7, 3));
  centeredText("Don't touch yet", 160, 1, rgb(195, 153, 70), rgb(8, 7, 3));
  centeredText("Tap as soon as", 221, 1, rgb(110, 105, 85), rgb(8, 7, 3));
  centeredText("the screen turns GREEN", 237, 1, rgb(110, 105, 85), rgb(8, 7, 3));
  display.drawCircle(LCD_W / 2, 190, 4 + (millis() / 90) % 13, rgb(76, 53, 12));
}

static void drawGo() {
  const uint16_t green = rgb(8, 255, 128);
  // Start immediately before the full-screen transition. The interrupt keeps
  // recording a tap even while fillScreen() blocks on the LCD's SPI transfer.
  noInterrupts();
  goTouchLatched = false;
  goTouchAtUs = 0;
  goAtUs = micros();
  goTimingActive = true;
  interrupts();

  display.fillScreen(green);
  display.drawRect(7, 7, LCD_W - 14, LCD_H - 14, rgb(0, 98, 48));
  display.drawRect(12, 12, LCD_W - 24, LCD_H - 24, rgb(0, 138, 68));
  centeredText("TAP!", 116, 5, rgb(0, 35, 23), green);
  centeredText("NOW", 181, 2, rgb(0, 70, 42), green);
  centeredText("TOUCH ANYWHERE", 269, 1, rgb(0, 82, 48), green);
}

static const char *rankFor(uint32_t ms) {
  if (ms < 180) return "LIGHTNING";
  if (ms < 250) return "ELITE REFLEX";
  if (ms < 350) return "NICE WORK";
  if (ms < 500) return "WARMING UP";
  return "TRY AGAIN";
}

static void drawResult() {
  const uint16_t cyan = rgb(0, 218, 255);
  display.fillScreen(TFT_BLACK);
  drawFrame(cyan);
  centeredText("YOUR TIME", 77, 1, rgb(96, 145, 162), TFT_BLACK);

  char timeText[12];
  snprintf(timeText, sizeof(timeText), "%lu", (unsigned long)reactionMs);
  centeredText(timeText, 112, 5, TFT_WHITE, TFT_BLACK);
  centeredText("MILLISECONDS", 166, 1, cyan, TFT_BLACK);
  centeredText(rankFor(reactionMs), 204, 2,
               reactionMs < 250 ? rgb(0, 255, 173) : rgb(255, 177, 64), TFT_BLACK);

  char bestText[28];
  if (bestMs == 0) snprintf(bestText, sizeof(bestText), "BEST  ---");
  else snprintf(bestText, sizeof(bestText), "BEST  %lu ms", (unsigned long)bestMs);
  centeredText(bestText, 250, 1, rgb(80, 140, 160), TFT_BLACK);

  char roundText[24];
  snprintf(roundText, sizeof(roundText), "ROUND  %u", rounds);
  centeredText(roundText, 276, 1, rgb(63, 100, 116), TFT_BLACK);
  centeredText("TAP TO PLAY AGAIN", 299, 1, cyan, TFT_BLACK);
}

static void drawFalseStart() {
  const uint16_t red = rgb(255, 54, 80);
  display.fillScreen(rgb(13, 2, 6));
  drawFrame(red);
  centeredText("TOO EARLY", 112, 3, red, rgb(13, 2, 6));
  centeredText("Wait for GREEN next time", 167, 1, rgb(168, 88, 99), rgb(13, 2, 6));
  centeredText("TAP TO RETRY", 267, 1, rgb(255, 152, 164), rgb(13, 2, 6));
}

static void startRound() {
  ++rounds;
  state = WAITING;
  waitUntil = millis() + random(1700, 4600);
  drawWaiting();
}

static void handleTap(uint32_t eventAtUs) {
  switch (state) {
    case READY:
    case RESULT:
    case FALSE_START:
      startRound();
      break;
    case WAITING:
      state = FALSE_START;
      drawFalseStart();
      break;
    case GO:
      reactionMs = (uint32_t)(eventAtUs - goAtUs) / 1000UL;
      noInterrupts();
      goTimingActive = false;
      goTouchLatched = false;
      interrupts();
      if (bestMs == 0 || reactionMs < bestMs) bestMs = reactionMs;
      state = RESULT;
      drawResult();
      break;
  }
}

void setup() {
  Serial.begin(115200);
  randomSeed(micros());

  if (!display.begin<Board_XIAO_1inch47_Touch_Display<LCD_RST_PIN, LCD_BL_PIN>,
                     Config_Seeed_1inch47_Touch_JD9853A>()) {
    Serial.println(display.lastResult().message);
    return;
  }

  if (!display.attachTouch(touch, display.panel().driver().bus())) {
    Serial.println(display.lastResult().message);
    return;
  }
  pinMode(D7, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(D7), onTouchInterrupt, FALLING);

  drawReady();
}

void loop() {
  const uint32_t now = millis();
  int32_t x = 0;
  int32_t y = 0;
  const bool touching = display.getTouch(&x, &y);
  const bool touchDown = touching && !wasTouching;
  wasTouching = touching;

  uint32_t irqTouchAtUs = 0;
  bool irqTouchDown = false;
  noInterrupts();
  if (goTouchLatched) {
    irqTouchAtUs = goTouchAtUs;
    goTouchLatched = false;
    irqTouchDown = true;
  }
  interrupts();

  if (state == WAITING && (int32_t)(now - waitUntil) >= 0) {
    state = GO;
    drawGo();
  }

  if (state == GO && irqTouchDown) {
    // Prefer the interrupt timestamp, which is captured during the display
    // transfer; otherwise use the normal touch-poll timestamp as a fallback.
    handleTap(irqTouchAtUs);
  } else if (touchDown) {
    handleTap(micros());
  }

  delay(8);
}
