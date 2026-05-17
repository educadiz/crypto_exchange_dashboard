#include "ticker_manager.h"
#include <math.h>
#include <string.h>

// ticker_manager.cpp
// Gera e renderiza o ticker rolante (itens, setas e cores).

#include "display_utils.h"

#pragma region TickerInternals
namespace {
constexpr uint16_t kFeedCount = 8;
constexpr uint16_t kTickerBgColor   = rgb565(8, 14, 24);        // quase preto azulado
constexpr uint16_t kTickerBgLine    = rgb565(8, 18, 32);        // segunda linha (1px)
constexpr uint16_t kTickerBorder    = rgb565(36, 68, 96);       // borda sup/inf
constexpr uint16_t kTextColorWhite  = rgb565(228, 236, 248);    // branco
constexpr uint16_t kTextColorMuted  = rgb565(128, 148, 176);    // cinza claro
constexpr uint16_t kColorUp         = rgb565(43, 248, 2);       // verde brilhante — subida
constexpr uint16_t kColorDown       = rgb565(250, 34, 34);      // vermelho brilhante — descida
constexpr uint16_t kArrowGreen      = rgb565(0, 156, 0);        // verde puro — triângulo cima
constexpr uint16_t kArrowRed        = rgb565(200, 0, 0);        // vermelho puro — triângulo baixo
constexpr uint16_t kArrowNeutral      = rgb565(88, 92, 96);     // cinza — neutro
constexpr uint16_t kSeparator       = rgb565(24, 72, 96);       // azul-cinza separador
constexpr uint16_t kSpeedPxPerSec = 120;
constexpr uint16_t kInnerPadX  = 8;
constexpr uint16_t kItemGap    = 14;
constexpr uint16_t kArrowW     = 10;
constexpr uint16_t kTickerTextY = 5;
constexpr uint16_t kRowY       = 5;

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
float gScrollPosPx = 0.0f;
unsigned long gLastScrollMs = 0;

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

// drawArrow
// Desenha uma pequena seta para cima/baixo ou uma linha neutra quando
// `isNeutral` for verdadeiro. O template `BufferType` permite usar tanto
// `GFXcanvas16` quanto `TFT_eSprite` sem duplicar lógica.
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

  // Cor do símbolo por ativo
  uint16_t symColor = kTextColorWhite;
  const char* sym = item.symbol;
  if (sym[0] == 'B' && sym[1] == 'T' && sym[2] == 'C') {
    symColor = rgb565(244, 114, 120);  // laranja-BTC
  } else if (sym[0] == 'E' && sym[1] == 'T' && sym[2] == 'H') {
    symColor = rgb565(172, 130, 252);  // lilás-ETH
  } else if ((sym[0] == 'U' && sym[1] == 'S') || (sym[0] == 'U' && sym[1] == 'S' && sym[2] == 'D')) {
    symColor = rgb565(48, 168, 255);   // azul-elétrico
  }

  buffer.setTextSize(1);
  // símbolo do ativo (cor por ativo)
  buffer.setTextColor(symColor, kTickerBgColor);
  buffer.setCursor(symbolX, kRowY);
  buffer.print(item.symbol);

  buffer.setTextColor(kTextColorMuted, kTickerBgColor);
  buffer.setCursor(priceX, kRowY);
  buffer.print(item.priceText);

  buffer.setTextColor(item.isNeutral ? kTextColorMuted : (item.isPositive ? kColorUp : kColorDown), kTickerBgColor);
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

  if (gScrollPosPx >= (float)gContentWidth) {
    gScrollPosPx = fmodf(gScrollPosPx, (float)gContentWidth);
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
  gSprite->fillSprite(kTickerBgColor);
#else
  gCanvas->fillScreen(kTickerBgColor);
#endif
}

void drawBackground() {
#ifdef USE_TFT_ESPI
  gSprite->drawFastHLine(0, 0, gW, kTickerBorder);
  gSprite->drawFastHLine(0, gH - 1, gW, kTickerBorder);
  gSprite->drawFastHLine(0, 1, gW, kTickerBgLine);
#else
  gCanvas->drawFastHLine(0, 0, gW, kTickerBorder);
  gCanvas->drawFastHLine(0, gH - 1, gW, kTickerBorder);
  gCanvas->drawFastHLine(0, 1, gW, kTickerBgLine);
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
    buffer.setTextColor(kTextColorMuted, kTickerBgColor);
    buffer.setCursor(10, 5);
    buffer.print("Waiting for market data...");
    return;
  }

  const unsigned long nowMs = millis();
  if (gLastScrollMs == 0) {
    gLastScrollMs = nowMs;
  }

  const unsigned long deltaMs = nowMs - gLastScrollMs;
  gLastScrollMs = nowMs;

  if (gContentWidth > 0 && deltaMs > 0) {
    gScrollPosPx += (float)deltaMs * (float)kSpeedPxPerSec / 1000.0f;
    if (gScrollPosPx >= (float)gContentWidth) {
      gScrollPosPx = fmodf(gScrollPosPx, (float)gContentWidth);
    }
  }

  const int offset = (int)gScrollPosPx;
  int startX = gW - offset;
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
// Inicializa o ticker para o backend TFT_eSPI.
void tickerInit(TFT_eSPI* display, int x, int y, int w, int h) {
  gDisplay = display;
  gX = x;
  gY = y;
  gW = w;
  gH = h;
  gStartMs = millis();
  gScrollPosPx = 0.0f;
  gLastScrollMs = gStartMs;

  if (!gSprite) {
    gSprite = new TFT_eSprite(gDisplay);
    gSprite->setColorDepth(16);
    gSprite->setTextWrap(false);
    gSprite->createSprite(gW, gH);
  }

  gReady = true;
}
#else
// Inicializa o ticker para o backend Adafruit_GFX.
void tickerInit(Adafruit_GFX* display, int x, int y, int w, int h) {
  gDisplay = display;
  gX = x;
  gY = y;
  gW = w;
  gH = h;
  gStartMs = millis();
  gScrollPosPx = 0.0f;
  gLastScrollMs = gStartMs;

  if (!gCanvas) {
    gCanvas = new GFXcanvas16(gW, gH);
  }

  gReady = true;
}
#endif

// Atualiza os valores fonte usados pelo ticker. Não redesenha imediatamente;
// `renderFinancialTicker()` fará o push no próximo ciclo.
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

// Renderiza o ticker atual para o display (empurra sprite/canvas).
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