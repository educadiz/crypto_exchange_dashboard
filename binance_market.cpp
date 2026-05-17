#include "binance_market.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include <cstring>

// ======================= CONFIG =========================
static const char* BINANCE_HOST = "stream.binance.com";
static const uint16_t BINANCE_PORT = 9443;

// ✔ STREAMS CORRETOS (ETH FIXADO)
static const char* BINANCE_PATH =
"/stream?streams="
"btcusdt@miniTicker/"
"usdtbrl@miniTicker/"
"ethusdt@miniTicker";

BinanceMarket* BinanceMarket::instance = nullptr;

// ======================= CONSTRUTOR ======================
BinanceMarket::BinanceMarket()
  : wsStarted(false),
    lastWifiAttemptMs(0),
    lastWsAttemptMs(0),
    wifiRetryMs(10000UL),
    wsRetryMs(5000UL) {

  std::memset(&snapshot, 0, sizeof(snapshot));
  std::memset(wifiSsid, 0, sizeof(wifiSsid));
  std::memset(wifiPassword, 0, sizeof(wifiPassword));
}

// ======================= INIT WIFI ======================
void BinanceMarket::begin(const char* ssid, const char* password) {
  std::snprintf(wifiSsid, sizeof(wifiSsid), "%s", ssid ? ssid : "");
  std::snprintf(wifiPassword, sizeof(wifiPassword), "%s", password ? password : "");

  instance = this;

  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);
  WiFi.begin(wifiSsid, wifiPassword);

  connectWiFi();
}

// ======================= LOOP ============================
void BinanceMarket::loop() {
  const unsigned long now = millis();

  snapshot.wifiConnected = (WiFi.status() == WL_CONNECTED);

  if (!snapshot.wifiConnected) {
    snapshot.wsConnected = false;

    if (wsStarted) {
      webSocket.disconnect();
      wsStarted = false;
    }

    if (now - lastWifiAttemptMs > wifiRetryMs) {
      connectWiFi();
    }
    return;
  }

  if (!wsStarted) {
    if (now - lastWsAttemptMs > wsRetryMs) {
      connectWebSocket();
    }
  }

  // 🔴 obrigatório
  webSocket.loop();
}

// ======================= WIFI RECONNECT ==================
void BinanceMarket::connectWiFi() {
  lastWifiAttemptMs = millis();
  WiFi.disconnect(false, false);
  WiFi.begin(wifiSsid, wifiPassword);
}

// ======================= WS CONNECT ======================
void BinanceMarket::connectWebSocket() {
  lastWsAttemptMs = millis();
  wsStarted = false;

  snapshot.reconnectCount++;

  webSocket.onEvent(BinanceMarket::handleWebSocketEventStatic);
  webSocket.setReconnectInterval(3000);

  webSocket.beginSSL(
    BINANCE_HOST,
    BINANCE_PORT,
    BINANCE_PATH
  );
}

// ======================= CALLBACK STATIC =================
void BinanceMarket::handleWebSocketEventStatic(WStype_t type, uint8_t* payload, size_t length) {
  if (instance) {
    instance->handleWebSocketEvent(type, payload, length);
  }
}

// ======================= CALLBACK ========================
void BinanceMarket::handleWebSocketEvent(WStype_t type, uint8_t* payload, size_t length) {

  switch (type) {

    case WStype_CONNECTED:
      wsStarted = true;
      snapshot.wsConnected = true;
      snapshot.lastUpdateMs = millis();
      Serial.println("[WS] CONNECTED");
      break;

    case WStype_DISCONNECTED:
      wsStarted = false;
      snapshot.wsConnected = false;
      Serial.println("[WS] DISCONNECTED");
      lastWsAttemptMs = millis();
      break;

    case WStype_ERROR:
      wsStarted = false;
      snapshot.wsConnected = false;
      Serial.println("[WS] ERROR");
      lastWsAttemptMs = millis();
      break;

    case WStype_TEXT: {

      JsonDocument doc;
      DeserializationError err = deserializeJson(doc, payload, length);

      if (err) {
        Serial.print("[JSON ERROR] ");
        Serial.println(err.c_str());
        return;
      }

      const char* streamName = doc["stream"] | "";
      const char* priceText  = doc["data"]["c"] | "0";

      if (!priceText || priceText[0] == '\0') return;

      float price = strtof(priceText, nullptr);

      // ================= BTC =================
      if (strstr(streamName, "btcusdt")) {
        snapshot.btcUsdt = price;
        snapshot.hasBtcUsdt = true;
      }

      // ================= BRL =================
      else if (strstr(streamName, "usdtbrl")) {
        snapshot.usdtBrl = price;
        snapshot.hasUsdtBrl = true;
      }

      // ================= ETH =================
      else if (strstr(streamName, "ethusdt")) {
        snapshot.ethUsdt = price;
        snapshot.hasEthUsdt = true;
      }

      snapshot.messageCount++;
      snapshot.lastUpdateMs = millis();

      updateDerivedValues();

      break;
    }

    default:
      break;
  }
}

// ======================= DERIVED =========================
void BinanceMarket::updateDerivedValues() {

  if (snapshot.hasBtcUsdt && snapshot.hasUsdtBrl) {
    snapshot.btcBrl = snapshot.btcUsdt * snapshot.usdtBrl;
  }

  if (snapshot.hasEthUsdt && snapshot.hasUsdtBrl) {
    snapshot.ethBrl = snapshot.ethUsdt * snapshot.usdtBrl;
  }

  // ✔ não depende de dados inexistentes
  snapshot.dataReady =
    snapshot.hasBtcUsdt &&
    snapshot.hasUsdtBrl;
  // eth is optional; but if present, it's marked by hasEthUsdt
}

// ======================= GET =============================
const CryptoMarketData& BinanceMarket::data() const {
  return snapshot;
}

bool BinanceMarket::isWifiConnected() const {
  return snapshot.wifiConnected;
}

bool BinanceMarket::isWsConnected() const {
  return snapshot.wsConnected;
}