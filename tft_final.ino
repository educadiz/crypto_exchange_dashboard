/*
 * Arquivo: tft_final.ino
 * Autor: Eduardo Cadiz
 * Foco: ponto de entrada do firmware do painel financeiro.
 * Data: 2026-05-17
 * Responsabilidade: inicializar display, mercado, watchdog e tasks FreeRTOS
 * sem embutir regra de negócio de visualização.
 */

#include "tft_display.h"
#include "binance_market.h"
#include <esp_task_wdt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define WIFI_SSID "spectrum_01"
#define WIFI_PASS "22602260"

static BinanceMarket market;

// ---- marketTask — Core 0 ----
// WebSocket + WiFi pertencem ao core 0 (lwIP stack).
// Separar do SPI elimina toda disputa de barramento.
void marketTask(void* pv) {
  esp_task_wdt_add(NULL);
  for (;;) {
    market.loop();
    displaySetMarketData(market.data());
    esp_task_wdt_reset();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// ---- tickerTask — Core 1 ----
// Scroll contínuo do ticker financeiro.
// Prioridade 3: igual à marketTask — nunca a bloqueia.
// gSpiMutex serializa com displayTask no mesmo core.
void tickerTask(void* pv) {
  esp_task_wdt_add(NULL);
  TickType_t lastWake = xTaskGetTickCount();
  uint8_t dashboardPump = 0;
  for (;;) {
    displayTickerOnly();
    // A cada alguns ciclos, o ticker também atualiza o dashboard completo.
    // Isso evita que cards/graficos fiquem sem janela de desenho caso a task
    // principal atrase por disputa de SPI ou scheduling.
    if ((dashboardPump++ & 0x03U) == 0U) {
      displayTick();
    }
    esp_task_wdt_reset();
    vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(12)); // reduz contencao e mantém scroll estável
  }
}

// ---- displayTask — Core 1 ----
// Cards, graficos, header e status bar.
// Roda devagar: so repinta quando os dados mudam.
void displayTask(void* pv) {
  esp_task_wdt_add(NULL);
  TickType_t lastWake = xTaskGetTickCount();
  for (;;) {
    displayTick();
    esp_task_wdt_reset();
    vTaskDelayUntil(&lastWake, pdMS_TO_TICKS(40)); // dá mais chance ao dashboard principal
  }
}

void setup() {
  Serial.begin(115200);
  delay(800);
  Serial.println("\n=== Cadiz Crypto Dashboard ===");

  displayInit();
  market.begin(WIFI_SSID, WIFI_PASS);

  // NAO reinicializamos o TWDT aqui.
  // O ESP-IDF ja inicializa o watchdog no boot com timeout padrao de 5s.
  // Apenas inscrevemos as tasks individualmente via esp_task_wdt_add(NULL)
  // dentro de cada task loop.

  // market no core 0 (lwIP/WiFi); ticker e display no core 1 (SPI)
  xTaskCreatePinnedToCore(marketTask,  "market",  8192, NULL, 3, NULL, 0);
  xTaskCreatePinnedToCore(tickerTask,  "ticker",  4096, NULL, 2, NULL, 1);
  xTaskCreatePinnedToCore(displayTask, "display", 6144, NULL, 3, NULL, 1);
}

void loop() {
  vTaskDelay(pdMS_TO_TICKS(1000));
}
