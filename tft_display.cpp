#include "tft_display.h"
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <math.h>
#include <string.h>

#define PIN_SCK   3
#define PIN_MOSI 45
#define PIN_CS   14
#define PIN_DC   47
#define PIN_RST  21

#define SCREEN_W 240
#define SCREEN_H 320

#define CARD_W 112
#define CARD_H 48

static Adafruit_ST7789* tft = nullptr;
static CryptoMarketData currentData;
static CryptoMarketData lastDrawnData;
static bool dashboardReady = false;

static char prevUsdText[20] = "";
static char prevBrlText[20] = "";
static char prevBtcText[20] = "";
static char prevEthText[20] = "";
static char prevWifiSsid[33] = "";
static char prevWifiIp[16] = "";
static bool prevWifi = false;
static bool prevWs = false;
static bool prevDataReady = false;
static uint32_t lastRenderedMessageCount = 0;
static unsigned long lastGraphTickMs = 0;

static int usdGraphX = 0;
static int btcGraphX = 0;
static int usdGraphPrevY = -1;
static int btcGraphPrevY = -1;

static uint16_t colorBg;
static uint16_t colorPanel;
static uint16_t colorPanel2;
static uint16_t colorLine;
static uint16_t colorText;
static uint16_t colorMuted;
static uint16_t colorAccent;
static uint16_t colorUsd;
static uint16_t colorBrl;
static uint16_t colorBtc;
static uint16_t colorEth;
static uint16_t colorOk;
static uint16_t colorDown;
static uint16_t colorWarn;

static uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  return ((uint16_t)(r & 0xF8) << 8) | ((uint16_t)(g & 0xFC) << 3) | (b >> 3);
}

static void initPalette() {
  colorBg     = rgb(9, 18, 30);
  colorPanel  = rgb(19, 31, 49);
  colorPanel2 = rgb(24, 41, 64);
  colorLine   = rgb(59, 92, 130);
  colorText   = rgb(231, 239, 246);
  colorMuted  = rgb(153, 173, 197);
  colorAccent = rgb(231, 239, 246);
  colorUsd    = rgb(89, 214, 137);
  colorBrl    = rgb(54, 169, 255);
  colorBtc    = rgb(255, 116, 80);
  colorEth    = rgb(180, 141, 255);
  colorOk     = rgb(86, 212, 126);
  colorDown   = rgb(255, 82, 82);
  colorWarn   = rgb(255, 167, 63);
}

static void fillText(char* out, size_t outSize, const char* text) {
  if (!out || outSize == 0) return;
  snprintf(out, outSize, "%s", text ? text : "");
}

static void formatFixed4(float value, char* out, size_t outSize) {
  snprintf(out, outSize, "%.4f", value);
}

static void formatCompact(float value, char* out, size_t outSize) {
  if (value >= 1000000.0f) {
    snprintf(out, outSize, "%.2fM", value / 1000000.0f);
  } else if (value >= 1000.0f) {
    snprintf(out, outSize, "%.2fK", value / 1000.0f);
  } else {
    snprintf(out, outSize, "%.4f", value);
  }
}

static void formatCompact3(float value, char* out, size_t outSize) {
  if (value >= 1000000.0f) {
    snprintf(out, outSize, "%.3fM", value / 1000000.0f);
  } else if (value >= 1000.0f) {
    snprintf(out, outSize, "%.3fK", value / 1000.0f);
  } else {
    snprintf(out, outSize, "%.4f", value);
  }
}

static void drawFrame(int x, int y, int w, int h, const char* title) {
  tft->fillRect(x, y, w, h, colorPanel);
  tft->drawRect(x, y, w, h, colorLine);
  if (title && title[0] != '\0') {
    tft->setTextSize(1);
    tft->setTextColor(colorMuted);
    tft->setCursor(x + 6, y + 4);
    tft->print(title);
  }
}

