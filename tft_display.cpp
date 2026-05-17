#include "tft_display.h"
#include "ticker_manager.h"
#include <SPI.h>
#include <math.h>
#include <string.h>

// FreeRTOS primitives for thread-safety
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include <esp_task_wdt.h>

#define PIN_SCK   3
#define PIN_MOSI 45
#define PIN_CS   14
#define PIN_DC   47
#define PIN_RST  21

#define SCREEN_W 240
#define SCREEN_H 320

#define CARD_W 112
#define CARD_H 48

#ifdef USE_TFT_ESPI
static TFT_eSPI* tft = nullptr;
#else
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
static Adafruit_ST7789* tft = nullptr;
#endif
static CryptoMarketData currentData;
static CryptoMarketData lastDrawnData;
static bool dashboardReady = false;

#ifdef USE_TFT_ESPI
static TFT_eSprite* btcGraphCanvas = nullptr;
static TFT_eSprite* usdGraphCanvas = nullptr;
#else
static GFXcanvas16* btcGraphCanvas = nullptr;
static GFXcanvas16* usdGraphCanvas = nullptr;
#endif

// Mutex protecting access to currentData
static SemaphoreHandle_t dataMutex = NULL;

static char prevUsdText[20] = "";
static char prevBrlText[20] = "";
static char prevBtcText[20] = "";
static char prevEthText[20] = "";
static uint16_t prevUsdColor = 0;
static uint16_t prevBrlColor = 0;
static uint16_t prevBtcColor = 0;
static uint16_t prevEthColor = 0;
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
static const uint16_t GRAPH_POINTS = 180;
static float usdGraphHistory[GRAPH_POINTS];
static float btcGraphHistory[GRAPH_POINTS];
static uint16_t usdGraphCount = 0;
static uint16_t btcGraphCount = 0;
static uint16_t usdGraphHead = 0;
static uint16_t btcGraphHead = 0;
static uint32_t lastGraphSampleCount = 0;
static unsigned long lastGraphSampleMs = 0;
static bool graphsNeedRedraw = true;

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
static uint16_t colorFlashUpBg;
static uint16_t colorFlashDownBg;

static const unsigned long PRICE_FLASH_MS = 150UL;

enum PriceMetricIndex {
  PRICE_METRIC_BRL = 0,
  PRICE_METRIC_BTC = 1,
  PRICE_METRIC_ETH = 2,
  PRICE_METRIC_COUNT = 3
};

static unsigned long priceFlashUntilMs[PRICE_METRIC_COUNT] = {0, 0, 0};
static uint16_t priceFlashBg[PRICE_METRIC_COUNT] = {0, 0, 0};

// Widget enable flags (permitem parar renderização independente)
static bool widgetUsdEnabled = true;
static bool widgetBrlEnabled = true;
static bool widgetBtcEnabled = true;
static bool widgetEthEnabled = true;

void displayEnableWidgetUsd(bool enabled) { widgetUsdEnabled = enabled; }
void displayEnableWidgetBrl(bool enabled) { widgetBrlEnabled = enabled; }
void displayEnableWidgetBtc(bool enabled) { widgetBtcEnabled = enabled; }
void displayEnableWidgetEth(bool enabled) { widgetEthEnabled = enabled; }

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
  colorFlashUpBg = rgb(19, 45, 31);
  colorFlashDownBg = rgb(45, 20, 24);
}

