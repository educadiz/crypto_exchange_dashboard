#ifndef MARKET_DATA_H
#define MARKET_DATA_H

/*
 * Arquivo: market_data.h
 * Autor: Eduardo Cadiz
 * Foco: estruturas de dados do snapshot de mercado e conectividade.
 * Data: 2026-05-17
 * Responsabilidade: definir o formato estável de dados compartilhado entre
 * market, display e ticker.
 */

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
