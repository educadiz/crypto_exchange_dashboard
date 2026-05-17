/*
 * =====================================================
 * Teste TFT 2.8" 240x320 - ESP32-S3  [FINAL]
 * Lib: Adafruit ST7789
 * Fix: objeto via ponteiro + correção BGR/rotação
 * =====================================================
 * INSTALAR:
 *   - "Adafruit ST7735 and ST7789 Library"
 *   - "Adafruit GFX Library"
 *
 * PINAGEM:
 *   SCK   -> GPIO 3
 *   MOSI  -> GPIO 45
 *   CS    -> GPIO 14
 *   DC    -> GPIO 47
 *   RST   -> GPIO 21
 *   LED   -> 3.3V
 * =====================================================
 */

#include "tft_display.h"

// Este arquivo agora fica apenas com a lógica principal
// e inicializa o módulo de display `tft_display`.
// ═══════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  delay(800);
  Serial.println("\n=== TFT ESP32-S3 FINAL (refatorado) ===");

  // Inicializa o display (módulo cuida da pinagem e do objeto)
  displayInit();

  // Exemplo de uso com sensores reais:
  // displayUseSimulation(false);
}

// Loop principal minimalista delegando ao módulo
void loop() {
  // Se estiver em modo real, atualize aqui antes de displayTick():
  // displaySetMeasurements(tempC, humPct, pressHpa, rpmMotor);
  displayTick();
}