static uint16_t blend565(uint16_t base, uint16_t accent, uint8_t accentWeight) {
  const uint8_t baseWeight = 255 - accentWeight;
  const uint8_t baseR = (base >> 11) & 0x1F;
  const uint8_t baseG = (base >> 5) & 0x3F;
  const uint8_t baseB = base & 0x1F;
  const uint8_t accentR = (accent >> 11) & 0x1F;
  const uint8_t accentG = (accent >> 5) & 0x3F;
  const uint8_t accentB = accent & 0x1F;

  const uint8_t outR = (uint8_t)((baseR * baseWeight + accentR * accentWeight) / 255);
  const uint8_t outG = (uint8_t)((baseG * baseWeight + accentG * accentWeight) / 255);
  const uint8_t outB = (uint8_t)((baseB * baseWeight + accentB * accentWeight) / 255);

  return (uint16_t)((outR << 11) | (outG << 5) | outB);
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
  // placeholder: old signature kept for static shell; real header uses data-aware version
  tft->fillRect(0, 0, SCREEN_W, 26, colorPanel2);
  tft->drawFastHLine(0, 25, SCREEN_W, colorLine);
  tft->setTextSize(1);
  tft->setTextColor(colorAccent);
  tft->setCursor(8, 8);
  tft->print("Cadiz - Crypto Exchange Dashboard");

  const uint16_t netColor = colorWarn;
  tft->fillCircle(SCREEN_W - 12, 13, 5, netColor);
  tft->drawCircle(SCREEN_W - 12, 13, 5, colorText);
}

static void drawHeaderStatic() {
  tft->fillRect(0, 0, SCREEN_W, 26, colorPanel2);
  tft->drawFastHLine(0, 25, SCREEN_W, colorLine);
  tft->setTextSize(1);
  tft->setTextColor(colorAccent);
  tft->setCursor(8, 8);
  tft->print("Cadiz - Crypto Exchange Dashboard");
}

static void updateHeaderStatus(const CryptoMarketData& data) {
  const uint16_t netColor = data.wifiConnected ? colorOk : colorWarn;
  tft->fillCircle(SCREEN_W - 12, 13, 5, colorPanel2);
  tft->fillCircle(SCREEN_W - 12, 13, 5, netColor);
  tft->drawCircle(SCREEN_W - 12, 13, 5, colorText);
}

static void drawStaticShell() {
  tft->fillScreen(colorBg);
  drawHeaderStatic();

  drawFrame(6, 32, CARD_W, CARD_H, "DOLAR");
  drawFrame(122, 32, CARD_W, CARD_H, "REAL");
  drawFrame(6, 84, CARD_W, CARD_H, "BITCOIN");
  drawFrame(122, 84, CARD_W, CARD_H, "ETHEREUM");

  drawFrame(6, 138, 228, 18, "");
  drawFrame(6, 162, 228, 74, "Tendencia ETH x BRL");
  drawFrame(6, 240, 228, 74, "Tendencia BTC x BRL");

  tft->drawFastHLine(10, 199, 220, rgb(46, 73, 102));
  tft->drawFastHLine(10, 277, 220, rgb(46, 73, 102));

  usdGraphX = 0;
  btcGraphX = 0;
  usdGraphPrevY = -1;
  btcGraphPrevY = -1;
}

static void drawMetricValue(int x, int y, int w, const char* value, const char* unit, uint16_t color, uint16_t bgColor, char* prevText, uint16_t* prevColor, bool forceRedraw) {
  if (!forceRedraw && strcmp(prevText, value) == 0 && prevColor && *prevColor == color) {
    return;
  }

  snprintf(prevText, 20, "%s", value);
  if (prevColor) {
    *prevColor = color;
  }

  // Limpa apenas a faixa do valor, preservando o titulo do card.
  tft->fillRect(x + 1, y + 14, w - 2, 28, bgColor);

  tft->setTextSize(2);
  tft->setTextColor(color, bgColor);
  tft->setCursor(x + 4, y + 18);
  tft->print(value);

  tft->setTextSize(1);
  tft->setTextColor(colorText, bgColor);
  const int unitX = x + w - (int)strlen(unit) * 6 - 6;
  tft->setCursor(unitX, y + 31);
  tft->print(unit);
}

static void paintPriceMetric(int metricIndex,
                             int x,
                             int y,
                             int w,
                             const char* value,
                             const char* unit,
                             uint16_t textColor,
                             uint16_t bgColor,
                             char* prevText,
                             uint16_t* prevColor,
                             bool forceRedraw) {
  drawMetricValue(x, y, w, value, unit, textColor, bgColor, prevText, prevColor, forceRedraw);
}

