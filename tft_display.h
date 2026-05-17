#ifndef TFT_DISPLAY_H
#define TFT_DISPLAY_H

#include <Arduino.h>
#include "market_data.h"

void displayInit();
void displayTick();

void displaySetMarketData(const CryptoMarketData& data);

// Habilitar/desabilitar widgets individuais (cards)
void displayEnableWidgetUsd(bool enabled);
void displayEnableWidgetBrl(bool enabled);
void displayEnableWidgetBtc(bool enabled);
void displayEnableWidgetEth(bool enabled);

// Compatibilidade com versões anteriores do projeto.
void displaySetDuration(unsigned long ms);
void displayUseSimulation(bool enabled);
void displaySetMeasurements(float temperatureC, float humidityPct, float pressureHpa, float motorRpm);

#endif // TFT_DISPLAY_H
