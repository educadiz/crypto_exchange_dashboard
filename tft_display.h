/*
 * Módulo de display TFT
 * Responsável por inicializar o display e desenhar as "cenas" de exemplo.
 * Interface pública mínima para uso em outros sketches.
 */
#ifndef TFT_DISPLAY_H
#define TFT_DISPLAY_H

#include <Arduino.h>

// Inicializa o display (espera que `Serial.begin()` já tenha sido chamado)
void displayInit();

// Deve ser chamado periodicamente em `loop()` para atualizar o dashboard
void displayTick();

// Mantido por compatibilidade com versões anteriores (sem efeito no dashboard)
void displaySetDuration(unsigned long ms);

// Habilita/desabilita modo simulado (true por padrão para demo)
void displayUseSimulation(bool enabled);

// Atualiza medições reais; útil quando modo simulado estiver desabilitado
void displaySetMeasurements(float temperatureC, float humidityPct, float pressureHpa, float motorRpm);

#endif // TFT_DISPLAY_H