static void startPriceFlash(int metricIndex, bool isUp, unsigned long nowMs) {
  if (metricIndex < 0 || metricIndex >= PRICE_METRIC_COUNT) {
    return;
  }

  priceFlashUntilMs[metricIndex] = nowMs + PRICE_FLASH_MS;
  priceFlashBg[metricIndex] = isUp ? colorFlashUpBg : colorFlashDownBg;
}

static void refreshExpiredPriceFlashes(const CryptoMarketData& data, unsigned long nowMs) {
  const struct {
    int metricIndex;
    int x;
    int y;
    int w;
    const char* value;
    const char* unit;
    uint16_t textColor;
    char* prevText;
    uint16_t* prevColor;
    bool hasValue;
  } metrics[] = {
    {PRICE_METRIC_BRL, 122, 32, CARD_W, nullptr, "BRL", 0, prevBrlText, &prevBrlColor, data.hasUsdtBrl},
    {PRICE_METRIC_BTC, 6, 84, CARD_W, nullptr, "USD", 0, prevBtcText, &prevBtcColor, data.hasBtcUsdt},
    {PRICE_METRIC_ETH, 122, 84, CARD_W, nullptr, "USD", 0, prevEthText, &prevEthColor, data.hasEthUsdt},
  };

  char brlText[20];
  char btcText[20];
  char ethText[20];
  formatFixed4(data.usdtBrl, brlText, sizeof(brlText));
  formatCompact(data.btcUsdt, btcText, sizeof(btcText));
  formatCompact3(data.ethUsdt, ethText, sizeof(ethText));

  for (const auto& metric : metrics) {
    if (priceFlashUntilMs[metric.metricIndex] == 0 || nowMs < priceFlashUntilMs[metric.metricIndex]) {
      continue;
    }

    const char* value = nullptr;
    switch (metric.metricIndex) {
      case PRICE_METRIC_BRL: value = brlText; break;
      case PRICE_METRIC_BTC: value = btcText; break;
      case PRICE_METRIC_ETH: value = ethText; break;
      default: break;
    }

    if (!value || !metric.hasValue) {
      priceFlashUntilMs[metric.metricIndex] = 0;
      continue;
    }

    const uint16_t textColor = (metric.metricIndex == PRICE_METRIC_BRL)
      ? ((lastDrawnData.hasUsdtBrl && data.usdtBrl > lastDrawnData.usdtBrl) ? colorOk : colorDown)
      : (metric.metricIndex == PRICE_METRIC_BTC)
        ? ((lastDrawnData.hasBtcUsdt && data.btcUsdt > lastDrawnData.btcUsdt) ? colorOk : colorDown)
        : ((lastDrawnData.hasEthUsdt && data.ethUsdt > lastDrawnData.ethUsdt) ? colorOk : colorDown);

    paintPriceMetric(metric.metricIndex,
                     metric.x,
                     metric.y,
                     metric.w,
                     value,
                     metric.unit,
                     textColor,
                     colorPanel,
                     metric.prevText,
                     metric.prevColor,
                     true);

    priceFlashUntilMs[metric.metricIndex] = 0;
  }
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

static void pushGraphSample(float ethBrl, float usdBrl) {
  usdGraphHistory[usdGraphHead] = usdBrl;
  btcGraphHistory[btcGraphHead] = ethBrl;

  usdGraphHead = (uint16_t)((usdGraphHead + 1) % GRAPH_POINTS);
  btcGraphHead = (uint16_t)((btcGraphHead + 1) % GRAPH_POINTS);

  if (usdGraphCount < GRAPH_POINTS) usdGraphCount++;
  if (btcGraphCount < GRAPH_POINTS) btcGraphCount++;

  graphsNeedRedraw = true;
}

template <typename BufferType>
static void drawGraphArea(BufferType& gfx, int width, int height, const float* history, uint16_t count, uint16_t head, uint16_t lineColor, uint16_t gridColor) {
  gfx.fillRect(0, 0, width, height, colorPanel);
  gfx.drawRect(0, 0, width, height, colorLine);

  if (count == 0) {
    return;
  }

  float minValue = history[(head + GRAPH_POINTS - count) % GRAPH_POINTS];
  float maxValue = minValue;

  for (uint16_t i = 0; i < count; ++i) {
    const uint16_t index = (uint16_t)((head + GRAPH_POINTS - count + i) % GRAPH_POINTS);
    const float value = history[index];
    if (value < minValue) minValue = value;
    if (value > maxValue) maxValue = value;
  }

  if ((maxValue - minValue) < 0.0001f) {
    maxValue += 1.0f;
    minValue -= 1.0f;
  }

  const int innerLeft = 2;
  const int innerTop = 2;
  const int innerWidth = width - 4;
  const int innerHeight = height - 4;
  const int innerBottom = innerTop + innerHeight - 1;

  gfx.drawFastHLine(innerLeft, innerTop + innerHeight / 2, innerWidth, gridColor);

  int prevX = -1;
  int prevY = -1;

  for (uint16_t i = 0; i < count; ++i) {
    const uint16_t index = (uint16_t)((head + GRAPH_POINTS - count + i) % GRAPH_POINTS);
    const float value = history[index];
    const int x = (count == 1)
      ? innerLeft + innerWidth / 2
      : innerLeft + (int)((uint32_t)i * (uint32_t)(innerWidth - 1) / (uint32_t)(count - 1));
    const int y = scaleToY(value, minValue, maxValue, innerTop, innerBottom);

    if (prevX >= 0) {
      gfx.drawLine(prevX, prevY, x, y, lineColor);
    } else {
      gfx.drawPixel(x, y, lineColor);
    }

    prevX = x;
    prevY = y;
  }
}

static void updateRealtimeGraphs(const CryptoMarketData& data) {
  if (!data.hasUsdtBrl || !data.hasEthUsdt || !data.hasBtcUsdt) {
    return;
  }

  const unsigned long now = millis();
  const bool newTick = (data.messageCount != lastGraphSampleCount);
  const bool due = ((now - lastGraphSampleMs) >= 250UL);

  if (!newTick && !due && !graphsNeedRedraw) {
    return;
  }

  lastGraphSampleCount = data.messageCount;
  lastGraphSampleMs = now;

  pushGraphSample(data.ethBrl, data.btcBrl);

  if (btcGraphCanvas) {
    drawGraphArea(*btcGraphCanvas, 220, 50, btcGraphHistory, btcGraphCount, btcGraphHead, rgb(255, 214, 64), rgb(46, 73, 102));
    #ifdef USE_TFT_ESPI
    tft->startWrite();
    btcGraphCanvas->pushSprite(10, 178);
    tft->endWrite();
    #else
    tft->drawRGBBitmap(10, 178, btcGraphCanvas->getBuffer(), 220, 50);
    #endif
  }

  if (usdGraphCanvas) {
    drawGraphArea(*usdGraphCanvas, 220, 50, usdGraphHistory, usdGraphCount, usdGraphHead, colorBrl, rgb(46, 73, 102));
    #ifdef USE_TFT_ESPI
    tft->startWrite();
    usdGraphCanvas->pushSprite(10, 256);
    tft->endWrite();
    #else
    tft->drawRGBBitmap(10, 256, usdGraphCanvas->getBuffer(), 220, 50);
    #endif
  }

  graphsNeedRedraw = false;
}

static void renderIfChanged(const CryptoMarketData& data) {
  char usdText[20];
  char brlText[20];
  char btcText[20];
  char ethText[20];

  fillText(usdText, sizeof(usdText), "1.0000");
  if (data.hasUsdtBrl) {
    formatFixed4(data.usdtBrl, brlText, sizeof(brlText));
  } else {
    fillText(brlText, sizeof(brlText), "--");
  }

  if (data.hasBtcUsdt) {
    formatCompact(data.btcUsdt, btcText, sizeof(btcText));
  } else {
    fillText(btcText, sizeof(btcText), "--");
  }

  if (data.hasEthUsdt) {
    formatCompact3(data.ethUsdt, ethText, sizeof(ethText));
  } else {
    fillText(ethText, sizeof(ethText), "--");
  }

  const bool brlChanged = lastDrawnData.hasUsdtBrl && data.hasUsdtBrl && (data.usdtBrl != lastDrawnData.usdtBrl);
  const bool btcChanged = lastDrawnData.hasBtcUsdt && data.hasBtcUsdt && (data.btcUsdt != lastDrawnData.btcUsdt);
  const bool ethChanged = lastDrawnData.hasEthUsdt && data.hasEthUsdt && (data.ethUsdt != lastDrawnData.ethUsdt);

  const uint16_t usdColor = colorUsd;
  const uint16_t brlColor = (!lastDrawnData.hasUsdtBrl || data.usdtBrl == lastDrawnData.usdtBrl)
    ? colorBrl
    : (data.usdtBrl > lastDrawnData.usdtBrl ? colorOk : colorDown);
  const uint16_t btcColor = (!lastDrawnData.hasBtcUsdt || data.btcUsdt == lastDrawnData.btcUsdt)
    ? colorBrl
    : (data.btcUsdt > lastDrawnData.btcUsdt ? colorOk : colorDown);
  const uint16_t ethColor = (!lastDrawnData.hasEthUsdt || data.ethUsdt == lastDrawnData.ethUsdt)
    ? colorBrl
    : (data.ethUsdt > lastDrawnData.ethUsdt ? colorOk : colorDown);

  if (widgetUsdEnabled) paintPriceMetric(-1, 6, 32, CARD_W, usdText, "USD", usdColor, colorPanel, prevUsdText, &prevUsdColor, false);
  if (widgetBrlEnabled) {
    if (brlChanged) startPriceFlash(PRICE_METRIC_BRL, data.usdtBrl > lastDrawnData.usdtBrl, millis());
    paintPriceMetric(PRICE_METRIC_BRL, 122, 32, CARD_W, brlText, "BRL", brlColor, priceFlashUntilMs[PRICE_METRIC_BRL] ? priceFlashBg[PRICE_METRIC_BRL] : colorPanel, prevBrlText, &prevBrlColor, brlChanged);
  }
  if (widgetBtcEnabled) {
    if (btcChanged) startPriceFlash(PRICE_METRIC_BTC, data.btcUsdt > lastDrawnData.btcUsdt, millis());
    paintPriceMetric(PRICE_METRIC_BTC, 6, 84, CARD_W, btcText, "USD", btcColor, priceFlashUntilMs[PRICE_METRIC_BTC] ? priceFlashBg[PRICE_METRIC_BTC] : colorPanel, prevBtcText, &prevBtcColor, btcChanged);
  }
  if (widgetEthEnabled) {
    if (ethChanged) startPriceFlash(PRICE_METRIC_ETH, data.ethUsdt > lastDrawnData.ethUsdt, millis());
    paintPriceMetric(PRICE_METRIC_ETH, 122, 84, CARD_W, ethText, "USD", ethColor, priceFlashUntilMs[PRICE_METRIC_ETH] ? priceFlashBg[PRICE_METRIC_ETH] : colorPanel, prevEthText, &prevEthColor, ethChanged);
  }

  tft->fillRect(120, 164, 114, 10, colorPanel);
  tft->fillRect(120, 242, 114, 10, colorPanel);
  tft->setTextSize(1);
  tft->setTextColor(colorText, colorPanel);

  char ethGraphText[20];
  char btcGraphText[20];
  snprintf(ethGraphText, sizeof(ethGraphText), "%.2fK BRL", data.ethBrl / 1000.0f);
  snprintf(btcGraphText, sizeof(btcGraphText), "%.2fK BRL", data.btcBrl / 1000.0f);

  tft->setCursor(124, 166);
  tft->print(ethGraphText);
  tft->setCursor(124, 244);
  tft->print(btcGraphText);

  lastDrawnData = data;
  dashboardReady = true;
}

void displayInit() {
  initPalette();

  // criar mutex para proteger currentData
  if (!dataMutex) {
    dataMutex = xSemaphoreCreateMutex();
  }

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

  // Cria e inicializa o objeto de display conforme backend selecionado
#ifdef USE_TFT_ESPI
  tft = new TFT_eSPI();
  tft->init();
  tft->setRotation(2);
#else
  tft = new Adafruit_ST7789(PIN_CS, PIN_DC, PIN_MOSI, PIN_SCK, PIN_RST);
  tft->init(SCREEN_W, SCREEN_H, SPI_MODE3);
  tft->setRotation(2);
  tft->sendCommand(0x36, (uint8_t[]){0x40}, 1);
  tft->invertDisplay(false);
#endif

  if (!btcGraphCanvas) {
    #ifdef USE_TFT_ESPI
    btcGraphCanvas = new TFT_eSprite(tft);
    btcGraphCanvas->setColorDepth(16);
    btcGraphCanvas->setTextWrap(false);
    btcGraphCanvas->createSprite(220, 50);
    #else
    btcGraphCanvas = new GFXcanvas16(220, 50);
    #endif
  }
  if (!usdGraphCanvas) {
    #ifdef USE_TFT_ESPI
    usdGraphCanvas = new TFT_eSprite(tft);
    usdGraphCanvas->setColorDepth(16);
    usdGraphCanvas->setTextWrap(false);
    usdGraphCanvas->createSprite(220, 50);
    #else
    usdGraphCanvas = new GFXcanvas16(220, 50);
    #endif
  }

  memset(&currentData, 0, sizeof(currentData));
  memset(&lastDrawnData, 0, sizeof(lastDrawnData));
  memset(usdGraphHistory, 0, sizeof(usdGraphHistory));
  memset(btcGraphHistory, 0, sizeof(btcGraphHistory));
  usdGraphCount = 0;
  btcGraphCount = 0;
  usdGraphHead = 0;
  btcGraphHead = 0;
  lastGraphSampleCount = 0;
  lastGraphSampleMs = 0;
  graphsNeedRedraw = true;
  drawStaticShell();
  tickerInit(tft, 6, 138, 228, 18);
  tickerSetMarketData(currentData);
  renderFinancialTicker();
  updateHeaderStatus(currentData);
  renderIfChanged(currentData);
}

void displaySetMarketData(const CryptoMarketData& data) {
  if (dataMutex) {
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
      currentData = data;
      xSemaphoreGive(dataMutex);
    }
  } else {
    currentData = data;
  }
}
void displayTick() {
  if (!tft) {
    return;
  }
  CryptoMarketData safeData;
  if (dataMutex && xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
    safeData = currentData;
    xSemaphoreGive(dataMutex);
  } else {
    // fallback copy without mutex (unlikely)
    safeData = currentData;
  }

  const bool statusChanged = (safeData.wifiConnected != prevWifi)
                          || (safeData.wsConnected != prevWs)
                          || (safeData.dataReady != prevDataReady)
                          || (strcmp(prevWifiSsid, safeData.wifiSsid) != 0)
                          || (strcmp(prevWifiIp, safeData.wifiIp) != 0);

  const bool marketChanged = (safeData.messageCount != lastRenderedMessageCount)
                          || (strcmp(prevUsdText, "1.0000") != 0)
                          || (strcmp(prevBrlText, "") == 0)
                          || (strcmp(prevBtcText, "") == 0)
                          || (strcmp(prevEthText, "") == 0);

  if (statusChanged) {
    prevWifi = safeData.wifiConnected;
    prevWs = safeData.wsConnected;
    prevDataReady = safeData.dataReady;
    snprintf(prevWifiSsid, sizeof(prevWifiSsid), "%s", safeData.wifiSsid);
    snprintf(prevWifiIp, sizeof(prevWifiIp), "%s", safeData.wifiIp);
    updateHeaderStatus(safeData);
  }

  if (marketChanged || !dashboardReady) {
    renderIfChanged(safeData);
    lastRenderedMessageCount = safeData.messageCount;
  }

  updateRealtimeGraphs(safeData);
  tickerSetMarketData(safeData);
  renderFinancialTicker();
  refreshExpiredPriceFlashes(safeData, millis());
}

void displaySetDuration(unsigned long) {}
void displayUseSimulation(bool) {}
void displaySetMeasurements(float, float, float, float) {}
