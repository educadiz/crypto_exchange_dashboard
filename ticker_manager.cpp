#include "ticker_manager.h"
#include <math.h>
#include <string.h>

namespace {

constexpr uint8_t kFeedCount = 8;
constexpr uint16_t kTickerBg = 0x08A2;
constexpr uint16_t kTickerBg2 = 0x0A20;
constexpr uint16_t kTickerLine = 0x2C4E;
constexpr uint16_t kTextWhite = 0xF7BE;
constexpr uint16_t kTextMuted = 0xBDF7;
constexpr uint16_t kGreen = 0x07E0;
constexpr uint16_t kRed = 0xF800;
constexpr uint16_t kArrowGreen = 0x04A0;
constexpr uint16_t kArrowRed = 0xC800;
constexpr uint16_t kArrowNeutral = 0x8C51;
constexpr uint16_t kSeparator = 0x3D6B;
constexpr uint16_t kSpeedPxPerSec = 84;
constexpr uint16_t kInnerPadX = 8;
constexpr uint16_t kItemGap = 14;
constexpr uint16_t kArrowW = 10;
constexpr uint16_t kTickerTextY = 5;
constexpr uint16_t kRowY = 5;

enum class FeedKind : uint8_t {
  BtcUsdt,
  EthUsdt,
  UsdtBrl,
  BtcBrl,
  EthBrl,
  BtcUsdAlias,
  EthUsdAlias,
  UsdtBrlAlias,
};

struct FeedSpec {
  const char* symbol;
  FeedKind kind;
  bool priceIsBrl;
};

constexpr FeedSpec kFeedSpecs[kFeedCount] = {
  {"BTC/USDT", FeedKind::BtcUsdt, false},
  {"ETH/USDT", FeedKind::EthUsdt, false},
  {"USD/BRL",  FeedKind::UsdtBrl,  true},
  {"BTC/BRL",  FeedKind::BtcBrl,   true},
  {"ETH/BRL",  FeedKind::EthBrl,   true},
  {"BTC/USD",  FeedKind::BtcUsdAlias, false},
  {"ETH/USD",  FeedKind::EthUsdAlias, false},
  {"USDT/BRL", FeedKind::UsdtBrlAlias, true},
};

#ifdef USE_TFT_ESPI
TFT_eSPI* gDisplay = nullptr;
TFT_eSprite* gSprite = nullptr;
#else
Adafruit_GFX* gDisplay = nullptr;
GFXcanvas16* gCanvas = nullptr;
#endif

int gX = 0;
int gY = 0;
int gW = 0;
int gH = 0;
bool gReady = false;
unsigned long gStartMs = 0;
uint32_t gLastSourceMessageCount = 0;
float gLastValues[kFeedCount] = {0.0f};
TickerItem gItems[kFeedCount];
uint8_t gItemCount = 0;
uint16_t gContentWidth = 0;

uint16_t textWidth(const char* text) {
  return text ? (uint16_t)strlen(text) * 6U : 0U;
}

float sourcePrice(const CryptoMarketData& data, FeedKind kind) {
  switch (kind) {
    case FeedKind::BtcUsdt: return data.btcUsdt;
    case FeedKind::EthUsdt: return data.ethUsdt;
    case FeedKind::UsdtBrl: return data.usdtBrl;
    case FeedKind::BtcBrl: return data.btcBrl;
    case FeedKind::EthBrl: return data.ethBrl;
    case FeedKind::BtcUsdAlias: return data.btcUsdt;
    case FeedKind::EthUsdAlias: return data.ethUsdt;
    case FeedKind::UsdtBrlAlias: return data.usdtBrl;
  }
  return 0.0f;
}

void formatPrice(float price, bool isBrl, char* out, size_t outSize) {
  if (isBrl) {
    snprintf(out, outSize, "%.2f", price);
  } else if (price >= 100000.0f) {
    snprintf(out, outSize, "%.1fK", price / 1000.0f);
  } else if (price >= 1000.0f) {
    snprintf(out, outSize, "%.2fK", price / 1000.0f);
  } else {
    snprintf(out, outSize, "%.2f", price);
  }
}

void formatChange(float changePercent, char* out, size_t outSize) {
  if (fabsf(changePercent) < 0.0001f) {
    snprintf(out, outSize, "0.00%%");
  } else {
    snprintf(out, outSize, "%c%.2f%%", changePercent > 0.0f ? '+' : '-', fabsf(changePercent));
  }
}

template <typename BufferType>
void drawArrow(BufferType& buffer, int cx, int cy, bool isPositive, bool isNeutral) {
  const uint16_t color = isNeutral ? kArrowNeutral : (isPositive ? kArrowGreen : kArrowRed);
  if (isNeutral) {
    buffer.drawFastHLine(cx - 4, cy, 8, color);
    return;
  }

  if (isPositive) {
    buffer.fillTriangle(cx, cy - 4, cx - 4, cy + 3, cx + 4, cy + 3, color);
  } else {
    buffer.fillTriangle(cx, cy + 4, cx - 4, cy - 3, cx + 4, cy - 3, color);
  }
}

template <typename BufferType>
void drawItem(BufferType& buffer, int x, const TickerItem& item) {
  const int centerY = 9;
  const int symbolX = x + kInnerPadX + kArrowW;
  const int priceX = symbolX + (int)textWidth(item.symbol) + 10;
  const int changeX = priceX + (int)textWidth(item.priceText) + 10;

  drawArrow(buffer, x + kInnerPadX + 4, centerY, item.isPositive, item.isNeutral);

  buffer.setTextSize(1);
  buffer.setTextColor(kTextWhite, kTickerBg);
  buffer.setCursor(symbolX, kRowY);
  buffer.print(item.symbol);

  buffer.setTextColor(kTextMuted, kTickerBg);
  buffer.setCursor(priceX, kRowY);
  buffer.print(item.priceText);

  buffer.setTextColor(item.isNeutral ? kTextMuted : (item.isPositive ? kGreen : kRed), kTickerBg);
  buffer.setCursor(changeX, kRowY);
  buffer.print(item.changeText);
}

void recomputeContentWidth() {
  gContentWidth = 0;
  for (uint8_t i = 0; i < gItemCount; ++i) {
    gContentWidth = (uint16_t)(gContentWidth + gItems[i].widthPx + kItemGap);
  }
  if (gContentWidth == 0) {
    gContentWidth = (uint16_t)gW;
  }
}

void buildItems(const CryptoMarketData& data) {
  gItemCount = kFeedCount;
  for (uint8_t i = 0; i < kFeedCount; ++i) {
    const FeedSpec& spec = kFeedSpecs[i];
    const float price = sourcePrice(data, spec.kind);
    const float prev = gLastValues[i];
    const bool hasPrev = prev > 0.0f;
    const bool isNeutral = !hasPrev || fabsf(price - prev) < 0.0001f;
    const bool isPositive = !hasPrev ? true : (price >= prev);
    const float changePercent = (!hasPrev || isNeutral) ? 0.0f : ((price - prev) / prev) * 100.0f;

    snprintf(gItems[i].symbol, sizeof(gItems[i].symbol), "%s", spec.symbol);

    char priceText[16];
    char changeText[16];
    formatPrice(price, spec.priceIsBrl, priceText, sizeof(priceText));
    formatChange(changePercent, changeText, sizeof(changeText));

    snprintf(gItems[i].priceText, sizeof(gItems[i].priceText), "%s", priceText);
    snprintf(gItems[i].changeText, sizeof(gItems[i].changeText), "%s", changeText);

    gItems[i].price = price;
    gItems[i].changePercent = changePercent;
    gItems[i].isPositive = isPositive;
    gItems[i].isNeutral = isNeutral;
    gItems[i].widthPx = (uint16_t)(kInnerPadX * 2 + kArrowW + textWidth(gItems[i].symbol) + 10 + textWidth(gItems[i].priceText) + 10 + textWidth(gItems[i].changeText) + 12);

    gLastValues[i] = price;
  }

  recomputeContentWidth();
}

void clearBuffer() {
#ifdef USE_TFT_ESPI
  gSprite->fillSprite(kTickerBg);
#else
  gCanvas->fillScreen(kTickerBg);
#endif
}

void drawBackground() {
#ifdef USE_TFT_ESPI
  gSprite->drawFastHLine(0, 0, gW, kTickerLine);
  gSprite->drawFastHLine(0, gH - 1, gW, kTickerLine);
  gSprite->drawFastHLine(0, 1, gW, kTickerBg2);
#else
  gCanvas->drawFastHLine(0, 0, gW, kTickerLine);
  gCanvas->drawFastHLine(0, gH - 1, gW, kTickerLine);
  gCanvas->drawFastHLine(0, 1, gW, kTickerBg2);
#endif
}

template <typename BufferType>
void drawSeparator(BufferType& buffer, int x) {
  buffer.drawFastVLine(x, 4, gH - 8, kSeparator);
}

template <typename BufferType>
void renderTickerToBuffer(BufferType& buffer) {
  buffer.setTextWrap(false);

  if (gItemCount == 0) {
    buffer.setTextSize(1);
    buffer.setTextColor(kTextMuted, kTickerBg);
    buffer.setCursor(10, 5);
    buffer.print("Waiting for market data...");
    return;
  }

  const unsigned long elapsed = millis() - gStartMs;
  const uint16_t cycleWidth = gContentWidth == 0 ? (uint16_t)gW : gContentWidth;
  const uint16_t offset = (uint16_t)(((elapsed * kSpeedPxPerSec) / 1000UL) % cycleWidth);

  int startX = (int)gW - (int)offset;
  while (startX < gW) {
    int itemX = startX;
    for (uint8_t i = 0; i < gItemCount; ++i) {
      drawItem(buffer, itemX, gItems[i]);
      itemX += gItems[i].widthPx;
      if (i + 1 < gItemCount) {
        drawSeparator(buffer, itemX + (kItemGap / 2));
      }
      itemX += kItemGap;
    }
    startX += (int)gContentWidth;
  }
}

} // namespace