static void drawCenteredStatusText(int x, int y, int w, const char* text) {
  const int textW = (int)strlen(text) * 6;
  int textX = x + (w - textW) / 2;
  if (textX < x) textX = x;

  tft->fillRect(x, y, w, 8, colorPanel);
  tft->setTextSize(1);
  tft->setTextColor(colorText, colorPanel);
  tft->setCursor(textX, y);
  tft->print(text);
}

static void drawHeader() {
  tft->fillRect(0, 0, SCREEN_W, 26, colorPanel2);
  tft->drawFastHLine(0, 25, SCREEN_W, colorLine);
  tft->setTextSize(1);
  tft->setTextColor(colorAccent);
  tft->setCursor(8, 8);
  tft->print("Cadiz - Crypto Exchange Dashboard");

  const uint16_t netColor = currentData.wifiConnected ? colorOk : colorWarn;
  tft->fillCircle(SCREEN_W - 12, 13, 5, netColor);
  tft->drawCircle(SCREEN_W - 12, 13, 5, colorText);
}

static void drawStaticShell() {
  tft->fillScreen(colorBg);
  drawHeader();

  drawFrame(6, 32, CARD_W, CARD_H, "DOLAR");
  drawFrame(122, 32, CARD_W, CARD_H, "REAL");
  drawFrame(6, 84, CARD_W, CARD_H, "BITCOIN");
  drawFrame(122, 84, CARD_W, CARD_H, "ETHEREUM");

  drawFrame(6, 138, 228, 18, "");
  drawFrame(6, 162, 228, 74, "Tendencia USD x BRL");
  drawFrame(6, 240, 228, 74, "Tendencia BTC x BRL");

  tft->setTextSize(1);
  tft->setTextColor(colorMuted);
  tft->setCursor(12, 145);
  tft->print("WiFi");
  tft->setCursor(88, 145);
  tft->print("WebSocket");
  tft->setCursor(170, 145);
  tft->print("Updates");

  tft->drawFastHLine(10, 199, 220, rgb(46, 73, 102));
  tft->drawFastHLine(10, 277, 220, rgb(46, 73, 102));

  usdGraphX = 0;
  btcGraphX = 0;
  usdGraphPrevY = -1;
  btcGraphPrevY = -1;
}

static void drawMetricValue(int x, int y, int w, const char* value, const char* unit, uint16_t color, char* prevText, uint16_t* prevColor) {
  if (strcmp(prevText, value) == 0 && prevColor && *prevColor == color) {
    return;
  }

  snprintf(prevText, 20, "%s", value);
  if (prevColor) {
    *prevColor = color;
  }

  // Limpa apenas a faixa do valor, preservando o titulo do card.
  tft->fillRect(x + 1, y + 14, w - 2, 28, colorPanel);

  tft->setTextSize(2);
  tft->setTextColor(color, colorPanel);
  tft->setCursor(x + 4, y + 18);
  tft->print(value);

  tft->setTextSize(1);
  tft->setTextColor(colorText, colorPanel);
  const int unitX = x + w - (int)strlen(unit) * 6 - 6;
  tft->setCursor(unitX, y + 31);
  tft->print(unit);
}

static void drawStatusCard() {
  // Card central propositalmente vazio por enquanto.
  tft->fillRect(10, 144, 220, 8, colorPanel);
}

static float clampFloat(float v, float vmin, float vmax) {
  if (v < vmin) return vmin;
  if (v > vmax) return vmax;
  return v;
}

static int scaleToY(float value, float vmin, float vmax, int top, int bottom) {
  const float n = clampFloat((value - vmin) / (vmax - vmin), 0.0f, 1.0f);
  return bottom - (int)(n * (bottom - top));
}

