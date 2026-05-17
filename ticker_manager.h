#ifndef TICKER_MANAGER_H
#define TICKER_MANAGER_H

/*
 * Arquivo: ticker_manager.h
 * Autor: Eduardo Cadiz
 * Foco: interface do ticker financeiro horizontal.
 * Data: 2026-05-17
 * Responsabilidade: declarar estruturas e funções para inicializar, alimentar
 * e renderizar a faixa de tickers sem interferir na lógica de mercado.
 */

#include <Arduino.h>
#include "market_data.h"


#ifdef USE_TFT_ESPI
#include <TFT_eSPI.h>
#else
#include <Adafruit_GFX.h>
#endif

struct TickerItem {
  char symbol[12];
  char priceText[16];
  char changeText[16];
  float price;
  float changePercent;
  bool isPositive;
  bool isNeutral;
  uint16_t widthPx;
};

#ifdef USE_TFT_ESPI
void tickerInit(TFT_eSPI* display, int x, int y, int w, int h);
#else
void tickerInit(Adafruit_GFX* display, int x, int y, int w, int h);
#endif

void tickerSetMarketData(const CryptoMarketData& data);
void renderFinancialTicker();

#endif // TICKER_MANAGER_H