#ifdef USE_TFT_ESPI
void tickerInit(TFT_eSPI* display, int x, int y, int w, int h) {
  gDisplay = display;
  gX = x;
  gY = y;
  gW = w;
  gH = h;
  gStartMs = millis();

  if (!gSprite) {
    gSprite = new TFT_eSprite(gDisplay);
    gSprite->setColorDepth(16);
    gSprite->setTextWrap(false);
    gSprite->createSprite(gW, gH);
  }

  gReady = true;
}
#else
void tickerInit(Adafruit_GFX* display, int x, int y, int w, int h) {
  gDisplay = display;
  gX = x;
  gY = y;
  gW = w;
  gH = h;
  gStartMs = millis();

  if (!gCanvas) {
    gCanvas = new GFXcanvas16(gW, gH);
  }

  gReady = true;
}
#endif

void tickerSetMarketData(const CryptoMarketData& data) {
  if (!gReady) {
    return;
  }

  if (gItemCount > 0 && data.messageCount == gLastSourceMessageCount) {
    return;
  }

  gLastSourceMessageCount = data.messageCount;
  buildItems(data);
}

void renderFinancialTicker() {
  if (!gReady || !gDisplay) {
    return;
  }

#ifdef USE_TFT_ESPI
  if (!gSprite) return;

  clearBuffer();
  drawBackground();
  renderTickerToBuffer(*gSprite);
  gDisplay->startWrite();
  gSprite->pushSprite(gX, gY);
  gDisplay->endWrite();
#else
  if (!gCanvas) return;

  clearBuffer();
  drawBackground();
  renderTickerToBuffer(*gCanvas);
  gDisplay->drawRGBBitmap(gX, gY, gCanvas->getBuffer(), gW, gH);
#endif
}