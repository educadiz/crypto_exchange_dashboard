/*
 * Arquivo: binance_market.cpp
 * Autor: Eduardo Cadiz
 * Foco: Conexão WiFi/WebSocket da Binance e cálculo do snapshot de mercado.
 * Data: 2026-05-17
 * Responsabilidade: manter a conexão, atualizar o estado do mercado e expor
 * um snapshot estável para o subsistema de display.
 */

#include "binance_market.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include <cstring>

static const char* BINANCE_HOST = "stream.binance.com";
static const uint16_t BINANCE_PORT = 9443;

static const char* BINANCE_PATH =
"/stream?streams="
"btcusdt@miniTicker/"
"usdtbrl@miniTicker/"
"ethusdt@miniTicker";

static bool lastWifiConnectedLogged = false;
static bool lastWsConnectedLogged = false;
static unsigned long lastHeartbeatMs = 0;

BinanceMarket* BinanceMarket::instance = nullptr;

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

void BinanceMarket::loop() {
  const unsigned long now = millis();

  snapshot.wifiConnected = (WiFi.status() == WL_CONNECTED);

  if (snapshot.wifiConnected) {
    IPAddress ip = WiFi.localIP();
    std::snprintf(snapshot.wifiIp, sizeof(snapshot.wifiIp), "%d.%d.%d.%d",
                  ip[0], ip[1], ip[2], ip[3]);
    if (!lastWifiConnectedLogged) {
      Serial.printf("[WIFI] connected IP=%s\n", snapshot.wifiIp);
      lastWifiConnectedLogged = true;
    }
  } else {
    if (lastWifiConnectedLogged) {
      Serial.println("[WIFI] disconnected");
      lastWifiConnectedLogged = false;
    }
    snapshot.wifiIp[0] = '\0';
    snapshot.wsConnected = false;
    if (lastWsConnectedLogged) {
      Serial.println("[WS] disconnected (wifi lost)");
      lastWsConnectedLogged = false;
    }

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

  if (snapshot.wifiConnected && (now - lastHeartbeatMs) >= 10000UL) {
    Serial.printf("[NET] wifi=%d ws=%d ready=%d msg=%lu ip=%s\n",
                  snapshot.wifiConnected,
                  snapshot.wsConnected,
                  snapshot.dataReady,
                  (unsigned long)snapshot.messageCount,
                  snapshot.wifiIp);
    lastHeartbeatMs = now;
  }

  // 🔴 obrigatório
  webSocket.loop();
}

void BinanceMarket::connectWiFi() {
  lastWifiAttemptMs = millis();
  Serial.println("[WIFI] reconnecting...");
  WiFi.disconnect(false, false);
  WiFi.begin(wifiSsid, wifiPassword);
}

void BinanceMarket::connectWebSocket() {
  lastWsAttemptMs = millis();
  wsStarted = false;

  snapshot.reconnectCount++;
  Serial.printf("[WS] connecting retry=%lu\n", (unsigned long)snapshot.reconnectCount);

  webSocket.onEvent(BinanceMarket::handleWebSocketEventStatic);
  webSocket.setReconnectInterval(3000);

  webSocket.beginSSL(
    BINANCE_HOST,
    BINANCE_PORT,
    BINANCE_PATH
  );
}

void BinanceMarket::handleWebSocketEventStatic(WStype_t type, uint8_t* payload, size_t length) {
  if (instance) {
    instance->handleWebSocketEvent(type, payload, length);
  }
}

void BinanceMarket::handleWebSocketEvent(WStype_t type, uint8_t* payload, size_t length) {

  switch (type) {

    case WStype_CONNECTED:
      wsStarted = true;
      snapshot.wsConnected = true;
      snapshot.lastUpdateMs = millis();
      Serial.println("[WS] CONNECTED");
      lastWsConnectedLogged = true;
      break;

    case WStype_DISCONNECTED:
      wsStarted = false;
      snapshot.wsConnected = false;
      Serial.println("[WS] DISCONNECTED");
      lastWsAttemptMs = millis();
      lastWsConnectedLogged = false;
      break;

    case WStype_ERROR:
      wsStarted = false;
      snapshot.wsConnected = false;
      Serial.println("[WS] ERROR");
      lastWsAttemptMs = millis();
      lastWsConnectedLogged = false;
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

      if (strstr(streamName, "btcusdt")) {
        snapshot.btcUsdt = price;
        snapshot.hasBtcUsdt = true;
      }

      else if (strstr(streamName, "usdtbrl")) {
        snapshot.usdtBrl = price;
        snapshot.hasUsdtBrl = true;
      }

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

void BinanceMarket::updateDerivedValues() {

  if (snapshot.hasBtcUsdt && snapshot.hasUsdtBrl) {
    snapshot.btcBrl = snapshot.btcUsdt * snapshot.usdtBrl;
  }

  if (snapshot.hasEthUsdt && snapshot.hasUsdtBrl) {
    snapshot.ethBrl = snapshot.ethUsdt * snapshot.usdtBrl;
  }

  snapshot.dataReady =
    snapshot.hasBtcUsdt &&
    snapshot.hasUsdtBrl;
}

const CryptoMarketData& BinanceMarket::data() const {
  return snapshot;
}

bool BinanceMarket::isWifiConnected() const {
  return snapshot.wifiConnected;
}

bool BinanceMarket::isWsConnected() const {
  return snapshot.wsConnected;
}