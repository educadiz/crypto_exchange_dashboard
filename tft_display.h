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

// Deve ser chamado periodicamente em `loop()` para trocar cenas automaticamente
void displayTick();

// Ajusta duração entre cenas (ms)
void displaySetDuration(unsigned long ms);

#endif // TFT_DISPLAY_H
