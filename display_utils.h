/*
 * Arquivo: display_utils.h
 * Autor: Eduardo Cadiz
 * Foco: utilitários de cor e mistura em RGB565/BGR565.
 * Data: 2026-05-17
 * Responsabilidade: centralizar conversão e blend de cores para manter o
 * desenho consistente entre ticker, gráficos e cards.
 */
#ifndef DISPLAY_UTILS_H
#define DISPLAY_UTILS_H

#include <stdint.h>

// Converte valores RGB (8-bit cada) para o formato 16-bit usado pelo display.
// OBS: neste projeto o display ST7789 está configurado para BGR565,
// portanto a função inverte R <-> B ao montar o valor final.
static constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return (uint16_t)(((uint16_t)(b & 0xF8) << 8) | ((uint16_t)(g & 0xFC) << 3) | (r >> 3));
}

// Mistura simples entre duas cores 565, onde accentWeight é 0-255.
// Interpreta o valor 565 na mesma ordem usada por rgb565 (BGR565).
static inline uint16_t blend565(uint16_t base, uint16_t accent, uint8_t accentWeight) {
  const uint8_t baseWeight = 255 - accentWeight;
  // Note: bit layout usado aqui é B(15..11), G(10..5), R(4..0)
  const uint8_t baseB = (base >> 11) & 0x1F;
  const uint8_t baseG = (base >> 5) & 0x3F;
  const uint8_t baseR = base & 0x1F;
  const uint8_t accB = (accent >> 11) & 0x1F;
  const uint8_t accG = (accent >> 5) & 0x3F;
  const uint8_t accR = accent & 0x1F;

  const uint8_t outR = (uint8_t)((baseR * baseWeight + accR * accentWeight) / 255);
  const uint8_t outG = (uint8_t)((baseG * baseWeight + accG * accentWeight) / 255);
  const uint8_t outB = (uint8_t)((baseB * baseWeight + accB * accentWeight) / 255);

  return (uint16_t)((outB << 11) | (outG << 5) | outR);
}

#endif // DISPLAY_UTILS_H
