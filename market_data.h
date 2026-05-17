#ifndef MARKET_DATA_H
#define MARKET_DATA_H

// market_data.h
// Estruturas simples que carregam snapshots do estado do mercado e do
// estado do sistema (WiFi/WS). Usado pelo subsistema de display.

#include <Arduino.h>

struct CryptoMarketData {
  float btcUsdt;
  float usdtBrl;
  float ethUsdt;
  float ethBrl;
  float btcBrl;

  bool hasBtcUsdt;
  bool hasUsdtBrl;
  bool hasEthUsdt;

  bool wifiConnected;
  bool wsConnected;
  bool dataReady;

  char wifiSsid[33];
  char wifiIp[16];

  uint32_t messageCount;
  uint32_t reconnectCount;
  unsigned long lastUpdateMs;
};

#endif // MARKET_DATA_H
