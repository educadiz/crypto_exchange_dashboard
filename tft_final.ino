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
#include <esp_task_wdt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Preencha com suas credenciais WiFi
#define WIFI_SSID "spectrum_01"
#define WIFI_PASS "22602260"

static BinanceMarket market;

// Tasks
void marketTask(void* pv) {
  // Registra no watchdog (reseta automatic. tarefa fará reset periodicamente)
  esp_task_wdt_add(NULL);
  for (;;) {
    market.loop();
    // Atualiza display com snapshot atômico
    displaySetMarketData(market.data());
    esp_task_wdt_reset();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void displayTask(void* pv) {
  esp_task_wdt_add(NULL);
  for (;;) {
    displayTick();
    esp_task_wdt_reset();
    vTaskDelay(pdMS_TO_TICKS(16)); // ~60Hz
  }
}

void animationTask(void* pv) {
  esp_task_wdt_add(NULL);
  for (;;) {
    // placeholder for ticker/animations
    // use displaySetMeasurements/displayUseSimulation as needed
    esp_task_wdt_reset();
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

// Este arquivo agora inicializa os subsistemas e cria as tasks FreeRTOS.
void setup() {
  Serial.begin(115200);
  delay(800);
  Serial.println("\n=== TFT ESP32-S3 FINAL (FreeRTOS) ===");

  // Inicializa display
  displayInit();

  // Inicializa mercado WebSocket/WiFi
  market.begin(WIFI_SSID, WIFI_PASS);

  // Inicializa watchdog (10s timeout, panic on timeout)
  {
    esp_task_wdt_config_t wdt_cfg;
    wdt_cfg.timeout_ms = 10000;
    wdt_cfg.idle_core_mask = 0; // não inscrever idle tasks por padrão
    wdt_cfg.trigger_panic = true;
    esp_task_wdt_init(&wdt_cfg);
  }

  // Criar tasks: mercado (alta prioridade), display (média), animação (baixa)
  xTaskCreatePinnedToCore(marketTask, "market", 4096, NULL, 3, NULL, 1);
  xTaskCreatePinnedToCore(displayTask, "display", 4096, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(animationTask, "anim", 2048, NULL, 1, NULL, 1);
}

void loop() {
  // Nada aqui: FreeRTOS cuida das tasks
  vTaskDelay(pdMS_TO_TICKS(1000));
}