static void plotTrendStep() {
  if (!currentData.hasUsdtBrl || !currentData.hasBtcUsdt) {
    return;
  }

  const int left = 10;
  const int width = 220;

  const int usdTop = 178;
  const int usdBottom = 227;
  const int btcTop = 256;
  const int btcBottom = 305;

  const float usdValue = currentData.usdtBrl;
  const float btcValueK = currentData.btcBrl / 1000.0f;

  if (usdGraphX == 0) {
    tft->fillRect(left, usdTop, width, usdBottom - usdTop + 1, colorPanel);
    tft->fillRect(left, btcTop, width, btcBottom - btcTop + 1, colorPanel);
    tft->drawFastHLine(left, (usdTop + usdBottom) / 2, width, rgb(46, 73, 102));
    tft->drawFastHLine(left, (btcTop + btcBottom) / 2, width, rgb(46, 73, 102));
    usdGraphPrevY = -1;
    btcGraphPrevY = -1;
  }

  tft->drawFastVLine(left + usdGraphX, usdTop, usdBottom - usdTop + 1, colorPanel);
  tft->drawFastVLine(left + btcGraphX, btcTop, btcBottom - btcTop + 1, colorPanel);

  const int usdY = scaleToY(usdValue, 4.0f, 7.5f, usdTop, usdBottom);
  const int btcY = scaleToY(btcValueK, 300.0f, 1000.0f, btcTop, btcBottom);

  if (usdGraphPrevY >= 0) {
    tft->drawLine(left + usdGraphX - 1, usdGraphPrevY, left + usdGraphX, usdY, colorBrl);
  } else {
    tft->drawPixel(left + usdGraphX, usdY, colorBrl);
  }

  if (btcGraphPrevY >= 0) {
    tft->drawLine(left + btcGraphX - 1, btcGraphPrevY, left + btcGraphX, btcY, colorBtc);
  } else {
    tft->drawPixel(left + btcGraphX, btcY, colorBtc);
  }

  usdGraphPrevY = usdY;
  btcGraphPrevY = btcY;

  usdGraphX++;
  btcGraphX++;
  if (usdGraphX >= width) usdGraphX = 0;
  if (btcGraphX >= width) btcGraphX = 0;
}

static bool shouldRefreshGraphs() {
  const unsigned long now = millis();
  if ((now - lastGraphTickMs) < 500UL) {
    return false;
  }
  lastGraphTickMs = now;
  return true;
}

static void renderIfChanged() {
  char usdText[20];
  char brlText[20];
  char btcText[20];
  char ethText[20];

  static uint16_t prevUsdColor = 0;
  static uint16_t prevBrlColor = 0;
  static uint16_t prevBtcColor = 0;
  static uint16_t prevEthColor = 0;

  fillText(usdText, sizeof(usdText), "1.0000");
  if (currentData.hasUsdtBrl) {
    formatFixed4(currentData.usdtBrl, brlText, sizeof(brlText));
  } else {
    fillText(brlText, sizeof(brlText), "--");
  }

  if (currentData.hasBtcUsdt) {
    formatCompact(currentData.btcUsdt, btcText, sizeof(btcText));
  } else {
    fillText(btcText, sizeof(btcText), "--");
  }

  if (currentData.hasEthUsdt) {
    formatCompact3(currentData.ethUsdt, ethText, sizeof(ethText));
  } else {
    fillText(ethText, sizeof(ethText), "--");
  }

  const uint16_t usdColor = colorUsd;
  const uint16_t brlColor = (!lastDrawnData.hasUsdtBrl || currentData.usdtBrl == lastDrawnData.usdtBrl)
    ? colorBrl
    : (currentData.usdtBrl > lastDrawnData.usdtBrl ? colorOk : colorDown);
  const uint16_t btcColor = (!lastDrawnData.hasBtcUsdt || currentData.btcUsdt == lastDrawnData.btcUsdt)
    ? colorBrl
    : (currentData.btcUsdt > lastDrawnData.btcUsdt ? colorOk : colorDown);
  const uint16_t ethColor = (!lastDrawnData.hasEthUsdt || currentData.ethUsdt == lastDrawnData.ethUsdt)
    ? colorBrl
    : (currentData.ethUsdt > lastDrawnData.ethUsdt ? colorOk : colorDown);

  drawMetricValue(6, 32, CARD_W, usdText, "USD", usdColor, prevUsdText, &prevUsdColor);
  drawMetricValue(122, 32, CARD_W, brlText, "BRL", brlColor, prevBrlText, &prevBrlColor);
  drawMetricValue(6, 84, CARD_W, btcText, "USD", btcColor, prevBtcText, &prevBtcColor);
  drawMetricValue(122, 84, CARD_W, ethText, "USD", ethColor, prevEthText, &prevEthColor);

  drawStatusCard();

  tft->fillRect(120, 164, 114, 10, colorPanel);
  tft->fillRect(120, 242, 114, 10, colorPanel);
  tft->setTextSize(1);
  tft->setTextColor(colorText, colorPanel);

  char usdGraphText[20];
  char btcGraphText[20];
  snprintf(usdGraphText, sizeof(usdGraphText), "%.4f BRL", currentData.usdtBrl);
  snprintf(btcGraphText, sizeof(btcGraphText), "%.2fK BRL", currentData.btcBrl / 1000.0f);

  tft->setCursor(124, 166);
  tft->print(usdGraphText);
  tft->setCursor(124, 244);
  tft->print(btcGraphText);

  lastDrawnData = currentData;
  dashboardReady = true;
}

