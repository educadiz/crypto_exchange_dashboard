#ifndef BINANCE_MARKET_H
#define BINANCE_MARKET_H

#include <Arduino.h>
#include <WebSocketsClient.h>
#include "market_data.h"

class BinanceMarket {
public:
  BinanceMarket();

  void begin(const char* ssid, const char* password);
  void loop();

  const CryptoMarketData& data() const;
  bool isWifiConnected() const;
  bool isWsConnected() const;

private:
  void connectWiFi();
  void connectWebSocket();
  void handleWebSocketEvent(WStype_t type, uint8_t* payload, size_t length);
  void updateDerivedValues();
  void parseTickerMessage(const char* streamName, const char* priceText);
  static void handleWebSocketEventStatic(WStype_t type, uint8_t* payload, size_t length);

  static BinanceMarket* instance;

  WebSocketsClient webSocket;
  CryptoMarketData snapshot;

  char wifiSsid[33];
  char wifiPassword[65];

  bool wsStarted;
  unsigned long lastWifiAttemptMs;
  unsigned long lastWsAttemptMs;
  unsigned long wifiRetryMs;
  unsigned long wsRetryMs;
};

#endif // BINANCE_MARKET_H
