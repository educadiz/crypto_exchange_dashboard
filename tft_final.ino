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
#include "binance_market.h"

static const char WIFI_SSID[] = "spectrum_01";
static const char WIFI_PASSWORD[] = "22602260";

BinanceMarket market;

// Este arquivo agora fica apenas com a lógica principal
// e inicializa o módulo de display `tft_display`.
// ═══════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  delay(800);
  Serial.println("\n=== BINANCE DASHBOARD ESP32-S3 ===");

  // Inicializa o display e a conexao de mercado.
  displayInit();
  market.begin(WIFI_SSID, WIFI_PASSWORD);
}

void loop() {
  market.loop();
  displaySetMarketData(market.data());
  displayTick();
}