void displayInit() {
  initPalette();

  pinMode(PIN_RST, OUTPUT);
  pinMode(PIN_DC, OUTPUT);
  pinMode(PIN_CS, OUTPUT);
  digitalWrite(PIN_CS, HIGH);
  digitalWrite(PIN_DC, HIGH);
  digitalWrite(PIN_RST, HIGH);
  delay(20);

  digitalWrite(PIN_RST, LOW);
  delay(100);
  digitalWrite(PIN_RST, HIGH);
  delay(200);

  tft = new Adafruit_ST7789(PIN_CS, PIN_DC, PIN_MOSI, PIN_SCK, PIN_RST);
  tft->init(SCREEN_W, SCREEN_H, SPI_MODE3);
  tft->setRotation(2);
  tft->sendCommand(0x36, (uint8_t[]){0x40}, 1);
  tft->invertDisplay(false);

  memset(&currentData, 0, sizeof(currentData));
  memset(&lastDrawnData, 0, sizeof(lastDrawnData));
  drawStaticShell();
  renderIfChanged();
}

void displaySetMarketData(const CryptoMarketData& data) {
  currentData = data;
}

void displayTick() {
  if (!tft) {
    return;
  }

  const bool statusChanged = (currentData.wifiConnected != prevWifi)
                          || (currentData.wsConnected != prevWs)
                          || (currentData.dataReady != prevDataReady)
                          || (strcmp(prevWifiSsid, currentData.wifiSsid) != 0)
                          || (strcmp(prevWifiIp, currentData.wifiIp) != 0);

  const bool marketChanged = (currentData.messageCount != lastRenderedMessageCount)
                          || (strcmp(prevUsdText, "1.0000") != 0)
                          || (strcmp(prevBrlText, "") == 0)
                          || (strcmp(prevBtcText, "") == 0)
                          || (strcmp(prevEthText, "") == 0);

  if (statusChanged || marketChanged || !dashboardReady) {
    prevWifi = currentData.wifiConnected;
    prevWs = currentData.wsConnected;
    prevDataReady = currentData.dataReady;
    snprintf(prevWifiSsid, sizeof(prevWifiSsid), "%s", currentData.wifiSsid);
    snprintf(prevWifiIp, sizeof(prevWifiIp), "%s", currentData.wifiIp);
    renderIfChanged();
    lastRenderedMessageCount = currentData.messageCount;
  }

  if (currentData.messageCount != 0 && shouldRefreshGraphs()) {
    plotTrendStep();
  }
}

void displaySetDuration(unsigned long) {}
void displayUseSimulation(bool) {}
void displaySetMeasurements(float, float, float, float) {}
