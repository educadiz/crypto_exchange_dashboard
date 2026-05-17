#ifndef TFT_DISPLAY_H
#define TFT_DISPLAY_H

#include <Arduino.h>
#include "market_data.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

/*
 * Arquivo: tft_display.h
 * Autor: Eduardo Cadiz
 * Foco: interface pública do dashboard financeiro no ST7789.
 * Data: 2026-05-17
 * Responsabilidade: expor inicialização, atualização e integração do ticker
 * com o display, além do mutex de SPI compartilhado pelas tasks.
 */

extern SemaphoreHandle_t gSpiMutex;

void displayInit();
void displayTick();           // Cards, graficos, header — chamado por displayTask
void displayTickerOnly();     // Apenas o ticker — chamado por tickerTask

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
